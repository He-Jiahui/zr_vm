#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/aot_ir_projection_descriptor.h"
#include "backend_aot_ir_adapter.h"

static void fill_contract(SZrExecutionContract *contract,
                          TZrMetadataToken token,
                          TZrUInt64 moduleHash,
                          TZrUInt64 signatureHash,
                          TZrUInt64 layoutHash) {
    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = token;
    contract->signatureHash = signatureHash;
    contract->layoutHash = layoutHash;
    contract->moduleHash = moduleHash;
}

int main(void) {
    const TZrUInt32 operands[] = {1u};
    const TZrUInt32 results[] = {1u};
    const TZrUInt32 successors[] = {1u};
    const SZrExecBcInstruction instruction = {
        .opcode = ZR_EXEC_IR_OPCODE_RETURN,
        .operands = {{.offset = 0u}, .count = 1u},
        .results = {{.offset = 0u}, .count = 1u},
        .typeToken = 11u, .sourceId = 17u};
    const SZrExecBcBlock block = {
        .id = 1u, .flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY,
        .instructions = {{.offset = 0u}, .count = 1u},
        .successors = {{.offset = 0u}, .count = 0u},
        .terminatorInstructionId = 1u};
    const TZrUInt32 gcSlotPool[] = {0u};
    const SZrExecIrGcMapEntry gcEntries[] = {
        {1u, {.start = 0u, .count = 1u}, {.start = 0u, .count = 0u}}};
    const SZrExecIrGcMap gcMap = {
        (SZrExecIrGcMapEntry *)gcEntries, 1u, 1u,
        (TZrUInt32 *)gcSlotPool, 1u, 1u, ZR_NULL, 0u, 0u,
        9u, 17u, {.start = 0u, .count = 1u}};
    const TZrExecIrValueId gcRoots[] = {1u};
    SZrAotIrProjection input = {
        .functionId = 1u,
        .functionToken = 7u,
        .signatureHash = 11u,
        .entryBlockId = 1u,
        .frameLayoutHash = 22u,
        .logicalSlotCount = 1u,
        .storageSlotCount = 1u,
        .frameByteSize = 16u,
        .frameByteAlign = 8u,
        .frameSlotCount = 1u,
        .frameSlots = (SZrExecIrFrameSlot[]){ {1u, 0u, 8u, 8u, 17u, 0u} },
        .instructionCount = 1u,
        .instructions = (SZrExecBcInstruction *)&instruction,
        .operands = (TZrExecIrValueId *)operands,
        .operandCount = 1u,
        .results = (TZrExecIrValueId *)results,
        .resultCount = 1u,
        .valueSlots = (TZrUInt32[]){0u},
        .valueSlotCount = 1u,
        .physicalSlotCount = 1u,
        .gcMap = gcMap,
        .gcRoots = (TZrExecIrValueId *)gcRoots,
        .gcRootCount = 1u,
        .gcMapPresent = ZR_TRUE,
        .gcMapCount = 1u,
        .blocks = (SZrExecBcBlock *)&block,
        .blockCount = 1u,
        .successors = (TZrExecIrBlockId *)successors,
        .successorCount = 1u,
        .contract = {0},
        .ownershipTag = ZR_EXEC_IR_PROJECTION_TAG
    };
    SZrAotIrProjectionDescriptor descriptor;
    SZrAotIrTargetContract target = {ZR_AOT_IR_TARGET_ABI_VERSION, sizeof(void *), 0u, 0u, 67u, 66u};
    SZrExecutionContract moduleContract;
    SZrAotIrDiagnostic diagnostic;
    SZrBackendAotIrDiagnostic backendDiagnostic;
    TZrUInt32 instructionCount = 0u;
    TZrUInt64 fullHash;
    TZrUInt64 noGcHash;
    SZrAotIrFunction noGcFunction;
    SZrAotIrModule noGcModule;

    fill_contract(&input.contract, input.functionToken, 33u,
                  input.signatureHash, input.frameLayoutHash);
    fill_contract(&moduleContract, 0u, 33u, 44u, 55u);
    memset(&descriptor, 0, sizeof(descriptor));
    if (!ZrParser_AotIrProjection_BuildDescriptor(
                   &input, &target,
                   &moduleContract, &descriptor, &diagnostic)) {
        (void)diagnostic;
        fputs("projection descriptor build failed\n", stderr);
        return 1;
    }
    assert(descriptor.owner == &input);
    assert(descriptor.module.functions == &descriptor.function);
    assert(descriptor.function.instructions[0].typeToken == 11u);
    assert(descriptor.function.frameSlots[0].byteSize == 8u);
    assert(descriptor.function.gcMap != ZR_NULL &&
           descriptor.function.gcMap->entries[0].site == 1u &&
           descriptor.function.gcMap->slotIndexPool[0] == 0u);
    assert(descriptor.function.gcRootCount == 1u &&
           descriptor.function.gcRootPool[0] == 1u);
    fullHash = ZrCore_AotIr_HashModule(&descriptor.module);
    noGcFunction = descriptor.function;
    noGcFunction.gcMap = ZR_NULL;
    noGcFunction.gcRootPool = ZR_NULL;
    noGcFunction.gcRootCount = 0u;
    noGcModule = descriptor.module;
    noGcModule.functions = &noGcFunction;
    noGcHash = ZrCore_AotIr_HashModule(&noGcModule);
    assert(fullHash != 0u && noGcHash != 0u && fullHash != noGcHash);
    assert(ZrCore_AotIr_ValidateModule(&descriptor.module, &diagnostic) ==
           ZR_AOT_IR_OK);
    assert(backend_aot_ir_adapter_validate(&descriptor.module,
                                           &backendDiagnostic) ==
           ZR_BACKEND_AOT_IR_OK);
    assert(backend_aot_ir_adapter_count_instructions(
                   &descriptor.module, &instructionCount, &backendDiagnostic));
    assert(instructionCount == 1u);
    ZrParser_AotIrProjection_FreeDescriptor(&descriptor);
    assert(descriptor.owner == ZR_NULL);
    return 0;
}
