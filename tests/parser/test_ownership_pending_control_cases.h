#ifndef ZR_TEST_OWNERSHIP_PENDING_CONTROL_CASES_H
#define ZR_TEST_OWNERSHIP_PENDING_CONTROL_CASES_H

static void pending_assert_script(const TZrChar *body, TZrInt64 expected);

static void pending_create_shared(SZrTypeValue *shared) {
    SZrTypeValue unique;
    init_direct_unique(create_resource_object(), &unique);
    ZrCore_Value_ResetAsNull(shared);
    TEST_ASSERT_TRUE(ZrCore_Ownership_ShareValue(g_state, shared, &unique));
}

static void pending_set_return(const SZrTypeValue *value) {
    execution_set_pending_control(g_state, ZR_VM_PENDING_CONTROL_RETURN,
                                  g_state->callInfoList, 17u, 3u, value);
}

static TZrUInt32 g_pending_drop_count;
static TZrBool g_pending_drop_saw_empty;
static TZrBool g_pending_drop_collect;
static TZrBool g_pending_drop_throw;

static TZrInt64 pending_reentrant_drop(SZrState *state) {
    ++g_pending_drop_count;
    g_pending_drop_saw_empty = (TZrBool)(
            state->pendingControl.kind == ZR_VM_PENDING_CONTROL_NONE &&
            !state->pendingControl.hasValue &&
            ZR_VALUE_IS_TYPE_NULL(state->pendingControl.value.type));
    execution_clear_pending_control(state);
    if (g_pending_drop_collect) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    }
    if (g_pending_drop_throw) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    }
    return 0;
}

