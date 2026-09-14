#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/exec_ir_vectorize.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange result;
    result.start = start;
    result.count = count;
    return result;
}

static void append_instruction(SZrExecIrFunction *function,
                               EZrExecIrOpcode opcode,
                               TZrUInt16 flags,
                               SZrExecIrRange memoryIn,
                               SZrExecIrRange memoryOut,
                               TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.flags = flags;
    instruction.memoryIn = memoryIn;
    instruction.memoryOut = memoryOut;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
    assert(id == function->instructionCount);
}

static void build_function(SZrExecIrFunction *function) {
    TZrExecIrBlockId block;
    TZrExecIrMemoryTokenId memoryToken = 1u;
    SZrExecIrRange memoryRange;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 0x901u;
    block = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    assert(block == 1u);
    block = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    assert(block == 2u);
    block = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    assert(block == 3u);
    assert(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &memoryToken, 1u,
                                                    &memoryRange));

    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH, 0u,
                       range(0u, 0u), range(0u, 0u), 10u);
    function->blocks[0].instructionRange = range(0u, 1u);
    function->blocks[0].terminatorInstructionId = 1u;

    /* A memory read is enough to exercise the runtime alias-guard decision;
     * no host pointer is stored in the resulting plan. */
    append_instruction(function, ZR_EXEC_IR_OPCODE_LOAD, 0u,
                       memoryRange, range(0u, 0u), 20u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_ADD, 0u,
                       range(0u, 0u), range(0u, 0u), 21u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH, 0u,
                       range(0u, 0u), range(0u, 0u), 22u);
    function->blocks[1].instructionRange = range(1u, 3u);
    function->blocks[1].terminatorInstructionId = 4u;

    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, 0u,
                       range(0u, 0u), range(0u, 0u), 30u);
    function->blocks[2].instructionRange = range(4u, 1u);
    function->blocks[2].terminatorInstructionId = 5u;
}

static void build_loop_info(SZrExecIrLoopInfo *info, TZrUInt64 tripCount) {
    ZrParser_ExecIr_LoopInfoInit(info);
    info->loops = (SZrExecIrLoop *)calloc(1u, sizeof(*info->loops));
    info->memberBlocks = (TZrExecIrBlockId *)calloc(1u,
                                                      sizeof(*info->memberBlocks));
    assert(info->loops != ZR_NULL && info->memberBlocks != ZR_NULL);
    info->loopCapacity = info->loopCount = 1u;
    info->memberCapacity = info->memberCount = 1u;
    info->memberBlocks[0] = 2u;
    info->loops[0].id = 1u;
    info->loops[0].headerBlockId = 2u;
    info->loops[0].preheaderBlockId = 1u;
    info->loops[0].latchBlockId = 2u;
    info->loops[0].memberOffset = 0u;
    info->loops[0].memberCount = 1u;
    info->loops[0].tripCountMin = tripCount;
    info->loops[0].tripCountMax = tripCount;
    info->loops[0].tripCountKnown = ZR_TRUE;
    info->loops[0].reducible = ZR_TRUE;
    info->loops[0].hasPreheader = ZR_TRUE;
}

