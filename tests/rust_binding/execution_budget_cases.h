#ifndef ZR_RUST_BINDING_EXECUTION_BUDGET_CASES_H
#define ZR_RUST_BINDING_EXECUTION_BUDGET_CASES_H

#include "../../zr_vm_rust_binding/src/zr_vm_rust_binding/native_call_context_internal.h"
#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"

typedef struct ExecutionBudgetFixture {
    ZrRustBindingProjectWorkspace *workspace;
    ZrRustBindingRuntime *runtime;
    ZrRustBindingProjectSession *session;
    ZrRustBindingNativeModule *module;
    ZrRustBindingRuntimeNativeModuleRegistration *registration;
    ZrRustBindingCancellationToken *token;
    TZrUInt64 nativeDeadline;
    TZrSize nativeCalls;
    TZrBool cancelInNative;
    TZrSize transientBytes;
    TZrUInt64 transientBase;
    TZrUInt64 transientPeak;
    TZrUInt32 collectionsInNative;
} ExecutionBudgetFixture;

static ZrRustBindingStatus execution_budget_native_boundary(ZrRustBindingNativeCallContext *context,
                                                            TZrPtr userData,
                                                            ZrRustBindingValue **outResult) {
    ExecutionBudgetFixture *fixture = (ExecutionBudgetFixture *)userData;
    fixture->nativeCalls++;
    if (fixture->transientBytes > 0) {
        SZrGlobalState *global = context->context->state->global;
        TZrPtr transient;
        fixture->transientBase = global->allocatedBytes;
        transient = ZrCore_Memory_RawMalloc(global, fixture->transientBytes);
        if (transient == ZR_NULL) {
            return ZR_RUST_BINDING_STATUS_INTERNAL_ERROR;
        }
        fixture->transientPeak = global->allocatedBytes;
        ZrCore_Memory_RawFree(global, transient, fixture->transientBytes);
    }
    for (TZrUInt32 index = 0; index < fixture->collectionsInNative; index++) {
        ZrCore_GarbageCollector_GcFull(context->context->state, ZR_FALSE);
    }
    if (fixture->cancelInNative) {
        ZrRustBinding_CancellationToken_Cancel(fixture->token);
    }
    while (fixture->nativeDeadline != 0 &&
           ZrRustBinding_ExecutionNowMicros() <= fixture->nativeDeadline) {
    }
    return ZrRustBinding_Value_NewInt(7, outResult);
}

static void execution_budget_fixture_start(ExecutionBudgetFixture *fixture, const TZrChar *name) {
    static const TZrChar *source =
            "let host = import(\"budget_host\");\n"
            "var caught: int = 0;\n"
            "pub ping(): int { return 123; }\n"
            "pub spin(): int { while (true) { } return 0; }\n"
            "pub protectedSpin(): int {\n"
            "  try { spin(); } catch (error) { caught = caught + 1; }\n"
            "  finally { caught = caught + 10; }\n"
            "  return 0;\n"
            "}\n"
            "pub caughtCount(): int { return caught; }\n"
            "pub nativeTwice(): int { host.boundary(); return host.boundary(); }\n"
            "return 0;\n";
    TZrChar workspaceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    ZrRustBindingScaffoldOptions scaffold = {0};
    ZrRustBindingRuntimeOptions runtimeOptions = {0};
    ZrRustBindingRunOptions runOptions = {0};
    ZrRustBindingNativeFunctionDescriptor function = {0};
    ZrRustBindingNativeModuleBuilder *builder = ZR_NULL;

    memset(fixture, 0, sizeof(*fixture));
    build_workspace_root(name, workspaceRoot, sizeof(workspaceRoot));
    clean_directory_tree(workspaceRoot);
    snprintf(mainPath, sizeof(mainPath), "%s/src/main.zr", workspaceRoot);
    scaffold.rootPath = workspaceRoot;
    scaffold.projectName = name;
    scaffold.overwriteExisting = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Project_Scaffold(&scaffold, &fixture->workspace));
    TEST_ASSERT_TRUE(write_text_file(mainPath, source));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_NewStandard(&runtimeOptions, &fixture->runtime));
    fixture->token = ZrRustBinding_CancellationToken_New();
    TEST_ASSERT_NOT_NULL(fixture->token);
    function.name = "boundary";
    function.callback = execution_budget_native_boundary;
    function.userData = fixture;
    function.returnTypeName = "int";
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_New("budget_host", &builder));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_AddFunction(builder, &function));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_NativeModuleBuilder_Build(builder, &fixture->module));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_NativeModuleBuilder_Free(builder));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_Runtime_RegisterNativeModule(fixture->runtime, fixture->module,
                                                                    &fixture->registration));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_Start(fixture->runtime, fixture->workspace,
                                                            &runOptions, &fixture->session));
}

