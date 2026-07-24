#include <string.h>

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/iterator_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"

typedef enum EZrIteratorGcDropTerminal {
    ZR_ITERATOR_GC_DROP_COMPLETE = 0,
    ZR_ITERATOR_GC_DROP_FAULT
} EZrIteratorGcDropTerminal;

typedef struct SZrIteratorGcDropContext {
    SZrObject *yieldedObject;
    SZrTypeValue cleanupOwner;
    EZrIteratorGcDropTerminal terminal;
    TZrSize rootCountDuringCleanup;
    TZrUInt32 cleanupCount;
    TZrBool emitted;
} SZrIteratorGcDropContext;

static SZrState *g_state;
static TZrUInt32 g_resourceDropCount;

static TZrInt64 iterator_gc_drop_resource_destructor(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    g_resourceDropCount++;
    return 0;
}

static void iterator_gc_drop_init_resource_unique(
        SZrIteratorGcDropContext *context) {
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "IteratorCleanupResource");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(
            g_state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0u);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(destructor);
    prototype->modifierFlags |= ZR_TYPE_MODIFIER_FLAG_RESOURCE;
    destructor->nativeFunction = iterator_gc_drop_resource_destructor;
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
    ZrCore_Value_ResetAsNull(&context->cleanupOwner);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(
            g_state,
            &context->cleanupOwner,
            ZR_CAST_RAW_OBJECT_AS_SUPER(object)));
}

static void iterator_gc_drop_produce(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorGcDropContext *context =
            (SZrIteratorGcDropContext *)userData;
    SZrTypeValue value;

    if (!context->emitted) {
        ZrCore_Value_InitAsRawObject(
                state,
                &value,
                ZR_CAST_RAW_OBJECT_AS_SUPER(context->yieldedObject));
        context->emitted = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Publish(state, frame, &value));
        return;
    }
    if (context->terminal == ZR_ITERATOR_GC_DROP_COMPLETE) {
        ZrCore_IteratorFrame_Complete(state, frame);
    } else {
        ZrCore_IteratorFrame_Fault(state, frame);
    }
}

static void iterator_gc_drop_cleanup(
        SZrState *state,
        SZrIteratorFrame *frame,
        TZrPtr userData) {
    SZrIteratorGcDropContext *context =
            (SZrIteratorGcDropContext *)userData;

    ZR_UNUSED_PARAMETER(frame);
    context->rootCountDuringCleanup = ZrCore_GcDomain_GetRootCount(state);
    context->cleanupCount++;
    ZrCore_Ownership_ReleaseValue(state, &context->cleanupOwner);
}

static void iterator_gc_drop_init_context(
        SZrIteratorGcDropContext *context,
        EZrIteratorGcDropTerminal terminal) {
    memset(context, 0, sizeof(*context));
    context->yieldedObject = ZrCore_Object_New(g_state, ZR_NULL);
    context->terminal = terminal;
    TEST_ASSERT_NOT_NULL(context->yieldedObject);
    iterator_gc_drop_init_resource_unique(context);
}

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_resourceDropCount = 0u;
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void test_iterator_gc_root_resolves_current_value_after_compact_collection(void) {
    SZrIteratorFrame frame;
    SZrIteratorGcDropContext context;
    SZrTypeValue current;
    SZrRawObject *resolved = ZR_NULL;
    TZrSize rootCountBefore;

    iterator_gc_drop_init_context(&context, ZR_ITERATOR_GC_DROP_COMPLETE);
    rootCountBefore = ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_gc_drop_produce,
            &context,
            iterator_gc_drop_cleanup);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(rootCountBefore + 1U),
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
            g_state, &frame.currentRoot, &resolved));
    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_Current(g_state, &frame, &current));
    TEST_ASSERT_EQUAL_PTR(resolved, current.value.object);

    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_UINT32(1u, context.cleanupCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_resourceDropCount);
}

static void test_iterator_completion_releases_root_before_direct_owner_cleanup(void) {
    SZrIteratorFrame frame;
    SZrIteratorGcDropContext context;
    TZrSize rootCountBefore;

    iterator_gc_drop_init_context(&context, ZR_ITERATOR_GC_DROP_COMPLETE);
    rootCountBefore = ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_gc_drop_produce,
            &context,
            iterator_gc_drop_cleanup);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_COMPLETED, frame.state);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)rootCountBefore,
            (TZrUInt64)context.rootCountDuringCleanup);
    TEST_ASSERT_EQUAL_UINT32(1u, context.cleanupCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_resourceDropCount);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(rootCountBefore - 1U),
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_iterator_fault_releases_root_before_direct_owner_cleanup(void) {
    SZrIteratorFrame frame;
    SZrIteratorGcDropContext context;
    TZrSize rootCountBefore;

    iterator_gc_drop_init_context(&context, ZR_ITERATOR_GC_DROP_FAULT);
    rootCountBefore = ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_gc_drop_produce,
            &context,
            iterator_gc_drop_cleanup);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_FALSE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_FAULTED, frame.state);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)rootCountBefore,
            (TZrUInt64)context.rootCountDuringCleanup);
    TEST_ASSERT_EQUAL_UINT32(1u, context.cleanupCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_resourceDropCount);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(rootCountBefore - 1U),
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_iterator_close_releases_root_before_direct_owner_cleanup_once(void) {
    SZrIteratorFrame frame;
    SZrIteratorGcDropContext context;
    TZrSize rootCountBefore;

    iterator_gc_drop_init_context(&context, ZR_ITERATOR_GC_DROP_COMPLETE);
    rootCountBefore = ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_IteratorFrame_Init(
            g_state,
            &frame,
            iterator_gc_drop_produce,
            &context,
            iterator_gc_drop_cleanup);

    TEST_ASSERT_TRUE(ZrCore_IteratorFrame_MoveNext(g_state, &frame));
    ZrCore_IteratorFrame_Close(g_state, &frame);
    ZrCore_IteratorFrame_Close(g_state, &frame);
    TEST_ASSERT_EQUAL_INT(ZR_ITERATOR_FRAME_CLOSED, frame.state);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)rootCountBefore,
            (TZrUInt64)context.rootCountDuringCleanup);
    TEST_ASSERT_EQUAL_UINT32(1u, context.cleanupCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_resourceDropCount);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(rootCountBefore - 1U),
            (TZrUInt64)ZrCore_GcDomain_GetRootCount(g_state));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_iterator_gc_root_resolves_current_value_after_compact_collection);
    RUN_TEST(test_iterator_completion_releases_root_before_direct_owner_cleanup);
    RUN_TEST(test_iterator_fault_releases_root_before_direct_owner_cleanup);
    RUN_TEST(test_iterator_close_releases_root_before_direct_owner_cleanup_once);
    return UNITY_END();
}
