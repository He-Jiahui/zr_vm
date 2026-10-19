#include "compiler_type_prototype_lifecycle.h"

#include "zr_vm_core/array.h"

/* These records own inline inferred types and arrays, not their AST/GC references. */
void compiler_type_member_free_owned_metadata(
        SZrState *state, SZrTypeMemberInfo *memberInfo) {
    if (state == ZR_NULL || memberInfo == ZR_NULL) {
        return;
    }

    if (memberInfo != ZR_NULL && memberInfo->hasStructuredReturnType) {
        ZrParser_InferredType_Free(state, &memberInfo->structuredReturnType);
        memberInfo->hasStructuredReturnType = ZR_FALSE;
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->parameterTypes.isValid &&
        memberInfo->parameterTypes.head != ZR_NULL &&
        memberInfo->parameterTypes.capacity > 0 &&
        memberInfo->parameterTypes.elementSize > 0) {
        for (TZrSize paramIndex = 0; paramIndex < memberInfo->parameterTypes.length; paramIndex++) {
            SZrInferredType *paramType =
                    (SZrInferredType *)ZrCore_Array_Get(&memberInfo->parameterTypes, paramIndex);
            if (paramType != ZR_NULL) {
                ZrParser_InferredType_Free(state, paramType);
            }
        }
        ZrCore_Array_Free(state, &memberInfo->parameterTypes);
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->parameterNames.isValid &&
        memberInfo->parameterNames.head != ZR_NULL &&
        memberInfo->parameterNames.capacity > 0 &&
        memberInfo->parameterNames.elementSize > 0) {
        ZrCore_Array_Free(state, &memberInfo->parameterNames);
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->parameterHasDefaultValues.isValid &&
        memberInfo->parameterHasDefaultValues.head != ZR_NULL &&
        memberInfo->parameterHasDefaultValues.capacity > 0 &&
        memberInfo->parameterHasDefaultValues.elementSize > 0) {
        ZrCore_Array_Free(state, &memberInfo->parameterHasDefaultValues);
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->parameterDefaultValues.isValid &&
        memberInfo->parameterDefaultValues.head != ZR_NULL &&
        memberInfo->parameterDefaultValues.capacity > 0 &&
        memberInfo->parameterDefaultValues.elementSize > 0) {
        ZrCore_Array_Free(state, &memberInfo->parameterDefaultValues);
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->genericParameters.isValid &&
        memberInfo->genericParameters.head != ZR_NULL &&
        memberInfo->genericParameters.capacity > 0 &&
        memberInfo->genericParameters.elementSize > 0) {
        for (TZrSize genericIndex = 0; genericIndex < memberInfo->genericParameters.length;
             genericIndex++) {
            SZrTypeGenericParameterInfo *genericInfo =
                    (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&memberInfo->genericParameters,
                                                                    genericIndex);
            if (genericInfo != ZR_NULL &&
                genericInfo->constraintTypeNames.isValid &&
                genericInfo->constraintTypeNames.head != ZR_NULL &&
                genericInfo->constraintTypeNames.capacity > 0 &&
                genericInfo->constraintTypeNames.elementSize > 0) {
                ZrCore_Array_Free(state, &genericInfo->constraintTypeNames);
            }
        }
        ZrCore_Array_Free(state, &memberInfo->genericParameters);
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->parameterPassingModes.isValid &&
        memberInfo->parameterPassingModes.head != ZR_NULL &&
        memberInfo->parameterPassingModes.capacity > 0 &&
        memberInfo->parameterPassingModes.elementSize > 0) {
        ZrCore_Array_Free(state, &memberInfo->parameterPassingModes);
    }
    if (memberInfo != ZR_NULL &&
        memberInfo->decorators.isValid &&
        memberInfo->decorators.head != ZR_NULL &&
        memberInfo->decorators.capacity > 0 &&
        memberInfo->decorators.elementSize > 0) {
        ZrCore_Array_Free(state, &memberInfo->decorators);
    }
}

void compiler_type_prototype_free_owned_metadata(
        SZrState *state, SZrTypePrototypeInfo *info) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }

    if (info->inherits.isValid && info->inherits.head != ZR_NULL &&
        info->inherits.capacity > 0 && info->inherits.elementSize > 0) {
        ZrCore_Array_Free(state, &info->inherits);
    }
    if (info->implements.isValid && info->implements.head != ZR_NULL &&
        info->implements.capacity > 0 && info->implements.elementSize > 0) {
        ZrCore_Array_Free(state, &info->implements);
    }
    if (info->genericParameters.isValid && info->genericParameters.head != ZR_NULL &&
        info->genericParameters.capacity > 0 && info->genericParameters.elementSize > 0) {
        for (TZrSize genericIndex = 0; genericIndex < info->genericParameters.length; genericIndex++) {
            SZrTypeGenericParameterInfo *genericInfo =
                    (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&info->genericParameters, genericIndex);
            if (genericInfo != ZR_NULL &&
                genericInfo->constraintTypeNames.isValid &&
                genericInfo->constraintTypeNames.head != ZR_NULL &&
                genericInfo->constraintTypeNames.capacity > 0 &&
                genericInfo->constraintTypeNames.elementSize > 0) {
                ZrCore_Array_Free(state, &genericInfo->constraintTypeNames);
            }
        }
        ZrCore_Array_Free(state, &info->genericParameters);
    }
    if (info->decorators.isValid && info->decorators.head != ZR_NULL &&
        info->decorators.capacity > 0 && info->decorators.elementSize > 0) {
        ZrCore_Array_Free(state, &info->decorators);
    }
    if (info->members.isValid && info->members.head != ZR_NULL &&
        info->members.capacity > 0 && info->members.elementSize > 0) {
        for (TZrSize memberIndex = 0; memberIndex < info->members.length; memberIndex++) {
            SZrTypeMemberInfo *memberInfo =
                     (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, memberIndex);
            compiler_type_member_free_owned_metadata(state, memberInfo);
        }
        ZrCore_Array_Free(state, &info->members);
    }
}
