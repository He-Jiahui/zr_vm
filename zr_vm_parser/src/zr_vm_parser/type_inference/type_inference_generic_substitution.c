#include "type_inference_generic_substitution.h"

#include "compiler_internal.h"
#include "type_inference_generic_prototype.h"
#include "type_inference_internal.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <stdio.h>
#include <string.h>

TZrBool type_inference_append_type_text(TZrChar *buffer,
                                  TZrSize bufferSize,
                                  TZrSize *offset,
                                  const TZrChar *fragment) {
    TZrSize fragmentLength;

    if (buffer == ZR_NULL || offset == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    fragmentLength = strlen(fragment);
    if (*offset + fragmentLength + 1 >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *offset, fragment, fragmentLength);
    *offset += fragmentLength;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

SZrString *substitute_generic_type_name(SZrState *state,
                                               SZrString *sourceTypeName,
                                               const SZrArray *genericParameters,
                                               const SZrArray *argumentTypeNames) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentNames;
    TZrNativeString nativeTypeName;
    TZrSize nativeTypeNameLength;

    if (state == ZR_NULL || sourceTypeName == ZR_NULL || genericParameters == ZR_NULL || argumentTypeNames == ZR_NULL) {
        return sourceTypeName;
    }

    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *parameterInfo =
                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)genericParameters, index);
        SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)argumentTypeNames, index);
        if (parameterInfo == ZR_NULL || parameterInfo->name == ZR_NULL || argumentNamePtr == ZR_NULL || *argumentNamePtr == ZR_NULL) {
            continue;
        }
        if (ZrCore_String_Equal(parameterInfo->name, sourceTypeName)) {
            return *argumentNamePtr;
        }
    }

    nativeTypeName = ZrCore_String_GetNativeString(sourceTypeName);
    nativeTypeNameLength = nativeTypeName != ZR_NULL ? strlen(nativeTypeName) : 0;
    if (nativeTypeName != ZR_NULL &&
        nativeTypeNameLength > 2 &&
        strcmp(nativeTypeName + nativeTypeNameLength - 2, "[]") == 0) {
        SZrString *elementTypeName = ZrCore_String_Create(state, nativeTypeName, nativeTypeNameLength - 2);
        SZrString *substitutedElement = substitute_generic_type_name(state,
                                                                     elementTypeName,
                                                                     genericParameters,
                                                                     argumentTypeNames);
        TZrChar buffer[ZR_PARSER_TEXT_BUFFER_LENGTH];
        TZrNativeString elementNative = substitutedElement != ZR_NULL ? ZrCore_String_GetNativeString(substitutedElement) : ZR_NULL;
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
        if (!type_inference_append_type_text(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(baseName)) ||
            !type_inference_append_type_text(buffer, sizeof(buffer), &offset, "<")) {
            ZrCore_Array_Free(state, &argumentNames);
            return sourceTypeName;
        }
        for (TZrSize index = 0; index < argumentNames.length; index++) {
            SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentNames, index);
            SZrString *substitutedName;
            if (argumentNamePtr == ZR_NULL || *argumentNamePtr == ZR_NULL) {
                continue;
            }
            if (index > 0 && !type_inference_append_type_text(buffer, sizeof(buffer), &offset, ", ")) {
                ZrCore_Array_Free(state, &argumentNames);
                return sourceTypeName;
            }
            substitutedName = substitute_generic_type_name(state, *argumentNamePtr, genericParameters, argumentTypeNames);
            if (substitutedName == ZR_NULL ||
                !type_inference_append_type_text(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(substitutedName))) {
                ZrCore_Array_Free(state, &argumentNames);
                return sourceTypeName;
            }
        }
        if (type_inference_append_type_text(buffer, sizeof(buffer), &offset, ">")) {
            resultName = ZrCore_String_Create(state, buffer, offset);
        }
        ZrCore_Array_Free(state, &argumentNames);
        if (resultName != ZR_NULL) {
            return resultName;
        }
    }

    return sourceTypeName;
}