static void pending_create_reentrant_shared(SZrTypeValue *shared) {
    SZrClosureNative *destructor = ZrCore_ClosureNative_New(g_state, 0);
    SZrObject *object;
    pending_create_shared(shared);
    object = (SZrObject *)shared->value.object;
    TEST_ASSERT_NOT_NULL(destructor);
    destructor->nativeFunction = pending_reentrant_drop;
    ZrCore_RawObject_MarkAsPermanent(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    ZrCore_ObjectPrototype_AddMeta(g_state, object->prototype, ZR_META_DESTRUCTOR,
            (SZrFunction *)ZR_CAST_RAW_OBJECT_AS_SUPER(destructor));
    g_pending_drop_count = 0u;
    g_pending_drop_saw_empty = ZR_FALSE;
    g_pending_drop_collect = ZR_FALSE;
    g_pending_drop_throw = ZR_FALSE;
}

static void test_pending_clear_detaches_before_reentrant_drop(void) {
    SZrTypeValue shared;
    pending_create_reentrant_shared(&shared);
    pending_set_return(&shared);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    execution_clear_pending_control(g_state);
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
    TEST_ASSERT_TRUE(g_pending_drop_saw_empty);
    TEST_ASSERT_EQUAL_INT(ZR_VM_PENDING_CONTROL_NONE, g_state->pendingControl.kind);
    execution_clear_pending_control(g_state);
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
}

static void test_pending_replacement_survives_reentrant_drop(void) {
    SZrTypeValue first;
    SZrTypeValue second;
    pending_create_reentrant_shared(&first);
    pending_create_shared(&second);
    pending_set_return(&first);
    ZrCore_Ownership_ReleaseValue(g_state, &first);
    pending_set_return(&second);
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
    TEST_ASSERT_TRUE(g_pending_drop_saw_empty);
    TEST_ASSERT_EQUAL_INT(ZR_VM_PENDING_CONTROL_RETURN, g_state->pendingControl.kind);
    TEST_ASSERT_TRUE(g_state->pendingControl.hasValue);
    TEST_ASSERT_EQUAL_UINT32(17u, g_state->pendingControl.targetInstructionOffset);
    TEST_ASSERT_EQUAL_UINT32(3u, g_state->pendingControl.valueSlot);
    TEST_ASSERT_EQUAL_PTR(second.ownershipControl,
                          g_state->pendingControl.value.ownershipControl);
    TEST_ASSERT_EQUAL_UINT32(2u, second.ownershipControl->strongRefCount);
    execution_clear_pending_control(g_state);
    ZrCore_Ownership_ReleaseValue(g_state, &second);
}

static void test_pending_exception_survives_drop_nested_catch(void) {
    SZrTypeValue shared;
    SZrTypeValue payload;
    SZrRawObject *originalException;
    SZrObject *object;
    SZrFunction *destructor = compile_source(
            "try { throw \"nested\"; } catch (error) {} return 0;\n");
    TEST_ASSERT_NOT_NULL(destructor);
    pending_create_shared(&shared);
    object = (SZrObject *)shared.value.object;
    ZrCore_ObjectPrototype_AddMeta(g_state, object->prototype, ZR_META_DESTRUCTOR,
                                  destructor);
    pending_set_return(&shared);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    ZrCore_Value_InitAsInt(g_state, &payload, 73);
    TEST_ASSERT_TRUE(ZrCore_Exception_NormalizeThrownValue(
            g_state, &payload, g_state->callInfoList, ZR_THREAD_STATUS_RUNTIME_ERROR));
    originalException = g_state->currentException.value.object;
    execution_set_pending_control(g_state, ZR_VM_PENDING_CONTROL_EXCEPTION,
                                  g_state->callInfoList, 0u, 0u, ZR_NULL);
    TEST_ASSERT_TRUE(g_state->hasCurrentException);
    TEST_ASSERT_EQUAL_PTR(originalException, g_state->currentException.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, g_state->currentExceptionStatus);
    TEST_ASSERT_EQUAL_INT(ZR_VM_PENDING_CONTROL_EXCEPTION, g_state->pendingControl.kind);
    ZrCore_Exception_ClearCurrent(g_state);
    execution_clear_pending_control(g_state);
}

static void test_pending_gc_result_survives_reentrant_collection(void) {
    SZrTypeValue shared;
    SZrTypeValue replacement;
    SZrString *string;
    pending_create_reentrant_shared(&shared);
    pending_set_return(&shared);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    string = ZrCore_String_CreateFromNative(g_state, "pending-gc-result");
    TEST_ASSERT_NOT_NULL(string);
    ZrCore_Value_InitAsRawObject(g_state, &replacement,
                               ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    g_pending_drop_collect = ZR_TRUE;
    pending_set_return(&replacement);
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, g_state->pendingControl.value.type);
    TEST_ASSERT_EQUAL_STRING("pending-gc-result", ZrCore_String_GetNativeString(
            (SZrString *)g_state->pendingControl.value.value.object));
    execution_clear_pending_control(g_state);
}

static void pending_replace_protected(SZrState *state, TZrPtr argument) {
    execution_set_pending_control(state, ZR_VM_PENDING_CONTROL_RETURN,
                                  state->callInfoList, 17u, 3u, argument);
}

static void test_pending_throwing_drop_finishes_release_and_discards_replacement(void) {
    SZrTypeValue shared;
    SZrTypeValue weak;
    SZrTypeValue replacement;
    SZrCallInfo *savedCallInfo = g_state->callInfoList;
    TZrMemoryOffset savedTop = ZrCore_Stack_SavePointerAsOffset(
            g_state, g_state->stackTop.valuePointer);
    TZrUInt32 savedYield = g_state->nestedNativeCallYieldFlag;
    TZrUInt32 savedRoots = g_state->aotGcRootFrameDepth;
    pending_create_reentrant_shared(&shared);
    pending_create_shared(&replacement);
    ZrCore_Value_ResetAsNull(&weak);
    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));
    pending_set_return(&shared);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    g_pending_drop_throw = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR,
            ZrCore_Exception_TryRun(g_state, pending_replace_protected, &replacement));
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
    TEST_ASSERT_EQUAL_UINT32(0u, weak.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(1u, weak.ownershipControl->weakRefCount);
    TEST_ASSERT_FALSE(weak.ownershipControl->dropInProgress);
    TEST_ASSERT_FALSE(weak.ownershipControl->objectIsAlive);
    TEST_ASSERT_EQUAL_UINT32(1u, replacement.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_PTR(savedCallInfo, g_state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_LoadOffsetToPointer(g_state, savedTop),
                          g_state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(savedYield, g_state->nestedNativeCallYieldFlag);
    TEST_ASSERT_EQUAL_UINT32(savedRoots, g_state->aotGcRootFrameDepth);
    TEST_ASSERT_FALSE(g_state->pendingControl.hasValue);
    ZrCore_Exception_ClearCurrent(g_state);
    g_state->threadStatus = ZR_THREAD_STATUS_FINE;
    ZrCore_Ownership_ReleaseValue(g_state, &replacement);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
}

static void test_shared_call_result_is_not_retained_after_explicit_drop(void) {
    pending_assert_script(
            "fn forward(owner: Shared<Tracker>): Shared<Tracker> { return owner; }\n"
            "fn run(): int {\n"
            " var unique = own Tracker(); var shared = share(unique);\n"
            " var returned = forward(shared); drop(shared); drop(returned);\n"
            " return Tracker.drops;\n"
            "}\nreturn run();\n", 1);
}

static void test_pending_shared_clear_releases_retention_once(void) {
    SZrTypeValue shared;
    pending_create_shared(&shared);
    pending_set_return(&shared);
    TEST_ASSERT_EQUAL_UINT32(2u, shared.ownershipControl->strongRefCount);

    execution_clear_pending_control(g_state);
    TEST_ASSERT_EQUAL_UINT32(1u, shared.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_INT(ZR_VM_PENDING_CONTROL_NONE, g_state->pendingControl.kind);
    TEST_ASSERT_FALSE(g_state->pendingControl.hasValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(g_state->pendingControl.value.type));
    execution_clear_pending_control(g_state);
    TEST_ASSERT_EQUAL_UINT32(1u, shared.ownershipControl->strongRefCount);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
}

static void test_pending_weak_clear_releases_retention_once(void) {
    SZrTypeValue shared;
    SZrTypeValue weak;
    pending_create_shared(&shared);
    ZrCore_Value_ResetAsNull(&weak);
    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));
    pending_set_return(&weak);
    TEST_ASSERT_EQUAL_UINT32(3u, weak.ownershipControl->weakRefCount);

    execution_clear_pending_control(g_state);
    TEST_ASSERT_EQUAL_UINT32(2u, weak.ownershipControl->weakRefCount);
    execution_clear_pending_control(g_state);
    TEST_ASSERT_EQUAL_UINT32(2u, weak.ownershipControl->weakRefCount);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    TEST_ASSERT_FALSE(weak.ownershipControl->objectIsAlive);
    TEST_ASSERT_EQUAL_UINT32(1u, weak.ownershipControl->weakRefCount);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
}

static void test_pending_valueless_transfer_releases_retention(void) {
    const EZrVmPendingControlKind kinds[] = {
        ZR_VM_PENDING_CONTROL_RETURN, ZR_VM_PENDING_CONTROL_BREAK,
        ZR_VM_PENDING_CONTROL_CONTINUE, ZR_VM_PENDING_CONTROL_EXCEPTION
    };
    SZrTypeValue shared;
    pending_create_shared(&shared);
    for (TZrSize i = 0u; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        pending_set_return(&shared);
        TEST_ASSERT_EQUAL_UINT32(2u, shared.ownershipControl->strongRefCount);
        execution_set_pending_control(g_state, kinds[i], g_state->callInfoList,
                                      19u, 0u, ZR_NULL);
        TEST_ASSERT_EQUAL_UINT32(1u, shared.ownershipControl->strongRefCount);
        TEST_ASSERT_EQUAL_INT(kinds[i], g_state->pendingControl.kind);
        TEST_ASSERT_FALSE(g_state->pendingControl.hasValue);
    }
    execution_clear_pending_control(g_state);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
}

static void test_pending_thread_reset_releases_retention(void) {
    SZrTypeValue shared;
    pending_create_shared(&shared);
    pending_set_return(&shared);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
                         ZrCore_State_ResetThread(g_state, ZR_THREAD_STATUS_FINE));
    TEST_ASSERT_EQUAL_UINT32(1u, shared.ownershipControl->strongRefCount);
    TEST_ASSERT_FALSE(g_state->pendingControl.hasValue);
    TEST_ASSERT_EQUAL_INT(ZR_VM_PENDING_CONTROL_NONE, g_state->pendingControl.kind);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
}

