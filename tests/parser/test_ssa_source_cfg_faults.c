#include "unity.h"
#include "ssa_source_cfg_faults.h"

#include <string.h>
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

void setUp(void) {
    ssa_source_cfg_fail_allocation(0u);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ssa_source_cfg_fail_allocation(0u);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT64(0u, ssa_source_cfg_outstanding_allocations());
}

static SZrAstNode *compile_many_statements(SZrCompilerState *compiler) {
    char source[4096];
    const char *prefix = "var value: int = 7;\n";
    const char *read = "value;\n";
    size_t length = strlen(prefix), index;
    SZrString *name;
    SZrAstNode *ast;
    memcpy(source, prefix, length);
    for (index = 0u; index < 256u; ++index) {
        memcpy(source + length, read, strlen(read));
        length += strlen(read);
    }
    source[length] = '\0';
    name = ZrCore_String_CreateFromNative(g_state, "ssa_cfg_faults.zr");
    ast = ZrParser_Parse(g_state, source, length, name);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    for (index = 0u; index < ast->data.script.statements->count; ++index)
        ZrParser_Statement_Compile(compiler, ast->data.script.statements->nodes[index]);
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, compiler->errorMessage);
    TEST_ASSERT_FALSE(compiler->preSemanticIrCfgActive);
    return ast;
}

static void free_source(SZrCompilerState *compiler, SZrAstNode *ast) {
    ZrCore_Function_Free(g_state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void assert_scratch_failures(TZrBool startWithAnalysisGraph) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_many_statements(&compiler);
    SZrSemanticIrFunction before;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 oldBlock, oldStart;
    size_t ordinal, failures = 0u;
    TZrBool completed = ZR_FALSE;
    if (startWithAnalysisGraph) {
        compiler.preSemanticIrCfgStartupSuppressed = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
        compiler.preSemanticIrCfgStartupSuppressed = ZR_FALSE;
        TEST_ASSERT_EQUAL_UINT64(2u, compiler.preSemanticIr.cfg.blocks.length);
    }
    before = compiler.preSemanticIr;
    oldBlock = compiler.preSemanticIrCfgBlock;
    oldStart = compiler.preSemanticIrCfgStart;
    for (ordinal = 1u; ordinal <= 32u; ++ordinal) {
        TZrBool finalized;
        ssa_source_cfg_fail_allocation(ordinal);
        finalized = ssa_source_cfg_finalize(&compiler);
        TEST_ASSERT_EQUAL_UINT64(0u, ssa_source_cfg_outstanding_allocations());
        if (ssa_source_cfg_allocation_failed()) {
            ++failures;
            TEST_ASSERT_FALSE(finalized);
            TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
            TEST_ASSERT_EQUAL_MEMORY(&before, &compiler.preSemanticIr, sizeof(before));
            TEST_ASSERT_EQUAL_UINT32(oldBlock, compiler.preSemanticIrCfgBlock);
            TEST_ASSERT_EQUAL_UINT32(oldStart, compiler.preSemanticIrCfgStart);
        } else {
            TEST_ASSERT_TRUE(finalized);
            completed = ZR_TRUE;
            break;
        }
    }
    ssa_source_cfg_fail_allocation(0u);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_GREATER_THAN_UINT64(1u, failures);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_EQUAL_UINT64(before.instructions.length + 2u,
                            compiler.preSemanticIr.instructions.length);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(&compiler.preSemanticIr, ZR_NULL,
                                         &output, &diagnostic));
    ZrCore_ExecIr_FreeFunction(&output);
    /* A sealed graph needs no preflight scratch allocation. */
    before = compiler.preSemanticIr;
    ssa_source_cfg_fail_allocation(1u);
    TEST_ASSERT_TRUE(ssa_source_cfg_finalize(&compiler));
    TEST_ASSERT_FALSE(ssa_source_cfg_allocation_failed());
    TEST_ASSERT_EQUAL_MEMORY(&before, &compiler.preSemanticIr, sizeof(before));
    ssa_source_cfg_fail_allocation(0u);
    free_source(&compiler, ast);
}

static void test_initial_and_growth_failures_preserve_unpublished_graph(void) {
    assert_scratch_failures(ZR_FALSE);
}

static void test_scratch_failure_preserves_existing_analysis_graph(void) {
    assert_scratch_failures(ZR_TRUE);
}

static void test_invalid_compiler_is_rejected_without_allocation(void) {
    SZrCompilerState uninitialized = {0};
    ssa_source_cfg_fail_allocation(1u);
    TEST_ASSERT_FALSE(ssa_source_cfg_finalize(ZR_NULL));
    TEST_ASSERT_FALSE(ssa_source_cfg_finalize(&uninitialized));
    TEST_ASSERT_FALSE(ssa_source_cfg_allocation_failed());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_and_growth_failures_preserve_unpublished_graph);
    RUN_TEST(test_scratch_failure_preserves_existing_analysis_graph);
    RUN_TEST(test_invalid_compiler_is_rejected_without_allocation);
    return UNITY_END();
}
