#include "exec_ir_internal.h"
#include "../semantic_ir_scalar_scratch_internal.h"

#include <stdint.h>
#include <string.h>

static TZrBool eligibility_array_shape(
        const SZrArray *array,
        TZrSize elementSize) {
    return (TZrBool)(array->length <= UINT32_MAX &&
                     array->length <= array->capacity &&
                     (array->length == 0u ||
                      (array->isValid && array->head != ZR_NULL &&
                       array->elementSize == elementSize &&
                       array->capacity <= SIZE_MAX / elementSize)));
}

static TZrBool eligibility_fail(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        diagnostic->functionToken = (TZrMetadataToken)semantic->symbolId;
    }
    return ZR_FALSE;
}

static const SZrParserPlace *eligibility_place_at(
        const SZrSemanticIrFunction *semantic,
        TZrPlaceId placeId) {
    if (placeId == ZR_PLACE_ID_INVALID ||
        placeId > semantic->places.places.length) {
        return ZR_NULL;
    }
    return (const SZrParserPlace *)ZrCore_Array_Get(
            (SZrArray *)&semantic->places.places, placeId - 1u);
}

static TZrPlaceId eligibility_root_place(
        const SZrSemanticIrFunction *semantic,
        TZrPlaceId placeId) {
    TZrSize remaining = semantic->places.places.length;
    while (placeId != ZR_PLACE_ID_INVALID && remaining-- != 0u) {
        const SZrParserPlace *place = eligibility_place_at(semantic, placeId);
        if (place == ZR_NULL) return ZR_PLACE_ID_INVALID;
        if (place->parentId == ZR_PLACE_ID_INVALID) return placeId;
        placeId = place->parentId;
    }
    return ZR_PLACE_ID_INVALID;
}

