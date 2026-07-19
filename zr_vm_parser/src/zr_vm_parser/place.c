#include "zr_vm_parser/place.h"

#include <string.h>

static TZrBool place_base_is_valid(const SZrParserPlaceBase *base) {
    return (TZrBool)(base != ZR_NULL &&
                     base->kind >= ZR_PARSER_PLACE_BASE_LOCAL &&
                     base->kind < ZR_PARSER_PLACE_BASE_ENUM_MAX);
}

static TZrBool place_projection_is_valid(
        const SZrParserPlaceProjection *projection) {
    return (TZrBool)(projection != ZR_NULL &&
                     projection->kind >= ZR_PARSER_PLACE_PROJECTION_FIELD &&
                     projection->kind < ZR_PARSER_PLACE_PROJECTION_ENUM_MAX);
}

static void place_free(SZrState *state, SZrParserPlace *place) {
    if (state == ZR_NULL || place == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(state, &place->projections);
    memset(place, 0, sizeof(*place));
}

void ZrParser_PlaceGraph_Init(SZrState *state, SZrParserPlaceGraph *graph) {
    if (state == ZR_NULL || graph == ZR_NULL) {
        return;
    }

    memset(graph, 0, sizeof(*graph));
    graph->state = state;
    ZrCore_Array_Init(
            state,
            &graph->places,
            sizeof(SZrParserPlace),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

void ZrParser_PlaceGraph_Free(SZrState *state, SZrParserPlaceGraph *graph) {
    TZrSize index;

    if (state == ZR_NULL || graph == ZR_NULL) {
        return;
    }

    if (graph->places.isValid) {
        for (index = 0; index < graph->places.length; index++) {
            place_free(
                    state,
                    (SZrParserPlace *)ZrCore_Array_Get(&graph->places, index));
        }
    }
    ZrCore_Array_Free(state, &graph->places);
    graph->state = ZR_NULL;
}

TZrPlaceId ZrParser_PlaceGraph_AddBase(SZrParserPlaceGraph *graph,
                                       const SZrParserPlaceBase *base,
                                       TZrTypeId typeId,
                                       SZrFileRange sourceRange) {
    SZrParserPlace place;

    if (graph == ZR_NULL || graph->state == ZR_NULL ||
        !graph->places.isValid || !place_base_is_valid(base)) {
        return ZR_PLACE_ID_INVALID;
    }

    memset(&place, 0, sizeof(place));
    place.id = (TZrPlaceId)(graph->places.length + 1U);
    place.parentId = ZR_PLACE_ID_INVALID;
    place.typeId = typeId;
    place.base = *base;
    place.sourceRange = sourceRange;
    ZrCore_Array_Init(
            graph->state,
            &place.projections,
            sizeof(SZrParserPlaceProjection),
            1U);
    ZrCore_Array_Push(graph->state, &graph->places, &place);
    return place.id;
}

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

TZrPlaceId ZrParser_PlaceGraph_Project(
        SZrParserPlaceGraph *graph,
        TZrPlaceId parentId,
        const SZrParserPlaceProjection *projection,
        TZrTypeId typeId,
        SZrFileRange sourceRange) {
    const SZrParserPlace *parent;
    SZrParserPlace place;
    TZrSize index;

    if (graph == ZR_NULL || graph->state == ZR_NULL ||
        !place_projection_is_valid(projection)) {
        return ZR_PLACE_ID_INVALID;
    }

    parent = ZrParser_PlaceGraph_Get(graph, parentId);
    if (parent == ZR_NULL) {
        return ZR_PLACE_ID_INVALID;
    }

    memset(&place, 0, sizeof(place));
    place.id = (TZrPlaceId)(graph->places.length + 1U);
    place.parentId = parentId;
    place.typeId = typeId;
    place.base = parent->base;
    place.sourceRange = sourceRange;
    ZrCore_Array_Init(
            graph->state,
            &place.projections,
            sizeof(SZrParserPlaceProjection),
            parent->projections.length + 1U);
    for (index = 0; index < parent->projections.length; index++) {
        const SZrParserPlaceProjection *parentProjection =
                ZrParser_Place_ProjectionAt(parent, index);
        ZrCore_Array_Push(
                graph->state,
                &place.projections,
                (TZrPtr)parentProjection);
    }
    ZrCore_Array_Push(graph->state, &place.projections, (TZrPtr)projection);
    ZrCore_Array_Push(graph->state, &graph->places, &place);
    return place.id;
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

static TZrBool place_contains_dereference(const SZrParserPlace *place) {
    TZrSize index;

    if (place == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < place->projections.length; index++) {
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
            place_contains_dereference(left) ||
            place_contains_dereference(right)) {
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
            return place_projection_divergence(leftProjection, rightProjection);
        }
    }

    if (left->projections.length == right->projections.length) {
        return ZR_PARSER_PLACE_EQUAL;
    }
    return ZR_PARSER_PLACE_OVERLAP;
}
