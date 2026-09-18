#include "exec_ir_internal.h"

#include <string.h>

static TZrBool eligibility_array_shape(
        const SZrArray *array,
        TZrSize elementSize) {
    return (TZrBool)(array->length <= UINT32_MAX &&
                     array->length <= array->capacity &&
                     (array->length == 0u ||
                      (array->isValid && array->head != ZR_NULL &&
                       array->elementSize == elementSize)));
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

TZrBool zr_parser_exec_ir_mark_place_values(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        TZrExecIrValueId firstPlaceValue,
        SZrExecIrDiagnostic *diagnostic) {
    TZrSize index;

    if (semantic == ZR_NULL || output == ZR_NULL ||
        !eligibility_array_shape(
                &semantic->locals, sizeof(SZrSemanticIrLocal)) ||
        !eligibility_array_shape(
                &semantic->loanFacts, sizeof(SZrSemanticIrLoanFact)) ||
        !eligibility_array_shape(
                &semantic->escapeFacts, sizeof(SZrSemanticEscapeFact)) ||
        semantic->places.places.length > output->valueCount ||
        firstPlaceValue == ZR_EXEC_IR_VALUE_ID_INVALID ||
        firstPlaceValue - 1u > output->valueCount -
                                      semantic->places.places.length) {
        return semantic != ZR_NULL
                       ? eligibility_fail(semantic, diagnostic)
                       : ZR_FALSE;
    }

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
    return ZR_TRUE;
}
