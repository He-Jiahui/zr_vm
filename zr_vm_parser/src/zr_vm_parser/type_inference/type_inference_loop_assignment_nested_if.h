#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_NESTED_IF_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_NESTED_IF_H

#include "zr_vm_parser/type_inference.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_NestedIfAssignsName(SZrAstNode *node,
                                                                 SZrString *name);
TZrBool ZrParser_TypeInferenceLoopAssignment_NestedIfCollectTargets(SZrState *state,
                                                                    SZrAstNode *node,
                                                                    SZrArray *targetNames,
                                                                    TZrBool *outHasAssignment);

#endif
