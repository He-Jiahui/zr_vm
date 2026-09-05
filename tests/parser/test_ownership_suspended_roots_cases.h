#ifndef ZR_TEST_OWNERSHIP_SUSPENDED_ROOTS_CASES_H
#define ZR_TEST_OWNERSHIP_SUSPENDED_ROOTS_CASES_H

#include "zr_vm_core/gc_domain.h"

static SZrVmExceptionHandlerState *suspended_test_handler_for_state(SZrState *state) {
    SZrVmExceptionHandlerState *handler;
    TEST_ASSERT_EQUAL_UINT32(0u, state->exceptionHandlerStackLength);
    if (state->exceptionHandlerStackCapacity == 0u) {
        state->exceptionHandlerStack = ZrCore_Memory_RawMallocWithType(
                state->global, sizeof(SZrVmExceptionHandlerState), ZR_MEMORY_NATIVE_TYPE_STATE);
        TEST_ASSERT_NOT_NULL(state->exceptionHandlerStack);
        state->exceptionHandlerStackCapacity = 1u;
    }
    handler = &state->exceptionHandlerStack[0];
    memset(handler, 0, sizeof(*handler));
    handler->callInfo = state->callInfoList;
    handler->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
    ZrCore_Value_ResetAsNull(&handler->suspendedControl.value);
    ZrCore_Value_ResetAsNull(&handler->suspendedException);
    state->exceptionHandlerStackLength = 1u;
    return handler;
}

static SZrVmExceptionHandlerState *suspended_test_handler(void) {
    return suspended_test_handler_for_state(g_state);
}

static void test_suspended_shared_and_weak_reset_release_retention(void) {
    SZrTypeValue shared;
    SZrTypeValue weak;
    SZrVmExceptionHandlerState *handler;
    pending_create_shared(&shared);
    ZrCore_Value_ResetAsNull(&weak);
    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));
    handler = suspended_test_handler();
    ZrCore_Value_Copy(g_state, &handler->suspendedControl.value, &shared);
    handler->suspendedControl.kind = ZR_VM_PENDING_CONTROL_RETURN;
    handler->suspendedControl.hasValue = ZR_TRUE;
    TEST_ASSERT_EQUAL_UINT64(2u, shared.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
            ZrCore_State_ResetThread(g_state, ZR_THREAD_STATUS_FINE));
    TEST_ASSERT_EQUAL_UINT64(1u, shared.ownershipControl->strongRefCount);
    handler = suspended_test_handler();
    ZrCore_Value_Copy(g_state, &handler->suspendedControl.value, &weak);
    handler->suspendedControl.kind = ZR_VM_PENDING_CONTROL_RETURN;
    handler->suspendedControl.hasValue = ZR_TRUE;
    TEST_ASSERT_EQUAL_UINT64(3u, shared.ownershipControl->weakRefCount);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
            ZrCore_State_ResetThread(g_state, ZR_THREAD_STATUS_FINE));
    TEST_ASSERT_EQUAL_UINT64(2u, shared.ownershipControl->weakRefCount);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
}

