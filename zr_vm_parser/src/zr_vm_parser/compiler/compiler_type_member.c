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

TZrBool compiler_type_member_register_function_symbol(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo) {
    TZrTypeId ownerTypeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->name == ZR_NULL || memberInfo->ownerTypeName == ZR_NULL ||
        memberInfo->declarationNode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (memberInfo->symbolId != ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    ownerTypeId = ZrParser_CanonicalType_FromName(
            cs->semanticContext, memberInfo->ownerTypeName);
    if (ownerTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    memberInfo->symbolId = ZrParser_Semantic_RegisterSymbol(
            cs->semanticContext,
            memberInfo->name,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            ownerTypeId,
            ZR_SEMANTIC_ID_INVALID,
            memberInfo->declarationNode,
            memberInfo->declarationNode->location);
    return memberInfo->symbolId != ZR_SEMANTIC_ID_INVALID;
}
