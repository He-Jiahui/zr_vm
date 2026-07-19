#ifndef ZR_VM_TEST_REFLECTION_RUNTIME_MODULE_IMPORT_ALLOCATOR_H
#define ZR_VM_TEST_REFLECTION_RUNTIME_MODULE_IMPORT_ALLOCATOR_H

static TZrBool reflection_import_allocator_fail_next_allocation = ZR_FALSE;
static TZrBool reflection_import_allocator_waiting_for_gc_retry = ZR_FALSE;
static SZrGlobalState *reflection_import_allocator_gc_global = ZR_NULL;
static SZrFunctionStackAnchor reflection_import_allocator_post_gc_caller_anchor;
static TZrBool reflection_import_allocator_post_gc_caller_anchor_valid = ZR_FALSE;

static void reflection_import_allocator_reset(void) {
    reflection_import_allocator_fail_next_allocation = ZR_FALSE;
    reflection_import_allocator_waiting_for_gc_retry = ZR_FALSE;
    reflection_import_allocator_gc_global = ZR_NULL;
    memset(&reflection_import_allocator_post_gc_caller_anchor,
           0,
           sizeof(reflection_import_allocator_post_gc_caller_anchor));
    reflection_import_allocator_post_gc_caller_anchor_valid = ZR_FALSE;
}

static void reflection_import_allocator_prepare_post_gc_caller(
        SZrState *state,
        TZrStackValuePointer callerRoot) {
    reflection_import_allocator_gc_global = state->global;
    ZrCore_Function_StackAnchorInit(
            state,
            callerRoot,
            &reflection_import_allocator_post_gc_caller_anchor);
    reflection_import_allocator_post_gc_caller_anchor_valid = ZR_TRUE;
}

static void reflection_import_allocator_fail_next(void) {
    reflection_import_allocator_fail_next_allocation = ZR_TRUE;
}

static TZrBool reflection_import_allocator_before_allocate(void) {
    SZrState *state;
    TZrStackValuePointer callerRoot;
    SZrTypeValue *callerValue;

    if (reflection_import_allocator_fail_next_allocation) {
        reflection_import_allocator_fail_next_allocation = ZR_FALSE;
        reflection_import_allocator_waiting_for_gc_retry = ZR_TRUE;
        return ZR_TRUE;
    }
    if (!reflection_import_allocator_waiting_for_gc_retry ||
        reflection_import_allocator_gc_global == ZR_NULL ||
        !reflection_import_allocator_post_gc_caller_anchor_valid) {
        return ZR_FALSE;
    }

    state = reflection_import_allocator_gc_global->mainThreadState;
    if (state == ZR_NULL || state->baseCallInfo.previous == ZR_NULL) {
        return ZR_FALSE;
    }
    callerRoot = ZrCore_Function_StackAnchorRestore(
            state, &reflection_import_allocator_post_gc_caller_anchor);
    callerValue = ZrCore_Stack_GetValue(callerRoot);
    if (callerValue == ZR_NULL || callerValue->type != ZR_VALUE_TYPE_FUNCTION ||
        !callerValue->isGarbageCollectable || callerValue->isNative ||
        callerValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    state->baseCallInfo.previous->metadataFunction =
            (SZrFunction *)callerValue->value.object;
    reflection_import_allocator_waiting_for_gc_retry = ZR_FALSE;
    return ZR_FALSE;
}

static void reflection_import_allocator_assert_consumed_and_clear(void) {
    TEST_ASSERT_FALSE(reflection_import_allocator_fail_next_allocation);
    TEST_ASSERT_FALSE(reflection_import_allocator_waiting_for_gc_retry);
    reflection_import_allocator_reset();
}

#endif
