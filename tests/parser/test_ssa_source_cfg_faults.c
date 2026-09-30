#include "unity.h"
#include "ssa_source_cfg_faults.h"

#include <stdio.h>
#include <string.h>
#include "harness/runtime_support.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_oracle.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_ir.h"

static void print_oracle_failure_details(
        const SZrExecIrFunction *function,
        const SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 instructionIndex;
    fprintf(stderr,
            "baseline Oracle diagnostic: code=%u function=%u block=%u instruction=%u source=%u expectedVersion=%u actualVersion=%u expectedHash=%llu actualHash=%llu\n",
            (unsigned)diagnostic->code,
            (unsigned)diagnostic->functionToken,
            (unsigned)diagnostic->blockId,
            (unsigned)diagnostic->instructionId,
            (unsigned)diagnostic->sourceId,
            (unsigned)diagnostic->expectedVersion,
            (unsigned)diagnostic->actualVersion,
            (unsigned long long)diagnostic->expectedHash,
            (unsigned long long)diagnostic->actualHash);
    fprintf(stderr,
            "baseline Oracle function: id=%u token=%u entry=%u blocks=%u instructions=%u values=%u operands=%u results=%u\n",
            (unsigned)function->id,
            (unsigned)function->functionToken,
            (unsigned)function->entryBlockId,
            (unsigned)function->blockCount,
            (unsigned)function->instructionCount,
            (unsigned)function->valueCount,
            (unsigned)function->operandCount,
            (unsigned)function->resultCount);
    for (instructionIndex = 0u;
         instructionIndex < function->instructionCount;
         ++instructionIndex) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[instructionIndex];
        TZrUInt32 itemIndex;
        fprintf(stderr,
                "instruction[%u] id=%u opcode=%u flags=%u results=(%u,%u) operands=(%u,%u) phi=(%u,%u) successors=(%u,%u) type=%u matchType=%u layout=%u memoryIn=(%u,%u) memoryOut=(%u,%u) effectIn=%u effectOut=%u source=%u deopt=%u binding=%u\n",
                (unsigned)instructionIndex,
                (unsigned)(instructionIndex + 1u),
                (unsigned)instruction->opcode,
                (unsigned)instruction->flags,
                (unsigned)instruction->results.offset,
                (unsigned)instruction->results.count,
                (unsigned)instruction->operands.offset,
                (unsigned)instruction->operands.count,
                (unsigned)instruction->phiRange.offset,
                (unsigned)instruction->phiRange.count,
                (unsigned)instruction->successorRange.offset,
                (unsigned)instruction->successorRange.count,
                (unsigned)instruction->typeToken,
                (unsigned)instruction->matchTypeToken,
                (unsigned)instruction->layoutId,
                (unsigned)instruction->memoryIn.offset,
                (unsigned)instruction->memoryIn.count,
                (unsigned)instruction->memoryOut.offset,
                (unsigned)instruction->memoryOut.count,
                (unsigned)instruction->effectIn,
                (unsigned)instruction->effectOut,
                (unsigned)instruction->sourceId,
                (unsigned)instruction->deoptId,
                (unsigned)instruction->bindingRow);
        if (function->results != ZR_NULL &&
            instruction->results.offset <= function->resultCount &&
            instruction->results.count <=
                    function->resultCount - instruction->results.offset) {
            for (itemIndex = 0u; itemIndex < instruction->results.count;
                 ++itemIndex) {
                fprintf(stderr, "  result[%u]=%u\n",
                        (unsigned)itemIndex,
                        (unsigned)function->results[
                                instruction->results.offset + itemIndex]);
            }
        }
        if (function->operands != ZR_NULL &&
            instruction->operands.offset <= function->operandCount &&
            instruction->operands.count <=
                    function->operandCount - instruction->operands.offset) {
            for (itemIndex = 0u; itemIndex < instruction->operands.count;
                 ++itemIndex) {
                fprintf(stderr, "  operand[%u]=%u\n",
                        (unsigned)itemIndex,
                        (unsigned)function->operands[
                                instruction->operands.offset + itemIndex]);
            }
        }
    }
}

