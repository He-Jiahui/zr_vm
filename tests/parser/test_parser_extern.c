//
// Created by Auto on 2026/03/31.
//

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "unity.h"
#include "zr_vm_parser.h"
#include "zr_vm_core/string.h"
#include "test_support.h"

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrExternParserTestTimer;

#define TEST_START(summary) do { \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_PASS_CUSTOM(timer, summary) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while(0)

static SZrState *create_test_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

static void destroy_test_state(SZrState *state) {
    ZrTests_State_Destroy(state);
}

static SZrAstNode *get_script_statement(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT || ast->data.script.statements == ZR_NULL ||
        index >= ast->data.script.statements->count) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

static const char *identifier_node_native(SZrState *state, SZrAstNode *node) {
    if (state == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(node->data.identifier.name);
}

static const char *identifier_native(SZrIdentifier *identifier) {
    if (identifier == ZR_NULL || identifier->name == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(identifier->name);
}

static void assert_decorator_leaf_name(SZrState *state, SZrAstNode *decoratorNode, const char *expectedLeafName) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primaryExpr;
    SZrAstNode *ffiMember;
    SZrAstNode *leafMember;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(expectedLeafName);
    TEST_ASSERT_NOT_NULL(decoratorNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DECORATOR_EXPRESSION, decoratorNode->type);

    expr = decoratorNode->data.decoratorExpression.expr;
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expr->type);

    primaryExpr = &expr->data.primaryExpression;
    TEST_ASSERT_EQUAL_STRING("zr", identifier_node_native(state, primaryExpr->property));
    TEST_ASSERT_NOT_NULL(primaryExpr->members);
    TEST_ASSERT_EQUAL_INT(2, (int)primaryExpr->members->count);

    ffiMember = primaryExpr->members->nodes[0];
    leafMember = primaryExpr->members->nodes[1];
    TEST_ASSERT_NOT_NULL(ffiMember);
    TEST_ASSERT_NOT_NULL(leafMember);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, ffiMember->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, leafMember->type);
    TEST_ASSERT_FALSE(ffiMember->data.memberExpression.computed);
    TEST_ASSERT_FALSE(leafMember->data.memberExpression.computed);
    TEST_ASSERT_EQUAL_STRING("ffi", identifier_node_native(state, ffiMember->data.memberExpression.property));
    TEST_ASSERT_EQUAL_STRING(expectedLeafName, identifier_node_native(state, leafMember->data.memberExpression.property));
}

void test_extern_delegate_parameter_decorator_flags_parsing(void);

void test_extern_delegate_parameter_decorator_flags_parsing(void) {
    SZrExternParserTestTimer timer;
    const char *testSummary = "Extern Delegate Parameter Decorator Flags Parsing";
    const char *source =
            "%extern(\"fixture\") {\n"
            "    delegate MutPtr(\n"
            "        #zr.ffi.out#\n"
            "        #zr.ffi.inout#\n"
            "        value:pointer<i32>\n"
            "    ): void;\n"
            "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *externStmt;
    SZrAstNode *delegateDecl;
    SZrAstNode *paramNode;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "extern_delegate_param_decorators.zr", 34);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

    externStmt = get_script_statement(ast, 0);
    TEST_ASSERT_NOT_NULL(externStmt);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, externStmt->type);
    TEST_ASSERT_NOT_NULL(externStmt->data.externBlock.declarations);
    TEST_ASSERT_EQUAL_INT(1, (int)externStmt->data.externBlock.declarations->count);

    delegateDecl = externStmt->data.externBlock.declarations->nodes[0];
    TEST_ASSERT_NOT_NULL(delegateDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_DELEGATE_DECLARATION, delegateDecl->type);
    TEST_ASSERT_NOT_NULL(delegateDecl->data.externDelegateDeclaration.params);
    TEST_ASSERT_EQUAL_INT(1, (int)delegateDecl->data.externDelegateDeclaration.params->count);

    paramNode = delegateDecl->data.externDelegateDeclaration.params->nodes[0];
    TEST_ASSERT_NOT_NULL(paramNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, paramNode->type);
    TEST_ASSERT_EQUAL_STRING("value", identifier_native(paramNode->data.parameter.name));
    TEST_ASSERT_NOT_NULL(paramNode->data.parameter.decorators);
    TEST_ASSERT_EQUAL_INT(2, (int)paramNode->data.parameter.decorators->count);

    assert_decorator_leaf_name(state, paramNode->data.parameter.decorators->nodes[0], "out");
    assert_decorator_leaf_name(state, paramNode->data.parameter.decorators->nodes[1], "inout");

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}
