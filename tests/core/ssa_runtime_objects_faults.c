#include "ssa_runtime_objects_faults.h"
#include <stdlib.h>
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/state.h"
#include "gc/gc_internal.h"
#include "gc/gc_domain_internal.h"

static TZrUInt32 failObjectOrdinal, objectCount, reserveCount, failReserveOrdinal;
static TZrUInt32 invalidMutationEndCount;
static TZrBool throwOomEnabled, collectEnabled;
static size_t failNativeOrdinal, nativeCount;
static TZrBool nativeFailed;
static TZrBool failRemembered;
static TZrBool failPause;
static void (*barrierHook)(SZrState *, void *);
static void *barrierContext;

void ssa_runtime_objects_faults(TZrUInt32 failObject, TZrBool throwOom,
                                TZrBool collectEachObject, TZrUInt32 failReserve) {
    failObjectOrdinal = failObject;
    objectCount = reserveCount = 0u;
    invalidMutationEndCount = 0u;
    throwOomEnabled = throwOom;
    collectEnabled = collectEachObject;
    failReserveOrdinal = failReserve;
    failRemembered = ZR_FALSE;
    failPause = ZR_FALSE;
    barrierHook = ZR_NULL;
    barrierContext = ZR_NULL;
    ssa_runtime_objects_fail_native_allocation(0u);
}

TZrUInt32 ssa_runtime_objects_allocation_count(void) { return objectCount; }
TZrUInt32 ssa_runtime_objects_invalid_mutation_end_count(void) {
    return invalidMutationEndCount;
}

void ssa_runtime_objects_fail_native_allocation(size_t ordinal) {
    failNativeOrdinal = ordinal;
    nativeCount = 0u;
    nativeFailed = ZR_FALSE;
}

TZrBool ssa_runtime_objects_native_allocation_failed(void) { return nativeFailed; }

void ssa_runtime_objects_fail_remembered_reservation(void) { failRemembered = ZR_TRUE; }
void ssa_runtime_objects_fail_pause(void) { failPause = ZR_TRUE; }
void ssa_runtime_objects_set_barrier_hook(void (*hook)(SZrState *, void *), void *context) {
    barrierHook = hook;
    barrierContext = context;
}

static TZrBool fail_native(void) {
    if (++nativeCount != failNativeOrdinal) return ZR_FALSE;
    nativeFailed = ZR_TRUE;
    return ZR_TRUE;
}

static void *test_calloc(size_t count, size_t size) {
    return fail_native() ? NULL : calloc(count, size);
}

static TZrBool fail_object(SZrState *state) {
    ++objectCount;
    if (collectEnabled) ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    if (failObjectOrdinal != objectCount) return ZR_FALSE;
    if (throwOomEnabled) ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    return ZR_TRUE;
}

static SZrObject *test_object_new(SZrState *state, SZrObjectPrototype *prototype) {
    if (fail_object(state)) return ZR_NULL;
    return ZrCore_Object_New(state, prototype);
}

static TZrBool test_reserve(SZrState *state, SZrHashSet *set, TZrSize count) {
    if (collectEnabled) ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    if (++reserveCount == failReserveOrdinal) return ZR_FALSE;
    return ZrCore_HashSet_EnsurePairPoolForElementCount(state, set, count);
}

static TZrBool test_remembered_reserve(SZrGlobalState *global, TZrSize count) {
    return failRemembered ? ZR_FALSE : garbage_collector_ensure_remembered_registry_capacity(global, count);
}

static TZrBool test_pause(SZrState *state, TZrUInt32 timeout, SZrGcDomainPauseDiagnostic *diagnostic) {
    return failPause ? ZR_FALSE : ZrCore_GcDomain_StopTheWorldBegin(state, timeout, diagnostic);
}

static void test_barrier(SZrState *state, SZrRawObject *object, SZrRawObject *value) {
    void (*hook)(SZrState *, void *) = barrierHook;
    barrierHook = ZR_NULL;
    if (hook != ZR_NULL) hook(state, barrierContext);
    ZrCore_RawObject_Barrier(state, object, value);
}

