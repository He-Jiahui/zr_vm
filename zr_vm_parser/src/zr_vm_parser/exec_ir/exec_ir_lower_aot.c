#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include <stdlib.h>
#include <string.h>

void ZrParser_AotIrProjection_Free(SZrAotIrProjection *projection) {
    if (projection == ZR_NULL || projection->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG) return;
    free(projection->opcodes);
    free(projection->instructions);
    free(projection->operands);
    free(projection->results);
    free(projection->memoryTokens);
    free(projection->frameSlots);
    free(projection->valueSlots);
    free(projection->slotValues);
    free(projection->blocks);
    free(projection->predecessors);
    free(projection->successors);
    free(projection->phis);
    free(projection->phiIncomings);
    free(projection->phiCopySources);
    free(projection->phiCopyDestinations);
    free(projection->phiCopyEdges);
    free(projection->phiMoves);
    free(projection->sourceMaps);
    free(projection->constants);
    free(projection->layouts);
    ZrCore_ExecIr_GcMapFree(&projection->gcMap);
    free(projection->gcRoots);
    free(projection->deoptStates);
    free(projection->deoptValues);
    free(projection->deoptAggregates);
    free(projection->deoptAggregateFields);
    ZrCore_ExecIr_StateMapFree(&projection->stateMap);
    memset(projection, 0, sizeof(*projection));
}

