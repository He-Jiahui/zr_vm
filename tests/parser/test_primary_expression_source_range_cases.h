#ifndef ZR_TEST_PRIMARY_EXPRESSION_SOURCE_RANGE_CASES_H
#define ZR_TEST_PRIMARY_EXPRESSION_SOURCE_RANGE_CASES_H

#include <stdio.h>
#include <string.h>

#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

typedef struct SZrTestPrimaryExpressionSourceRangeFixture {
    SZrParserState parser;
    SZrAstNode *ast;
    SZrString *sourceName;
    SZrGcRootLease sourceNameRoot;
    TZrBool parserInitialized;
} SZrTestPrimaryExpressionSourceRangeFixture;

/* Keep ownership reachable when a Unity prerequisite assertion longjmps. */
static SZrTestPrimaryExpressionSourceRangeFixture g_primaryExpressionSourceRange;

static void primary_expression_source_range_cleanup(void) {
    if (g_state != ZR_NULL) {
        if (g_primaryExpressionSourceRange.ast != ZR_NULL) {
            ZrParser_Ast_Free(g_state, g_primaryExpressionSourceRange.ast);
        }
        if (g_primaryExpressionSourceRange.parserInitialized) {
            ZrParser_State_Free(&g_primaryExpressionSourceRange.parser);
        }
        ZrCore_Gc_RootLeaseRelease(
                g_state->global, &g_primaryExpressionSourceRange.sourceNameRoot);
    }
    memset(&g_primaryExpressionSourceRange, 0, sizeof(g_primaryExpressionSourceRange));
}

static TZrBool primary_expression_source_range_matches(
        const SZrAstNode *node, TZrSize start, TZrSize end) {
    return node != ZR_NULL &&
           node->location.source == g_primaryExpressionSourceRange.sourceName &&
           node->location.start.offset == start && node->location.end.offset == end &&
           node->location.start.line == 1 && node->location.end.line == 1 &&
           node->location.start.column == (TZrInt32)start + 1 &&
           node->location.end.column == (TZrInt32)end + 1;
}

static void primary_expression_source_range_require_local(
        const SZrAstNode *node, EZrAstNodeType type, TZrSize start, TZrSize end) {
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "Fixture prerequisite: local AST node is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(type, node->type,
                                 "Fixture prerequisite: local AST kind changed");
    TEST_ASSERT_TRUE_MESSAGE(primary_expression_source_range_matches(node, start, end),
                             "Fixture prerequisite: leaf or postfix segment range changed");
}

static void primary_expression_source_range_require_identifier(
        const SZrAstNode *node, const char *name, TZrSize start) {
    primary_expression_source_range_require_local(
            node, ZR_AST_IDENTIFIER_LITERAL, start, start + strlen(name));
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.identifier.name,
                                 "Fixture prerequisite: identifier name is missing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(name, ZrCore_String_GetNativeString(node->data.identifier.name),
                                    "Fixture prerequisite: identifier identity changed");
}

static SZrAstNodeArray *primary_expression_source_range_require_primary(
        SZrAstNode *node, TZrSize memberCount) {
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "Fixture prerequisite: PRIMARY is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_PRIMARY_EXPRESSION, node->type,
                                 "Fixture prerequisite: postfix source did not produce a PRIMARY");
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.primaryExpression.property,
                                 "Fixture prerequisite: PRIMARY root property is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.primaryExpression.members,
                                 "Fixture prerequisite: PRIMARY members are missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.primaryExpression.members->nodes,
                                 "Fixture prerequisite: PRIMARY member storage is missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE((TZrUInt64)memberCount,
            (TZrUInt64)node->data.primaryExpression.members->count,
            "Fixture prerequisite: PRIMARY member count changed");
    return node->data.primaryExpression.members;
}

static SZrAstNode *primary_expression_source_range_require_call(
        SZrAstNode *node, TZrSize start, TZrSize end) {
    primary_expression_source_range_require_local(node, ZR_AST_FUNCTION_CALL, start, end);
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.functionCall.args,
                                 "Fixture prerequisite: CALL arguments are missing");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1u, (TZrUInt64)node->data.functionCall.args->count,
                                   "Fixture prerequisite: CALL argument count changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.functionCall.args->nodes,
                                 "Fixture prerequisite: CALL argument storage is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.functionCall.args->nodes[0],
                                 "Fixture prerequisite: CALL argument is missing");
    return node->data.functionCall.args->nodes[0];
}