static void test_suspended_return_and_exception_are_gc_roots(void) {
    SZrVmExceptionHandlerState *handler = suspended_test_handler();
    char resultText[ZR_VM_SHORT_STRING_MAX + 65u];
    char errorText[ZR_VM_SHORT_STRING_MAX + 65u];
    SZrString *value;
    memset(resultText, 'r', sizeof(resultText) - 1u);
    memset(errorText, 'e', sizeof(errorText) - 1u);
    resultText[sizeof(resultText) - 1u] = '\0';
    errorText[sizeof(errorText) - 1u] = '\0';
    value = ZrCore_String_Create(g_state, resultText, sizeof(resultText) - 1u);
    TEST_ASSERT_NOT_NULL(value);
    ZrCore_Value_InitAsRawObject(g_state, &handler->suspendedControl.value,
            ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    handler->suspendedControl.value.type = ZR_VALUE_TYPE_STRING;
    handler->suspendedControl.kind = ZR_VM_PENDING_CONTROL_RETURN;
    handler->suspendedControl.hasValue = ZR_TRUE;
    value = ZrCore_String_Create(g_state, errorText, sizeof(errorText) - 1u);
    TEST_ASSERT_NOT_NULL(value);
    ZrCore_Value_InitAsRawObject(g_state, &handler->suspendedException,
            ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    handler->suspendedException.type = ZR_VALUE_TYPE_STRING;
    handler->hasSuspendedException = ZR_TRUE;
    handler->suspendedExceptionStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    value = ZR_NULL;
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    handler = &g_state->exceptionHandlerStack[0];
    TEST_ASSERT_EQUAL_STRING(resultText,
            ZrCore_String_GetNativeString((SZrString *)handler->suspendedControl.value.value.object));
    TEST_ASSERT_EQUAL_STRING(errorText,
            ZrCore_String_GetNativeString((SZrString *)handler->suspendedException.value.object));
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
            ZrCore_State_ResetThread(g_state, ZR_THREAD_STATUS_FINE));
}

static void test_suspended_roots_relocate_only_in_their_own_domain(void) {
    SZrState *other = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrVmExceptionHandlerState *handler;
    SZrRawObject *before[2];
    SZrGarbageCollector *collector;
    TEST_ASSERT_NOT_NULL(other);
    TEST_ASSERT_FALSE(ZrCore_GcDomain_IdentityEquals(
            ZrCore_GcDomain_GetIdentity(g_state), ZrCore_GcDomain_GetIdentity(other)));
    handler = suspended_test_handler_for_state(other);
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        SZrObject *object = ZrCore_Object_New(other, ZR_NULL);
        TEST_ASSERT_NOT_NULL(object);
        ZrCore_Object_Init(other, object);
        before[index] = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        ZrCore_Value_InitAsRawObject(other, index == 0u
                ? &handler->suspendedControl.value : &handler->suspendedException, before[index]);
    }
    handler->suspendedControl.kind = ZR_VM_PENDING_CONTROL_RETURN;
    handler->suspendedControl.hasValue = ZR_TRUE;
    handler->hasSuspendedException = ZR_TRUE;
    handler->suspendedExceptionStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_PTR(before[0], handler->suspendedControl.value.value.object);
    TEST_ASSERT_EQUAL_PTR(before[1], handler->suspendedException.value.object);
    collector = other->global->garbageCollector;
    collector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    collector->gcDebtSize = 4096;
    collector->gcLastStepWork = 0u;
    ZrCore_GarbageCollector_GcStep(other);
    handler = &other->exceptionHandlerStack[0];
    TEST_ASSERT_TRUE(handler->suspendedControl.value.value.object == before[0] ||
            before[0]->garbageCollectMark.forwardingAddress ==
                    handler->suspendedControl.value.value.object);
    TEST_ASSERT_TRUE(handler->suspendedException.value.object == before[1] ||
            before[1]->garbageCollectMark.forwardingAddress ==
                    handler->suspendedException.value.object);
    TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR,
            handler->suspendedControl.value.value.object->garbageCollectMark.regionKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR,
            handler->suspendedException.value.object->garbageCollectMark.regionKind);
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(other,
            handler->suspendedControl.value.value.object));
    TEST_ASSERT_TRUE(ZrCore_GcDomain_ObjectBelongsToState(other,
            handler->suspendedException.value.object));
    ZrTests_Runtime_State_Destroy(other);
}

static TZrUInt32 g_suspended_growing_drop_count;

static TZrInt64 suspended_growing_drop(SZrState *state) {
    SZrVmExceptionHandlerState *before = state->exceptionHandlerStack;
    TZrUInt32 baseLength = state->exceptionHandlerStackLength;
    TZrUInt32 count = state->exceptionHandlerStackCapacity + 1u;
    ++g_suspended_growing_drop_count;
    for (TZrUInt32 index = 0u; index < count; ++index) {
        TEST_ASSERT_TRUE(execution_push_exception_handler(state, state->callInfoList, index));
    }
    TEST_ASSERT_NOT_EQUAL((TZrNativePtr)before, (TZrNativePtr)state->exceptionHandlerStack);
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    while (state->exceptionHandlerStackLength > baseLength) {
        execution_pop_exception_handler(state,
                &state->exceptionHandlerStack[state->exceptionHandlerStackLength - 1u]);
    }
    return 0;
}

static void test_suspended_drop_survives_handler_array_relocation(void) {
    SZrTypeValue owner, weak;
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0u);
    TZrUInt32 rootDepth = g_state->aotGcRootFrameDepth;
    TEST_ASSERT_NOT_NULL(destructor);
    destructor->nativeFunction = suspended_growing_drop;
    ZrCore_RawObject_MarkAsPermanent(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    ZrCore_Value_ResetAsNull(&weak);
    g_suspended_growing_drop_count = 0u;
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        pending_create_shared(&owner);
        ZrCore_ObjectPrototype_AddMeta(g_state, ((SZrObject *)owner.value.object)->prototype,
                ZR_META_DESTRUCTOR, (SZrFunction *)ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
        if (index == 0u) {
            TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &owner));
        }
        TEST_ASSERT_TRUE(execution_push_exception_handler(g_state, g_state->callInfoList, index));
        pending_set_return(&owner);
        execution_enter_finally(g_state,
                &g_state->exceptionHandlerStack[g_state->exceptionHandlerStackLength - 1u]);
        ZrCore_Ownership_ReleaseValue(g_state, &owner);
    }
    execution_pop_exception_handler(g_state, &g_state->exceptionHandlerStack[1]);
    TEST_ASSERT_EQUAL_UINT32(1u, g_suspended_growing_drop_count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_state->exceptionHandlerStackLength);
    TEST_ASSERT_EQUAL_PTR(weak.ownershipControl,
            g_state->exceptionHandlerStack[0].suspendedControl.value.ownershipControl);
    TEST_ASSERT_EQUAL_UINT64(1u, weak.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, ZrCore_State_ResetThread(g_state, ZR_THREAD_STATUS_FINE));
    TEST_ASSERT_EQUAL_UINT32(2u, g_suspended_growing_drop_count);
    TEST_ASSERT_EQUAL_UINT64(0u, weak.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_state->exceptionHandlerStackLength);
    TEST_ASSERT_EQUAL_UINT32(rootDepth, g_state->aotGcRootFrameDepth);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
}

