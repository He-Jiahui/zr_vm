#include <assert.h>
#include <string.h>

#include "zr_vm_core/aot_ir.h"

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
    const TZrUInt32 operandPool[] = {1u};
    const TZrUInt32 resultPool[] = {2u};
    const SZrAotIrInstruction instructions[] = {
        {1u, ZR_EXEC_IR_OPCODE_RETURN, 0u, {0u, 0u}, {0u, 1u},
         {0u, 0u}, {0u, 0u}, 0u, 0u, 1u, 0u, 0u, 0u}
    };
    const SZrAotIrBlock blocks[] = {
        {1u, ZR_EXEC_IR_BLOCK_FLAG_ENTRY, {0u, 1u}, {0u, 0u},
         {0u, 0u}, 1u}
    };
    SZrAotIrFunction function;
    SZrAotIrModule module;
    SZrAotIrDiagnostic diagnostic;
    TZrUInt64 hash;

    memset(&function, 0, sizeof(function));
    function.id = 1u;
    function.functionToken = 1u;
    function.signatureHash = UINT64_C(11);
    function.frameLayout.logicalSlotCount = 1u;
    function.frameLayout.storageSlotCount = 1u;
    function.frameLayout.frameByteSize = 16u;
    function.frameLayout.frameByteAlign = 8u;
    function.frameLayout.layoutHash = UINT64_C(22);
    function.blocks = blocks;
    function.blockCount = 1u;
    function.instructions = instructions;
    function.instructionCount = 1u;
    function.operandPool = operandPool;
    function.operandCount = 1u;
    function.resultPool = resultPool;
    function.resultCount = 1u;
    fill_contract(&function.contract, function.functionToken, UINT64_C(33),
                  function.signatureHash, function.frameLayout.layoutHash);

    memset(&module, 0, sizeof(module));
    module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    module.moduleHash = UINT64_C(33);
    module.functions = &function;
    module.functionCount = 1u;
    fill_contract(&module.contract, 0u, module.moduleHash, UINT64_C(44), UINT64_C(55));
    module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    module.target.pointerSize = (TZrUInt32)sizeof(void *);
    module.target.abiHash = UINT64_C(66);

    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    assert(ZrCore_AotIr_IsRelocationFree(&module, &diagnostic));
    hash = ZrCore_AotIr_HashModule(&module);
    assert(hash != 0u);
    assert(hash == ZrCore_AotIr_HashModule(&module));
    {
        SZrAotIrModule malformed = module;
        SZrAotIrFunction malformedFunction = function;
        SZrAotIrInstruction malformedInstruction = instructions[0];
        malformedInstruction.operands.offset = 2u;
        malformedFunction.instructions = &malformedInstruction;
        malformed.functions = &malformedFunction;
        assert(ZrCore_AotIr_ValidateModule(&malformed, &diagnostic) != ZR_AOT_IR_OK);
        assert(diagnostic.status == ZR_AOT_IR_INVALID_OPCODE ||
               diagnostic.status == ZR_AOT_IR_INVALID_RANGE);
    }
    module.relocationCount = 1u;
    assert(!ZrCore_AotIr_IsRelocationFree(&module, &diagnostic));
    assert(diagnostic.status == ZR_AOT_IR_RELOCATION);
    return 0;
}