static SZrAstNode *primary_expression_source_range_require_member(
        SZrAstNode *node, TZrSize start, TZrSize end, TZrBool computed) {
    primary_expression_source_range_require_local(node, ZR_AST_MEMBER_EXPRESSION, start, end);
    TEST_ASSERT_EQUAL_INT_MESSAGE(computed, node->data.memberExpression.computed,
                                 "Fixture prerequisite: MEMBER access form changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(node->data.memberExpression.property,
                                 "Fixture prerequisite: MEMBER property is missing");
    return node->data.memberExpression.property;
}

static SZrAstNode *primary_expression_source_range_parse(const char *source) {
    SZrParserState *parser = &g_primaryExpressionSourceRange.parser;
    SZrAstNode *first;
    SZrAstNode *following;
    const char *returnText = strstr(source, "return 9;");
    TZrSize returnStart;

    primary_expression_source_range_cleanup();
    TEST_ASSERT_NOT_NULL_MESSAGE(g_state, "Fixture prerequisite: runtime state is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(returnText, "Fixture prerequisite: following RETURN source is missing");
    returnStart = (TZrSize)(returnText - source);
    g_primaryExpressionSourceRange.sourceName = ZrCore_String_CreateFromNative(
            g_state, "primary_expression_source_range.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_primaryExpressionSourceRange.sourceName,
                                 "Fixture prerequisite: source name allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(g_primaryExpressionSourceRange.sourceName),
            &g_primaryExpressionSourceRange.sourceNameRoot),
            "Fixture prerequisite: source name rooting failed");
    g_primaryExpressionSourceRange.parserInitialized = ZR_TRUE;
    ZrParser_State_Init(parser, g_state, source, strlen(source),
                        g_primaryExpressionSourceRange.sourceName);
    TEST_ASSERT_FALSE_MESSAGE(parser->hasError || parser->hasFatalError,
                              "Fixture prerequisite: parser initialization failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(parser->lexer, "Fixture prerequisite: lexer is missing");
    parser->suppressErrorOutput = ZR_TRUE;
    g_primaryExpressionSourceRange.ast = ZrParser_ParseWithState(parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_primaryExpressionSourceRange.ast,
                                 "Fixture prerequisite: real source parse failed");
    TEST_ASSERT_FALSE_MESSAGE(parser->hasError || parser->hasFatalError || parser->lexer->t.hasLexError,
                              "Fixture prerequisite: source parsing reported an error");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_TK_EOS, parser->lexer->t.token,
                                 "Fixture prerequisite: source parsing did not reach EOS");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_SCRIPT, g_primaryExpressionSourceRange.ast->type,
                                 "Fixture prerequisite: source did not produce a SCRIPT");
    TEST_ASSERT_NOT_NULL(g_primaryExpressionSourceRange.ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(2u,
            (TZrUInt64)g_primaryExpressionSourceRange.ast->data.script.statements->count,
            "Fixture prerequisite: following statement was not preserved");
    TEST_ASSERT_NOT_NULL(g_primaryExpressionSourceRange.ast->data.script.statements->nodes);
    first = g_primaryExpressionSourceRange.ast->data.script.statements->nodes[0];
    following = g_primaryExpressionSourceRange.ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL_MESSAGE(first, "Fixture prerequisite: first statement is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_EXPRESSION_STATEMENT, first->type,
                                 "Fixture prerequisite: first statement is not an expression");
    primary_expression_source_range_require_local(
            following, ZR_AST_RETURN_STATEMENT, returnStart, strlen(source));
    primary_expression_source_range_require_local(
            following->data.returnStatement.expr, ZR_AST_INTEGER_LITERAL,
            returnStart + 7u, returnStart + 8u);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(9, following->data.returnStatement.expr->data.integerLiteral.value,
                                   "Fixture prerequisite: following RETURN value changed");
    return first->data.expressionStatement.expr;
}

