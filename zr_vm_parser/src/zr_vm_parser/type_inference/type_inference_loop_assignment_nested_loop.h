#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_NESTED_LOOP_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_NESTED_LOOP_H

#include "zr_vm_parser/type_inference.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_NestedLoopAssignsName(SZrAstNode *node,
                                                                   SZrString *name);
TZrBool ZrParser_TypeInferenceLoopAssignment_NestedLoopCollectTargets(SZrState *state,
                                                                      SZrAstNode *node,
                                                                      SZrArray *targetNames,
                                                                      TZrBool *outHasAssignment);
TZrBool ZrParser_TypeInferenceLoopAssignment_TryJoinNestedLoop(SZrCompilerState *cs,
                                                               SZrAstNode *node);

#endif
