#if !defined(ZR_PLATFORM_WIN) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/ownership_transfer.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
typedef HANDLE ZrTransferTestThread;
typedef volatile LONG ZrTransferTestAtomic;
#else
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
typedef pthread_t ZrTransferTestThread;
typedef _Atomic int ZrTransferTestAtomic;
#endif

typedef struct ZrOwnershipTransferRaceContext {
    /* 两个 worker 借用同一 envelope/start；主用例必须先发布 start，
     * 再 join 两线程，最后才能释放 envelope 和栈上上下文。 */
    SZrOwnershipTransferEnvelope *envelope;
    SZrState *state;
    SZrTypeValue *target;
    ZrTransferTestAtomic *start;
    TZrUInt64 workerId;
    TZrUInt64 claimEpoch;
    TZrBool commit;
    TZrBool result;
} ZrOwnershipTransferRaceContext;

static SZrState *g_state;
static TZrUInt32 g_drop_count;

static int transfer_test_atomic_load(ZrTransferTestAtomic *value) {
#if defined(ZR_PLATFORM_WIN)
    return (int)InterlockedCompareExchange(value, 0, 0);
#else
    return atomic_load_explicit(value, memory_order_acquire);
#endif
}

static void transfer_test_atomic_store(
        ZrTransferTestAtomic *value,
        int next) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange(value, (LONG)next);
#else
    atomic_store_explicit(value, next, memory_order_release);
#endif
}

static void transfer_race_run(ZrOwnershipTransferRaceContext *context) {
    while (!transfer_test_atomic_load(context->start)) {
#if defined(ZR_PLATFORM_WIN)
        (void)SwitchToThread();
#else
        (void)sched_yield();
#endif
    }
    context->result = context->commit
                              ? ZrCore_OwnershipTransfer_Commit(
                                        context->envelope,
                                        context->state,
                                        context->workerId,
                                        context->claimEpoch,
                                        context->target)
                              : ZrCore_OwnershipTransfer_Abort(
                                        context->envelope,
                                        context->state,
                                        context->workerId,
                                        context->claimEpoch);
}

#if defined(ZR_PLATFORM_WIN)
static DWORD WINAPI transfer_race_entry(LPVOID argument) {
    transfer_race_run((ZrOwnershipTransferRaceContext *)argument);
    return 0;
}

static ZrTransferTestThread transfer_thread_start(
        ZrOwnershipTransferRaceContext *context) {
    return CreateThread(ZR_NULL, 0u, transfer_race_entry, context, 0u, ZR_NULL);
}

static void transfer_thread_join(ZrTransferTestThread thread) {
    TEST_ASSERT_NOT_NULL(thread);
    TEST_ASSERT_EQUAL_UINT32(WAIT_OBJECT_0, WaitForSingleObject(thread, 5000u));
    CloseHandle(thread);
}
#else
static void *transfer_race_entry(void *argument) {
    transfer_race_run((ZrOwnershipTransferRaceContext *)argument);
    return ZR_NULL;
}

static ZrTransferTestThread transfer_thread_start(
        ZrOwnershipTransferRaceContext *context) {
    ZrTransferTestThread thread;
    /* BUG: 第二次 pthread_create 若失败，Unity 断言直接跳出用例；
     * 第一线程仍等待 start，随后栈上 context 失效且无法 join。 */
    TEST_ASSERT_EQUAL_INT(
            0, pthread_create(&thread, ZR_NULL, transfer_race_entry, context));
    return thread;
}

static void transfer_thread_join(ZrTransferTestThread thread) {
    TEST_ASSERT_EQUAL_INT(0, pthread_join(thread, ZR_NULL));
}
#endif

static TZrInt64 count_drop(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    g_drop_count++;
    return 0;
}

