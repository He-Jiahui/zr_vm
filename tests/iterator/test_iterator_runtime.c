#include <string.h>

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/iterator_runtime.h"
#include "zr_vm_core/object.h"

typedef struct SZrIteratorRuntimeTestProducer {
    TZrInt64 values[3];
    TZrSize count;
    TZrSize nextIndex;
} SZrIteratorRuntimeTestProducer;

typedef struct SZrIteratorRuntimeTestCleanup {
    TZrUInt32 count;
} SZrIteratorRuntimeTestCleanup;

typedef struct SZrIteratorRuntimeTestContext {
    SZrIteratorRuntimeTestProducer producer;
    SZrIteratorRuntimeTestCleanup cleanup;
} SZrIteratorRuntimeTestContext;

typedef struct SZrIteratorRuntimeTestReentrantProducer {
    TZrBool attemptedNestedMove;
    TZrBool nestedMoveResult;
    TZrUInt32 publishCount;
} SZrIteratorRuntimeTestReentrantProducer;

typedef struct SZrIteratorRuntimeTestObjectProducer {
    SZrObject *objects[2];
    TZrSize count;
    TZrSize nextIndex;
} SZrIteratorRuntimeTestObjectProducer;

static SZrState *g_state;

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

static void iterator_runtime_test_complete(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData);
    ZrCore_IteratorFrame_Complete(state, frame);
}

static void iterator_runtime_test_fault(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData);
    ZrCore_IteratorFrame_Fault(state, frame);
}

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

static void iterator_runtime_test_context_produce(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestContext *context =
            (SZrIteratorRuntimeTestContext *) userData;

    iterator_runtime_test_produce(state, frame, &context->producer);
}

static void iterator_runtime_test_context_cleanup(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorRuntimeTestContext *context =
            (SZrIteratorRuntimeTestContext *) userData;

    iterator_runtime_test_cleanup(state, frame, &context->cleanup);
}

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
