#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

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

static const SZrFunction *compiled_child_function_at(SZrCompilerState *compiler,
                                                      TZrSize index) {
    SZrFunction *const *child;

    if (compiler == ZR_NULL || index >= compiler->childFunctions.length) {
        return ZR_NULL;
    }
    child = (SZrFunction *const *) ZrCore_Array_Get(&compiler->childFunctions, index);
    return child != ZR_NULL ? *child : ZR_NULL;
}

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

static void test_yield_requires_canonical_iterator_carrier_and_projects_facts(void) {
    static const TZrChar source[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn values(limit: int): zr.iteration.Iterator<int> {\n"
            "    yield limit;\n"
            "}\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *ast;
    const SZrFunction *compiledFunction;
    const SZrSemanticIrInstruction *yieldValue;
    const SZrSemanticIrInstruction *yieldSuspend;
    const SZrSemanticIrInstruction *yieldResume;
    const TZrChar *yieldSource;

    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, source, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_FALSE(compiler->hasError);
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(
            &compiler->preSemanticIr, ZR_SEMANTIC_IR_YIELD_VALUE));
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(
            &compiler->preSemanticIr, ZR_SEMANTIC_IR_YIELD_SUSPEND));
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(
            &compiler->preSemanticIr, ZR_SEMANTIC_IR_YIELD_RESUME));
    TEST_ASSERT_EQUAL_UINT64(
            1U,
            semantic_ir_opcode_count(
                    &compiler->preSemanticIr, ZR_SEMANTIC_IR_ITERATOR_COMPLETE));
    TEST_ASSERT_EQUAL_UINT64(1U, compiler->childFunctions.length);
    compiledFunction = compiled_child_function_at(compiler, 0U);
    TEST_ASSERT_NOT_NULL(compiledFunction);
    TEST_ASSERT_EQUAL_UINT32(0U, compiledFunction->instructionsLength);
    yieldValue = semantic_ir_first_opcode(
            &compiler->preSemanticIr, ZR_SEMANTIC_IR_YIELD_VALUE);
    yieldSuspend = semantic_ir_first_opcode(
            &compiler->preSemanticIr, ZR_SEMANTIC_IR_YIELD_SUSPEND);
    yieldResume = semantic_ir_first_opcode(
            &compiler->preSemanticIr, ZR_SEMANTIC_IR_YIELD_RESUME);
    yieldSource = strstr(source, "yield limit;");
    TEST_ASSERT_NOT_NULL(yieldValue);
    TEST_ASSERT_NOT_NULL(yieldSuspend);
    TEST_ASSERT_NOT_NULL(yieldResume);
    TEST_ASSERT_NOT_NULL(yieldSource);
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32) (yieldSource - source), yieldValue->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(
            yieldValue->sourceRange.start.offset, yieldSuspend->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(
            yieldValue->sourceRange.start.offset, yieldResume->sourceRange.start.offset);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
}

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

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, emptyReturnSource, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_FALSE(compiler->hasError);
    TEST_ASSERT_EQUAL_UINT64(
            1U,
            semantic_ir_opcode_count(
                    &compiler->preSemanticIr, ZR_SEMANTIC_IR_ITERATOR_COMPLETE));
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, valueReturnSource, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_TRUE(compiler->hasError);
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
}

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

static void test_nested_iterator_yield_does_not_reclassify_the_outer_function(void) {
    static const TZrChar source[] =
            "var iteration = import(\"zr.iteration\");\n"
            "fn outer(): void {\n"
            "    fn inner(): zr.iteration.Iterator<int> { yield 1; }\n"
            "}\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    compiler = compile_source(state, source, &ast);
    TEST_ASSERT_NOT_NULL(compiler);
    TEST_ASSERT_FALSE(compiler->hasError);
    TEST_ASSERT_EQUAL_UINT64(
            1U,
            semantic_ir_opcode_count(
                    &compiler->preSemanticIr, ZR_SEMANTIC_IR_ITERATOR_COMPLETE));
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
}

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
    return UNITY_END();
}
