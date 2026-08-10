#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"

static SZrState *g_state;

typedef struct SZrCapturedParserDiagnostic {
    TZrBool reported;
    TZrChar message[256];
} SZrCapturedParserDiagnostic;

static void capture_parser_error(TZrPtr userData,
                                 const SZrFileRange *location,
                                 const TZrChar *message,
                                 EZrToken token) {
    SZrCapturedParserDiagnostic *diagnostic =
            (SZrCapturedParserDiagnostic *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (diagnostic == ZR_NULL || diagnostic->reported) {
        return;
    }
    diagnostic->reported = ZR_TRUE;
    diagnostic->message[0] = '\0';
    if (message != ZR_NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
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

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_intrinsic_member_separation.zr");

    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *statement_expression(SZrAstNode *script, TZrSize index) {
    SZrAstNode *statement;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)script->data.script.statements->count);
    statement = script->data.script.statements->nodes[index];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
    return statement->data.expressionStatement.expr;
}

static void assert_parse_error(const TZrChar *source, const TZrChar *expectedFragment) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ownership_intrinsic_error.zr");
    SZrCapturedParserDiagnostic diagnostic;
    SZrParserState parserState;
    SZrAstNode *script;

    memset(&diagnostic, 0, sizeof(diagnostic));
    ZrParser_State_Init(&parserState, g_state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    script = ZrParser_ParseWithState(&parserState);
    TEST_ASSERT_TRUE(diagnostic.reported || parserState.hasError || script == ZR_NULL);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, expectedFragment));

    if (script != ZR_NULL) {
        ZrParser_Ast_Free(g_state, script);
    }
    ZrParser_State_Free(&parserState);
}

static void test_question_dot_is_one_token(void) {
    const TZrChar *source = "?.";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "question_dot.zr");
    SZrLexState lexer;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), sourceName);

    TEST_ASSERT_EQUAL_INT(ZR_TK_QUESTION_DOT, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);
}

static void test_question_dot_requires_adjacent_characters(void) {
    const TZrChar *source = "? .";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "question_space_dot.zr");
    SZrLexState lexer;

    ZrParser_Lexer_Init(&lexer, g_state, source, strlen(source), sourceName);

    TEST_ASSERT_EQUAL_INT(ZR_TK_QUESTIONMARK, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_DOT, lexer.t.token);
    ZrParser_Lexer_Next(&lexer);
    TEST_ASSERT_EQUAL_INT(ZR_TK_EOS, lexer.t.token);
}

static void test_reserved_intrinsics_have_independent_ast(void) {
    static const EZrOwnershipIntrinsicOperation expectedOperations[] = {
            ZR_OWNERSHIP_INTRINSIC_SHARE,
            ZR_OWNERSHIP_INTRINSIC_DEGRADE,
            ZR_OWNERSHIP_INTRINSIC_WAKE,
            ZR_OWNERSHIP_INTRINSIC_INTO_GC,
            ZR_OWNERSHIP_INTRINSIC_DROP,
    };
    SZrAstNode *script = parse_source(
            "share(owner); degrade(shared); wake(weak); intoGc(owner); drop(owner);");

    for (TZrSize index = 0u;
         index < sizeof(expectedOperations) / sizeof(expectedOperations[0]);
         index++) {
        SZrAstNode *expression = statement_expression(script, index);

        TEST_ASSERT_EQUAL_INT(ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION, expression->type);
        TEST_ASSERT_EQUAL_INT(
                expectedOperations[index],
                expression->data.ownershipIntrinsicExpression.operation);
        TEST_ASSERT_NOT_NULL(expression->data.ownershipIntrinsicExpression.argument);
    }

    ZrParser_Ast_Free(g_state, script);
}

