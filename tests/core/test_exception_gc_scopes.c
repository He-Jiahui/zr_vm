#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"
#include "gc/gc_domain_internal.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef volatile LONG ZrScopeAtomic;
typedef HANDLE ZrScopeThread;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef _Atomic int ZrScopeAtomic;
typedef pthread_t ZrScopeThread;
#endif

#define ZR_SCOPE_WAIT_ATTEMPTS 2000u
#define ZR_SCOPE_LOCK_OBSERVATION_MS 100u
#define ZR_SCOPE_MAJOR_DRAIN_LIMIT 4096u

typedef struct ZrScopeCallback {
    EZrGcNativeSafepointMode mode;
    TZrBool enterExecution;
    TZrBool returnNormally;
    TZrBool executionEntered;
    TZrBool nativeEntered;
    TZrBool firstMutationLocked;
    TZrBool secondMutationLocked;
} ZrScopeCallback;

typedef struct ZrScopeWorker {
    SZrState state;
    ZrScopeAtomic started;
    ZrScopeAtomic acquired;
    ZrScopeAtomic exited;
} ZrScopeWorker;

static SZrState *g_state;

static void scope_fail_process(const char *reason) {
    (void)fprintf(stderr, "exception_gc_scopes: %s\n", reason);
    (void)fflush(stderr);
    _Exit(EXIT_FAILURE);
}

