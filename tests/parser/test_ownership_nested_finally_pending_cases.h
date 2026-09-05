#ifndef ZR_TEST_OWNERSHIP_NESTED_FINALLY_PENDING_CASES_H
#define ZR_TEST_OWNERSHIP_NESTED_FINALLY_PENDING_CASES_H

static void test_suspended_return_survives_inner_finally_break(void) {
    pending_assert_script(
            "fn run(): int { try { return 7; } finally {\n"
            " while (true) { try { break; } finally { Tracker.trace = 1; } }\n"
            " Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } return 9; }\n"
            "var result = run(); return result * 100 + Tracker.trace;\n", 712);
}

static void test_suspended_return_survives_inner_finally_continue(void) {
    pending_assert_script(
            "fn run(): int { try { return 7; } finally {\n"
            " var i = 0; while (i < 2) { try { i = i + 1; continue; } finally {\n"
            "  Tracker.trace = Tracker.trace * 10 + 1;\n"
            " } } Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } return 9; }\n"
            "var result = run(); return result * 1000 + Tracker.trace;\n", 7112);
}

static void test_suspended_return_runs_remaining_outer_finally(void) {
    pending_assert_script(
            "fn run(): int { try { return 7; } finally {\n"
            " try { Tracker.trace = 1; } finally { Tracker.trace = Tracker.trace * 10 + 2; }\n"
            " Tracker.trace = Tracker.trace * 10 + 3;\n"
            " } return 9; }\n"
            "var result = run(); return result * 1000 + Tracker.trace;\n", 7123);
}

static void test_suspended_return_survives_caught_inner_exception(void) {
    pending_assert_script(
            "fn run(): int { try { return 7; } finally {\n"
            " try { throw \"inner\"; } catch (error) { Tracker.trace = 1; }\n"
            " Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } return 9; }\n"
            "var result = run(); return result * 100 + Tracker.trace;\n", 712);
}

static void test_suspended_exception_survives_caught_inner_exception(void) {
    pending_assert_script(
            "fn run(): int { try { try { throw \"outer\"; } finally {\n"
            " try { throw \"inner\"; } catch (error) { Tracker.trace = 1; }\n"
            " Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } } catch (error) { if (error.exception != \"outer\") { return 98; }\n"
            " return Tracker.trace; } return 99; }\n"
            "return run();\n", 12);
}

static void test_suspended_shared_return_survives_inner_finally_break(void) {
    pending_assert_script(
            "fn forward(first: Shared<Tracker>, second: Shared<Tracker>): Shared<Tracker> {\n"
            " try { return first; } finally {\n"
            "  while (true) { try { break; } finally { Tracker.trace = 1; } }\n"
            "  Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } return second; }\n"
            "fn run(): int {\n"
            " var ua = own Tracker(); var a = share(ua);\n"
            " var ub = own Tracker(); var b = share(ub);\n"
            " var returned = forward(a, b);\n"
            " var same = 0; if (returned == a) { same = 1; }\n"
            " drop(a); drop(b);\n"
            " var before = Tracker.drops; drop(returned);\n"
            " return same * 10000 + before * 1000 + Tracker.trace * 10 + Tracker.drops;\n"
            "} return run();\n", 11122);
}

static void test_suspended_shared_return_override_releases_original(void) {
    pending_assert_script(
            "fn forward(first: Shared<Tracker>, second: Shared<Tracker>): Shared<Tracker> {\n"
            " try { return first; } finally {\n"
            "  try { return second; } finally { Tracker.trace = 1; }\n"
            " } }\n"
            "fn run(): int {\n"
            " var ua = own Tracker(); var a = share(ua);\n"
            " var ub = own Tracker(); var b = share(ub);\n"
            " var returned = forward(a, b);\n"
            " var same = 0; if (returned == b) { same = 1; }\n"
            " drop(a); drop(b); var before = Tracker.drops; drop(returned);\n"
            " return same * 100 + before * 10 + Tracker.drops;\n"
            "} return run();\n", 112);
}

static void test_pending_break_discards_exited_catch_handler(void) {
    pending_assert_script(
            "fn run(): int { var trace = 0; try { while (true) {\n"
            " try { try { break; } finally { trace = trace + 1; }\n"
            " } catch (inner) { return 100; }\n"
            " } throw 1; } catch (outer) { trace = trace + 10;\n"
            " } finally { trace = trace + 1000; } return trace; }\n"
            "return run();\n", 1011);
}

static void test_suspended_weak_return_survives_inner_finally_break(void) {
    pending_assert_script(
            "fn forward(first: Weak<Tracker>, second: Weak<Tracker>): Weak<Tracker> {\n"
            " try { return first; } finally {\n"
            "  while (true) { try { break; } finally { Tracker.trace = 1; } }\n"
            " } return second; }\n"
            "fn run(): int {\n"
            " var ua = own Tracker(); var a = share(ua); var wa = degrade(a);\n"
            " var ub = own Tracker(); var b = share(ub); var wb = degrade(b);\n"
            " var returned = forward(wa, wb);\n"
            " var same = 0; if (returned == wa) { same = 1; }\n"
            " drop(a); drop(b);\n"
            " if (wake(returned) != null) { return 99; }\n"
            " drop(returned); drop(wa); drop(wb);\n"
            " return same * 10 + Tracker.drops;\n"
            "} return run();\n", 12);
}