static void init_resource_unique(SZrTypeValue *outOwner) {
    /* 为转移状态机构造带计数析构的 unique resource；Release/Abort/
     * Commit 谁取得最终所有权，可由 g_drop_count 验证恰好一次释放。 */
    SZrString *name = ZrCore_String_CreateFromNative(g_state, "HandoffResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0u);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(destructor);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    destructor->nativeFunction = count_drop;
    ZrCore_RawObject_MarkAsPermanent(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    ZrCore_ObjectPrototype_AddMeta(
            g_state,
            prototype,
            ZR_META_DESTRUCTOR,
            ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor)));
    object = ZrCore_Object_New(g_state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(g_state, object);
    ZrCore_Value_ResetAsNull(outOwner);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_state, outOwner, ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_drop_count = 0u;
}

void tearDown(void) {
    /* BUG: 断言若在 PrepareSameDomain 后、显式 Free 前失败，
     * teardown 只销毁 VM，不持有 envelope 指针以释放原生分配。 */
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void test_same_domain_prepare_publish_claim_commit_moves_one_owner(void) {
    /* Prepare 消耗 source 的唯一所有权，Commit 将同一对象/control
     * 交给 target；Free envelope 不得再调用资源析构。 */
    SZrTypeValue source;
    SZrTypeValue target;
    SZrOwnershipTransferEnvelope *envelope;
    SZrOwnershipTransferSnapshot snapshot;
    SZrGcDomainIdentity identity = ZrCore_GcDomain_GetIdentity(g_state);
    SZrRawObject *sourceObject;
    SZrOwnershipControl *sourceControl;

    init_resource_unique(&source);
    sourceObject = ZrCore_Value_GetRawObject(&source);
    sourceControl = source.ownershipControl;
    TEST_ASSERT_NOT_NULL(sourceObject);
    ZrCore_Value_ResetAsNull(&target);
    envelope = ZrCore_OwnershipTransfer_PrepareSameDomain(
            g_state, identity, &source);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source.type));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_state, 41u, 7u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Commit(
            envelope, g_state, 41u, 7u, &target));
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_UNIQUE, target.ownershipKind);
    TEST_ASSERT_EQUAL_PTR(sourceObject, ZrCore_Value_GetRawObject(&target));
    TEST_ASSERT_EQUAL_PTR(sourceControl, target.ownershipControl);
    TEST_ASSERT_EQUAL_UINT32(0u, g_drop_count);

    ZrCore_OwnershipTransfer_GetSnapshot(envelope, &snapshot);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED, snapshot.state);
    TEST_ASSERT_FALSE(snapshot.hasPayload);
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 41u, 7u));

    ZrCore_Ownership_ReleaseValue(g_state, &target);
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
    ZrCore_OwnershipTransfer_Free(g_state, envelope);
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
}

static void test_queued_and_claimed_envelope_keeps_exact_ownership_root(void) {
    /* queued/claimed 阶段没有用户值持有资源，envelope 的 ownership root
     * 必须跨越 full GC 保持对象，Abort 后才移除。 */
    SZrTypeValue source;
    SZrOwnershipTransferEnvelope *envelope;
    SZrRawObject *resourceObject;

    init_resource_unique(&source);
    resourceObject = ZrCore_Value_GetRawObject(&source);
    TEST_ASSERT_NOT_NULL(resourceObject);
    TEST_ASSERT_EQUAL_UINT64(
            1u, ZrCore_GcDomain_GetOwnershipRootCount(g_state));
    envelope = ZrCore_OwnershipTransfer_PrepareSameDomain(
            g_state, ZrCore_GcDomain_GetIdentity(g_state), &source);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source.type));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_IsOwnershipRoot(
            g_state, resourceObject));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT32(0u, g_drop_count);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_IsOwnershipRoot(
            g_state, resourceObject));

    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_state, 12u, 4u));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT32(0u, g_drop_count);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_IsOwnershipRoot(
            g_state, resourceObject));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 12u, 4u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
    TEST_ASSERT_EQUAL_UINT64(
            0u, ZrCore_GcDomain_GetOwnershipRootCount(g_state));
    ZrCore_OwnershipTransfer_Free(g_state, envelope);
}

static void test_queue_close_aborts_prepared_or_queued_owner_exactly_once(void) {
    SZrTypeValue source;
    SZrOwnershipTransferEnvelope *envelope;

    init_resource_unique(&source);
    envelope = ZrCore_OwnershipTransfer_PrepareSameDomain(
            g_state, ZrCore_GcDomain_GetIdentity(g_state), &source);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 0u, 0u));
    ZrCore_OwnershipTransfer_Free(g_state, envelope);
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
}

static void test_claimed_owner_requires_matching_worker_and_epoch_to_abort(void) {
    SZrTypeValue source;
    SZrOwnershipTransferEnvelope *envelope;

    init_resource_unique(&source);
    envelope = ZrCore_OwnershipTransfer_PrepareSameDomain(
            g_state, ZrCore_GcDomain_GetIdentity(g_state), &source);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_state, 91u, 12u));
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 90u, 12u));
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 91u, 11u));
    TEST_ASSERT_EQUAL_UINT32(0u, g_drop_count);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 91u, 12u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
    ZrCore_OwnershipTransfer_Free(g_state, envelope);
}

