#include "zr_vm_parser/exec_ir_fusion.h"
#include "zr_vm_parser/exec_ir_binding_facts.h"
#include "zr_vm_core/exec_ir_state_map.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void init_function(SZrExecIrFunction *function,
                          TZrUInt32 instructionCount) {
    ZrCore_ExecIr_FunctionInit(function);
    function->functionToken = 0x100u;
    function->signatureHash = UINT64_C(0x1111222233334444);
    function->contract.generation = 7u;
    function->contract.signatureHash = function->signatureHash;
    function->contract.moduleHash = UINT64_C(0xaaaabbbbccccdddd);
    function->contract.layoutHash = UINT64_C(0x12345678);
    function->instructionCount = instructionCount;
    function->instructionCapacity = instructionCount;
    function->instructions = (SZrExecIrInstruction *)calloc(
            instructionCount, sizeof(*function->instructions));
    function->valueCount = instructionCount * 2u;
    function->valueCapacity = function->valueCount;
    function->values = (SZrExecIrValue *)calloc(
            function->valueCount, sizeof(*function->values));
    function->operandCount = instructionCount * 2u;
    function->operandCapacity = function->operandCount;
    function->operands = (TZrExecIrValueId *)calloc(
            function->operandCount, sizeof(*function->operands));
    function->resultCount = instructionCount;
    function->resultCapacity = function->resultCount;
    function->results = (TZrExecIrValueId *)calloc(
            function->resultCount, sizeof(*function->results));
    function->blockCount = 1u;
    function->blockCapacity = 1u;
    function->blocks = (SZrExecIrBlock *)calloc(1u, sizeof(*function->blocks));
    function->blocks[0].id = 1u;
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = instructionCount;
    for (TZrUInt32 index = 0u; index < instructionCount; ++index) {
        function->instructions[index].sourceId = 1000u + index;
        function->instructions[index].typeToken = 9u;
        function->instructions[index].results.start = index;
        function->instructions[index].results.count = 1u;
        function->results[index] = index + 1u;
        function->values[index].id = index + 1u;
        function->values[index].definition = index + 1u;
        function->values[index].typeToken = 9u;
    }
    for (TZrUInt32 index = instructionCount; index < function->valueCount;
         ++index) {
        function->values[index].id = index + 1u;
        function->values[index].typeToken = 9u;
    }
    for (TZrUInt32 index = 0u; index < instructionCount * 2u; ++index) {
        function->operands[index] = function->valueCount;
    }
}

static void set_instruction(SZrExecIrFunction *function,
                            TZrUInt32 index,
                            EZrExecIrOpcode opcode,
                            TZrUInt32 operandStart,
                            TZrUInt32 operandCount,
                            TZrUInt32 resultId) {
    SZrExecIrInstruction *instruction = &function->instructions[index];
    instruction->opcode = (TZrUInt16)opcode;
    instruction->operands.start = operandStart;
    instruction->operands.count = operandCount;
    instruction->results.start = index;
    instruction->results.count = resultId == 0u ? 0u : 1u;
    if (resultId != 0u) {
        function->results[index] = resultId;
    }
}