static void test_vector_row_and_tail(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrVectorizeOptions options;
    SZrExecIrVectorizePlan plan;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrVectorizeLoopPlan *row;

    build_function(&function);
    build_loop_info(&loops, 9u);
    ZrParser_ExecIr_VectorizeOptionsInit(&options);
    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.loopInfo = &loops;
    options.plan = &plan;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row != ZR_NULL);
    assert(row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR);
    assert(row->vectorLanes == 4u);
    assert(row->vectorIterations == 2u);
    assert(row->tailLanes == 1u);
    assert(row->needsAliasGuard == ZR_TRUE);
    assert(row->usesMaskedTail == ZR_TRUE);
    assert(plan.vectorizedCount == 1u && plan.scalarFallbackCount == 0u);
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_LOAD);
    ZrParser_ExecIr_VectorizePlanFree(&plan);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_alias_and_effect_fallbacks(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrVectorizeOptions options;
    SZrExecIrVectorizePlan plan;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrVectorizeLoopPlan *row;

    build_function(&function);
    build_loop_info(&loops, 16u);
    ZrParser_ExecIr_VectorizeOptionsInit(&options);
    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.loopInfo = &loops;
    options.plan = &plan;
    options.aliasState = ZR_EXEC_IR_VECTORIZER_ALIAS_MAY_ALIAS;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK);
    assert(row->reason == ZR_EXEC_IR_VECTORIZER_REASON_ALIAS_UNSAFE);
    ZrParser_ExecIr_VectorizePlanFree(&plan);

    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.plan = &plan;
    options.aliasState = ZR_EXEC_IR_VECTORIZER_ALIAS_DISJOINT;
    function.instructions[1].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK);
    assert(row->reason == ZR_EXEC_IR_VECTORIZER_REASON_EFFECT);
    ZrParser_ExecIr_VectorizePlanFree(&plan);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_unknown_trip_and_disable_are_visible(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrVectorizeOptions options;
    SZrExecIrVectorizePlan plan;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrVectorizeLoopPlan *row;

    build_function(&function);
    build_loop_info(&loops, ZR_EXEC_IR_LOOP_TRIP_COUNT_UNKNOWN);
    loops.loops[0].tripCountKnown = ZR_FALSE;
    ZrParser_ExecIr_VectorizeOptionsInit(&options);
    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.loopInfo = &loops;
    options.plan = &plan;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK);
    assert(row->reason == ZR_EXEC_IR_VECTORIZER_REASON_UNKNOWN_TRIP);
    ZrParser_ExecIr_VectorizePlanFree(&plan);

    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.plan = &plan;
    options.disableVectorization = ZR_TRUE;
    loops.loops[0].tripCountKnown = ZR_TRUE;
    loops.loops[0].tripCountMax = 16u;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_SCALAR_FALLBACK);
    assert(row->reason == ZR_EXEC_IR_VECTORIZER_REASON_DISABLED);
    ZrParser_ExecIr_VectorizePlanFree(&plan);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_memory_proofs_are_required_and_negative_stride_is_allowed(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrVectorizeOptions options;
    SZrExecIrVectorizePlan plan;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrVectorizeLoopPlan *row;

    build_function(&function);
    build_loop_info(&loops, 16u);
    ZrParser_ExecIr_VectorizeOptionsInit(&options);
    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.loopInfo = &loops;
    options.plan = &plan;
    options.aliasState = ZR_EXEC_IR_VECTORIZER_ALIAS_DISJOINT;
    options.boundsProven = ZR_FALSE;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row->reason == ZR_EXEC_IR_VECTORIZER_REASON_BOUNDS_UNKNOWN);
    ZrParser_ExecIr_VectorizePlanFree(&plan);

    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.plan = &plan;
    options.boundsProven = ZR_TRUE;
    options.strideProven = ZR_FALSE;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row->reason == ZR_EXEC_IR_VECTORIZER_REASON_STRIDE_UNKNOWN);
    ZrParser_ExecIr_VectorizePlanFree(&plan);

    ZrParser_ExecIr_VectorizePlanInit(&plan);
    options.plan = &plan;
    options.strideProven = ZR_TRUE;
    options.strideBytes = -1;
    assert(ZrParser_ExecIr_VectorizeLoops(&function, &options, &diagnostic));
    row = ZrParser_ExecIr_VectorizePlanAt(&plan, 0u);
    assert(row->decision == ZR_EXEC_IR_VECTORIZER_DECISION_VECTOR);
    assert(row->strideBytes == -1);
    ZrParser_ExecIr_VectorizePlanFree(&plan);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_vector_row_and_tail();
    test_alias_and_effect_fallbacks();
    test_unknown_trip_and_disable_are_visible();
    test_memory_proofs_are_required_and_negative_stride_is_allowed();
    return 0;
}
