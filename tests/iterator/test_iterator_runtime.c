#include <string.h>

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/iterator_runtime.h"
#include "zr_vm_core/object.h"

/* 生产器的游标只由 MoveNext 回调推进，终止后不再发布 current。 */
typedef struct SZrIteratorRuntimeTestProducer {
    TZrInt64 values[3];
    TZrSize count;
    TZrSize nextIndex;
} SZrIteratorRuntimeTestProducer;

/* 记录终态清理次数，以检查重复 MoveNext/Close 的幂等性。 */
typedef struct SZrIteratorRuntimeTestCleanup {
    TZrUInt32 count;
} SZrIteratorRuntimeTestCleanup;

/* 同一 userData 在生产和清理回调之间共享。 */
typedef struct SZrIteratorRuntimeTestContext {
    SZrIteratorRuntimeTestProducer producer;
    SZrIteratorRuntimeTestCleanup cleanup;
} SZrIteratorRuntimeTestContext;

/* 捕获同一个 frame 内嵌套 MoveNext 的拒绝结果。 */
typedef struct SZrIteratorRuntimeTestReentrantProducer {
    TZrBool attemptedNestedMove;
    TZrBool nestedMoveResult;
    TZrUInt32 publishCount;
} SZrIteratorRuntimeTestReentrantProducer;

/* 原始对象指针只用于发布，跨 compact GC 的可达性由 frame root 保证。 */
typedef struct SZrIteratorRuntimeTestObjectProducer {
    SZrObject *objects[2];
    TZrSize count;
    TZrSize nextIndex;
} SZrIteratorRuntimeTestObjectProducer;

/* Unity 在每个用例前后重建状态；回调均在该状态存活期间执行。 */
static SZrState *g_state;

/* MoveNext 调用的生产回调：每次仅发布一个值，耗尽时显式完成。 */
static void iterator_runtime_test_produce(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestProducer *producer =
            (SZrIteratorRuntimeTestProducer *) userData;
    SZrTypeValue value;

    if (producer->nextIndex >= producer->count) {
        ZrCore_IteratorFrame_Complete(state, frame);
        return;
    }

    ZrCore_Value_InitAsInt(state, &value, producer->values[producer->nextIndex]);
    producer->nextIndex++;
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Publish(state, frame, &value));
}

/* 让 MoveNext 路径触发正常终态。 */
static void iterator_runtime_test_complete(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData);
    ZrCore_IteratorFrame_Complete(state, frame);
}

/* 让 MoveNext 路径触发故障终态。 */
static void iterator_runtime_test_fault(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData);
    ZrCore_IteratorFrame_Fault(state, frame);
}

/* 终态回调不得自行改变 frame 状态，只记录调用次数。 */
static void iterator_runtime_test_cleanup(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestCleanup *cleanup =
            (SZrIteratorRuntimeTestCleanup *) userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(frame);
    cleanup->count++;
}

/* 将组合 userData 中的生产器转交给通用生产回调。 */
static void iterator_runtime_test_context_produce(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestContext *context =
            (SZrIteratorRuntimeTestContext *) userData;

    iterator_runtime_test_produce(state, frame, &context->producer);
}

/* 将组合 userData 中的清理计数器转交给通用清理回调。 */
static void iterator_runtime_test_context_cleanup(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestContext *context =
            (SZrIteratorRuntimeTestContext *) userData;

    iterator_runtime_test_cleanup(state, frame, &context->cleanup);
}

/* 在生产回调内重入同一 frame，验证内层调用不会覆盖外层发布。 */
static void iterator_runtime_test_reentrant_produce(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestReentrantProducer *producer =
            (SZrIteratorRuntimeTestReentrantProducer *) userData;
    SZrTypeValue value;

    if (!producer->attemptedNestedMove) {
        producer->attemptedNestedMove = ZR_TRUE;
        producer->nestedMoveResult = ZrCore_IteratorFrame_MoveNext(state, frame);
    }
    if (frame->state == ZR_ITERATOR_FRAME_READY) {
        ZrCore_Value_InitAsInt(state, &value, 9);
        producer->publishCount++;
        TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Publish(state, frame, &value));
    }
}

