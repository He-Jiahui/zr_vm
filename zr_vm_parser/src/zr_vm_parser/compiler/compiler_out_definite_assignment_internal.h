#ifndef ZR_VM_PARSER_COMPILER_OUT_DEFINITE_ASSIGNMENT_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_OUT_DEFINITE_ASSIGNMENT_INTERNAL_H

#include "compiler_internal.h"

typedef struct SZrOutParameterInfo {
    SZrString *name;
    SZrString **fieldNames;
    TZrSize fieldCount;
    TZrSize slotOffset;
    TZrSize slotCount;
} SZrOutParameterInfo;

typedef struct SZrOutTrackedState {
    SZrOutParameterInfo *parameters;
    TZrSize parameterCount;
    TZrSize slotCount;
} SZrOutTrackedState;

typedef struct SZrOutPlaceRef {
    TZrInt32 parameterIndex;
    TZrInt32 fieldIndex;
} SZrOutPlaceRef;

typedef struct SZrOutFlowAnalysis {
    SZrCompilerState *compiler;
    const SZrOutTrackedState *tracked;
    TZrBool *breakState;
    TZrBool hasBreakState;
    TZrBool *exceptionState;
    TZrBool hasExceptionState;
} SZrOutFlowAnalysis;

const TZrChar *out_string_native(SZrString *value);
TZrBool *out_state_new(const SZrOutTrackedState *tracked);
void out_state_copy(const SZrOutTrackedState *tracked,
                    const TZrBool *source,
                    TZrBool *destination);
void out_state_intersect(const SZrOutTrackedState *tracked,
                         TZrBool *destination,
                         const TZrBool *other);
TZrBool out_report_incomplete(SZrCompilerState *cs,
                              const SZrOutTrackedState *tracked,
                              const TZrBool *state,
                              SZrFileRange location);
SZrOutPlaceRef out_resolve_place(const SZrOutTrackedState *tracked,
                                 const SZrAstNode *node);
void out_mark_place(const SZrOutTrackedState *tracked,
                    TZrBool *state,
                    SZrOutPlaceRef place);
TZrBool out_place_initialized(const SZrOutTrackedState *tracked,
                              const TZrBool *state,
                              SZrOutPlaceRef place);
TZrBool out_analyze_statement(SZrOutFlowAnalysis *analysis,
                              SZrAstNode *node,
                              const TZrBool *before,
                              TZrBool *after,
                              TZrBool *continues);

#endif /* ZR_VM_PARSER_COMPILER_OUT_DEFINITE_ASSIGNMENT_INTERNAL_H */
