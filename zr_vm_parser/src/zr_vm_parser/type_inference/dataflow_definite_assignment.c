#include "dataflow_definite_assignment.h"

static EZrParserDefiniteAssignmentState definite_assignment_join_value(
        EZrParserDefiniteAssignmentState left,
        EZrParserDefiniteAssignmentState right) {
    if (left == right) {
        return left;
    }
    return ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
}

TZrSize ZrParser_DefiniteAssignment_StateSize(TZrSize symbolCount) {
    return symbolCount * sizeof(EZrParserDefiniteAssignmentState);
}

void ZrParser_DefiniteAssignment_InitState(
        void *state,
        TZrSize symbolCount,
        EZrParserDefiniteAssignmentState initialState) {
    EZrParserDefiniteAssignmentState *states = (EZrParserDefiniteAssignmentState *)state;
    TZrSize index;

    if (states == ZR_NULL) {
        return;
    }

    for (index = 0; index < symbolCount; index++) {
        states[index] = initialState;
    }
}

EZrParserDefiniteAssignmentState ZrParser_DefiniteAssignment_Get(
        const void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex) {
    const EZrParserDefiniteAssignmentState *states =
            (const EZrParserDefiniteAssignmentState *)state;

    if (states == ZR_NULL || symbolIndex >= symbolCount) {
        return ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
    }

    return states[symbolIndex];
}

void ZrParser_DefiniteAssignment_Set(
        void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex,
        EZrParserDefiniteAssignmentState value) {
    EZrParserDefiniteAssignmentState *states = (EZrParserDefiniteAssignmentState *)state;

    if (states == ZR_NULL || symbolIndex >= symbolCount) {
        return;
    }

    states[symbolIndex] = value;
}

TZrBool ZrParser_DefiniteAssignment_Join(
        void *dst,
        const void *src,
        TZrSize symbolCount) {
    EZrParserDefiniteAssignmentState *dstStates = (EZrParserDefiniteAssignmentState *)dst;
    const EZrParserDefiniteAssignmentState *srcStates =
            (const EZrParserDefiniteAssignmentState *)src;
    TZrBool changed = ZR_FALSE;
    TZrSize index;

    if (dstStates == ZR_NULL || srcStates == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < symbolCount; index++) {
        EZrParserDefiniteAssignmentState joined =
                definite_assignment_join_value(dstStates[index], srcStates[index]);
        if (joined != dstStates[index]) {
            dstStates[index] = joined;
            changed = ZR_TRUE;
        }
    }

    return changed;
}