static SZrState *g_state;

void setUp(void) {
    ssa_source_cfg_fail_allocation(0u);
    ssa_source_cfg_fail_promotion(SSA_SOURCE_CFG_PROMOTION_NO_FAULT);
    ssa_source_cfg_fail_finish(SSA_SOURCE_CFG_FINISH_NO_FAULT);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ssa_source_cfg_fail_allocation(0u);
    ssa_source_cfg_fail_promotion(SSA_SOURCE_CFG_PROMOTION_NO_FAULT);
    ssa_source_cfg_fail_finish(SSA_SOURCE_CFG_FINISH_NO_FAULT);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
    TEST_ASSERT_EQUAL_UINT64(0u, ssa_source_cfg_outstanding_allocations());
}

static SZrAstNode *compile_text(SZrCompilerState *compiler,
                               const char *source) {
    size_t length = strlen(source), index;
    SZrString *name;
    SZrAstNode *ast;
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
    return ast;
}

static SZrAstNode *compile_many_statements(SZrCompilerState *compiler) {
    char source[4096];
    const char *prefix = "var value: int = 7;\n";
    const char *read = "value;\n";
    size_t length = strlen(prefix), index;
    memcpy(source, prefix, length);
    for (index = 0u; index < 256u; ++index) {
        memcpy(source + length, read, strlen(read));
        length += strlen(read);
    }
    source[length] = '\0';
    {
        SZrAstNode *ast = compile_text(compiler, source);
        TEST_ASSERT_FALSE(compiler->preSemanticIrCfgActive);
        return ast;
    }
}

typedef struct SZrSourceCfgOracleMemory {
    const SZrSemanticIrFunction *semantic;
    SZrExecIrOracleValue places[64];
    TZrUInt32 stores;
} SZrSourceCfgOracleMemory;

static TZrBool source_cfg_place_provider(
        void *userData, const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrSourceCfgOracleMemory *memory = (SZrSourceCfgOracleMemory *)userData;
    const SZrSemanticIrInstruction *source;
    (void)operands;
    if (instruction->sourceId == 0u) return ZR_FALSE;
    source = ZrParser_SemanticIr_InstructionAt(
            memory->semantic, instruction->sourceId - 1u);
    if (source == ZR_NULL || source->opcode != ZR_SEMANTIC_IR_PLACE_BASE ||
        operandCount != 1u || source->placeId == 0u ||
        source->placeId >= ZR_ARRAY_COUNT(memory->places))
        return ZR_FALSE;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = (TZrInt64)source->placeId;
    return ZR_TRUE;
}