static TZrUInt32 g_suspended_cleanup_drop_count;

static TZrInt64 suspended_cleanup_throwing_drop(SZrState *state) {
    SZrTypeValue payload;
    ++g_suspended_cleanup_drop_count;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    ZrCore_Value_InitAsInt(state, &payload, g_suspended_cleanup_drop_count);
    if (!ZrCore_Exception_NormalizeThrownValue(
                state, &payload, state->callInfoList, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
    }
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
    return 0;
}

static void assert_suspended_cleanup_preserves_failure(TZrBool withOriginalFailure) {
    SZrTypeValue owner;
    SZrTypeValue payload;
    SZrTypeValue key;
    const SZrTypeValue *actual;
    SZrString *field;
    TZrUInt32 rootDepth = g_state->aotGcRootFrameDepth;
    g_suspended_cleanup_drop_count = 0u;
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0);
        TEST_ASSERT_NOT_NULL(destructor);
        destructor->nativeFunction = suspended_cleanup_throwing_drop;
        ZrCore_RawObject_MarkAsPermanent(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
        pending_create_shared(&owner);
        ZrCore_ObjectPrototype_AddMeta(g_state, ((SZrObject *)owner.value.object)->prototype,
                ZR_META_DESTRUCTOR, (SZrFunction *)ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
        TEST_ASSERT_TRUE(execution_push_exception_handler(g_state, g_state->callInfoList, index));
        pending_set_return(&owner);
        execution_enter_finally(g_state,
                &g_state->exceptionHandlerStack[g_state->exceptionHandlerStackLength - 1u]);
        ZrCore_Ownership_ReleaseValue(g_state, &owner);
    }
    if (withOriginalFailure) {
        ZrCore_Value_InitAsInt(g_state, &payload, 73);
        TEST_ASSERT_TRUE(ZrCore_Exception_NormalizeThrownValue(
                g_state, &payload, g_state->callInfoList, ZR_THREAD_STATUS_EXCEPTION_ERROR));
        g_state->threadStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    }
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_EXCEPTION_ERROR,
            execution_discard_exception_handlers_to_depth(g_state, 0u));
    TEST_ASSERT_EQUAL_UINT32(2u, g_suspended_cleanup_drop_count);
    TEST_ASSERT_EQUAL_UINT32(0u, g_state->exceptionHandlerStackLength);
    TEST_ASSERT_EQUAL_UINT32(rootDepth, g_state->aotGcRootFrameDepth);
    TEST_ASSERT_FALSE(g_state->pendingControl.hasValue);
    TEST_ASSERT_TRUE(g_state->hasCurrentException);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_EXCEPTION_ERROR, g_state->threadStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_EXCEPTION_ERROR, g_state->currentExceptionStatus);
    field = ZrCore_String_CreateFromNative(g_state, "exception");
    TEST_ASSERT_NOT_NULL(field);
    ZrCore_Value_InitAsRawObject(g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(field));
    actual = ZrCore_Object_GetValue(g_state, (SZrObject *)g_state->currentException.value.object, &key);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(actual->type));
    TEST_ASSERT_EQUAL_INT64(withOriginalFailure ? 73 : 1, actual->value.nativeObject.nativeInt64);
    ZrCore_Exception_ClearCurrent(g_state);
    g_state->threadStatus = ZR_THREAD_STATUS_FINE;
}

static void test_suspended_cleanup_keeps_original_exception_through_gc_and_multiple_drops(void) {
    assert_suspended_cleanup_preserves_failure(ZR_TRUE);
}

static void test_suspended_cleanup_keeps_first_drop_exception_through_gc(void) {
    assert_suspended_cleanup_preserves_failure(ZR_FALSE);
}

#endif
