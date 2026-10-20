#include "compiler_type_field.h"

#include "zr_vm_parser/type_inference.h"

/* The destination is a fresh field record; the donor remains initialized. */
void compiler_type_field_move_inferred_type(
        SZrCompilerState *cs, SZrTypeMemberInfo *memberInfo,
        SZrInferredType *inferredType) {
    if (cs == ZR_NULL || memberInfo == ZR_NULL || inferredType == ZR_NULL) {
        return;
    }

    memberInfo->structuredReturnType = *inferredType;
    memberInfo->hasStructuredReturnType = ZR_TRUE;
    ZrParser_InferredType_Init(cs->state, inferredType, ZR_VALUE_TYPE_OBJECT);
}

TZrBool compiler_type_field_capture_annotation(
        SZrCompilerState *cs, SZrTypeMemberInfo *memberInfo,
        const SZrType *fieldType) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fieldType == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(cs, fieldType, &inferredType)) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return ZR_FALSE;
    }
    compiler_type_field_move_inferred_type(cs, memberInfo, &inferredType);
    return ZR_TRUE;
}
