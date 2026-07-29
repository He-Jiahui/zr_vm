#include "reference_loan_nll_test_support.h"

#include <string.h>

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"

ZR_PARSER_API TZrBool compiler_validate_reference_escapes(
        SZrCompilerState *compiler,
        SZrAstNode *node);

typedef struct SParserErrorCapture {
    TZrBool sawNativeExternFnRequirement;
} SParserErrorCapture;

static void capture_parser_error(
        TZrPtr userData,
        const SZrFileRange *location,
        const TZrChar *message,
        EZrToken token) {
    SParserErrorCapture *capture = (SParserErrorCapture *)userData;
    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (capture != ZR_NULL && message != ZR_NULL &&
        strstr(message, "must start with 'fn'") != ZR_NULL) {
        capture->sawNativeExternFnRequirement = ZR_TRUE;
    }
}

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reference_escape_closure_suspension.zr");
    SZrParserState parser;
    SZrAstNode *script;

    ZrParser_State_Init(&parser, g_state, source, strlen(source), sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_FALSE_MESSAGE(parser.hasError, parser.errorMessage);
    ZrParser_State_Free(&parser);
    return script;
}

static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static void assert_escape_validation(
        const TZrChar *source,
        TZrBool expectedSuccess,
        const TZrChar *expectedMessage,
        TZrInt32 expectedOriginLine,
        TZrInt32 expectedEscapeLine) {
    SZrCompilerState compiler;
    SZrAstNode *script = parse_source(source);
    TZrBool success;

    TEST_ASSERT_NOT_NULL(script);
    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    success = ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                      &compiler, script) &&
              compiler_validate_reference_escapes(&compiler, script);

    if (expectedSuccess) {
        TEST_ASSERT_TRUE_MESSAGE(success, compiler.errorMessage);
    } else {
        TEST_ASSERT_FALSE(success);
    }
    TEST_ASSERT_EQUAL_INT(!expectedSuccess, compiler.hasError);
    if (!expectedSuccess) {
        const SZrStructuredDiagnosticRelatedInformation *origin;

        TEST_ASSERT_NOT_NULL(compiler.errorMessage);
        TEST_ASSERT_NOT_NULL_MESSAGE(
                strstr(compiler.errorMessage, expectedMessage),
                compiler.errorMessage);
        TEST_ASSERT_TRUE(compiler.hasStructuredError);
        TEST_ASSERT_EQUAL_INT(expectedEscapeLine, compiler.structuredError.location.start.line);
        TEST_ASSERT_GREATER_THAN_UINT32(
                0U, (TZrUInt32)compiler.structuredError.relatedInformation.length);
        origin = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                &compiler.structuredError.relatedInformation, 0U);
        TEST_ASSERT_NOT_NULL(origin);
        TEST_ASSERT_EQUAL_INT(expectedOriginLine, origin->location.start.line);
    }

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_ref_type_surface_preserves_access_and_scoped_contract(void) {
    SZrAstNode *script = parse_source(
            "fn writable(value: ref int): ref int { return value; }\n"
            "fn observed(value: ref readonly int): ref readonly int { return value; }\n"
            "fn local(value: scoped ref int): scoped ref int { return value; }\n");
    const SZrFunctionDeclaration *writable =
            &script_statement(script, 0U)->data.functionDeclaration;
    const SZrFunctionDeclaration *observed =
            &script_statement(script, 1U)->data.functionDeclaration;
    const SZrFunctionDeclaration *local =
            &script_statement(script, 2U)->data.functionDeclaration;

    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_WRITABLE,
            writable->returnType->referenceAccess);
    TEST_ASSERT_FALSE(writable->returnType->isScopedReference);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_READONLY,
            observed->returnType->referenceAccess);
    TEST_ASSERT_FALSE(observed->returnType->isScopedReference);
    TEST_ASSERT_EQUAL_INT(
            ZR_REFERENCE_ACCESS_WRITABLE,
            local->returnType->referenceAccess);
    TEST_ASSERT_TRUE(local->returnType->isScopedReference);

    ZrParser_Ast_Free(g_state, script);
}