static TZrBool source_cfg_memory_provider(
        void *userData, const SZrExecIrInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrSourceCfgOracleMemory *memory = (SZrSourceCfgOracleMemory *)userData;
    TZrInt64 address;
    (void)instruction;
    if (operandCount == 0u ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED)
        return ZR_FALSE;
    address = operands[0].as.signedInteger;
    if (address < 1 || address >= (TZrInt64)ZR_ARRAY_COUNT(memory->places))
        return ZR_FALSE;
    if (operation == ZR_EXEC_IR_ORACLE_MEMORY_LOAD && operandCount == 1u &&
        result != ZR_NULL) {
        *result = memory->places[address];
    } else if (operation == ZR_EXEC_IR_ORACLE_MEMORY_STORE &&
               operandCount == 2u) {
        memory->places[address] = operands[1];
        ++memory->stores;
    } else {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void free_source(SZrCompilerState *compiler, SZrAstNode *ast) {
    ZrCore_Function_Free(g_state, compiler->currentFunction);
    compiler->currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(compiler);
    ZrParser_Ast_Free(g_state, ast);
}

#define SOURCE_CFG_SNAPSHOT_MAX_BLOCKS 16u
#define SOURCE_CFG_SNAPSHOT_MAX_EDGES 8u
#define SOURCE_CFG_SNAPSHOT_MAX_ARRAY_BYTES 8192u

typedef struct SZrSourceCfgFaultSnapshot {
    TZrUInt32 blockCount;
    TZrUInt32 entryBlockId;
    TZrUInt32 exitBlockId;
    SZrState *cfgState;
    SZrSemanticContext *semanticContext;
    TZrBool blocksValid;
    TZrSize blockElementSize;
    SZrParserCfgBlock blocks[SOURCE_CFG_SNAPSHOT_MAX_BLOCKS];
    TZrSize edgeCounts[SOURCE_CFG_SNAPSHOT_MAX_BLOCKS];
    SZrParserCfgEdge edges[SOURCE_CFG_SNAPSHOT_MAX_BLOCKS]
                          [SOURCE_CFG_SNAPSHOT_MAX_EDGES];
    TZrSize instructionCount;
    TZrSize sourceMapCount;
    TZrSize operandCount;
    size_t instructionBytes;
    size_t sourceMapBytes;
    size_t operandBytes;
    TZrUInt8 instructions[SOURCE_CFG_SNAPSHOT_MAX_ARRAY_BYTES];
    TZrUInt8 sourceMap[SOURCE_CFG_SNAPSHOT_MAX_ARRAY_BYTES];
    TZrUInt8 operands[SOURCE_CFG_SNAPSHOT_MAX_ARRAY_BYTES];
    TZrUInt32 cursorBlock;
    TZrUInt32 cursorStart;
    TZrBool cfgActive;
    TZrBool cfgTerminated;
    TZrBool validated;
} SZrSourceCfgFaultSnapshot;

static TZrBool snapshot_source_cfg_fault_state(
        const SZrCompilerState *compiler,
        SZrSourceCfgFaultSnapshot *snapshot) {
    const SZrParserCfg *cfg = &compiler->preSemanticIr.cfg;
    const SZrArray *arrays[3];
    size_t *byteCounts[3];
    TZrUInt8 *byteCopies[3];
    TZrSize index;
    arrays[0] = &compiler->preSemanticIr.instructions;
    arrays[1] = &compiler->preSemanticIr.sourceMap;
    arrays[2] = &compiler->preSemanticIr.valueOperands;
    byteCounts[0] = &snapshot->instructionBytes;
    byteCounts[1] = &snapshot->sourceMapBytes;
    byteCounts[2] = &snapshot->operandBytes;
    byteCopies[0] = snapshot->instructions;
    byteCopies[1] = snapshot->sourceMap;
    byteCopies[2] = snapshot->operands;
    if (cfg->blocks.length > SOURCE_CFG_SNAPSHOT_MAX_BLOCKS) return ZR_FALSE;
    snapshot->blockCount = (TZrUInt32)cfg->blocks.length;
    snapshot->entryBlockId = cfg->entryBlockId;
    snapshot->exitBlockId = cfg->exitBlockId;
    snapshot->cfgState = cfg->state;
    snapshot->semanticContext = cfg->semanticContext;
    snapshot->blocksValid = cfg->blocks.isValid;
    snapshot->blockElementSize = cfg->blocks.elementSize;
    for (index = 0u; index < cfg->blocks.length; ++index) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)
                ZrCore_Array_Get((SZrArray *)&cfg->blocks, index);
        TZrSize edgeIndex;
        if (block == ZR_NULL ||
            block->outgoingEdges.length > SOURCE_CFG_SNAPSHOT_MAX_EDGES)
            return ZR_FALSE;
        snapshot->blocks[index] = *block;
        snapshot->edgeCounts[index] = block->outgoingEdges.length;
        for (edgeIndex = 0u; edgeIndex < block->outgoingEdges.length;
             ++edgeIndex) {
            const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(
                    block, edgeIndex);
            if (edge == ZR_NULL) return ZR_FALSE;
            snapshot->edges[index][edgeIndex] = *edge;
        }
    }
    for (index = 0u; index < ZR_ARRAY_COUNT(arrays); ++index) {
        *byteCounts[index] = arrays[index]->length * arrays[index]->elementSize;
        if (*byteCounts[index] > SOURCE_CFG_SNAPSHOT_MAX_ARRAY_BYTES ||
            (*byteCounts[index] != 0u && arrays[index]->head == ZR_NULL))
            return ZR_FALSE;
        if (*byteCounts[index] != 0u) {
            memcpy(byteCopies[index], arrays[index]->head, *byteCounts[index]);
        }
    }
    snapshot->instructionCount = arrays[0]->length;
    snapshot->sourceMapCount = arrays[1]->length;
    snapshot->operandCount = arrays[2]->length;
    snapshot->cursorBlock = compiler->preSemanticIrCfgBlock;
    snapshot->cursorStart = compiler->preSemanticIrCfgStart;
    snapshot->cfgActive = compiler->preSemanticIrCfgActive;
    snapshot->cfgTerminated = compiler->preSemanticIrCfgTerminated;
    snapshot->validated = compiler->preSemanticIrValidated;
    return ZR_TRUE;
}

