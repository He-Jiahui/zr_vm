#ifndef ZR_VM_PARSER_BOUND_EXPRESSION_H
#define ZR_VM_PARSER_BOUND_EXPRESSION_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/array.h"

struct SZrSemanticContext;
struct SZrState;

typedef enum EZrBoundExpressionKind {
    ZR_BOUND_EXPRESSION_INVALID = 0,
    ZR_BOUND_EXPRESSION_VALUE_CONSTRUCT
} EZrBoundExpressionKind;

typedef struct SZrBoundValueConstructArgumentInput {
    TZrTypeId typeId;
    SZrString *name;
    EZrCanonicalCallSiteMarker callSiteMarker;
    SZrFileRange sourceRange;
} SZrBoundValueConstructArgumentInput;

typedef struct SZrBoundValueConstructArgument {
    TZrTypeId typeId;
    SZrString *name;
    EZrCanonicalCallSiteMarker callSiteMarker;
    TZrUInt32 sourceIndex;
    TZrUInt32 parameterIndex;
    SZrFileRange sourceRange;
} SZrBoundValueConstructArgument;

typedef struct SZrBoundValueConstruct {
    EZrBoundExpressionKind kind;
    TZrTypeId typeId;
    TZrSymbolId constructorId;
    SZrArray arguments; /* SZrBoundValueConstructArgument */
    TZrTypeId resultTypeId;
    SZrFileRange sourceRange;
} SZrBoundValueConstruct;

ZR_PARSER_API void ZrParser_BoundValueConstruct_Init(
        struct SZrState *state,
        SZrBoundValueConstruct *bound);

ZR_PARSER_API void ZrParser_BoundValueConstruct_Free(
        struct SZrState *state,
        SZrBoundValueConstruct *bound);

ZR_PARSER_API EZrValueConstructorResolution ZrParser_BoundValueConstruct_Bind(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrBoundValueConstructArgumentInput *arguments,
        TZrSize argumentCount,
        SZrFileRange sourceRange,
        SZrBoundValueConstruct *outBound);

#endif // ZR_VM_PARSER_BOUND_EXPRESSION_H
