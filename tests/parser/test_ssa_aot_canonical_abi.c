#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/aot_ir_projection_descriptor.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

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

static TZrUInt64 canonical_structural_hash(
        const SZrSemanticContext *context, TZrTypeId typeId) {
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(context, typeId);
    assert(node != NULL && node->structuralHash != 0u);
    return node->structuralHash;
}

static void set_signature(SZrExecIrFunction *function, TZrUInt64 signatureHash) {
    function->signatureHash = signatureHash;
    function->contract.signatureHash = signatureHash;
}

static void build_two_block_exec_ir(SZrExecIrFunction *function,
                                    TZrTypeId intType,
                                    TZrUInt64 callableSignatureHash) {
    TZrExecIrValueId valueId;
    TZrExecIrBlockId entry, exitBlock;
    SZrExecIrInstruction instruction;
    SZrExecIrRange resultRange, returnOperands;

    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 7u;
    set_signature(function, callableSignatureHash);
    fill_contract(&function->contract, 7u, callableSignatureHash, 22u);
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
    function->frameLayout->slots[0].typeToken = intType;
    function->frameLayout->slots[0].kind = ZR_EXEC_IR_FRAME_SLOT_VALUE;

    valueId = ZrCore_ExecIr_FunctionAddValue(
            function, intType, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
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
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = resultRange;
    instruction.typeToken = intType;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperands;
    instruction.typeToken = intType;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL));
    function->blocks[0].instructionRange.count = 1u;
    function->blocks[0].terminatorInstructionId = 1u;
    function->blocks[1].instructionRange.start = 1u;
    function->blocks[1].instructionRange.count = 2u;
    function->blocks[1].terminatorInstructionId = 3u;
}

static void expect_rejected_without_publishing(
        const SZrExecIrFunction *function,
        const SZrExecIrConstant *constants, TZrUInt32 constantCount,
        const SZrSemanticContext *context, TZrTypeId callableTypeId,
        SZrAotIrProjection *output) {
    SZrAotIrProjection before;
    SZrExecIrDiagnostic diagnostic = {0};

    memcpy(&before, output, sizeof(before));
    assert(!ZrParser_ExecIr_LowerAotWithCanonicalCallable(
            function, constants, constantCount, NULL, 0u, context,
            callableTypeId, output, &diagnostic));
    assert(diagnostic.code != ZR_EXECUTION_DIAGNOSTIC_NONE);
    assert(memcmp(&before, output, sizeof(before)) == 0);
}

