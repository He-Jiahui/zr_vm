#include "zr_vm_parser/place.h"

const SZrParserPlace *ZrParser_PlaceGraph_Get(
        const SZrParserPlaceGraph *graph,
        TZrPlaceId placeId) {
    if (graph == ZR_NULL || !graph->places.isValid ||
        placeId == ZR_PLACE_ID_INVALID || placeId > graph->places.length) {
        return ZR_NULL;
    }

    return (const SZrParserPlace *)ZrCore_Array_Get(
            (SZrArray *)&graph->places,
            (TZrSize)placeId - 1U);
}

const SZrParserPlaceProjection *ZrParser_Place_ProjectionAt(
        const SZrParserPlace *place,
        TZrSize index) {
    if (place == ZR_NULL || !place->projections.isValid ||
        index >= place->projections.length) {
        return ZR_NULL;
    }

    return (const SZrParserPlaceProjection *)ZrCore_Array_Get(
            (SZrArray *)&place->projections,
            index);
}

static TZrBool place_projection_equals(
        const SZrParserPlaceProjection *left,
        const SZrParserPlaceProjection *right) {
    if (left == ZR_NULL || right == ZR_NULL || left->kind != right->kind) {
        return ZR_FALSE;
    }

    switch (left->kind) {
        case ZR_PARSER_PLACE_PROJECTION_FIELD:
        case ZR_PARSER_PLACE_PROJECTION_UNION_VARIANT:
            return (TZrBool)(left->data.symbolId == right->data.symbolId);
        case ZR_PARSER_PLACE_PROJECTION_INDEX:
            return (TZrBool)(left->data.valueId == right->data.valueId);
        case ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX:
        case ZR_PARSER_PLACE_PROJECTION_TUPLE_ELEMENT:
            return (TZrBool)(left->data.index == right->data.index);
        case ZR_PARSER_PLACE_PROJECTION_DEREFERENCE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool place_contains_dereference_from(
        const SZrParserPlace *place, TZrSize firstProjection) {
    TZrSize index;

    if (place == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = firstProjection; index < place->projections.length; index++) {
        const SZrParserPlaceProjection *projection =
                ZrParser_Place_ProjectionAt(place, index);
        if (projection != ZR_NULL &&
            projection->kind == ZR_PARSER_PLACE_PROJECTION_DEREFERENCE) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static EZrParserPlaceOverlap place_projection_divergence(
        const SZrParserPlaceProjection *left,
        const SZrParserPlaceProjection *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_PARSER_PLACE_UNKNOWN;
    }
    if (left->kind == ZR_PARSER_PLACE_PROJECTION_DEREFERENCE ||
        right->kind == ZR_PARSER_PLACE_PROJECTION_DEREFERENCE ||
        left->kind == ZR_PARSER_PLACE_PROJECTION_INDEX ||
        right->kind == ZR_PARSER_PLACE_PROJECTION_INDEX) {
        return ZR_PARSER_PLACE_UNKNOWN;
    }
    if (left->kind != right->kind) {
        return ZR_PARSER_PLACE_UNKNOWN;
    }

    switch (left->kind) {
        case ZR_PARSER_PLACE_PROJECTION_FIELD:
        case ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX:
        case ZR_PARSER_PLACE_PROJECTION_TUPLE_ELEMENT:
            return ZR_PARSER_PLACE_DISJOINT;
        case ZR_PARSER_PLACE_PROJECTION_UNION_VARIANT:
            return ZR_PARSER_PLACE_OVERLAP;
        default:
            return ZR_PARSER_PLACE_UNKNOWN;
    }
}

EZrParserPlaceOverlap ZrParser_PlaceGraph_Overlap(
        const SZrParserPlaceGraph *graph,
        TZrPlaceId leftId,
        TZrPlaceId rightId) {
    const SZrParserPlace *left = ZrParser_PlaceGraph_Get(graph, leftId);
    const SZrParserPlace *right = ZrParser_PlaceGraph_Get(graph, rightId);
    TZrSize commonCount;
    TZrSize index;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_PARSER_PLACE_UNKNOWN;
    }
    if (left->base.kind != right->base.kind ||
        left->base.identity != right->base.identity) {
        if (left->base.kind == ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE ||
            right->base.kind == ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE ||
            place_contains_dereference_from(left, 0u) ||
            place_contains_dereference_from(right, 0u)) {
            return ZR_PARSER_PLACE_UNKNOWN;
        }
        return ZR_PARSER_PLACE_DISJOINT;
    }

    commonCount = left->projections.length < right->projections.length
                          ? left->projections.length
                          : right->projections.length;
    for (index = 0; index < commonCount; index++) {
        const SZrParserPlaceProjection *leftProjection =
                ZrParser_Place_ProjectionAt(left, index);
        const SZrParserPlaceProjection *rightProjection =
                ZrParser_Place_ProjectionAt(right, index);

        if (!place_projection_equals(leftProjection, rightProjection)) {
            /* Divergent slots can hold pointers to the same target. A shared
             * dereference before this divergence still permits disjoint fields. */
            if (place_contains_dereference_from(left, index + 1u) ||
                place_contains_dereference_from(right, index + 1u)) {
                return ZR_PARSER_PLACE_UNKNOWN;
            }
            return place_projection_divergence(leftProjection, rightProjection);
        }
    }

    if (left->projections.length == right->projections.length) {
        return ZR_PARSER_PLACE_EQUAL;
    }
    return ZR_PARSER_PLACE_OVERLAP;
}
