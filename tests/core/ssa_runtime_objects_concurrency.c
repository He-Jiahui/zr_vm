#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "ssa_runtime_objects_concurrency.h"
#include "ssa_runtime_objects_faults.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/state.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef HANDLE ZrCollectorThread;
typedef volatile LONG ZrCollectorAtomic;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef pthread_t ZrCollectorThread;
typedef _Atomic int ZrCollectorAtomic;
#endif

enum {
    ZR_TEST_COLLECTOR_PAUSE_TIMEOUT_MS = 2000,
    ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS = 5000,
    ZR_TEST_COLLECTOR_ROOT_GROUPS = 3
};

typedef struct ZrCompetingCollector {
    SZrState state;
    ZrCollectorAtomic start;
    ZrCollectorAtomic stop;
    ZrCollectorAtomic requested;
    ZrCollectorAtomic materializationReturned;
    ZrCollectorAtomic done;
    TZrBool collected;
    TZrBool collectedAfterReturn;
    TZrBool hookObserved;
    TZrBool pauseAcquired;
    TZrUInt64 collectionsBefore;
    TZrUInt64 collectionsAfter;
} ZrCompetingCollector;

typedef struct ZrCollectorCallerRoots {
    SZrAotGcRootSlot *slots;
    SZrAotGcRootMap maps[ZR_TEST_COLLECTOR_ROOT_GROUPS];
    SZrAotGcRootFrame frames[ZR_TEST_COLLECTOR_ROOT_GROUPS];
    TZrBool pushed[ZR_TEST_COLLECTOR_ROOT_GROUPS];
} ZrCollectorCallerRoots;