static void assert_source_cfg_fault_state_unchanged(
        const SZrCompilerState *compiler,
        const SZrSourceCfgFaultSnapshot *snapshot) {
    const SZrParserCfg *cfg = &compiler->preSemanticIr.cfg;
    const SZrArray *arrays[3];
    const TZrSize expectedCounts[3] = {
        snapshot->instructionCount, snapshot->sourceMapCount,
        snapshot->operandCount
    };
    const size_t expectedBytes[3] = {
        snapshot->instructionBytes, snapshot->sourceMapBytes,
        snapshot->operandBytes
    };
    const TZrUInt8 *expectedCopies[3] = {
        snapshot->instructions, snapshot->sourceMap, snapshot->operands
    };
    TZrSize index;
    TEST_ASSERT_EQUAL_UINT64(snapshot->blockCount, cfg->blocks.length);
    TEST_ASSERT_EQUAL_UINT32(snapshot->entryBlockId, cfg->entryBlockId);
    TEST_ASSERT_EQUAL_UINT32(snapshot->exitBlockId, cfg->exitBlockId);
    TEST_ASSERT_EQUAL_PTR(snapshot->cfgState, cfg->state);
    TEST_ASSERT_EQUAL_PTR(snapshot->semanticContext, cfg->semanticContext);
    TEST_ASSERT_EQUAL_INT(snapshot->blocksValid, cfg->blocks.isValid);
    TEST_ASSERT_EQUAL_UINT64(snapshot->blockElementSize,
                             cfg->blocks.elementSize);
    for (index = 0u; index < snapshot->blockCount; ++index) {
        const SZrParserCfgBlock *actual = (const SZrParserCfgBlock *)
                ZrCore_Array_Get((SZrArray *)&cfg->blocks, index);
        const SZrParserCfgBlock *expected = &snapshot->blocks[index];
        TZrSize edgeIndex;
        TEST_ASSERT_NOT_NULL(actual);
        TEST_ASSERT_EQUAL_UINT32(expected->id, actual->id);
        TEST_ASSERT_EQUAL_INT(expected->kind, actual->kind);
        TEST_ASSERT_EQUAL_PTR(expected->statement, actual->statement);
        TEST_ASSERT_EQUAL_UINT32(expected->firstInstructionIndex,
                                 actual->firstInstructionIndex);
        TEST_ASSERT_EQUAL_UINT32(expected->instructionCount,
                                 actual->instructionCount);
        TEST_ASSERT_EQUAL_MEMORY(expected->successors, actual->successors,
                                 sizeof(expected->successors));
        TEST_ASSERT_EQUAL_UINT32(expected->successorCount,
                                 actual->successorCount);
        TEST_ASSERT_EQUAL_UINT32(expected->predecessorCount,
                                 actual->predecessorCount);
        TEST_ASSERT_EQUAL_INT(expected->terminatorKind, actual->terminatorKind);
        TEST_ASSERT_EQUAL_INT(expected->isTerminator, actual->isTerminator);
        TEST_ASSERT_EQUAL_INT(expected->visited, actual->visited);
        TEST_ASSERT_EQUAL_INT(expected->unreachableCause,
                              actual->unreachableCause);
        TEST_ASSERT_EQUAL_PTR(expected->unreachableCauseNode,
                              actual->unreachableCauseNode);
        TEST_ASSERT_EQUAL_INT(expected->outgoingEdges.isValid,
                              actual->outgoingEdges.isValid);
        TEST_ASSERT_EQUAL_UINT64(expected->outgoingEdges.elementSize,
                                 actual->outgoingEdges.elementSize);
        TEST_ASSERT_EQUAL_UINT64(snapshot->edgeCounts[index],
                                 actual->outgoingEdges.length);
        for (edgeIndex = 0u; edgeIndex < snapshot->edgeCounts[index];
             ++edgeIndex) {
            const SZrParserCfgEdge *actualEdge = ZrParser_Cfg_BlockEdgeAt(
                    actual, edgeIndex);
            TEST_ASSERT_NOT_NULL(actualEdge);
            TEST_ASSERT_EQUAL_MEMORY(&snapshot->edges[index][edgeIndex],
                                     actualEdge, sizeof(*actualEdge));
        }
    }
    arrays[0] = &compiler->preSemanticIr.instructions;
    arrays[1] = &compiler->preSemanticIr.sourceMap;
    arrays[2] = &compiler->preSemanticIr.valueOperands;
    for (index = 0u; index < ZR_ARRAY_COUNT(arrays); ++index) {
        TEST_ASSERT_EQUAL_UINT64(expectedCounts[index], arrays[index]->length);
        if (expectedBytes[index] != 0u) {
            TEST_ASSERT_EQUAL_MEMORY(expectedCopies[index], arrays[index]->head,
                                     expectedBytes[index]);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(snapshot->cursorBlock,
                             compiler->preSemanticIrCfgBlock);
    TEST_ASSERT_EQUAL_UINT32(snapshot->cursorStart,
                             compiler->preSemanticIrCfgStart);
    TEST_ASSERT_EQUAL_INT(snapshot->cfgActive,
                          compiler->preSemanticIrCfgActive);
    TEST_ASSERT_EQUAL_INT(snapshot->cfgTerminated,
                          compiler->preSemanticIrCfgTerminated);
    TEST_ASSERT_EQUAL_INT(snapshot->validated,
                          compiler->preSemanticIrValidated);
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

static void assert_promotion_failure_restores_graph(
        TZrBool startWithAnalysisGraph,
        ESsaSourceCfgPromotionFault fault) {
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_many_statements(&compiler);
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    SZrParserCfg previous;
    TZrSize instructionCount, sourceMapCount, operandCount;
    TZrUInt32 block, start;
    TZrBool validated;
    if (startWithAnalysisGraph) {
        compiler.preSemanticIrCfgStartupSuppressed = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
        compiler.preSemanticIrCfgStartupSuppressed = ZR_FALSE;
    }
    previous = compiler.preSemanticIr.cfg;
    instructionCount = compiler.preSemanticIr.instructions.length;
    sourceMapCount = compiler.preSemanticIr.sourceMap.length;
    operandCount = compiler.preSemanticIr.valueOperands.length;
    block = compiler.preSemanticIrCfgBlock;
    start = compiler.preSemanticIrCfgStart;
    validated = compiler.preSemanticIrValidated;
    ssa_source_cfg_fail_promotion(fault);
    TEST_ASSERT_FALSE(ssa_source_cfg_finalize(&compiler));
    ssa_source_cfg_fail_promotion(SSA_SOURCE_CFG_PROMOTION_NO_FAULT);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    TEST_ASSERT_EQUAL_MEMORY(&previous, &compiler.preSemanticIr.cfg,
                             sizeof(previous));
    TEST_ASSERT_EQUAL_UINT64(instructionCount,
                            compiler.preSemanticIr.instructions.length);
    TEST_ASSERT_EQUAL_UINT64(sourceMapCount,
                            compiler.preSemanticIr.sourceMap.length);
    TEST_ASSERT_EQUAL_UINT64(operandCount,
                            compiler.preSemanticIr.valueOperands.length);
    TEST_ASSERT_EQUAL_UINT32(block, compiler.preSemanticIrCfgBlock);
    TEST_ASSERT_EQUAL_UINT32(start, compiler.preSemanticIrCfgStart);
    TEST_ASSERT_EQUAL_INT(validated, compiler.preSemanticIrValidated);
    TEST_ASSERT_TRUE(ssa_source_cfg_finalize(&compiler));
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(&compiler.preSemanticIr, ZR_NULL,
                                         &output, &diagnostic));
    ZrCore_ExecIr_FreeFunction(&output);
    free_source(&compiler, ast);
}

static void assert_retry_builds_and_oracle_returns_nine(
        SZrCompilerState *compiler) {
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult result;
    SZrExecIrOracleValue constants[16] = {{0}};
    SZrExecIrOracleValue initial[128] = {{0}};
    SZrSourceCfgOracleMemory memory = {0};
    TZrSize index;
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(compiler));
    TEST_ASSERT_TRUE(compiler->constants.length <= ZR_ARRAY_COUNT(constants));
    for (index = 0u; index < compiler->constants.length; ++index) {
        const SZrTypeValue *constant = (const SZrTypeValue *)ZrCore_Array_Get(
                &compiler->constants, index);
        if (ZR_VALUE_IS_TYPE_BOOL(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            constants[index].as.boolean =
                    constant->value.nativeObject.nativeBool;
        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            constants[index].as.signedInteger =
                    constant->value.nativeObject.nativeInt64;
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constant->type)) {
            constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
            constants[index].as.unsignedInteger =
                    constant->value.nativeObject.nativeUInt64;
        }
    }
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            &compiler->preSemanticIr, ZR_NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(output.valueCount <= ZR_ARRAY_COUNT(initial));
    for (index = 0u; index < output.valueCount; ++index) {
        if (output.values[index].definitionInstructionId ==
                ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            TZrUInt32 instructionIndex;
            TZrBool placeProvenance = ZR_FALSE;
            for (instructionIndex = 0u;
                 instructionIndex < output.instructionCount;
                 ++instructionIndex) {
                const SZrExecIrInstruction *instruction =
                        &output.instructions[instructionIndex];
                if (instruction->opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE &&
                    instruction->operandRange.count == 1u &&
                    output.operandPool[instruction->operandRange.offset] ==
                            index + 1u)
                    placeProvenance = ZR_TRUE;
            }
            TEST_ASSERT_TRUE(placeProvenance);
            initial[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
            initial[index].as.signedInteger = (TZrInt64)index + 1;
        }
    }
    /* Standalone verifier and oracle require published identities. The
     * builder verifies unpublished candidates with temporary ones, so keep
     * the same valid identities for both checks here. */
    if (output.id == ZR_EXEC_IR_FUNCTION_ID_INVALID) output.id = 1u;
    if (output.functionToken == 0u) output.functionToken = 1u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &output, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    input.function = &output;
    input.initialValues = initial;
    input.initialValueCount = output.valueCount;
    input.constants = constants;
    input.constantCount = (TZrUInt32)compiler->constants.length;
    memory.semantic = &compiler->preSemanticIr;
    input.place = source_cfg_place_provider;
    input.placeUserData = &memory;
    input.memory = source_cfg_memory_provider;
    input.memoryUserData = &memory;
    ZrCore_ExecIr_OracleResultInit(&result);
    {
        TZrBool oracleSucceeded = ZrCore_ExecIr_RunOracleEx(
                &input, &result, &diagnostic);
        if (!oracleSucceeded) {
            print_oracle_failure_details(&output, &diagnostic);
        }
        TEST_ASSERT_TRUE(oracleSucceeded);
    }
    TEST_ASSERT_TRUE(result.returned);
    TEST_ASSERT_FALSE(result.terminatedByThrow);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                          result.returnValue.kind);
    TEST_ASSERT_EQUAL_INT64(9, result.returnValue.as.signedInteger);
    ZrCore_ExecIr_OracleResultFree(&result);
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_finish_internal_failures_preserve_active_source_cfg(void) {
    static const ESsaSourceCfgFinishFault faults[] = {
        SSA_SOURCE_CFG_FINISH_EXIT_BLOCK_APPEND,
        SSA_SOURCE_CFG_FINISH_BRANCH_INSTRUCTION_APPEND,
        SSA_SOURCE_CFG_FINISH_BRANCH_RANGE_BIND,
        SSA_SOURCE_CFG_FINISH_NORMAL_EDGE_APPEND,
        SSA_SOURCE_CFG_FINISH_RETURN_INSTRUCTION_APPEND,
        SSA_SOURCE_CFG_FINISH_RETURN_RANGE_BIND
    };
    size_t index;
    for (index = 0u; index < ZR_ARRAY_COUNT(faults); ++index) {
        SZrCompilerState compiler;
        SZrAstNode *ast = compile_text(
                &compiler, "if (true) { return 9; }\n");
        SZrSourceCfgFaultSnapshot before;
        TZrBool finalized;
        TZrBool faultTriggered;
        TEST_ASSERT_TRUE(compiler.preSemanticIrCfgActive);
        TEST_ASSERT_NOT_EQUAL(ZR_PARSER_CFG_INVALID_BLOCK_ID,
                              compiler.preSemanticIrCfgBlock);
        TEST_ASSERT_TRUE(snapshot_source_cfg_fault_state(&compiler, &before));

        ssa_source_cfg_fail_finish(faults[index]);
        finalized = ssa_source_cfg_finalize(&compiler);
        faultTriggered = ssa_source_cfg_finish_fault_triggered();
        ssa_source_cfg_fail_finish(SSA_SOURCE_CFG_FINISH_NO_FAULT);

        TEST_ASSERT_TRUE(faultTriggered);
        TEST_ASSERT_FALSE(finalized);
        assert_source_cfg_fault_state_unchanged(&compiler, &before);
        TEST_ASSERT_TRUE(ssa_source_cfg_finalize(&compiler));
        assert_retry_builds_and_oracle_returns_nine(&compiler);
        free_source(&compiler, ast);
    }
}

static void test_promotion_failures_preserve_old_graph_and_retry(void) {
    assert_promotion_failure_restores_graph(
            ZR_FALSE, SSA_SOURCE_CFG_PROMOTION_AFTER_ACTIVATION);
    assert_promotion_failure_restores_graph(
            ZR_TRUE, SSA_SOURCE_CFG_PROMOTION_AFTER_ACTIVATION);
    assert_promotion_failure_restores_graph(
            ZR_FALSE, SSA_SOURCE_CFG_PROMOTION_AFTER_FINISH);
    assert_promotion_failure_restores_graph(
            ZR_TRUE, SSA_SOURCE_CFG_PROMOTION_AFTER_FINISH);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_and_growth_failures_preserve_unpublished_graph);
    RUN_TEST(test_scratch_failure_preserves_existing_analysis_graph);
    RUN_TEST(test_invalid_compiler_is_rejected_without_allocation);
    RUN_TEST(test_promotion_failures_preserve_old_graph_and_retry);
    RUN_TEST(test_finish_internal_failures_preserve_active_source_cfg);
    return UNITY_END();
}