static TZrInt32 g_pending_reset_status;

static void pending_reset_protected(SZrState *state, TZrPtr argument) {
    ZR_UNUSED_PARAMETER(argument);
    g_pending_reset_status = ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
}

static void test_pending_thread_reset_reports_drop_failure_after_cleanup(void) {
    SZrTypeValue shared;
    SZrTypeValue weak;
    pending_create_reentrant_shared(&shared);
    ZrCore_Value_ResetAsNull(&weak);
    TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(g_state, &weak, &shared));
    pending_set_return(&shared);
    ZrCore_Ownership_ReleaseValue(g_state, &shared);
    g_pending_drop_throw = ZR_TRUE;
    g_pending_reset_status = ZR_THREAD_STATUS_FINE;
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE,
            ZrCore_Exception_TryRun(g_state, pending_reset_protected, ZR_NULL));
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, g_pending_reset_status);
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
    TEST_ASSERT_EQUAL_UINT32(1u, weak.ownershipControl->weakRefCount);
    TEST_ASSERT_FALSE(weak.ownershipControl->objectIsAlive);
    TEST_ASSERT_FALSE(g_state->pendingControl.hasValue);
    TEST_ASSERT_EQUAL_UINT32(0u, g_state->aotGcRootFrameDepth);
    ZrCore_Ownership_ReleaseValue(g_state, &weak);
}