static void test_mutation_end(SZrState *state, TZrBool locked) {
    TZrUInt32 depth = 0u;
    SZrGcDomain *domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    for (TZrSize index = 0u; index < domain->mutatorLength; ++index) {
        if (domain->mutators[index].state == state) {
            depth = domain->mutators[index].mutationDepth;
            break;
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    if (locked && depth == 0u) {
        /* Record the real invalid release request before calling an unlock
         * with undefined platform behavior, so the regression fails cleanly. */
        ++invalidMutationEndCount;
        return;
    }
    ZrCore_GcDomain_MutationEnd(state, locked);
}

/* Fault only the production call boundaries; objects, GC and storage remain
 * the real runtime. A distinct symbol coexists with the unwrapped library API. */
#define ZrCore_ExecIr_MaterializeObjects ssa_runtime_objects_materialize
#define ZrCore_Object_New test_object_new
#define ZrCore_HashSet_EnsurePairPoolForElementCount test_reserve
#define garbage_collector_ensure_remembered_registry_capacity test_remembered_reserve
#define ZrCore_GcDomain_StopTheWorldBegin test_pause
#define ZrCore_RawObject_Barrier test_barrier
#define ZrCore_GcDomain_MutationEnd test_mutation_end
#define calloc test_calloc
#include "exec_ir/exec_ir_materialize_objects.c"

/**
 * @var TZrUInt32 failObjectOrdinal
 *
 * ## Summary
 * 选择一次对象创建尝试的失败序号。
 *
 * ## Constraints and lifecycle
 * faults 写入，fail_object 在 objectCount 先增后比较；正常可实现的测试序列中0用作禁用。只影响 text-included Object_New 调用边界，序号不代表全 runtime allocator 次数。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:12；tests/core/ssa_runtime_objects_faults.c:24；tests/core/ssa_runtime_objects_faults.c:68；tests/core/ssa_runtime_objects_faults.c:70。
 */

/**
 * @var TZrUInt32 failReserveOrdinal
 *
 * ## Summary
 * 选择一次 detached pair-pool 预留返回 FALSE 的序号。
 *
 * ## Constraints and lifecycle
 * faults 写入，与递增后的 reserveCount 比较；0用于测试序列中禁用；触发时跳过真正 EnsurePairPool 调用，不改成模拟整个 HashSet实现。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:12；tests/core/ssa_runtime_objects_faults.c:29；tests/core/ssa_runtime_objects_faults.c:82；tests/core/ssa_runtime_objects_faults.c:83。
 */

/**
 * @var TZrUInt32 objectCount
 *
 * ## Summary
 * 记录故障实例的 Object_New 尝试次数。
 *
 * ## Constraints and lifecycle
 * faults 置0，fail_object 在可选 FullGC 前递增，allocation_count 读出；包括即将返回NULL或Throw的尝试，不是成功对象数。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:12；tests/core/ssa_runtime_objects_faults.c:25；tests/core/ssa_runtime_objects_faults.c:37；tests/core/ssa_runtime_objects_faults.c:68；tests/core/ssa_runtime_objects_faults.c:70。
 */

/**
 * @var TZrUInt32 reserveCount
 *
 * ## Summary
 * 记录字段 detached pair-pool 预留尝试次数。
 *
 * ## Constraints and lifecycle
 * faults 复位，test_reserve 在可选 FullGC 后先增再比较；不统计哈希实现或 GC 内部的其它 reserve/allocator。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:12；tests/core/ssa_runtime_objects_faults.c:25；tests/core/ssa_runtime_objects_faults.c:82。
 */

/**
 * @var TZrUInt32 invalidMutationEndCount
 *
 * ## Summary
 * 计数 locked 为真但当前 mutator mutationDepth 为0的释放请求。
 *
 * ## Constraints and lifecycle
 * faults 置0；test_mutation_end 在域锁下读深度后发现该组合就加1并不调用真正 unlock，getter只读。0只说明此检测器未命中，不证明所有锁状态或线程交错。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:13；tests/core/ssa_runtime_objects_faults.c:26；tests/core/ssa_runtime_objects_faults.c:39；tests/core/ssa_runtime_objects_faults.c:104；tests/core/ssa_runtime_objects_faults.c:107；tests/core/ssa_runtime_objects_faults.c:112；tests/core/ssa_runtime_objects_faults.c:115。
 */

/**
 * @var TZrBool collectEnabled
 *
 * ## Summary
 * 决定在对象尝试和字段 pair-pool 预留前执行真实 FullGC。
 *
 * ## Constraints and lifecycle
 * faults 配置；fail_object 与 test_reserve 都读取；因此一次物化可能产生多于对象数的收集，不固定精确GC次数。调用方必须满足真实 GC 根/作用域条件。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:14；tests/core/ssa_runtime_objects_faults.c:28；tests/core/ssa_runtime_objects_faults.c:69；tests/core/ssa_runtime_objects_faults.c:81。
 */

/**
 * @var TZrBool throwOomEnabled
 *
 * ## Summary
 * 决定命中 Object_New 失败序号时抛 MEMORY_ERROR 或普通返回空。
 *
 * ## Constraints and lifecycle
 * faults 配置；仅 fail_object 使用；Throw 跳过普通返回，需外层对象物化的 TryRun 捕获。不是所有 allocator 的异常开关。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:14；tests/core/ssa_runtime_objects_faults.c:27；tests/core/ssa_runtime_objects_faults.c:71。
 */

/**
 * @var size_t failNativeOrdinal
 *
 * ## Summary
 * 选择本故障编译实例的一次直接 calloc 尝试失败。
 *
 * ## Constraints and lifecycle
 * set_native 写入并复位 nativeCount/nativeFailed；仅 text-included Objects C 经 test_calloc 的直接调用受影响，不含链接进来的逻辑物化/Owner/Liveness/Hash/GC allocator。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:15；tests/core/ssa_runtime_objects_faults.c:43；tests/core/ssa_runtime_objects_faults.c:58；tests/core/ssa_runtime_objects_faults.c:63；tests/core/ssa_runtime_objects_faults.c:130。
 */

/**
 * @var size_t nativeCount
 *
 * ## Summary
 * 记录本故障编译实例 test_calloc 的尝试序号。
 *
 * ## Constraints and lifecycle
 * set_native 置0，fail_native 每调用先增后比较；不表示成功分配数。五个必需 work 池加可选 ignored 快照是可控边界，不能从本计数外推 linked allocator。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:15；tests/core/ssa_runtime_objects_faults.c:44；tests/core/ssa_runtime_objects_faults.c:58；tests/core/ssa_runtime_objects_faults.c:64；tests/core/ssa_runtime_objects_faults.c:130。
 */

/**
 * @var TZrBool nativeFailed
 *
 * ## Summary
 * 记录自上次 native 配置以来是否已命中过指定 calloc 失败。
 *
 * ## Constraints and lifecycle
 * set_native 置FALSE，命中时置TRUE，getter只读且不清除；该标志粘滞至复位，后续不匹配的分配仍可成功。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:16；tests/core/ssa_runtime_objects_faults.c:45；tests/core/ssa_runtime_objects_faults.c:48；tests/core/ssa_runtime_objects_faults.c:59。
 */

/**
 * @var TZrBool failRemembered
 *
 * ## Summary
 * 持续拒绝物化内部 remembered registry 预留边界。
 *
 * ## Constraints and lifecycle
 * faults 复位FALSE，setter置TRUE，test_remembered_reserve 读取后短路返回FALSE；非一次性开关，不覆盖真实 helper 的内部 allocator。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:17；tests/core/ssa_runtime_objects_faults.c:30；tests/core/ssa_runtime_objects_faults.c:50；tests/core/ssa_runtime_objects_faults.c:87。
 */

/**
 * @var TZrBool failPause
 *
 * ## Summary
 * 持续拒绝物化附着阶段的 StopTheWorldBegin 边界。
 *
 * ## Constraints and lifecycle
 * faults 复位FALSE，setter置TRUE；test_pause 读取后直接FALSE，不调用真实暂停也不填 pause diagnostic。保持到下一次 faults 复位。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:18；tests/core/ssa_runtime_objects_faults.c:31；tests/core/ssa_runtime_objects_faults.c:51；tests/core/ssa_runtime_objects_faults.c:91。
 */

/**
 * @var void (*barrierHook)(SZrState *, void *)
 *
 * ## Summary
 * 保存首个附着 barrier 将同步调用的测试回调。
 *
 * ## Constraints and lifecycle
 * setter存储借用函数指针，test_barrier 先复制并清空再回调，允许回调Throw而不重复触发；full reset清空。无需拥有或释放函数地址，不能与另一个 case 并发重配。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:19；tests/core/ssa_runtime_objects_faults.c:32；tests/core/ssa_runtime_objects_faults.c:52；tests/core/ssa_runtime_objects_faults.c:53；tests/core/ssa_runtime_objects_faults.c:95；tests/core/ssa_runtime_objects_faults.c:96；tests/core/ssa_runtime_objects_faults.c:97。
 */

/**
 * @var void *barrierContext
 *
 * ## Summary
 * 保存一拍 barrier 回调借用的上下文地址。
 *
 * ## Constraints and lifecycle
 * setter存储，full reset清空；barrierHook清空时并不自动清空context，只有存在hook才读取。栈上的 held/context 必须活到同步回调返回/抛转移完成，回调实际写入的对象须可写。 本状态非原子且非每线程隔离，只允许本测试程序约定的串行case/同步使用，不提供跨并发重配保障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:20；tests/core/ssa_runtime_objects_faults.c:33；tests/core/ssa_runtime_objects_faults.c:52；tests/core/ssa_runtime_objects_faults.c:54；tests/core/ssa_runtime_objects_faults.c:97。
 */

/**
 * @fn void ssa_runtime_objects_faults(TZrUInt32 failObject, TZrBool throwOom, TZrBool collectEachObject, TZrUInt32 failReserve)
 *
 * ## Summary
 * 在一次串行测试场景开始前复位本故障编译实例的计数与开关。
 *
 * ## Parameters
 * @param[in] failObject 要使Object_New尝试失败的1起序号；当前有界测试使用0禁用。
 * @param[in] throwOom 命中对象失败序号时为真则Throw MEMORY_ERROR，否则返回NULL。
 * @param[in] collectEachObject 为真时在对象创建和detached pair-pool预留前都运行真实FullGC。
 * @param[in] failReserve 要使detached pair-pool预留返回FALSE的1起序号；0禁用当前测试序列。
 *
 * ## Returns
 * 无C返回值；覆盖本TU故障配置、清零对象/预留/非法释放计数，并通过native setter复位native序号与命中标志。
 *
 * ## Constraints and lifecycle
 * 还清除remembered/pause失败开关和一次性barrier hook/context；不会释放GC对象、native分配或清理上一次case资源。状态是TU-static而非每state/线程私有，必须在没有正在使用该wrapper的物化/回调时串行重配；concurrency支持只在单次调用前后配置hook。只替换included Objects C的边界，真实GC/对象/哈希实现和linked logical analysis不随之被模拟。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:22；tests/core/ssa_runtime_objects_faults.c:24；tests/core/ssa_runtime_objects_faults.c:25；tests/core/ssa_runtime_objects_faults.c:26；tests/core/ssa_runtime_objects_faults.c:30；tests/core/ssa_runtime_objects_faults.c:31；tests/core/ssa_runtime_objects_faults.c:32；tests/core/ssa_runtime_objects_faults.c:33；tests/core/ssa_runtime_objects_faults.c:34；tests/core/test_ssa_runtime_objects.c:139；tests/core/test_ssa_runtime_objects.c:578；tests/core/ssa_runtime_objects_faults.h:5。
 */

/**
 * @fn TZrUInt32 ssa_runtime_objects_allocation_count(void)
 *
 * ## Summary
 * 读取自上次faults复位后经故障Object_New入口发生的尝试次数。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return 当前objectCount快照；包括返回NULL或Throw前已计数的尝试，不是成功对象数。
 *
 * ## Constraints and lifecycle
 * 纯读TU-static非原子计数，不清空。须与本wrapper的调用/配置串行或按测试支持的同步协议读取；不统计普通库入口或GC内部其它对象分配，不分配/释放资源。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:37；tests/core/ssa_runtime_objects_faults.c:25；tests/core/ssa_runtime_objects_faults.c:68；tests/core/test_ssa_runtime_objects.c:192；tests/core/test_ssa_runtime_objects.c:508；tests/core/ssa_runtime_objects_faults.h:7。
 */

/**
 * @fn TZrUInt32 ssa_runtime_objects_invalid_mutation_end_count(void)
 *
 * ## Summary
 * 读取检测器捕获的过期mutation释放请求次数。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return 自faults复位以来locked为真而当前mutationDepth为0的请求数。
 *
 * ## Constraints and lifecycle
 * 仅检测test_mutation_end中明确的组合，命中时已抑制真实平台unlock以避免未定义行为；返回0不证明任意锁错误都被覆盖。getter不加锁、不清空、不拥有资源；在materialize同步返回后读取。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:38；tests/core/ssa_runtime_objects_faults.c:26；tests/core/ssa_runtime_objects_faults.c:39；tests/core/ssa_runtime_objects_faults.c:112；tests/core/ssa_runtime_objects_faults.c:115；tests/core/test_ssa_runtime_objects.c:409；tests/core/test_ssa_runtime_objects.c:416；tests/core/ssa_runtime_objects_faults.h:8。
 */

/**
 * @fn void ssa_runtime_objects_fail_native_allocation(size_t ordinal)
 *
 * ## Summary
 * 为text-included Objects C的直接calloc设置一次故障序号并重开计数窗口。
 *
 * ## Parameters
 * @param[in] ordinal 从1开始的test_calloc尝试序号；当前有界测试以0表示不注入。
 *
 * ## Returns
 * 无C返回值；保存序号、nativeCount置0且nativeFailed置FALSE。
 *
 * ## Constraints and lifecycle
 * 只改变TU-static测试状态，不回收旧内存或影响已完成分配。只控制include后宏展开的直接calloc，当前为五个必需work数组和一个按需ignored快照；linked MaterializeState/Owner/Liveness及真实GC/哈希内部allocator不被此宏覆盖。须在无并发使用时配置，未运行的源码遍历不能报告实际命中次数。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:42；tests/core/ssa_runtime_objects_faults.c:43；tests/core/ssa_runtime_objects_faults.c:44；tests/core/ssa_runtime_objects_faults.c:45；tests/core/ssa_runtime_objects_faults.c:58；tests/core/ssa_runtime_objects_faults.c:130；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:444；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:451；tests/core/test_ssa_runtime_objects.c:579；tests/core/ssa_runtime_objects_faults.h:9。
 */

/**
 * @fn TZrBool ssa_runtime_objects_native_allocation_failed(void)
 *
 * ## Summary
 * 读取当前native故障窗口是否已命中指定calloc序号。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return nativeFailed粘滞标志；命中过一次后保持TRUE，直到native setter或faults复位。
 *
 * ## Constraints and lifecycle
 * 此getter不清除标志，不证明最终公开API一定失败，也不统计linked allocator。test569用它区分被注入的失败与首次未命中的成功；TU-static非原子状态要求串行控制。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:48；tests/core/ssa_runtime_objects_faults.c:45；tests/core/ssa_runtime_objects_faults.c:59；tests/core/test_ssa_runtime_objects.c:581；tests/core/test_ssa_runtime_objects.c:586；tests/core/ssa_runtime_objects_faults.h:10。
 */

/**
 * @fn void ssa_runtime_objects_fail_remembered_reservation(void)
 *
 * ## Summary
 * 打开物化remembered registry预留的持续失败开关。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；failRemembered置TRUE。
 *
 * ## Constraints and lifecycle
 * 之后test_remembered_reserve直接FALSE且跳过真实预留；不是单次消费，须由faults复位。仅模拟该边界失败，不改变registry资源所有权或证明所有GC分配路径。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:50；tests/core/ssa_runtime_objects_faults.c:30；tests/core/ssa_runtime_objects_faults.c:87；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:582；tests/core/test_ssa_runtime_objects.c:248；tests/core/ssa_runtime_objects_faults.h:11。
 */

/**
 * @fn void ssa_runtime_objects_fail_pause(void)
 *
 * ## Summary
 * 打开物化附着暂停入口的持续失败开关。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * 无C返回值；failPause置TRUE。
 *
 * ## Constraints and lifecycle
 * 之后test_pause直接FALSE，不调用真实StopTheWorldBegin也不填写该helper的pause diagnostic；须由faults复位，不会结束已经存在的暂停。仅模拟该边界，非真实超时/全部域故障。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:51；tests/core/ssa_runtime_objects_faults.c:31；tests/core/ssa_runtime_objects_faults.c:91；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570；tests/core/test_ssa_runtime_objects.c:360；tests/core/ssa_runtime_objects_faults.h:12。
 */

/**
 * @fn void ssa_runtime_objects_set_barrier_hook(void (*hook)(SZrState *, void *), void *context)
 *
 * ## Summary
 * 为下一次经故障barrier入口的调用借用一个同步回调及上下文。
 *
 * ## Parameters
 * @param[in] hook 可为空的函数指针；非空时在下一次test_barrier先清空注册再调用。
 * @param[in] context 只存储指针值的借用上下文；若hook将写入它，其目标须可写且活到同步回调完成。
 *
 * ## Returns
 * 无C返回值；覆盖当前barrierHook和barrierContext，不调用hook或释放旧context。
 *
 * ## Constraints and lifecycle
 * 不是拥有型注册或线程安全队列；配置/使用须服从串行测试及并发helper的前后同步。test_barrier在hook抛错前已清空hook，但并不自动清空context；set(NULL,NULL)可同时撤销两者。栈context不得在未消费的hook仍可能调用时失效。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:52；tests/core/ssa_runtime_objects_faults.c:53；tests/core/ssa_runtime_objects_faults.c:54；tests/core/ssa_runtime_objects_faults.c:95；tests/core/ssa_runtime_objects_faults.c:96；tests/core/ssa_runtime_objects_faults.c:97；tests/core/test_ssa_runtime_objects.c:347；tests/core/test_ssa_runtime_objects.c:381；tests/core/ssa_runtime_objects_concurrency.c:270；tests/core/ssa_runtime_objects_concurrency.c:274；tests/core/ssa_runtime_objects_faults.h:13。
 */

/**
 * @fn TZrBool fail_native(void)
 *
 * ## Summary
 * 推进native尝试序号并在配置的单个序号置命中标志。
 *
 * ## Parameters
 * 无形参；仅按下述调用时机访问本测试fixture或TU-static状态。
 *
 * ## Returns
 * @return 本次递增后的nativeCount等于failNativeOrdinal时TRUE，否则FALSE。
 *
 * ## Constraints and lifecycle
 * 仅由test_calloc调用；命中不会复位序号，后续不匹配的调用仍可分配；nativeFailed保持粘滞TRUE。TU-static普通整数不是并发分配统计器；当前有界测试用0禁用，不据理论计数wrap推断已复现BUG。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:57；tests/core/ssa_runtime_objects_faults.c:58；tests/core/ssa_runtime_objects_faults.c:59；tests/core/ssa_runtime_objects_faults.c:64；tests/core/test_ssa_runtime_objects.c:576。
 */

/**
 * @fn void *test_calloc(size_t count, size_t size)
 *
 * ## Summary
 * 将included Objects C的直接calloc尝试映射到native序号注入器。
 *
 * ## Parameters
 * @param[in] count 透传给C calloc的元素数量；本wrapper不额外验证乘积。
 * @param[in] size 透传给C calloc的单元素字节数。
 *
 * ## Returns
 * @return 命中注入时NULL；否则真实calloc结果，可能因自然分配失败为空，成功内存归调用方并须按C free兼容方式释放。
 *
 * ## Constraints and lifecycle
 * 先推进fail_native；命中时不调用真正calloc。wrapper定义早于calloc宏，故其内部calloc是库分配器而非递归替换。宏仅作用于后续include的Objects C直接调用，不覆盖linked函数和GC内部allocator。候选分配的局部失败由生产cleanup统一释放，wrapper本身不追踪旧指针。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:63；tests/core/ssa_runtime_objects_faults.c:64；tests/core/ssa_runtime_objects_faults.c:130；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:444；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:449；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:451；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:657。
 */

/**
 * @fn TZrBool fail_object(SZrState *state)
 *
 * ## Summary
 * 在真实GC对象创建边界前计数、按需FullGC，并选择NULL或Throw故障。
 *
 * ## Parameters
 * @param[in,out] state 启用FullGC或Throw时必须是有效runtime state；调用方须满足真实GC根和异常恢复前提。
 *
 * ## Returns
 * @return 未命中对象序号时FALSE表示继续真实创建；命中且未抛错时TRUE表示模拟返回空；throwOomEnabled为真时通过Exception_Throw转移控制，不普通返回。
 *
 * ## Constraints and lifecycle
 * objectCount先增，FullGC后才比较序号，因此失败尝试也可收集并更新已注册根。只由test_object_new调用，不创建/拥有对象。实际included调用传NULL prototype，任何外部复用均不能把未根保护的裸prototype跨FullGC保留。计数/开关非原子，须串行配置。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:67；tests/core/ssa_runtime_objects_faults.c:68；tests/core/ssa_runtime_objects_faults.c:69；tests/core/ssa_runtime_objects_faults.c:70；tests/core/ssa_runtime_objects_faults.c:71；tests/core/ssa_runtime_objects_faults.c:76；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:550；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:707。
 */

/**
 * @fn SZrObject *test_object_new(SZrState *state, SZrObjectPrototype *prototype)
 *
 * ## Summary
 * 以对象序号故障包装真实Object_New，保留真实对象与GC行为。
 *
 * ## Parameters
 * @param[in,out] state 供计数前GC、可能Throw及真实Object_New使用的有效state。
 * @param[in] prototype 借用的有效prototype或NULL；当前生产物化调用固定NULL，若复用非空值须保证其跨可选GC仍有效。
 *
 * ## Returns
 * @return fail_object普通命中时NULL，否则真实Object_New返回的GC管理对象或空；抛错路径不普通返回。
 *
 * ## Constraints and lifecycle
 * 返回对象由runtime GC管理，不能C free；wrapper不分配替代对象或改变真实对象实现。其内部真实API调用在后续#define之前解析，避免递归。只覆盖文本include实例的Object_New边界，普通库公开入口不经此计数器。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:75；tests/core/ssa_runtime_objects_faults.c:76；tests/core/ssa_runtime_objects_faults.c:77；tests/core/ssa_runtime_objects_faults.c:124；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:550；tests/core/test_ssa_runtime_objects.c:205。
 */

/**
 * @fn TZrBool test_reserve(SZrState *state, SZrHashSet *set, TZrSize count)
 *
 * ## Summary
 * 在真实detached HashSet pair-pool预留前按需GC并注入一个失败序号。
 *
 * ## Parameters
 * @param[in,out] state 真实FullGC和HashSet预留使用的有效state。
 * @param[in,out] set 调用方持有的有效候选HashSet；未命中时由真实helper更新其存储。
 * @param[in] count 透传给EnsurePairPoolForElementCount的目标元素数量。
 *
 * ## Returns
 * @return 命中递增后的reserveCount时FALSE，否则真实预留helper的结果；不将自然失败与注入失败另行编码。
 *
 * ## Constraints and lifecycle
 * FullGC发生在reserveCount递增/比较前；命中时不调用真实预留。候选set及外部源的保活须由生产准备阶段保障，wrapper不拥有set。只控制这一pair-pool边界，不包含前面的bucket预留或其内部allocator；定义早于替换宏以避免递归。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:80；tests/core/ssa_runtime_objects_faults.c:81；tests/core/ssa_runtime_objects_faults.c:82；tests/core/ssa_runtime_objects_faults.c:83；tests/core/ssa_runtime_objects_faults.c:125；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:563；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:564；tests/core/test_ssa_runtime_objects.c:234。
 */

/**
 * @fn TZrBool test_remembered_reserve(SZrGlobalState *global, TZrSize count)
 *
 * ## Summary
 * 按持续开关短路remembered registry容量预留。
 *
 * ## Parameters
 * @param[in,out] global 未注入时交给真实GC registry helper的有效global。
 * @param[in] count 透传的目标容量需求。
 *
 * ## Returns
 * @return failRemembered为真时FALSE，否则真实registry helper结果。
 *
 * ## Constraints and lifecycle
 * 注入时完全跳过真实预留；开关不会被此调用清除，不分配/释放替代registry。global和其GC存储由runtime持有；仅text-included调用被重命名，其余linked GC路径保留真实行为。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:86；tests/core/ssa_runtime_objects_faults.c:87；tests/core/ssa_runtime_objects_faults.c:126；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:582；tests/core/test_ssa_runtime_objects.c:248。
 */

/**
 * @fn TZrBool test_pause(SZrState *state, TZrUInt32 timeout, SZrGcDomainPauseDiagnostic *diagnostic)
 *
 * ## Summary
 * 按持续开关拒绝或转发附着阶段的StopTheWorldBegin。
 *
 * ## Parameters
 * @param[in,out] state 未注入时交给真实暂停入口的有效state/domain。
 * @param[in] timeout 透传的暂停超时毫秒数。
 * @param[out] diagnostic 真实入口允许的可选诊断槽；注入短路分支不写此槽。
 *
 * ## Returns
 * @return failPause为真时FALSE，否则真实StopTheWorldBegin结果；本wrapper不另行获得/释放暂停。
 *
 * ## Constraints and lifecycle
 * TRUE的真实调用由生产worldPaused/cleanup成对End；FALSE注入不改已存在的暂停。diagnostic若非空须独立于仍被读写的runtime记录；不声称注入验证了真实超时或全部暂停状态。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:90；tests/core/ssa_runtime_objects_faults.c:91；tests/core/ssa_runtime_objects_faults.c:127；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:572；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:653；tests/core/test_ssa_runtime_objects.c:360。
 */

/**
 * @fn void test_barrier(SZrState *state, SZrRawObject *object, SZrRawObject *value)
 *
 * ## Summary
 * 在真实对象写屏障之前消费一次测试hook，保留真实barrier作为正常后续。
 *
 * ## Parameters
 * @param[in,out] state 同步hook和真实barrier使用的有效runtime state。
 * @param[in,out] object 正常分支交给真实写屏障的有效宿主对象；由runtime管理。
 * @param[in,out] value 可为空的借用目标对象；真实barrier会更新GC/ignored标记，NULL时真实屏障不工作。
 *
 * ## Returns
 * 无C返回值；hook正常完成后调用真实barrier再返回；hook若Throw则跳过该真实barrier语句并由外层TryRun恢复。
 *
 * ## Constraints and lifecycle
 * 文本包含生产正文中只有原型附接 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595 调用本替身；字段 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:615 调用独立翻译单元的 ZrCore_Gc_WriteBarrier，不进入本hook。
 * 先复制barrierHook并把全局hook置NULL，再以barrierContext同步调用，因此抛错不会遗留重复hook；context本身不自动清空或释放。它不转移对象/上下文所有权，context须存活且满足hook写入要求。真实barrier定义在宏前，避免递归。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:94；tests/core/ssa_runtime_objects_faults.c:95；tests/core/ssa_runtime_objects_faults.c:96；tests/core/ssa_runtime_objects_faults.c:97；tests/core/ssa_runtime_objects_faults.c:98；tests/core/ssa_runtime_objects_faults.c:128；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595；tests/core/test_ssa_runtime_objects.c:381；tests/core/ssa_runtime_objects_concurrency.c:270；zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1681；zr_vm_core/src/zr_vm_core/gc/gc_mark.c:1695。
 */

/**
 * @fn void test_mutation_end(SZrState *state, TZrBool locked)
 *
 * ## Summary
 * 在转发MutationEnd前记录过期locked token，避免故障测试触发真实未定义unlock。
 *
 * ## Parameters
 * @param[in,out] state 带有效gcDomain的state；在域锁保护下寻找其当前mutator记录。
 * @param[in] locked 生产调用方请求释放的mutation锁租约标志。
 *
 * ## Returns
 * 无C返回值；locked且查得depth0时增加invalidMutationEndCount并提前返回，否则调用真实MutationEnd。
 *
 * ## Constraints and lifecycle
 * 域锁只保护查找和深度快照，wrapper随后已解锁；测试的state/registration生命周期与同步协议仍由调用方保障。未找到记录也保留depth0；不是对所有锁错误的通用验证器。抑制错误释放不表示生产错误解锁被真实执行；计数0仅是该观测条件未命中。wrapper不拥有state/domain/锁，仅正常转发调用方租约。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:101；tests/core/ssa_runtime_objects_faults.c:103；tests/core/ssa_runtime_objects_faults.c:104；tests/core/ssa_runtime_objects_faults.c:107；tests/core/ssa_runtime_objects_faults.c:111；tests/core/ssa_runtime_objects_faults.c:112；tests/core/ssa_runtime_objects_faults.c:115；tests/core/ssa_runtime_objects_faults.c:118；tests/core/ssa_runtime_objects_faults.c:129；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:647；tests/core/test_ssa_runtime_objects.c:409。
 */

/**
 * @anchor zr_test_runtime_objects_fault_compilation_block
 *
 * ## Summary
 * 把Objects生产正文编译为独立故障实例，并严格限定八个宏替换的可观测边界。
 *
 * ## Constraints and lifecycle
 * 本块位于wrapper定义之后，include前配置入口名、Object_New、pair-pool、remembered预留、暂停Begin、RawObject_Barrier、MutationEnd与calloc八个替换。引用的生产C32个物理单元没有复制成新候选，不计第二次信用；编译实例只追加ssa_runtime_objects_materialize。真正对象/GC/Hash和linked MaterializeState/Owner/Liveness算法仍调用真实实现，不能把宏命中视作全allocator覆盖。此TU普通static故障配置不是线程隔离设施；并发helper仅在同步调用前后安排hook。Windows shared构建把此TU放入core_shared以访问private GC helper，其它配置放入测试target；原程序/宏/include均保留不改。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:121；tests/core/ssa_runtime_objects_faults.c:123；tests/core/ssa_runtime_objects_faults.c:130；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:666；tests/cmake/ssa-tests.cmake:311；tests/cmake/ssa-tests.cmake:313；tests/cmake/ssa-tests.cmake:316；tests/core/ssa_runtime_objects_faults.c:124；tests/core/ssa_runtime_objects_faults.c:125；tests/core/ssa_runtime_objects_faults.c:126；tests/core/ssa_runtime_objects_faults.c:127；tests/core/ssa_runtime_objects_faults.c:128；tests/core/ssa_runtime_objects_faults.c:129。
 */

/**
 * @def ZrCore_ExecIr_MaterializeObjects
 *
 * ## Summary
 * 为同物理生产入口生成可与原库API并存的独立故障实例符号。
 *
 * ## Constraints and lifecycle
 * 只重命名后续文本include中的入口定义；原public库定义及先前已包含的声明不被改写。alias调用来自主测试31处及concurrency271。生产函数定义仍属于Objects C同物理单元，不据此再计一次C函数信用。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:123；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:666。
 */

/**
 * @def ZrCore_Object_New
 *
 * ## Summary
 * 把候选GC对象创建边界接到对象序号/FullGC/Throw替身。
 *
 * ## Constraints and lifecycle
 * 真正Object_New在宏之前的wrapper正文中调用，避免递归；没有重命名GC/对象实现内部allocator。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:124；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:550。
 */

/**
 * @def ZrCore_HashSet_EnsurePairPoolForElementCount
 *
 * ## Summary
 * 把detached候选HashSet的pair-pool预留接到序号替身。
 *
 * ## Constraints and lifecycle
 * 只替换该预留入口；bucket预留和真实helper内部存储行为未被此宏整体模拟。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:125；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:564。
 */

/**
 * @def garbage_collector_ensure_remembered_registry_capacity
 *
 * ## Summary
 * 把物化预先remembered容量申请接到持续失败开关。
 *
 * ## Constraints and lifecycle
 * linked GC代码中的同名调用不因当前TU宏而重编译；只控制included物化的直接边界。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:126；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:582。
 */

/**
 * @def ZrCore_GcDomain_StopTheWorldBegin
 *
 * ## Summary
 * 把物化附着暂停入口接到可控拒绝替身。
 *
 * ## Constraints and lifecycle
 * 不替换StopTheWorldEnd，也不修改case中普通API获取的外层暂停；注入短路不会写pause diagnostic。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:127；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:570。
 */

/**
 * @def ZrCore_RawObject_Barrier
 *
 * ## Summary
 * 让原型附接的 RawObject_Barrier 先有机会消费一次同步测试hook。
 *
 * ## Constraints and lifecycle
 * 宏只替换文本包含正文在 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595 的原型附接 RawObject_Barrier；hook仅首个调用消费。字段 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:615 调用独立翻译单元的 Gc_WriteBarrier，不进入本替身；正常后续保留真实barrier，Throw会跳过当前真实调用。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:128；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595。
 */

/**
 * @def ZrCore_GcDomain_MutationEnd
 *
 * ## Summary
 * 在物化统一cleanup退役mutation token时加过期租约检测。
 *
 * ## Constraints and lifecycle
 * wrapper在locked且depth0时计数并跳过真实unlock；宏不改写exception恢复或GC内部其它TU的MutationEnd。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:129；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:647。
 */

/**
 * @def calloc
 *
 * ## Summary
 * 把included Objects C的直接native数组分配接到ordinal注入。
 *
 * ## Constraints and lifecycle
 * 五个必需work池及可选ignored快照可被控制；MaterializeState/Owner/Liveness/Hash/GC以及测试fixture自身calloc均在此宏范围外。free仍为真实C free。 对象式宏没有宏形参或运行期返回值；作用域从该define至本TU结束，源文件末尾文本include是唯一受控生产实例。wrapper定义早于所有alias，真实callee不会递归落回替身。
 *
 * ## References
 * tests/core/ssa_runtime_objects_faults.c:130；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:444；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:445；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:446；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:447；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:449；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:451。
 */
