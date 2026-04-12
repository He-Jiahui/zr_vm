//
// Created by Auto on 2026/04/02.
//

#include "type_inference_internal.h"
#include "compiler_internal.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <string.h>

static void resolved_call_signature_init(SZrState *state, SZrResolvedCallSignature *signature) {
    if (state == ZR_NULL || signature == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &signature->returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&signature->parameterTypes);
    ZrCore_Array_Construct(&signature->parameterPassingModes);
}

void free_resolved_call_signature(SZrState *state, SZrResolvedCallSignature *signature) {
    if (state == ZR_NULL || signature == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Free(state, &signature->returnType);
    free_inferred_type_array(state, &signature->parameterTypes);
    if (signature->parameterPassingModes.isValid &&
        signature->parameterPassingModes.head != ZR_NULL &&
        signature->parameterPassingModes.capacity > 0 &&
        signature->parameterPassingModes.elementSize > 0) {
        ZrCore_Array_Free(state, &signature->parameterPassingModes);
    }
    ZrCore_Array_Construct(&signature->parameterTypes);
    ZrCore_Array_Construct(&signature->parameterPassingModes);
}

static TZrBool copy_parameter_passing_modes(SZrState *state, SZrArray *dest, const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(dest);
    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, dest, sizeof(EZrParameterPassingMode), src->length);
    for (TZrSize index = 0; index < src->length; index++) {
        EZrParameterPassingMode *mode = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)src, index);
        if (mode != ZR_NULL) {
            ZrCore_Array_Push(state, dest, mode);
        }
    }

    return ZR_TRUE;
}

static TZrBool copy_inferred_type_array(SZrState *state, SZrArray *dest, const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(dest);
    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, dest, sizeof(SZrInferredType), src->length);
    for (TZrSize index = 0; index < src->length; index++) {
        SZrInferredType *srcType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)src, index);
        SZrInferredType copiedType;
        if (srcType == ZR_NULL) {
            continue;
        }

        ZrParser_InferredType_Copy(state, &copiedType, srcType);
        ZrCore_Array_Push(state, dest, &copiedType);
    }

    return ZR_TRUE;
}

static TZrBool copy_direct_function_signature(SZrState *state,
                                              const SZrFunctionTypeInfo *funcType,
                                              SZrResolvedCallSignature *signature) {
    if (state == ZR_NULL || funcType == ZR_NULL || signature == ZR_NULL) {
        return ZR_FALSE;
    }

    resolved_call_signature_init(state, signature);
    ZrParser_InferredType_Copy(state, &signature->returnType, &funcType->returnType);
    return copy_inferred_type_array(state, &signature->parameterTypes, &funcType->paramTypes) &&
           copy_parameter_passing_modes(state, &signature->parameterPassingModes, &funcType->parameterPassingModes);
}

static TZrBool copy_direct_member_signature(SZrCompilerState *cs,
                                            const SZrTypeMemberInfo *memberInfo,
                                            SZrResolvedCallSignature *signature) {
    if (cs == ZR_NULL || memberInfo == ZR_NULL || signature == ZR_NULL) {
        return ZR_FALSE;
    }

    resolved_call_signature_init(cs->state, signature);
    if (!copy_inferred_type_array(cs->state, &signature->parameterTypes, &memberInfo->parameterTypes) ||
        !copy_parameter_passing_modes(cs->state,
                                      &signature->parameterPassingModes,
                                      &memberInfo->parameterPassingModes)) {
        return ZR_FALSE;
    }

    if (memberInfo->returnTypeName == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &signature->returnType, ZR_VALUE_TYPE_NULL);
        return ZR_TRUE;
    }

    return inferred_type_from_type_name(cs, memberInfo->returnTypeName, &signature->returnType);
}

static TZrBool append_text_fragment_local(TZrChar *buffer,
                                          TZrSize bufferSize,
                                          TZrSize *offset,
                                          const TZrChar *fragment) {
    TZrSize fragmentLength;

    if (buffer == ZR_NULL || bufferSize == 0 || offset == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    fragmentLength = strlen(fragment);
    if (*offset + fragmentLength >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *offset, fragment, fragmentLength);
    *offset += fragmentLength;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

static SZrString *binding_type_name_for_substitution(SZrState *state, const SZrGenericCallBinding *binding) {
    TZrChar buffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
    const TZrChar *typeText;

    if (state == ZR_NULL || binding == ZR_NULL || !binding->isBound) {
        return ZR_NULL;
    }

    if (binding->inferredType.typeName != ZR_NULL) {
        return binding->inferredType.typeName;
    }

    typeText = ZrParser_TypeNameString_Get(state, &binding->inferredType, buffer, sizeof(buffer));
    if (typeText == ZR_NULL || typeText[0] == '\0') {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)typeText, strlen(typeText));
}

static SZrString *substitute_generic_type_name_from_bindings(SZrState *state,
                                                             SZrString *sourceTypeName,
                                                             const SZrArray *bindings) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentNames;
    const TZrChar *nativeTypeName;
    TZrSize nativeTypeNameLength;

    if (state == ZR_NULL || sourceTypeName == ZR_NULL || bindings == ZR_NULL) {
        return sourceTypeName;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get((SZrArray *)bindings, index);
        SZrString *boundTypeName;

        if (binding == ZR_NULL || binding->parameterInfo == ZR_NULL || binding->parameterInfo->name == ZR_NULL) {
            continue;
        }

        if (!ZrCore_String_Equal(binding->parameterInfo->name, sourceTypeName)) {
            continue;
        }

        boundTypeName = binding_type_name_for_substitution(state, binding);
        return boundTypeName != ZR_NULL ? boundTypeName : sourceTypeName;
    }

    nativeTypeName = ZrCore_String_GetNativeString(sourceTypeName);
    nativeTypeNameLength = nativeTypeName != ZR_NULL ? strlen(nativeTypeName) : 0;
    if (nativeTypeName != ZR_NULL &&
        nativeTypeNameLength > 2 &&
        strcmp(nativeTypeName + nativeTypeNameLength - 2, "[]") == 0) {
        SZrString *elementTypeName =
            ZrCore_String_Create(state, (TZrNativeString)nativeTypeName, nativeTypeNameLength - 2);
        SZrString *substitutedElement = substitute_generic_type_name_from_bindings(state, elementTypeName, bindings);
        TZrChar buffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
        const TZrChar *elementNative =
                substitutedElement != ZR_NULL ? ZrCore_String_GetNativeString(substitutedElement) : ZR_NULL;

        if (elementNative == ZR_NULL) {
            return sourceTypeName;
        }

        snprintf(buffer, sizeof(buffer), "%s[]", elementNative);
        return ZrCore_String_CreateFromNative(state, buffer);
    }

    ZrCore_Array_Construct(&argumentNames);
    if (try_parse_generic_instance_type_name(state, sourceTypeName, &baseName, &argumentNames)) {
        TZrChar buffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
        TZrSize offset = 0;
        SZrString *resultName = ZR_NULL;

        buffer[0] = '\0';
        if (!append_text_fragment_local(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(baseName)) ||
            !append_text_fragment_local(buffer, sizeof(buffer), &offset, "<")) {
            ZrCore_Array_Free(state, &argumentNames);
            return sourceTypeName;
        }

        for (TZrSize index = 0; index < argumentNames.length; index++) {
            SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentNames, index);
            SZrString *substitutedName;

            if (argumentNamePtr == ZR_NULL || *argumentNamePtr == ZR_NULL) {
                continue;
            }

            if (index > 0 && !append_text_fragment_local(buffer, sizeof(buffer), &offset, ", ")) {
                ZrCore_Array_Free(state, &argumentNames);
                return sourceTypeName;
            }

            substitutedName = substitute_generic_type_name_from_bindings(state, *argumentNamePtr, bindings);
            if (substitutedName == ZR_NULL ||
                !append_text_fragment_local(buffer,
                                            sizeof(buffer),
                                            &offset,
                                            ZrCore_String_GetNativeString(substitutedName))) {
                ZrCore_Array_Free(state, &argumentNames);
                return sourceTypeName;
            }
        }

        if (append_text_fragment_local(buffer, sizeof(buffer), &offset, ">")) {
            resultName = ZrCore_String_Create(state, buffer, offset);
        }

        ZrCore_Array_Free(state, &argumentNames);
        if (resultName != ZR_NULL) {
            return resultName;
        }
    }

    return sourceTypeName;
}

