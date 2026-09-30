#include <stdint.h>
#include <stdlib.h>

#include "zr_vm_common/zr_type_conf.h"
#include "exec_ir_execbc_vm_internal.h"

static TZrBool execbc_vm_place_range_valid(
        SZrExecIrRange range,
        TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool execbc_vm_place_scalar_type(TZrExecIrTypeToken typeToken) {
    return (TZrBool)(typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 ||
                     typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL);
}

static TZrBool execbc_vm_place_value_id_valid(
        const SZrExecBcProjection *projection,
        TZrExecIrValueId valueId) {
    return (TZrBool)(valueId != ZR_EXEC_IR_VALUE_ID_INVALID &&
                     valueId <= projection->valueSlotCount);
}

static TZrBool execbc_vm_place_fail(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecBcProjection *projection,
        TZrExecIrInstructionId instructionId,
        TZrExecIrSourceId sourceId,
        TZrUInt32 expected,
        TZrUInt32 actual) {
    return execbc_vm_fail(diagnostic, code, projection, 0u, instructionId,
                          sourceId, expected, actual);
}

/* Validate the narrow metadata-free PLACE_BASE exception before the ordinary
 * scalar-value checks. The caller has already enforced the projection-wide
 * frame, GC, deopt, state-map, and memory-token guards; this pass only permits
 * place flags/provenance whose every use is removed with a dead PLACE_BASE. */
TZrBool execbc_vm_validate_dead_place_uses(
        const SZrExecBcProjection *projection,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 *placeInstructionByValue = ZR_NULL;
    TZrUInt32 *useCounts = ZR_NULL;
    TZrUInt32 index;
    TZrBool valid = ZR_FALSE;

    if (projection == ZR_NULL || projection->instructionCount == 0u ||
        projection->instructions == ZR_NULL || projection->opcodes == ZR_NULL ||
        projection->valueSlotCount == 0u || projection->valueSlots == ZR_NULL ||
        projection->physicalSlotCount < projection->valueSlotCount ||
        projection->slotValues == ZR_NULL ||
        (projection->operandCount != 0u && projection->operands == ZR_NULL) ||
        (projection->resultCount != 0u && projection->results == ZR_NULL) ||
        (projection->phiCopyCount != 0u &&
         (projection->phiCopySources == ZR_NULL ||
          projection->phiCopyDestinations == ZR_NULL)) ||
        (projection->phiCount != 0u && projection->phis == ZR_NULL) ||
        (projection->phiIncomingCount != 0u &&
         projection->phiIncomings == ZR_NULL)) {
        return execbc_vm_fail(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
                              projection, 0u, 0u, 0u, 1u, 0u);
    }
    if (projection->valueSlotCount == UINT32_MAX ||
        (size_t)(projection->valueSlotCount + 1u) >
                SIZE_MAX / sizeof(*placeInstructionByValue) ||
        (size_t)(projection->valueSlotCount + 1u) >
                SIZE_MAX / sizeof(*useCounts)) {
        return execbc_vm_fail(diagnostic,
                              ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                              projection, 0u, 0u, 0u,
                              projection->valueSlotCount,
                              projection->valueSlotCount);
    }
    placeInstructionByValue = (TZrUInt32 *)calloc(
            (size_t)projection->valueSlotCount + 1u,
            sizeof(*placeInstructionByValue));
    useCounts = (TZrUInt32 *)calloc(
            (size_t)projection->valueSlotCount + 1u, sizeof(*useCounts));
    if (placeInstructionByValue == ZR_NULL || useCounts == ZR_NULL) {
        (void)execbc_vm_fail(diagnostic,
                             ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                             projection, 0u, 0u, 0u,
                             projection->valueSlotCount, 0u);
        goto cleanup;
    }

    for (index = 0u; index < projection->valueSlotCount; ++index) {
        TZrUInt32 slot = projection->valueSlots[index];
        const SZrExecIrValue *value;
        if (slot >= projection->physicalSlotCount ||
            projection->slotValues[slot].id != index + 1u) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, 0u, 0u, projection->physicalSlotCount, slot);
            goto cleanup;
        }
        value = &projection->slotValues[slot];
        if (!execbc_vm_place_scalar_type(value->typeToken) ||
            (value->flags & ~ZR_EXEC_IR_VALUE_FLAG_MASK) != 0u ||
            ((value->flags & ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE) != 0u &&
             (value->flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) == 0u)) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, value->definition, 0u,
                    ZR_EXEC_IR_VALUE_FLAG_MASK, value->flags);
            goto cleanup;
        }
        if ((value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u &&
            (value->definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
             (value->flags & (ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
                              ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE)) != 0u)) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, value->definition, 0u,
                    ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY, value->flags);
            goto cleanup;
        }
    }
    for (index = 0u; index < projection->physicalSlotCount; ++index) {
        const SZrExecIrValue *value = &projection->slotValues[index];
        if ((value->flags & ~ZR_EXEC_IR_VALUE_FLAG_MASK) != 0u ||
            (value->id == 0u && value->flags != 0u) ||
            (value->id != 0u &&
             (value->id > projection->valueSlotCount ||
              projection->valueSlots[value->id - 1u] != index))) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, value->definition, 0u,
                    projection->valueSlotCount, value->id);
            goto cleanup;
        }
    }

    for (index = 0u; index < projection->instructionCount; ++index) {
        const SZrExecBcInstruction *instruction =
                &projection->instructions[index];
        TZrExecIrInstructionId instructionId = index + 1u;
        if (!execbc_vm_place_range_valid(instruction->operands,
                                         projection->operandCount) ||
            !execbc_vm_place_range_valid(instruction->results,
                                         projection->resultCount) ||
            !execbc_vm_place_range_valid(instruction->phiRange,
                                         projection->phiIncomingCount) ||
            !execbc_vm_place_range_valid(instruction->successorRange,
                                         projection->successorCount) ||
            !execbc_vm_place_range_valid(instruction->memoryIn,
                                         projection->memoryTokenCount) ||
            !execbc_vm_place_range_valid(instruction->memoryOut,
                                         projection->memoryTokenCount)) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                    projection, instructionId, instruction->sourceId,
                    projection->instructionCount, index);
            goto cleanup;
        }
        {
            TZrUInt32 resultIndex;
            for (resultIndex = 0u; resultIndex < instruction->results.count;
                 ++resultIndex) {
                TZrExecIrValueId resultId = projection->results[
                        instruction->results.start + resultIndex];
                TZrUInt32 resultSlot;
                if (!execbc_vm_place_value_id_valid(projection, resultId)) {
                    (void)execbc_vm_place_fail(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                            projection, instructionId, instruction->sourceId,
                            projection->valueSlotCount, resultId);
                    goto cleanup;
                }
                resultSlot = projection->valueSlots[resultId - 1u];
                if ((projection->slotValues[resultSlot].flags &
                     ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
                    (void)execbc_vm_place_fail(
                            diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                            projection, instructionId, instruction->sourceId,
                            ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY,
                            projection->slotValues[resultSlot].flags);
                    goto cleanup;
                }
            }
        }
        if (instruction->opcode != (TZrUInt16)ZR_EXEC_IR_OPCODE_PLACE_BASE) {
            continue;
        }
        if (projection->opcodes[index] != (TZrUInt32)instruction->opcode ||
            instruction->operands.count != 1u ||
            instruction->results.count != 1u ||
            instruction->phiRange.count != 0u ||
            instruction->successorRange.count != 0u ||
            instruction->memoryIn.count != 0u ||
            instruction->memoryOut.count != 0u ||
            instruction->flags != 0u || instruction->effectIn != 0u ||
            instruction->effectOut != 0u || instruction->deoptId != 0u ||
            instruction->bindingRow != 0u || instruction->layoutId != 0u ||
            instruction->matchTypeToken != 0u ||
            !execbc_vm_place_scalar_type(instruction->typeToken)) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, instructionId, instruction->sourceId,
                    1u, (TZrUInt32)instruction->opcode);
            goto cleanup;
        }
        {
            TZrExecIrValueId inputId =
                    projection->operands[instruction->operands.start];
            TZrExecIrValueId resultId =
                    projection->results[instruction->results.start];
            TZrUInt32 inputSlot;
            TZrUInt32 resultSlot;
            const SZrExecIrValue *inputValue;
            const SZrExecIrValue *resultValue;
            TZrUInt32 allowedPlaceFlags =
                    ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
                    ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE;

            if (!execbc_vm_place_value_id_valid(projection, inputId) ||
                !execbc_vm_place_value_id_valid(projection, resultId)) {
                (void)execbc_vm_place_fail(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        projection, instructionId, instruction->sourceId,
                        projection->valueSlotCount,
                        inputId == ZR_EXEC_IR_VALUE_ID_INVALID
                                ? inputId
                                : resultId);
                goto cleanup;
            }
            inputSlot = projection->valueSlots[inputId - 1u];
            resultSlot = projection->valueSlots[resultId - 1u];
            if (inputSlot >= projection->physicalSlotCount ||
                resultSlot >= projection->physicalSlotCount) {
                (void)execbc_vm_place_fail(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        projection, instructionId, instruction->sourceId,
                        projection->physicalSlotCount,
                        inputSlot >= projection->physicalSlotCount
                                ? inputSlot
                                : resultSlot);
                goto cleanup;
            }
            inputValue = &projection->slotValues[inputSlot];
            resultValue = &projection->slotValues[resultSlot];
            if (!execbc_vm_place_scalar_type(inputValue->typeToken) ||
                resultValue->id != resultId ||
                resultValue->definition != instructionId ||
                resultValue->typeToken != instruction->typeToken ||
                inputValue->typeToken != instruction->typeToken ||
                (resultValue->flags &
                 ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u ||
                (resultValue->flags & ~allowedPlaceFlags) != 0u ||
                placeInstructionByValue[resultId] != 0u) {
                (void)execbc_vm_place_fail(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        projection, instructionId, instruction->sourceId,
                        instruction->typeToken, resultValue->typeToken);
                goto cleanup;
            }
            placeInstructionByValue[resultId] = instructionId;
        }
    }

    for (index = 0u; index < projection->valueSlotCount; ++index) {
        TZrExecIrValueId valueId = index + 1u;
        TZrUInt32 slot = projection->valueSlots[index];
        const SZrExecIrValue *value = &projection->slotValues[slot];
        TZrUInt32 placeFlags = value->flags &
                (ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
                 ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE);
        if (placeFlags != 0u &&
            (placeInstructionByValue[valueId] == 0u ||
             value->definition != placeInstructionByValue[valueId])) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, value->definition, 0u,
                    ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS, placeFlags);
            goto cleanup;
        }
    }

    for (index = 0u; index < projection->instructionCount; ++index) {
        const SZrExecBcInstruction *instruction =
                &projection->instructions[index];
        TZrUInt32 operandIndex;
        for (operandIndex = 0u;
             operandIndex < instruction->operands.count;
             ++operandIndex) {
            TZrExecIrValueId valueId = projection->operands[
                    instruction->operands.start + operandIndex];
            const SZrExecIrValue *value;
            if (!execbc_vm_place_value_id_valid(projection, valueId)) {
                (void)execbc_vm_place_fail(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                        projection, index + 1u, instruction->sourceId,
                        projection->valueSlotCount, valueId);
                goto cleanup;
            }
            if (useCounts[valueId] == UINT32_MAX) {
                (void)execbc_vm_place_fail(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                        projection, index + 1u, instruction->sourceId,
                        UINT32_MAX, useCounts[valueId]);
                goto cleanup;
            }
            ++useCounts[valueId];
            value = &projection->slotValues[
                    projection->valueSlots[valueId - 1u]];
            if ((value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
                TZrExecIrValueId placeResultId = 0u;
                TZrBool isDeadPlaceOperand = ZR_FALSE;
                if (instruction->opcode ==
                            (TZrUInt16)ZR_EXEC_IR_OPCODE_PLACE_BASE &&
                    instruction->operands.count == 1u &&
                    instruction->results.count == 1u) {
                    placeResultId = projection->results[
                            instruction->results.start];
                    isDeadPlaceOperand = (TZrBool)(
                            operandIndex == 0u &&
                            execbc_vm_place_value_id_valid(
                                    projection, placeResultId) &&
                            placeInstructionByValue[placeResultId] == index + 1u);
                }
                if (isDeadPlaceOperand == ZR_FALSE) {
                    (void)execbc_vm_place_fail(
                            diagnostic,
                            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                            projection, index + 1u, instruction->sourceId,
                            ZR_EXEC_IR_OPCODE_PLACE_BASE,
                            (TZrUInt32)instruction->opcode);
                    goto cleanup;
                }
            }
        }
    }

    for (index = 0u; index < projection->phiIncomingCount; ++index) {
        TZrExecIrValueId valueId = projection->phiIncomings[index].value;
        const SZrExecIrValue *value;
        if (!execbc_vm_place_value_id_valid(projection, valueId)) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, 0u, 0u, projection->valueSlotCount, valueId);
            goto cleanup;
        }
        if (useCounts[valueId] == UINT32_MAX) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                    projection, 0u, 0u, UINT32_MAX, useCounts[valueId]);
            goto cleanup;
        }
        ++useCounts[valueId];
        value = &projection->slotValues[
                projection->valueSlots[valueId - 1u]];
        if ((value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, 0u, 0u,
                    ZR_EXEC_IR_OPCODE_PLACE_BASE, valueId);
            goto cleanup;
        }
    }

    for (index = 0u; index < projection->phiCopyCount; ++index) {
        TZrExecIrValueId sourceId = projection->phiCopySources[index];
        TZrExecIrValueId destinationId =
                projection->phiCopyDestinations[index];
        const SZrExecIrValue *sourceValue;
        const SZrExecIrValue *destinationValue;
        if (!execbc_vm_place_value_id_valid(projection, sourceId) ||
            !execbc_vm_place_value_id_valid(projection, destinationId)) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, 0u, 0u, projection->valueSlotCount,
                    !execbc_vm_place_value_id_valid(projection, sourceId)
                            ? sourceId
                            : destinationId);
            goto cleanup;
        }
        sourceValue = &projection->slotValues[
                projection->valueSlots[sourceId - 1u]];
        destinationValue = &projection->slotValues[
                projection->valueSlots[destinationId - 1u]];
        if ((sourceValue->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u ||
            (destinationValue->flags &
             ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u ||
            placeInstructionByValue[destinationId] != 0u) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, 0u, 0u,
                    ZR_EXEC_IR_OPCODE_PLACE_BASE,
                    destinationId);
            goto cleanup;
        }
        if (useCounts[sourceId] == UINT32_MAX) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                    projection, 0u, 0u, UINT32_MAX, useCounts[sourceId]);
            goto cleanup;
        }
        ++useCounts[sourceId];
    }

    for (index = 0u; index < projection->phiCount; ++index) {
        TZrExecIrValueId resultId = projection->phis[index].result;
        if (!execbc_vm_place_value_id_valid(projection, resultId)) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, 0u, 0u, projection->valueSlotCount,
                    resultId);
            goto cleanup;
        }
        if ((projection->slotValues[
                     projection->valueSlots[resultId - 1u]].flags &
             ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    projection, 0u, 0u,
                    ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY,
                    projection->slotValues[
                            projection->valueSlots[resultId - 1u]].flags);
            goto cleanup;
        }
        if (placeInstructionByValue[resultId] != 0u) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, placeInstructionByValue[resultId], 0u,
                    ZR_EXEC_IR_OPCODE_PHI, resultId);
            goto cleanup;
        }
    }

    for (index = 0u; index < projection->valueSlotCount; ++index) {
        TZrExecIrValueId valueId = index + 1u;
        const SZrExecIrValue *value = &projection->slotValues[
                projection->valueSlots[index]];
        if ((value->flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u &&
            useCounts[valueId] == 0u) {
            (void)execbc_vm_place_fail(
                    diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, value->definition, 0u, 1u, 0u);
            goto cleanup;
        }
        if (placeInstructionByValue[valueId] != 0u &&
            useCounts[valueId] != 0u) {
            TZrUInt32 instructionIndex =
                    placeInstructionByValue[valueId] - 1u;
            const SZrExecBcInstruction *instruction =
                    &projection->instructions[instructionIndex];
            (void)execbc_vm_place_fail(
                    diagnostic,
                    ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
                    projection, instructionIndex + 1u,
                    instruction->sourceId, 0u, useCounts[valueId]);
            goto cleanup;
        }
    }

    valid = ZR_TRUE;

cleanup:
    free(placeInstructionByValue);
    free(useCounts);
    return valid;
}
