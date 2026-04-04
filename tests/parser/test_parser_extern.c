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
void test_top_level_class_decorator_parsing(void);
void test_compile_time_class_decorator_parsing(void);
void test_compile_time_public_class_decorator_parsing(void);
void test_compile_time_struct_decorator_parsing(void);
void test_compile_time_function_decorator_parsing(void);

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

void test_top_level_class_decorator_parsing(void) {
    SZrExternParserTestTimer timer;
    const char *testSummary = "Top Level Class Decorator Parsing";
    const char *source =
            "#singleton#\n"
            "class SingletonClass {\n"
            "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *classDecl;
    SZrAstNode *decoratorNode;
    SZrAstNode *expr;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "top_level_class_decorator.zr", 28);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

    classDecl = get_script_statement(ast, 0);
    TEST_ASSERT_NOT_NULL(classDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classDecl->type);
    TEST_ASSERT_EQUAL_STRING("SingletonClass", identifier_native(classDecl->data.classDeclaration.name));
    TEST_ASSERT_NOT_NULL(classDecl->data.classDeclaration.decorators);
    TEST_ASSERT_EQUAL_INT(1, (int)classDecl->data.classDeclaration.decorators->count);

    decoratorNode = classDecl->data.classDeclaration.decorators->nodes[0];
    TEST_ASSERT_NOT_NULL(decoratorNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DECORATOR_EXPRESSION, decoratorNode->type);
    expr = decoratorNode->data.decoratorExpression.expr;
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expr->type);
    TEST_ASSERT_EQUAL_STRING("singleton", identifier_node_native(state, expr));

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

void test_compile_time_class_decorator_parsing(void) {
    SZrExternParserTestTimer timer;
    const char *testSummary = "Compile Time Class Decorator Parsing";
    const char *source =
            "%compileTime class Serializable {\n"
            "    @decorate(target: %type Class): DecoratorPatch {\n"
            "        return { metadata: { serializable: true } };\n"
            "    }\n"
            "}\n"
            "\n"
            "#Serializable#\n"
            "class User {\n"
            "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *compileTimeDecl;
    SZrAstNode *decoratedClassDecl;
    SZrAstNode *decoratorNode;
    SZrAstNode *expr;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "compile_time_class_decorator.zr", 31);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

    compileTimeDecl = get_script_statement(ast, 0);
    TEST_ASSERT_NOT_NULL(compileTimeDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, compileTimeDecl->type);
    TEST_ASSERT_NOT_NULL(compileTimeDecl->data.compileTimeDeclaration.declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, compileTimeDecl->data.compileTimeDeclaration.declaration->type);
    TEST_ASSERT_EQUAL_STRING("Serializable",
                             identifier_native(
                                     compileTimeDecl->data.compileTimeDeclaration.declaration->data.classDeclaration.name));

    decoratedClassDecl = get_script_statement(ast, 1);
    TEST_ASSERT_NOT_NULL(decoratedClassDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, decoratedClassDecl->type);
    TEST_ASSERT_EQUAL_STRING("User", identifier_native(decoratedClassDecl->data.classDeclaration.name));
    TEST_ASSERT_NOT_NULL(decoratedClassDecl->data.classDeclaration.decorators);
    TEST_ASSERT_EQUAL_INT(1, (int)decoratedClassDecl->data.classDeclaration.decorators->count);

    decoratorNode = decoratedClassDecl->data.classDeclaration.decorators->nodes[0];
    TEST_ASSERT_NOT_NULL(decoratorNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DECORATOR_EXPRESSION, decoratorNode->type);
    expr = decoratorNode->data.decoratorExpression.expr;
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expr->type);
    TEST_ASSERT_EQUAL_STRING("Serializable", identifier_node_native(state, expr));

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

void test_compile_time_public_class_decorator_parsing(void) {
    SZrExternParserTestTimer timer;
    const char *testSummary = "Compile Time Public Class Decorator Parsing";
    const char *source =
            "%compileTime class Serializable {\n"
            "    @decorate(target: %type Class): DecoratorPatch {\n"
            "        return { metadata: { serializable: true } };\n"
            "    }\n"
            "}\n"
            "\n"
            "#Serializable#\n"
            "pub class User {\n"
            "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *decoratedClassDecl;
    SZrAstNode *decoratorNode;
    SZrAstNode *expr;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "compile_time_public_class_decorator.zr", 38);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

    decoratedClassDecl = get_script_statement(ast, 1);
    TEST_ASSERT_NOT_NULL(decoratedClassDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, decoratedClassDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, decoratedClassDecl->data.classDeclaration.accessModifier);
    TEST_ASSERT_EQUAL_STRING("User", identifier_native(decoratedClassDecl->data.classDeclaration.name));
    TEST_ASSERT_NOT_NULL(decoratedClassDecl->data.classDeclaration.decorators);
    TEST_ASSERT_EQUAL_INT(1, (int)decoratedClassDecl->data.classDeclaration.decorators->count);

    decoratorNode = decoratedClassDecl->data.classDeclaration.decorators->nodes[0];
    TEST_ASSERT_NOT_NULL(decoratorNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DECORATOR_EXPRESSION, decoratorNode->type);
    expr = decoratorNode->data.decoratorExpression.expr;
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expr->type);
    TEST_ASSERT_EQUAL_STRING("Serializable", identifier_node_native(state, expr));

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

