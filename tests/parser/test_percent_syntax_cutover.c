#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/parser.h"

typedef struct SZrCutoverDiagnosticCapture {
    TZrUInt32 errorCount;
    TZrUInt32 removedSyntaxCount;
} SZrCutoverDiagnosticCapture;

static SZrState *g_state;

static TZrBool function_tree_contains_opcode(const SZrFunction *function,
                                             EZrInstructionCode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < function->instructionsLength; index++) {
        if (function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }
    for (index = 0U; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        if (constant->type == ZR_VALUE_TYPE_FUNCTION &&
            constant->value.object != ZR_NULL && !constant->isNative &&
            function_tree_contains_opcode(
                    ZR_CAST_FUNCTION(g_state, constant->value.object), opcode)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
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

static void capture_cutover_diagnostic(TZrPtr userData,
                                       const SZrStructuredDiagnostic *diagnostic,
                                       EZrToken token) {
    SZrCutoverDiagnosticCapture *capture = (SZrCutoverDiagnosticCapture *)userData;
    const TZrChar *code;

    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    if (diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_ERROR) {
        capture->errorCount++;
    }
    code = diagnostic->code != ZR_NULL
                   ? ZrCore_String_GetNativeString(diagnostic->code)
                   : ZR_NULL;
    if (code != ZR_NULL && strcmp(code, "legacy_syntax_removed") == 0) {
        capture->removedSyntaxCount++;
    }
}

static void capture_cutover_parser_error(TZrPtr userData,
                                         const SZrFileRange *location,
                                         const TZrChar *message,
                                         EZrToken token) {
    SZrCutoverDiagnosticCapture *capture = (SZrCutoverDiagnosticCapture *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(message);
    ZR_UNUSED_PARAMETER(token);
    if (capture != ZR_NULL) {
        capture->errorCount++;
    }
}

static void assert_legacy_source_is_rejected(const TZrChar *source) {
    SZrString *sourceName;
    SZrParserState parserState;
    SZrCutoverDiagnosticCapture capture = {0};
    SZrAstNode *ast;

    sourceName = ZrCore_String_Create(g_state,
                                      "percent_syntax_cutover_legacy.zr",
                                      strlen("percent_syntax_cutover_legacy.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_cutover_parser_error;
    parserState.structuredErrorCallback = capture_cutover_diagnostic;
    parserState.errorUserData = &capture;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0U, capture.errorCount, source);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0U, capture.removedSyntaxCount, source);
    TEST_ASSERT_NULL_MESSAGE(ast, source);
    ZrParser_State_Free(&parserState);
}

static void test_known_percent_directives_are_diagnostics_only(void) {
    static const TZrChar *const sources[] = {
            "%module legacy.module;",
            "let legacy = %import(\"legacy.module\");",
            "%extern(\"legacy\") { fn call(): void; }",
            "%compileTime { let value = 1; }",
            "%test(\"legacy\") { }",
            "%owned class LegacyResource { }",
            "let value = %borrow(owner);",
            "let value = %loan(owner);",
            "let value = %release(owner);",
            "let value = %upgrade(weakValue);",
            "let value = %weak(sharedValue);",
            "let value = %shared(uniqueValue);",
            "let value = %detach(uniqueValue);",
            "let value = %unique(owner);",
            "let value: %unique LegacyResource;",
            "let value: %shared LegacyResource;",
            "let value: %borrow LegacyResource;",
            "let value: %loan LegacyResource;",
            "let value: %borrowed LegacyResource;",
            "let value: %loaned LegacyResource;",
            "let callback: %func(int) => int;",
            "let reflected = %type(value);",
            "fn input(%in value: int): void { }",
            "fn output(%out value: int): void { }",
            "fn reference(%ref value: int): void { }",
            "%using (var value = resource) { }",
            "%async fn work(): zr.task.Task<void> { }",
            "let value = %await task;",
    };
    TZrSize index;

    for (index = 0U; index < sizeof(sources) / sizeof(sources[0]); index++) {
        assert_legacy_source_is_rejected(sources[index]);
    }
}

static void test_non_percent_legacy_forms_are_diagnostics_only(void) {
    static const TZrChar *const sources[] = {
            "func legacy(): void { }",
            "legacy(): void { }",
            "test(\"legacy\") { }",
            "fn legacy() -> void { }",
            "fn legacy() => void { }",
            "let callback: fn(int) => int;",
            "let value = $LegacyValue();",
            "let value = $(factory)();",
            "let value = Unique<LegacyResource>(resource);",
            "let value = Shared<LegacyResource>(resource);",
            "let value = Weak<LegacyResource>(resource);",
            "let value = Borrow<LegacyResource>(resource);",
            "let value = Loan<LegacyResource>(resource);",
            "let callback = () => { };",
            "let callback = (value: int) -> { return value; };",
            "class LegacyClass { func method(): void { } }",
            "class LegacyClass { method(): void { } }",
            "struct LegacyStruct { method(): void { } }",
            "interface LegacyInterface { method(): void; }",
            "module \"legacy.module\";",
            "let legacy = import \"legacy.module\";",
            "let legacy = import legacy.module;",
            "var const legacy = 1;",
            "class LegacyClassField { var const value = 1; }",
            "struct LegacyStructField { var const value = 1; }",
            "comptime var legacy = 1;",
            "comptime class LegacyDecorator { }",
            "comptime struct LegacyDecorator { }",
            "let value: Borrow<LegacyResource>;",
            "let value: Loan<LegacyResource>;",
            "let value = {{ out 1; }};",
            "out 1;",
            "intermediate legacy(): int % < > [ ] ( ) { FunctionReturn 0 1; }",
            "class LegacyProperty { pub get value: int { return 1; } }",
    };
    TZrSize index;

    for (index = 0U; index < sizeof(sources) / sizeof(sources[0]); index++) {
        assert_legacy_source_is_rejected(sources[index]);
    }
}

static void test_unknown_percent_identifier_is_not_a_migration_rule(void) {
    const TZrChar *source = "let value = %future(value);";
    SZrString *sourceName = ZrCore_String_Create(g_state,
                                                 "percent_syntax_cutover_unknown.zr",
                                                 strlen("percent_syntax_cutover_unknown.zr"));
    SZrParserState parserState;
    SZrCutoverDiagnosticCapture capture = {0};
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_cutover_parser_error;
    parserState.structuredErrorCallback = capture_cutover_diagnostic;
    parserState.errorUserData = &capture;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, capture.errorCount);
    TEST_ASSERT_EQUAL_UINT32(0U, capture.removedSyntaxCount);

    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
    }
    ZrParser_State_Free(&parserState);
}

static void test_current_surface_and_modulo_remain_parseable(void) {
    const TZrChar *source =
            "module test.current;\n"
            "let system = import(\"zr.system\");\n"
            "resource class CurrentResource { }\n"
            "let owner: Unique<CurrentResource>;\n"
            "let callback: fn(int) -> int;\n"
            "let modes: fn(in int, out int, ref int) -> int;\n"
            "let transform = fn(value: int): int => value;\n"
            "native extern(\"current\") { fn call(value: int): int; }\n"
            "comptime { let value = 1; }\n"
            "#zr.testing.test# fn sample(): void { }\n"
            "fn remainder(): int { return 7 % 2; }\n"
            "fn inspect(value: object): object { return typeof(value); }\n";
    SZrString *sourceName = ZrCore_String_Create(g_state,
                                                 "percent_syntax_cutover_current.zr",
                                                 strlen("percent_syntax_cutover_current.zr"));
    SZrParserState parserState;
    SZrCutoverDiagnosticCapture capture = {0};
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_cutover_parser_error;
    parserState.structuredErrorCallback = capture_cutover_diagnostic;
    parserState.errorUserData = &capture;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_EQUAL_UINT32(0U, capture.errorCount);
    TEST_ASSERT_EQUAL_UINT32(0U, capture.removedSyntaxCount);

    ZrParser_Ast_Free(g_state, ast);
    ZrParser_State_Free(&parserState);
}

static void test_ownership_like_member_names_use_only_object_dispatch(void) {
    static const TZrChar *const sources[] = {
            "class MemberCarrier { pub fn borrow(): int { return 1; } }\n"
            "return new MemberCarrier().borrow();\n",
            "class MemberCarrier { pub fn loan(): int { return 1; } }\n"
            "return new MemberCarrier().loan();\n",
            "class MemberCarrier { pub fn release(): int { return 1; } }\n"
            "return new MemberCarrier().release();\n",
            "class MemberCarrier { pub fn detach(): int { return 1; } }\n"
            "return new MemberCarrier().detach();\n",
    };
    TZrSize index;

    for (index = 0U; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrString *sourceName = ZrCore_String_Create(
                g_state,
                "ownership_compatibility_member_cutover.zr",
                strlen("ownership_compatibility_member_cutover.zr"));
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(sourceName);
        function = ZrParser_Source_Compile(
                g_state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL_MESSAGE(function, sources[index]);
        TEST_ASSERT_FALSE(function_tree_contains_opcode(
                function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(
                function, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(
                function, ZR_INSTRUCTION_ENUM(OWN_DROP)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(
                function, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX)));
        ZrCore_Function_Free(g_state, function);
    }
}

static void test_removed_using_reference_ownership_form_does_not_lower(void) {
    const TZrChar *source =
            "resource class LegacyResource { }\n"
            "let owner = own LegacyResource();\n"
            "using (ref owner) { }\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "ownership_using_reference_cutover.zr",
            strlen("ownership_using_reference_cutover.zr"));
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    if (function != ZR_NULL) {
        ZrCore_Function_Free(g_state, function);
    }
    TEST_ASSERT_NULL(function);
}

static void test_canonical_reference_bindings_compile_without_legacy_ownership_types(void) {
    const TZrChar *source =
            "resource class CurrentResource { }\n"
            "fn lifecycle(): int {\n"
            "    var uniqueOwner = own CurrentResource();\n"
            "    { var writable: ref CurrentResource = ref uniqueOwner; }\n"
            "    var sharedOwner = share(uniqueOwner);\n"
            "    { var readonlyView: ref readonly CurrentResource = ref sharedOwner; }\n"
            "    return 1;\n"
            "}\n"
            "return lifecycle();\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "canonical_reference_bindings.zr",
            strlen("canonical_reference_bindings.zr"));
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_known_percent_directives_are_diagnostics_only);
    RUN_TEST(test_non_percent_legacy_forms_are_diagnostics_only);
    RUN_TEST(test_unknown_percent_identifier_is_not_a_migration_rule);
    RUN_TEST(test_current_surface_and_modulo_remain_parseable);
    RUN_TEST(test_ownership_like_member_names_use_only_object_dispatch);
    RUN_TEST(test_removed_using_reference_ownership_form_does_not_lower);
    RUN_TEST(test_canonical_reference_bindings_compile_without_legacy_ownership_types);
    return UNITY_END();
}
