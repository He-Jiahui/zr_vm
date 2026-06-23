#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SELF_DEPENDENCY_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_SELF_DEPENDENCY_H

#include "zr_vm_core/conf.h"
#include "zr_vm_parser/type_inference.h"

TZrBool ZrParser_TypeInferenceLoopAssignment_TrySelfDependentDelta(SZrCompilerState *cs,
                                                                   SZrAstNode *assignmentNode,
                                                                   SZrString *targetName,
                                                                   TZrInt64 *outDeltaMin,
                                                                   TZrInt64 *outDeltaMax);
SZrAstNode *ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(SZrAstNode *right,
                                                                              SZrString *targetName);
SZrString *ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaIdentifier(SZrAstNode *right,
                                                                             SZrString *targetName);
TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaSign(SZrAstNode *right,
                                                                    SZrString *targetName,
                                                                    TZrInt32 *outSign);
TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpressionsEqual(
        SZrAstNode *left,
        SZrAstNode *right,
        SZrString *targetName);
TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaUsesAnyTarget(
        SZrAstNode *right,
        SZrString *targetName,
        const SZrArray *targetNames);
TZrBool ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(SZrAstNode *node,
                                                                SZrString *name);
TZrBool ZrParser_TypeInferenceLoopAssignment_WidenSelfDependentType(SZrCompilerState *cs,
                                                                    const SZrInferredType *sourceType,
                                                                    TZrInt64 deltaMin,
                                                                    TZrInt64 deltaMax,
                                                                    SZrInferredType *outType);

#endif
