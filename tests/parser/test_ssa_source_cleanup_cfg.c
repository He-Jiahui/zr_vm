#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

ZR_PARSER_API void ZrParser_Compiler_PredeclareFunctionBindings(
        SZrCompilerState *cs, SZrAstNodeArray *statements);

static SZrState *g_state;

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

static SZrAstNode *compile_source(SZrCompilerState *compiler,
                                  const TZrChar *source,
                                  TZrSize sourceLength,
                                  TZrChar *sourceName) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, sourceName);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, sourceLength, name);
    TZrSize index;

    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    if (compiler->currentFunction == ZR_NULL) {
        return ast;
    }
    ZrParser_Compiler_PredeclareFunctionBindings(
            compiler, ast->data.script.statements);
    for (index = 0U;
         !compiler->hasError &&
         index < ast->data.script.statements->count;
         index++) {
        ZrParser_Statement_Compile(
                compiler, ast->data.script.statements->nodes[index]);
    }
    return ast;
}

static void free_source(SZrCompilerState *compiler, SZrAstNode *ast) {
    if (compiler->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, compiler->currentFunction);
        compiler->currentFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(compiler);
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
    }
}

static const SZrParserCfgBlock *find_block_kind(
        const SZrSemanticIrFunction *function,
        EZrParserCfgBlockKind kind) {
    TZrSize index;

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        if (block != ZR_NULL && block->kind == kind) {
            return block;
        }
    }
    return ZR_NULL;
}

static void assert_single_edge(
        const SZrParserCfgBlock *block,
        EZrParserCfgEdgeKind kind,
        TZrUInt32 target) {
    const SZrParserCfgEdge *edge;

    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_UINT32(1U, block->successorCount);
    edge = ZrParser_Cfg_BlockEdgeAt(block, 0U);
    TEST_ASSERT_NOT_NULL(edge);
    TEST_ASSERT_EQUAL_INT(kind, edge->kind);
    TEST_ASSERT_EQUAL_UINT32(target, edge->toBlockId);
}