static void make_six_pattern_function(SZrExecIrFunction *function) {
    const EZrExecIrOpcode opcodes[] = {
        ZR_EXEC_IR_OPCODE_LOAD, ZR_EXEC_IR_OPCODE_ADD,
        ZR_EXEC_IR_OPCODE_COMPARE, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
        ZR_EXEC_IR_OPCODE_PLACE_PROJECT, ZR_EXEC_IR_OPCODE_LOAD,
        ZR_EXEC_IR_OPCODE_PLACE_PROJECT, ZR_EXEC_IR_OPCODE_CALL,
        ZR_EXEC_IR_OPCODE_ADD, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
        ZR_EXEC_IR_OPCODE_CALL, ZR_EXEC_IR_OPCODE_RETURN
    };
    const TZrUInt32 operandCounts[] = {1u, 2u, 2u, 1u, 2u, 1u,
                                      2u, 2u, 2u, 1u, 1u, 1u};
    const TZrUInt32 resultIds[] = {13u, 14u, 15u, 0u, 16u, 17u,
                                   18u, 19u, 20u, 0u, 21u, 0u};
    init_function(function, (TZrUInt32)(sizeof(opcodes) / sizeof(opcodes[0])));
    for (TZrUInt32 index = 0u; index < 12u; ++index) {
        const TZrUInt32 operandStart = index * 2u;
        set_instruction(function, index, opcodes[index], operandStart,
                        operandCounts[index], resultIds[index]);
        function->instructions[index].effectIn = index;
        function->instructions[index].effectOut = index + 1u;
    }
    function->instructions[4].layoutId = 1u;
    function->instructions[6].layoutId = 1u;
    /* Each candidate's head result is consumed exactly once by its tail. */
    function->operands[0] = 1u;
    function->operands[2] = 13u;
    function->operands[3] = 2u;
    function->operands[4] = 3u;
    function->operands[5] = 4u;
    function->operands[6] = 15u;
    function->operands[8] = 5u;
    function->operands[9] = 6u;
    function->operands[10] = 16u;
    function->operands[12] = 7u;
    function->operands[13] = 8u;
    function->operands[14] = 18u;
    function->operands[15] = 9u;
    function->operands[16] = 10u;
    function->operands[17] = 11u;
    function->operands[18] = 20u;
    function->operands[20] = 12u;
    function->operands[22] = 21u;
    /* Binding rows are scalar identities produced by static-binding facts;
     * zero remains the conservative "not published" marker for this
     * lightweight fixture. */
    function->instructions[7].bindingRow = 2u;
    function->instructions[10].bindingRow = 3u;
    function->successorCount = 4u;
    function->successorCapacity = 4u;
    function->successors = (TZrExecIrBlockId *)calloc(1u, sizeof(*function->successors));
    function->successors = (TZrExecIrBlockId *)realloc(
            function->successors, 4u * sizeof(*function->successors));
    assert(function->successors != NULL);
    function->successors[0] = 1u;
    function->successors[1] = 1u;
    function->successors[2] = 1u;
    function->successors[3] = 1u;
    function->blocks[0].successorRange.start = 0u;
    function->blocks[0].successorRange.count = 4u;
    function->instructions[3].successorRange.start = 0u;
    function->instructions[3].successorRange.count = 2u;
    function->instructions[9].successorRange.start = 2u;
    function->instructions[9].successorRange.count = 2u;
}

