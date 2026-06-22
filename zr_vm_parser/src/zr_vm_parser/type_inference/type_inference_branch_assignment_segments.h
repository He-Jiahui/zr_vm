#ifndef ZR_VM_PARSER_TYPE_INFERENCE_BRANCH_ASSIGNMENT_SEGMENTS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_BRANCH_ASSIGNMENT_SEGMENTS_H

#include "zr_vm_parser/type_inference.h"

TZrBool ZrParser_TypeInferenceBranchAssignment_ApplyJoinedSegments(SZrCompilerState *cs,
                                                                   const SZrInferredType *thenType,
                                                                   const SZrInferredType *elseType,
                                                                   SZrInferredType *outType);

#endif
