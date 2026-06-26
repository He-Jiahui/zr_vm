#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "test_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *generic_constraints_create_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

static SZrCompilerState *generic_constraints_create_compiler_state(SZrState *state) {
    SZrCompilerState *cs;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_CompilerState_Init(cs, state);
    return cs;
}

static void generic_constraints_destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(cs->state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }

    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(cs->state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static void generic_constraints_ensure_root_scope(SZrCompilerState *cs) {
    SZrScope scope;

    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->state);

    if (cs->scopeStack.length != 0) {
        return;
    }

    memset(&scope, 0, sizeof(scope));
    scope.startVarIndex = cs->localVarCount;
    scope.parentCompiler = cs->currentFunction != ZR_NULL ? cs : ZR_NULL;
    ZrCore_Array_Push(cs->state, &cs->scopeStack, &scope);
}

static void generic_constraints_compile_top_level_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(node);

    if (cs->currentFunction == ZR_NULL) {
        cs->currentFunction = ZrCore_Function_New(cs->state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
    }

    cs->isScriptLevel = ZR_TRUE;
    generic_constraints_ensure_root_scope(cs);

    switch (node->type) {
        case ZR_AST_INTERFACE_DECLARATION:
            ZrParser_Compiler_CompileInterfaceDeclaration(cs, node);
            break;
        case ZR_AST_CLASS_DECLARATION:
            ZrParser_Compiler_CompileClassDeclaration(cs, node);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            ZrParser_Compiler_CompileStructDeclaration(cs, node);
            break;
        default:
            TEST_FAIL_MESSAGE("expected top-level type declaration");
            break;
    }
}

void setUp(void) {}

void tearDown(void) {}

static void test_generic_new_constraint_accepts_constructible_and_rejects_interface(void) {
    SZrState *state = generic_constraints_create_state();
    SZrCompilerState *cs = generic_constraints_create_compiler_state(state);
    const char *source =
            "interface AbstractThing { }\n"
            "class DefaultCtor { }\n"
            "class NeedNew<T> where T: new() { var value: T; }\n"
            "new NeedNew<DefaultCtor>();\n"
            "new NeedNew<AbstractThing>();";
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *successExpr;
    SZrAstNode *failureExpr;
    SZrInferredType result;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);

    sourceName = ZrCore_String_CreateFromNative(state, "generic_new_constraint_test.zr");
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(5u, ast->data.script.statements->count);
    cs->scriptAst = ast;

    generic_constraints_compile_top_level_declaration(cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    generic_constraints_compile_top_level_declaration(cs, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);
    generic_constraints_compile_top_level_declaration(cs, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE(cs->hasError);

    successExpr = ast->data.script.statements->nodes[3]->data.expressionStatement.expr;
    failureExpr = ast->data.script.statements->nodes[4]->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(successExpr);
    TEST_ASSERT_NOT_NULL(failureExpr);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, successExpr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("NeedNew<DefaultCtor>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, failureExpr, &result));
    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "new() constraint"));
    ZrParser_InferredType_Free(state, &result);

    ZrParser_Ast_Free(state, ast);
    generic_constraints_destroy_compiler_state(cs);
    ZrTests_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generic_new_constraint_accepts_constructible_and_rejects_interface);
    return UNITY_END();
}