static void test_six_patterns_emit_fixed_width_words_and_side_maps(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    make_six_pattern_function(&function);
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 6u);
    assert(plan.instructionCount == 6u);
    assert(plan.sideEntryCount == 6u);
    assert(plan.sourceMapCount == 12u);
    assert(sizeof(plan.instructions[0]) == sizeof(SZrInstruction));
    assert(plan.instructions[0].operationCode >=
           ZR_EXEC_BC_FUSION_OPCODE_BASE);
    assert(plan.sideEntries[0].headInstructionId == 1u);
    assert(plan.sideEntries[0].tailInstructionId == 2u);
    assert(plan.sideEntries[0].headOperandCount == 1u);
    assert(plan.sideEntries[0].tailOperandCount == 2u);
    assert(plan.sideEntries[0].operandCount == 3u);
    assert(plan.sourceMaps[0].originalInstructionId == 1u);
    assert(plan.sourceMaps[1].originalInstructionId == 2u);
    assert(plan.sourceMaps[0].resumeId == 1u);
    assert(plan.sourceMaps[1].resumeId == 2u);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_boundary_and_type_failures_keep_original_sequence(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    make_six_pattern_function(&function);
    function.instructions[2].flags = ZR_EXEC_IR_FLAG_DEBUG_POLL;
    function.instructions[2].typeToken = 77u;
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 5u);
    assert(plan.fallbackCount != 0u);
    assert(plan.fallbacks[0].reason == ZR_EXEC_BC_FUSION_FALLBACK_DEBUG_BOUNDARY ||
           plan.fallbacks[0].reason == ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_only_declared_boundaries_are_preserved(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    make_six_pattern_function(&function);
    /* COMPARE_BRANCH_INT declares debug preservation in its generated row. */
    function.instructions[2].flags = ZR_EXEC_IR_FLAG_DEBUG_POLL;
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 6u);
    assert((plan.sideEntries[1].guardMask &
            ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG) != 0u);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);

    make_six_pattern_function(&function);
    /* INDEX_LOAD_STORE does not declare a debug boundary, even though it
     * carries the generic preserve-boundary constraint for exceptions. */
    function.instructions[4].flags = ZR_EXEC_IR_FLAG_DEBUG_POLL;
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 5u);
    assert(plan.fallbackCount != 0u);
    {
        TZrBool sawDebugFallback = ZR_FALSE;
        for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
            if (plan.fallbacks[index].reason ==
                    ZR_EXEC_BC_FUSION_FALLBACK_DEBUG_BOUNDARY) {
                sawDebugFallback = ZR_TRUE;
                break;
            }
        }
        assert(sawDebugFallback);
    }
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_branch_fusion_requires_all_resolved_targets(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    make_six_pattern_function(&function);
    function.successors = (TZrExecIrBlockId *)realloc(
            function.successors, 4u * sizeof(*function.successors));
    assert(function.successors != NULL);
    function.successorCount = 4u;
    function.successorCapacity = 4u;
    /* A conditional branch with only one explicit edge is malformed for the
     * ExecIR contract; the fusion matcher must retain both original ops. */
    function.instructions[3].successorRange.count = 1u;
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 5u);
    {
        TZrBool sawBranchFallback = ZR_FALSE;
        for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
            if (plan.fallbacks[index].reason ==
                    ZR_EXEC_BC_FUSION_FALLBACK_BRANCH_TARGET) {
                sawBranchFallback = ZR_TRUE;
                break;
            }
        }
        assert(sawBranchFallback);
    }
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_branch_target_is_remapped_after_a_later_fused_window(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    init_function(&function, 4u);
    function.blocks = (SZrExecIrBlock *)realloc(
            function.blocks, 2u * sizeof(*function.blocks));
    assert(function.blocks != NULL);
    function.blockCount = 2u;
    function.blockCapacity = 2u;
    memset(&function.blocks[1], 0, sizeof(function.blocks[1]));
    function.blocks[0].id = 1u;
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = 2u;
    function.blocks[0].successorRange.start = 0u;
    function.blocks[0].successorRange.count = 2u;
    function.blocks[1].id = 2u;
    function.blocks[1].instructionRange.start = 2u;
    function.blocks[1].instructionRange.count = 2u;
    function.entryBlockId = 1u;

    set_instruction(&function, 0u, ZR_EXEC_IR_OPCODE_COMPARE, 0u, 2u, 1u);
    set_instruction(&function, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                    2u, 1u, 0u);
    set_instruction(&function, 2u, ZR_EXEC_IR_OPCODE_LOAD, 4u, 1u, 3u);
    set_instruction(&function, 3u, ZR_EXEC_IR_OPCODE_ADD, 6u, 2u, 4u);
    function.operands[2] = 1u;
    function.operands[6] = 3u;
    function.instructions[0].effectOut = 1u;
    function.instructions[1].effectIn = 1u;
    function.instructions[2].effectOut = 3u;
    function.instructions[3].effectIn = 3u;
    function.successors = (TZrExecIrBlockId *)calloc(
            2u, sizeof(*function.successors));
    assert(function.successors != NULL);
    function.successorCount = 2u;
    function.successorCapacity = 2u;
    function.successors[0] = 2u;
    function.successors[1] = 2u;
    function.instructions[1].successorRange.start = 0u;
    function.instructions[1].successorRange.count = 2u;

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 2u);
    assert(plan.instructionCount == 2u);
    assert(plan.sideEntries[0].branchTarget == 2u);
    assert(plan.sideEntries[0].branchTargetPc == 1u);
    assert(plan.sideEntries[0].branchTargetCount == 2u);
    assert(plan.sideEntries[0].branchTargets[0] == 2u);
    assert(plan.sideEntries[0].branchTargets[1] == 2u);
    assert(plan.sideEntries[0].branchTargetPcs[0] == 1u);
    assert(plan.sideEntries[0].branchTargetPcs[1] == 1u);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_long_branch_target_uses_u32_side_table_pc(void) {
    enum { instructionCount = 65538u };
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    TZrUInt32 index;

    /* The fixed word stays u16-shaped, but its branch destination must not
     * truncate at that width.  Keep the target in a late sparse block so the
     * projection has to remap a PC above UINT16_MAX. */
    ZrCore_ExecIr_FunctionInit(&function);
    function.functionToken = 0x100u;
    function.signatureHash = UINT64_C(0x1111222233334444);
    function.contract.generation = 7u;
    function.contract.signatureHash = function.signatureHash;
    function.contract.moduleHash = UINT64_C(0xaaaabbbbccccdddd);
    function.contract.layoutHash = UINT64_C(0x12345678);
    function.instructionCount = instructionCount;
    function.instructionCapacity = instructionCount;
    function.instructions = (SZrExecIrInstruction *)calloc(
            instructionCount, sizeof(*function.instructions));
    function.valueCount = 3u;
    function.valueCapacity = 3u;
    function.values = (SZrExecIrValue *)calloc(3u, sizeof(*function.values));
    function.operandCount = 3u;
    function.operandCapacity = 3u;
    function.operands = (TZrExecIrValueId *)calloc(
            3u, sizeof(*function.operands));
    function.resultCount = 1u;
    function.resultCapacity = 1u;
    function.results = (TZrExecIrValueId *)calloc(
            1u, sizeof(*function.results));
    function.blockCount = 2u;
    function.blockCapacity = 2u;
    function.blocks = (SZrExecIrBlock *)calloc(2u, sizeof(*function.blocks));
    function.successorCount = 2u;
    function.successorCapacity = 2u;
    function.successors = (TZrExecIrBlockId *)calloc(
            2u, sizeof(*function.successors));
    assert(function.instructions != NULL && function.values != NULL &&
           function.operands != NULL && function.results != NULL &&
           function.blocks != NULL && function.successors != NULL);
    for (index = 0u; index < function.valueCount; ++index) {
        function.values[index].id = index + 1u;
        function.values[index].typeToken = 9u;
    }
    for (index = 0u; index < instructionCount; ++index) {
        function.instructions[index].opcode = ZR_EXEC_IR_OPCODE_NOP;
    }
    set_instruction(&function, 0u, ZR_EXEC_IR_OPCODE_COMPARE, 0u, 2u, 1u);
    set_instruction(&function, 1u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                    2u, 1u, 0u);
    function.instructions[0].typeToken = 9u;
    function.instructions[1].typeToken = 9u;
    function.instructions[0].effectOut = 1u;
    function.instructions[1].effectIn = 1u;
    function.operands[0] = 2u;
    function.operands[1] = 3u;
    function.operands[2] = 1u;
    function.blocks[0].id = 1u;
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = 2u;
    function.blocks[0].successorRange.start = 0u;
    function.blocks[0].successorRange.count = 2u;
    function.blocks[1].id = 2u;
    function.blocks[1].instructionRange.start = instructionCount - 1u;
    function.blocks[1].instructionRange.count = 1u;
    function.entryBlockId = 1u;
    function.successors[0] = 2u;
    function.successors[1] = 2u;
    function.instructions[1].successorRange.start = 0u;
    function.instructions[1].successorRange.count = 2u;

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 1u);
    assert(plan.sideEntries[0].branchTargetPcs[0] > UINT16_MAX);
    assert(plan.sideEntries[0].branchTargetPcs[1] > UINT16_MAX);
    assert(plan.sideEntries[0].branchTargetPcs[0] ==
           plan.sideEntries[0].branchTargetPcs[1]);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_index_store_variant_preserves_exception_boundary(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    init_function(&function, 2u);
    set_instruction(&function, 0u, ZR_EXEC_IR_OPCODE_PLACE_PROJECT, 0u, 2u, 1u);
    set_instruction(&function, 1u, ZR_EXEC_IR_OPCODE_STORE, 2u, 2u, 0u);
    function.instructions[0].layoutId = 1u;
    function.instructions[1].layoutId = 1u;
    function.instructions[0].effectOut = 1u;
    function.instructions[1].effectIn = 1u;
    function.operands[2] = 1u;
    function.operands[3] = 2u;
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 1u);
    assert(plan.sideEntryCount == 1u);
    assert(plan.sideEntries[0].pattern ==
           ZR_EXEC_BC_FUSION_PATTERN_INDEX_LOAD_STORE);
    assert((plan.sideEntries[0].guardMask &
            ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION) != 0u);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_unresolved_binding_keeps_call_windows_unfused(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    TZrBool sawBindingFallback = ZR_FALSE;

    make_six_pattern_function(&function);
    /* Remove both static rows.  Zero and the explicit all-ones sentinel are
     * intentionally treated as unresolved by the fusion contract. */
    function.instructions[7].bindingRow = 0u;
    function.instructions[10].bindingRow = ZR_CALL_BINDING_SLOT_NONE;
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 4u);
    for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
        if (plan.fallbacks[index].reason ==
                ZR_EXEC_BC_FUSION_FALLBACK_BINDING_MISSING) {
            sawBindingFallback = ZR_TRUE;
            break;
        }
    }
    assert(sawBindingFallback);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static SZrCallBindingContract make_direct_binding_contract(
        TZrUInt32 rid, TZrUInt64 moduleHash) {
    SZrCallBindingContract contract;
    memset(&contract, 0, sizeof(contract));
    contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    contract.targetMetadataToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, rid);
    contract.signatureToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, rid);
    contract.signatureHash = UINT64_C(0x100000000) + rid;
    contract.moduleSignatureHash = moduleHash;
    contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    return contract;
}

