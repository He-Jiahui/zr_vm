#include <assert.h>
#include <string.h>

#include "zr_vm_parser/aot_ir_projection_descriptor.h"
#include "backend_aot_ir_adapter.h"

static void fill_contract(SZrExecutionContract *contract,
                          TZrMetadataToken token, TZrUInt64 moduleHash,
                          TZrUInt64 signatureHash, TZrUInt64 layoutHash) {
    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = token;
    contract->moduleHash = moduleHash;
    contract->signatureHash = signatureHash;
    contract->layoutHash = layoutHash;
}

static SZrAotIrModule make_module(SZrAotIrFunction *function,
                                 SZrAotIrBlock *block,
                                 SZrAotIrInstruction *instructions,
                                 const SZrExecIrConstant *constant,
                                 const TZrUInt32 *valueId) {
    SZrAotIrModule module;
    memset(function, 0, sizeof(*function));
    memset(block, 0, sizeof(*block));
    memset(instructions, 0, sizeof(*instructions) * 2u);
    memset(&module, 0, sizeof(module));
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instructions[0].results.count = 1u;
    instructions[0].typeToken = 17u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instructions[1].operands.count = 1u;
    instructions[1].typeToken = 17u;
    block->id = 1u;
    block->flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY;
    block->instructions.count = 2u;
    block->terminatorInstructionId = 2u;
    function->id = 1u;
    function->functionToken = 7u;
    function->signatureHash = 11u;
    function->frameLayout.frameByteSize = 16u;
    function->frameLayout.frameByteAlign = 8u;
    function->frameLayout.layoutHash = 22u;
    function->blocks = block;
    function->blockCount = 1u;
    function->instructions = instructions;
    function->instructionCount = 2u;
    function->operandPool = valueId;
    function->operandCount = 1u;
    function->resultPool = valueId;
    function->resultCount = 1u;
    fill_contract(&function->contract, 7u, 33u, 11u, 22u);
    module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    module.moduleHash = 33u;
    module.functions = function;
    module.functionCount = 1u;
    module.constantPool = constant;
    module.constantCount = 1u;
    module.target.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    module.target.pointerSize = (TZrUInt32)sizeof(void *);
    module.target.targetTripleHash = 67u;
    module.target.abiHash = 66u;
    fill_contract(&module.contract, 0u, 33u, 44u, 55u);
    return module;
}

static void explicit_abi_is_validated_and_hashed(void) {
    const TZrUInt32 valueId = 1u;
    const SZrExecIrConstant constant = {17u, 0u, UINT64_C(42)};
    SZrAotIrInstruction instructions[2];
    SZrAotIrBlock block;
    SZrAotIrFunction function;
    SZrAotIrModule module = make_module(&function, &block, instructions,
                                       &constant, &valueId);
    SZrAotIrCallableAbi observed;
    SZrAotIrDiagnostic diagnostic;
    TZrUInt64 unknownHash;
    TZrUInt64 knownHash;

    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    unknownHash = ZrCore_AotIr_HashModule(&module);
    memset(&observed, 0xff, sizeof(observed));
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, &observed,
                                             &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(diagnostic.functionId == function.id);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);

    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    function.callableAbi.returnTypeToken = 17u;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    knownHash = ZrCore_AotIr_HashModule(&module);
    assert(knownHash != 0u && knownHash != unknownHash);
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, &observed,
                                             &diagnostic) == ZR_AOT_IR_OK);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64);
    assert(observed.returnTypeToken == 17u);

    function.callableAbi.returnTypeToken = 18u;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    assert(diagnostic.functionId == function.id);
    assert(diagnostic.instructionId == instructions[1].id);
    function.callableAbi.returnTypeToken = 0u;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    function.callableAbi.returnTypeToken = 17u;
    instructions[1].operands.count = 0u;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    instructions[1].operands.count = 1u;
    instructions[0].typeToken = 0u;
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, &observed,
                                             &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
    instructions[0].typeToken = 17u;
    function.callableAbi.kind = (EZrAotIrCallableAbiKind)99u;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_UNKNOWN;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
}

