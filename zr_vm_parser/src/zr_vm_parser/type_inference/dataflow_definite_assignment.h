#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_DEFINITE_ASSIGNMENT_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_DEFINITE_ASSIGNMENT_H

#include "zr_vm_parser/conf.h"

typedef enum EZrParserDefiniteAssignmentState {
    ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT = 0,
    ZR_PARSER_DEFINITE_ASSIGNMENT_INIT,
    ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT
} EZrParserDefiniteAssignmentState;

ZR_PARSER_API TZrSize ZrParser_DefiniteAssignment_StateSize(TZrSize symbolCount);
ZR_PARSER_API void ZrParser_DefiniteAssignment_InitState(
        void *state,
        TZrSize symbolCount,
        EZrParserDefiniteAssignmentState initialState);
ZR_PARSER_API EZrParserDefiniteAssignmentState ZrParser_DefiniteAssignment_Get(
        const void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex);
ZR_PARSER_API void ZrParser_DefiniteAssignment_Set(
        void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex,
        EZrParserDefiniteAssignmentState value);
ZR_PARSER_API TZrBool ZrParser_DefiniteAssignment_Join(
        void *dst,
        const void *src,
        TZrSize symbolCount);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_DEFINITE_ASSIGNMENT_H