static void test_cross_domain_claim_is_rejected_without_losing_payload(void) {
    SZrState *other = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue source;
    SZrOwnershipTransferEnvelope *envelope;

    TEST_ASSERT_NOT_NULL(other);
    init_resource_unique(&source);
    envelope = ZrCore_OwnershipTransfer_PrepareSameDomain(
            g_state, ZrCore_GcDomain_GetIdentity(g_state), &source);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_FALSE(ZrCore_OwnershipTransfer_Claim(
            envelope, other, 3u, 1u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_state, 3u, 1u));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Abort(
            envelope, g_state, 3u, 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
    ZrCore_OwnershipTransfer_Free(g_state, envelope);
    ZrTests_Runtime_State_Destroy(other);
}

static void test_queue_close_race_aborts_payload_once(void) {
    /* 两个同时 Abort 竞争同一终态；所有权根和析构必须只被消费一次。 */
    SZrTypeValue source;
    SZrOwnershipTransferEnvelope *envelope;
    ZrTransferTestAtomic start = 0;
    ZrOwnershipTransferRaceContext first = {0};
    ZrOwnershipTransferRaceContext second = {0};
    ZrTransferTestThread firstThread;
    ZrTransferTestThread secondThread;

    init_resource_unique(&source);
    envelope = ZrCore_OwnershipTransfer_PrepareSameDomain(
            g_state, ZrCore_GcDomain_GetIdentity(g_state), &source);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source.type));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    first.envelope = envelope;
    first.state = g_state;
    first.start = &start;
    second = first;
    firstThread = transfer_thread_start(&first);
    secondThread = transfer_thread_start(&second);
    transfer_test_atomic_store(&start, 1);
    transfer_thread_join(firstThread);
    transfer_thread_join(secondThread);

    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)first.result + (TZrUInt32)second.result);
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
    ZrCore_OwnershipTransfer_Free(g_state, envelope);
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
}

static void test_worker_exit_and_commit_race_has_one_terminal_owner(void) {
    /* Commit 与 worker-exit Abort 使用相同 worker/epoch 竞争，胜方
     * 唯一决定 payload 是交给 target 还是由 envelope 释放。 */
    SZrTypeValue source;
    SZrTypeValue target;
    SZrOwnershipTransferEnvelope *envelope;
    ZrTransferTestAtomic start = 0;
    ZrOwnershipTransferRaceContext commit = {0};
    ZrOwnershipTransferRaceContext workerExit = {0};
    ZrTransferTestThread commitThread;
    ZrTransferTestThread exitThread;

    init_resource_unique(&source);
    ZrCore_Value_ResetAsNull(&target);
    envelope = ZrCore_OwnershipTransfer_PrepareSameDomain(
            g_state, ZrCore_GcDomain_GetIdentity(g_state), &source);
    TEST_ASSERT_NOT_NULL(envelope);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(source.type));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Publish(envelope));
    TEST_ASSERT_TRUE(ZrCore_OwnershipTransfer_Claim(
            envelope, g_state, 77u, 9u));

    commit.envelope = envelope;
    commit.state = g_state;
    commit.target = &target;
    commit.start = &start;
    commit.workerId = 77u;
    commit.claimEpoch = 9u;
    commit.commit = ZR_TRUE;
    workerExit = commit;
    workerExit.target = ZR_NULL;
    workerExit.commit = ZR_FALSE;
    commitThread = transfer_thread_start(&commit);
    exitThread = transfer_thread_start(&workerExit);
    transfer_test_atomic_store(&start, 1);
    transfer_thread_join(commitThread);
    transfer_thread_join(exitThread);

    TEST_ASSERT_EQUAL_UINT32(
            1u, (TZrUInt32)commit.result + (TZrUInt32)workerExit.result);
    if (commit.result) {
        TEST_ASSERT_EQUAL_INT(
                ZR_OWNERSHIP_VALUE_KIND_UNIQUE, target.ownershipKind);
        TEST_ASSERT_EQUAL_UINT32(0u, g_drop_count);
        ZrCore_Ownership_ReleaseValue(g_state, &target);
    } else {
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(target.type));
    }
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
    ZrCore_OwnershipTransfer_Free(g_state, envelope);
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_count);
}

/* TODO: CMake 当前只构建此目标，仓库内未见 CTest/suite 引用；
 * 核查 CI 是否单独执行，若无则接入常规测试入口。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_same_domain_prepare_publish_claim_commit_moves_one_owner);
    RUN_TEST(test_queued_and_claimed_envelope_keeps_exact_ownership_root);
    RUN_TEST(test_queue_close_aborts_prepared_or_queued_owner_exactly_once);
    RUN_TEST(test_claimed_owner_requires_matching_worker_and_epoch_to_abort);
    RUN_TEST(test_cross_domain_claim_is_rejected_without_losing_payload);
    RUN_TEST(test_queue_close_race_aborts_payload_once);
    RUN_TEST(test_worker_exit_and_commit_race_has_one_terminal_owner);
    return UNITY_END();
}