void test_compile_time_struct_decorator_parsing(void) {
    SZrExternParserTestTimer timer;
    const char *testSummary = "Compile Time Struct Decorator Parsing";
    const char *source =
            "%compileTime struct Packed {\n"
            "    @decorate(target: %type Struct): DecoratorPatch {\n"
            "        return { metadata: { packed: true } };\n"
            "    }\n"
            "}\n"
            "\n"
            "#Packed#\n"
            "struct Packet {\n"
            "    var id: int = 1;\n"
            "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *compileTimeDecl;
    SZrAstNode *decoratedStructDecl;
    SZrAstNode *decoratorNode;
    SZrAstNode *expr;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "compile_time_struct_decorator.zr", 32);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

    compileTimeDecl = get_script_statement(ast, 0);
    TEST_ASSERT_NOT_NULL(compileTimeDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, compileTimeDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_COMPILE_TIME_STRUCT, compileTimeDecl->data.compileTimeDeclaration.declarationType);
    TEST_ASSERT_NOT_NULL(compileTimeDecl->data.compileTimeDeclaration.declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, compileTimeDecl->data.compileTimeDeclaration.declaration->type);
    TEST_ASSERT_EQUAL_STRING("Packed",
                             identifier_native(
                                     compileTimeDecl->data.compileTimeDeclaration.declaration->data.structDeclaration.name));

    decoratedStructDecl = get_script_statement(ast, 1);
    TEST_ASSERT_NOT_NULL(decoratedStructDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, decoratedStructDecl->type);
    TEST_ASSERT_EQUAL_STRING("Packet", identifier_native(decoratedStructDecl->data.structDeclaration.name));
    TEST_ASSERT_NOT_NULL(decoratedStructDecl->data.structDeclaration.decorators);
    TEST_ASSERT_EQUAL_INT(1, (int)decoratedStructDecl->data.structDeclaration.decorators->count);

    decoratorNode = decoratedStructDecl->data.structDeclaration.decorators->nodes[0];
    TEST_ASSERT_NOT_NULL(decoratorNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DECORATOR_EXPRESSION, decoratorNode->type);
    expr = decoratorNode->data.decoratorExpression.expr;
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expr->type);
    TEST_ASSERT_EQUAL_STRING("Packed", identifier_node_native(state, expr));

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

void test_compile_time_function_decorator_parsing(void) {
    SZrExternParserTestTimer timer;
    const char *testSummary = "Compile Time Function Decorator Parsing";
    const char *source =
            "%compileTime decorate(target: %type Class, version: int = 7): DecoratorPatch {\n"
            "    return { metadata: { version: version } };\n"
            "}\n"
            "\n"
            "#decorate(version: 11)#\n"
            "class User {\n"
            "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *compileTimeDecl;
    SZrAstNode *decoratedClassDecl;
    SZrAstNode *decoratorNode;
    SZrAstNode *expr;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "compile_time_function_decorator.zr", 34);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

    compileTimeDecl = get_script_statement(ast, 0);
    TEST_ASSERT_NOT_NULL(compileTimeDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, compileTimeDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_COMPILE_TIME_FUNCTION, compileTimeDecl->data.compileTimeDeclaration.declarationType);
    TEST_ASSERT_NOT_NULL(compileTimeDecl->data.compileTimeDeclaration.declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, compileTimeDecl->data.compileTimeDeclaration.declaration->type);
    TEST_ASSERT_EQUAL_STRING("decorate",
                             identifier_native(
                                     compileTimeDecl->data.compileTimeDeclaration.declaration->data.functionDeclaration.name));

    decoratedClassDecl = get_script_statement(ast, 1);
    TEST_ASSERT_NOT_NULL(decoratedClassDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, decoratedClassDecl->type);
    TEST_ASSERT_EQUAL_STRING("User", identifier_native(decoratedClassDecl->data.classDeclaration.name));
    TEST_ASSERT_NOT_NULL(decoratedClassDecl->data.classDeclaration.decorators);
    TEST_ASSERT_EQUAL_INT(1, (int)decoratedClassDecl->data.classDeclaration.decorators->count);

    decoratorNode = decoratedClassDecl->data.classDeclaration.decorators->nodes[0];
    TEST_ASSERT_NOT_NULL(decoratorNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DECORATOR_EXPRESSION, decoratorNode->type);
    expr = decoratorNode->data.decoratorExpression.expr;
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expr->type);
    TEST_ASSERT_NOT_NULL(expr->data.primaryExpression.property);
    TEST_ASSERT_EQUAL_STRING("decorate",
                             identifier_node_native(state, expr->data.primaryExpression.property));
    TEST_ASSERT_NOT_NULL(expr->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_INT(1, (int)expr->data.primaryExpression.members->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, expr->data.primaryExpression.members->nodes[0]->type);

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}
