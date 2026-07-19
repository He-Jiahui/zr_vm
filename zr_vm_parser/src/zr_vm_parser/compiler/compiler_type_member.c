#include "compiler_internal.h"

TZrBool compiler_type_member_capture_structured_return_type(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        SZrType *returnType) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    memberInfo->returnTypeName =
            returnType != ZR_NULL ? extract_type_name_string(cs, returnType) : ZR_NULL;
    memberInfo->hasStructuredReturnType = ZR_FALSE;
    memset(&memberInfo->structuredReturnType, 0, sizeof(memberInfo->structuredReturnType));
    if (returnType == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!ZrParser_AstTypeToInferredType_Convert(cs, returnType, &inferredType)) {
        return ZR_FALSE;
    }
    memberInfo->structuredReturnType = inferredType;
    memberInfo->hasStructuredReturnType = ZR_TRUE;
    return ZR_TRUE;
}