static void test_primary_expression_ranges_include_postfix_members(void) {
    static const char *sources[] = {
            "target(watcher); return 9;",
            "owner.field; return 9;",
            "owner[0]; return 9;",
            "owner.call(watcher).field[0]; return 9;",
            "target(other(watcher)).field; return 9;"};
    static const TZrSize memberCounts[] = {1u, 1u, 1u, 4u, 2u};
    TZrBool allPassed = ZR_TRUE;
    TZrUInt32 executed = 0u;

    for (TZrSize index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrAstNode *primary = primary_expression_source_range_parse(sources[index]);
        SZrAstNodeArray *members = primary_expression_source_range_require_primary(
                primary, memberCounts[index]);
        const char *semicolon = strchr(sources[index], ';');
        TZrSize expectedEnd;
        TZrBool passed;
        SZrAstNode *node;

        TEST_ASSERT_NOT_NULL_MESSAGE(semicolon, "Fixture prerequisite: first semicolon is missing");
        expectedEnd = (TZrSize)(semicolon - sources[index]);
        passed = primary_expression_source_range_matches(primary, 0u, expectedEnd);
        primary_expression_source_range_require_identifier(
                primary->data.primaryExpression.property,
                index == 0u || index == 4u ? "target" : "owner", 0u);
        switch (index) {
            case 0u:
                node = primary_expression_source_range_require_call(members->nodes[0], 6u, 15u);
                primary_expression_source_range_require_identifier(node, "watcher", 7u);
                break;
            case 1u:
                node = primary_expression_source_range_require_member(members->nodes[0], 5u, 11u, ZR_FALSE);
                primary_expression_source_range_require_identifier(node, "field", 6u);
                break;
            case 2u:
                node = primary_expression_source_range_require_member(members->nodes[0], 5u, 8u, ZR_TRUE);
                primary_expression_source_range_require_local(node, ZR_AST_INTEGER_LITERAL, 6u, 7u);
                TEST_ASSERT_EQUAL_INT64(0, node->data.integerLiteral.value);
                break;
            case 3u:
                node = primary_expression_source_range_require_member(members->nodes[0], 5u, 10u, ZR_FALSE);
                primary_expression_source_range_require_identifier(node, "call", 6u);
                node = primary_expression_source_range_require_call(members->nodes[1], 10u, 19u);
                primary_expression_source_range_require_identifier(node, "watcher", 11u);
                node = primary_expression_source_range_require_member(members->nodes[2], 19u, 25u, ZR_FALSE);
                primary_expression_source_range_require_identifier(node, "field", 20u);
                node = primary_expression_source_range_require_member(members->nodes[3], 25u, 28u, ZR_TRUE);
                primary_expression_source_range_require_local(node, ZR_AST_INTEGER_LITERAL, 26u, 27u);
                TEST_ASSERT_EQUAL_INT64(0, node->data.integerLiteral.value);
                break;
            case 4u: {
                SZrAstNode *nested = primary_expression_source_range_require_call(members->nodes[0], 6u, 22u);
                SZrAstNodeArray *nestedMembers = primary_expression_source_range_require_primary(nested, 1u);
                TZrBool nestedPassed = primary_expression_source_range_matches(nested, 7u, 21u);
                primary_expression_source_range_require_identifier(
                        nested->data.primaryExpression.property, "other", 7u);
                node = primary_expression_source_range_require_call(nestedMembers->nodes[0], 12u, 21u);
                primary_expression_source_range_require_identifier(node, "watcher", 13u);
                node = primary_expression_source_range_require_member(members->nodes[1], 22u, 28u, ZR_FALSE);
                primary_expression_source_range_require_identifier(node, "field", 23u);
                if (!nestedPassed) {
                    printf("PRIMARY nested range observation: case=%u actual=%llu..%llu expected=7..21\n",
                           (unsigned)index, (unsigned long long)nested->location.start.offset,
                           (unsigned long long)nested->location.end.offset);
                }
                passed = passed && nestedPassed;
                break;
            }
        }
        if (!passed) {
            printf("PRIMARY range observation: case=%u actual=%llu..%llu %d:%d..%d:%d "
                   "expected=0..%llu 1:1..1:%d sourceMatches=%u\n",
                   (unsigned)index, (unsigned long long)primary->location.start.offset,
                   (unsigned long long)primary->location.end.offset,
                   primary->location.start.line, primary->location.start.column,
                   primary->location.end.line, primary->location.end.column,
                   (unsigned long long)expectedEnd, (int)expectedEnd + 1,
                   (unsigned)(primary->location.source == g_primaryExpressionSourceRange.sourceName));
        }
        allPassed = allPassed && passed;
        executed++;
        primary_expression_source_range_cleanup();
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, executed,
                                   "Fixture prerequisite: all five sources must be parsed");
    TEST_ASSERT_TRUE_MESSAGE(allPassed,
            "PRIMARY ranges must include every adopted postfix member without absorbing the next statement");
}

#endif