static int scope_atomic_load(ZrScopeAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void scope_atomic_store(ZrScopeAtomic *value, int next) {
#if defined(ZR_PLATFORM_WIN)
    InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void scope_sleep_ms(TZrUInt32 milliseconds) {
#if defined(ZR_PLATFORM_WIN)
    Sleep(milliseconds);
#else
    struct timespec duration;
    duration.tv_sec = (time_t)(milliseconds / 1000u);
    duration.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    (void)nanosleep(&duration, ZR_NULL);
#endif
}

static TZrBool scope_wait_flag(ZrScopeAtomic *flag) {
    for (TZrUInt32 attempt = 0u; attempt < ZR_SCOPE_WAIT_ATTEMPTS; ++attempt) {
        if (scope_atomic_load(flag)) return ZR_TRUE;
        scope_sleep_ms(1u);
    }
    return (TZrBool)scope_atomic_load(flag);
}

static SZrGcDomainMutatorRecord scope_read_record(SZrState *state) {
    SZrGcDomainMutatorRecord result = {0};
    SZrGcDomain *domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    for (TZrSize index = 0u; index < domain->mutatorLength; ++index) {
        if (domain->mutators[index].state == state) {
            result = domain->mutators[index];
            break;
        }
    }
    ZrCore_GcDomain_Unlock(domain);
    return result;
}

static void scope_start_major(void) {
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    ZrCore_GarbageCollector_SetWorkerCount(g_state->global, 1u);
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_ScheduleCollection(
            g_state->global, ZR_GARBAGE_COLLECT_COLLECTION_KIND_MAJOR);
    ZrCore_GarbageCollector_GcStep(g_state);
    if (!collector->concurrentMajorActive) {
        scope_fail_process("real concurrent major did not start");
    }
}

/* Failure observations are taken before this owner-thread cleanup, allowing
 * depth regressions to report through Unity without destroying a locked VM. */
static void scope_cleanup_caller(void) {
    SZrGcDomainMutatorRecord record = scope_read_record(g_state);
    for (TZrUInt32 depth = record.mutationDepth; depth > 0u; --depth)
        ZrCore_GcDomain_MutationEnd(g_state, ZR_TRUE);
    record = scope_read_record(g_state);
    for (TZrUInt32 depth = record.nativeDepth; depth > 0u; --depth)
        ZrCore_GcDomain_NativeLeave(g_state);
    record = scope_read_record(g_state);
    for (TZrUInt32 depth = record.executionDepth; depth > 0u; --depth)
        ZrCore_GcDomain_MutatorLeave(g_state);
}

static void scope_finish_major(void) {
    SZrGarbageCollector *collector = g_state->global->garbageCollector;
    for (TZrUInt32 step = 0u;
         collector->concurrentMajorActive && step < ZR_SCOPE_MAJOR_DRAIN_LIMIT;
         ++step) {
        ZrCore_GarbageCollector_GcStep(g_state);
    }
    if (collector->concurrentMajorActive)
        scope_fail_process("major did not drain before state cleanup");
}

static void scope_callback(SZrState *state, TZrPtr arguments) {
    ZrScopeCallback *context = (ZrScopeCallback *)arguments;
    if (context->enterExecution)
        context->executionEntered = ZrCore_GcDomain_MutatorEnter(state);
    context->nativeEntered = ZrCore_GcDomain_NativeEnter(state, context->mode);
    context->firstMutationLocked = ZrCore_GcDomain_MutationBegin(state);
    context->secondMutationLocked = ZrCore_GcDomain_MutationBegin(state);
    if (!context->returnNormally) {
        /* INVALID avoids allocating an Error while testing the unwind itself. */
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_INVALID);
    }
    ZrCore_GcDomain_MutationEnd(state, context->secondMutationLocked);
    ZrCore_GcDomain_MutationEnd(state, context->firstMutationLocked);
    if (context->nativeEntered) ZrCore_GcDomain_NativeLeave(state);
    if (context->executionEntered) ZrCore_GcDomain_MutatorLeave(state);
}

static void scope_assert_record_equal(
        const SZrGcDomainMutatorRecord *expected,
        const SZrGcDomainMutatorRecord *actual) {
    TEST_ASSERT_EQUAL_UINT64(expected->mutatorId, actual->mutatorId);
    TEST_ASSERT_EQUAL_UINT32(expected->executionDepth, actual->executionDepth);
    TEST_ASSERT_EQUAL_UINT32(expected->nativeDepth, actual->nativeDepth);
    TEST_ASSERT_EQUAL_UINT32(expected->mutationDepth, actual->mutationDepth);
    TEST_ASSERT_EQUAL_INT(expected->nativeMode, actual->nativeMode);
    TEST_ASSERT_EQUAL_INT(expected->status, actual->status);
    TEST_ASSERT_EQUAL_INT(expected->nativeEnteredFromInactive,
                          actual->nativeEnteredFromInactive);
}

static void scope_run_case(TZrBool outerExecution, TZrBool outerNative,
                           TZrBool outerMutation, EZrGcNativeSafepointMode mode,
                           TZrBool returnNormally) {
    ZrScopeCallback context = {0};
    SZrGcDomainMutatorRecord before;
    SZrGcDomainMutatorRecord after;
    EZrThreadStatus status;
    scope_start_major();
    if (outerExecution && !ZrCore_GcDomain_MutatorEnter(g_state))
        scope_fail_process("outer execution entry failed");
    if (outerNative && !ZrCore_GcDomain_NativeEnter(g_state, mode))
        scope_fail_process("outer native entry failed");
    if (outerMutation && !ZrCore_GcDomain_MutationBegin(g_state))
        scope_fail_process("outer mutation entry failed");
    before = scope_read_record(g_state);
    context.mode = mode;
    context.enterExecution = (TZrBool)(mode == ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE);
    context.returnNormally = returnNormally;
    status = ZrCore_Exception_TryRun(g_state, scope_callback, &context);
    after = scope_read_record(g_state);
    (void)fprintf(stderr,
                  "exception_gc_scopes: exec %u->%u native %u->%u mutation %u->%u mode %d->%d\n",
                  (unsigned)before.executionDepth, (unsigned)after.executionDepth,
                  (unsigned)before.nativeDepth, (unsigned)after.nativeDepth,
                  (unsigned)before.mutationDepth, (unsigned)after.mutationDepth,
                  (int)before.nativeMode, (int)after.nativeMode);
    scope_cleanup_caller();
    scope_finish_major();
    TEST_ASSERT_TRUE(context.nativeEntered);
    TEST_ASSERT_TRUE(context.firstMutationLocked);
    TEST_ASSERT_TRUE(context.secondMutationLocked);
    if (context.enterExecution) TEST_ASSERT_TRUE(context.executionEntered);
    TEST_ASSERT_EQUAL_INT(returnNormally ? ZR_THREAD_STATUS_FINE : ZR_THREAD_STATUS_INVALID,
                          status);
    scope_assert_record_equal(&before, &after);
}

static void test_throw_restores_empty_entry(void) {
    scope_run_case(ZR_FALSE, ZR_FALSE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_throw_preserves_outer_execution(void) {
    scope_run_case(ZR_TRUE, ZR_FALSE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_throw_preserves_outer_gc_aware_native(void) {
    scope_run_case(ZR_TRUE, ZR_TRUE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_throw_preserves_outer_detached_native(void) {
    scope_run_case(ZR_FALSE, ZR_TRUE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED, ZR_FALSE);
}
static void test_throw_preserves_outer_critical_native(void) {
    scope_run_case(ZR_FALSE, ZR_TRUE, ZR_FALSE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL, ZR_FALSE);
}
static void test_throw_preserves_outer_mutation(void) {
    scope_run_case(ZR_TRUE, ZR_TRUE, ZR_TRUE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_FALSE);
}
static void test_normal_return_preserves_scopes(void) {
    scope_run_case(ZR_TRUE, ZR_TRUE, ZR_TRUE,
                   ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE, ZR_TRUE);
}

typedef struct ZrNestedScopeCallback {
    ZrScopeCallback inner;
    SZrGcDomainMutatorRecord beforeInner;
    SZrGcDomainMutatorRecord afterInner;
    EZrThreadStatus innerStatus;
    TZrBool outerEntered;
} ZrNestedScopeCallback;

static void scope_nested_callback(SZrState *state, TZrPtr arguments) {
    ZrNestedScopeCallback *context = (ZrNestedScopeCallback *)arguments;
    context->outerEntered = (TZrBool)(
            ZrCore_GcDomain_MutatorEnter(state) &&
            ZrCore_GcDomain_NativeEnter(state, ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE) &&
            ZrCore_GcDomain_MutationBegin(state));
    context->beforeInner = scope_read_record(state);
    context->inner.mode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    context->inner.enterExecution = ZR_TRUE;
    context->innerStatus = ZrCore_Exception_TryRun(state, scope_callback, &context->inner);
    context->afterInner = scope_read_record(state);
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_INVALID);
}

static void test_nested_tryrun_restores_each_entry_scope(void) {
    ZrNestedScopeCallback context = {0};
    SZrGcDomainMutatorRecord before;
    SZrGcDomainMutatorRecord after;
    EZrThreadStatus status;
    scope_start_major();
    before = scope_read_record(g_state);
    status = ZrCore_Exception_TryRun(g_state, scope_nested_callback, &context);
    after = scope_read_record(g_state);
    scope_cleanup_caller();
    scope_finish_major();
    TEST_ASSERT_TRUE(context.outerEntered);
    TEST_ASSERT_TRUE(context.inner.firstMutationLocked);
    TEST_ASSERT_TRUE(context.inner.secondMutationLocked);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_INVALID, context.innerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_INVALID, status);
    scope_assert_record_equal(&context.beforeInner, &context.afterInner);
    scope_assert_record_equal(&before, &after);
}

static void scope_worker_run(ZrScopeWorker *worker) {
    TZrBool locked;
    scope_atomic_store(&worker->started, 1);
    locked = ZrCore_GcDomain_MutationBegin(&worker->state);
    scope_atomic_store(&worker->acquired, locked ? 1 : 0);
    ZrCore_GcDomain_MutationEnd(&worker->state, locked);
    scope_atomic_store(&worker->exited, 1);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI scope_worker_entry(LPVOID argument) {
    scope_worker_run((ZrScopeWorker *)argument);
    return 0u;
}
#else
static void *scope_worker_entry(void *argument) {
    scope_worker_run((ZrScopeWorker *)argument);
    return ZR_NULL;
}
#endif

static void scope_run_lock_case(TZrBool retainOuterMutation) {
    ZrScopeCallback callback = {0};
    ZrScopeWorker worker;
    ZrScopeThread thread;
    SZrGcDomainMutatorRecord after;
    TZrBool outerBlockedWorker = ZR_TRUE;
    EZrThreadStatus status;
    memset(&worker, 0, sizeof(worker));
#if !defined(ZR_PLATFORM_WIN)
    atomic_init(&worker.started, 0);
    atomic_init(&worker.acquired, 0);
    atomic_init(&worker.exited, 0);
#endif
    scope_start_major();
    if (retainOuterMutation) {
        if (!ZrCore_GcDomain_MutatorEnter(g_state) ||
            !ZrCore_GcDomain_MutationBegin(g_state))
            scope_fail_process("outer lock entry failed");
    }
    callback.mode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    callback.enterExecution = ZR_TRUE;
    status = ZrCore_Exception_TryRun(g_state, scope_callback, &callback);
    after = scope_read_record(g_state);
    ZrCore_RawObject_Construct(&worker.state.super, ZR_RAW_OBJECT_TYPE_THREAD);
    worker.state.global = g_state->global;
    if (!ZrCore_GcDomain_MutatorAttach(g_state, &worker.state))
        scope_fail_process("worker attachment failed");
#if defined(ZR_PLATFORM_WIN)
    thread = CreateThread(ZR_NULL, 0u, scope_worker_entry, &worker, 0u, ZR_NULL);
    if (thread == ZR_NULL) scope_fail_process("worker creation failed");
#else
    if (pthread_create(&thread, ZR_NULL, scope_worker_entry, &worker) != 0)
        scope_fail_process("worker creation failed");
#endif
    if (!scope_wait_flag(&worker.started)) scope_fail_process("worker never started");
    if (retainOuterMutation) {
        scope_sleep_ms(ZR_SCOPE_LOCK_OBSERVATION_MS);
        outerBlockedWorker = (TZrBool)!scope_atomic_load(&worker.acquired);
        scope_cleanup_caller();
    }
    if (!scope_wait_flag(&worker.exited))
        scope_fail_process("caught Throw retained mutation lock; refusing unsafe cleanup");
#if defined(ZR_PLATFORM_WIN)
    if (WaitForSingleObject(thread, ZR_SCOPE_WAIT_ATTEMPTS) != WAIT_OBJECT_0)
        scope_fail_process("worker did not join");
    CloseHandle(thread);
#else
    if (pthread_join(thread, ZR_NULL) != 0) scope_fail_process("worker did not join");
#endif
    ZrCore_GcDomain_MutatorDetach(&worker.state);
    scope_finish_major();
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_INVALID, status);
    TEST_ASSERT_TRUE(callback.firstMutationLocked);
    TEST_ASSERT_TRUE(callback.secondMutationLocked);
    TEST_ASSERT_TRUE(scope_atomic_load(&worker.acquired));
    TEST_ASSERT_TRUE(outerBlockedWorker);
    TEST_ASSERT_EQUAL_UINT32(retainOuterMutation ? 1u : 0u, after.mutationDepth);
    TEST_ASSERT_EQUAL_UINT32(retainOuterMutation ? 1u : 0u, after.executionDepth);
}

static void test_throw_keeps_outer_lock_until_caller_releases_it(void) {
    scope_run_lock_case(ZR_TRUE);
}
static void test_throw_releases_lock_for_another_mutator(void) {
    scope_run_lock_case(ZR_FALSE);
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        SZrGcDomainMutatorRecord record = scope_read_record(g_state);
        if (record.mutationDepth != 0u || record.executionDepth != 0u ||
            record.nativeDepth != 0u ||
            g_state->global->garbageCollector->concurrentMajorActive)
            scope_fail_process("scope or major remains active at teardown");
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

int main(int argc, char **argv) {
    TZrBool depthsOnly = (TZrBool)(argc == 2 && strcmp(argv[1], "--scope-depths-only") == 0);
    UNITY_BEGIN();
    RUN_TEST(test_throw_restores_empty_entry);
    RUN_TEST(test_throw_preserves_outer_execution);
    RUN_TEST(test_throw_preserves_outer_gc_aware_native);
    RUN_TEST(test_throw_preserves_outer_detached_native);
    RUN_TEST(test_throw_preserves_outer_critical_native);
    RUN_TEST(test_throw_preserves_outer_mutation);
    RUN_TEST(test_normal_return_preserves_scopes);
    RUN_TEST(test_nested_tryrun_restores_each_entry_scope);
    if (!depthsOnly) {
        RUN_TEST(test_throw_keeps_outer_lock_until_caller_releases_it);
        RUN_TEST(test_throw_releases_lock_for_another_mutator);
    }
    return UNITY_END();
}

/**
 * @file tests/core/test_exception_gc_scopes.c
 *
 * ## Summary
 * 通过 Unity 在真实并发 major 期间验证 Exception_TryRun 对 GC-domain scope 快照的异常恢复、嵌套恢复、正常返回边界和递归 mutation lock 释放。
 *
 * ## Constraints and lifecycle
 * 该夹具读取 core 私有 GC-domain 记录，只在静态 core 测试目标中编译。每个 Unity 用例由 setUp 创建独立 runtime state、tearDown 检查 scope/major 已结束后销毁；并发 worker 使用栈上独立 state，必须 join 后才能 detach 或离开其栈帧。关键先决条件或超时会 fail-stop 退出进程，跳过普通 fixture 清理。
 *
 * ## References
 * 测试目标由 tests/cmake/exception-gc-scopes-tests.cmake:4-18 建立并注册；tests/CMakeLists.txt:739 include 该片段。Exception_TryRun 的快照入口和恢复实现见 zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:26,45。
 */

/**
 * @def _POSIX_C_SOURCE
 *
 * ## Summary
 * 在 POSIX 系统头文件包含前请求 POSIX.1-2008 声明，使本文件可使用 nanosleep 与 pthread 接口。
 *
 * ## Constraints and lifecycle
 * 仅在非 Windows 且构建环境尚未定义该宏时提供默认值 200809L；宏必须位于系统头 include 之前。Windows 分支不依赖它。
 *
 * ## References
 * 本文件的 POSIX include 与实现分支见 :19-25、:72-80、:295-298、:332-349。
 */

/**
 * @typedef ZrScopeAtomic
 *
 * ## Summary
 * 为 worker 与测试线程间的 started/acquired/exited 标志提供平台原子类型。
 *
 * ## Value model
 * Windows 使用 volatile LONG 并通过 Interlocked API 访问；POSIX 使用 _Atomic int。
 *
 * ## Constraints and lifecycle
 * POSIX 对象在 worker 启动前以 atomic_init 初始化；Windows 对象先清零后由 Interlocked 操作访问。不要直接进行普通并发读写。
 *
 * ## References
 * 类型定义在本文件 :17 或 :23；读写封装为 scope_atomic_load 与 scope_atomic_store。
 */

/**
 * @typedef ZrScopeThread
 *
 * ## Summary
 * 抽象保存本测试 worker 的系统线程句柄/标识。
 *
 * ## Value model
 * Windows 保存 CreateThread 返回的 HANDLE；POSIX 保存 pthread_t。
 *
 * ## Constraints and lifecycle
 * 创建成功后必须等待线程退出；Windows 还须 CloseHandle，POSIX 须 pthread_join。worker 使用的 ZrScopeWorker 栈存储在完成 join 前保持有效。
 *
 * ## References
 * 类型定义见本文件 :18 或 :24；创建、等待、关闭分别见 :329-349。
 */

/**
 * @def ZR_SCOPE_WAIT_ATTEMPTS
 *
 * ## Summary
 * 限制 started/exited 标志的轮询次数；Windows join 也将同一数值作为毫秒超时，避免 worker 失败时测试无限等待。
 *
 * ## Value model
 * 每次轮询最多调用一次 1 ms 睡眠，随后再读一次标志。
 *
 * ## Constraints and lifecycle
 * 在轮询中它是次数上限，Windows WaitForSingleObject 则以毫秒解释该值；两者都不是精确墙钟期限，线程调度和睡眠精度会影响实际时间。
 *
 * ## References
 * 定义在本文件 :27；scope_wait_flag 与 Windows WaitForSingleObject 使用该上限。
 */

/**
 * @def ZR_SCOPE_LOCK_OBSERVATION_MS
 *
 * ## Summary
 * 指定保留外层 mutation lock 后、读取 worker acquired 标志前等待的观察窗口。
 *
 * ## Value model
 * 固定为 100 ms。
 *
 * ## Constraints and lifecycle
 * 此延迟不是 worker 已经尝试获取锁的握手；started 在 MutationBegin 前发布，线程可能仍未运行到锁调用。
 *
 * ## References
 * 定义在本文件 :28；scope_run_lock_case 在 :336-339 使用。
 */

/**
 * @def ZR_SCOPE_MAJOR_DRAIN_LIMIT
 *
 * ## Summary
 * 限制每个用例结束时用于排空 concurrent major 的 GcStep 次数。
 *
 * ## Value model
 * 固定最多 4096 步。
 *
 * ## Constraints and lifecycle
 * 若到上限后 concurrentMajorActive 仍为真，夹具 fail-stop 退出，不销毁仍在工作的 runtime state。
 *
 * ## References
 * 定义在本文件 :29；scope_finish_major 在 :131-140 使用。
 */

/**
 * @struct ZrScopeCallback
 *
 * ## Summary
 * 保存 scope_callback 的输入模式、正常/异常分支选择及进入各层 scope 的结果。
 *
 * ## Fields
 * mode 与 enterExecution 选择要进入的 native/execution scope；returnNormally 为假时 callback 抛 INVALID。executionEntered、nativeEntered、firstMutationLocked、secondMutationLocked 记录对应 Begin/Enter 是否成功，供测试断言。
 *
 * ## Constraints and lifecycle
 * 作为同步 Exception_TryRun 的借用参数，须从调用前初始化并保持到 TryRun 返回。各测试以零初始化，未显式设置 returnNormally 时选择抛异常路径；Throw 会非局部离开 callback，之后的正常 Leave/End 不会执行。
 *
 * ## References
 * 定义见本文件 :31-39；消费者 scope_callback :142-157、scope_run_case :172-207、scope_nested_callback :246-258、scope_run_lock_case :301-359。
 */

/**
 * @struct ZrScopeWorker
 *
 * ## Summary
 * 承载跨线程 mutation-lock 用例的临时 state 与原子进度标记。
 *
 * ## Fields
 * state 是附着到主 fixture 同一 GC domain 的 worker state；started 表示线程已进入 worker 函数，acquired 表示 MutationBegin 成功，exited 表示 worker 已执行完其锁清理。
 *
 * ## Constraints and lifecycle
 * 对象由 scope_run_lock_case 栈上持有并清零；POSIX 原子成员显式初始化。附着成功后才启动线程，线程退出并 join/等待成功后才 detach；在此之前不可返回栈帧。
 *
 * ## References
 * 定义见本文件 :41-46；worker 读写见 scope_worker_run :280-287，线程入口和生命周期见 :289-350。
 */

/**
 * @struct ZrNestedScopeCallback
 *
 * ## Summary
 * 保存嵌套 TryRun 用例的内层回调上下文、内层前后记录、返回状态和外层 Enter 结果。
 *
 * ## Fields
 * inner 是复用的 ZrScopeCallback；beforeInner/afterInner 是按值复制的 mutator 记录；innerStatus 保存内层 TryRun 结果；outerEntered 汇总外层 execution/native/mutation 的成功状态。
 *
 * ## Constraints and lifecycle
 * 作为同步外层 TryRun 的借用参数，必须活到回调和异常恢复结束。用例零初始化它，因此 inner.returnNormally 默认 false，内层 callback 会抛 INVALID。
 *
 * ## References
 * 定义见本文件 :238-244；填充者 scope_nested_callback :246-258，断言者 test_nested_tryrun_restores_each_entry_scope :260-278。
 */

/**
 * @var g_state
 *
 * ## Summary
 * 指向当前 Unity 用例拥有的 runtime state，供同一测试线程的 scope helper 使用。
 *
 * ## Constraints and lifecycle
 * 只由 setUp 创建并由 tearDown 销毁；测试期间保持有效。worker 不读写此指针，而使用其自身的 ZrScopeWorker.state 与共享 global。
 *
 * ## References
 * 生命周期见 setUp/tearDown :368-382；直接使用者包括 scope_start_major、scope_cleanup_caller、scope_finish_major 和各 case helper，scope_read_record 则借用调用方传入的 state。
 */
/**
 * @fn static void scope_fail_process(const char *reason)
 *
 * ## Summary
 * 报告夹具无法安全继续的前置条件、线程或 GC 排空失败，并以失败状态立即结束测试进程。
 *
 * ## Parameters
 * @param[in] reason 非空、以 NUL 结尾的静态或当前有效诊断字符串；本函数只在调用期间读取。
 *
 * ## Returns
 * 不返回；输出到 stderr 并 fflush 后调用 _Exit(EXIT_FAILURE)，不会运行 Unity tearDown 或常规资源清理。
 *
 * ## Constraints and lifecycle
 * 只用于继续执行会使锁、线程或 GC state 生命周期不安全的失败路径；调用方不能依赖栈展开、atexit 或 fixture cleanup。
 *
 * ## References
 * 失败分支见 scope_start_major :112-114、scope_finish_major :138-140、scope_run_case :180-185、scope_run_lock_case :318-345、tearDown :375-379。
 */

/**
 * @fn static int scope_atomic_load(ZrScopeAtomic *value)
 *
 * ## Summary
 * 以平台对应的 acquire 操作读取 worker 进度标志。
 *
 * ## Parameters
 * @param[in] value 指向已初始化且在并发访问期间仍存活的原子标志。
 *
 * ## Returns
 * @return 返回当次观察到的整数值；调用者按零/非零解释标志。
 *
 * ## Constraints and lifecycle
 * Windows 通过 InterlockedCompareExchange(value, 0, 0) 读取；POSIX 使用 memory_order_acquire。不可用普通非原子读取替代。
 *
 * ## References
 * 被 scope_wait_flag :84-88、scope_run_lock_case :338 和 :355 调用；对应写操作是 scope_atomic_store。
 */

/**
 * @fn static void scope_atomic_store(ZrScopeAtomic *value, int next)
 *
 * ## Summary
 * 以平台对应的 release 操作发布 worker 进度标志。
 *
 * ## Parameters
 * @param[in,out] value 指向已初始化、并在消费者读取前保持存活的原子标志。
 * @param[in] next 写入的零/非零状态。
 *
 * ## Returns
 * 无返回值；新状态通过原子对象发布。
 *
 * ## Constraints and lifecycle
 * Windows 使用 InterlockedExchange；POSIX 使用 memory_order_release，与消费者 acquire 读取配对。对象不能在线程结束前销毁。
 *
 * ## References
 * scope_worker_run 在 :282、:284、:286 发布 started/acquired/exited；读取封装为 scope_atomic_load。
 */

/**
 * @fn static void scope_sleep_ms(TZrUInt32 milliseconds)
 *
 * ## Summary
 * 为有界轮询或锁状态观察让出当前线程，不承担线程间同步。
 *
 * ## Parameters
 * @param[in] milliseconds 请求等待的毫秒数。
 *
 * ## Returns
 * 无返回值；不报告 Windows Sleep 或 POSIX nanosleep 的中断/精度状态。
 *
 * ## Constraints and lifecycle
 * POSIX 将毫秒拆成 timespec 并忽略 nanosleep 返回；因此等待时长不是精确时钟或成功握手。共享状态仍须由原子操作发布。
 *
 * ## References
 * 被 scope_wait_flag :86 和 scope_run_lock_case :337 调用；平台实现分支在 :72-80。
 */

/**
 * @fn static TZrBool scope_wait_flag(ZrScopeAtomic *flag)
 *
 * ## Summary
 * 对 started/exited 等 worker 标志作最多 ZR_SCOPE_WAIT_ATTEMPTS 次轮询，并给每次未完成轮询安排短睡眠。
 *
 * ## Parameters
 * @param[in] flag 指向已初始化且在整个等待期间存活的原子标志。
 *
 * ## Returns
 * @return 若轮询或最后一次读取发现非零则返回 ZR_TRUE，否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 上限次数不保证精确墙钟超时；本 helper 不终止线程或清理资源。调用者须在 FALSE 时自行采用安全失败策略。
 *
 * ## References
 * scope_run_lock_case 在 :335 等待 started、在 :341 等待 exited；实现使用 scope_atomic_load :85、:88。
 */

/**
 * @fn static SZrGcDomainMutatorRecord scope_read_record(SZrState *state)
 *
 * ## Summary
 * 在 GC-domain coordination lock 下按 state 地址复制当前 mutator record，供测试保存前后状态。
 *
 * ## Parameters
 * @param[in] state 必须是仍附着于有效 domain 的 state；本 helper 不处理空 state 或空 domain。
 *
 * ## Returns
 * @return 返回按值复制的 record；未在 mutator 表中找到 state 时返回零初始化记录。
 *
 * ## Constraints and lifecycle
 * 复制在 domain 锁内完成，因此返回值不借用表内存，可在解锁后比较。该值仅是观测快照，不提供对 state/domain 生命周期的保留。
 *
 * ## References
 * 实现使用 ZrCore_GcDomain_Lock/Unlock :94、:101；调用点包括 scope_run_case :186、:191，嵌套用例 :252、:256、:266、:268，以及清理/teardown。
 */

/**
 * @fn static void scope_start_major(void)
 *
 * ## Summary
 * 为单个夹具用例启动可观察的 generational concurrent major，使 scope/锁恢复发生在 GC 工作活跃时。
 *
 * ## Returns
 * 无返回值；成功时留下活跃 major 供当前用例执行。
 *
 * ## Constraints and lifecycle
 * 作用于 g_state 所属 global，设置一个 GC worker、generational 模式并安排 major；一次 GcStep 后若 concurrentMajorActive 未置位即 fail-stop。只能在 state 有效且没有上一用例遗留 major 时调用。
 *
 * ## References
 * 用于 scope_run_case :179、嵌套用例 :265 和 scope_run_lock_case :314；排空由 scope_finish_major 配对完成。
 */

/**
 * @fn static void scope_cleanup_caller(void)
 *
 * ## Summary
 * 在记录完异常恢复结果后，由持有这些 scopes 的测试线程按 mutation、native、execution 顺序清理该 state 剩余深度。
 *
 * ## Returns
 * 无返回值；把当前 caller state 的三个深度逐层减至零。
 *
 * ## Constraints and lifecycle
 * 必须在 state 仍附着且由当前线程持有相关递归 mutation lock 时调用。mutationDepth 每层以 locked=true 交给 MutationEnd；不得用来修复线程外的锁所有权，也不能放在快照观察之前。
 *
 * ## References
 * 被 scope_run_case :198、嵌套用例 :269 和保留外层锁分支 :339 调用；读取深度依赖 scope_read_record。
 */

/**
 * @fn static void scope_finish_major(void)
 *
 * ## Summary
 * 在 scope 清理后主动推进 concurrent major，确保测试 state 销毁前 collector 已退出该周期。
 *
 * ## Returns
 * 无返回值；major 完成后返回，超过步数仍活跃时 fail-stop。
 *
 * ## Constraints and lifecycle
 * 最多执行 ZR_SCOPE_MAJOR_DRAIN_LIMIT 次 GcStep。失败时不调用 state destroy，以免释放仍被 collector 使用的资源；这不是 collector 通用等待 API。
 *
 * ## References
 * 与 scope_start_major 配对，用于 scope_run_case :199、嵌套用例 :270 和 scope_run_lock_case :351。
 */

/**
 * @fn static void scope_callback(SZrState *state, TZrPtr arguments)
 *
 * ## Summary
 * 作为 TryRun 的同步回调，按 context 请求进入 execution/native scope 和两层递归 mutation scope，并选择正常配对退出或主动抛 INVALID。
 *
 * ## Parameters
 * @param[in,out] state TryRun 当前 state；其 GC domain 必须有效。
 * @param[in,out] arguments 指向调用者持有的 ZrScopeCallback，借用到正常返回或 Throw 非局部离开。
 *
 * ## Returns
 * 无返回值；returnNormally 为真时本函数配对 MutationEnd/NativeLeave/MutatorLeave；为假时在清理语句前 Throw，清理由 TryRun 异常恢复路径负责。
 *
 * ## Constraints and lifecycle
 * context 中的进入结果标志由调用者零初始化并在返回后检查。此回调故意用 INVALID，避免构造 Error 干扰正在测试的 scope unwind。它只记录 Begin/Enter 结果，测试负责断言。
 *
 * ## References
 * 通过 ZrCore_Exception_TryRun 在 scope_run_case :190、scope_nested_callback :255 和 scope_run_lock_case :322 传入；状态检查见 scope_run_case :200-206。
 */

/**
 * @fn static void scope_assert_record_equal(const SZrGcDomainMutatorRecord *expected, const SZrGcDomainMutatorRecord *actual)
 *
 * ## Summary
 * 用 Unity 断言比较测试关注的 mutator 身份和 scope 状态字段。
 *
 * ## Parameters
 * @param[in] expected 入口或内层调用前按值保存的记录。
 * @param[in] actual TryRun 返回后重新读取的记录。
 *
 * ## Returns
 * 无返回值；不匹配由 Unity 记录为当前用例失败。
 *
 * ## Constraints and lifecycle
 * 只比较 mutatorId、三个深度、nativeMode、status 和 nativeEnteredFromInactive；并非整结构体逐字节比较，也不比较 epoch/其他字段。
 *
 * ## References
 * scope_run_case :206 与嵌套用例 :276-277 调用；实际断言位于本函数 :162-169。
 */

/**
 * @fn static void scope_run_case(TZrBool outerExecution, TZrBool outerNative, TZrBool outerMutation, EZrGcNativeSafepointMode mode, TZrBool returnNormally)
 *
 * ## Summary
 * 驱动单个深度组合：启动 concurrent major，建立可选外层 scope，保存记录，执行 scope_callback，再比较 TryRun 后的记录。
 *
 * ## Parameters
 * @param[in] outerExecution 是否在 TryRun 前进入外层 execution scope。
 * @param[in] outerNative 是否在 TryRun 前进入 outer native scope。
 * @param[in] outerMutation 是否在 TryRun 前取得 outer mutation 层。
 * @param[in] mode 传给 outer native scope 和 callback 的 native safepoint mode。
 * @param[in] returnNormally 选择 callback 配对正常返回或抛 INVALID。
 *
 * ## Returns
 * 无返回值；Unity 记录每个进入状态、TryRun status 和保存记录的比较结果。
 *
 * ## Constraints and lifecycle
 * 只在 setUp 创建的 g_state 上运行。after 记录在 scope_cleanup_caller 之前读取；随后无论断言结果如何都会清理剩余深度并排空 major。正常返回用例验证 callback 自己平衡 scopes 后状态相等，不验证 TryRun 对正常返回自动回滚。
 *
 * ## References
 * 七个独立 Unity 用例在 :210-235 调用；回调/断言 helper 为 scope_callback、scope_read_record、scope_assert_record_equal。
 */

/**
 * @fn static void test_throw_restores_empty_entry(void)
 *
 * ## Summary
 * 验证从 attached-inactive 的空 scope 入口运行 GC-aware callback 后，Throw(INVALID) 恢复入口记录。
 *
 * ## Returns
 * 无返回值；失败由 scope_run_case 中的 Unity 断言报告。
 *
 * ## Constraints and lifecycle
 * callback 进入 execution、native 和两层 mutation scope 后立即抛；用例覆盖空外层深度，不覆盖其他错误类型。
 *
 * ## References
 * main 在 :388 注册；参数组合见本文件 :210-212，具体断言位于 scope_run_case :200-206。
 */

/**
 * @fn static void test_throw_preserves_outer_execution(void)
 *
 * ## Summary
 * 验证异常展开保留 TryRun 进入前已存在的 outer execution depth。
 *
 * ## Returns
 * 无返回值；结果由 Unity 记录。
 *
 * ## Constraints and lifecycle
 * outer native/mutation 为空，callback 再进入一层 execution 与其他 scope 后 Throw。
 *
 * ## References
 * main :389 注册；参数组合 :213-215；共用断言 scope_run_case :200-206。
 */

/**
 * @fn static void test_throw_preserves_outer_gc_aware_native(void)
 *
 * ## Summary
 * 验证 outer execution 与 GC-aware native scope 存在时，异常恢复保留入口深度和模式。
 *
 * ## Returns
 * 无返回值；结果由 Unity 记录。
 *
 * ## Constraints and lifecycle
 * callback 再嵌套进入 GC-aware execution/native 与两层 mutation，随后 Throw。
 *
 * ## References
 * main :390 注册；参数组合 :217-219；共用断言 scope_run_case :200-206。
 */

/**
 * @fn static void test_throw_preserves_outer_detached_native(void)
 *
 * ## Summary
 * 验证 inactive state 的 outer blocking-detached native scope 在异常后仍按入口记录保留。
 *
 * ## Returns
 * 无返回值；结果由 Unity 记录。
 *
 * ## Constraints and lifecycle
 * 外层不进入 execution；用例比较 native depth、mode、status 与 nativeEnteredFromInactive 的入口/返回值，不验证之后的调度效果。
 *
 * ## References
 * main :391 注册；参数组合 :221-223；字段断言 scope_assert_record_equal :162-169。
 */

/**
 * @fn static void test_throw_preserves_outer_critical_native(void)
 *
 * ## Summary
 * 验证 inactive state 的 outer no-safepoint-critical native scope 在异常后恢复为入口记录。
 *
 * ## Returns
 * 无返回值；结果由 Unity 记录。
 *
 * ## Constraints and lifecycle
 * 外层不进入 execution；测试断言 mode/status/depth 和 nativeEnteredFromInactive 字段相等，不触发或断言额外的 pause 行为。
 *
 * ## References
 * main :392 注册；参数组合 :225-227；字段断言 scope_assert_record_equal :162-169。
 */

/**
 * @fn static void test_throw_preserves_outer_mutation(void)
 *
 * ## Summary
 * 验证已有 outer execution、GC-aware native 与 mutation 层同时存在时，Throw 仅撤销 callback 多取得的递归层。
 *
 * ## Returns
 * 无返回值；结果由 Unity 记录。
 *
 * ## Constraints and lifecycle
 * scope_run_case 在读取 after 后清理 caller 持有的 outer 层；记录比较先于该清理。
 *
 * ## References
 * main :393 注册；参数组合 :229-231；深度和锁状态比较见 scope_run_case :191-206。
 */

/**
 * @fn static void test_normal_return_preserves_scopes(void)
 *
 * ## Summary
 * 验证 callback 正常配对结束其嵌套 execution/native/mutation scopes 后，TryRun 返回 FINE 且记录回到入口状态。
 *
 * ## Returns
 * 无返回值；结果由 Unity 记录。
 *
 * ## Constraints and lifecycle
 * 此用例只证明 callback 的显式 Leave/End 与正常返回相配；TryRun 正常分支不调用异常快照恢复，不能据此声称任意漏配 scope 会被自动修复。
 *
 * ## References
 * main :394 注册；参数组合 :233-235；status 和记录断言见 scope_run_case :200-206。
 */
/**
 * @fn static void scope_nested_callback(SZrState *state, TZrPtr arguments)
 *
 * ## Summary
 * 构造一个有外层 execution/native/mutation scopes 的 TryRun 回调，在其中运行必抛异常的内层 TryRun，再抛出外层异常。
 *
 * ## Parameters
 * @param[in,out] state 当前已附着 domain 的测试 state。
 * @param[in,out] arguments 指向外层 TryRun 借用的 ZrNestedScopeCallback，存活到外层调用返回。
 *
 * ## Returns
 * 无返回值；内层 status/记录和 outerEntered 写回 context，随后外层 Throw(INVALID) 非局部离开。
 *
 * ## Constraints and lifecycle
 * outerEntered 汇总三个外层 Enter/Begin 的短路结果。inner 是零初始化上下文，只设置 GC-aware mode 和 enterExecution，因此 returnNormally 保持 false；内层 scope_callback 会抛 INVALID。外层和内层快照均由各自同步 TryRun 栈帧管理。
 *
 * ## References
 * 仅由 test_nested_tryrun_restores_each_entry_scope 在 :267 作为 TryRun 回调传入；保存内层记录见 :252、:256。
 */

/**
 * @fn static void test_nested_tryrun_restores_each_entry_scope(void)
 *
 * ## Summary
 * 验证内层异常返回时恢复其捕获的外层 scope，随后外层异常又恢复测试入口 scope。
 *
 * ## Returns
 * 无返回值；Unity 记录状态与快照断言。
 *
 * ## Constraints and lifecycle
 * 用例启动 concurrent major，分别比较内层 TryRun 前后及最外层 TryRun 前后记录。它断言 outerEntered、内层两次 mutation lock 成功及两层 INVALID status；当前没有显式断言 inner.executionEntered 或 inner.nativeEntered，TODO:若该用例需证明内层两种 Enter 成功，应补对应断言。
 *
 * ## References
 * main :395 注册；回调是 scope_nested_callback :246-258；所比较字段由 scope_assert_record_equal :159-170 负责。
 */

/**
 * @fn static void scope_worker_run(ZrScopeWorker *worker)
 *
 * ## Summary
 * 在线程入口中发布启动标志，尝试取得 worker state 的 mutation lock，再发布获取和退出状态。
 *
 * ## Parameters
 * @param[in,out] worker 指向调用线程栈上持有且已附着 GC domain 的 ZrScopeWorker；整个 worker 生命周期内必须有效。
 *
 * ## Returns
 * 无返回值；调用者经原子 acquired/exited 标志观测结果。
 *
 * ## Constraints and lifecycle
 * 即使 MutationBegin 返回 false 也会发布 acquired=false、exited=true；worker 不负责 detach state 或释放主线程的 outer lock。调用者须 join 后 detach。
 *
 * ## References
 * 只由平台 wrapper scope_worker_entry 调用；用例创建与 join 位于 scope_run_lock_case :324-350。
 */

/**
 * @fn static DWORD WINAPI scope_worker_entry(LPVOID argument)
 *
 * ## Summary
 * 在 Windows 分支以 WINAPI 线程入口 ABI 转调 scope_worker_run；POSIX 的互斥实现使用下方另列的 void* 签名。
 *
 * ## Parameters
 * @param[in,out] argument 指向仍存活的 ZrScopeWorker；Windows ABI 以 LPVOID 传入，借用至本线程退出。
 *
 * ## Returns
 * @return Windows 入口返回 DWORD 0；它不转交 worker 的 acquired 状态，该状态由原子字段报告。
 *
 * ## Constraints and lifecycle
 * worker 返回前只能使用 scope_run_lock_case 的栈对象。创建成功后由相应平台 wait/join 与 handle cleanup 配对，不可提前离开父栈帧。
 *
 * ## References
 * 条件定义位于 :289-299；通过 CreateThread :329 或 pthread_create :332 注册为入口。
 */

/**
 * @fn static void scope_run_lock_case(TZrBool retainOuterMutation)
 *
 * ## Summary
 * 在 concurrent major 中执行抛异常 callback，启动第二个 mutator state 的 worker，观察并验证 mutation-lock 的保留或释放。
 *
 * ## Parameters
 * @param[in] retainOuterMutation 为真时先建立 caller-owned execution/mutation scope，要求该锁在 caller 清理前仍阻塞 worker；为假时要求异常后 worker 能获取 callback 的锁。
 *
 * ## Returns
 * 无返回值；断言失败由 Unity 记录，线程创建/等待或 major 清理失败则 fail-stop。
 *
 * ## Constraints and lifecycle
 * worker 是栈上 state，attach 在创建线程前，只有 worker 完成并 join/关闭句柄后才 detach。保留外层分支在发布 started 后等待 100 ms，再读取 acquired；没有单独确认 worker 已进入阻塞等待的握手。TODO:用明确的 attempt/blocked 同步状态替代固定睡眠，避免调度延迟把“尚未尝试”误作“被锁阻塞”。不保留 outer 的分支中 outerBlockedWorker 保持初始化为 TRUE，末尾对此变量的断言是恒真；该分支真正的锁证据是 acquired/exited 最终置位及入口记录深度为零。
 *
 * ## References
 * 两个调用者是 test_throw_keeps_outer_lock_until_caller_releases_it :362 和 test_throw_releases_lock_for_another_mutator :365；worker 的锁操作在 scope_worker_run :280-287。
 */

/**
 * @fn static void test_throw_keeps_outer_lock_until_caller_releases_it(void)
 *
 * ## Summary
 * 验证 TryRun 异常恢复保留调用者原有 mutation lock，且其他 mutator 在 caller 释放后才完成锁操作。
 *
 * ## Returns
 * 无返回值；Unity 记录 status、锁成功和深度断言。
 *
 * ## Constraints and lifecycle
 * 依赖 scope_run_lock_case 的 100 ms 观察窗口；该观察不是 worker 已开始等待锁的握手。after 快照的 executionDepth/mutationDepth 应为 1，随后 caller 才清理。
 *
 * ## References
 * main :397 注册，转交 scope_run_lock_case(ZR_TRUE) :361-363；具体 worker、join 与断言在 :329-359。
 */

/**
 * @fn static void test_throw_releases_lock_for_another_mutator(void)
 *
 * ## Summary
 * 验证没有 caller-owned outer mutation lock 时，Throw 后另一 mutator 最终可以取得并释放锁。
 *
 * ## Returns
 * 无返回值；Unity 记录 worker acquisition、status 和深度断言。
 *
 * ## Constraints and lifecycle
 * 此路径不修改 outerBlockedWorker 的 TRUE 初始化值，故对应断言不提供覆盖；主要锁结论来自 worker 必须在有限等待内 acquired/exited，且异常后快照 executionDepth/mutationDepth 均为零。
 *
 * ## References
 * main :398 注册，转交 scope_run_lock_case(ZR_FALSE) :364-366；worker与清理入口位于 :324-359。
 */

/**
 * @fn void setUp(void)
 *
 * ## Summary
 * 在每个 Unity 用例开始前创建供本夹具使用的 runtime state。
 *
 * ## Returns
 * 无返回值；成功时将拥有的 state 存入 g_state。
 *
 * ## Constraints and lifecycle
 * state 由测试 harness 创建并仅在本用例期间有效；空结果由 Unity 断言报告。创建后才可运行依赖 GC domain/collector 的 helper。
 *
 * ## References
 * main :388-398 通过 RUN_TEST 注册用例；UnityDefaultTestRun 在 tests/third_party/zr_unity/Unity/src/unity.c:2298 实际调用 setUp，销毁检查见 tearDown :373-382。
 */

/**
 * @fn void tearDown(void)
 *
 * ## Summary
 * 在每个 Unity 用例结束时确认无遗留 GC scopes 或 concurrent major，再销毁 fixture state。
 *
 * ## Returns
 * 无返回值；成功时销毁 state 并清空 g_state。
 *
 * ## Constraints and lifecycle
 * 若 mutation/execution/native depth 非零或 major 仍活跃，则调用 scope_fail_process 而不是冒险销毁 state。该检查前提是用例线程/worker 已按各自流程清理；worker detach 在 join 后完成。
 *
 * ## References
 * UnityDefaultTestRun 在 tests/third_party/zr_unity/Unity/src/unity.c:2303 实际调用；main :388-398 是 RUN_TEST 注册入口，state 创建/销毁见 :369-380。
 */

/**
 * @fn int main(int argc, char **argv)
 *
 * ## Summary
 * 启动 Unity runner 并注册 10 个 GC scope 恢复用例。
 *
 * ## Parameters
 * @param[in] argc 进程参数个数。
 * @param[in] argv 进程参数数组；只识别一个精确参数 --scope-depths-only。
 *
 * ## Returns
 * @return Unity_END 提供的测试进程结果，供 CTest 判断通过或失败。
 *
 * ## Constraints and lifecycle
 * 精确收到 --scope-depths-only 时跳过最后两个跨线程锁用例；CTest 注册命令没有传参数，因此默认执行全部 10 例。每个例子的 state 生命周期由 Unity setUp/tearDown 包围。
 *
 * ## References
 * CMake 入口是 tests/cmake/exception-gc-scopes-tests.cmake:16；用例注册见本函数 :388-398，测试 fixture helper 定义于 tests/CMakeLists.txt:65-75。
 */

/**
 * @fn static void *scope_worker_entry(void *argument)
 *
 * ## Summary
 * 在非 Windows 分支以 pthread 入口 ABI 转调 scope_worker_run；与 Windows 入口互斥，属于同一逻辑测试单元。
 *
 * ## Parameters
 * @param[in,out] argument 指向父测试栈上、已附着 domain 的 ZrScopeWorker；借用至本线程退出。
 *
 * ## Returns
 * @return worker 结束后返回 ZR_NULL；取得 mutation lock 的结果通过 acquired/exited 原子字段交付。
 *
 * ## Constraints and lifecycle
 * 父线程必须在 pthread_create 成功后等待本线程退出并 pthread_join，之后才能 detach worker state 或离开其栈帧；本 wrapper 不负责这些清理，也不读取 g_state。
 *
 * ## References
 * 本文件 :295-298 定义 POSIX 实现，:332 注册线程入口，:348 join，:350 detach；Windows 互斥实现见 :290-293。
 */
