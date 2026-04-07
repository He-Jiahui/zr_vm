#include <string.h>
#include <time.h>

#include "unity.h"

#include "container_test_common.h"
#include "test_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                      \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                   \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

static SZrAstNode *parse_test_ast(SZrState *state, const char *path, const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || path == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)path, strlen(path));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Parse(state, source, strlen(source), sourceName);
}

static char *read_reference_file(const char *relativePath, size_t *outSize) {
    return ZrTests_Reference_ReadFixture(relativePath, outSize);
}

static void test_container_type_inference_fixed_array_length_identity_and_mismatch(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Fixed Array Length Identity And Mismatch";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *okAst;
    SZrAstNode *errorAst;
    const char *okSource =
            "var xs: int[4] = [1, 2, 3, 4];\n"
            "xs[2];\n";
    const char *errorSource =
            "var xs: int[4] = [1, 2, 3];\n";
    SZrInferredType result;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    okAst = parse_test_ast(state, "container_fixed_array_type_test.zr", okSource);
    TEST_ASSERT_NOT_NULL(okAst);
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(cs, okAst->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   okAst->data.script.statements->nodes[1]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    ZrParser_InferredType_Free(state, &result);
    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, okAst);

    errorAst = parse_test_ast(state, "container_fixed_array_mismatch_type_test.zr", errorSource);
    TEST_ASSERT_NOT_NULL(errorAst);
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(cs, errorAst->data.script.statements->nodes[0]);
    TEST_ASSERT_TRUE(cs->hasError);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, errorAst);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_fixed_arrays_satisfy_arraylike_and_iterable_constraints(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Fixed Arrays Satisfy ArrayLike And Iterable Constraints";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "class NeedsArrayLike<T> where T: ArrayLike<int> { var value: T; }\n"
            "class NeedsIterable<T> where T: Iterable<int> { var value: T; }\n"
            "new NeedsArrayLike<int[3]>();\n"
            "new NeedsIterable<int[3]>();\n";
    SZrInferredType result;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_test_ast(state, "container_fixed_array_constraint_type_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[2]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("NeedsArrayLike<int[3]>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[3]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("NeedsIterable<int[3]>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_fixed_array_assigns_to_unsized_array_annotation(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Fixed Array Assigns To Unsized Array Annotation";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "var xs: int[] = [1, 2, 3];\n"
            "xs[1];\n";
    SZrInferredType result;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_test_ast(state, "container_fixed_array_unsized_assignment_type_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[1]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_native_generic_constraints_accept_pair_and_reject_plain_source_type(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Native Generic Constraints Accept Pair And Reject Plain Source Type";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *okAst;
    SZrAstNode *errorAst;
    const char *okSource =
            "var container = %import(\"zr.container\");\n"
            "var {Pair} = %import(\"zr.container\");\n"
            "new container.Map<Pair<int, string>, int>();\n"
            "new container.Set<Pair<int, string>>();\n";
    const char *errorSource =
            "class Device { }\n"
            "var container = %import(\"zr.container\");\n"
            "new container.Map<Device, int>();\n";
    SZrInferredType result;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    okAst = parse_test_ast(state, "container_generic_constraint_ok_type_test.zr", okSource);
    TEST_ASSERT_NOT_NULL(okAst);
    cs->scriptAst = okAst;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    ZrContainerTests_CompileTopLevelStatement(cs, okAst->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, okAst->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, okAst->data.script.statements->nodes[2]->data.expressionStatement.expr, &result));
    TEST_ASSERT_EQUAL_STRING("Map<Pair<int, string>, int>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);
    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, okAst->data.script.statements->nodes[3]->data.expressionStatement.expr, &result));
    TEST_ASSERT_EQUAL_STRING("Set<Pair<int, string>>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);
    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, okAst);

    errorAst = parse_test_ast(state, "container_generic_constraint_error_type_test.zr", errorSource);
    TEST_ASSERT_NOT_NULL(errorAst);
    cs->scriptAst = errorAst;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(cs, errorAst->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, errorAst->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, errorAst->data.script.statements->nodes[2]->data.expressionStatement.expr, &result));
    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Device"));
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Hashable"));
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, errorAst);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_computed_access_and_native_method_signatures_flow_types(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Computed Access And Native Method Signatures Flow Types";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Map, LinkedList} = %import(\"zr.container\");\n"
            "var xs: Array<int> = new container.Array<int>();\n"
            "var map: Map<string, int> = new container.Map<string, int>();\n"
            "var list: LinkedList<int> = new container.LinkedList<int>();\n"
            "xs.insert(0, 1);\n"
            "map.containsKey(\"answer\");\n"
            "list.addLast(1);\n"
            "xs[0];\n"
            "map[\"answer\"];\n";
    SZrInferredType result;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_test_ast(state, "container_method_signature_type_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    for (TZrSize index = 0; index < 5; index++) {
        ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(cs->hasError);
    }

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ast->data.script.statements->nodes[5]->data.expressionStatement.expr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ast->data.script.statements->nodes[6]->data.expressionStatement.expr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ast->data.script.statements->nodes[7]->data.expressionStatement.expr, &result));
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("LinkedNode<int>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ast->data.script.statements->nodes[8]->data.expressionStatement.expr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ast->data.script.statements->nodes[9]->data.expressionStatement.expr, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_typed_function_returns_preserve_native_container_method_types(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Typed Function Returns Preserve Native Container Method Types";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {LinkedList} = %import(\"zr.container\");\n"
            "buildList(): LinkedList<int> {\n"
            "    var list: LinkedList<int> = new container.LinkedList<int>();\n"
            "    return list;\n"
            "}\n"
            "var list: LinkedList<int> = buildList();\n"
            "list.addLast(1);\n"
            "list.removeFirst();\n";
    SZrInferredType result;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_test_ast(state, "container_typed_function_return_type_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    for (TZrSize index = 0; index < 4; index++) {
        ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(cs->hasError);
    }

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[4]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("LinkedNode<int>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[5]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_requires_explicit_binding_for_imported_type_names(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Imported Type Names Require Explicit Binding";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var xs: Array<int> = new container.Array<int>();\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_test_ast(state, "container_explicit_type_binding_error_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Unqualified type name 'Array' requires an explicit module qualifier or destructuring import"));

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_accepts_qualified_and_destructured_imported_type_names(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Qualified And Destructured Imported Type Names";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var xs: container.Array<int> = new container.Array<int>();\n"
            "var {Map, Set} = %import(\"zr.container\");\n"
            "var map: Map<string, int> = new container.Map<string, int>();\n"
            "var values: Set<int> = new container.Set<int>();\n"
            "xs[0];\n";
    SZrInferredType result;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    ast = parse_test_ast(state, "container_explicit_type_binding_ok_test.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    for (TZrSize index = 0; index < 5; index++) {
        ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(cs->hasError);
    }

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[5]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_type_inference_rejects_invalid_computed_access_on_set_and_linked_list(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Type Inference - Rejects Invalid Computed Access On Set And LinkedList";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *setAst;
    SZrAstNode *listAst;
    SZrInferredType result;
    const char *setSource =
            "var container = %import(\"zr.container\");\n"
            "var {Set} = %import(\"zr.container\");\n"
            "var values: Set<int> = new container.Set<int>();\n"
            "values[0];\n";
    const char *listSource =
            "var container = %import(\"zr.container\");\n"
            "var {LinkedList} = %import(\"zr.container\");\n"
            "var values: LinkedList<int> = new container.LinkedList<int>();\n"
            "values[0];\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    setAst = parse_test_ast(state, "container_set_invalid_index_type_test.zr", setSource);
    TEST_ASSERT_NOT_NULL(setAst);
    cs->scriptAst = setAst;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(cs, setAst->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, setAst->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, setAst->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs,
                                                    setAst->data.script.statements->nodes[3]->data.expressionStatement.expr,
                                                    &result));
    TEST_ASSERT_TRUE(cs->hasError);
    ZrParser_InferredType_Free(state, &result);
    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, setAst);

    cs->hasError = ZR_FALSE;
    cs->errorMessage = ZR_NULL;

    listAst = parse_test_ast(state, "container_list_invalid_index_type_test.zr", listSource);
    TEST_ASSERT_NOT_NULL(listAst);
    cs->scriptAst = listAst;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);
    ZrContainerTests_CompileTopLevelStatement(cs, listAst->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, listAst->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, listAst->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs,
                                                    listAst->data.script.statements->nodes[3]->data.expressionStatement.expr,
                                                    &result));
    TEST_ASSERT_TRUE(cs->hasError);
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, listAst);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_protocols_arraylike_indexable_fixture_binds_protocols_and_index_types(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Protocol Fixture - ArrayLike Iterable And Indexing Compose";
    SZrState *state;
    SZrCompilerState *cs;
    SZrAstNode *ast;
    SZrInferredType result;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    cs = ZrContainerTests_CreateCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    source = read_reference_file("core_semantics/protocols_iteration_comparable/arraylike_indexable_combination.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    ast = parse_test_ast(state, "reference_arraylike_indexable_combination.zr", source);
    TEST_ASSERT_NOT_NULL(ast);

    cs->scriptAst = ast;
    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);
    ZrContainerTests_CompileTopLevelStatement(cs, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE(cs->hasError);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[3]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("NeedsArrayLike<int[3]>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[4]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_NOT_NULL(result.typeName);
    TEST_ASSERT_EQUAL_STRING("NeedsIterable<int[3]>", ZrCore_String_GetNativeString(result.typeName));
    ZrParser_InferredType_Free(state, &result);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs,
                                                   ast->data.script.statements->nodes[5]->data.expressionStatement.expr,
                                                   &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    free(source);
    ZrContainerTests_DestroyCompilerState(cs);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_container_type_inference_fixed_array_length_identity_and_mismatch);
    RUN_TEST(test_container_type_inference_fixed_arrays_satisfy_arraylike_and_iterable_constraints);
    RUN_TEST(test_container_type_inference_fixed_array_assigns_to_unsized_array_annotation);
    RUN_TEST(test_container_type_inference_native_generic_constraints_accept_pair_and_reject_plain_source_type);
    RUN_TEST(test_container_type_inference_computed_access_and_native_method_signatures_flow_types);
    RUN_TEST(test_container_type_inference_typed_function_returns_preserve_native_container_method_types);
    RUN_TEST(test_container_type_inference_requires_explicit_binding_for_imported_type_names);
    RUN_TEST(test_container_type_inference_accepts_qualified_and_destructured_imported_type_names);
    RUN_TEST(test_container_type_inference_rejects_invalid_computed_access_on_set_and_linked_list);
    RUN_TEST(test_reference_protocols_arraylike_indexable_fixture_binds_protocols_and_index_types);
    return UNITY_END();
}
