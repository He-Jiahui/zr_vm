#ifndef ZR_TEST_OWNERSHIP_DROP_FAILURE_CASES_H
#define ZR_TEST_OWNERSHIP_DROP_FAILURE_CASES_H

static SZrTypeValue *g_drop_callback_pending_value;
static TZrBool g_drop_callback_throws;
static TZrBool g_drop_callback_saw_empty_pending;
static TZrUInt32 g_drop_callback_count;

static TZrInt64 drop_callback_install_pending(SZrState *state) {
    ++g_drop_callback_count;
    g_drop_callback_saw_empty_pending = (TZrBool)(
            state->pendingControl.kind == ZR_VM_PENDING_CONTROL_NONE &&
            !state->pendingControl.hasValue);
    execution_set_pending_control(
            state, ZR_VM_PENDING_CONTROL_RETURN, state->callInfoList,
            41u, 2u, g_drop_callback_pending_value);
    if (g_drop_callback_throws) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    }
    return 0;
}

static void drop_failure_create_callback_owner(
        SZrTypeValue *owner, SZrTypeValue *callbackPending, TZrBool throws) {
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0);
    pending_create_shared(owner);
    TEST_ASSERT_NOT_NULL(destructor);
    destructor->nativeFunction = drop_callback_install_pending;
    ZrCore_RawObject_MarkAsPermanent(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    ZrCore_ObjectPrototype_AddMeta(
            g_state, ((SZrObject *)owner->value.object)->prototype,
            ZR_META_DESTRUCTOR,
            (SZrFunction *)ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    g_drop_callback_pending_value = callbackPending;
    g_drop_callback_throws = throws;
    g_drop_callback_saw_empty_pending = ZR_FALSE;
    g_drop_callback_count = 0u;
}

static void drop_failure_release_owner(SZrState *state, TZrPtr argument) {
    ZrCore_Ownership_ReleaseValue(state, (SZrTypeValue *)argument);
}

static void test_drop_failure_releases_callback_pending_return(void) {
    SZrTypeValue owner;
    SZrTypeValue callbackPending;
    SZrCallInfo *savedCallInfo = g_state->callInfoList;
    pending_create_shared(&callbackPending);
    drop_failure_create_callback_owner(&owner, &callbackPending, ZR_TRUE);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR,
            ZrCore_Exception_TryRun(g_state, drop_failure_release_owner, &owner));
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_callback_count);
    TEST_ASSERT_TRUE(g_drop_callback_saw_empty_pending);
    TEST_ASSERT_EQUAL_INT(ZR_VM_PENDING_CONTROL_NONE, g_state->pendingControl.kind);
    TEST_ASSERT_FALSE(g_state->pendingControl.hasValue);
    TEST_ASSERT_EQUAL_UINT32(1u, callbackPending.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_PTR(savedCallInfo, g_state->callInfoList);
    TEST_ASSERT_TRUE(g_state->hasCurrentException);
    ZrCore_Exception_ClearCurrent(g_state);
    g_state->threadStatus = ZR_THREAD_STATUS_FINE;
    ZrCore_Ownership_ReleaseValue(g_state, &callbackPending);
    g_drop_callback_pending_value = ZR_NULL;
}

static void drop_failure_assert_preserves_caller_pending(TZrBool throws) {
    SZrTypeValue owner;
    SZrTypeValue callerPending;
    SZrTypeValue callbackPending;
    SZrCallInfo *savedCallInfo = g_state->callInfoList;
    pending_create_shared(&callerPending);
    pending_create_shared(&callbackPending);
    drop_failure_create_callback_owner(&owner, &callbackPending, throws);
    pending_set_return(&callerPending);

    TEST_ASSERT_EQUAL_INT(throws ? ZR_THREAD_STATUS_RUNTIME_ERROR : ZR_THREAD_STATUS_FINE,
            ZrCore_Exception_TryRun(g_state, drop_failure_release_owner, &owner));
    TEST_ASSERT_EQUAL_UINT32(1u, g_drop_callback_count);
    TEST_ASSERT_TRUE(g_drop_callback_saw_empty_pending);
    TEST_ASSERT_EQUAL_INT(ZR_VM_PENDING_CONTROL_RETURN, g_state->pendingControl.kind);
    TEST_ASSERT_TRUE(g_state->pendingControl.hasValue);
    TEST_ASSERT_EQUAL_PTR(savedCallInfo, g_state->pendingControl.callInfo);
    TEST_ASSERT_EQUAL_UINT32(17u, g_state->pendingControl.targetInstructionOffset);
    TEST_ASSERT_EQUAL_UINT32(3u, g_state->pendingControl.valueSlot);
    TEST_ASSERT_EQUAL_PTR(callerPending.ownershipControl,
                          g_state->pendingControl.value.ownershipControl);
    TEST_ASSERT_EQUAL_UINT32(2u, callerPending.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, callbackPending.ownershipControl->strongRefCount);
    ZrCore_Exception_ClearCurrent(g_state);
    g_state->threadStatus = ZR_THREAD_STATUS_FINE;
    execution_clear_pending_control(g_state);
    ZrCore_Ownership_ReleaseValue(g_state, &callerPending);
    ZrCore_Ownership_ReleaseValue(g_state, &callbackPending);
    g_drop_callback_pending_value = ZR_NULL;
}

static void test_drop_success_preserves_caller_pending_return(void) {
    drop_failure_assert_preserves_caller_pending(ZR_FALSE);
}

static void test_drop_failure_preserves_caller_pending_return(void) {
    drop_failure_assert_preserves_caller_pending(ZR_TRUE);
}

#endif