static TZrBool block_has_source_line(
        const SZrSemanticIrFunction *function,
        const SZrParserCfgBlock *block,
        TZrInt32 line) {
    TZrUInt32 index;

    for (index = block->firstInstructionIndex;
         index < block->firstInstructionIndex + block->instructionCount;
         index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        if (instruction != ZR_NULL &&
            instruction->sourceRange.start.line == line) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const SZrSemanticIrInstruction *block_tail(
        const SZrSemanticIrFunction *function,
        const SZrParserCfgBlock *block) {
    if (function == ZR_NULL || block == ZR_NULL ||
        block->instructionCount == 0U) {
        return ZR_NULL;
    }
    return ZrParser_SemanticIr_InstructionAt(
            function,
            block->firstInstructionIndex + block->instructionCount - 1U);
}

static const SZrParserCfgBlock *find_block_terminator(
        const SZrSemanticIrFunction *function,
        EZrParserCfgTerminatorKind terminatorKind) {
    TZrSize index;

    for (index = 0U; index < function->cfg.blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, index);
        if (block != ZR_NULL &&
            block->terminatorKind == terminatorKind) {
            return block;
        }
    }
    return ZR_NULL;
}

static void test_linear_try_finally_emits_cleanup_region(void) {
    static const TZrChar source[] =
            "var seed: int = 7;\n"
            "try {\n"
            "  seed = 8;\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "linear_try_finally_cleanup.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *join;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(4U, function->cfg.blocks.length);

    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            function->cfg.entryBlockId);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    join = find_block_kind(function, ZR_PARSER_CFG_BLOCK_JOIN);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(join);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, entry->terminatorKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, cleanup->terminatorKind);
    assert_single_edge(entry, ZR_PARSER_CFG_EDGE_CLEANUP, cleanup->id);
    assert_single_edge(cleanup, ZR_PARSER_CFG_EDGE_CLEANUP, join->id);
    TEST_ASSERT_TRUE(block_has_source_line(function, entry, 3U));
    TEST_ASSERT_FALSE(block_has_source_line(function, cleanup, 3U));
    TEST_ASSERT_TRUE(block_has_source_line(function, cleanup, 5U));
    TEST_ASSERT_TRUE(block_has_source_line(function, join, 7U));

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(
            (output.blocks[cleanup->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    free_source(&compiler, ast);
}

static void test_return_try_finally_preserves_precleanup_value(void) {
    static const TZrChar source[] =
            "var seed: int = 7;\n"
            "try {\n"
            "  return seed;\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n";
    static TZrChar sourceName[] = "return_try_finally_cleanup.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *returnBlock;
    const SZrSemanticIrInstruction *returnInstruction;
    const SZrSemanticIrInstruction *returnValueDefinition;
    const SZrSemanticIrValue *returnValue;
    const TZrValueId *returnOperand;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(3U, function->cfg.blocks.length);

    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            function->cfg.entryBlockId);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    returnBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            function->cfg.exitBlockId);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, entry->terminatorKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, cleanup->terminatorKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_RETURN, returnBlock->terminatorKind);
    assert_single_edge(entry, ZR_PARSER_CFG_EDGE_CLEANUP, cleanup->id);
    assert_single_edge(
            cleanup, ZR_PARSER_CFG_EDGE_CLEANUP, returnBlock->id);
    TEST_ASSERT_TRUE(block_has_source_line(function, entry, 3U));
    TEST_ASSERT_TRUE(block_has_source_line(function, cleanup, 5U));

    returnInstruction = block_tail(function, returnBlock);
    TEST_ASSERT_NOT_NULL(returnInstruction);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_RETURN, returnInstruction->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, returnInstruction->operandCount);
    TEST_ASSERT_EQUAL_INT(3, returnInstruction->sourceRange.start.line);
    returnOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            returnInstruction->operandStart);
    TEST_ASSERT_NOT_NULL(returnOperand);
    returnValue = ZrParser_SemanticIr_Value(function, *returnOperand);
    TEST_ASSERT_NOT_NULL(returnValue);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            returnValue->definitionInstructionId);
    returnValueDefinition = ZrParser_SemanticIr_InstructionAt(
            function, returnValue->definitionInstructionId - 1U);
    TEST_ASSERT_NOT_NULL(returnValueDefinition);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_LOAD, returnValueDefinition->opcode);
    TEST_ASSERT_EQUAL_INT(
            3, returnValueDefinition->sourceRange.start.line);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(
            (output.blocks[cleanup->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    free_source(&compiler, ast);
}

static void test_nonlinear_return_try_finally_stays_on_legacy_path(void) {
    static const TZrChar source[] =
            "var seed: int = 7;\n"
            "try { return seed + 1; } finally { seed = 9; }\n";
    static TZrChar sourceName[] = "nonlinear_return_try_finally_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));

    free_source(&compiler, ast);
}

static void test_throw_try_finally_preserves_precleanup_value(void) {
    static const TZrChar source[] =
            "var seed: int = 7;\n"
            "try {\n"
            "  throw seed;\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n";
    static TZrChar sourceName[] = "throw_try_finally_cleanup.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *entry;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *throwBlock;
    const SZrSemanticIrInstruction *throwInstruction;
    const SZrSemanticIrInstruction *throwValueDefinition;
    const SZrSemanticIrValue *throwValue;
    const TZrValueId *throwOperand;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(3U, function->cfg.blocks.length);

    entry = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            function->cfg.entryBlockId);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    throwBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks,
            function->cfg.exitBlockId);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(throwBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, entry->terminatorKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_BRANCH, cleanup->terminatorKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_THROW, throwBlock->terminatorKind);
    assert_single_edge(entry, ZR_PARSER_CFG_EDGE_CLEANUP, cleanup->id);
    assert_single_edge(
            cleanup, ZR_PARSER_CFG_EDGE_CLEANUP, throwBlock->id);
    TEST_ASSERT_TRUE(block_has_source_line(function, entry, 3U));
    TEST_ASSERT_TRUE(block_has_source_line(function, cleanup, 5U));

    throwInstruction = block_tail(function, throwBlock);
    TEST_ASSERT_NOT_NULL(throwInstruction);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_THROW, throwInstruction->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, throwInstruction->operandCount);
    TEST_ASSERT_EQUAL_INT(3, throwInstruction->sourceRange.start.line);
    throwOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            throwInstruction->operandStart);
    TEST_ASSERT_NOT_NULL(throwOperand);
    throwValue = ZrParser_SemanticIr_Value(function, *throwOperand);
    TEST_ASSERT_NOT_NULL(throwValue);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            throwValue->definitionInstructionId);
    throwValueDefinition = ZrParser_SemanticIr_InstructionAt(
            function, throwValue->definitionInstructionId - 1U);
    TEST_ASSERT_NOT_NULL(throwValueDefinition);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_LOAD, throwValueDefinition->opcode);
    TEST_ASSERT_EQUAL_INT(
            3, throwValueDefinition->sourceRange.start.line);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(
            (output.blocks[cleanup->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0U);
    ZrCore_ExecIr_FreeFunction(&output);

    free_source(&compiler, ast);
}

static void test_nonlinear_throw_try_finally_stays_on_legacy_path(void) {
    static const TZrChar source[] =
            "var seed: int = 7;\n"
            "try { throw seed + 1; } finally { seed = 9; }\n";
    static TZrChar sourceName[] = "nonlinear_throw_try_finally_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));

    free_source(&compiler, ast);
}

static void test_conditional_return_try_finally_dispatches_pending_state(void) {
    static const TZrChar source[] =
            "var choose: bool = true;\n"
            "var seed: int = 7;\n"
            "try {\n"
            "  if (choose) { return seed; }\n"
            "  seed = 8;\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] = "conditional_return_try_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *returnBlock;
    const SZrParserCfgBlock *joinBlock;
    const SZrParserCfgEdge *returnEdge;
    const SZrParserCfgEdge *joinEdge;
    const SZrSemanticIrInstruction *dispatch;
    const SZrSemanticIrInstruction *returnInstruction;
    const SZrSemanticIrInstruction *returnDefinition;
    const SZrSemanticIrValue *returnValue;
    const SZrParserPlace *pendingPlace;
    const TZrValueId *returnOperand;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);

    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    returnBlock = find_block_terminator(
            function, ZR_PARSER_CFG_TERMINATOR_RETURN);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
            cleanup->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->successorCount);
    returnEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 0U);
    joinEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 1U);
    TEST_ASSERT_NOT_NULL(returnEdge);
    TEST_ASSERT_NOT_NULL(joinEdge);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_CASE, returnEdge->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT, joinEdge->kind);
    TEST_ASSERT_EQUAL_UINT32(returnBlock->id, returnEdge->toBlockId);
    joinBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&function->cfg.blocks, joinEdge->toBlockId);
    TEST_ASSERT_NOT_NULL(joinBlock);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, joinBlock->kind);
    TEST_ASSERT_TRUE(block_has_source_line(function, cleanup, 7U));
    TEST_ASSERT_TRUE(block_has_source_line(function, joinBlock, 9U));

    dispatch = block_tail(function, cleanup);
    TEST_ASSERT_NOT_NULL(dispatch);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_SWITCH, dispatch->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, dispatch->operandCount);

    returnInstruction = block_tail(function, returnBlock);
    TEST_ASSERT_NOT_NULL(returnInstruction);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_RETURN, returnInstruction->opcode);
    TEST_ASSERT_EQUAL_UINT32(1U, returnInstruction->operandCount);
    returnOperand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&function->valueOperands,
            returnInstruction->operandStart);
    TEST_ASSERT_NOT_NULL(returnOperand);
    returnValue = ZrParser_SemanticIr_Value(function, *returnOperand);
    TEST_ASSERT_NOT_NULL(returnValue);
    returnDefinition = ZrParser_SemanticIr_InstructionAt(
            function, returnValue->definitionInstructionId - 1U);
    TEST_ASSERT_NOT_NULL(returnDefinition);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_LOAD, returnDefinition->opcode);
    TEST_ASSERT_EQUAL_INT(4, returnDefinition->sourceRange.start.line);
    pendingPlace = ZrParser_PlaceGraph_Get(
            &function->places, returnDefinition->placeId);
    TEST_ASSERT_NOT_NULL(pendingPlace);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_PLACE_BASE_TEMPORARY,
            pendingPlace->base.kind);

    ZrCore_ExecIr_FunctionInit(&output);
    memset(&diagnostic, 0, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            function, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(
            (output.blocks[cleanup->id].flags &
             ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0U);
    TEST_ASSERT_EQUAL_INT(
            ZR_EXEC_IR_OPCODE_SWITCH,
            output.instructions[output.blocks[cleanup->id]
                                        .instructionRange.start +
                                output.blocks[cleanup->id]
                                        .instructionRange.count - 1U]
                    .opcode);
    ZrCore_ExecIr_FreeFunction(&output);

    free_source(&compiler, ast);
}

static void test_two_return_sites_try_finally_stays_on_legacy_path(void) {
    static const TZrChar source[] =
            "var choose: bool = true;\n"
            "var seed: int = 7;\n"
            "try {\n"
            "  if (choose) { return seed; }\n"
            "  return 8;\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n";
    static TZrChar sourceName[] =
            "two_return_sites_try_finally_fallback.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    TEST_ASSERT_NULL(find_block_kind(
            function, ZR_PARSER_CFG_BLOCK_CLEANUP));

    free_source(&compiler, ast);
}