static void execution_budget_fixture_free(ExecutionBudgetFixture *fixture) {
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectSession_Free(fixture->session));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_RuntimeNativeModuleRegistration_Free(fixture->registration));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_NativeModule_Free(fixture->module));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Runtime_Free(fixture->runtime));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_ProjectWorkspace_Free(fixture->workspace));
    ZrRustBinding_CancellationToken_Free(fixture->token);
}

static void execution_budget_assert_unbounded_int(ExecutionBudgetFixture *fixture,
                                                 const TZrChar *exportName,
                                                 TZrInt64 expected) {
    ZrRustBindingValue *result = ZR_NULL;
    TZrInt64 actual = 0;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExport(fixture->session, "main", exportName,
                                                                       ZR_NULL, 0, &result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_ReadInt(result, &actual));
    TEST_ASSERT_EQUAL_INT64(expected, actual);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
}

static void test_rust_binding_execution_budget_instruction_boundary_and_recovery(void) {
    ExecutionBudgetFixture fixture;
    ZrRustBindingCallBudget budget = {0};
    ZrRustBindingCallUsage usage;
    ZrRustBindingValue *result = ZR_NULL;
    TZrUInt64 exactCount;
    TZrUInt64 limit;

    execution_budget_fixture_start(&fixture, "execution_budget_instruction");
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "ping", ZR_NULL, 0, &budget, &usage, &result));
    exactCount = usage.executedInstructions;
    TEST_ASSERT_TRUE(exactCount > 0);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_NONE, usage.termination);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    budget.hasInstructionLimit = ZR_TRUE;
    budget.maxInstructions = exactCount;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "ping", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_UINT64(exactCount, usage.executedInstructions);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));

    for (limit = 0; limit < exactCount; limit++) {
        budget.maxInstructions = limit;
        TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                              ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                      fixture.session, "main", "ping", ZR_NULL, 0, &budget, &usage, &result));
        TEST_ASSERT_NULL(result);
        TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_INSTRUCTION_LIMIT, usage.termination);
        TEST_ASSERT_EQUAL_UINT64(limit, usage.executedInstructions);
        execution_budget_assert_unbounded_int(&fixture, "ping", 123);
    }
    budget.maxInstructions = 257;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "protectedSpin", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_UINT64(257, usage.executedInstructions);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_INSTRUCTION_LIMIT, usage.termination);
    execution_budget_assert_unbounded_int(&fixture, "caughtCount", 0);
    execution_budget_assert_unbounded_int(&fixture, "ping", 123);
    execution_budget_fixture_free(&fixture);
}