static void qualification_rejects_missing_return_and_definitions(void) {
    const TZrUInt32 valueId = 1u;
    const SZrExecIrConstant constant = {17u, 0u, UINT64_C(42)};
    SZrAotIrInstruction instructions[2];
    SZrAotIrBlock block;
    SZrAotIrFunction function;
    SZrAotIrModule module = make_module(&function, &block, instructions,
                                       &constant, &valueId);
    SZrAotIrCallableAbi observed;
    SZrAotIrDiagnostic diagnostic;

    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    function.callableAbi.returnTypeToken = 17u;
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, ZR_NULL,
                                             &diagnostic) ==
           ZR_AOT_IR_INVALID_ARGUMENT);
    memset(&observed, 0xff, sizeof(observed));
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, 0u, &observed,
                                             &diagnostic) ==
           ZR_AOT_IR_INVALID_ARGUMENT);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
    assert(observed.returnTypeToken == 0u);
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, 99u, &observed,
                                             &diagnostic) == ZR_AOT_IR_INVALID_ID);

    instructions[0].results.count = 0u;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, &observed,
                                             &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    instructions[0].results.count = 1u;
    block.terminatorInstructionId = 0u;
    block.instructions.count = 1u;
    function.instructionCount = 1u;
    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, &observed,
                                             &diagnostic) == ZR_AOT_IR_UNSUPPORTED);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
}

static void qualification_rejects_duplicate_return_definition(void) {
    const TZrUInt32 values[2] = {1u, 1u};
    const SZrExecIrConstant constant = {17u, 0u, UINT64_C(42)};
    SZrAotIrInstruction instructions[3];
    SZrAotIrBlock block;
    SZrAotIrFunction function;
    SZrAotIrModule module = make_module(&function, &block, instructions,
                                       &constant, values);
    SZrAotIrCallableAbi observed;
    SZrAotIrDiagnostic diagnostic;

    instructions[2] = instructions[1];
    instructions[2].id = 3u;
    instructions[1].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instructions[1].operands.count = 0u;
    instructions[1].results.offset = 1u;
    instructions[1].results.count = 1u;
    function.instructionCount = 3u;
    function.resultPool = values;
    function.resultCount = 2u;
    block.instructions.count = 3u;
    block.terminatorInstructionId = 3u;
    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    function.callableAbi.returnTypeToken = 17u;

    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, &observed,
                                             &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
}

static void qualification_rejects_later_duplicate_definition(void) {
    const TZrUInt32 valueId = 1u;
    const SZrExecIrConstant constant = {17u, 0u, UINT64_C(42)};
    SZrAotIrInstruction instructions[3];
    SZrAotIrBlock blocks[2];
    SZrAotIrFunction function;
    SZrAotIrModule module = make_module(&function, blocks, instructions,
                                       &constant, &valueId);
    SZrAotIrCallableAbi observed;
    SZrAotIrDiagnostic diagnostic;

    instructions[2] = instructions[0];
    instructions[2].id = 3u;
    memset(&blocks[1], 0, sizeof(blocks[1]));
    blocks[1].id = 2u;
    blocks[1].instructions.offset = 2u;
    blocks[1].instructions.count = 1u;
    function.blockCount = 2u;
    function.instructionCount = 3u;
    function.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    function.callableAbi.returnTypeToken = 17u;

    assert(ZrCore_AotIr_ValidateModule(&module, &diagnostic) == ZR_AOT_IR_OK);
    assert(ZrCore_AotIr_RequireExecutableAbi(&module, function.id, &observed,
                                             &diagnostic) ==
           ZR_AOT_IR_INVALID_SIGNATURE);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
}