static void test_conditional_throw_try_finally_dispatches_pending_state(void) {
    static const TZrChar source[] =
            "var choose: bool = true;\n"
            "var seed: int = 7;\n"
            "try {\n"
            "  if (choose) { throw seed; }\n"
            "  seed = 8;\n"
            "} finally {\n"
            "  seed = 9;\n"
            "}\n"
            "seed;\n";
    static TZrChar sourceName[] =
            "conditional_throw_try_finally.zr";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_source(
            &compiler, source, sizeof(source) - 1U,
            sourceName);
    const SZrSemanticIrFunction *function;
    const SZrParserCfgBlock *cleanup;
    const SZrParserCfgBlock *throwBlock;
    const SZrParserCfgEdge *throwEdge;
    const SZrParserCfgEdge *joinEdge;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    cleanup = find_block_kind(function, ZR_PARSER_CFG_BLOCK_CLEANUP);
    throwBlock = find_block_terminator(
            function, ZR_PARSER_CFG_TERMINATOR_THROW);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_NOT_NULL(throwBlock);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
            cleanup->terminatorKind);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->predecessorCount);
    TEST_ASSERT_EQUAL_UINT32(2U, cleanup->successorCount);
    throwEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 0U);
    joinEdge = ZrParser_Cfg_BlockEdgeAt(cleanup, 1U);
    TEST_ASSERT_NOT_NULL(throwEdge);
    TEST_ASSERT_NOT_NULL(joinEdge);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_CASE, throwEdge->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT, joinEdge->kind);
    TEST_ASSERT_EQUAL_UINT32(throwBlock->id, throwEdge->toBlockId);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_IR_SWITCH, block_tail(function, cleanup)->opcode);

    free_source(&compiler, ast);
}