static TZrBool infer_substituted_generic_type_name(SZrCompilerState *cs,
                                                   SZrString *sourceTypeName,
                                                   const SZrArray *bindings,
                                                   SZrInferredType *outType) {
    SZrString *substitutedTypeName;

    if (cs == ZR_NULL || sourceTypeName == ZR_NULL || bindings == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    substitutedTypeName = substitute_generic_type_name_from_bindings(cs->state, sourceTypeName, bindings);
    return substitutedTypeName != ZR_NULL && inferred_type_from_type_name(cs, substitutedTypeName, outType);
}

static TZrInt32 find_generic_binding_index(const SZrArray *bindings, SZrString *typeName) {
    if (bindings == ZR_NULL || typeName == ZR_NULL) {
        return ZR_PARSER_I32_NONE;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get((SZrArray *)bindings, index);
        if (binding != ZR_NULL &&
            binding->parameterInfo != ZR_NULL &&
            binding->parameterInfo->name != ZR_NULL &&
            ZrCore_String_Equal(binding->parameterInfo->name, typeName)) {
            return (TZrInt32)index;
        }
    }

    return ZR_PARSER_I32_NONE;
}

static const TZrChar *generic_callable_name_text(SZrString *name) {
    if (name == ZR_NULL) {
        return "<unknown>";
    }

    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(name);
    }

    return ZrCore_String_GetNativeString(name);
}

static void format_generic_call_diagnostic(TZrChar *diagnosticBuffer,
                                           TZrSize diagnosticBufferSize,
                                           const TZrChar *callableKind,
                                           SZrString *callableName,
                                           const TZrChar *detail) {
    if (diagnosticBuffer == ZR_NULL || diagnosticBufferSize == 0 || callableKind == ZR_NULL || detail == ZR_NULL) {
        return;
    }

    snprintf(diagnosticBuffer,
             diagnosticBufferSize,
             "Generic %s '%s': %s",
             callableKind,
             generic_callable_name_text(callableName),
             detail);
}

static void finalize_generic_call_diagnostic(TZrChar *diagnosticBuffer,
                                             TZrSize diagnosticBufferSize,
                                             const TZrChar *callableKind,
                                             SZrString *callableName,
                                             const TZrChar *fallbackDetail) {
    TZrChar detailBuffer[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (diagnosticBuffer == ZR_NULL || diagnosticBufferSize == 0 || callableKind == ZR_NULL) {
        return;
    }

    if (diagnosticBuffer[0] != '\0') {
        snprintf(detailBuffer, sizeof(detailBuffer), "%s", diagnosticBuffer);
    } else if (fallbackDetail != ZR_NULL) {
        snprintf(detailBuffer, sizeof(detailBuffer), "%s", fallbackDetail);
    } else {
        snprintf(detailBuffer, sizeof(detailBuffer), "unable to resolve from provided arguments");
    }

    format_generic_call_diagnostic(diagnosticBuffer,
                                   diagnosticBufferSize,
                                   callableKind,
                                   callableName,
                                   detailBuffer);
}

static TZrBool explicit_const_generic_argument_to_type(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrInferredType *result) {
    SZrTypeValue evaluatedValue;
    TZrChar integerBuffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_Compiler_EvaluateCompileTimeExpression(cs, node, &evaluatedValue)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(evaluatedValue.type)) {
        snprintf(integerBuffer,
                 sizeof(integerBuffer),
                 "%lld",
                 (long long)evaluatedValue.value.nativeObject.nativeInt64);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(evaluatedValue.type)) {
        snprintf(integerBuffer,
                 sizeof(integerBuffer),
                 "%llu",
                 (unsigned long long)evaluatedValue.value.nativeObject.nativeUInt64);
    } else {
        return ZR_FALSE;
    }

    ZrParser_InferredType_InitFull(cs->state,
                                   result,
                                   ZR_VALUE_TYPE_OBJECT,
                                   ZR_FALSE,
                                   ZrCore_String_CreateFromNative(cs->state, integerBuffer));
    return ZR_TRUE;
}

