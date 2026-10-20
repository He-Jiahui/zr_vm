#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

/* 读取编译后的语义 IR，确认 yield 分阶段事实实际被发射。 */
static TZrBool semantic_ir_has_opcode(const SZrSemanticIrFunction *function,
                                      EZrSemanticIrOpcode opcode) {
    TZrSize index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        if (instruction != ZR_NULL && instruction->opcode == opcode) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 对特定 opcode 计数，防止重复插入暂停或恢复事实。 */
static TZrSize semantic_ir_opcode_count(const SZrSemanticIrFunction *function,
                                        EZrSemanticIrOpcode opcode) {
    TZrSize count = 0U;
    TZrSize index;

    if (function == ZR_NULL) {
        return 0U;
    }
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        if (instruction != ZR_NULL && instruction->opcode == opcode) {
            count++;
        }
    }
    return count;
}

/* 返回所属 IR 内的只读指令，调用方需保持编译器存活。 */
static const SZrSemanticIrInstruction *semantic_ir_first_opcode(
        const SZrSemanticIrFunction *function,
        EZrSemanticIrOpcode opcode) {
    TZrSize index;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        if (instruction != ZR_NULL && instruction->opcode == opcode) {
            return instruction;
        }
    }
    return ZR_NULL;
}

/* 从顶层编译器读取函数产物，索引对应源代码声明次序。 */
static const SZrFunction *compiled_child_function_at(SZrCompilerState *compiler,
                                                      TZrSize index) {
    SZrFunction *const *child;

    if (compiler == ZR_NULL || index >= compiler->childFunctions.length) {
        return ZR_NULL;
    }
    child = (SZrFunction *const *) ZrCore_Array_Get(&compiler->childFunctions, index);
    return child != ZR_NULL ? *child : ZR_NULL;
}

/* 经 parser 和逐语句编译走真实 yield 降低链路；outAst 由调用方释放。 */
static SZrCompilerState *compile_source(SZrState *state,
                                        const TZrChar *source,
                                        SZrAstNode **outAst) {
    SZrCompilerState *compiler;
    SZrString *sourceName;
    SZrAstNode *ast;
    TZrSize index;

    if (outAst != ZR_NULL) {
        *outAst = ZR_NULL;
    }
    sourceName = ZrCore_String_CreateFromNative(state, "yield_semantic_test.zr");
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    compiler = ZrContainerTests_CreateCompilerState(state);
    if (compiler == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        return ZR_NULL;
    }
    compiler->scriptAst = ast;
    compiler->currentFunction = ZrCore_Function_New(state);
    if (compiler->currentFunction == ZR_NULL) {
        ZrContainerTests_DestroyCompilerState(compiler);
        ZrParser_Ast_Free(state, ast);
        return ZR_NULL;
    }
    for (index = 0U; index < ast->data.script.statements->count && !compiler->hasError; index++) {
        ZrContainerTests_CompileTopLevelStatement(
                compiler, ast->data.script.statements->nodes[index]);
    }
    if (outAst != ZR_NULL) {
        *outAst = ast;
    }
    return compiler;
}

/* 先释放编译函数和编译器，再释放 AST；调用方随后销毁 state。 */
static void destroy_compilation(SZrState *state,
                                SZrCompilerState *compiler,
                                SZrAstNode *ast) {
    if (compiler != ZR_NULL && compiler->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(state, compiler->currentFunction);
        compiler->currentFunction = ZR_NULL;
    }
    ZrContainerTests_DestroyCompilerState(compiler);
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
}

#include "yield_function_body_cases.inc"

/* 显式 zr.iteration.Iterator<T> 返回载体使 yield 投影三阶段语义事实。 */
static void test_yield_requires_canonical_iterator_carrier_and_projects_facts(void) {
    static const TZrChar source[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn values(limit: int): zr.iteration.Iterator<int> {\n"
            "    yield limit;\n"
            "}\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrYieldFunctionBodyFacts facts;
    TZrBool stateCreated = (state != ZR_NULL), compiled = ZR_FALSE, childExecBcEmpty = ZR_FALSE;
    TZrSize childCount = 0U;
    const TZrChar *yieldSource = strstr(source, "yield limit;");

    if (state != ZR_NULL) {
        compiler = compile_source(state, source, &ast);
    }
    if (compiler != ZR_NULL) {
        const SZrFunction *child = compiled_child_function_at(compiler, 0U);
        compiled = !compiler->hasError;
        childCount = compiler->childFunctions.length;
        childExecBcEmpty = (child != ZR_NULL && child->instructionsLength == 0U &&
                            child->instructionsList == ZR_NULL);
    }
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
    facts = observe_yield_function_body(source, ZR_FALSE);

    TEST_ASSERT_TRUE(stateCreated);
    TEST_ASSERT_TRUE(compiled);
    TEST_ASSERT_EQUAL_UINT64(1U, childCount);
    TEST_ASSERT_TRUE(childExecBcEmpty);
    TEST_ASSERT_TRUE(facts.prepared);
    TEST_ASSERT_TRUE(facts.compiled);
    TEST_ASSERT_TRUE(facts.scopesRestored);
    TEST_ASSERT_TRUE(facts.execBcEmpty);
    TEST_ASSERT_TRUE(facts.rangesPresent);
    for (TZrSize index = 0U; index < 4U; index++) {
        TEST_ASSERT_EQUAL_UINT64(1U, facts.counts[index]);
    }
    TEST_ASSERT_NOT_NULL(yieldSource);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)(yieldSource - source), facts.offsets[0]);
    TEST_ASSERT_EQUAL_UINT32(facts.offsets[0], facts.offsets[1]);
    TEST_ASSERT_EQUAL_UINT32(facts.offsets[0], facts.offsets[2]);
}

