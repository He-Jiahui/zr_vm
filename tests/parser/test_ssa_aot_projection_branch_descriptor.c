#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_parser/aot_ir_projection_descriptor.h"

static void fill_contract(SZrExecutionContract *contract,
                          TZrMetadataToken token, TZrUInt64 signatureHash,
                          TZrUInt64 layoutHash) {
    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = token;
    contract->moduleHash = 33u;
    contract->signatureHash = signatureHash;
    contract->layoutHash = layoutHash;
}

static void build_two_block_exec_ir(SZrExecIrFunction *function) {
    TZrExecIrValueId valueId;
    TZrExecIrBlockId entry, exitBlock;
    SZrExecIrInstruction instruction;
    SZrExecIrRange resultRange, returnOperands;

    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 7u;
    function->signatureHash = 11u;
    fill_contract(&function->contract, 7u, 11u, 22u);
    function->frameLayout = (SZrExecIrFrameLayout *)calloc(
            1u, sizeof(*function->frameLayout));
    assert(function->frameLayout != NULL);
    function->frameLayout->slots = (SZrExecIrFrameSlot *)calloc(
            1u, sizeof(*function->frameLayout->slots));
    assert(function->frameLayout->slots != NULL);
    function->frameLayout->slotCount = 1u;
    function->frameLayout->slotCapacity = 1u;
    function->frameLayout->storageSlotCount = 1u;
    function->frameLayout->logicalSlotCount = 1u;
    function->frameLayout->frameByteSize = 8u;
    function->frameLayout->frameByteAlign = 8u;
    function->frameLayout->layoutHash = 22u;
    function->frameLayout->slots[0].slotId = 1u;
    function->frameLayout->slots[0].byteSize = 8u;
    function->frameLayout->slots[0].byteAlign = 8u;
    function->frameLayout->slots[0].typeToken = 17u;
    function->frameLayout->slots[0].kind = ZR_EXEC_IR_FRAME_SLOT_VALUE;

    valueId = ZrCore_ExecIr_FunctionAddValue(
            function, 17u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    exitBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    assert(valueId == 1u && entry == 1u && exitBlock == 2u);
    function->entryBlockId = entry;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(
            function, &exitBlock, 1u, &function->blocks[0].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
            function, &entry, 1u, &function->blocks[1].predecessorRange));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &valueId, 1u, &resultRange));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &valueId, 1u, &returnOperands));

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = function->blocks[0].successorRange;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            function, &instruction, NULL));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = resultRange;
    instruction.typeToken = 17u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            function, &instruction, NULL));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperands;
    instruction.typeToken = 17u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(
            function, &instruction, NULL));
    function->blocks[0].instructionRange.count = 1u;
    function->blocks[0].terminatorInstructionId = 1u;
    function->blocks[1].instructionRange.start = 1u;
    function->blocks[1].instructionRange.count = 2u;
    function->blocks[1].terminatorInstructionId = 3u;
}

int main(void) {
    const SZrExecIrConstant constant = {17u, 0u, UINT64_C(42)};
    SZrExecIrFunction function;
    SZrAotIrProjection projection = {0};
    SZrAotIrProjectionDescriptor descriptor = {0};
    SZrAotIrTargetContract target = {
        ZR_AOT_IR_TARGET_ABI_VERSION, sizeof(void *), 0u, 0u, 67u, 66u};
    SZrExecutionContract moduleContract;
    SZrExecIrDiagnostic irDiagnostic;
    SZrAotIrDiagnostic aotDiagnostic;
    TZrExecIrBlockId savedEdge;
    const TZrUInt32 *ownedPool;

    build_two_block_exec_ir(&function);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &irDiagnostic));
    assert(ZrParser_ExecIr_LowerAotWithConstants(
            &function, &constant, 1u, &projection, &irDiagnostic));
    assert(projection.callableAbi.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
    assert(projection.blockCount == 2u);
    assert(projection.successorCount == 1u && projection.successors[0] == 2u);
    assert(projection.predecessorCount == 1u &&
           projection.predecessors[0] == 1u);
    fill_contract(&moduleContract, 0u, 44u, 55u);

    if (!ZrParser_AotIrProjection_BuildDescriptor(
            &projection, &target, &moduleContract, &descriptor,
            &aotDiagnostic)) {
        fprintf(stderr, "descriptor status=%s block=%u expected=%llu actual=%llu\n",
                ZrCore_AotIr_StatusName(aotDiagnostic.status),
                (unsigned)aotDiagnostic.blockId,
                (unsigned long long)aotDiagnostic.expected,
                (unsigned long long)aotDiagnostic.actual);
        assert(!"verified branch projection must build a descriptor");
    }
    assert(descriptor.owner == &projection);
    assert(descriptor.function.successorCount == 2u);
    assert(descriptor.function.successorPool[0] == 2u);
    assert(descriptor.function.successorPool[1] == 1u);
    assert(descriptor.function.blocks[0].successors.offset == 0u);
    assert(descriptor.function.blocks[0].successors.count == 1u);
    assert(descriptor.function.blocks[1].predecessors.offset == 1u);
    assert(descriptor.function.blocks[1].predecessors.count == 1u);
    assert(descriptor.function.instructions[0].successors.offset == 0u);
    assert(descriptor.function.instructions[0].successors.count == 1u);
    assert(ZrCore_AotIr_ValidateModule(&descriptor.module,
                                       &aotDiagnostic) == ZR_AOT_IR_OK);
    ownedPool = descriptor.function.successorPool;
    assert(ownedPool != projection.successors &&
           ownedPool != projection.predecessors);

    projection.blocks[1].predecessors.start = 1u;
    assert(!ZrParser_AotIrProjection_BuildDescriptor(
            &projection, &target, &moduleContract, &descriptor,
            &aotDiagnostic));
    assert(descriptor.function.successorPool == ownedPool);
    projection.blocks[1].predecessors.start = 0u;
    projection.instructions[0].successorRange.start = 1u;
    assert(!ZrParser_AotIrProjection_BuildDescriptor(
            &projection, &target, &moduleContract, &descriptor,
            &aotDiagnostic));
    assert(descriptor.function.successorPool == ownedPool);
    projection.instructions[0].successorRange.start = 0u;

    savedEdge = projection.predecessors[0];
    projection.predecessors[0] = 99u;
    assert(!ZrParser_AotIrProjection_BuildDescriptor(
            &projection, &target, &moduleContract, &descriptor,
            &aotDiagnostic));
    assert(descriptor.function.successorPool == ownedPool);
    assert(descriptor.function.successorPool[1] == 1u);
    projection.predecessors[0] = savedEdge;
    assert(ZrParser_AotIrProjection_BuildDescriptor(
            &projection, &target, &moduleContract, &descriptor,
            &aotDiagnostic));
    assert(descriptor.function.successorPool != projection.successors);
    ZrParser_AotIrProjection_FreeDescriptor(&descriptor);
    assert(descriptor.function.successorPool == NULL &&
           descriptor.owner == NULL);
    ZrParser_AotIrProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
    return 0;
}