static TZrBool eligibility_has_projection(
        const SZrSemanticIrFunction *semantic,
        TZrPlaceId rootId) {
    TZrSize index;
    for (index = 0u; index < semantic->places.places.length; ++index) {
        const SZrParserPlace *place = (const SZrParserPlace *)ZrCore_Array_Get(
                (SZrArray *)&semantic->places.places, index);
        if (place != ZR_NULL && place->id != rootId &&
            eligibility_root_place(semantic, place->id) == rootId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool eligibility_has_alias(
        const SZrSemanticIrFunction *semantic,
        TZrPlaceId rootId) {
    TZrSize index;
    for (index = 0u; index < semantic->loanFacts.length; ++index) {
        const SZrSemanticIrLoanFact *loan =
                (const SZrSemanticIrLoanFact *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->loanFacts, index);
        if (loan != ZR_NULL &&
            eligibility_root_place(semantic, loan->sourcePlaceId) == rootId) {
            return ZR_TRUE;
        }
    }
    for (index = 0u; index < semantic->escapeFacts.length; ++index) {
        const SZrSemanticEscapeFact *escape =
                (const SZrSemanticEscapeFact *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->escapeFacts, index);
        if (escape != ZR_NULL &&
            eligibility_root_place(semantic, escape->sourcePlaceId) == rootId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool eligibility_has_local_reference(
        const SZrSemanticIrFunction *semantic,
        TZrPlaceId placeId) {
    TZrSize index;
    for (index = 0U; index < semantic->locals.length; ++index) {
        const SZrSemanticIrLocal *local =
                (const SZrSemanticIrLocal *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->locals, index);
        if (local == ZR_NULL || local->placeId == placeId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool eligibility_has_scalar_scratch_fact_observer(
        const SZrSemanticIrFunction *semantic,
        TZrPlaceId placeId) {
    TZrSize index;
    for (index = 0U; index < semantic->contiguousViewFacts.length; ++index) {
        const SZrSemanticContiguousViewFact *fact =
                (const SZrSemanticContiguousViewFact *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->contiguousViewFacts, index);
        if (fact == ZR_NULL || fact->viewPlaceId == placeId ||
            fact->sourcePlaceId == placeId) {
            return ZR_TRUE;
        }
    }
    for (index = 0U; index < semantic->boundsFacts.length; ++index) {
        const SZrSemanticBoundsFact *fact =
                (const SZrSemanticBoundsFact *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->boundsFacts, index);
        if (fact == ZR_NULL || fact->viewPlaceId == placeId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* PlaceGraph overlap treats identities from different base kinds as
 * disjoint. Compiler stack-slot identities are one namespace, so a literal
 * scratch candidate must also reject any other root with the same numeric ID. */
static TZrBool eligibility_has_root_identity_collision(
        const SZrSemanticIrFunction *semantic,
        const SZrParserPlace *candidate) {
    TZrSize index;
    for (index = 0U; index < semantic->places.places.length; ++index) {
        const SZrParserPlace *other = (const SZrParserPlace *)ZrCore_Array_Get(
                (SZrArray *)&semantic->places.places, index);
        if (other == ZR_NULL) {
            return ZR_TRUE;
        }
        if (other->id != candidate->id &&
            other->parentId == ZR_PLACE_ID_INVALID &&
            other->base.identity == candidate->base.identity) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool eligibility_places_are_pairwise_disjoint_from_candidate(
        const SZrSemanticIrFunction *semantic,
        const SZrParserPlace *candidate) {
    TZrSize index;
    for (index = 0U; index < semantic->places.places.length; ++index) {
        const SZrParserPlace *other = (const SZrParserPlace *)ZrCore_Array_Get(
                (SZrArray *)&semantic->places.places, index);
        if (other == ZR_NULL) {
            return ZR_FALSE;
        }
        if (other->id != candidate->id &&
            ZrParser_PlaceGraph_Overlap(
                    &semantic->places, candidate->id, other->id) !=
                    ZR_PARSER_PLACE_DISJOINT) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool eligibility_exec_ir_scalar_type_matches(
        const SZrExecIrFunction *output,
        TZrExecIrValueId firstPlaceValue,
        const SZrParserPlace *place,
        const SZrSemanticIrScalarScratchProof *proof) {
    TZrSize placeValueIndex;
    const SZrExecIrValue *constantValue;
    const SZrExecIrValue *placeValue;
    TZrExecIrTypeToken expectedType;

    if (output == ZR_NULL || output->values == ZR_NULL || place == ZR_NULL ||
        proof == ZR_NULL || proof->constantValueId == ZR_VALUE_ID_INVALID ||
        proof->constantValueId > output->valueCount ||
        place->id == ZR_PLACE_ID_INVALID || firstPlaceValue == 0U ||
        firstPlaceValue - 1U > output->valueCount ||
        place->id - 1U >= output->valueCount - (firstPlaceValue - 1U)) {
        return ZR_FALSE;
    }
    placeValueIndex = firstPlaceValue - 1U + place->id - 1U;
    constantValue = &output->values[proof->constantValueId - 1U];
    placeValue = &output->values[placeValueIndex];
    expectedType = (TZrExecIrTypeToken)proof->typeId;
    return (TZrBool)(constantValue->id == proof->constantValueId &&
                     placeValue->id == placeValueIndex + 1U &&
                     constantValue->typeToken == expectedType &&
                     placeValue->typeToken == expectedType);
}

static TZrBool eligibility_scalar_scratch_candidate_is_safe(
        const SZrSemanticIrFunction *semantic,
        const SZrExecIrFunction *output,
        TZrExecIrValueId firstPlaceValue,
        const SZrParserPlace *place,
        const SZrSemanticIrScalarScratchProof *proof) {
    return (TZrBool)(place != ZR_NULL && proof != ZR_NULL &&
                     place->parentId == ZR_PLACE_ID_INVALID &&
                     place->base.kind == ZR_PARSER_PLACE_BASE_TEMPORARY &&
                     place->typeId != ZR_SEMANTIC_ID_INVALID &&
                     place->projections.length == 0U &&
                     !eligibility_has_projection(semantic, place->id) &&
                     !eligibility_has_alias(semantic, place->id) &&
                     !eligibility_has_local_reference(semantic, place->id) &&
                     !eligibility_has_scalar_scratch_fact_observer(
                             semantic, place->id) &&
                     !eligibility_has_root_identity_collision(semantic, place) &&
                     eligibility_places_are_pairwise_disjoint_from_candidate(
                             semantic, place) &&
                     zr_parser_semantic_ir_scalar_scratch_matches_direct_initializer(
                             semantic, proof) &&
                     eligibility_exec_ir_scalar_type_matches(
                             output, firstPlaceValue, place, proof));
}

TZrBool zr_parser_exec_ir_mark_place_values(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        TZrExecIrValueId firstPlaceValue,
        SZrExecIrDiagnostic *diagnostic) {
    TZrSize index;
    TZrBool scalarScratchProofsAreWellFormed;

    if (semantic == ZR_NULL || output == ZR_NULL ||
        !eligibility_array_shape(
                &semantic->places.places, sizeof(SZrParserPlace)) ||
        !eligibility_array_shape(
                &semantic->locals, sizeof(SZrSemanticIrLocal)) ||
        !eligibility_array_shape(
                &semantic->loanFacts, sizeof(SZrSemanticIrLoanFact)) ||
        !eligibility_array_shape(
                &semantic->escapeFacts, sizeof(SZrSemanticEscapeFact)) ||
        !eligibility_array_shape(
                &semantic->contiguousViewFacts,
                sizeof(SZrSemanticContiguousViewFact)) ||
        !eligibility_array_shape(
                &semantic->boundsFacts, sizeof(SZrSemanticBoundsFact)) ||
        (output->valueCount != 0U && output->values == ZR_NULL) ||
        semantic->places.places.length > output->valueCount ||
        firstPlaceValue == ZR_EXEC_IR_VALUE_ID_INVALID ||
        firstPlaceValue - 1u > output->valueCount -
                                      semantic->places.places.length) {
        return semantic != ZR_NULL
                       ? eligibility_fail(semantic, diagnostic)
                       : ZR_FALSE;
    }
    scalarScratchProofsAreWellFormed =
            zr_parser_semantic_ir_scalar_scratch_proofs_well_formed(semantic);

    for (index = 0u; index < semantic->places.places.length; ++index) {
        output->values[firstPlaceValue - 1u + index].flags |=
                ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS;
    }
    for (index = 0u; index < semantic->locals.length; ++index) {
        const SZrSemanticIrLocal *local =
                (const SZrSemanticIrLocal *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->locals, index);
        const SZrParserPlace *place;
        if (local == ZR_NULL || local->placeId == ZR_PLACE_ID_INVALID ||
            local->placeId > semantic->places.places.length) {
            return eligibility_fail(semantic, diagnostic);
        }
        place = eligibility_place_at(semantic, local->placeId);
        if (place == ZR_NULL || place->typeId != local->typeId) {
            return eligibility_fail(semantic, diagnostic);
        }
        if (place->parentId == ZR_PLACE_ID_INVALID &&
            place->base.kind == ZR_PARSER_PLACE_BASE_LOCAL &&
            !local->isParameter && local->isScalar &&
            !eligibility_has_projection(semantic, place->id) &&
            !eligibility_has_alias(semantic, place->id)) {
            output->values[firstPlaceValue - 1u + place->id - 1u].flags |=
                    ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE;
        }
    }
    if (scalarScratchProofsAreWellFormed) {
        for (index = 0U; index < semantic->scalarScratchProofs.length; ++index) {
            const SZrSemanticIrScalarScratchProof *proof =
                    (const SZrSemanticIrScalarScratchProof *)ZrCore_Array_Get(
                            (SZrArray *)&semantic->scalarScratchProofs, index);
            const SZrParserPlace *place =
                    proof != ZR_NULL
                            ? eligibility_place_at(semantic, proof->placeId)
                            : ZR_NULL;
            if (eligibility_scalar_scratch_candidate_is_safe(
                        semantic, output, firstPlaceValue, place, proof)) {
                output->values[firstPlaceValue - 1U + place->id - 1U].flags |=
                        ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE;
            }
        }
    }
    return ZR_TRUE;
}