static void test_suspended_shared_return_throw_override_releases_original(void) {
    pending_assert_script(
            "fn forward(owner: Shared<Tracker>): Shared<Tracker> {\n"
            " try { return owner; } finally {\n"
            "  try { throw \"replacement\"; } finally { Tracker.trace = 1; }\n"
            " } }\n"
            "fn run(): int {\n"
            " var seed = own Tracker(); var owner = share(seed);\n"
            " try { var returned = forward(owner); } catch (error) {\n"
            "  if (error.exception != \"replacement\") { return 99; }\n"
            "  Tracker.trace = Tracker.trace * 10 + 2;\n"
            " } drop(owner); return Tracker.trace * 10 + Tracker.drops;\n"
            "} return run();\n", 121);
}

static void test_suspended_handler_cleanup_preserves_original_host_failure(void) {
    SZrTypeValue owner;
    SZrTypeValue payload;
    SZrRawObject *originalException;
    SZrVmExceptionHandlerState *handler;
    pending_create_reentrant_shared(&owner);
    TEST_ASSERT_TRUE(execution_push_exception_handler(g_state, g_state->callInfoList, 0u));
    pending_set_return(&owner);
    handler = &g_state->exceptionHandlerStack[g_state->exceptionHandlerStackLength - 1u];
    execution_enter_finally(g_state, handler);
    ZrCore_Ownership_ReleaseValue(g_state, &owner);
    ZrCore_Value_InitAsInt(g_state, &payload, 73);
    TEST_ASSERT_TRUE(ZrCore_Exception_NormalizeThrownValue(
            g_state, &payload, g_state->callInfoList, ZR_THREAD_STATUS_EXCEPTION_ERROR));
    originalException = g_state->currentException.value.object;
    g_state->threadStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    g_pending_drop_throw = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR,
            execution_discard_exception_handlers_to_depth(g_state, 0u));
    TEST_ASSERT_EQUAL_PTR(originalException, g_state->currentException.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_EXCEPTION_ERROR, g_state->currentExceptionStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_EXCEPTION_ERROR, g_state->threadStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
    TEST_ASSERT_EQUAL_UINT32(0u, g_state->exceptionHandlerStackLength);
    g_pending_drop_throw = ZR_FALSE;
    ZrCore_Exception_ClearCurrent(g_state);
    g_state->threadStatus = ZR_THREAD_STATUS_FINE;
}

static void suspended_test_unwind(SZrState *state, TZrPtr argument) {
    ZrAotGeneratedFrame *frame = argument;
    TEST_ASSERT_TRUE(execution_unwind_exception_to_handler(state, &frame->callInfo));
}

static void test_suspended_unwind_keeps_original_exception_when_drop_throws(void) {
    ZrAotGeneratedFrame frame;
    SZrTypeValue owner, payload;
    SZrRawObject *originalException;
    TZrUInt32 outerIndex = UINT32_MAX, innerIndex = UINT32_MAX;
    TZrUInt32 rootDepth = g_state->aotGcRootFrameDepth;
    EZrThreadStatus status;
    aot_pending_prepare_source_frame(&frame,
            "try { try { return 7; } finally {} } catch (error) { return 1; }\n");
    for (TZrUInt32 index = 0u; index < frame.function->exceptionHandlerCount; ++index) {
        if (frame.function->exceptionHandlerList[index].hasFinally) { innerIndex = index; }
        if (frame.function->exceptionHandlerList[index].catchClauseCount > 0u) { outerIndex = index; }
    }
    TEST_ASSERT_NOT_EQUAL_UINT32(UINT32_MAX, outerIndex);
    TEST_ASSERT_NOT_EQUAL_UINT32(UINT32_MAX, innerIndex);
    pending_create_reentrant_shared(&owner);
    TEST_ASSERT_TRUE(execution_push_exception_handler(g_state, frame.callInfo, outerIndex));
    TEST_ASSERT_TRUE(execution_push_exception_handler(g_state, frame.callInfo, innerIndex));
    pending_set_return(&owner);
    execution_enter_finally(g_state, execution_find_handler_state(g_state, frame.callInfo, innerIndex));
    ZrCore_Ownership_ReleaseValue(g_state, &owner);
    ZrCore_Value_InitAsInt(g_state, &payload, 73);
    TEST_ASSERT_TRUE(ZrCore_Exception_NormalizeThrownValue(
            g_state, &payload, frame.callInfo, ZR_THREAD_STATUS_EXCEPTION_ERROR));
    originalException = g_state->currentException.value.object;
    g_state->threadStatus = ZR_THREAD_STATUS_EXCEPTION_ERROR;
    g_pending_drop_throw = ZR_TRUE;
    status = ZrCore_Exception_TryRun(g_state, suspended_test_unwind, &frame);
    g_pending_drop_throw = ZR_FALSE;
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, status);
    TEST_ASSERT_EQUAL_PTR(originalException, g_state->currentException.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_EXCEPTION_ERROR, g_state->currentExceptionStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, g_pending_drop_count);
    TEST_ASSERT_EQUAL_UINT32(rootDepth, g_state->aotGcRootFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, g_state->exceptionHandlerStackLength);
    TEST_ASSERT_EQUAL_INT(ZR_VM_EXCEPTION_HANDLER_PHASE_CATCH, g_state->exceptionHandlerStack[0].phase);
    TEST_ASSERT_EQUAL_PTR(frame.function->instructionsList + frame.function->catchClauseList[
            frame.function->exceptionHandlerList[outerIndex].catchClauseStartIndex].targetInstructionOffset,
            frame.callInfo->context.context.programCounter);
    ZrCore_Exception_ClearCurrent(g_state);
    g_state->threadStatus = ZR_THREAD_STATUS_FINE;
}

#endif