static void test_ref_type_projects_to_canonical_ref_without_legacy_owner_qualifier(void) {
    SZrAstNode *script = parse_source(
            "fn observe(value: ref readonly int): ref readonly int { return value; }");
    const SZrFunctionDeclaration *function =
            &script_statement(script, 0U)->data.functionDeclaration;
    SZrCompilerState compiler;
    SZrInferredType inferred;
    TZrTypeId typeId;
    const SZrCanonicalTypeNode *type;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &compiler, function->returnType, &inferred));
    TEST_ASSERT_EQUAL_INT(ZR_REFERENCE_ACCESS_READONLY, inferred.referenceAccess);
    TEST_ASSERT_EQUAL_INT(
            ZR_OWNERSHIP_QUALIFIER_NONE, inferred.ownershipQualifier);
    typeId = ZrParser_CanonicalType_FromInferred(
            compiler.semanticContext, &inferred);
    type = ZrParser_CanonicalType_Find(compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_REF, type->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_REF_READONLY, type->data.refType.access);

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_escape_fact_lattice_reports_origin_and_escape_ranges(void) {
    SLoanFixture fixture;
    TZrPlaceId parameterPlace;
    TZrRegionId functionRegion;
    TZrRegionId unknownRegion;
    TZrUInt32 escapeFactId;
    TZrUInt32 unknownEscapeFactId;
    const SZrSemanticEscapeFact *fact;
    const SZrSemanticFlowDiagnostic *diagnostic;

    fixture_init(&fixture);
    parameterPlace = add_place(
            &fixture, ZR_PARSER_PLACE_BASE_PARAMETER, 12U);
    functionRegion = ZrParser_SemanticIr_AddRegion(
            &fixture.function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            test_range(3));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_REGION_ID_INVALID, functionRegion);
    escapeFactId = ZrParser_SemanticIr_AddEscapeFact(
            &fixture.function,
            ZR_SEMANTIC_ESCAPE_KIND_RETURN,
            functionRegion,
            parameterPlace,
            ZR_SEMANTIC_ESCAPE_CALLER,
            test_range(8));
    TEST_ASSERT_NOT_EQUAL(0U, escapeFactId);
    fact = ZrParser_SemanticIr_EscapeFactAt(
            &fixture.function, escapeFactId - 1U);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_ESCAPE_FUNCTION, fact->sourceEscapeBound);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_ESCAPE_CALLER, fact->targetEscape);

    unknownRegion = ZrParser_SemanticIr_AddRegion(
            &fixture.function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_UNKNOWN,
            test_range(4));
    unknownEscapeFactId = ZrParser_SemanticIr_AddEscapeFact(
            &fixture.function,
            ZR_SEMANTIC_ESCAPE_KIND_CLOSURE_CAPTURE,
            unknownRegion,
            parameterPlace,
            ZR_SEMANTIC_ESCAPE_CALLER,
            test_range(9));
    TEST_ASSERT_NOT_EQUAL(0U, unknownEscapeFactId);

    bind_linear_cfg(&fixture);
    analyze(&fixture);
    diagnostic = ZrParser_SemanticFlow_EscapeDiagnostic(
            &fixture.result, escapeFactId);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(3, diagnostic->escapeOriginRange.start.line);
    TEST_ASSERT_EQUAL_INT(8, diagnostic->escapeTargetRange.start.line);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_ESCAPE_FUNCTION, diagnostic->sourceEscapeBound);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_ESCAPE_CALLER, diagnostic->targetEscape);
    diagnostic = ZrParser_SemanticFlow_EscapeDiagnostic(
            &fixture.result, unknownEscapeFactId);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_ESCAPE_UNKNOWN, diagnostic->sourceEscapeBound);

    fixture_free(&fixture);
}