/* 发布 GC 对象，供 currentRoot 的建立、替换和释放用例复用。 */
static void iterator_runtime_test_object_produce(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestObjectProducer *producer =
            (SZrIteratorRuntimeTestObjectProducer *) userData;
    SZrTypeValue value;

    if (producer->nextIndex >= producer->count) {
        ZrCore_IteratorFrame_Complete(state, frame);
        return;
    }

    ZrCore_Value_InitAsRawObject(
            state,
            &value,
            ZR_CAST_RAW_OBJECT_AS_SUPER(
                    producer->objects[producer->nextIndex]));
    producer->nextIndex++;
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Publish(state, frame, &value));
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* IteratorFrame 的源语言迭代顺序：逐次产值，耗尽后 current 失效。 */
static void test_iterator_frame_yields_multiple_values_then_completes(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestProducer producer;
    SZrTypeValue current;

    memset(&producer, 0, sizeof(producer));
    producer.values[0] = 1;
    producer.values[1] = 2;
    producer.values[2] = 3;
    producer.count = 3U;
    ZrCore_IteratorFrame_Init(g_state, &frame, iterator_runtime_test_produce, &producer, ZR_NULL);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    TEST_ASSERT_EQUAL_INT64(1, current.value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    TEST_ASSERT_EQUAL_INT64(2, current.value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    TEST_ASSERT_EQUAL_INT64(3, current.value.nativeObject.nativeInt64);
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_COMPLETED, frame.state);
}

/* 正常完成后的重复推进不能再次清理 userData。 */
static void test_iterator_frame_completion_runs_cleanup_once(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestCleanup cleanup;

    memset(&cleanup, 0, sizeof(cleanup));
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_runtime_test_complete,
            &cleanup,
            iterator_runtime_test_cleanup);

    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_UINT32(1U, cleanup.count);
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_UINT32(1U, cleanup.count);
}

/* 故障也是终态，重复推进不能重入清理回调。 */
static void test_iterator_frame_fault_runs_cleanup_once(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestCleanup cleanup;

    memset(&cleanup, 0, sizeof(cleanup));
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_runtime_test_fault,
            &cleanup,
            iterator_runtime_test_cleanup);

    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_FAULTED, frame.state);
    TEST_ASSERT_EQUAL_UINT32(1U, cleanup.count);
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_UINT32(1U, cleanup.count);
}

/* 缺失生产回调须走故障清理，避免半初始化 frame 泄漏。 */
static void test_iterator_frame_missing_producer_faults_and_cleans_up(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestCleanup cleanup;

    memset(&cleanup, 0, sizeof(cleanup));
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            ZR_NULL,
            &cleanup,
            iterator_runtime_test_cleanup);

    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_FAULTED, frame.state);
    TEST_ASSERT_EQUAL_UINT32(1U, cleanup.count);
}

/* 先到的终态决定外部可见结果，后续终态请求不能改写它。 */
static void test_iterator_frame_preserves_the_first_terminal_state(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestCleanup cleanup;

    memset(&cleanup, 0, sizeof(cleanup));
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            ZR_NULL,
            &cleanup,
            iterator_runtime_test_cleanup);

    ZrCore_IteratorFrame_Fault(g_state, &frame);
    ZrCore_IteratorFrame_Complete(g_state, &frame);
    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_FAULTED, frame.state);
    TEST_ASSERT_EQUAL_UINT32(1U, cleanup.count);

    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            ZR_NULL,
            &cleanup,
            iterator_runtime_test_cleanup);
    ZrCore_IteratorFrame_Complete(g_state, &frame);
    ZrCore_IteratorFrame_Fault(g_state, &frame);
    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_COMPLETED, frame.state);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup.count);
}

/* 消费方提前退出时 Close 使 current 失效并只清理一次。 */
static void test_iterator_frame_early_close_runs_cleanup_once(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestContext context;
    SZrTypeValue current;

    memset(&context, 0, sizeof(context));
    context.producer.values[0] = 7;
    context.producer.count = 1U;
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_runtime_test_context_produce,
            &context,
            iterator_runtime_test_context_cleanup);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_CLOSED, frame.state);
    TEST_ASSERT_EQUAL_UINT32(1U, context.cleanup.count);
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_UINT32(1U, context.cleanup.count);
}

/* 同 frame 重入被拒绝后外层仍能完成一次发布。 */
static void test_iterator_frame_rejects_same_frame_reentrancy(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestReentrantProducer producer;

    memset(&producer, 0, sizeof(producer));
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_runtime_test_reentrant_produce,
            &producer,
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_TRUE(producer.attemptedNestedMove);
    TEST_ASSERT_FALSE(producer.nestedMoveResult);
    TEST_ASSERT_EQUAL_UINT32(1U, producer.publishCount);
}

/* 发布的对象经 compact GC 移动后 Current 必须解析新地址。 */
static void test_iterator_frame_roots_current_object_across_compact_gc(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestObjectProducer producer;
    SZrTypeValue current;
    SZrRawObject *resolved = ZR_NULL;
    TZrSize rootCountBefore = ZrCore_GcDomain_GetRootCount(g_state);

    memset(&producer, 0, sizeof(producer));
    producer.objects[0] = ZrCore_Object_New(g_state, ZR_NULL);
    producer.count = 1U;
    TEST_ASSERT_NOT_NULL(producer.objects[0]);
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_runtime_test_object_produce,
            &producer,
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_TRUE(frame.hasCurrentRoot);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(rootCountBefore + 1U),
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);

    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
            g_state, &frame.currentRoot, &resolved));
    TEST_ASSERT_NOT_NULL(resolved);
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    TEST_ASSERT_EQUAL_PTR(resolved, current.value.object);

    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)rootCountBefore,
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));
}