static void projection_and_adapter_preserve_explicit_abi(void) {
    TZrExecIrValueId valueId = 1u;
    SZrExecIrConstant constant = {17u, 0u, UINT64_C(42)};
    SZrExecBcInstruction instructions[2] = {
        {.opcode = ZR_EXEC_IR_OPCODE_CONSTANT, .pc = 0u,
         .results = {{.offset = 0u}, .count = 1u}, .typeToken = 17u},
        {.opcode = ZR_EXEC_IR_OPCODE_RETURN, .pc = 1u,
         .operands = {{.offset = 0u}, .count = 1u}, .typeToken = 17u}
    };
    SZrExecBcBlock block = {
        .id = 1u, .flags = ZR_EXEC_IR_BLOCK_FLAG_ENTRY,
        .instructions = {{.offset = 0u}, .count = 2u},
        .terminatorInstructionId = 2u
    };
    SZrAotIrProjection projection;
    SZrAotIrProjectionDescriptor descriptor;
    SZrAotIrTargetContract target = {
        ZR_AOT_IR_TARGET_ABI_VERSION, sizeof(void *), 0u, 0u, 67u, 66u};
    SZrExecutionContract moduleContract;
    SZrAotIrCallableAbi observed;
    SZrAotIrDiagnostic diagnostic;
    SZrBackendAotIrDiagnostic backendDiagnostic;

    memset(&projection, 0, sizeof(projection));
    memset(&descriptor, 0, sizeof(descriptor));
    projection.functionId = 1u;
    projection.functionToken = 7u;
    projection.signatureHash = 11u;
    projection.frameLayoutHash = 22u;
    projection.frameByteSize = 16u;
    projection.frameByteAlign = 8u;
    projection.instructions = instructions;
    projection.instructionCount = 2u;
    projection.blocks = &block;
    projection.blockCount = 1u;
    projection.operands = &valueId;
    projection.operandCount = 1u;
    projection.results = &valueId;
    projection.resultCount = 1u;
    projection.constants = &constant;
    projection.constantCount = 1u;
    projection.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    projection.callableAbi.returnTypeToken = 17u;
    projection.ownershipTag = ZR_EXEC_IR_PROJECTION_TAG;
    fill_contract(&projection.contract, 7u, 33u, 11u, 22u);
    fill_contract(&moduleContract, 0u, 33u, 44u, 55u);

    assert(ZrParser_AotIrProjection_BuildDescriptor(
        &projection, &target, &moduleContract, &descriptor, &diagnostic));
    assert(descriptor.function.callableAbi.kind ==
           ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64);
    assert(descriptor.function.callableAbi.returnTypeToken == 17u);
    assert(backend_aot_ir_adapter_require_executable_abi(
        &descriptor.module, 1u, &observed, &backendDiagnostic) ==
           ZR_BACKEND_AOT_IR_OK);
    assert(observed.kind == projection.callableAbi.kind &&
           observed.returnTypeToken == projection.callableAbi.returnTypeToken);

    projection.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_UNKNOWN;
    projection.callableAbi.returnTypeToken = 0u;
    assert(ZrParser_AotIrProjection_BuildDescriptor(
        &projection, &target, &moduleContract, &descriptor, &diagnostic));
    assert(descriptor.function.callableAbi.kind ==
           ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
    assert(backend_aot_ir_adapter_require_executable_abi(
        &descriptor.module, 1u, &observed, &backendDiagnostic) ==
           ZR_BACKEND_AOT_IR_UNSUPPORTED);
    assert(observed.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
    ZrParser_AotIrProjection_FreeDescriptor(&descriptor);
}

int main(void) {
    explicit_abi_is_validated_and_hashed();
    qualification_rejects_missing_return_and_definitions();
    qualification_rejects_duplicate_return_definition();
    qualification_rejects_later_duplicate_definition();
    projection_and_adapter_preserve_explicit_abi();
    return 0;
}
