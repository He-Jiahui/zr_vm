#ifndef ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_JOIN_INTERNAL_H
#define ZR_VM_PARSER_TYPE_INFERENCE_LOOP_ASSIGNMENT_JOIN_INTERNAL_H

#include "zr_vm_core/array.h"
#include "zr_vm_parser/type_inference.h"

typedef enum EZrTypeInferenceLoopAssignmentStepKind {
    ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_ASSIGNMENT = 0,
    ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_IF = 1,
    ZR_TYPE_INFERENCE_LOOP_ASSIGNMENT_STEP_NESTED_LOOP = 2
} EZrTypeInferenceLoopAssignmentStepKind;

typedef struct SZrTypeInferenceLoopAssignmentStep {
    EZrTypeInferenceLoopAssignmentStepKind kind;
    SZrString *name;
    SZrAstNode *assignment;
    SZrAstNode *right;
    TZrBool hasSelfDependentDelta;
    TZrBool resolveSelfDependentDeltaOnReplay;
    TZrInt64 selfDependentDeltaMin;
    TZrInt64 selfDependentDeltaMax;
} SZrTypeInferenceLoopAssignmentStep;

typedef struct SZrTypeInferenceLoopAssignmentPlan {
    SZrArray steps;
    SZrArray targetNames;
    TZrBool isInitialized;
} SZrTypeInferenceLoopAssignmentPlan;

typedef struct SZrTypeInferenceLoopAssignmentBindingType {
    SZrString *name;
    SZrInferredType type;
    TZrBool hasType;
} SZrTypeInferenceLoopAssignmentBindingType;

typedef struct SZrTypeInferenceLoopAssignmentResult {
    SZrArray bindings;
    TZrBool isInitialized;
} SZrTypeInferenceLoopAssignmentResult;

TZrBool ZrParser_TypeInferenceLoopAssignment_JoinBody(SZrCompilerState *cs,
                                                      SZrAstNode *body);

TZrBool ZrParser_TypeInferenceLoopAssignment_JoinBodyAtLeastOnce(SZrCompilerState *cs,
                                                                 SZrAstNode *body);

TZrBool ZrParser_TypeInferenceLoopAssignment_JoinBodyWithStep(SZrCompilerState *cs,
                                                              SZrAstNode *body,
                                                              SZrAstNode *step);

TZrBool ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrInferredType *type);

TZrBool ZrParser_TypeInferenceLoopAssignment_PushReplayScope(
        SZrCompilerState *cs,
        SZrTypeInferenceBranchScope *scope);

void ZrParser_TypeInferenceLoopAssignment_PopReplayScope(
        SZrCompilerState *cs,
        SZrTypeInferenceBranchScope *scope);

TZrBool ZrParser_TypeInferenceLoopAssignment_ResultInit(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentResult *result,
        TZrSize capacity);

void ZrParser_TypeInferenceLoopAssignment_ResultFree(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentResult *result);

const SZrInferredType *ZrParser_TypeInferenceLoopAssignment_ResultFindType(
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name);

TZrBool ZrParser_TypeInferenceLoopAssignment_ResultStoreType(
        SZrState *state,
        SZrTypeInferenceLoopAssignmentResult *result,
        SZrString *name,
        const SZrInferredType *type);

TZrBool ZrParser_TypeInferenceLoopAssignment_InferPlan(
        SZrCompilerState *cs,
        const SZrTypeInferenceLoopAssignmentPlan *plan,
        SZrTypeInferenceLoopAssignmentResult *result);

#endif