TZrBool ZrParser_ExecIr_LowerAotWithConstantsAndLayouts(
        const SZrExecIrFunction *function, const SZrExecIrConstant *constants,
        TZrUInt32 constantCount, const SZrExecIrLayout *layouts,
        TZrUInt32 layoutCount, SZrAotIrProjection *output,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecBcProjection prepared;
    SZrAotIrProjection candidate;
    if (output == ZR_NULL) {
        if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    memset(&prepared, 0, sizeof(prepared));
    if (!ZrParser_ExecIr_BuildProjectionWithConstantsAndLayouts(
            function, constants, constantCount, layouts, layoutCount,
            &prepared, diagnostic)) {
        return ZR_FALSE;
    }
    memset(&candidate, 0, sizeof(candidate));
    ZrParser_ExecIr_MoveProjectionToAot(&prepared, &candidate);
    candidate.signatureHash = function->signatureHash;
    candidate.functionToken = function->functionToken;
    candidate.contract = function->contract;
    /* ExecIR currently has no trusted primitive return ABI producer. */
    candidate.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_UNKNOWN;
    candidate.callableAbi.returnTypeToken = 0u;
    /* This record is an AOTIR seam, not executable native code yet. */
    candidate.runnable = ZR_FALSE;
    candidate.ownershipTag = ZR_EXEC_IR_PROJECTION_TAG;
    if (function->stateMap != ZR_NULL) {
        EZrExecutionDiagnosticCode failure =
                ZrCore_ExecIr_StateMapStorageValid(function->stateMap)
                ? ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY
                : ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION;
        if (failure == ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION ||
            !ZrCore_ExecIr_StateMapClone(function->stateMap, &candidate.stateMap)) {
            if (diagnostic != ZR_NULL) {
                diagnostic->code = failure;
                diagnostic->functionToken = function->functionToken;
            }
            ZrParser_AotIrProjection_Free(&candidate);
            ZrParser_ExecBcProjection_Free(&prepared);
            return ZR_FALSE;
        }
    }
    ZrParser_AotIrProjection_Free(output);
    *output = candidate;
    ZrParser_ExecBcProjection_Free(&prepared);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_LowerAotWithConstants(
        const SZrExecIrFunction *function, const SZrExecIrConstant *constants,
        TZrUInt32 constantCount, SZrAotIrProjection *output,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_LowerAotWithConstantsAndLayouts(
            function, constants, constantCount, ZR_NULL, 0u, output, diagnostic);
}

static void exec_ir_lower_aot_canonical_diag(
        SZrExecIrDiagnostic *diagnostic,
        const SZrExecIrFunction *function,
        EZrExecutionDiagnosticCode code,
        TZrExecIrInstructionId instructionId,
        TZrUInt64 expectedHash,
        TZrUInt64 actualHash) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    if (function != ZR_NULL) {
        diagnostic->functionToken = function->functionToken;
    }
    diagnostic->instructionId = instructionId;
    diagnostic->expectedHash = expectedHash;
    diagnostic->actualHash = actualHash;
}

static TZrBool exec_ir_lower_aot_canonical_return_matches(
        const SZrExecIrFunction *function,
        TZrExecIrTypeToken returnTypeToken,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 returnCount = 0u;
    for (TZrUInt32 i = 0u; i < function->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &function->instructions[i];
        TZrExecIrValueId valueId;
        const SZrExecIrValue *value;
        const SZrExecIrInstruction *definition;
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_RETURN) continue;
        ++returnCount;
        if (instruction->typeToken != returnTypeToken ||
            instruction->operands.count != 1u ||
            instruction->operands.start >= function->operandCount) {
            exec_ir_lower_aot_canonical_diag(
                    diagnostic, function, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                    i + 1u, returnTypeToken, instruction->typeToken);
            return ZR_FALSE;
        }
        valueId = function->operandPool[instruction->operands.start];
        if (valueId == 0u || valueId > function->valueCount) {
            exec_ir_lower_aot_canonical_diag(
                    diagnostic, function, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                    i + 1u, function->valueCount, valueId);
            return ZR_FALSE;
        }
        value = &function->values[valueId - 1u];
        if (value->id != valueId || value->typeToken != returnTypeToken ||
            value->definition == 0u ||
            value->definition == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            value->definition > function->instructionCount) {
            exec_ir_lower_aot_canonical_diag(
                    diagnostic, function, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                    i + 1u, returnTypeToken, value->typeToken);
            return ZR_FALSE;
        }
        definition = &function->instructions[value->definition - 1u];
        if (definition->typeToken != returnTypeToken) {
            exec_ir_lower_aot_canonical_diag(
                    diagnostic, function, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                    i + 1u, returnTypeToken, definition->typeToken);
            return ZR_FALSE;
        }
    }
    if (returnCount == 0u) {
        exec_ir_lower_aot_canonical_diag(
                diagnostic, function, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                0u, 1u, 0u);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_LowerAotWithCanonicalCallable(
        const SZrExecIrFunction *function, const SZrExecIrConstant *constants,
        TZrUInt32 constantCount, const SZrExecIrLayout *layouts,
        TZrUInt32 layoutCount, const struct SZrSemanticContext *context,
        TZrTypeId callableTypeId, SZrAotIrProjection *output,
        SZrExecIrDiagnostic *diagnostic) {
    const SZrCanonicalTypeNode *callableNode;
    const SZrCanonicalTypeNode *returnNode;
    SZrAotIrProjection candidate;
    TZrTypeId returnTypeId;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (output == ZR_NULL) {
        exec_ir_lower_aot_canonical_diag(
                diagnostic, function, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (function == ZR_NULL || context == ZR_NULL ||
        callableTypeId == ZR_SEMANTIC_ID_INVALID) {
        exec_ir_lower_aot_canonical_diag(
                diagnostic, function, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                0u, 1u, 0u);
        return ZR_FALSE;
    }
    callableNode = ZrParser_CanonicalType_Find(context, callableTypeId);
    if (callableNode == ZR_NULL ||
        callableNode->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        callableNode->structuralHash == 0u) {
        exec_ir_lower_aot_canonical_diag(
                diagnostic, function, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                0u, ZR_CANONICAL_TYPE_FUNCTION,
                callableNode != ZR_NULL ? callableNode->kind : UINT32_MAX);
        return ZR_FALSE;
    }
    if (function->signatureHash != callableNode->structuralHash ||
        function->contract.signatureHash != callableNode->structuralHash) {
        TZrUInt64 actualHash = function->signatureHash != callableNode->structuralHash
                ? function->signatureHash
                : function->contract.signatureHash;
        exec_ir_lower_aot_canonical_diag(
                diagnostic, function, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                0u, callableNode->structuralHash, actualHash);
        return ZR_FALSE;
    }
    if (function->contract.declaredEffects != callableNode->data.function.effectFlags) {
        exec_ir_lower_aot_canonical_diag(
                diagnostic, function, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                0u, callableNode->data.function.effectFlags,
                function->contract.declaredEffects);
        return ZR_FALSE;
    }
    returnTypeId = callableNode->data.function.returnTypeId;
    returnNode = ZrParser_CanonicalType_Find(context, returnTypeId);
    if (callableNode->data.function.parameterContracts.length != 0u ||
        callableNode->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        callableNode->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE ||
        returnNode == ZR_NULL || returnNode->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        returnNode->data.primitive.valueType != ZR_VALUE_TYPE_INT64 ||
        function->frameLayout == ZR_NULL ||
        function->frameLayout->parameterPrefixCount != 0u ||
        function->frameLayout->parameterCount != 0u) {
        exec_ir_lower_aot_canonical_diag(
                diagnostic, function, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                0u, ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, callableTypeId);
        return ZR_FALSE;
    }

    memset(&candidate, 0, sizeof(candidate));
    if (!ZrParser_ExecIr_LowerAotWithConstantsAndLayouts(
                function, constants, constantCount, layouts, layoutCount,
                &candidate, diagnostic)) {
        if (diagnostic != ZR_NULL &&
            diagnostic->code == ZR_EXECUTION_DIAGNOSTIC_NONE) {
            exec_ir_lower_aot_canonical_diag(
                    diagnostic, function, ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                    0u, 1u, 0u);
        }
        return ZR_FALSE;
    }
    if (!exec_ir_lower_aot_canonical_return_matches(
                function, returnTypeId, diagnostic)) {
        ZrParser_AotIrProjection_Free(&candidate);
        return ZR_FALSE;
    }
    candidate.callableAbi.kind = ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64;
    candidate.callableAbi.returnTypeToken = returnTypeId;
    ZrParser_AotIrProjection_Free(output);
    *output = candidate;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_LowerAot(const SZrExecIrFunction *function,
                                 SZrAotIrProjection *output,
                                 SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_LowerAotWithConstants(
            function, ZR_NULL, 0u, output, diagnostic);
}
