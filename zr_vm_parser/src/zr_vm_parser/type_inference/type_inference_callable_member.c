#include "type_inference_internal.h"

/* Imported static members are values when they are read without a call.  Keep
 * their callable shape in the inferred type so a source expression such as
 * `provider.add` can satisfy a declared fn(...) parameter.  The shape is
 * rebuilt from the structured member metadata and canonical type registry;
 * the display label is only the resulting presentation string. */
TZrBool inferred_type_from_static_callable_member(SZrCompilerState *cs,
                                                  const SZrTypeMemberInfo *memberInfo,
                                                  SZrInferredType *result) {
    SZrInferredType returnType;
    TZrTypeId callableTypeId;
    TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    SZrString *canonicalTypeName;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        result == ZR_NULL || memberInfo->declarationNode != ZR_NULL || !memberInfo->isStatic ||
        memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ||
        !memberInfo->parameterTypes.isValid ||
        memberInfo->parameterTypes.length != memberInfo->parameterCount ||
        memberInfo->genericParameters.length != 0U) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
    if (memberInfo->hasStructuredReturnType) {
        ZrParser_InferredType_Free(cs->state, &returnType);
        ZrParser_InferredType_Copy(cs->state, &returnType, &memberInfo->structuredReturnType);
    } else if (!inferred_type_from_type_name(cs, memberInfo->returnTypeName, &returnType)) {
        ZrParser_InferredType_Free(cs->state, &returnType);
        return ZR_FALSE;
    }

    callableTypeId = ZrParser_CanonicalType_FromFunctionSignature(
            cs->semanticContext,
            &memberInfo->parameterTypes,
            memberInfo->parameterPassingModes.isValid
                    ? &memberInfo->parameterPassingModes
                    : ZR_NULL,
            &returnType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    ZrParser_InferredType_Free(cs->state, &returnType);
    if (callableTypeId == ZR_SEMANTIC_ID_INVALID ||
        !ZrParser_CanonicalType_Format(cs->semanticContext,
                                       callableTypeId,
                                       typeBuffer,
                                       sizeof(typeBuffer))) {
        return ZR_FALSE;
    }

    canonicalTypeName = ZrCore_String_CreateFromNative(cs->state, typeBuffer);
    if (canonicalTypeName == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_InitFull(
            cs->state,
            result,
            ZR_VALUE_TYPE_CLOSURE,
            ZR_FALSE,
            canonicalTypeName);
    return ZR_TRUE;
}
