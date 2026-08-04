#include "unity.h"
#include "runtime_support.h"

#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/test_contract.h"
#include "zr_vm_core/task_runtime.h"

#include <string.h>

static SZrState *create_test_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    if (state == ZR_NULL ||
        !ZrCore_TaskRuntime_RegisterBuiltins(state->global)) {
        ZrTests_Runtime_State_Destroy(state);
        return ZR_NULL;
    }
    return state;
}

static void destroy_test_state(SZrState *state) {
    ZrTests_Runtime_State_Destroy(state);
}

static SZrFunction *compile_source_named(
        SZrState *state,
        const TZrChar *source,
        const TZrChar *sourcePath,
        TZrBool testBuild) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            state, (TZrNativeString)sourcePath);

    TEST_ASSERT_NOT_NULL(sourceName);
    return testBuild
           ? ZrParser_Source_CompileTest(state, source, strlen(source), sourceName)
           : ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_source(SZrState *state, const TZrChar *source, TZrBool testBuild) {
    return compile_source_named(state, source, "test_role_binding.zr", testBuild);
}

static void test_testing_roles_have_test_phase_owner_and_stable_ids(void) {
    const SZrParserAttributeSchema *testRole =
            ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_TEST);
    const SZrParserAttributeSchema *caseRole =
            ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE);
    const SZrParserAttributeSchema *skipRole =
            ZrParser_AttributeContract_FindBuiltinByRole(ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP);

    TEST_ASSERT_NOT_NULL(testRole);
    TEST_ASSERT_NOT_NULL(caseRole);
    TEST_ASSERT_NOT_NULL(skipRole);
    TEST_ASSERT_EQUAL_STRING("zr.testing", testRole->ownerModule);
    TEST_ASSERT_EQUAL_STRING("zr.testing", caseRole->ownerModule);
    TEST_ASSERT_EQUAL_STRING("zr.testing", skipRole->ownerModule);
    TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_TEST, testRole->providerPhase);
    TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_TEST, caseRole->providerPhase);
    TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_TEST, skipRole->providerPhase);
    TEST_ASSERT_EQUAL_UINT32(
            ZrParser_AttributeContract_ComputeId(testRole->qualifiedName),
            testRole->attributeId);
    TEST_ASSERT_EQUAL_UINT32(
            ZrParser_AttributeContract_ComputeId(caseRole->qualifiedName),
            caseRole->attributeId);
    TEST_ASSERT_EQUAL_UINT32(
            ZrParser_AttributeContract_ComputeId(skipRole->qualifiedName),
            skipRole->attributeId);
}

static void test_test_build_emits_manifest_for_ordinary_function(void) {
    static const TZrChar *source =
            "#zr.testing.test#\n"
            "fn parsesEmptyInput(): void { }\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrParserTestManifest manifest;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, function->testManifestDataLength);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Decode(
            state, function->testManifestData, function->testManifestDataLength, &manifest));
    TEST_ASSERT_EQUAL_UINT32(ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION, manifest.schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(1U, manifest.entryCount);
    TEST_ASSERT_NOT_NULL(strstr(manifest.entries[0].qualifiedName, "parsesEmptyInput"));
    TEST_ASSERT_NOT_EQUAL(
            ZrParser_AttributeContract_ComputeId("parsesEmptyInput"),
            manifest.entries[0].functionSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, manifest.entries[0].functionSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, manifest.entries[0].functionTypeId);
    TEST_ASSERT_EQUAL_UINT64(function->moduleSignatureHash, manifest.moduleGraphHash);
    TEST_ASSERT_FALSE(manifest.entries[0].isAsync);
    TEST_ASSERT_EQUAL_UINT32(0U, manifest.entries[0].caseCount);
    TEST_ASSERT_NULL(manifest.entries[0].skipReason);
    TEST_ASSERT_LESS_THAN_UINT32(function->childFunctionLength,
                                 manifest.entries[0].callableChildIndex);

    ZrParser_TestManifest_Free(state, &manifest);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_production_build_typechecks_then_trims_test_roots(void) {
    static const TZrChar *source =
            "#zr.testing.test#\n"
            "fn productionHidden(): void { let checked: int = 1; }\n";
    SZrState *state = create_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, ZR_FALSE);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0U, function->testManifestDataLength);
    TEST_ASSERT_EQUAL_UINT32(0U, function->childFunctionLength);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_parameter_cases_and_skip_are_bound_as_constants(void) {
    static const TZrChar *source =
            "#zr.testing.test#\n"
            "#zr.testing.case(1, 2)#\n"
            "#zr.testing.case(-1, 1)#\n"
            "#zr.testing.skip(reason: \"tracked externally\")#\n"
            "fn adds(lhs: int, rhs: int): void { }\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrParserTestManifest manifest;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Decode(
            state, function->testManifestData, function->testManifestDataLength, &manifest));
    TEST_ASSERT_EQUAL_UINT32(1U, manifest.entryCount);
    TEST_ASSERT_EQUAL_UINT32(2U, manifest.entries[0].caseCount);
    TEST_ASSERT_EQUAL_UINT32(2U, manifest.entries[0].cases[0].argumentCount);
    TEST_ASSERT_EQUAL(ZR_PARSER_TEST_CONSTANT_INT,
                      manifest.entries[0].cases[0].arguments[0].kind);
    TEST_ASSERT_EQUAL_INT64(1, manifest.entries[0].cases[0].arguments[0].value.intValue);
    TEST_ASSERT_EQUAL_INT64(-1, manifest.entries[0].cases[1].arguments[0].value.intValue);
    TEST_ASSERT_EQUAL_STRING("tracked externally", manifest.entries[0].skipReason);

    ZrParser_TestManifest_Free(state, &manifest);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_async_test_uses_explicit_task_void_contract(void) {
    static const TZrChar *source =
            "#zr.testing.test#\n"
            "async fn loadsFixture(): zr.task.Task<void> { }\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrParserTestManifest manifest;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, ZR_TRUE);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Decode(
            state, function->testManifestData, function->testManifestDataLength, &manifest));
    TEST_ASSERT_EQUAL_UINT32(1U, manifest.entryCount);
    TEST_ASSERT_TRUE(manifest.entries[0].isAsync);
    TEST_ASSERT_NOT_NULL(strstr(manifest.entries[0].qualifiedName, "loadsFixture"));

    ZrParser_TestManifest_Free(state, &manifest);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_overloaded_tests_keep_canonical_symbol_and_type_identity(void) {
    static const TZrChar *source =
            "#zr.testing.test#\n"
            "fn overloaded(): void { }\n"
            "#zr.testing.test#\n"
            "#zr.testing.case(1)#\n"
            "fn overloaded(value: int): void { }\n";
    SZrState *state = create_test_state();
    SZrFunction *function;
    SZrParserTestManifest manifest;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source_named(state, source, "canonical/test_overload.zr", ZR_TRUE);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Decode(
            state, function->testManifestData, function->testManifestDataLength, &manifest));
    TEST_ASSERT_EQUAL_UINT32(2U, manifest.entryCount);
    TEST_ASSERT_NOT_EQUAL_UINT32(
            manifest.entries[0].functionSymbolId,
            manifest.entries[1].functionSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(
            manifest.entries[0].functionTypeId,
            manifest.entries[1].functionTypeId);
    TEST_ASSERT_NOT_NULL(strstr(manifest.entries[0].qualifiedName, manifest.entries[0].moduleId));
    TEST_ASSERT_NOT_NULL(strstr(manifest.entries[1].qualifiedName, manifest.entries[1].moduleId));

    ZrParser_TestManifest_Free(state, &manifest);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void assert_test_compile_rejected(const TZrChar *source) {
    SZrState *state = create_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, ZR_TRUE);
    TEST_ASSERT_NULL_MESSAGE(function, source);
    destroy_test_state(state);
}

