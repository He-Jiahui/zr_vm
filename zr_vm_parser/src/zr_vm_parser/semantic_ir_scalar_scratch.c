#include "semantic_ir_scalar_scratch_internal.h"

#include <stdint.h>

static TZrBool scalar_scratch_value_type_is_supported(EZrValueType valueType) {
    return (TZrBool)(valueType == ZR_VALUE_TYPE_BOOL ||
                     valueType == ZR_VALUE_TYPE_INT64);
}

static TZrBool scalar_scratch_array_shape(
        const SZrArray *array,
        TZrSize elementSize,
        TZrBool allowEmptyUninitialized) {
    if (array == ZR_NULL || array->length > array->capacity) {
        return ZR_FALSE;
    }
    if (array->length == 0U && allowEmptyUninitialized &&
        !array->isValid && array->head == ZR_NULL &&
        array->capacity == 0U && array->elementSize == 0U) {
        return ZR_TRUE;
    }
    if (elementSize == 0U ||
        array->capacity > (TZrSize)SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    return (TZrBool)(array->isValid && array->head != ZR_NULL &&
                     array->elementSize == elementSize &&
                     array->capacity != 0U);
}

static TZrBool scalar_scratch_proof_rows_are_well_formed(
        const SZrSemanticIrFunction *function) {
    const SZrArray *proofs;
    TZrSize index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    proofs = &function->scalarScratchProofs;
    if (!scalar_scratch_array_shape(
                proofs, sizeof(SZrSemanticIrScalarScratchProof),
                ZR_TRUE)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < proofs->length; ++index) {
        const SZrSemanticIrScalarScratchProof *proof =
                (const SZrSemanticIrScalarScratchProof *)ZrCore_Array_Get(
                        (SZrArray *)proofs, index);
        TZrSize previousIndex;

        if (proof == ZR_NULL || proof->placeId == ZR_PLACE_ID_INVALID ||
            proof->placeId > function->places.places.length ||
            proof->constantValueId == ZR_VALUE_ID_INVALID ||
            proof->constantValueId > function->values.length ||
            proof->typeId == ZR_SEMANTIC_ID_INVALID ||
            !scalar_scratch_value_type_is_supported(proof->valueType)) {
            return ZR_FALSE;
        }
        for (previousIndex = 0U; previousIndex < index; ++previousIndex) {
            const SZrSemanticIrScalarScratchProof *previous =
                    (const SZrSemanticIrScalarScratchProof *)ZrCore_Array_Get(
                            (SZrArray *)proofs, previousIndex);
            if (previous == ZR_NULL ||
                previous->placeId == proof->placeId ||
                previous->constantValueId == proof->constantValueId) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool zr_parser_semantic_ir_scalar_scratch_proofs_well_formed(
        const SZrSemanticIrFunction *function) {
    return scalar_scratch_proof_rows_are_well_formed(function);
}

const SZrSemanticIrScalarScratchProof *
zr_parser_semantic_ir_scalar_scratch_proof_for_place(
        const SZrSemanticIrFunction *function,
        TZrPlaceId placeId) {
    const SZrArray *proofs;
    TZrSize index;

    if (placeId == ZR_PLACE_ID_INVALID ||
        !scalar_scratch_proof_rows_are_well_formed(function)) {
        return ZR_NULL;
    }
    proofs = &function->scalarScratchProofs;
    for (index = 0U; index < proofs->length; ++index) {
        const SZrSemanticIrScalarScratchProof *proof =
                (const SZrSemanticIrScalarScratchProof *)ZrCore_Array_Get(
                        (SZrArray *)proofs, index);
        if (proof->placeId == placeId) {
            return proof;
        }
    }
    return ZR_NULL;
}

TZrBool zr_parser_semantic_ir_scalar_scratch_matches_direct_initializer(
        const SZrSemanticIrFunction *function,
        const SZrSemanticIrScalarScratchProof *proof) {
    const SZrParserPlace *place;
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *constant;
    const SZrArray *instructions;
    TZrSize index;
    TZrSize constantIndex;
    TZrSize baseIndex = 0U;
    TZrSize initializeIndex = 0U;
    TZrUInt32 baseCount = 0U;
    TZrUInt32 initializeCount = 0U;

    if (function == ZR_NULL || proof == ZR_NULL ||
        !scalar_scratch_value_type_is_supported(proof->valueType) ||
        proof->placeId == ZR_PLACE_ID_INVALID ||
        proof->placeId > function->places.places.length ||
        proof->constantValueId == ZR_VALUE_ID_INVALID ||
        proof->constantValueId > function->values.length ||
        !scalar_scratch_array_shape(
                &function->places.places, sizeof(SZrParserPlace), ZR_FALSE) ||
        !scalar_scratch_array_shape(
                &function->values, sizeof(SZrSemanticIrValue), ZR_FALSE) ||
        !scalar_scratch_array_shape(
                &function->instructions,
                sizeof(SZrSemanticIrInstruction), ZR_FALSE)) {
        return ZR_FALSE;
    }

    place = (const SZrParserPlace *)ZrCore_Array_Get(
            (SZrArray *)&function->places.places, proof->placeId - 1U);
    value = (const SZrSemanticIrValue *)ZrCore_Array_Get(
            (SZrArray *)&function->values, proof->constantValueId - 1U);
    if (place == ZR_NULL || value == ZR_NULL ||
        place->id != proof->placeId ||
        place->parentId != ZR_PLACE_ID_INVALID ||
        place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY ||
        place->typeId == ZR_SEMANTIC_ID_INVALID ||
        place->typeId != value->typeId ||
        place->typeId != proof->typeId ||
        place->projections.length != 0U ||
        value->id != proof->constantValueId ||
        value->definitionInstructionId ==
                ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        value->definitionInstructionId > function->instructions.length ||
        !scalar_scratch_array_shape(
                &place->projections, sizeof(SZrParserPlaceProjection),
                ZR_FALSE)) {
        return ZR_FALSE;
    }

    constant = (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
            (SZrArray *)&function->instructions,
            value->definitionInstructionId - 1U);
    constantIndex = value->definitionInstructionId - 1U;
    if (constant == ZR_NULL ||
        constant->id != value->definitionInstructionId ||
        constant->opcode != ZR_SEMANTIC_IR_CONSTANT ||
        constant->resultValueId != proof->constantValueId ||
        constant->typeId != value->typeId ||
        constant->hasConstantPoolIndex == ZR_FALSE) {
        return ZR_FALSE;
    }

    instructions = &function->instructions;
    for (index = 0U; index < instructions->length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                        (SZrArray *)instructions, index);
        if (instruction == ZR_NULL || instruction->id != index + 1U) {
            return ZR_FALSE;
        }
        if (instruction->placeId != proof->placeId) {
            continue;
        }
        if (instruction->opcode == ZR_SEMANTIC_IR_PLACE_BASE) {
            if (instruction->typeId != place->typeId) {
                return ZR_FALSE;
            }
            ++baseCount;
            baseIndex = index;
        } else if (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE) {
            if (instruction->typeId != place->typeId ||
                instruction->valueId != proof->constantValueId) {
                return ZR_FALSE;
            }
            ++initializeCount;
            initializeIndex = index;
        } else {
            /* LOAD, STORE, borrow, calls, and any future place operation are
             * observers until this proof is deliberately extended. */
            return ZR_FALSE;
        }
    }
    return (TZrBool)(baseCount == 1U && initializeCount == 1U &&
                     constantIndex < initializeIndex &&
                     baseIndex < initializeIndex);
}