static void test_pending_self_alias_and_replacement_preserve_value(void) {
    SZrTypeValue first;
    SZrTypeValue second;
    pending_create_shared(&first);
    pending_create_shared(&second);
    pending_set_return(&first);
    pending_set_return(&g_state->pendingControl.value);
    TEST_ASSERT_EQUAL_UINT32(2u, first.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_PTR(first.ownershipControl,
                          g_state->pendingControl.value.ownershipControl);

    pending_set_return(&second);
    TEST_ASSERT_EQUAL_UINT32(1u, first.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_UINT32(2u, second.ownershipControl->strongRefCount);
    TEST_ASSERT_EQUAL_PTR(second.ownershipControl,
                          g_state->pendingControl.value.ownershipControl);
    execution_clear_pending_control(g_state);
    TEST_ASSERT_EQUAL_UINT32(1u, second.ownershipControl->strongRefCount);
    ZrCore_Ownership_ReleaseValue(g_state, &first);
    ZrCore_Ownership_ReleaseValue(g_state, &second);
}

static void pending_assert_script(const TZrChar *body, TZrInt64 expected) {
    TZrChar source[8192];
    TZrInt64 result = 0;
    SZrFunction *function;
    int length = snprintf(source, sizeof(source),
            "resource class Tracker {\n"
            " pub static var drops: int = 0;\n"
            " pub static var trace: int = 0;\n"
            " pub @destructor() { Tracker.drops = Tracker.drops + 1; }\n"
            "}\n%s", body);
    TEST_ASSERT_TRUE(length > 0 && (TZrSize)length < sizeof(source));
    function = compile_source(source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
    ZrCore_Function_Free(g_state, function);
}

static void test_pending_shared_return_through_nested_finally(void) {
    pending_assert_script(
            "fn forward(owner: Shared<Tracker>): Shared<Tracker> {\n"
            " try { try { return owner; } finally {\n"
            "  Tracker.trace = Tracker.trace * 10 + 1;\n"
            " } } finally { Tracker.trace = Tracker.trace * 10 + 2; }\n"
            "}\n"
            "fn run(): int {\n"
            " var unique = own Tracker(); var shared = share(unique);\n"
            " var returned = forward(shared); drop(shared);\n"
            " var before = Tracker.drops; drop(returned);\n"
            " return before * 1000 + Tracker.trace * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 121);
}

static void test_pending_return_preserves_value_before_finally_assignment(void) {
    pending_assert_script(
            "fn forward(first: Shared<Tracker>, second: Shared<Tracker>): Shared<Tracker> {\n"
            " try { return first; } finally { first = second; }\n"
            "}\n"
            "fn run(): int {\n"
            " var ua = own Tracker(); var a = share(ua);\n"
            " var ub = own Tracker(); var b = share(ub);\n"
            " var returned = forward(a, b);\n"
            " var same = 0; if (returned == a) { same = 1; }\n"
            " drop(a); drop(b); drop(returned);\n"
            " return same * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 12);
}

static void test_pending_weak_return_through_nested_finally(void) {
    pending_assert_script(
            "fn forward(owner: Shared<Tracker>): Weak<Tracker> {\n"
            " var weak = degrade(owner);\n"
            " try { try { return weak; } finally {\n"
            "  Tracker.trace = Tracker.trace * 10 + 1;\n"
            " } } finally { Tracker.trace = Tracker.trace * 10 + 2; }\n"
            "}\n"
            "fn run(): int {\n"
            " var unique = own Tracker(); var shared = share(unique);\n"
            " var returned = forward(shared); drop(shared);\n"
            " var expired = wake(returned);\n"
            " if (expired != null) { return 999; }\n"
            " drop(returned); return Tracker.trace * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 121);
}

static void test_pending_return_replaced_by_finally_exception(void) {
    pending_assert_script(
            "fn abandoned(owner: Shared<Tracker>): Shared<Tracker> {\n"
            " try { return owner; } finally { throw \"replacement\"; }\n"
            "}\n"
            "fn run(): int {\n"
            " var unique = own Tracker(); var shared = share(unique);\n"
            " try { var returned = abandoned(shared); } catch (error) {\n"
            "  Tracker.trace = 7;\n"
            " }\n"
            " drop(shared); return Tracker.trace * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 71);
}

static void test_pending_loop_local_transfers_do_not_exit_enclosing_try(void) {
    pending_assert_script(
            "fn run(): int {\n"
            " var i = 0;\n"
            " try { while (i < 3) {\n"
            "  var unique = own Tracker(); var shared = share(unique);\n"
            "  i = i + 1; if (i < 2) { continue; } break;\n"
            " } Tracker.trace = Tracker.trace * 10 + 1;\n"
            " } finally { Tracker.trace = Tracker.trace * 10 + 2; }\n"
            " return Tracker.trace * 10 + Tracker.drops;\n"
            "}\nreturn run();\n", 122);
}

#endif
