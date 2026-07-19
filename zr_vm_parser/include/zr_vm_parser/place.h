#ifndef ZR_VM_PARSER_PLACE_H
#define ZR_VM_PARSER_PLACE_H

#include "zr_vm_core/array.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"

typedef TZrUInt32 TZrPlaceId;
typedef TZrUInt32 TZrValueId;

#define ZR_PLACE_ID_INVALID ((TZrPlaceId)0U)
#define ZR_VALUE_ID_INVALID ((TZrValueId)0U)

typedef enum EZrParserPlaceBaseKind {
    ZR_PARSER_PLACE_BASE_LOCAL = 0,
    ZR_PARSER_PLACE_BASE_PARAMETER,
    ZR_PARSER_PLACE_BASE_THIS,
    ZR_PARSER_PLACE_BASE_STATIC,
    ZR_PARSER_PLACE_BASE_TEMPORARY,
    ZR_PARSER_PLACE_BASE_RETURN_SLOT,
    ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE,
    ZR_PARSER_PLACE_BASE_ENUM_MAX
} EZrParserPlaceBaseKind;

typedef struct SZrParserPlaceBase {
    EZrParserPlaceBaseKind kind;
    TZrUInt32 identity;
} SZrParserPlaceBase;

typedef enum EZrParserPlaceProjectionKind {
    ZR_PARSER_PLACE_PROJECTION_FIELD = 0,
    ZR_PARSER_PLACE_PROJECTION_INDEX,
    ZR_PARSER_PLACE_PROJECTION_CONSTANT_INDEX,
    ZR_PARSER_PLACE_PROJECTION_DEREFERENCE,
    ZR_PARSER_PLACE_PROJECTION_UNION_VARIANT,
    ZR_PARSER_PLACE_PROJECTION_TUPLE_ELEMENT,
    ZR_PARSER_PLACE_PROJECTION_ENUM_MAX
} EZrParserPlaceProjectionKind;

typedef struct SZrParserPlaceProjection {
    EZrParserPlaceProjectionKind kind;
    union {
        TZrSymbolId symbolId;
        TZrValueId valueId;
        TZrUInt32 index;
    } data;
} SZrParserPlaceProjection;

typedef enum EZrParserPlaceOverlap {
    ZR_PARSER_PLACE_EQUAL = 0,
    ZR_PARSER_PLACE_DISJOINT,
    ZR_PARSER_PLACE_OVERLAP,
    ZR_PARSER_PLACE_UNKNOWN
} EZrParserPlaceOverlap;

typedef enum EZrParserPlaceExpressionKind {
    ZR_PARSER_PLACE_EXPRESSION_INVALID = 0,
    ZR_PARSER_PLACE_EXPRESSION_LOCAL,
    ZR_PARSER_PLACE_EXPRESSION_FIELD,
    ZR_PARSER_PLACE_EXPRESSION_INDEX,
    ZR_PARSER_PLACE_EXPRESSION_ENUM_MAX
} EZrParserPlaceExpressionKind;

typedef struct SZrParserPlace {
    TZrPlaceId id;
    TZrPlaceId parentId;
    TZrTypeId typeId;
    SZrParserPlaceBase base;
    SZrArray projections; /* SZrParserPlaceProjection */
    SZrFileRange sourceRange;
} SZrParserPlace;

typedef struct SZrParserPlaceGraph {
    SZrState *state;
    SZrArray places; /* SZrParserPlace */
} SZrParserPlaceGraph;

ZR_PARSER_API void ZrParser_PlaceGraph_Init(SZrState *state,
                                             SZrParserPlaceGraph *graph);
ZR_PARSER_API void ZrParser_PlaceGraph_Free(SZrState *state,
                                             SZrParserPlaceGraph *graph);
ZR_PARSER_API TZrPlaceId ZrParser_PlaceGraph_AddBase(
        SZrParserPlaceGraph *graph,
        const SZrParserPlaceBase *base,
        TZrTypeId typeId,
        SZrFileRange sourceRange);
ZR_PARSER_API TZrPlaceId ZrParser_PlaceGraph_Project(
        SZrParserPlaceGraph *graph,
        TZrPlaceId parentId,
        const SZrParserPlaceProjection *projection,
        TZrTypeId typeId,
        SZrFileRange sourceRange);
ZR_PARSER_API const SZrParserPlace *ZrParser_PlaceGraph_Get(
        const SZrParserPlaceGraph *graph,
        TZrPlaceId placeId);
ZR_PARSER_API const SZrParserPlaceProjection *ZrParser_Place_ProjectionAt(
        const SZrParserPlace *place,
        TZrSize index);
ZR_PARSER_API EZrParserPlaceOverlap ZrParser_PlaceGraph_Overlap(
        const SZrParserPlaceGraph *graph,
        TZrPlaceId leftId,
        TZrPlaceId rightId);
ZR_PARSER_API EZrParserPlaceExpressionKind ZrParser_PlaceExpression_Classify(
        const SZrAstNode *node);

#endif /* ZR_VM_PARSER_PLACE_H */