static int collector_atomic_load(ZrCollectorAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void collector_atomic_store(ZrCollectorAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void collector_atomic_init(ZrCollectorAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    *value = 0;
#else
    atomic_init(value, 0);
#endif
}

static void collector_sleep(void) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(1u);
#else
    struct timespec duration = {0, 1000000L};
    (void)nanosleep(&duration, ZR_NULL);
#endif
}

static void collector_run(ZrCompetingCollector *context) {
    TZrUInt32 attempts;
    for (attempts = 0u; attempts < ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS; ++attempts) {
        if (collector_atomic_load(&context->start) ||
            collector_atomic_load(&context->stop)) break;
        collector_sleep();
    }
    if (collector_atomic_load(&context->start) &&
        !collector_atomic_load(&context->stop)) {
        collector_atomic_store(&context->requested, 1);
        /* Stay inactive until the hook: a running worker would park before
         * the materializer reaches its first barrier. An inactive attached
         * state may own a stop-the-world collection directly. */
        if (ZrCore_GcDomain_StopTheWorldBegin(
                    &context->state, ZR_TEST_COLLECTOR_PAUSE_TIMEOUT_MS, ZR_NULL)) {
            SZrGarbageCollector *collector = context->state.global->garbageCollector;
            TZrUInt64 before = collector->statsSnapshot.fullCollectionCount;
            context->pauseAcquired = ZR_TRUE;
            context->collectionsBefore = before;
            /* Current GC promotion/compaction preserves object addresses.
             * Assert the real protection boundary: no competing collection
             * may enter while attachment or temporary-root cleanup is live. */
            context->collectedAfterReturn =
                    collector_atomic_load(&context->materializationReturned) != 0;
            ZrCore_GarbageCollector_GcFull(&context->state, ZR_TRUE);
            context->collectionsAfter = collector->statsSnapshot.fullCollectionCount;
            context->collected = collector->statsSnapshot.fullCollectionCount == before + 1u;
            ZrCore_GcDomain_StopTheWorldEnd(&context->state);
        }
    }
    collector_atomic_store(&context->done, 1);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI collector_entry(LPVOID argument) {
    collector_run((ZrCompetingCollector *)argument);
    return 0;
}

static TZrBool collector_start(ZrCollectorThread *thread, ZrCompetingCollector *context) {
    *thread = CreateThread(ZR_NULL, 0u, collector_entry, context, 0u, ZR_NULL);
    return *thread != ZR_NULL;
}

static TZrBool collector_join(ZrCollectorThread thread) {
    DWORD result = WaitForSingleObject(thread, ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS);
    if (result != WAIT_OBJECT_0) {
        fputs("competing collector thread could not be joined\n", stderr);
        abort();
    }
    return CloseHandle(thread) != 0;
}
#else
static void *collector_entry(void *argument) {
    collector_run((ZrCompetingCollector *)argument);
    return ZR_NULL;
}

static TZrBool collector_start(ZrCollectorThread *thread, ZrCompetingCollector *context) {
    return pthread_create(thread, ZR_NULL, collector_entry, context) == 0;
}

static TZrBool collector_join(ZrCollectorThread thread) {
    if (pthread_join(thread, ZR_NULL) != 0) {
        fputs("competing collector thread could not be joined\n", stderr);
        abort();
    }
    return ZR_TRUE;
}
#endif

static void collector_barrier_hook(SZrState *state, void *argument) {
    ZrCompetingCollector *context = argument;
    TZrUInt32 attempts;
    collector_atomic_store(&context->start, 1);
    for (attempts = 0u; attempts < ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS; ++attempts) {
        SZrGcDomainMutatorSnapshot snapshot;
        ZrCore_GcDomain_GetMutatorSnapshot(state, &snapshot);
        if (collector_atomic_load(&context->requested) && snapshot.pauseRequested) {
            context->hookObserved = ZR_TRUE;
            return;
        }
        if (collector_atomic_load(&context->done)) return;
        collector_sleep();
    }
}

static TZrBool caller_root_push(SZrState *state, ZrCollectorCallerRoots *roots,
                               TZrUInt32 group, void *base) {
    if (roots->maps[group].rootCount == 0u) return ZR_TRUE;
    roots->pushed[group] = ZrCore_Gc_AotRootFramePush(
            state, &roots->frames[group], (TZrStackValuePointer)base, &roots->maps[group]);
    return roots->pushed[group];
}

static void caller_root_slot(SZrAotGcRootSlot *slot, size_t offset) {
    slot->locationKind = ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS;
    slot->frameByteOffset = (TZrUInt32)offset;
}

static TZrBool caller_roots_prepare(const SZrExecIrObjectMaterializationRequest *request,
                                   ZrCollectorCallerRoots *roots) {
    TZrUInt64 count = (TZrUInt64)request->valueCount + request->typeCount + request->target->capacity;
    TZrUInt32 index, used = 0u;
    if (count == 0u || count > SIZE_MAX / sizeof(*roots->slots) || count > UINT32_MAX ||
        request->valueCount > UINT32_MAX / sizeof(*request->values) ||
        request->typeCount > UINT32_MAX / sizeof(*request->types) ||
        request->target->capacity > UINT32_MAX / sizeof(*request->target->objects)) return ZR_FALSE;
    roots->slots = calloc((size_t)count, sizeof(*roots->slots));
    if (roots->slots == ZR_NULL) return ZR_FALSE;
    roots->maps[0].roots = roots->slots;
    for (index = 0u; index < request->valueCount; ++index) {
        if (request->values[index].isGarbageCollectable) {
            caller_root_slot(&roots->slots[used++],
                    (size_t)index * sizeof(*request->values) + offsetof(SZrTypeValue, value));
            ++roots->maps[0].rootCount;
        }
    }
    roots->maps[1].roots = &roots->slots[used];
    roots->maps[1].rootCount = request->typeCount;
    for (index = 0u; index < request->typeCount; ++index)
        caller_root_slot(&roots->slots[used++], (size_t)index * sizeof(*request->types) +
                         offsetof(SZrExecIrRuntimeTypeBinding, prototype));
    roots->maps[2].roots = &roots->slots[used];
    roots->maps[2].rootCount = request->target->count;
    for (index = 0u; index < request->target->capacity; ++index)
        caller_root_slot(&roots->slots[used++], (size_t)index * sizeof(*request->target->objects));
    return caller_root_push(request->state, roots, 0u, request->values) &&
           caller_root_push(request->state, roots, 1u, request->types) &&
           caller_root_push(request->state, roots, 2u, request->target->objects);
}

static TZrBool caller_roots_release(SZrState *state, ZrCollectorCallerRoots *roots) {
    TZrUInt32 group = ZR_TEST_COLLECTOR_ROOT_GROUPS;
    TZrBool success = ZR_TRUE;
    while (group != 0u) {
        --group;
        if (roots->pushed[group] && !ZrCore_Gc_AotRootFramePop(state, &roots->frames[group]))
            success = ZR_FALSE;
    }
    free(roots->slots);
    return success;
}

TZrBool ssa_runtime_objects_competing_collector(
        const SZrExecIrObjectMaterializationRequest *request,
        SZrExecIrDiagnostic *diagnostic) {
    ZrCompetingCollector context;
    ZrCollectorCallerRoots roots = {0};
    ZrCollectorThread thread;
    TZrBool success = ZR_FALSE, attached = ZR_FALSE, entered = ZR_FALSE;
    TZrBool materialized = ZR_FALSE;
    const char *stage = "caller roots";
    TZrUInt32 attempts, priorRootDepth;
    if (request == ZR_NULL || request->state == ZR_NULL || request->target == ZR_NULL ||
        request->target->count > request->target->capacity ||
        (request->valueCount != 0u && request->values == ZR_NULL) ||
        (request->typeCount != 0u && request->types == ZR_NULL) ||
        (request->target->capacity != 0u && request->target->objects == ZR_NULL)) return ZR_FALSE;
    memset(&context, 0, sizeof(context));
    collector_atomic_init(&context.start);
    collector_atomic_init(&context.stop);
    collector_atomic_init(&context.requested);
    collector_atomic_init(&context.materializationReturned);
    collector_atomic_init(&context.done);
    priorRootDepth = ZrCore_Gc_AotRootFrameDepth(request->state);
    if (!caller_roots_prepare(request, &roots)) goto finish;
    stage = "caller enter";
    entered = ZrCore_GcDomain_MutatorEnter(request->state);
    if (!entered) goto finish;
    ZrCore_RawObject_Construct(&context.state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    context.state.global = request->state->global;
    stage = "worker attach";
    attached = ZrCore_GcDomain_MutatorAttach(request->state, &context.state);
    if (!attached) goto finish;
    stage = "worker start";
    if (!collector_start(&thread, &context)) goto finish;

    stage = "materialization/collection";
    ssa_runtime_objects_set_barrier_hook(collector_barrier_hook, &context);
    success = ssa_runtime_objects_materialize(request, diagnostic);
    collector_atomic_store(&context.materializationReturned, 1);
    materialized = success;
    ssa_runtime_objects_set_barrier_hook(ZR_NULL, ZR_NULL);
    /* Our extra RUNNING scope prevents the waiting collector from taking the
     * world between publication and registration of the new output roots. */
    roots.maps[2].rootCount = request->target->count;
    if (!roots.pushed[2] && !caller_root_push(request->state, &roots, 2u, request->target->objects))
        success = ZR_FALSE;
    if (!collector_atomic_load(&context.start)) collector_atomic_store(&context.stop, 1);
    for (attempts = 0u; attempts < ZR_TEST_COLLECTOR_WAIT_TIMEOUT_MS; ++attempts) {
        if (collector_atomic_load(&context.done)) break;
        (void)ZrCore_GcDomain_MutatorPoll(request->state);
        collector_sleep();
    }
    if (!collector_atomic_load(&context.done)) {
        /* Returning would let Unity tear down a domain still used by a live
         * collector. Fail the process instead of leaving a dangling worker. */
        fputs("competing collector did not finish within its bounded wait\n", stderr);
        abort();
    }
    if (!collector_join(thread)) success = ZR_FALSE;
    success = success && context.hookObserved && context.collected && context.collectedAfterReturn;
finish:
    if (attached) ZrCore_GcDomain_MutatorDetach(&context.state);
    if (!caller_roots_release(request->state, &roots)) success = ZR_FALSE;
    if (ZrCore_Gc_AotRootFrameDepth(request->state) != priorRootDepth) success = ZR_FALSE;
    if (entered) ZrCore_GcDomain_MutatorLeave(request->state);
    if (!success) {
        fprintf(stderr,
                "competing collector failed: stage=%s materialized=%d "
                "hook=%d pause=%d collected=%d afterReturn=%d collections=%llu->%llu "
                "roots=%u->%u diagnostic=%d\n",
                stage, (int)materialized, (int)context.hookObserved,
                (int)context.pauseAcquired, (int)context.collected, (int)context.collectedAfterReturn,
                (unsigned long long)context.collectionsBefore,
                (unsigned long long)context.collectionsAfter,
                (unsigned)priorRootDepth, (unsigned)ZrCore_Gc_AotRootFrameDepth(request->state),
                diagnostic != ZR_NULL ? (int)diagnostic->code : -1);
    }
    return success;
}

/**
 * @def _POSIX_C_SOURCE
 *
 * ## Summary
 * 在系统头包含前为非 Windows 分支选择 POSIX.1-2008 声明。
 *
 * ## Constraints and lifecycle
 * 仅当未定义 ZR_PLATFORM_WIN 且宿主未预定义 _POSIX_C_SOURCE 时定义为 200809L；保留宿主已有值，不能保证更低宿主值也暴露所需 API。此宏仅影响本翻译单元的系统头，不是运行期平台切换。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:1；tests/core/ssa_runtime_objects_concurrency.c:2；tests/core/ssa_runtime_objects_concurrency.c:23；tests/core/ssa_runtime_objects_concurrency.c:25；tests/core/ssa_runtime_objects_concurrency.c:87。
 */

/**
 * @typedef HANDLE ZrCollectorThread
 *
 * ## Summary
 * 表示收集器工作线程的本平台令牌，由启动函数产生并在 join 后退役。
 *
 * ## Constraints and lifecycle
 * Windows 分支是 HANDLE，成功启动后由 collector_join 等待并 CloseHandle；POSIX 分支是 pthread_t，由 pthread_join 消耗可连接线程。仅在 collector_start 返回 TRUE 后使用；不能把令牌当 SZrState 或跨平台持久数据。Windows CloseHandle 失败会返回 FALSE，当前 helper 没有额外重试；POSIX join 失败直接 abort。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:20；tests/core/ssa_runtime_objects_concurrency.c:130；tests/core/ssa_runtime_objects_concurrency.c:135；tests/core/ssa_runtime_objects_concurrency.c:240；tests/core/ssa_runtime_objects_concurrency.c:267；tests/core/ssa_runtime_objects_concurrency.c:292。
 */

/**
 * @typedef pthread_t ZrCollectorThread
 *
 * ## Summary
 * 表示收集器工作线程的本平台令牌，由启动函数产生并在 join 后退役。
 *
 * ## Constraints and lifecycle
 * Windows 分支是 HANDLE，成功启动后由 collector_join 等待并 CloseHandle；POSIX 分支是 pthread_t，由 pthread_join 消耗可连接线程。仅在 collector_start 返回 TRUE 后使用；不能把令牌当 SZrState 或跨平台持久数据。Windows CloseHandle 失败会返回 FALSE，当前 helper 没有额外重试；POSIX join 失败直接 abort。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:26；tests/core/ssa_runtime_objects_concurrency.c:149；tests/core/ssa_runtime_objects_concurrency.c:153；tests/core/ssa_runtime_objects_concurrency.c:240；tests/core/ssa_runtime_objects_concurrency.c:267；tests/core/ssa_runtime_objects_concurrency.c:292。
 */

/**
 * @typedef volatile LONG ZrCollectorAtomic
 *
 * ## Summary
 * 保存两个线程间的整型握手标志，按平台通过统一的 load/store/init 包装访问。
 *
 * ## Constraints and lifecycle
 * Windows 是 volatile LONG，但并发原子性依赖 Interlocked 操作，不能仅凭 volatile 读写共享字段；POSIX 是 _Atomic int，读取采用 acquire，写入采用 release。五个实例均在线程发布前显式初始化为0，之后只通过包装访问，不承担普通指针或 GC 根所有权。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:21；tests/core/ssa_runtime_objects_concurrency.c:38；tests/core/ssa_runtime_objects_concurrency.c:42；tests/core/ssa_runtime_objects_concurrency.c:58；tests/core/ssa_runtime_objects_concurrency.c:66；tests/core/ssa_runtime_objects_concurrency.c:74；tests/core/ssa_runtime_objects_concurrency.c:251；tests/core/ssa_runtime_objects_concurrency.c:255。
 */

/**
 * @typedef _Atomic int ZrCollectorAtomic
 *
 * ## Summary
 * 保存两个线程间的整型握手标志，按平台通过统一的 load/store/init 包装访问。
 *
 * ## Constraints and lifecycle
 * Windows 是 volatile LONG，但并发原子性依赖 Interlocked 操作，不能仅凭 volatile 读写共享字段；POSIX 是 _Atomic int，读取采用 acquire，写入采用 release。五个实例均在线程发布前显式初始化为0，之后只通过包装访问，不承担普通指针或 GC 根所有权。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:27；tests/core/ssa_runtime_objects_concurrency.c:38；tests/core/ssa_runtime_objects_concurrency.c:42；tests/core/ssa_runtime_objects_concurrency.c:58；tests/core/ssa_runtime_objects_concurrency.c:66；tests/core/ssa_runtime_objects_concurrency.c:74；tests/core/ssa_runtime_objects_concurrency.c:251；tests/core/ssa_runtime_objects_concurrency.c:255。
 */

/**
 * @anchor zr_test_competing_collector_constants
 *
 * ## Summary
 * 集中定义暂停请求预算、轮询次数/Windows 等待参数以及根组数。
 *
 * ## Constraints and lifecycle
 * 2000 用作 StopTheWorldBegin 的毫秒参数；5000 对三个轮询循环是最多尝试次数，各尝试调用约1ms休眠并可能因调度/中断变化，不是墙钟上界；只有 Windows WaitForSingleObject 直接接收该毫秒值。POSIX pthread_join 没有超时参数。三根组固定为 values、types、output，不是可配置数组宽度。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:30；tests/core/ssa_runtime_objects_concurrency.c:93；tests/core/ssa_runtime_objects_concurrency.c:105；tests/core/ssa_runtime_objects_concurrency.c:136；tests/core/ssa_runtime_objects_concurrency.c:154；tests/core/ssa_runtime_objects_concurrency.c:166；tests/core/ssa_runtime_objects_concurrency.c:224；tests/core/ssa_runtime_objects_concurrency.c:281。
 */

/**
 * @struct ZrCompetingCollector
 *
 * ## Summary
 * 在调用线程的栈上保存独立附着 state、线程握手和收集结果，供工作线程借用到 join 完成。
 *
 * ## Fields and invariants
 * start/stop/requested/materializationReturned/done 是握手标志；collected、collectedAfterReturn、pauseAcquired 和两个 collections 计数由 worker 在持有暂停时更新。hookObserved 记录调用方看见暂停请求；它与收集成功不是同一个条件。state/global 不拥有调用者域或 GC 对象；结构地址及内嵌 state 须稳定到线程结束及 detach。
 *
 * ## Constraints and lifecycle
 * state 只构造线程 raw-object 头、借用调用方 global 并附着同一 GC 域，不是拥有独立堆/完整执行栈的通用新 state，不能调用 State_Destroy。调用方先清零并初始化五个原子，再启动线程；worker 正常退出前 release 写 done，主线程 acquire 观察并 join 后读取 worker 的非原子结果。hookObserved 由调用方 hook 写入。任何正常返回均不得早于已成功启动线程的回收；超时通过 abort 避免栈 context 被仍运行的 worker 借用。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:36；tests/core/ssa_runtime_objects_concurrency.c:91；tests/core/ssa_runtime_objects_concurrency.c:121；tests/core/ssa_runtime_objects_concurrency.c:238；tests/core/ssa_runtime_objects_concurrency.c:250；tests/core/ssa_runtime_objects_concurrency.c:264；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_concurrency.c:295。
 */

/**
 * @struct ZrCollectorCallerRoots
 *
 * ## Summary
 * 拥有 caller 根槽数组并在本栈内保存三组根映射、帧节点和成功 push 标记。
 *
 * ## Fields and invariants
 * slots 分为运行时 values 中的 GC 载荷、所有类型 prototype、caller output 三组；maps[i].rootCount 与 roots 指向槽子区间，frames[i] 连接调用方 state 的根栈；pushed[i] 只说明本次曾成功 push，不是可重复释放的 ownership token。
 *
 * ## Constraints and lifecycle
 * 使用前须全零且不含既有活动帧。slots 是单次 calloc 的自有 C 存储；maps/frames 的地址被 state 的 AOT 根链借用，直到对应逆序 Pop 成功前都须稳定可读。每组仅成功 push 时标记；组2按容量预铺槽但仅 rootCount 个已发布对象可被扫描。release 只用于本次单次清理，不清零 slots 或 pushed，不能在未复位下重复 release/prepare。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:51；tests/core/ssa_runtime_objects_concurrency.c:178；tests/core/ssa_runtime_objects_concurrency.c:191；tests/core/ssa_runtime_objects_concurrency.c:223；tests/core/ssa_runtime_objects_concurrency.c:239；tests/core/ssa_runtime_objects_concurrency.c:277。
 */

/**
 * @fn int collector_atomic_load(ZrCollectorAtomic *value)
 *
 * ## Summary
 * 读取一个共享握手标志，供 worker 与调用线程确认开始、停止、请求、返回和完成。
 *
 * ## Parameters
 * @param[in,out] value 指向已在线程发布前初始化的非空原子单元；Windows 路径使用 compare-exchange，不能传只读存储。
 *
 * ## Returns
 * @return 当前读出的整型标志值；不校验指针，也不申请资源。
 *
 * ## Constraints and lifecycle
 * Windows 通过 InterlockedCompareExchange(value,0,0) 读取，值为0时包含一次写回；POSIX 采用 acquire load，与 release store 配对。不能以此替代普通非原子字段的发布协议；context 的 worker 结果仍只在 done 观察及 join 后由调用者消费。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:58；tests/core/ssa_runtime_objects_concurrency.c:60；tests/core/ssa_runtime_objects_concurrency.c:62；tests/core/ssa_runtime_objects_concurrency.c:121；tests/core/ssa_runtime_objects_concurrency.c:272；tests/core/ssa_runtime_objects_concurrency.c:292。
 */

/**
 * @fn void collector_atomic_store(ZrCollectorAtomic *value, int next)
 *
 * ## Summary
 * 发布一个握手标志，形成调用线程与 worker 的通信边界。
 *
 * ## Parameters
 * @param[in,out] value 已初始化且持续存活的非空共享原子单元。
 * @param[in] next 要发布的标志值；本协议的并发写入均使用1。
 *
 * ## Returns
 * 无 C 返回值；将 next 写入原子单元。
 *
 * ## Constraints and lifecycle
 * Windows 用 InterlockedExchange，POSIX 用 release store；初始化必须先于线程发布，运行期间不得用普通赋值重置这些单元。done 的发布发生在 worker 结果写入后，materializationReturned 的发布发生在物化 API 返回后。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:66；tests/core/ssa_runtime_objects_concurrency.c:68；tests/core/ssa_runtime_objects_concurrency.c:70；tests/core/ssa_runtime_objects_concurrency.c:100；tests/core/ssa_runtime_objects_concurrency.c:121；tests/core/ssa_runtime_objects_concurrency.c:165；tests/core/ssa_runtime_objects_concurrency.c:272；tests/core/ssa_runtime_objects_concurrency.c:280。
 */

/**
 * @fn void collector_atomic_init(ZrCollectorAtomic *value)
 *
 * ## Summary
 * 在线程建立前把一个握手单元初始化为0。
 *
 * ## Parameters
 * @param[out] value 尚未并发访问的非空原子单元；指向外层 context 的字段。
 *
 * ## Returns
 * 无 C 返回值；单元准备为初始未触发状态。
 *
 * ## Constraints and lifecycle
 * Windows 为发布前普通赋值，POSIX 调用 atomic_init；这不是运行期并发复位函数。外层依次初始化五个单元后才 collector_start，不分配或释放存储。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:74；tests/core/ssa_runtime_objects_concurrency.c:76；tests/core/ssa_runtime_objects_concurrency.c:78；tests/core/ssa_runtime_objects_concurrency.c:250；tests/core/ssa_runtime_objects_concurrency.c:267。
 */

/**
 * @fn void collector_sleep(void)
 *
 * ## Summary
 * 在有次数上限的握手轮询之间让出约1毫秒执行机会。
 *
 * ## Returns
 * 无 C 返回值；不报告睡眠完成、提前中断或调度延迟。
 *
 * ## Constraints and lifecycle
 * Windows 用 Sleep(1)，POSIX 请求 nanosleep({0,1000000}) 并忽略其结果及剩余时间。轮询常量是尝试次数，休眠和调度不构成精确墙钟截止时间；POSIX join 的等待也不因此具有超时上界。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:82；tests/core/ssa_runtime_objects_concurrency.c:84；tests/core/ssa_runtime_objects_concurrency.c:86；tests/core/ssa_runtime_objects_concurrency.c:87；tests/core/ssa_runtime_objects_concurrency.c:93；tests/core/ssa_runtime_objects_concurrency.c:166；tests/core/ssa_runtime_objects_concurrency.c:281；tests/core/ssa_runtime_objects_concurrency.c:154。
 */

/**
 * @fn void collector_run(ZrCompetingCollector *context)
 *
 * ## Summary
 * 等待原型附接屏障 hook 发出 start，在同域的 inactive worker state 上请求一次完整 GC，并记录该次收集是否晚于物化返回。
 *
 * ## Parameters
 * @param[in,out] context 外层栈 context，已清零、原子初始化并成功附着同一域；借用持续到 worker join。
 *
 * ## Returns
 * 无 C 返回值；正常路径最终 release 发布 done。只有暂停成功且完整回收计数恰加1才置 collected；停止、等待耗尽或暂停失败也会正常发布 done，结果仍可为假。
 *
 * ## Constraints and lifecycle
 * 本 worker 不先 MutatorEnter，避免把自己变成另一个必须响应的 RUNNING 阻塞者。start/stop 轮询最多5000次；requested 在 StopTheWorldBegin 前发布，暂停预算2000ms。外层额外 RUNNING 层直到物化返回并登记输出根后才 Poll，worker 获得暂停后读取 materializationReturned，然后 GcFull 自身嵌套 Begin/End，再释放外层暂停。工作线程不拥有 caller 根链，不创建/销毁完整 state；不捕获任意用户回调异常，不能承诺任意非局部退出都发布 done。只比较回收计数与有限图断言，不证明移动 GC 发生或地址被更新。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:91；tests/core/ssa_runtime_objects_concurrency.c:93；tests/core/ssa_runtime_objects_concurrency.c:100；tests/core/ssa_runtime_objects_concurrency.c:104；tests/core/ssa_runtime_objects_concurrency.c:114；tests/core/ssa_runtime_objects_concurrency.c:115；tests/core/ssa_runtime_objects_concurrency.c:118；tests/core/ssa_runtime_objects_concurrency.c:121；tests/core/ssa_runtime_objects_concurrency.c:259；tests/core/ssa_runtime_objects_concurrency.c:264；tests/core/ssa_runtime_objects_concurrency.c:272；tests/core/ssa_runtime_objects_concurrency.c:277；tests/core/ssa_runtime_objects_concurrency.c:283；zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:179；zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:180；zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:781；zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:852；zr_vm_core/src/zr_vm_core/gc/gc.c:672；zr_vm_core/src/zr_vm_core/gc/gc.c:715；zr_vm_core/src/zr_vm_core/gc/gc.c:716。
 */

/**
 * @fn DWORD WINAPI collector_entry(LPVOID argument)
 *
 * ## Summary
 * 把本平台线程 ABI 转接到 collector_run，向工作线程传递同一个外层栈 context。
 *
 * ## Parameters
 * @param[in,out] argument 实际指向 ZrCompetingCollector；启动者须保证其初始化、附着状态与存活期到 join 完成。
 *
 * ## Returns
 * @return Windows 正常回调返回 DWORD 0；POSIX 正常回调返回空指针。两个值仅是线程 ABI 退出值，不表示 GC 场景成功；实际结果存于 context。
 *
 * ## Constraints and lifecycle
 * 本符号在两互斥平台分支各有一个定义：Windows 使用 WINAPI/LPVOID，由 CreateThread 注册；POSIX 使用 void * 签名，由 pthread_create 注册。没有普通 C 直接调用，也没有复制或接管 context；collector_run 的成功/失败条件及未捕获异常限制原样适用。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:125；tests/core/ssa_runtime_objects_concurrency.c:126；tests/core/ssa_runtime_objects_concurrency.c:131；tests/core/ssa_runtime_objects_concurrency.c:145；tests/core/ssa_runtime_objects_concurrency.c:150；tests/core/ssa_runtime_objects_concurrency.c:267；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_faults.c:128；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595。
 */

/**
 * @fn void *collector_entry(void *argument)
 *
 * ## Summary
 * 把本平台线程 ABI 转接到 collector_run，向工作线程传递同一个外层栈 context。
 *
 * ## Parameters
 * @param[in,out] argument 实际指向 ZrCompetingCollector；启动者须保证其初始化、附着状态与存活期到 join 完成。
 *
 * ## Returns
 * @return Windows 正常回调返回 DWORD 0；POSIX 正常回调返回空指针。两个值仅是线程 ABI 退出值，不表示 GC 场景成功；实际结果存于 context。
 *
 * ## Constraints and lifecycle
 * 本符号在两互斥平台分支各有一个定义：Windows 使用 WINAPI/LPVOID，由 CreateThread 注册；POSIX 使用 void * 签名，由 pthread_create 注册。没有普通 C 直接调用，也没有复制或接管 context；collector_run 的成功/失败条件及未捕获异常限制原样适用。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:144；tests/core/ssa_runtime_objects_concurrency.c:126；tests/core/ssa_runtime_objects_concurrency.c:131；tests/core/ssa_runtime_objects_concurrency.c:145；tests/core/ssa_runtime_objects_concurrency.c:150；tests/core/ssa_runtime_objects_concurrency.c:267；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_faults.c:128；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595。
 */

/**
 * @fn TZrBool collector_start(ZrCollectorThread *thread, ZrCompetingCollector *context)
 *
 * ## Summary
 * 创建一个可连接的 worker，让平台入口借用本次 context。
 *
 * ## Parameters
 * @param[out] thread 非空线程令牌输出地址；仅 TRUE 返回后可传 collector_join。
 * @param[in,out] context 已附着并初始化的共享栈 context；线程可能在启动函数返回前开始读写，必须保持到 join。
 *
 * ## Returns
 * @return Windows CreateThread 产生非空 HANDLE 或 POSIX pthread_create 返回0时为 TRUE；否则 FALSE。Windows 失败把输出写为 NULL，POSIX 失败不保证输出令牌可用。
 *
 * ## Constraints and lifecycle
 * 仅建立线程，不承诺已观察 start、请求暂停、完成 GC 或结束。成功后外层负责等待完成、join 并 detach；失败时没有本函数授予的可连接线程，外层仍负责先前的附着及根资源。禁止把 context 作为临时值复制后提前退栈。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:130；tests/core/ssa_runtime_objects_concurrency.c:131；tests/core/ssa_runtime_objects_concurrency.c:150；tests/core/ssa_runtime_objects_concurrency.c:264；tests/core/ssa_runtime_objects_concurrency.c:286；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_concurrency.c:295。
 */

/**
 * @fn TZrBool collector_start(ZrCollectorThread *thread, ZrCompetingCollector *context)
 *
 * ## Summary
 * 创建一个可连接的 worker，让平台入口借用本次 context。
 *
 * ## Parameters
 * @param[out] thread 非空线程令牌输出地址；仅 TRUE 返回后可传 collector_join。
 * @param[in,out] context 已附着并初始化的共享栈 context；线程可能在启动函数返回前开始读写，必须保持到 join。
 *
 * ## Returns
 * @return Windows CreateThread 产生非空 HANDLE 或 POSIX pthread_create 返回0时为 TRUE；否则 FALSE。Windows 失败把输出写为 NULL，POSIX 失败不保证输出令牌可用。
 *
 * ## Constraints and lifecycle
 * 仅建立线程，不承诺已观察 start、请求暂停、完成 GC 或结束。成功后外层负责等待完成、join 并 detach；失败时没有本函数授予的可连接线程，外层仍负责先前的附着及根资源。禁止把 context 作为临时值复制后提前退栈。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:149；tests/core/ssa_runtime_objects_concurrency.c:131；tests/core/ssa_runtime_objects_concurrency.c:150；tests/core/ssa_runtime_objects_concurrency.c:264；tests/core/ssa_runtime_objects_concurrency.c:286；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_concurrency.c:295。
 */

/**
 * @fn TZrBool collector_join(ZrCollectorThread thread)
 *
 * ## Summary
 * 在线程完成观测后回收本次可连接线程，防止 worker 留用外层栈 context。
 *
 * ## Parameters
 * @param[in] thread 一次成功 collector_start 产生、尚未回收的本平台令牌。
 *
 * ## Returns
 * @return Windows 在线程确实结束后返回 CloseHandle 是否成功；等待结果异常或超时直接 abort。POSIX pthread_join 成功返回 TRUE，join 错误直接 abort。
 *
 * ## Constraints and lifecycle
 * Windows WaitForSingleObject 使用5000ms参数；POSIX pthread_join 本身没有超时，不能将外层有限次数的 done 轮询写成 POSIX join 墙钟上界。成功回收后不可重复使用令牌。Windows CloseHandle 返回 FALSE 时线程已结束但句柄释放未确认，本实现不重试；所有 abort 都结束进程，不是 Unity 仅退出当前 case 的 longjmp。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:135；tests/core/ssa_runtime_objects_concurrency.c:136；tests/core/ssa_runtime_objects_concurrency.c:139；tests/core/ssa_runtime_objects_concurrency.c:141；tests/core/ssa_runtime_objects_concurrency.c:154；tests/core/ssa_runtime_objects_concurrency.c:156；tests/core/ssa_runtime_objects_concurrency.c:281；tests/core/ssa_runtime_objects_concurrency.c:286；tests/core/ssa_runtime_objects_concurrency.c:290。
 */

/**
 * @fn TZrBool collector_join(ZrCollectorThread thread)
 *
 * ## Summary
 * 在线程完成观测后回收本次可连接线程，防止 worker 留用外层栈 context。
 *
 * ## Parameters
 * @param[in] thread 一次成功 collector_start 产生、尚未回收的本平台令牌。
 *
 * ## Returns
 * @return Windows 在线程确实结束后返回 CloseHandle 是否成功；等待结果异常或超时直接 abort。POSIX pthread_join 成功返回 TRUE，join 错误直接 abort。
 *
 * ## Constraints and lifecycle
 * Windows WaitForSingleObject 使用5000ms参数；POSIX pthread_join 本身没有超时，不能将外层有限次数的 done 轮询写成 POSIX join 墙钟上界。成功回收后不可重复使用令牌。Windows CloseHandle 返回 FALSE 时线程已结束但句柄释放未确认，本实现不重试；所有 abort 都结束进程，不是 Unity 仅退出当前 case 的 longjmp。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:153；tests/core/ssa_runtime_objects_concurrency.c:136；tests/core/ssa_runtime_objects_concurrency.c:139；tests/core/ssa_runtime_objects_concurrency.c:141；tests/core/ssa_runtime_objects_concurrency.c:154；tests/core/ssa_runtime_objects_concurrency.c:156；tests/core/ssa_runtime_objects_concurrency.c:281；tests/core/ssa_runtime_objects_concurrency.c:286；tests/core/ssa_runtime_objects_concurrency.c:290。
 */

/**
 * @fn void collector_barrier_hook(SZrState *state, void *argument)
 *
 * ## Summary
 * 在首个原型附接屏障处启动竞争 collector，并暂候 worker 意向标志与域 pauseRequested 同时可见。
 *
 * ## Parameters
 * @param[in,out] state 发出屏障的 caller state；其域快照用于判断 pauseRequested，须与 context 的域匹配。
 * @param[in,out] argument 与 worker 共用的非空 ZrCompetingCollector；hook 写 start 与 hookObserved。
 *
 * ## Returns
 * 无 C 返回值；观察到 requested 且 pauseRequested 时置 hookObserved 后返回；worker 已完成或轮询耗尽时直接返回，并不报告单独失败码。
 *
 * ## Constraints and lifecycle
 * 此回调由故障 TU 的 one-shot test_barrier 先取出再清空 hook 后调用，发生在文本包含的 Objects 原型 RawObject_Barrier 调用处。Objects 的字段 Gc_WriteBarrier 进入已链接 gc.c/Value_Barrier，不受这个 RawObject_Barrier 宏替换；不能从 case 名称推出字段屏障命中。hook 不 Poll 或释放物化期间的暂停/锁；requested 在 worker 调用 Begin 前发布，而本 hook 所在的原型附接本来就持有物化线程的暂停；snapshot.pauseRequested 可来自这层暂停。两条件同时可见并不证明 worker 已取得或拥有暂停，实际取得及晚于 API 返回由 worker 的 Begin/collectedAfterReturn 与外层最终组合另行检查。最多5000次轮询，超时只保留未观察状态给外层最终判定。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:162；tests/core/ssa_runtime_objects_concurrency.c:165；tests/core/ssa_runtime_objects_concurrency.c:168；tests/core/ssa_runtime_objects_concurrency.c:169；tests/core/ssa_runtime_objects_concurrency.c:170；tests/core/ssa_runtime_objects_concurrency.c:173；tests/core/ssa_runtime_objects_concurrency.c:174；tests/core/ssa_runtime_objects_concurrency.c:270；tests/core/ssa_runtime_objects_faults.c:52；tests/core/ssa_runtime_objects_faults.c:95；tests/core/ssa_runtime_objects_faults.c:96；tests/core/ssa_runtime_objects_faults.c:97；tests/core/ssa_runtime_objects_faults.c:128；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:615；zr_vm_core/src/zr_vm_core/gc/gc.c:1100；zr_vm_core/src/zr_vm_core/gc/gc.c:1102。
 */

/**
 * @fn TZrBool caller_root_push(SZrState *state, ZrCollectorCallerRoots *roots, TZrUInt32 group, void *base)
 *
 * ## Summary
 * 为一个非空 caller 根组借用帧节点和根映射，空组不入链。
 *
 * ## Parameters
 * @param[in,out] state 根栈所属的已附着 caller state；由当前调用线程按 LIFO 操作。
 * @param[in,out] roots 本次根记录；所选组尚未活动，maps 已准备且存储稳定。
 * @param[in] group 已由调用方限定为0、1或2的组号；内部不做下标校验。
 * @param[in,out] base 根槽偏移所基于的可写运行时数组；非空组须非空且覆盖所有偏移。
 *
 * ## Returns
 * @return 空组直接 TRUE 且不改 pushed；非空组返回 AotRootFramePush 结果，并将该结果写入 pushed[group]。失败不接管或释放 slots。
 *
 * ## Constraints and lifecycle
 * Push 只验证帧/映射的部分形状，不证明 base 存储大小或 GC 对象有效。成功后 state 借用 roots.frames/maps/slots 及 base，直到同线程逆序 Pop 前不得移动或释放；GC 可以通过根位置更新指针载荷。空的 output 组可在物化成功后补入，已入链 output 组只在额外 RUNNING 保护下修改 rootCount。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:178；tests/core/ssa_runtime_objects_concurrency.c:180；tests/core/ssa_runtime_objects_concurrency.c:181；tests/core/ssa_runtime_objects_concurrency.c:183；tests/core/ssa_runtime_objects_concurrency.c:277；zr_vm_core/src/zr_vm_core/gc/gc.c:257；zr_vm_core/src/zr_vm_core/gc/gc.c:261；zr_vm_core/src/zr_vm_core/gc/gc.c:273；zr_vm_core/src/zr_vm_core/gc/gc.c:282。
 */

/**
 * @fn void caller_root_slot(SZrAotGcRootSlot *slot, size_t offset)
 *
 * ## Summary
 * 把一个已清零的槽配置为基于根帧 base 的本地地址偏移。
 *
 * ## Parameters
 * @param[in,out] slot 非空、已在 calloc 根槽数组内的槽；本函数只写 locationKind 与 frameByteOffset。
 * @param[in] offset 调用方已保证可表示为 TZrUInt32 且定位完整可写根载荷的字节偏移。
 *
 * ## Returns
 * 无 C 返回值；写入 LOCAL_ADDRESS 与截断转换后的32位偏移。
 *
 * ## Constraints and lifecycle
 * 本函数不验证指针、offset 溢出或对象种类；prepare 先约束数组总字节数，循环索引及 offsetof 位于元素范围内，保证当前三组调用的偏移可表示。单独初始化槽不会注册 GC 根；必须由准备好的 map/frame 成功 Push 后才生效。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:186；tests/core/ssa_runtime_objects_concurrency.c:187；tests/core/ssa_runtime_objects_concurrency.c:188；tests/core/ssa_runtime_objects_concurrency.c:195；tests/core/ssa_runtime_objects_concurrency.c:196；tests/core/ssa_runtime_objects_concurrency.c:197；tests/core/ssa_runtime_objects_concurrency.c:198；tests/core/ssa_runtime_objects_concurrency.c:204；tests/core/ssa_runtime_objects_concurrency.c:205；tests/core/ssa_runtime_objects_concurrency.c:212；tests/core/ssa_runtime_objects_concurrency.c:213；tests/core/ssa_runtime_objects_concurrency.c:217。
 */

/**
 * @fn TZrBool caller_roots_prepare(const SZrExecIrObjectMaterializationRequest *request, ZrCollectorCallerRoots *roots)
 *
 * ## Summary
 * 在 caller 进入 GC-aware RUNNING 层之前为 values、类型 prototype 和旧 output 预备并压入三组根。
 *
 * ## Parameters
 * @param[in] request 非空且 records/数组稳定的当前测试请求；state/target 非空，计数与可写 arrays 满足外层及运行时请求契约。
 * @param[in,out] roots 全零、尚未拥有 slots 或活动帧的本次记录；成功或局部失败后均交给 caller_roots_release。
 *
 * ## Returns
 * @return 总槽数/偏移可表示、calloc 成功且全部非空组 Push 成功时 TRUE；否则 FALSE。后续组失败可保留已成功 Push 的前缀与 slots，本函数不回滚。
 *
 * ## Constraints and lifecycle
 * count64 覆盖 valueCount+typeCount+output.capacity；拒绝0、SIZE_MAX/slot 与 UINT32 上界及三种数组字节数不可表示。它不认证分配来源、内存可读性、任意 alias 或完整 logical checkpoint。会读取所有 values 的 isGarbageCollectable，故要求这些 tag 都可读，不是跳过 inactive-owner 毒化 cell 的通用适配器。types 的每个 prototype 均列根；output 按 capacity 铺偏移，但起初仅旧 count 个槽参与扫描。slots 是本 TU 的普通 calloc，不受另一 fault TU 对其文本包含生产 C 的 calloc 宏替换。所有成功帧借用的 metadata/arrays 保留到 release。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:191；tests/core/ssa_runtime_objects_concurrency.c:193；tests/core/ssa_runtime_objects_concurrency.c:195；tests/core/ssa_runtime_objects_concurrency.c:198；tests/core/ssa_runtime_objects_concurrency.c:199；tests/core/ssa_runtime_objects_concurrency.c:203；tests/core/ssa_runtime_objects_concurrency.c:205；tests/core/ssa_runtime_objects_concurrency.c:210；tests/core/ssa_runtime_objects_concurrency.c:215；tests/core/ssa_runtime_objects_concurrency.c:216；tests/core/ssa_runtime_objects_concurrency.c:218；tests/core/ssa_runtime_objects_concurrency.c:219；tests/core/ssa_runtime_objects_concurrency.c:220；tests/core/ssa_runtime_objects_concurrency.c:245；tests/core/ssa_runtime_objects_concurrency.c:249；tests/core/ssa_runtime_objects_concurrency.c:296；tests/core/ssa_runtime_objects_faults.c:130；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_runtime.h:35；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_runtime.h:39。
 */

/**
 * @fn TZrBool caller_roots_release(SZrState *state, ZrCollectorCallerRoots *roots)
 *
 * ## Summary
 * 按 output、types、values 的逆序弹出曾成功 Push 的 caller 帧，并释放本次根槽数组。
 *
 * ## Parameters
 * @param[in,out] state 与 Push 相同的 caller state；其根栈上本次帧须保持同线程 LIFO 次序。
 * @param[in,out] roots 本次零初始化记录或 prepare 的完整/局部结果，且尚未清理；不拥有运行时 values/types/output 数组。
 *
 * ## Returns
 * @return 所有需要的 Pop 均成功时 TRUE；任一 Pop 失败返回 FALSE，但仍继续检查其余组并 free slots。
 *
 * ## Constraints and lifecycle
 * 只释放 slots，不释放 values/types/output、state 或 context；不重置 slots/pushed，因此不能重复 release。Pop 失败时底层不修复根链，本函数仍释放 slots，不能把 FALSE 当作已恢复可安全继续扫描的保证。当前唯一 caller 先让 production cleanup 弹出其后压入的帧，再等待 worker结束/join后进入此处；worker 使用另一 state，Unity 断言在 helper 返回后，满足正常同线程 LIFO。此约束已闭合，不把违反前提的假设标成既有 BUG/TODO。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:223；tests/core/ssa_runtime_objects_concurrency.c:224；tests/core/ssa_runtime_objects_concurrency.c:228；tests/core/ssa_runtime_objects_concurrency.c:231；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_concurrency.c:295；tests/core/ssa_runtime_objects_concurrency.c:296；tests/core/ssa_runtime_objects_concurrency.c:297；zr_vm_core/src/zr_vm_core/gc/gc.c:281；zr_vm_core/src/zr_vm_core/gc/gc.c:282；zr_vm_core/src/zr_vm_core/gc/gc.c:283；zr_vm_core/src/zr_vm_core/gc/gc.c:287；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:649；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:651；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:46；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:47；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:73；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:74；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:89；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:90。
 */

/**
 * @fn TZrBool ssa_runtime_objects_competing_collector(const SZrExecIrObjectMaterializationRequest *request, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 串行 Unity 场景中保持 caller 输入及输出根，安排竞争线程在首个原型附接屏障请求 GC，并在物化返回、输出登记和 worker 回收后报告组合结果。
 *
 * ## Parameters
 * @param[in] request 完整存活到返回的测试请求记录；嵌套 caller state 与可写 values/types/target 存储仍由 caller 拥有，须适合根登记和实际物化。
 * @param[in,out] diagnostic 可为空的独立、已初始化诊断 slot；只有被调物化 API 按其分支写入，本 helper 的前置/晚期失败不统一清零或设置它，失败日志可能读取其既有 code。
 *
 * ## Returns
 * @return 物化成功、hook 观察暂停、worker 完整回收计数恰增1且该观察晚于 API 返回，并且线程/根收尾检查成功时 TRUE；任一条件失败返回 FALSE，部分失控等待直接 abort。FALSE 不保证 target 保持调用前内容。
 *
 * ## Constraints and lifecycle
 * 本函数是当前有效 fixture 的并发验证桥，不是任意请求的完整验证器：外层仅检查部分 NULL/count/capacity，prepare 随后会读取全部 value tag。request 记录可 const 借用，但其 state、values/prototype 指针载荷与 target 输出按运行时契约可修改；输入记录、function/map/池和诊断须满足被调接口的稳定可读及不相互覆盖前提，diagnostic 可空，非空时须是独立可读写 slot。全局 fault hook 不支持并发重配，本 helper 在同域单一测试协议内串行使用。先根化输入再 Enter，worker 附着后保持 inactive；物化 API 的内部暂停/标记锁由其本体释放，helper 不持有或消费其锁 token。API 返回后 release 发布 materializationReturned，再在额外 RUNNING 层下登记新 output 根，随后 Poll 让 worker收集；等待/join 完成才 detach、逆序 Pop/free caller 槽和 Leave。正常清理不表示任意非局部退出都受保护；worker超时及 OS join 错误使用 abort 而非 Unity case longjmp。晚期观察、线程句柄关闭或根平衡失败可在 target 已发布后使返回 FALSE，不回滚新 target；GC 也可更新源 payload。无 hook/无实际 collection 或 collection 早于返回均使组合结果为假。当前 hook 仅覆盖原型 RawObject_Barrier，不覆盖跨 TU 字段 Gc_WriteBarrier，也不证明发生移动 GC。
 *
 * ## References
 * tests/core/ssa_runtime_objects_concurrency.c:235；tests/core/ssa_runtime_objects_concurrency.c:245；tests/core/ssa_runtime_objects_concurrency.c:257；tests/core/ssa_runtime_objects_concurrency.c:259；tests/core/ssa_runtime_objects_concurrency.c:264；tests/core/ssa_runtime_objects_concurrency.c:267；tests/core/ssa_runtime_objects_concurrency.c:270；tests/core/ssa_runtime_objects_concurrency.c:271；tests/core/ssa_runtime_objects_concurrency.c:272；tests/core/ssa_runtime_objects_concurrency.c:274；tests/core/ssa_runtime_objects_concurrency.c:277；tests/core/ssa_runtime_objects_concurrency.c:278；tests/core/ssa_runtime_objects_concurrency.c:280；tests/core/ssa_runtime_objects_concurrency.c:283；tests/core/ssa_runtime_objects_concurrency.c:286；tests/core/ssa_runtime_objects_concurrency.c:290；tests/core/ssa_runtime_objects_concurrency.c:292；tests/core/ssa_runtime_objects_concurrency.c:293；tests/core/ssa_runtime_objects_concurrency.c:295；tests/core/ssa_runtime_objects_concurrency.c:296；tests/core/ssa_runtime_objects_concurrency.c:297；tests/core/ssa_runtime_objects_concurrency.c:298；tests/core/ssa_runtime_objects_concurrency.c:309；tests/core/ssa_runtime_objects_concurrency.h:9；tests/core/test_ssa_runtime_objects.c:439；tests/core/test_ssa_runtime_objects.c:442；tests/core/test_ssa_runtime_objects.c:444；tests/core/test_ssa_runtime_objects.c:445；tests/core/test_ssa_runtime_objects.c:831；tests/core/ssa_runtime_objects_faults.c:128；tests/core/ssa_runtime_objects_faults.c:131；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:595；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:615；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:725；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:727。
 */
