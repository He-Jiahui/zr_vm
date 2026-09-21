#include <assert.h>
#include <string.h>

#include "zr_vm_parser/aot_ir_lowering.h"

static void contract(SZrExecutionContract *c, TZrUInt32 token,
                     TZrUInt64 moduleHash, TZrUInt64 sig, TZrUInt64 layout) {
    memset(c, 0, sizeof(*c));
    c->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    c->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    c->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    c->generation = 1u;
    c->targetToken = token;
    c->signatureHash = sig;
    c->layoutHash = layout;
    c->moduleHash = moduleHash;
}

int main(void) {
    const TZrUInt32 operands[] = {1u, 2u};
    const TZrUInt32 results[] = {3u};
    const TZrUInt32 successors[] = {1u, 1u};
    const SZrAotIrInstruction instructions[] = {
        {1u, ZR_EXEC_IR_OPCODE_ADD, 0u, {0u, 1u}, {0u, 2u}, {0u, 0u}, {0u, 0u}, 0u, 0u, 4u, 0u, 0u, 0u},
        {2u, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, 0u, {0u, 0u}, {0u, 1u}, {0u, 2u}, {0u, 0u}, 0u, 0u, 5u, 0u, 0u, 0u},
        {3u, ZR_EXEC_IR_OPCODE_CALL, ZR_EXEC_IR_FLAG_MAY_ALLOCATE, {0u, 1u}, {0u, 1u}, {0u, 0u}, {0u, 0u}, 0u, 0u, 6u, 0u, 0u, 9u},
        {4u, ZR_EXEC_IR_OPCODE_SUSPEND, ZR_EXEC_IR_FLAG_MAY_SUSPEND, {0u, 0u}, {0u, 1u}, {0u, 0u}, {0u, 0u}, 0u, 0u, 7u, 1u, 0u, 0u},
        /* NOP is deliberately retained as a runtime bridge in the shared
         * lowering so its accounting cannot be hidden by an adapter. */
        {5u, ZR_EXEC_IR_OPCODE_NOP, 0u, {0u, 0u}, {0u, 0u}, {0u, 0u}, {0u, 0u}, 0u, 0u, 8u, 0u, 0u, 0u}
    };
    const SZrAotIrBlock block = {1u, ZR_EXEC_IR_BLOCK_FLAG_ENTRY, {0u, 5u}, {0u, 0u}, {0u, 0u}, 2u};
    SZrAotIrFunction function;
    SZrAotIrModule module;
    SZrAotIrLoweringRecord records[5];
    SZrAotIrLoweringResult result;
    SZrAotIrEmitOptions cOptions;
    SZrAotIrEmitOptions llvmOptions;
    SZrAotIrEmitResult cResult;
    SZrAotIrEmitResult llvmResult;
    SZrAotIrDiagnostic diagnostic;

    memset(&function, 0, sizeof(function));
    function.id = 1u;
    function.functionToken = 1u;
    function.signatureHash = 11u;
    function.frameLayout.frameByteSize = 16u;
    function.frameLayout.frameByteAlign = 8u;
    function.frameLayout.layoutHash = 22u;
    function.blocks = &block;
    function.blockCount = 1u;
    function.instructions = instructions;
    function.instructionCount = 5u;
    function.operandPool = operands;
    function.operandCount = 2u;
    function.resultPool = results;
    function.resultCount = 1u;
    function.successorPool = successors;
    function.successorCount = 2u;
    contract(&function.contract, 1u, 33u, 11u, 22u);
    memset(&module, 0, sizeof(module));
    module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    module.moduleHash = 33u;
    module.functions = &function;
    module.functionCount = 1u;
    contract(&module.contract, 0u, 33u, 44u, 55u);
    module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    module.target.pointerSize = (TZrUInt32)sizeof(void *);
    module.target.targetTripleHash = 67u;
    module.target.abiHash = 66u;
    result.records = records;
    result.capacity = 5u;
    if (!ZrParser_AotIr_LowerShared(&module, &result, &diagnostic)) {
        fprintf(stderr, "status=%d fn=%u block=%u ins=%u idx=%u exp=%llu act=%llu\\n", (int)diagnostic.status,
                diagnostic.functionId, diagnostic.blockId, diagnostic.instructionId, diagnostic.index,
                (unsigned long long)diagnostic.expected, (unsigned long long)diagnostic.actual);
        return 1;
    }
    assert(result.count == 5u);
    assert(result.records[0].kind == ZR_AOT_IR_LOWERING_TYPED_SCALAR);
    assert(result.records[1].kind == ZR_AOT_IR_LOWERING_CONTROL);
    assert(result.records[2].kind == ZR_AOT_IR_LOWERING_CALL);
    assert(result.records[3].kind == ZR_AOT_IR_LOWERING_ASYNC_BOUNDARY);
    assert(result.records[4].kind == ZR_AOT_IR_LOWERING_RUNTIME_BRIDGE);
    assert(result.sourceHash != 0u && result.loweringHash != 0u);
    assert(ZrParser_AotIr_LoweringIsPointerFree(&result));
    memset(&cOptions, 0, sizeof(cOptions));
    cOptions.target = ZR_AOT_IR_EMITTER_C;
    cOptions.strictFloatingPoint = ZR_TRUE;
    cOptions.allowRuntimeBridge = ZR_TRUE;
    assert(ZrParser_AotIr_EmitC(&module, &cOptions, &cResult, &diagnostic));
    assert(cResult.nativeCount == 3u);
    assert(cResult.runtimeBridgeCount == 2u);
    llvmOptions = cOptions;
    llvmOptions.target = ZR_AOT_IR_EMITTER_LLVM;
    assert(ZrParser_AotIr_EmitLlvm(&module, &llvmOptions, &llvmResult, &diagnostic));
    assert(cResult.nativeCount == llvmResult.nativeCount);
    assert(cResult.runtimeBridgeCount == llvmResult.runtimeBridgeCount);
    assert(cResult.sourceHash == llvmResult.sourceHash);
    assert(cResult.contractHash != llvmResult.contractHash);
    cOptions.allowRuntimeBridge = ZR_FALSE;
    assert(!ZrParser_AotIr_EmitC(&module, &cOptions, &cResult, &diagnostic));
    assert(diagnostic.status == ZR_AOT_IR_UNSUPPORTED);
    function.relocationCount = 1u;
    cOptions.allowRuntimeBridge = ZR_TRUE;
    assert(!ZrParser_AotIr_EmitC(&module, &cOptions, &cResult, &diagnostic));
    assert(diagnostic.status == ZR_AOT_IR_RELOCATION);
    return 0;
}