static TZrSize generic_parameter_index_by_name(
        const SZrArray *genericParameters,
        SZrString *name) {
    TZrSize index;

    if (genericParameters == ZR_NULL || name == ZR_NULL) {
        return ZR_MAX_SIZE;
    }
    for (index = 0; index < genericParameters->length; index++) {
        const SZrTypeGenericParameterInfo *parameter =
                (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                        (SZrArray *)genericParameters,
                        index);
        if (parameter != ZR_NULL &&
            parameter->name != ZR_NULL &&
            ZrCore_String_Equal(parameter->name, name)) {
            return index;
        }
    }
    return ZR_MAX_SIZE;
}

TZrBool substitute_parameter_type(SZrCompilerState *cs,
                                         const SZrInferredType *sourceType,
                                         const SZrArray *genericParameters,
                                         const SZrArray *argumentTypeNames,
                                         const SZrArray *argumentTypes,
                                         SZrInferredType *outType) {
    SZrString *substitutedTypeName = sourceType != ZR_NULL ? sourceType->typeName : ZR_NULL;
    TZrSize bindingIndex;

    if (cs == ZR_NULL ||
        sourceType == ZR_NULL ||
        genericParameters == ZR_NULL ||
        argumentTypeNames == ZR_NULL ||
        argumentTypes == ZR_NULL ||
        outType == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingIndex = sourceType->elementTypes.length == 0U
                           ? generic_parameter_index_by_name(
                                     genericParameters,
                                     sourceType->typeName)
                           : ZR_MAX_SIZE;
    if (bindingIndex != ZR_MAX_SIZE && bindingIndex < argumentTypes->length) {
        const SZrInferredType *argumentType =
                (const SZrInferredType *)ZrCore_Array_Get(
                        (SZrArray *)argumentTypes,
                        bindingIndex);
        if (argumentType == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrParser_InferredType_Copy(cs->state, outType, argumentType);
        outType->ownershipQualifier = sourceType->ownershipQualifier;
        outType->gcBridgeKind = sourceType->gcBridgeKind;
        outType->referenceAccess = sourceType->referenceAccess;
        outType->isReadonlyView = sourceType->isReadonlyView;
        outType->protocolMask |= sourceType->protocolMask;
        outType->isNullable = sourceType->isNullable;
        outType->knownBoolValue = sourceType->knownBoolValue;
        outType->hasKnownBoolValue = sourceType->hasKnownBoolValue;
        return ZR_TRUE;
    }

    if (sourceType->typeName != ZR_NULL) {
        SZrString *candidate = substitute_generic_type_name(
                cs->state,
                sourceType->typeName,
                genericParameters,
                argumentTypeNames);
        if (candidate != ZR_NULL) {
            substitutedTypeName = candidate;
        }
    }

    ZrParser_InferredType_InitFull(
            cs->state,
            outType,
            sourceType->baseType,
            sourceType->isNullable,
            substitutedTypeName);
    outType->ownershipQualifier = sourceType->ownershipQualifier;
    outType->gcBridgeKind = sourceType->gcBridgeKind;
    outType->referenceAccess = sourceType->referenceAccess;
    outType->isReadonlyView = sourceType->isReadonlyView;
    outType->protocolMask = sourceType->protocolMask;
    outType->nominalDefinitionToken = sourceType->nominalDefinitionToken;
    outType->genericArgumentKind = sourceType->genericArgumentKind;
    outType->genericConstIntValue = sourceType->genericConstIntValue;
    outType->minValue = sourceType->minValue;
    outType->maxValue = sourceType->maxValue;
    outType->hasRangeConstraint = sourceType->hasRangeConstraint;
    ZrParser_InferredType_CopyRangeSegments(cs->state, outType, sourceType);
    outType->knownBoolValue = sourceType->knownBoolValue;
    outType->hasKnownBoolValue = sourceType->hasKnownBoolValue;
    outType->arrayFixedSize = sourceType->arrayFixedSize;
    outType->arrayMinSize = sourceType->arrayMinSize;
    outType->arrayMaxSize = sourceType->arrayMaxSize;
    outType->hasArraySizeConstraint = sourceType->hasArraySizeConstraint;

    if (sourceType->elementTypes.length > 0U) {
        TZrSize index;

        ZrCore_Array_Init(
                cs->state,
                &outType->elementTypes,
                sizeof(SZrInferredType),
                sourceType->elementTypes.length);
        for (index = 0; index < sourceType->elementTypes.length; index++) {
            const SZrInferredType *sourceElement =
                    (const SZrInferredType *)ZrCore_Array_Get(
                            (SZrArray *)&sourceType->elementTypes,
                            index);
            SZrInferredType substitutedElement;

            if (sourceElement == ZR_NULL ||
                !substitute_parameter_type(
                        cs,
                        sourceElement,
                        genericParameters,
                        argumentTypeNames,
                        argumentTypes,
                        &substitutedElement)) {
                ZrParser_InferredType_Free(cs->state, outType);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(cs->state, &outType->elementTypes, &substitutedElement);
        }
    }
    return ZR_TRUE;
}

static TZrUInt32 generic_instance_field_size_for_value_type(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_INT8: return sizeof(TZrInt8);
        case ZR_VALUE_TYPE_INT16: return sizeof(TZrInt16);
        case ZR_VALUE_TYPE_INT32: return sizeof(TZrInt32);
        case ZR_VALUE_TYPE_INT64: return sizeof(TZrInt64);
        case ZR_VALUE_TYPE_UINT8: return sizeof(TZrUInt8);
        case ZR_VALUE_TYPE_UINT16: return sizeof(TZrUInt16);
        case ZR_VALUE_TYPE_UINT32: return sizeof(TZrUInt32);
        case ZR_VALUE_TYPE_UINT64: return sizeof(TZrUInt64);
        case ZR_VALUE_TYPE_FLOAT: return sizeof(TZrFloat32);
        case ZR_VALUE_TYPE_DOUBLE: return sizeof(TZrDouble);
        case ZR_VALUE_TYPE_BOOL: return sizeof(TZrBool);
        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
        case ZR_VALUE_TYPE_CLOSURE:
            return sizeof(SZrTypeValue);
        default:
            return 0u;
    }
}

static TZrUInt32 generic_instance_field_alignment_for_value_type(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_INT8: return (TZrUInt32)_Alignof(TZrInt8);
        case ZR_VALUE_TYPE_INT16: return (TZrUInt32)_Alignof(TZrInt16);
        case ZR_VALUE_TYPE_INT32: return (TZrUInt32)_Alignof(TZrInt32);
        case ZR_VALUE_TYPE_INT64: return (TZrUInt32)_Alignof(TZrInt64);
        case ZR_VALUE_TYPE_UINT8: return (TZrUInt32)_Alignof(TZrUInt8);
        case ZR_VALUE_TYPE_UINT16: return (TZrUInt32)_Alignof(TZrUInt16);
        case ZR_VALUE_TYPE_UINT32: return (TZrUInt32)_Alignof(TZrUInt32);
        case ZR_VALUE_TYPE_UINT64: return (TZrUInt32)_Alignof(TZrUInt64);
        case ZR_VALUE_TYPE_FLOAT: return (TZrUInt32)_Alignof(TZrFloat32);
        case ZR_VALUE_TYPE_DOUBLE: return (TZrUInt32)_Alignof(TZrDouble);
        case ZR_VALUE_TYPE_BOOL: return (TZrUInt32)_Alignof(TZrBool);
        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
        case ZR_VALUE_TYPE_CLOSURE:
            return (TZrUInt32)_Alignof(SZrTypeValue);
        default:
            return 0u;
    }
}

static TZrBool generic_instance_type_name_primitive_layout(SZrString *typeName,
                                                           TZrUInt32 *outSize,
                                                           TZrUInt32 *outAlign) {
    const TZrChar *typeNameText;
    EZrValueType baseType;
    TZrUInt32 fieldSize;

    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    if (typeNameText == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!inferred_type_try_map_primitive_name(
                (TZrNativeString)typeNameText,
                strlen(typeNameText),
                &baseType)) {
        if (strcmp(typeNameText, "object") == 0 ||
            strcmp(typeNameText, "value") == 0 ||
            strcmp(typeNameText, "any") == 0) {
            baseType = ZR_VALUE_TYPE_OBJECT;
        } else if (strcmp(typeNameText, "array") == 0) {
            baseType = ZR_VALUE_TYPE_ARRAY;
        } else if (strcmp(typeNameText, "function") == 0 ||
                   strcmp(typeNameText, "closure") == 0) {
            baseType = ZR_VALUE_TYPE_CLOSURE;
        } else {
            return ZR_FALSE;
        }
    }

    fieldSize = generic_instance_field_size_for_value_type(baseType);
    if (fieldSize == 0u) {
        return ZR_FALSE;
    }

    if (outSize != ZR_NULL) {
        *outSize = fieldSize;
    }
    if (outAlign != ZR_NULL) {
        *outAlign = generic_instance_field_alignment_for_value_type(baseType);
    }
    return ZR_TRUE;
}

static TZrBool generic_instance_type_name_layout(SZrCompilerState *cs,
                                                 SZrString *typeName,
                                                 TZrUInt32 *outSize,
                                                 TZrUInt32 *outAlign) {
    SZrTypePrototypeInfo *prototype;

    if (generic_instance_type_name_primitive_layout(typeName, outSize, outAlign)) {
        return ZR_TRUE;
    }

    prototype = find_compiler_type_prototype_inference_exact(cs, typeName);
    if (prototype != ZR_NULL &&
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION) {
        if (outSize != ZR_NULL) {
            *outSize = (TZrUInt32)sizeof(SZrTypeValue);
        }
        if (outAlign != ZR_NULL) {
            *outAlign = (TZrUInt32)_Alignof(SZrTypeValue);
        }
        return ZR_TRUE;
    }
    if (prototype != ZR_NULL &&
        prototype->layoutByteSize > 0u &&
        prototype->layoutByteAlign > 0u) {
        if (outSize != ZR_NULL) {
            *outSize = prototype->layoutByteSize;
        }
        if (outAlign != ZR_NULL) {
            *outAlign = prototype->layoutByteAlign;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

void finalize_generic_instance_inline_layout(SZrCompilerState *cs,
                                                    SZrTypePrototypeInfo *prototypeInfo) {
    TZrUInt32 currentOffset = 0u;
    TZrUInt32 maxAlign = 0u;

    if (cs == ZR_NULL ||
        prototypeInfo == ZR_NULL ||
        prototypeInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        return;
    }

    for (TZrSize index = 0; index < prototypeInfo->members.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&prototypeInfo->members, index);
        TZrUInt32 fieldSize;
        TZrUInt32 align;

        if (memberInfo == ZR_NULL ||
            memberInfo->memberType != ZR_AST_STRUCT_FIELD ||
            memberInfo->isStatic) {
            continue;
        }

        fieldSize = memberInfo->fieldSize;
        align = 0u;
        if (!generic_instance_type_name_layout(cs, memberInfo->fieldTypeName, &fieldSize, &align)) {
            fieldSize = fieldSize != 0u ? fieldSize : ZR_ALIGN_SIZE;
            align = memberInfo->fieldType != ZR_NULL ? get_type_alignment(cs, memberInfo->fieldType)
                                                     : (TZrUInt32)_Alignof(SZrTypeValue);
        }

        if (fieldSize == 0u) {
            fieldSize = ZR_ALIGN_SIZE;
        }
        if (align == 0u) {
            align = (TZrUInt32)_Alignof(SZrTypeValue);
        }

        if (align > maxAlign) {
            maxAlign = align;
        }

        currentOffset = align_offset(currentOffset, align);
        memberInfo->fieldOffset = currentOffset;
        memberInfo->fieldSize = fieldSize;
        currentOffset += fieldSize;
    }

    prototypeInfo->layoutByteAlign = maxAlign;
    prototypeInfo->layoutByteSize = maxAlign > 0u ? align_offset(currentOffset, maxAlign) : 0u;
}