int main(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrSemanticContext *context;
    SZrCanonicalParameterContract parameter = {0};
    SZrExecIrFunction function;
    SZrExecIrConstant constant;
    SZrAotIrProjection legacy = {0}, projection = {0};
    SZrAotIrProjectionDescriptor descriptor = {0};
    SZrAotIrTargetContract target = {
        ZR_AOT_IR_TARGET_ABI_VERSION, sizeof(void *), 0u, 0u, 67u, 66u};
    SZrExecutionContract moduleContract;
    SZrExecIrDiagnostic irDiagnostic = {0};
    SZrAotIrDiagnostic aotDiagnostic = {0};
    SZrAotIrCallableAbi observedAbi = {0};
    TZrTypeId intType, boolType, callableType, parameterType;
    TZrTypeId receiverType, effectType, boolReturnType;
    TZrExecIrBlockId savedSuccessor;
    const SZrCanonicalTypeNode *callableNode;

    assert(state != NULL);
    context = ZrParser_SemanticContext_New(state);
    assert(context != NULL);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    boolType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_BOOL);
    callableType = ZrParser_CanonicalType_InternFunction(
            context, NULL, 0u, intType, ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    assert(intType != ZR_SEMANTIC_ID_INVALID);
    assert(boolType != ZR_SEMANTIC_ID_INVALID);
    assert(callableType != ZR_SEMANTIC_ID_INVALID);
    callableNode = ZrParser_CanonicalType_Find(context, callableType);
    assert(callableNode != NULL && callableNode->kind == ZR_CANONICAL_TYPE_FUNCTION);
    assert(callableNode->structuralHash != 0u);

    parameter.typeId = intType;
    parameter.passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.acceptsTemporary = ZR_TRUE;
    parameter.callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;
    parameterType = ZrParser_CanonicalType_InternFunction(
            context, &parameter, 1u, intType, ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    receiverType = ZrParser_CanonicalType_InternFunction(
            context, NULL, 0u, intType, ZR_CANONICAL_RECEIVER_READONLY,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    effectType = ZrParser_CanonicalType_InternFunction(
            context, NULL, 0u, intType, ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    boolReturnType = ZrParser_CanonicalType_InternFunction(
            context, NULL, 0u, boolType, ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    assert(parameterType != ZR_SEMANTIC_ID_INVALID);
    assert(receiverType != ZR_SEMANTIC_ID_INVALID);
    assert(effectType != ZR_SEMANTIC_ID_INVALID);
    assert(boolReturnType != ZR_SEMANTIC_ID_INVALID);

    build_two_block_exec_ir(&function, intType, callableNode->structuralHash);
    constant.typeToken = intType;
    constant.flags = 0u;
    constant.bits = UINT64_C(42);
    assert(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &irDiagnostic));
    assert(ZrParser_ExecIr_LowerAotWithConstants(
            &function, &constant, 1u, &legacy, &irDiagnostic));
    assert(legacy.callableAbi.kind == ZR_AOT_IR_CALLABLE_ABI_UNKNOWN);
    fill_contract(&moduleContract, 0u, 44u, 55u);
    assert(ZrParser_AotIrProjection_BuildDescriptor(
            &legacy, &target, &moduleContract, &descriptor, &aotDiagnostic));
    assert(ZrCore_AotIr_RequireExecutableAbi(
            &descriptor.module, 1u, &observedAbi, &aotDiagnostic) ==
           ZR_AOT_IR_UNSUPPORTED);
    ZrParser_AotIrProjection_FreeDescriptor(&descriptor);
    ZrParser_AotIrProjection_Free(&legacy);

    assert(ZrParser_ExecIr_LowerAotWithCanonicalCallable(
            &function, &constant, 1u, NULL, 0u, context, callableType,
            &projection, &irDiagnostic));
    assert(projection.callableAbi.kind == ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64);
    assert(projection.callableAbi.returnTypeToken == intType);
    assert(projection.signatureHash == callableNode->structuralHash);
    assert(projection.blockCount == 2u && projection.runnable == ZR_FALSE);
    assert(ZrParser_AotIrProjection_BuildDescriptor(
            &projection, &target, &moduleContract, &descriptor, &aotDiagnostic));
    assert(descriptor.function.callableAbi.kind == ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64);
    assert(descriptor.function.callableAbi.returnTypeToken == intType);
    assert(ZrCore_AotIr_RequireExecutableAbi(
            &descriptor.module, 1u, &observedAbi, &aotDiagnostic) == ZR_AOT_IR_OK);
    assert(observedAbi.kind == ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64);
    assert(observedAbi.returnTypeToken == intType);
    ZrParser_AotIrProjection_FreeDescriptor(&descriptor);

    /* The canonical callable must be present in this context, and its
     * canonical node structural hash here must match the verified contract. */
    expect_rejected_without_publishing(
            &function, &constant, 1u, NULL, callableType, &projection);
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, 0u, &projection);
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, parameterType, &projection);
    set_signature(&function, canonical_structural_hash(context, intType));
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, intType, &projection);
    set_signature(&function, canonical_structural_hash(context, parameterType));
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, parameterType, &projection);
    set_signature(&function, canonical_structural_hash(context, receiverType));
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, receiverType, &projection);
    set_signature(&function, canonical_structural_hash(context, effectType));
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, effectType, &projection);
    set_signature(&function, canonical_structural_hash(context, boolReturnType));
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, boolReturnType, &projection);
    set_signature(&function, canonical_structural_hash(context, callableType));

    /* A well-formed ExecIR contract can still name the wrong canonical
     * callable; matching function/contract hashes alone is not sufficient. */
    set_signature(&function, canonical_structural_hash(context, parameterType));
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    set_signature(&function, canonical_structural_hash(context, callableType));

    function.contract.signatureHash = canonical_structural_hash(context, parameterType);
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    function.contract.signatureHash = canonical_structural_hash(context, callableType);
    function.contract.declaredEffects = 1u;
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    function.contract.declaredEffects = 0u;

    function.frameLayout->parameterPrefixCount = 1u;
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    function.frameLayout->parameterPrefixCount = 0u;

    function.instructions[2].typeToken = boolType;
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    function.instructions[2].typeToken = intType;
    function.values[0].typeToken = boolType;
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    function.values[0].typeToken = intType;
    function.instructions[2].operands.count = 0u;
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    function.instructions[2].operands.count = 1u;
    savedSuccessor = function.successors[0];
    function.successors[0] = 99u;
    expect_rejected_without_publishing(
            &function, &constant, 1u, context, callableType, &projection);
    function.successors[0] = savedSuccessor;
    expect_rejected_without_publishing(
            &function, NULL, 1u, context, callableType, &projection);

    ZrParser_AotIrProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrParser_SemanticContext_Free(context);
    ZrTests_Runtime_State_Destroy(state);
    return 0;
}