#include "test_ssa_source_cleanup_cfg_exceptional.inc"
#include "test_ssa_source_cleanup_cfg_loop.inc"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_linear_try_finally_emits_cleanup_region);
    RUN_TEST(test_return_try_finally_preserves_precleanup_value);
    RUN_TEST(test_nonlinear_return_try_finally_stays_on_legacy_path);
    RUN_TEST(test_throw_try_finally_preserves_precleanup_value);
    RUN_TEST(test_nonlinear_throw_try_finally_stays_on_legacy_path);
    RUN_TEST(test_conditional_return_try_finally_dispatches_pending_state);
    RUN_TEST(test_two_return_sites_try_finally_stays_on_legacy_path);
    RUN_TEST(test_conditional_throw_try_finally_dispatches_pending_state);
    RUN_TEST(test_invoke_try_finally_rethrows_exception_after_cleanup);
    RUN_TEST(test_invoke_argument_is_captured_before_exception_cleanup);
    RUN_TEST(test_literal_argument_try_finally_stays_on_legacy_path);
    RUN_TEST(test_two_invoke_try_finally_stays_on_legacy_path);
    RUN_TEST(test_try_catch_finally_stays_on_legacy_path);
    RUN_TEST(test_terminal_break_try_finally_routes_to_loop_join);
    RUN_TEST(test_conditional_break_try_finally_dispatches_to_loop_join);
    RUN_TEST(test_terminal_continue_try_finally_routes_to_condition);
    RUN_TEST(test_conditional_continue_try_finally_dispatches_to_condition);
    RUN_TEST(test_two_continue_try_finally_stays_on_legacy_path);
    RUN_TEST(test_two_break_sites_try_finally_stay_on_legacy_path);
    return UNITY_END();
}
