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

EZrParserPlaceExpressionKind ZrParser_PlaceExpression_Classify(
        const SZrAstNode *node) {
    EZrParserPlaceExpressionKind kind;
    const SZrPrimaryExpression *primary;

    if (node == ZR_NULL) {
        return ZR_PARSER_PLACE_EXPRESSION_INVALID;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_PARSER_PLACE_EXPRESSION_LOCAL;
    }
    if (node->type == ZR_AST_MEMBER_EXPRESSION) {
        return node->data.memberExpression.computed
                       ? ZR_PARSER_PLACE_EXPRESSION_INDEX
                       : ZR_PARSER_PLACE_EXPRESSION_FIELD;
    }
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_PARSER_PLACE_EXPRESSION_INVALID;
    }

    primary = &node->data.primaryExpression;
    kind = ZrParser_PlaceExpression_Classify(primary->property);
    if (kind == ZR_PARSER_PLACE_EXPRESSION_INVALID) {
        return kind;
    }
    if (primary->members == ZR_NULL) {
        return kind;
    }

    for (TZrSize index = 0u; index < primary->members->count; index++) {
        const SZrAstNode *member = primary->members->nodes[index];
        if (member == ZR_NULL || member->type != ZR_AST_MEMBER_EXPRESSION) {
            return ZR_PARSER_PLACE_EXPRESSION_INVALID;
        }
        kind = member->data.memberExpression.computed
                       ? ZR_PARSER_PLACE_EXPRESSION_INDEX
                       : ZR_PARSER_PLACE_EXPRESSION_FIELD;
    }
    return kind;
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