static void test_static_binding_facts_authorize_zero_row(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    SZrExecIrBindingFacts facts;
    SZrExecIrBindingSegment segment;
    SZrExecIrBindingRow row;
    SZrCallBindingContract contract;

    make_six_pattern_function(&function);
    /* Projection writes row indexes directly, so the first valid static row
     * is zero.  It must remain conservative unless the accompanying facts
     * table proves that zero belongs to this CALL instruction. */
    function.instructions[7].bindingRow = 0u;
    contract = make_direct_binding_contract(
            1u, function.contract.moduleHash);
    memset(&segment, 0, sizeof(segment));
    segment.index = 0u;
    segment.kind = ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL;
    segment.flags = ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL;
    segment.instructionId = 8u;
    segment.memberToken = contract.targetMetadataToken;
    segment.memberId = ZR_EXEC_IR_BINDING_MEMBER_NONE;
    segment.bindingRow = 0u;
    segment.sourceId = function.instructions[7].sourceId;
    memset(&row, 0, sizeof(row));
    row.rowIndex = 0u;
    row.instructionId = 8u;
    row.segmentIndex = 0u;
    row.contract = contract;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
    row.location.targetIndex = 1u;
    row.sourceId = segment.sourceId;
    ZrParser_ExecIr_BindingFacts_Init(&facts);
    facts.functionToken = function.functionToken;
    facts.signatureHash = function.signatureHash;
    facts.moduleHash = function.contract.moduleHash;
    facts.generation = function.contract.generation;
    facts.segments = &segment;
    facts.segmentCount = 1u;
    facts.rows = &row;
    facts.rowCount = 1u;
    facts.finalSegmentIndex = 0u;
    facts.expectedHash = ZrParser_ExecIr_BindingFacts_Hash(&facts);
    assert(ZrParser_ExecIr_BindingFacts_Validate(&facts, &function,
                                                 &diagnostic));

    ZrParser_ExecBcPatternOptions_Init(&options);
    options.bindingFacts = &facts;
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 6u);
    assert(plan.sideEntries[3].bindingRow == 0u);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_side_table_retains_wide_ids_and_budget_falls_back(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    make_six_pattern_function(&function);
    function.values = (SZrExecIrValue *)realloc(
            function.values, (size_t)65537u * sizeof(*function.values));
    memset(function.values + function.valueCount, 0,
           (size_t)(65537u - function.valueCount) * sizeof(*function.values));
    function.valueCount = 65537u;
    function.valueCapacity = 65537u;
    function.values[65519u].id = 65520u;
    function.values[65519u].typeToken = 9u;
    function.values[65520u].id = 65521u;
    function.values[65520u].typeToken = 9u;
    function.operands[0] = 65520u;
    function.operands[1] = 65521u;
    function.operands[2] = 13u;
    ZrParser_ExecBcPatternOptions_Init(&options);
    options.maxSideTableEntries = 1u;
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 1u);
    assert(plan.sideEntries[0].operands[0] == 65520u);
    assert(plan.fallbackCount >= 1u);
    {
        TZrBool sawBudgetFallback = ZR_FALSE;
        for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
            if (plan.fallbacks[index].reason ==
                    ZR_EXEC_BC_FUSION_FALLBACK_BUDGET ||
                plan.fallbacks[index].reason ==
                    ZR_EXEC_BC_FUSION_FALLBACK_SIDE_TABLE) {
                sawBudgetFallback = ZR_TRUE;
                break;
            }
        }
        assert(sawBudgetFallback);
    }
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_known_nonuniform_types_still_fuse_index_load(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    init_function(&function, 2u);
    set_instruction(&function, 0u, ZR_EXEC_IR_OPCODE_PLACE_PROJECT,
                    0u, 2u, 1u);
    set_instruction(&function, 1u, ZR_EXEC_IR_OPCODE_LOAD, 2u, 1u, 4u);
    /* A projection result, its receiver/index operands, and the loaded value
     * legitimately have different types.  "typed" means each identity is
     * known, not that a place and its loaded value share one token. */
    function.values[0].typeToken = 41u;
    function.values[1].typeToken = 42u;
    function.values[2].typeToken = 43u;
    function.values[3].typeToken = 44u;
    function.instructions[0].typeToken = 41u;
    function.instructions[1].typeToken = 44u;
    function.operands[0] = 2u;
    function.operands[1] = 3u;
    function.operands[2] = 1u;
    function.instructions[0].layoutId = 1u;
    function.instructions[1].layoutId = 1u;
    function.instructions[0].effectOut = 1u;
    function.instructions[1].effectIn = 1u;

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 1u);
    assert(plan.sideEntries[0].pattern ==
           ZR_EXEC_BC_FUSION_PATTERN_INDEX_LOAD_STORE);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_memory_token_discontinuity_keeps_window_unfused(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    TZrBool sawEffectFallback = ZR_FALSE;

    make_six_pattern_function(&function);
    function.memoryTokenPool = (TZrExecIrMemoryTokenId *)calloc(
            2u, sizeof(*function.memoryTokenPool));
    assert(function.memoryTokenPool != NULL);
    function.memoryTokenCount = 2u;
    function.memoryTokenCapacity = 2u;
    function.memoryTokenPool[0] = 1u;
    function.memoryTokenPool[1] = 2u;
    function.instructions[0].memoryOut.start = 0u;
    function.instructions[0].memoryOut.count = 1u;
    function.instructions[1].memoryIn.start = 1u;
    function.instructions[1].memoryIn.count = 1u;

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 5u);
    for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
        if (plan.fallbacks[index].reason ==
                ZR_EXEC_BC_FUSION_FALLBACK_EFFECT_MISMATCH) {
            sawEffectFallback = ZR_TRUE;
            break;
        }
    }
    assert(sawEffectFallback);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_state_map_liveness_blocks_erasing_intermediate_value(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    SZrExecIrStateMap *map;
    TZrUInt64 before;
    TZrUInt64 after;
    TZrBool sawMultipleUse = ZR_FALSE;
    TZrBool sawSafepointFallback = ZR_FALSE;

    make_six_pattern_function(&function);
    map = (SZrExecIrStateMap *)calloc(1u, sizeof(*map));
    assert(map != NULL);
    ZrCore_ExecIr_StateMapInit(map);
    map->functionToken = function.functionToken;
    map->signatureHash = function.signatureHash;
    map->entries = (SZrExecIrStateMapEntry *)calloc(1u, sizeof(*map->entries));
    map->valuePool = (TZrExecIrValueId *)calloc(1u, sizeof(*map->valuePool));
    map->ownerStatePool = (TZrUInt32 *)calloc(1u, sizeof(*map->ownerStatePool));
    assert(map->entries != NULL && map->valuePool != NULL &&
           map->ownerStatePool != NULL);
    map->entryCount = map->entryCapacity = 1u;
    map->valueCount = map->valueCapacity = 1u;
    map->ownerStateCount = map->ownerStateCapacity = 1u;
    map->entries[0].instructionId = 1u;
    map->entries[0].liveValues.start = 0u;
    map->entries[0].liveValues.count = 1u;
    map->entries[0].ownerStates.start = 0u;
    map->entries[0].ownerStates.count = 1u;
    map->valuePool[0] = 13u;
    map->ownerStatePool[0] = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    function.stateMap = map;

    before = ZrParser_ExecBcFusion_InputHash(&function);
    assert(before != 0u);
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 5u);
    for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
        if (plan.fallbacks[index].reason ==
                ZR_EXEC_BC_FUSION_FALLBACK_MULTIPLE_USE) {
            sawMultipleUse = ZR_TRUE;
        }
    }
    assert(sawMultipleUse);
    map->entries[0].boundaryFlags = ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND;
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 5u);
    for (TZrUInt32 index = 0u; index < plan.fallbackCount; ++index) {
        if (plan.fallbacks[index].reason == ZR_EXEC_BC_FUSION_FALLBACK_SAFEPOINT) {
            sawSafepointFallback = ZR_TRUE;
            break;
        }
    }
    assert(sawSafepointFallback);
    map->generation = 1u;
    after = ZrParser_ExecBcFusion_InputHash(&function);
    assert(after != 0u && after != before);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_malformed_gc_map_rejects_projection(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    SZrExecIrGcMap *map;

    make_six_pattern_function(&function);
    map = (SZrExecIrGcMap *)calloc(1u, sizeof(*map));
    assert(map != NULL);
    ZrCore_ExecIr_GcMapInit(map);
    map->entryCount = 1u;
    map->entryCapacity = 1u;
    /* A non-zero entry count with no entry storage is malformed; fusion must
     * reject it before any matcher dereferences the map. */
    function.gcMap = map;
    function.gcMapCount = 1u;
    function.gcMapCapacity = 1u;
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(!ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                              &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_deopt_resume_ids_are_projected_to_source_events(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    make_six_pattern_function(&function);
    function.deoptStates = (SZrExecIrDeoptState *)calloc(
            1u, sizeof(*function.deoptStates));
    assert(function.deoptStates != NULL);
    function.deoptStateCount = 1u;
    function.deoptStateCapacity = 1u;
    function.deoptStates[0].id = 91u;
    function.deoptStates[0].source = function.instructions[6].sourceId;
    function.deoptStates[0].resumeId = 701u;
    /* BINDING_CALL explicitly represents a reentrant/deopt boundary; attach
     * the resume point to its head so the test exercises both preservation
     * and map projection rather than asking LOAD_ADD_INT to absorb it. */
    function.instructions[6].deoptId = 91u;

    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.fusedCount == 6u);
    assert(plan.sideEntries[3].headResumeId == 701u);
    assert(plan.sourceMaps[6].resumeId == 701u);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_generation_and_input_hash_invalidation_is_structured(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcFusionDiagnostic fusionDiagnostic;
    SZrExecBcPatternOptions options;
    TZrUInt64 hash;

    make_six_pattern_function(&function);
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    hash = plan.generatedHash;
    assert(hash != 0u);
    assert(ZrParser_ExecBcFusion_CheckValidity(&plan, &function, 7u,
                                               &fusionDiagnostic) ==
           ZR_EXEC_BC_FUSION_INVALIDATION_NONE);
    function.contract.generation = 8u;
    assert(ZrParser_ExecBcFusion_CheckValidity(&plan, &function, 8u,
                                               &fusionDiagnostic) ==
           ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION);
    ZrParser_ExecBcFusion_Invalidate(&plan,
                                     ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION);
    assert(plan.fusedCount == 0u);
    assert(plan.instructionCount == plan.originalInstructionCount);
    assert(plan.generatedHash != hash);
    assert(!ZrParser_ExecBcFusion_Validate(&plan, &diagnostic));
    ZrParser_ExecBcFusion_Invalidate(
            &plan, (EZrExecBcFusionInvalidationReason)UINT32_MAX);
    assert(plan.invalidationReason == ZR_EXEC_BC_FUSION_INVALIDATION_EXPLICIT);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_generation_is_deterministic_and_select_wrapper_uses_output(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan first;
    SZrExecBcFusionPlan second;
    SZrExecBcFusionPlan selected;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;

    make_six_pattern_function(&function);
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&first);
    ZrParser_ExecBcFusionPlan_Init(&second);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &first,
                                             &diagnostic));
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &second,
                                             &diagnostic));
    assert(first.generatedHash == second.generatedHash);
    assert(first.instructionCount == second.instructionCount);
    assert(memcmp(first.instructions, second.instructions,
                  first.instructionCount * sizeof(*first.instructions)) == 0);
    ZrParser_ExecBcFusionPlan_Init(&selected);
    options.output = &selected;
    assert(ZrParser_ExecIr_SelectExecBcPatterns(&function, &options,
                                                &diagnostic));
    assert(selected.generatedHash == first.generatedHash);
    ZrParser_ExecBcFusionPlan_Free(&selected);
    ZrParser_ExecBcFusionPlan_Free(&second);
    ZrParser_ExecBcFusionPlan_Free(&first);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_contract_mismatch_is_rejected_without_overwriting_output(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    TZrUInt64 oldHash;

    make_six_pattern_function(&function);
    ZrParser_ExecBcPatternOptions_Init(&options);
    options.expectedSignatureHash = UINT64_C(0x9999);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    plan.generatedHash = UINT64_C(0x55);
    oldHash = plan.generatedHash;
    assert(!ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                              &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    assert(plan.generatedHash == oldHash);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_empty_function_projection_is_valid(void) {
    SZrExecIrFunction function;
    SZrExecBcFusionPlan plan;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcPatternOptions options;
    TZrUInt64 generatedHash;

    ZrCore_ExecIr_FunctionInit(&function);
    function.functionToken = 1u;
    ZrParser_ExecBcPatternOptions_Init(&options);
    ZrParser_ExecBcFusionPlan_Init(&plan);
    assert(ZrParser_ExecIr_BuildExecBcFusion(&function, &options, &plan,
                                             &diagnostic));
    assert(plan.valid == ZR_TRUE);
    assert(plan.originalInstructionCount == 0u);
    assert(plan.instructionCount == 0u);
    assert(plan.fusedCount == 0u);
    assert(plan.sourceMapCount == 0u);
    generatedHash = plan.generatedHash;
    assert(ZrParser_ExecBcFusion_CheckValidity(
                   &plan, &function, 0u, ZR_NULL) ==
           ZR_EXEC_BC_FUSION_INVALIDATION_NONE);
    ZrParser_ExecBcFusion_Invalidate(
            &plan, ZR_EXEC_BC_FUSION_INVALIDATION_EXPLICIT);
    assert(plan.generatedHash != generatedHash);
    ZrParser_ExecBcFusionPlan_Free(&plan);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_six_patterns_emit_fixed_width_words_and_side_maps();
    test_boundary_and_type_failures_keep_original_sequence();
    test_only_declared_boundaries_are_preserved();
    test_branch_fusion_requires_all_resolved_targets();
    test_branch_target_is_remapped_after_a_later_fused_window();
    test_long_branch_target_uses_u32_side_table_pc();
    test_index_store_variant_preserves_exception_boundary();
    test_unresolved_binding_keeps_call_windows_unfused();
    test_static_binding_facts_authorize_zero_row();
    test_side_table_retains_wide_ids_and_budget_falls_back();
    test_known_nonuniform_types_still_fuse_index_load();
    test_memory_token_discontinuity_keeps_window_unfused();
    test_state_map_liveness_blocks_erasing_intermediate_value();
    test_malformed_gc_map_rejects_projection();
    test_deopt_resume_ids_are_projected_to_source_events();
    test_generation_and_input_hash_invalidation_is_structured();
    test_generation_is_deterministic_and_select_wrapper_uses_output();
    test_contract_mismatch_is_rejected_without_overwriting_output();
    test_empty_function_projection_is_valid();
    return 0;
}