static TZrBool generic_arguments_match_shape(SZrState *state,
                                             const SZrInferredType *expectedType,
                                             const SZrInferredType *actualType) {
    SZrString *expectedBaseName = ZR_NULL;
    SZrString *actualBaseName = ZR_NULL;
    SZrArray expectedArgumentNames;
    SZrArray actualArgumentNames;
    TZrBool matched = ZR_FALSE;

    if (state == ZR_NULL || expectedType == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (expectedType->baseType != actualType->baseType) {
        return ZR_FALSE;
    }

    if (expectedType->elementTypes.length != actualType->elementTypes.length) {
        return ZR_FALSE;
    }

    if (expectedType->typeName == ZR_NULL || actualType->typeName == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Array_Construct(&expectedArgumentNames);
    ZrCore_Array_Construct(&actualArgumentNames);
    if (try_parse_generic_instance_type_name(state, expectedType->typeName, &expectedBaseName, &expectedArgumentNames) &&
        try_parse_generic_instance_type_name(state, actualType->typeName, &actualBaseName, &actualArgumentNames)) {
        matched = expectedBaseName != ZR_NULL &&
                  actualBaseName != ZR_NULL &&
                  ZrCore_String_Equal(expectedBaseName, actualBaseName) &&
                  expectedArgumentNames.length == actualArgumentNames.length;
    } else {
        matched = ZrCore_String_Equal(expectedType->typeName, actualType->typeName);
    }

    if (expectedArgumentNames.isValid && expectedArgumentNames.head != ZR_NULL) {
        ZrCore_Array_Free(state, &expectedArgumentNames);
    }
    if (actualArgumentNames.isValid && actualArgumentNames.head != ZR_NULL) {
        ZrCore_Array_Free(state, &actualArgumentNames);
    }

    return matched;
}

static TZrBool substitute_generic_parameter_type(SZrState *state,
                                                 const SZrArray *bindings,
                                                 const SZrInferredType *sourceType,
                                                 SZrInferredType *result) {
    TZrInt32 bindingIndex;

    if (state == ZR_NULL || bindings == ZR_NULL || sourceType == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingIndex = find_generic_binding_index(bindings, sourceType->typeName);
    if (bindingIndex != ZR_PARSER_I32_NONE && sourceType->elementTypes.length == 0) {
        SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get((SZrArray *)bindings,
                                                                                   (TZrSize)bindingIndex);
        if (binding == ZR_NULL || !binding->isBound) {
            return ZR_FALSE;
        }
        ZrParser_InferredType_Copy(state, result, &binding->inferredType);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(state, result, sourceType->baseType);
    result->isNullable = sourceType->isNullable;
    result->ownershipQualifier = sourceType->ownershipQualifier;
    result->typeName = sourceType->typeName;
    result->minValue = sourceType->minValue;
    result->maxValue = sourceType->maxValue;
    result->hasRangeConstraint = sourceType->hasRangeConstraint;
    result->arrayFixedSize = sourceType->arrayFixedSize;
    result->arrayMinSize = sourceType->arrayMinSize;
    result->arrayMaxSize = sourceType->arrayMaxSize;
    result->hasArraySizeConstraint = sourceType->hasArraySizeConstraint;

    if (sourceType->elementTypes.length > 0) {
        SZrString *baseName = ZR_NULL;
        SZrArray originalArgumentNames;

        ZrCore_Array_Init(state, &result->elementTypes, sizeof(SZrInferredType), sourceType->elementTypes.length);
        for (TZrSize index = 0; index < sourceType->elementTypes.length; index++) {
            SZrInferredType *sourceElement = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&sourceType->elementTypes,
                                                                                 index);
            SZrInferredType resolvedElement;

            if (sourceElement == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &resolvedElement, ZR_VALUE_TYPE_OBJECT);
            if (!substitute_generic_parameter_type(state, bindings, sourceElement, &resolvedElement)) {
                ZrParser_InferredType_Free(state, &resolvedElement);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(state, &result->elementTypes, &resolvedElement);
        }

        ZrCore_Array_Construct(&originalArgumentNames);
        if (sourceType->typeName != ZR_NULL &&
            try_parse_generic_instance_type_name(state, sourceType->typeName, &baseName, &originalArgumentNames) &&
            baseName != ZR_NULL &&
            result->elementTypes.length == originalArgumentNames.length) {
            SZrString *canonicalName = build_generic_instance_name(state, baseName, &result->elementTypes);
            if (canonicalName != ZR_NULL) {
                result->typeName = canonicalName;
            }
        }
        if (originalArgumentNames.isValid && originalArgumentNames.head != ZR_NULL) {
            ZrCore_Array_Free(state, &originalArgumentNames);
        }
    }

    return ZR_TRUE;
}

static TZrBool unify_generic_binding(SZrState *state,
                                     SZrArray *bindings,
                                     const SZrInferredType *expectedType,
                                     const SZrInferredType *actualType,
                                     TZrChar *diagnosticBuffer,
                                     TZrSize diagnosticBufferSize) {
    TZrInt32 bindingIndex;

    if (state == ZR_NULL || bindings == ZR_NULL || expectedType == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingIndex = find_generic_binding_index(bindings, expectedType->typeName);
    if (bindingIndex != ZR_PARSER_I32_NONE && expectedType->elementTypes.length == 0) {
        SZrGenericCallBinding *binding =
                (SZrGenericCallBinding *)ZrCore_Array_Get(bindings, (TZrSize)bindingIndex);
        if (binding == ZR_NULL) {
            return ZR_FALSE;
        }

        if (!binding->isBound) {
            binding->isBound = ZR_TRUE;
            ZrParser_InferredType_Copy(state, &binding->inferredType, actualType);
            return ZR_TRUE;
        }

        if (!ZrParser_InferredType_Equal(&binding->inferredType, actualType)) {
            if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0 && binding->parameterInfo != ZR_NULL &&
                binding->parameterInfo->name != ZR_NULL) {
                TZrChar boundTypeBuffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
                TZrChar actualTypeBuffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
                const TZrChar *boundTypeText =
                        ZrParser_TypeNameString_Get(state,
                                                    &binding->inferredType,
                                                    boundTypeBuffer,
                                                    sizeof(boundTypeBuffer));
                const TZrChar *actualTypeText =
                        ZrParser_TypeNameString_Get(state,
                                                    actualType,
                                                    actualTypeBuffer,
                                                    sizeof(actualTypeBuffer));
                snprintf(diagnosticBuffer,
                         diagnosticBufferSize,
                         "conflicting inferences for generic argument '%s' (bound=%s base=%d owner=%d, actual=%s base=%d owner=%d)",
                         ZrCore_String_GetNativeString(binding->parameterInfo->name),
                         boundTypeText != ZR_NULL && boundTypeText[0] != '\0' ? boundTypeText : "<unknown>",
                         (int)binding->inferredType.baseType,
                         (int)binding->inferredType.ownershipQualifier,
                         actualTypeText != ZR_NULL && actualTypeText[0] != '\0' ? actualTypeText : "<unknown>",
                         (int)actualType->baseType,
                         (int)actualType->ownershipQualifier);
            }
            return ZR_FALSE;
        }

        return ZR_TRUE;
    }

    if (!generic_arguments_match_shape(state, expectedType, actualType)) {
        return ZR_FALSE;
    }

    if (expectedType->elementTypes.length > 0) {
        for (TZrSize index = 0; index < expectedType->elementTypes.length; index++) {
            SZrInferredType *expectedElement =
                    (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&expectedType->elementTypes, index);
            SZrInferredType *actualElement =
                    (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&actualType->elementTypes, index);
            if (expectedElement == ZR_NULL || actualElement == ZR_NULL) {
                return ZR_FALSE;
            }
            if (!unify_generic_binding(state,
                                       bindings,
                                       expectedElement,
                                       actualElement,
                                       diagnosticBuffer,
                                       diagnosticBufferSize)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool infer_call_argument_type_node_for_generic(SZrCompilerState *cs,
                                                         SZrAstNode *argNode,
                                                         SZrArray *argTypes) {
    SZrInferredType argType;
    SZrInferredType normalizedType;

    if (cs == ZR_NULL || argNode == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &argType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, argNode, &argType)) {
        ZrParser_InferredType_Free(cs->state, &argType);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &normalizedType, ZR_VALUE_TYPE_OBJECT);
    if (argType.typeName != ZR_NULL &&
        argType.elementTypes.length == 0 &&
        inferred_type_from_type_name(cs, argType.typeName, &normalizedType) &&
        normalizedType.elementTypes.length > 0) {
        normalizedType.isNullable = argType.isNullable;
        normalizedType.ownershipQualifier = argType.ownershipQualifier;
        ZrParser_InferredType_Free(cs->state, &argType);
        ZrParser_InferredType_Copy(cs->state, &argType, &normalizedType);
    }
    ZrParser_InferredType_Free(cs->state, &normalizedType);

    ZrCore_Array_Push(cs->state, argTypes, &argType);
    return ZR_TRUE;
}

static SZrAstNodeArray *member_declaration_param_list(SZrAstNode *declarationNode) {
    if (declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_METHOD:
            return declarationNode->data.classMethod.params;
        case ZR_AST_STRUCT_METHOD:
            return declarationNode->data.structMethod.params;
        case ZR_AST_CLASS_META_FUNCTION:
            return declarationNode->data.classMetaFunction.params;
        case ZR_AST_STRUCT_META_FUNCTION:
            return declarationNode->data.structMetaFunction.params;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return declarationNode->data.interfaceMethodSignature.params;
        case ZR_AST_INTERFACE_META_SIGNATURE:
            return declarationNode->data.interfaceMetaSignature.params;
        default:
            return ZR_NULL;
    }
}

static SZrType *member_declaration_return_type(SZrAstNode *declarationNode) {
    if (declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_METHOD:
            return declarationNode->data.classMethod.returnType;
        case ZR_AST_STRUCT_METHOD:
            return declarationNode->data.structMethod.returnType;
        case ZR_AST_CLASS_META_FUNCTION:
            return declarationNode->data.classMetaFunction.returnType;
        case ZR_AST_STRUCT_META_FUNCTION:
            return declarationNode->data.structMetaFunction.returnType;
        default:
            return ZR_NULL;
    }
}

static EZrGenericCallResolveStatus build_substituted_member_signature_from_declaration(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        const SZrArray *bindings,
        SZrResolvedCallSignature *signature) {
    SZrAstNodeArray *params;
    SZrType *returnTypeNode;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || bindings == ZR_NULL || signature == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    params = member_declaration_param_list(memberInfo->declarationNode);
    returnTypeNode = member_declaration_return_type(memberInfo->declarationNode);
    if (params == ZR_NULL && returnTypeNode == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    resolved_call_signature_init(cs->state, signature);
    if (!copy_parameter_passing_modes(cs->state,
                                      &signature->parameterPassingModes,
                                      &memberInfo->parameterPassingModes)) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (params != ZR_NULL && params->count > 0) {
        ZrCore_Array_Init(cs->state, &signature->parameterTypes, sizeof(SZrInferredType), params->count);
        for (TZrSize index = 0; index < params->count; index++) {
            SZrAstNode *paramNode = params->nodes[index];
            SZrInferredType *memberParameterType =
                    index < memberInfo->parameterTypes.length
                            ? (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index)
                            : ZR_NULL;
            TZrBool hasUnresolvedType = ZR_FALSE;
            SZrInferredType unresolvedType;
            SZrInferredType resolvedType;

            ZrParser_InferredType_Init(cs->state, &unresolvedType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Init(cs->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);

            if (memberParameterType != ZR_NULL) {
                ZrParser_InferredType_Copy(cs->state, &unresolvedType, memberParameterType);
                hasUnresolvedType = ZR_TRUE;
            } else if (paramNode != ZR_NULL &&
                       paramNode->type == ZR_AST_PARAMETER &&
                       paramNode->data.parameter.typeInfo != ZR_NULL &&
                       ZrParser_AstTypeToInferredType_Convert(cs,
                                                               paramNode->data.parameter.typeInfo,
                                                               &unresolvedType)) {
                hasUnresolvedType = ZR_TRUE;
            }

            if (!hasUnresolvedType ||
                !substitute_generic_parameter_type(cs->state, bindings, &unresolvedType, &resolvedType)) {
                ZrParser_InferredType_Free(cs->state, &unresolvedType);
                ZrParser_InferredType_Free(cs->state, &resolvedType);
                return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
            }

            ZrParser_InferredType_Free(cs->state, &unresolvedType);
            ZrCore_Array_Push(cs->state, &signature->parameterTypes, &resolvedType);
        }
    }

    if (returnTypeNode != ZR_NULL) {
        SZrInferredType unresolvedReturnType;

        if (memberInfo->returnTypeName != ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, &signature->returnType, ZR_VALUE_TYPE_OBJECT);
            if (!infer_substituted_generic_type_name(cs,
                                                     memberInfo->returnTypeName,
                                                     bindings,
                                                     &signature->returnType)) {
                ZrParser_InferredType_Free(cs->state, &signature->returnType);
                return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
            }
            return ZR_GENERIC_CALL_RESOLVE_OK;
        }

        ZrParser_InferredType_Init(cs->state, &unresolvedReturnType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(cs, returnTypeNode, &unresolvedReturnType) ||
            !substitute_generic_parameter_type(cs->state, bindings, &unresolvedReturnType, &signature->returnType)) {
            ZrParser_InferredType_Free(cs->state, &unresolvedReturnType);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        ZrParser_InferredType_Free(cs->state, &unresolvedReturnType);
        return ZR_GENERIC_CALL_RESOLVE_OK;
    }

    if (memberInfo->returnTypeName == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &signature->returnType, ZR_VALUE_TYPE_NULL);
        return ZR_GENERIC_CALL_RESOLVE_OK;
    }

    return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
}

static TZrBool infer_call_argument_types_against_param_list(SZrCompilerState *cs,
                                                            SZrAstNodeArray *params,
                                                            SZrFunctionCall *call,
                                                            SZrArray *argTypes,
                                                            TZrBool *mismatch) {
    TZrSize paramCount;
    TZrSize argCount;

    if (mismatch != ZR_NULL) {
        *mismatch = ZR_FALSE;
    }

    if (cs == ZR_NULL || argTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(argTypes);
    paramCount = params != ZR_NULL ? params->count : 0;
    argCount = call != ZR_NULL && call->args != ZR_NULL ? call->args->count : 0;

    if (paramCount == 0) {
        if (argCount > 0 && mismatch != ZR_NULL) {
            *mismatch = ZR_TRUE;
        }
        return ZR_TRUE;
    }

    if (argCount != paramCount) {
        if (mismatch != ZR_NULL) {
            *mismatch = ZR_TRUE;
        }
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, argTypes, sizeof(SZrInferredType), argCount);
    for (TZrSize index = 0; index < argCount; index++) {
        if (!infer_call_argument_type_node_for_generic(cs, call->args->nodes[index], argTypes)) {
            free_inferred_type_array(cs->state, argTypes);
            ZrCore_Array_Construct(argTypes);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static EZrGenericCallResolveStatus resolve_explicit_generic_argument_bindings(
        SZrCompilerState *cs,
        const SZrArray *genericParameters,
        SZrFunctionCall *call,
        SZrArray *bindings,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize) {
    TZrSize explicitCount;

    if (cs == ZR_NULL || genericParameters == ZR_NULL || bindings == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    explicitCount = call != ZR_NULL && call->genericArguments != ZR_NULL ? call->genericArguments->count : 0;
    if (explicitCount == 0) {
        return ZR_GENERIC_CALL_RESOLVE_OK;
    }

    if (explicitCount != genericParameters->length) {
        if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0) {
            snprintf(diagnosticBuffer,
                     diagnosticBufferSize,
                     "expected %zu generic argument(s), got %zu",
                     genericParameters->length,
                     explicitCount);
        }
        return ZR_GENERIC_CALL_RESOLVE_ARITY_MISMATCH;
    }

    for (TZrSize index = 0; index < explicitCount; index++) {
        SZrAstNode *argumentNode = call->genericArguments->nodes[index];
        SZrTypeGenericParameterInfo *parameterInfo =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)genericParameters, index);
        SZrGenericCallBinding *binding =
                (SZrGenericCallBinding *)ZrCore_Array_Get(bindings, index);

        if (parameterInfo == ZR_NULL || binding == ZR_NULL) {
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        if (parameterInfo->genericKind == ZR_GENERIC_PARAMETER_TYPE) {
            if (argumentNode == ZR_NULL || argumentNode->type != ZR_AST_TYPE) {
                if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0 && parameterInfo->name != ZR_NULL) {
                    snprintf(diagnosticBuffer,
                             diagnosticBufferSize,
                             "generic argument '%s' expects a type argument",
                             ZrCore_String_GetNativeString(parameterInfo->name));
                }
                return ZR_GENERIC_CALL_RESOLVE_KIND_MISMATCH;
            }

            binding->isBound = ZR_TRUE;
            ZrParser_InferredType_Free(cs->state, &binding->inferredType);
            ZrParser_InferredType_Init(cs->state, &binding->inferredType, ZR_VALUE_TYPE_OBJECT);
            if (!ZrParser_AstTypeToInferredType_Convert(cs, &argumentNode->data.type, &binding->inferredType)) {
                return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
            }
            continue;
        }

        if (argumentNode == ZR_NULL || argumentNode->type == ZR_AST_TYPE) {
            if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0 && parameterInfo->name != ZR_NULL) {
                snprintf(diagnosticBuffer,
                         diagnosticBufferSize,
                         "generic argument '%s' expects a compile-time integer",
                         ZrCore_String_GetNativeString(parameterInfo->name));
            }
            return ZR_GENERIC_CALL_RESOLVE_KIND_MISMATCH;
        }

        binding->isBound = ZR_TRUE;
        ZrParser_InferredType_Free(cs->state, &binding->inferredType);
        ZrParser_InferredType_Init(cs->state, &binding->inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!explicit_const_generic_argument_to_type(cs, argumentNode, &binding->inferredType)) {
            if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0 && parameterInfo->name != ZR_NULL) {
                snprintf(diagnosticBuffer,
                         diagnosticBufferSize,
                         "generic argument '%s' expects a compile-time integer",
                         ZrCore_String_GetNativeString(parameterInfo->name));
            }
            return ZR_GENERIC_CALL_RESOLVE_KIND_MISMATCH;
        }
    }

    return ZR_GENERIC_CALL_RESOLVE_OK;
}

static EZrGenericCallResolveStatus build_substituted_function_signature(
        SZrState *state,
        const SZrFunctionTypeInfo *funcType,
        const SZrArray *bindings,
        SZrResolvedCallSignature *signature) {
    if (state == ZR_NULL || funcType == ZR_NULL || bindings == ZR_NULL || signature == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    resolved_call_signature_init(state, signature);
    if (!substitute_generic_parameter_type(state, bindings, &funcType->returnType, &signature->returnType) ||
        !copy_parameter_passing_modes(state, &signature->parameterPassingModes, &funcType->parameterPassingModes)) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (funcType->paramTypes.length > 0) {
        ZrCore_Array_Init(state, &signature->parameterTypes, sizeof(SZrInferredType), funcType->paramTypes.length);
        for (TZrSize index = 0; index < funcType->paramTypes.length; index++) {
            SZrInferredType *sourceType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&funcType->paramTypes, index);
            SZrInferredType resolvedType;

            if (sourceType == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            if (!substitute_generic_parameter_type(state, bindings, sourceType, &resolvedType)) {
                ZrParser_InferredType_Free(state, &resolvedType);
                return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
            }
            ZrCore_Array_Push(state, &signature->parameterTypes, &resolvedType);
        }
    }

    return ZR_GENERIC_CALL_RESOLVE_OK;
}

static EZrGenericCallResolveStatus build_substituted_member_signature(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        const SZrArray *bindings,
        SZrResolvedCallSignature *signature) {
    EZrGenericCallResolveStatus astStatus;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || bindings == ZR_NULL || signature == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    astStatus = build_substituted_member_signature_from_declaration(cs, memberInfo, bindings, signature);
    if (astStatus == ZR_GENERIC_CALL_RESOLVE_OK) {
        return ZR_GENERIC_CALL_RESOLVE_OK;
    }

    resolved_call_signature_init(cs->state, signature);
    if (!copy_parameter_passing_modes(cs->state,
                                      &signature->parameterPassingModes,
                                      &memberInfo->parameterPassingModes)) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (memberInfo->parameterTypes.length > 0) {
        ZrCore_Array_Init(cs->state,
                          &signature->parameterTypes,
                          sizeof(SZrInferredType),
                          memberInfo->parameterTypes.length);
        for (TZrSize index = 0; index < memberInfo->parameterTypes.length; index++) {
            SZrInferredType *sourceType =
                    (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index);
            SZrInferredType resolvedType;

            if (sourceType == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(cs->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            if (!substitute_generic_parameter_type(cs->state, bindings, sourceType, &resolvedType)) {
                ZrParser_InferredType_Free(cs->state, &resolvedType);
                return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
            }
            ZrCore_Array_Push(cs->state, &signature->parameterTypes, &resolvedType);
        }
    }

    if (memberInfo->returnTypeName == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, &signature->returnType, ZR_VALUE_TYPE_NULL);
        return ZR_GENERIC_CALL_RESOLVE_OK;
    }

    ZrParser_InferredType_Init(cs->state, &signature->returnType, ZR_VALUE_TYPE_OBJECT);
    if (!infer_substituted_generic_type_name(cs, memberInfo->returnTypeName, bindings, &signature->returnType)) {
        ZrParser_InferredType_Free(cs->state, &signature->returnType);
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    return ZR_GENERIC_CALL_RESOLVE_OK;
}

static EZrGenericCallResolveStatus resolve_generic_bindings_from_arguments(
        SZrState *state,
        const SZrArray *parameterTypes,
        const SZrArray *argumentTypes,
        SZrArray *bindings,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize) {
    if (state == ZR_NULL || parameterTypes == ZR_NULL || argumentTypes == ZR_NULL || bindings == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (parameterTypes->length != argumentTypes->length) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    for (TZrSize index = 0; index < parameterTypes->length; index++) {
        SZrInferredType *parameterType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)parameterTypes, index);
        SZrInferredType *argumentType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)argumentTypes, index);

        if (parameterType == ZR_NULL || argumentType == ZR_NULL) {
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }

        if (!unify_generic_binding(state,
                                   bindings,
                                   parameterType,
                                   argumentType,
                                   diagnosticBuffer,
                                   diagnosticBufferSize)) {
            return (diagnosticBuffer != ZR_NULL && diagnosticBuffer[0] != '\0')
                           ? ZR_GENERIC_CALL_RESOLVE_CONFLICTING_INFERENCE
                           : ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }
    }

    return ZR_GENERIC_CALL_RESOLVE_OK;
}

static EZrGenericCallResolveStatus ensure_all_generic_bindings_resolved(
        const SZrArray *bindings,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize) {
    if (bindings == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get((SZrArray *)bindings, index);
        if (binding != ZR_NULL && !binding->isBound) {
            if (diagnosticBuffer != ZR_NULL &&
                diagnosticBufferSize > 0 &&
                binding->parameterInfo != ZR_NULL &&
                binding->parameterInfo->name != ZR_NULL) {
                snprintf(diagnosticBuffer,
                         diagnosticBufferSize,
                         "cannot infer generic argument '%s'",
                         ZrCore_String_GetNativeString(binding->parameterInfo->name));
            }
            return ZR_GENERIC_CALL_RESOLVE_CANNOT_INFER;
        }
    }

    return ZR_GENERIC_CALL_RESOLVE_OK;
}

static TZrBool initialize_generic_call_bindings(SZrCompilerState *cs,
                                                const SZrArray *genericParameters,
                                                SZrArray *bindings) {
    if (cs == ZR_NULL || genericParameters == ZR_NULL || bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(bindings);
    if (genericParameters->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(cs->state, bindings, sizeof(SZrGenericCallBinding), genericParameters->length);
    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *parameterInfo =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)genericParameters, index);
        SZrGenericCallBinding binding;

        if (parameterInfo == ZR_NULL) {
            continue;
        }

        memset(&binding, 0, sizeof(binding));
        binding.parameterInfo = parameterInfo;
        binding.isBound = ZR_FALSE;
        ZrParser_InferredType_Init(cs->state, &binding.inferredType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(cs->state, bindings, &binding);
    }

    return ZR_TRUE;
}

static void free_generic_call_bindings(SZrState *state, SZrArray *bindings) {
    if (state == ZR_NULL || bindings == ZR_NULL) {
        return;
    }

    if (bindings->isValid && bindings->head != ZR_NULL) {
        for (TZrSize index = 0; index < bindings->length; index++) {
            SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get(bindings, index);
            if (binding != ZR_NULL) {
                ZrParser_InferredType_Free(state, &binding->inferredType);
            }
        }
        ZrCore_Array_Free(state, bindings);
    }
}

ZR_PARSER_API EZrGenericCallResolveStatus resolve_generic_constructed_type_name(
        SZrCompilerState *cs,
        SZrString *baseTypeName,
        const SZrArray *genericParameters,
        const SZrArray *parameterTypes,
        SZrAstNodeArray *arguments,
        SZrString **outTypeName,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize) {
    SZrArray bindings;
    SZrArray argumentTypes;
    SZrArray resolvedTypeArguments;
    EZrGenericCallResolveStatus status = ZR_GENERIC_CALL_RESOLVE_CONFLICT;

    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }
    if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0) {
        diagnosticBuffer[0] = '\0';
    }

    if (cs == ZR_NULL || baseTypeName == ZR_NULL || genericParameters == ZR_NULL || parameterTypes == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (genericParameters->length == 0) {
        if (outTypeName != ZR_NULL) {
            *outTypeName = baseTypeName;
        }
        return ZR_GENERIC_CALL_RESOLVE_OK;
    }

    if (!initialize_generic_call_bindings(cs, genericParameters, &bindings)) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    ZrCore_Array_Construct(&argumentTypes);
    if (arguments != ZR_NULL && arguments->count > 0) {
        ZrCore_Array_Init(cs->state, &argumentTypes, sizeof(SZrInferredType), arguments->count);
        for (TZrSize index = 0; index < arguments->count; index++) {
            if (!infer_call_argument_type_node_for_generic(cs, arguments->nodes[index], &argumentTypes)) {
                free_inferred_type_array(cs->state, &argumentTypes);
                free_generic_call_bindings(cs->state, &bindings);
                return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
            }
        }
    }

    status = resolve_generic_bindings_from_arguments(cs->state,
                                                     parameterTypes,
                                                     &argumentTypes,
                                                     &bindings,
                                                     diagnosticBuffer,
                                                     diagnosticBufferSize);
    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = ensure_all_generic_bindings_resolved(&bindings, diagnosticBuffer, diagnosticBufferSize);
    }
    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = validate_generic_call_bindings_constraints(cs, &bindings, diagnosticBuffer, diagnosticBufferSize);
    }

    if (status == ZR_GENERIC_CALL_RESOLVE_OK && outTypeName != ZR_NULL) {
        ZrCore_Array_Construct(&resolvedTypeArguments);
        ZrCore_Array_Init(cs->state, &resolvedTypeArguments, sizeof(SZrInferredType), bindings.length);
        for (TZrSize index = 0; index < bindings.length; index++) {
            SZrGenericCallBinding *binding = (SZrGenericCallBinding *)ZrCore_Array_Get(&bindings, index);
            if (binding == ZR_NULL || !binding->isBound) {
                continue;
            }
            ZrCore_Array_Push(cs->state, &resolvedTypeArguments, &binding->inferredType);
        }
        *outTypeName = build_generic_instance_name(cs->state, baseTypeName, &resolvedTypeArguments);
        free_inferred_type_array(cs->state, &resolvedTypeArguments);
        if (*outTypeName == ZR_NULL) {
            if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0) {
                snprintf(diagnosticBuffer,
                         diagnosticBufferSize,
                         "unable to materialize generic type '%s' from constructor arguments",
                         generic_callable_name_text(baseTypeName));
            }
            status = ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }
    }

    free_inferred_type_array(cs->state, &argumentTypes);
    free_generic_call_bindings(cs->state, &bindings);
    return status;
}

EZrGenericCallResolveStatus resolve_generic_function_call_signature_detailed(
        SZrCompilerState *cs,
        SZrTypeEnvironment *env,
        SZrString *funcName,
        const SZrFunctionTypeInfo *funcType,
        SZrFunctionCall *call,
        SZrResolvedCallSignature *signature,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize) {
    SZrArray bindings;
    SZrArray argumentTypes;
    TZrBool mismatch = ZR_FALSE;
    EZrGenericCallResolveStatus status;

    if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0) {
        diagnosticBuffer[0] = '\0';
    }

    if (cs == ZR_NULL || funcType == ZR_NULL || signature == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (!initialize_generic_call_bindings(cs, &funcType->genericParameters, &bindings)) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (funcType->genericParameters.length == 0) {
        free_generic_call_bindings(cs->state, &bindings);
        return copy_direct_function_signature(cs->state, funcType, signature)
                       ? ZR_GENERIC_CALL_RESOLVE_OK
                       : ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

        status = resolve_explicit_generic_argument_bindings(cs,
                                                            &funcType->genericParameters,
                                                            call,
                                                            &bindings,
                                                            diagnosticBuffer,
                                                            diagnosticBufferSize);
    if (status != ZR_GENERIC_CALL_RESOLVE_OK) {
        finalize_generic_call_diagnostic(diagnosticBuffer,
                                         diagnosticBufferSize,
                                         "function",
                                         funcName,
                                         ZR_NULL);
        free_generic_call_bindings(cs->state, &bindings);
        return status;
    }

    ZrCore_Array_Construct(&argumentTypes);
    if (!infer_function_call_argument_types_for_candidate(cs,
                                                          env,
                                                          funcName,
                                                          call,
                                                          funcType,
                                                          &argumentTypes,
                                                          &mismatch)) {
        free_generic_call_bindings(cs->state, &bindings);
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (!mismatch) {
        status = resolve_generic_bindings_from_arguments(cs->state,
                                                         &funcType->paramTypes,
                                                         &argumentTypes,
                                                         &bindings,
                                                         diagnosticBuffer,
                                                         diagnosticBufferSize);
    } else {
        status = ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = ensure_all_generic_bindings_resolved(&bindings, diagnosticBuffer, diagnosticBufferSize);
    }

    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = validate_generic_call_bindings_constraints(cs, &bindings, diagnosticBuffer, diagnosticBufferSize);
    }

    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = build_substituted_function_signature(cs->state, funcType, &bindings, signature);
    }

    if (status != ZR_GENERIC_CALL_RESOLVE_OK) {
        finalize_generic_call_diagnostic(diagnosticBuffer,
                                         diagnosticBufferSize,
                                         "function",
                                         funcName,
                                         ZR_NULL);
    }

    free_inferred_type_array(cs->state, &argumentTypes);
    free_generic_call_bindings(cs->state, &bindings);
    return status;
}

TZrBool resolve_generic_function_call_signature(SZrCompilerState *cs,
                                                const SZrFunctionTypeInfo *funcType,
                                                SZrFunctionCall *call,
                                                SZrResolvedCallSignature *signature) {
    return resolve_generic_function_call_signature_detailed(cs,
                                                            cs != ZR_NULL ? cs->typeEnv : ZR_NULL,
                                                            funcType != ZR_NULL ? funcType->name : ZR_NULL,
                                                            funcType,
                                                            call,
                                                            signature,
                                                            ZR_NULL,
                                                            0) == ZR_GENERIC_CALL_RESOLVE_OK;
}

EZrGenericCallResolveStatus resolve_generic_member_call_signature_detailed(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        SZrFunctionCall *call,
        SZrResolvedCallSignature *signature,
        TZrChar *diagnosticBuffer,
        TZrSize diagnosticBufferSize) {
    SZrArray bindings;
    SZrArray argumentTypes;
    TZrBool mismatch = ZR_FALSE;
    SZrAstNodeArray *params;
    EZrGenericCallResolveStatus status;

    if (diagnosticBuffer != ZR_NULL && diagnosticBufferSize > 0) {
        diagnosticBuffer[0] = '\0';
    }

    if (cs == ZR_NULL || memberInfo == ZR_NULL || signature == ZR_NULL) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (!initialize_generic_call_bindings(cs, &memberInfo->genericParameters, &bindings)) {
        return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (memberInfo->genericParameters.length == 0) {
        free_generic_call_bindings(cs->state, &bindings);
        return copy_direct_member_signature(cs, memberInfo, signature)
                       ? ZR_GENERIC_CALL_RESOLVE_OK
                       : ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    status = resolve_explicit_generic_argument_bindings(cs,
                                                        &memberInfo->genericParameters,
                                                        call,
                                                        &bindings,
                                                        diagnosticBuffer,
                                                        diagnosticBufferSize);
    if (status != ZR_GENERIC_CALL_RESOLVE_OK) {
        finalize_generic_call_diagnostic(diagnosticBuffer,
                                         diagnosticBufferSize,
                                         "method",
                                         memberInfo->name,
                                         ZR_NULL);
        free_generic_call_bindings(cs->state, &bindings);
        return status;
    }

    params = member_declaration_param_list(memberInfo->declarationNode);
    ZrCore_Array_Construct(&argumentTypes);
    if (params != ZR_NULL) {
        if (!infer_call_argument_types_against_param_list(cs, params, call, &argumentTypes, &mismatch)) {
            free_generic_call_bindings(cs->state, &bindings);
            return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
        }
    } else {
        TZrSize expectedCount = memberInfo->parameterTypes.length;
        TZrSize argCount = call != ZR_NULL && call->args != ZR_NULL ? call->args->count : 0;

        if (expectedCount != argCount) {
            mismatch = ZR_TRUE;
        } else if (argCount > 0) {
            ZrCore_Array_Init(cs->state, &argumentTypes, sizeof(SZrInferredType), argCount);
            for (TZrSize index = 0; index < argCount; index++) {
                if (!infer_call_argument_type_node_for_generic(cs, call->args->nodes[index], &argumentTypes)) {
                    free_inferred_type_array(cs->state, &argumentTypes);
                    free_generic_call_bindings(cs->state, &bindings);
                    return ZR_GENERIC_CALL_RESOLVE_CONFLICT;
                }
            }
        }
    }

    if (!mismatch) {
        status = resolve_generic_bindings_from_arguments(cs->state,
                                                         &memberInfo->parameterTypes,
                                                         &argumentTypes,
                                                         &bindings,
                                                         diagnosticBuffer,
                                                         diagnosticBufferSize);
    } else {
        status = ZR_GENERIC_CALL_RESOLVE_CONFLICT;
    }

    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = ensure_all_generic_bindings_resolved(&bindings, diagnosticBuffer, diagnosticBufferSize);
    }

    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = validate_generic_call_bindings_constraints(cs, &bindings, diagnosticBuffer, diagnosticBufferSize);
    }

    if (status == ZR_GENERIC_CALL_RESOLVE_OK) {
        status = build_substituted_member_signature(cs, memberInfo, &bindings, signature);
    }

    if (status != ZR_GENERIC_CALL_RESOLVE_OK) {
        finalize_generic_call_diagnostic(diagnosticBuffer,
                                         diagnosticBufferSize,
                                         "method",
                                         memberInfo->name,
                                         ZR_NULL);
    }

    free_inferred_type_array(cs->state, &argumentTypes);
    free_generic_call_bindings(cs->state, &bindings);
    return status;
}

TZrBool resolve_generic_member_call_signature(SZrCompilerState *cs,
                                              const SZrTypeMemberInfo *memberInfo,
                                              SZrFunctionCall *call,
                                              SZrResolvedCallSignature *signature) {
    return resolve_generic_member_call_signature_detailed(cs,
                                                          memberInfo,
                                                          call,
                                                          signature,
                                                          ZR_NULL,
                                                          0) == ZR_GENERIC_CALL_RESOLVE_OK;
}