static void test_optional_member_and_call_segments_record_access_mode(void) {
    SZrAstNode *script = parse_source("weak?.service.send(1)?.(2);");
    SZrAstNode *expression = statement_expression(script, 0u);
    SZrAstNodeArray *segments;

    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    segments = expression->data.primaryExpression.members;
    TEST_ASSERT_NOT_NULL(segments);
    TEST_ASSERT_EQUAL_UINT32(4u, (TZrUInt32)segments->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, segments->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_OPTIONAL,
            segments->nodes[0]->data.memberExpression.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, segments->nodes[1]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_DIRECT,
            segments->nodes[1]->data.memberExpression.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, segments->nodes[2]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_DIRECT,
            segments->nodes[2]->data.functionCall.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, segments->nodes[3]->type);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_OPTIONAL,
            segments->nodes[3]->data.functionCall.accessMode);
    TEST_ASSERT_EQUAL_UINT32(4u, segments->nodes[0]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(13u, segments->nodes[1]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(18u, segments->nodes[2]->location.start.offset);
    TEST_ASSERT_EQUAL_UINT32(21u, segments->nodes[3]->location.start.offset);

    ZrParser_Ast_Free(g_state, script);
}

static void test_intrinsic_spellings_remain_legal_member_names(void) {
    static const TZrChar *expectedNames[] = {
            "share", "degrade", "wake", "intoGc", "drop",
    };
    SZrAstNode *script = parse_source(
            "class Box {\n"
            "  pub fn share(): int { return 1; }\n"
            "  pub fn degrade(): int { return 2; }\n"
            "  pub fn wake(): int { return 3; }\n"
            "  pub fn intoGc(): int { return 4; }\n"
            "  pub fn drop(): int { return 5; }\n"
            "}\n"
            "new Box().share();\n"
            "new Box().degrade();\n"
            "new Box().wake();\n"
            "new Box().intoGc();\n"
            "new Box().drop();\n");
    SZrAstNode *declaration;

    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(6u, (TZrUInt32)script->data.script.statements->count);
    declaration = script->data.script.statements->nodes[0];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT32(
            5u,
            (TZrUInt32)declaration->data.classDeclaration.members->count);
    for (TZrSize index = 0u; index < 5u; index++) {
        SZrAstNode *member = declaration->data.classDeclaration.members->nodes[index];

        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, member->type);
        TEST_ASSERT_NOT_NULL(member->data.classMethod.name);
        TEST_ASSERT_NOT_NULL(member->data.classMethod.name->name);
        TEST_ASSERT_EQUAL_STRING(
                expectedNames[index],
                ZrCore_String_GetNativeString(member->data.classMethod.name->name));
    }

    for (TZrSize index = 1u; index < 6u; index++) {
        SZrAstNode *expression = statement_expression(script, index);
        SZrAstNodeArray *segments;

        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
        segments = expression->data.primaryExpression.members;
        TEST_ASSERT_NOT_NULL(segments);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2u, (TZrUInt32)segments->count);
        TEST_ASSERT_EQUAL_INT(
                ZR_POSTFIX_ACCESS_DIRECT,
                segments->nodes[segments->count - 2u]->data.memberExpression.accessMode);
        TEST_ASSERT_EQUAL_INT(
                ZR_POSTFIX_ACCESS_DIRECT,
                segments->nodes[segments->count - 1u]->data.functionCall.accessMode);
    }

    ZrParser_Ast_Free(g_state, script);
}

static void test_direct_and_optional_callable_syntax_are_distinct(void) {
    SZrAstNode *script = parse_source("callback(1); callback?.(2);");
    SZrAstNode *direct = statement_expression(script, 0u);
    SZrAstNode *optional = statement_expression(script, 1u);

    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, direct->type);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)direct->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_DIRECT,
            direct->data.primaryExpression.members->nodes[0]->data.functionCall.accessMode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, optional->type);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)optional->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_POSTFIX_ACCESS_OPTIONAL,
            optional->data.primaryExpression.members->nodes[0]->data.functionCall.accessMode);

    ZrParser_Ast_Free(g_state, script);
}

static void test_intrinsic_syntax_reports_precise_errors(void) {
    assert_parse_error(
            "share;",
            "Ownership intrinsic must be called with exactly one positional argument");
    assert_parse_error(
            "share();",
            "Ownership intrinsic requires exactly one positional argument");
    assert_parse_error(
            "share(first, second);",
            "Ownership intrinsic accepts exactly one positional argument");
    assert_parse_error(
            "share(value: owner);",
            "Ownership intrinsic accepts exactly one positional argument");
    assert_parse_error("let share = owner;", "Expected identifier");
}

static void test_invalid_optional_postfix_forms_report_distinct_errors(void) {
    assert_parse_error("callback.(1);", "Missing member name after '.'");
    assert_parse_error("receiver?.[index];", "Optional computed access '?.[' is not supported");
    assert_parse_error("receiver?.;", "Missing member name after '.'");
    assert_parse_error("receiver?.(1;", "Missing closing ')' in function call");
}

static void test_syntax_writer_preserves_intrinsic_and_access_modes(void) {
    SZrAstNode *script = parse_source("share(owner); weak?.service.send(1)?.(2);");
    TZrChar outputPath[1024];
    TZrChar *output;
    TZrSize outputLength = 0u;

    TEST_ASSERT_GREATER_THAN_INT(
            0,
            snprintf(
                    outputPath,
                    sizeof(outputPath),
                    "%s/%s",
                    ZR_VM_TESTS_BINARY_DIR,
                    "ownership_intrinsic_member_separation.zrs"));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteSyntaxTreeFile(g_state, script, outputPath));
    output = ZrTests_ReadTextFile(outputPath, &outputLength);
    remove(outputPath);

    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)outputLength);
    TEST_ASSERT_NOT_NULL(strstr(output, "OWNERSHIP_INTRINSIC_EXPRESSION"));
    TEST_ASSERT_NOT_NULL(strstr(output, "operation: share"));
    TEST_ASSERT_NOT_NULL(strstr(output, "access: optional"));
    TEST_ASSERT_NOT_NULL(strstr(output, "access: direct"));

    free(output);
    ZrParser_Ast_Free(g_state, script);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_question_dot_is_one_token);
    RUN_TEST(test_question_dot_requires_adjacent_characters);
    RUN_TEST(test_reserved_intrinsics_have_independent_ast);
    RUN_TEST(test_optional_member_and_call_segments_record_access_mode);
    RUN_TEST(test_intrinsic_spellings_remain_legal_member_names);
    RUN_TEST(test_direct_and_optional_callable_syntax_are_distinct);
    RUN_TEST(test_intrinsic_syntax_reports_precise_errors);
    RUN_TEST(test_invalid_optional_postfix_forms_report_distinct_errors);
    RUN_TEST(test_syntax_writer_preserves_intrinsic_and_access_modes);
    return UNITY_END();
}