static void assert_production_compile_rejected(const TZrChar *source) {
    SZrState *state = create_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, ZR_FALSE);
    TEST_ASSERT_NULL_MESSAGE(function, source);
    destroy_test_state(state);
}

static void test_invalid_test_signatures_and_role_combinations_are_rejected(void) {
    assert_test_compile_rejected(
            "#zr.testing.test# fn missingCase(value: int): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# fn wrongReturn(): int { return 1; }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# fn generic<T>(): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# #zr.testing.skip(reason: \"\")# fn emptySkip(): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.case(1)# fn caseWithoutTest(value: int): void { }\n");
    assert_test_compile_rejected(
            "let runtimeValue = 1; #zr.testing.test# #zr.testing.case(runtimeValue)# "
            "fn dynamicCase(value: int): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# #zr.testing.test# fn duplicate(): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# #zr.testing.case(1, 2)# fn wrongArity(value: int): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# #zr.testing.case(\"wrong\")# fn wrongType(value: int): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# fn byReference(value: ref int): void { }\n");
    assert_test_compile_rejected(
            "#zr.testing.test# async fn implicitAsyncVoid(): void { }\n");
    assert_test_compile_rejected(
            "fn outer(): void { #zr.testing.test# fn nested(): void { } }\n");
    assert_test_compile_rejected(
            "class Suite { #zr.testing.test# fn member(): void { } }\n");
    assert_test_compile_rejected(
            "struct Suite { #zr.testing.case(1)# fn member(): void { } }\n");
}

static void test_production_still_rejects_invalid_test_body_and_dangling_reference(void) {
    assert_production_compile_rejected(
            "#zr.testing.test# fn invalidBody(): void { let value: int = \"wrong\"; }\n");
    assert_production_compile_rejected(
            "#zr.testing.test# fn hidden(): void { }\n"
            "hidden();\n");
    assert_production_compile_rejected(
            "#zr.testing.test# fn hidden(): void { }\n"
            "fn production(): void { hidden(); }\n");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_testing_roles_have_test_phase_owner_and_stable_ids);
    RUN_TEST(test_test_build_emits_manifest_for_ordinary_function);
    RUN_TEST(test_production_build_typechecks_then_trims_test_roots);
    RUN_TEST(test_parameter_cases_and_skip_are_bound_as_constants);
    RUN_TEST(test_async_test_uses_explicit_task_void_contract);
    RUN_TEST(test_overloaded_tests_keep_canonical_symbol_and_type_identity);
    RUN_TEST(test_invalid_test_signatures_and_role_combinations_are_rejected);
    RUN_TEST(test_production_still_rejects_invalid_test_body_and_dangling_reference);
    return UNITY_END();
}