static void test_non_scoped_ref_return_and_stricter_conditional_bound(void) {
    assert_escape_validation(
            "fn choose(left: ref int, right: ref int, flag: bool): ref int {\n"
            "  return flag ? left : right;\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
    assert_escape_validation(
            "fn choose(left: ref int, right: scoped ref int, flag: bool): ref int {\n"
            "  return flag ? left : right;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            1,
            2);
}

static void test_in_out_and_scoped_ref_cannot_escape_through_return(void) {
    assert_escape_validation(
            "fn leak(value: in int): ref readonly int {\n"
            "  return value;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            1,
            2);
    assert_escape_validation(
            "fn leak(value: out int): ref int {\n"
            "  value = 1;\n"
            "  return value;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            1,
            3);
    assert_escape_validation(
            "fn leak(value: scoped ref int): ref int {\n"
            "  return value;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            1,
            2);
}

static void test_ref_local_and_heap_container_store_obey_source_bound(void) {
    assert_escape_validation(
            "fn leak(value: scoped ref int): ref int {\n"
            "  var alias: ref int = value;\n"
            "  return alias;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            1,
            3);
    assert_escape_validation(
            "fn store(value: scoped ref int): void {\n"
            "  var items = [value];\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to heap/static through container store",
            1,
            2);
    assert_escape_validation(
            "fn store(value: scoped ref int): void {\n"
            "  var holder = { value: 0 };\n"
            "  holder.value = value;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to heap/static through heap/static store",
            1,
            3);
    assert_escape_validation(
            "fn store_values(value: int): void {\n"
            "  var items = [value];\n"
            "  var holder = { value: value };\n"
            "  holder.value = value;\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
    assert_escape_validation(
            "fn field(): ref int {\n"
            "  var holder = { value: 1 };\n"
            "  return holder.value;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            2,
            3);
    assert_escape_validation(
            "fn index(): ref int {\n"
            "  var items = [1];\n"
            "  return items[0];\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            2,
            3);
    assert_escape_validation(
            "fn field(holder: object): ref int {\n"
            "  return holder.value;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            1,
            2);
    assert_escape_validation(
            "fn field(holder: ref object): ref int {\n"
            "  return holder.value;\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
}

static void test_closure_capture_uses_closure_escape_and_scoped_rule(void) {
    assert_escape_validation(
            "fn local(value: scoped ref int): int {\n"
            "  var read = fn(): int => value;\n"
            "  return 0;\n"
            "}\n",
            ZR_FALSE,
            "cannot be captured by a closure",
            1,
            2);
    assert_escape_validation(
            "fn factory(value: ref int): fn() -> int {\n"
            "  return fn(): int => value;\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
    assert_escape_validation(
            "fn local(value: scoped ref int): int {\n"
            "  fn read(): int { return value; }\n"
            "  return 0;\n"
            "}\n",
            ZR_FALSE,
            "cannot be captured by a closure",
            1,
            2);
    assert_escape_validation(
            "fn factory(): fn() -> int {\n"
            "  var local: int = 1;\n"
            "  var alias: ref int = local;\n"
            "  fn read(): int { return alias; }\n"
            "  return read;\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through escaping closure return",
            2,
            5);
}

static void test_writable_capture_blocks_external_access_only_while_live(void) {
    assert_escape_validation(
            "fn invalid(value: ref int): int {\n"
            "  var write = fn(): int { value = value + 1; return value; };\n"
            "  value = 2;\n"
            "  return write();\n"
            "}\n",
            ZR_FALSE,
            "conflicts with writable closure capture",
            1,
            3);
    assert_escape_validation(
            "fn valid(value: ref int): int {\n"
            "  var write = fn(): int { value = value + 1; return value; };\n"
            "  var observed = write();\n"
            "  value = 2;\n"
            "  return observed;\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
}

static void test_await_checks_last_use_and_binding_epoch(void) {
    assert_escape_validation(
            "async fn invalid(value: ref int): Task<int> {\n"
            "  var task = pause().start();\n"
            "  await task;\n"
            "  return value;\n"
            "}\n",
            ZR_FALSE,
            "cannot cross an await suspension",
            1,
            3);
    assert_escape_validation(
            "async fn valid(value: scoped ref int): Task<int> {\n"
            "  var observed = value;\n"
            "  var task = pause().start();\n"
            "  await task;\n"
            "  return observed == 0 ? 0 : 1;\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
}

static void test_generator_yield_checks_reference_liveness(void) {
    assert_escape_validation(
            "fn invalid(value: ref int): int {\n"
            "  var sequence = {{\n"
            "    out 1;\n"
            "    out value;\n"
            "  }};\n"
            "  return 0;\n"
            "}\n",
            ZR_FALSE,
            "cannot cross a yield suspension",
            1,
            3);
}

static void test_reference_escape_follows_active_comptime_branch(void) {
    assert_escape_validation(
            "comptime if (true) {\n"
            "  fn leak(value: scoped ref int): ref int {\n"
            "    return value;\n"
            "  }\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return",
            2,
            3);
    assert_escape_validation(
            "comptime if (false) {\n"
            "  fn leak(value: scoped ref int): ref int {\n"
            "    return value;\n"
            "  }\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
}

static void test_native_ref_argument_is_call_scoped_by_default(void) {
    assert_escape_validation(
            "native extern(\"sample\") { fn inspect(value: scoped ref int): void; }\n"
            "fn valid(value: scoped ref int): void {\n"
            "  inspect(ref value);\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL,
            0,
            0);
}

static void test_native_extern_requires_fn_without_reserving_native_identifier(void) {
    const TZrChar *invalidSource =
            "native extern(\"sample\") { inspect(value: ref int): void; }";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "reference_escape_invalid_native.zr");
    SZrParserState parser;
    SParserErrorCapture capture;
    SZrAstNode *script;

    memset(&capture, 0, sizeof(capture));
    ZrParser_State_Init(
            &parser,
            g_state,
            invalidSource,
            strlen(invalidSource),
            sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    parser.errorCallback = capture_parser_error;
    parser.errorUserData = &capture;
    script = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_TRUE(parser.hasError);
    TEST_ASSERT_TRUE(capture.sawNativeExternFnRequirement);
    if (script != ZR_NULL) {
        ZrParser_Ast_Free(g_state, script);
    }
    ZrParser_State_Free(&parser);

    script = parse_source(
            "fn inspect(native: object): void { native.inspect(); }");
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_FUNCTION_DECLARATION, script_statement(script, 0U)->type);
    ZrParser_Ast_Free(g_state, script);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ref_type_surface_preserves_access_and_scoped_contract);
    RUN_TEST(test_ref_type_projects_to_canonical_ref_without_legacy_owner_qualifier);
    RUN_TEST(test_escape_fact_lattice_reports_origin_and_escape_ranges);
    RUN_TEST(test_non_scoped_ref_return_and_stricter_conditional_bound);
    RUN_TEST(test_in_out_and_scoped_ref_cannot_escape_through_return);
    RUN_TEST(test_ref_local_and_heap_container_store_obey_source_bound);
    RUN_TEST(test_closure_capture_uses_closure_escape_and_scoped_rule);
    RUN_TEST(test_writable_capture_blocks_external_access_only_while_live);
    RUN_TEST(test_await_checks_last_use_and_binding_epoch);
    RUN_TEST(test_generator_yield_checks_reference_liveness);
    RUN_TEST(test_reference_escape_follows_active_comptime_branch);
    RUN_TEST(test_native_ref_argument_is_call_scoped_by_default);
    RUN_TEST(test_native_extern_requires_fn_without_reserving_native_identifier);
    return UNITY_END();
}