/* 连续发布对象只保留当前一个 root，避免 root 数随迭代增长。 */
static void test_iterator_frame_replaces_the_previous_object_root(void) {
    SZrIteratorFrame frame;
    SZrIteratorRuntimeTestObjectProducer producer;
    TZrSize rootCountBefore = ZrCore_GcDomain_GetRootCount(g_state);

    memset(&producer, 0, sizeof(producer));
    producer.objects[0] = ZrCore_Object_New(g_state, ZR_NULL);
    producer.objects[1] = ZrCore_Object_New(g_state, ZR_NULL);
    producer.count = 2U;
    TEST_ASSERT_NOT_NULL(producer.objects[0]);
    TEST_ASSERT_NOT_NULL(producer.objects[1]);
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_runtime_test_object_produce,
            &producer,
            ZR_NULL);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(rootCountBefore + 1U),
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(rootCountBefore + 1U),
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));

    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)rootCountBefore,
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));
}

/* 池复用同一内存时，旧 current、回调和清理标记必须重新初始化。 */
static void test_iterator_frame_pool_reuses_storage_without_state_leaks(void) {
    SZrIteratorFramePool pool;
    SZrIteratorFrame *firstFrame;
    SZrIteratorFrame *secondFrame;
    SZrIteratorRuntimeTestContext firstContext;
    SZrIteratorRuntimeTestContext secondContext;
    SZrTypeValue current;

    memset(&firstContext, 0, sizeof(firstContext));
    memset(&secondContext, 0, sizeof(secondContext));
    firstContext.producer.values[0] = 41;
    firstContext.producer.count = 1U;
    ZrCore_IteratorFramePool_Init(&pool);

    firstFrame = ZrCore_IteratorFramePool_Acquire(
            g_state,
            &pool,
            iterator_runtime_test_context_produce,
            &firstContext,
            iterator_runtime_test_context_cleanup);
    TEST_ASSERT_NOT_NULL(firstFrame);
    TEST_ASSERT_EQUAL_UINT64(1U, (TZrUInt64)pool.allocationCount);
    TEST_ASSERT_EQUAL_UINT64(0U, (TZrUInt64)pool.reuseCount);
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, firstFrame));
    ZrCore_IteratorFrame_Close(g_state, firstFrame);
    TEST_ASSERT_EQUAL_UINT32(1U, firstContext.cleanup.count);
    TEST_ASSERT_TRUE(ZrCore_IteratorFramePool_Release(
            g_state, &pool, firstFrame));

    secondFrame = ZrCore_IteratorFramePool_Acquire(
            g_state,
            &pool,
            iterator_runtime_test_complete,
            &secondContext,
            iterator_runtime_test_context_cleanup);
    TEST_ASSERT_EQUAL_PTR(firstFrame, secondFrame);
    TEST_ASSERT_EQUAL_UINT64(1U, (TZrUInt64)pool.allocationCount);
    TEST_ASSERT_EQUAL_UINT64(1U, (TZrUInt64)pool.reuseCount);
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_READY, secondFrame->state);
    TEST_ASSERT_FALSE(secondFrame->cleanupInvoked);
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_Current(g_state, secondFrame, &current));
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, secondFrame));
    TEST_ASSERT_EQUAL_UINT32(1U, secondContext.cleanup.count);
    TEST_ASSERT_TRUE(ZrCore_IteratorFramePool_Release(
            g_state, &pool, secondFrame));
    ZrCore_IteratorFramePool_Free(g_state, &pool);
}

/* 运行中的 frame 不可归还池；终态后才允许新租约复用。 */
static void test_iterator_frame_pool_rejects_a_nonterminal_lease(void) {
    SZrIteratorFramePool pool;
    SZrIteratorFrame *frame;

    ZrCore_IteratorFramePool_Init(&pool);
    frame = ZrCore_IteratorFramePool_Acquire(
            g_state,
            &pool,
            iterator_runtime_test_complete,
            ZR_NULL,
            ZR_NULL);
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_READY, frame->state);
    TEST_ASSERT_FALSE(ZrCore_IteratorFramePool_Release(g_state, &pool, frame));

    ZrCore_IteratorFrame_Close(g_state, frame);
    TEST_ASSERT_TRUE(ZrCore_IteratorFramePool_Release(g_state, &pool, frame));
    ZrCore_IteratorFramePool_Free(g_state, &pool);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_iterator_frame_yields_multiple_values_then_completes);
    RUN_TEST(test_iterator_frame_completion_runs_cleanup_once);
    RUN_TEST(test_iterator_frame_fault_runs_cleanup_once);
    RUN_TEST(test_iterator_frame_missing_producer_faults_and_cleans_up);
    RUN_TEST(test_iterator_frame_preserves_the_first_terminal_state);
    RUN_TEST(test_iterator_frame_early_close_runs_cleanup_once);
    RUN_TEST(test_iterator_frame_rejects_same_frame_reentrancy);
    RUN_TEST(test_iterator_frame_roots_current_object_across_compact_gc);
    RUN_TEST(test_iterator_frame_replaces_the_previous_object_root);
    RUN_TEST(test_iterator_frame_pool_reuses_storage_without_state_leaks);
    RUN_TEST(test_iterator_frame_pool_rejects_a_nonterminal_lease);
    return UNITY_END();
}
