#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_DELTA_TERMS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SEQUENCE_SYMBOLIC_DELTA_TERMS_H

#include "type_inference_loop_assignment_sequence.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelTerms(
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *termCount,
        SZrString *sequenceName);

TZrBool ZrParser_TypeInferenceLoopAssignment_SequenceSymbolicDeltaCancelCollectedTerms(
        SZrTypeInferenceLoopAssignmentSequenceSymbolicTerm *terms,
        TZrSize *rawTermCount,
        TZrSize *termCount,
        SZrString *sequenceName);

#endif