static void test_rust_binding_execution_budget_deadline_and_native_cancel(void) {
    ExecutionBudgetFixture fixture;
    ZrRustBindingCallBudget budget = {0};
    ZrRustBindingCallUsage usage;
    ZrRustBindingValue *result = ZR_NULL;

    execution_budget_fixture_start(&fixture, "execution_budget_native");
    budget.hasInstructionLimit = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "budget_host", "boundary", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_UINT64(0, usage.executedInstructions);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.nativeCalls);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK, ZrRustBinding_Value_Free(result));
    fixture.nativeCalls = 0;
    budget.hasInstructionLimit = ZR_FALSE;
    budget.hasDeadline = ZR_TRUE;
    budget.deadlineMicros = ZrRustBinding_ExecutionNowMicros();
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_DEADLINE, usage.termination);
    TEST_ASSERT_EQUAL_UINT64(0, usage.executedInstructions);
    TEST_ASSERT_EQUAL_UINT32(0, fixture.nativeCalls);
    budget.deadlineMicros = ZrRustBinding_ExecutionNowMicros() + 20000;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "spin", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_DEADLINE, usage.termination);
    TEST_ASSERT_TRUE(usage.executedInstructions > 0);
    execution_budget_assert_unbounded_int(&fixture, "ping", 123);

    budget.deadlineMicros = ZrRustBinding_ExecutionNowMicros() + 100000;
    fixture.nativeDeadline = budget.deadlineMicros;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_DEADLINE, usage.termination);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.nativeCalls);
    fixture.nativeDeadline = 0;
    fixture.nativeCalls = 0;
    budget.hasDeadline = ZR_FALSE;
    budget.cancelToken = fixture.token;
    fixture.cancelInNative = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrRustBinding_CancellationToken_IsCancelled(fixture.token));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_CANCELLED, usage.termination);
    TEST_ASSERT_TRUE(ZrRustBinding_CancellationToken_IsCancelled(fixture.token));
    TEST_ASSERT_EQUAL_UINT32(1, fixture.nativeCalls);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_UINT64(0, usage.executedInstructions);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.nativeCalls);
    execution_budget_assert_unbounded_int(&fixture, "ping", 123);
    fixture.cancelInNative = ZR_FALSE;
    execution_budget_assert_unbounded_int(&fixture, "nativeTwice", 7);
    TEST_ASSERT_EQUAL_UINT32(3, fixture.nativeCalls);

    memset(&budget, 0, sizeof(budget));
    fixture.nativeCalls = 0;
    budget.hasNativeCallLimit = ZR_TRUE;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "budget_host", "boundary", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_NATIVE_CALL_LIMIT, usage.termination);
    TEST_ASSERT_EQUAL_UINT64(0, usage.nativeCalls);
    TEST_ASSERT_EQUAL_UINT32(0, fixture.nativeCalls);
    execution_budget_assert_unbounded_int(&fixture, "ping", 123);
    budget.maxNativeCalls = 2;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_UINT64(2, usage.nativeCalls);
    TEST_ASSERT_EQUAL_UINT32(2, fixture.nativeCalls);
    ZrRustBinding_Value_Free(result);
    fixture.nativeCalls = 0;
    budget.maxNativeCalls = 1;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_NATIVE_CALL_LIMIT, usage.termination);
    TEST_ASSERT_EQUAL_UINT64(1, usage.nativeCalls);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.nativeCalls);
    execution_budget_assert_unbounded_int(&fixture, "ping", 123);

    memset(&budget, 0, sizeof(budget));
    fixture.transientBytes = 65536;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "budget_host", "boundary", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_UINT64(fixture.transientBase + fixture.transientBytes, fixture.transientPeak);
    TEST_ASSERT_TRUE(usage.peakHeapBytes >= fixture.transientPeak);
    budget.maxHeapBytes = fixture.transientPeak;
    budget.hasHeapLimit = ZR_TRUE;
    ZrRustBinding_Value_Free(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "budget_host", "boundary", ZR_NULL, 0, &budget, &usage, &result));
    ZrRustBinding_Value_Free(result);
    budget.maxHeapBytes = fixture.transientPeak - 1;
    fixture.nativeCalls = 0;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_HEAP_LIMIT, usage.termination);
    TEST_ASSERT_TRUE(usage.peakHeapBytes > budget.maxHeapBytes);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.nativeCalls);
    fixture.transientBytes = 0;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "ping", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_TRUE(usage.peakHeapBytes <= budget.maxHeapBytes);
    ZrRustBinding_Value_Free(result);
    execution_budget_assert_unbounded_int(&fixture, "ping", 123);

    memset(&budget, 0, sizeof(budget));
    fixture.collectionsInNative = 2;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_OK,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "budget_host", "boundary", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_TRUE(usage.gcMicros > 0);
    TEST_ASSERT_TRUE(usage.gcMicros <= usage.elapsedMicros);
    ZrRustBinding_Value_Free(result);
    budget.hasGcTimeLimit = ZR_TRUE;
    fixture.nativeCalls = 0;
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                          ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
                                  fixture.session, "main", "nativeTwice", ZR_NULL, 0, &budget, &usage, &result));
    TEST_ASSERT_EQUAL_INT(ZR_RUST_BINDING_TERMINATION_GC_TIME_LIMIT, usage.termination);
    TEST_ASSERT_TRUE(usage.gcMicros > 0);
    TEST_ASSERT_EQUAL_UINT32(1, fixture.nativeCalls);
    fixture.collectionsInNative = 0;
    execution_budget_assert_unbounded_int(&fixture, "ping", 123);
    execution_budget_fixture_free(&fixture);
}

#endif