/* Iterable<T> 不是同步 Iterator<T> 载体，元素类型亦须兼容。 */
static void test_yield_rejects_iterable_carrier_and_incompatible_payload(void) {
    static const TZrChar iterableSource[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn values(limit: int): zr.iteration.Iterable<int> { yield limit; }\n";
    static const TZrChar incompatiblePayloadSource[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn values(): zr.iteration.Iterator<int> { yield \"not an int\"; }\n";
    SZrState *state;
    SZrCompilerState *compiler;
    SZrAstNode *ast;

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, iterableSource, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_TRUE(compiler->hasError);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, incompatiblePayloadSource, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_TRUE(compiler->hasError);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
}

/* 产值靠 yield，显式 return 只能表示无值完成。 */
static void test_yield_function_allows_only_empty_return_completion(void) {
    static const TZrChar emptyReturnSource[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn values(): zr.iteration.Iterator<int> { yield 1; return; }\n";
    static const TZrChar valueReturnSource[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn values(): zr.iteration.Iterator<int> { yield 1; return 2; }\n";
    SZrState *state;
    SZrCompilerState *compiler;
    SZrAstNode *ast;
    SZrYieldFunctionBodyFacts facts;
    TZrBool stateCreated, compiled;

    state = ZrContainerTests_CreateState();
    stateCreated = (state != ZR_NULL);
    ast = ZR_NULL;
    compiler = state != ZR_NULL ? compile_source(state, emptyReturnSource, &ast) : ZR_NULL;
    compiled = (compiler != ZR_NULL && !compiler->hasError);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
    facts = observe_yield_function_body(emptyReturnSource, ZR_FALSE);

    TEST_ASSERT_TRUE(stateCreated);
    TEST_ASSERT_TRUE(compiled);
    TEST_ASSERT_TRUE(facts.prepared);
    TEST_ASSERT_TRUE(facts.compiled);
    TEST_ASSERT_TRUE(facts.scopesRestored);
    TEST_ASSERT_TRUE(facts.execBcEmpty);
    TEST_ASSERT_TRUE(facts.rangesPresent);
    for (TZrSize index = 0U; index < 4U; index++) {
        TEST_ASSERT_EQUAL_UINT64(1U, facts.counts[index]);
    }
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)(strstr(emptyReturnSource, "yield 1;") - emptyReturnSource), facts.offsets[0]);
    TEST_ASSERT_EQUAL_UINT32(facts.offsets[0], facts.offsets[1]);
    TEST_ASSERT_EQUAL_UINT32(facts.offsets[0], facts.offsets[2]);

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, valueReturnSource, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_TRUE(compiler->hasError);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
}

/* 顶层无 iterator frame，yield 必须被语义阶段拒绝。 */
static void test_yield_rejects_top_level_statement(void) {
    static const TZrChar source[] = "yield 1;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, source, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_TRUE(compiler->hasError);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
}

/* 属性访问器不提供迭代器返回载体，不能借用 yield 语法。 */
static void test_yield_rejects_property_accessor_context(void) {
    static const TZrChar propertyAccessorSource[] =
            "class Box {\n"
            "    pub property value: int { get { yield 1; } }\n"
            "}\n";
    SZrState *state;
    SZrCompilerState *compiler;
    SZrAstNode *ast;

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, propertyAccessorSource, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_TRUE(compiler->hasError);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
}

/* 内层函数的 yield 不得把外层普通函数误分类为迭代器。 */
static void test_nested_iterator_yield_does_not_reclassify_the_outer_function(void) {
    static const TZrChar source[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn outer(): void {\n"
            "    fn inner(): zr.iteration.Iterator<int> { yield 1; }\n"
            "}\n";
    SZrYieldFunctionBodyFacts facts = observe_yield_function_body(source, ZR_TRUE);
    SZrYieldOuterGraphFacts outer = observe_public_outer_yield_graph(source);

    TEST_ASSERT_TRUE(facts.prepared);
    TEST_ASSERT_TRUE(facts.compiled);
    TEST_ASSERT_TRUE(facts.scopesRestored);
    TEST_ASSERT_TRUE(facts.execBcEmpty);
    TEST_ASSERT_TRUE(facts.rangesPresent);
    for (TZrSize index = 0U; index < 4U; index++) {
        TEST_ASSERT_EQUAL_UINT64(1U, facts.counts[index]);
    }
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)(strstr(source, "yield 1;") - source), facts.offsets[0]);
    TEST_ASSERT_EQUAL_UINT32(facts.offsets[0], facts.offsets[1]);
    TEST_ASSERT_EQUAL_UINT32(facts.offsets[0], facts.offsets[2]);
    TEST_ASSERT_TRUE(outer.compiled);
    TEST_ASSERT_TRUE(outer.outerNamed);
    TEST_ASSERT_TRUE(outer.innerNamed);
    TEST_ASSERT_TRUE(outer.outerReturns);
    TEST_ASSERT_TRUE(outer.innerExecBcEmpty);
    TEST_ASSERT_TRUE(outer.graphValid);
}

#include "yield_public_compile_graph_cases.inc"
void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_yield_requires_canonical_iterator_carrier_and_projects_facts);
    RUN_TEST(test_yield_rejects_iterable_carrier_and_incompatible_payload);
    RUN_TEST(test_yield_function_allows_only_empty_return_completion);
    RUN_TEST(test_yield_rejects_top_level_statement);
    RUN_TEST(test_yield_rejects_property_accessor_context);
    RUN_TEST(test_nested_iterator_yield_does_not_reclassify_the_outer_function);
    RUN_TEST(test_yield_public_compile_preserves_zero_execbc_child_graph);
    return UNITY_END();
}
