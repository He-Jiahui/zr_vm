#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include "type_inference_internal.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrTypeId canonical_type_from_inferred_recursive(
        SZrSemanticContext *context,
        const SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        TZrSize depth);

static const SZrCanonicalGenericBinding *canonical_type_find_generic_binding(
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        SZrString *name) {
    TZrSize index;

    if (genericBindings == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < genericBindingCount; index++) {
        if (genericBindings[index].name != ZR_NULL &&
            ZrCore_String_Equal(genericBindings[index].name, name)) {
            return &genericBindings[index];
        }
    }
    return ZR_NULL;
}

static TZrTypeId canonical_type_from_name_raw(
        SZrSemanticContext *context,
        SZrString *qualifiedName) {
    TZrNativeString text;
    TZrSize length;
    const TZrChar *separator;
    SZrString *moduleIdentity = ZR_NULL;
    SZrString *name = qualifiedName;

    if (context == ZR_NULL || qualifiedName == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    text = ZrCore_String_GetNativeString(qualifiedName);
    length = ZrCore_String_GetByteLength(qualifiedName);
    if (text == ZR_NULL || length == 0) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    separator = strrchr(text, '.');
    if (separator != ZR_NULL && separator > text && (TZrSize)(separator - text + 1) < length) {
        TZrSize moduleLength = (TZrSize)(separator - text);
        TZrSize nameLength = length - moduleLength - 1U;

        moduleIdentity = ZrCore_String_Create(context->state, text, moduleLength);
        name = ZrCore_String_Create(context->state, (TZrNativeString)(separator + 1), nameLength);
    }
    return ZrParser_CanonicalType_InternNominal(context, moduleIdentity, name, 0U);
}

TZrTypeId ZrParser_CanonicalType_FromName(
        SZrSemanticContext *context,
        SZrString *qualifiedName) {
    TZrTypeId typeId = canonical_type_from_name_raw(context, qualifiedName);

    return typeId == ZR_SEMANTIC_ID_INVALID
                   ? typeId
                   : ZrParser_CanonicalType_ResolveProjection(context, typeId);
}

static TZrTypeId canonical_type_from_generic_inferred(
        SZrSemanticContext *context,
        const SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        TZrSize depth) {
    SZrString *definitionName = type->typeName;
    SZrString *parsedDefinitionName = ZR_NULL;
    SZrArray parsedArgumentNames;
    SZrArray arguments;
    TZrTypeId definitionTypeId;
    TZrTypeId result = ZR_SEMANTIC_ID_INVALID;
    TZrSize index;

    ZrCore_Array_Construct(&parsedArgumentNames);
    if (try_parse_generic_instance_type_name(
                context->state,
                type->typeName,
                &parsedDefinitionName,
                &parsedArgumentNames) &&
        parsedArgumentNames.length == type->elementTypes.length) {
        definitionName = parsedDefinitionName;
    }

    definitionTypeId = canonical_type_from_name_raw(context, definitionName);
    if (definitionTypeId == ZR_SEMANTIC_ID_INVALID) {
        if (parsedArgumentNames.isValid) {
            ZrCore_Array_Free(context->state, &parsedArgumentNames);
        }
        return ZR_SEMANTIC_ID_INVALID;
    }

    ZrCore_Array_Init(
            context->state,
            &arguments,
            sizeof(SZrCanonicalGenericArgument),
            type->elementTypes.length);
    for (index = 0; index < type->elementTypes.length; index++) {
        const SZrInferredType *argument = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)&type->elementTypes,
                index);
        SZrCanonicalGenericArgument canonicalArgument;

        if (argument != ZR_NULL &&
            argument->genericArgumentKind == ZR_INFERRED_GENERIC_ARGUMENT_CONST_INT) {
            canonicalArgument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT;
            canonicalArgument.data.constIntValue = argument->genericConstIntValue;
        } else if (argument != ZR_NULL &&
                   argument->genericArgumentKind == ZR_INFERRED_GENERIC_ARGUMENT_CONST_PARAMETER) {
            const SZrCanonicalGenericBinding *binding =
                    canonical_type_find_generic_binding(
                            genericBindings,
                            genericBindingCount,
                            argument->typeName);
            if (binding == ZR_NULL ||
                binding->kind != ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER ||
                binding->ownerSymbolId == ZR_SEMANTIC_ID_INVALID) {
                ZrCore_Array_Free(context->state, &arguments);
                if (parsedArgumentNames.isValid) {
                    ZrCore_Array_Free(context->state, &parsedArgumentNames);
                }
                return ZR_SEMANTIC_ID_INVALID;
            }
            canonicalArgument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
            canonicalArgument.data.constParameter.ownerSymbolId = binding->ownerSymbolId;
            canonicalArgument.data.constParameter.ordinal = binding->ordinal;
            canonicalArgument.data.constParameter.displayName = binding->name;
        } else {
            if (argument == ZR_NULL ||
                argument->genericArgumentKind != ZR_INFERRED_GENERIC_ARGUMENT_TYPE) {
                ZrCore_Array_Free(context->state, &arguments);
                if (parsedArgumentNames.isValid) {
                    ZrCore_Array_Free(context->state, &parsedArgumentNames);
                }
                return ZR_SEMANTIC_ID_INVALID;
            }
            canonicalArgument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
            canonicalArgument.data.typeId = canonical_type_from_inferred_recursive(
                    context,
                    argument,
                    genericBindings,
                    genericBindingCount,
                    depth + 1U);
            if (canonicalArgument.data.typeId == ZR_SEMANTIC_ID_INVALID) {
                ZrCore_Array_Free(context->state, &arguments);
                if (parsedArgumentNames.isValid) {
                    ZrCore_Array_Free(context->state, &parsedArgumentNames);
                }
                return ZR_SEMANTIC_ID_INVALID;
            }
        }
        ZrCore_Array_Push(context->state, &arguments, &canonicalArgument);
    }

    result = ZrParser_CanonicalType_InternGenericInstanceEx(
            context,
            definitionTypeId,
            (const SZrCanonicalGenericArgument *)arguments.head,
            arguments.length);
    ZrCore_Array_Free(context->state, &arguments);
    if (parsedArgumentNames.isValid) {
        ZrCore_Array_Free(context->state, &parsedArgumentNames);
    }
    return result == ZR_SEMANTIC_ID_INVALID
                   ? result
                   : ZrParser_CanonicalType_ResolveProjection(context, result);
}

static TZrTypeId canonical_type_from_array_inferred(
        SZrSemanticContext *context,
        const SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        TZrSize depth) {
    const SZrInferredType *elementType;
    TZrTypeId elementTypeId;

    if (type->elementTypes.length == 0) {
        return ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_ARRAY);
    }
    elementType = (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, 0);
    elementTypeId = canonical_type_from_inferred_recursive(
            context,
            elementType,
            genericBindings,
            genericBindingCount,
            depth + 1U);
    if (elementTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return ZrParser_CanonicalType_InternArray(
            context,
            elementTypeId,
            1U,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
}

static TZrTypeId canonical_type_from_tuple_inferred(
        SZrSemanticContext *context,
        const SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        TZrSize depth) {
    SZrArray elementTypeIds;
    TZrTypeId result;
    TZrSize index;

    ZrCore_Array_Init(
            context->state,
            &elementTypeIds,
            sizeof(TZrTypeId),
            type->elementTypes.length);
    for (index = 0; index < type->elementTypes.length; index++) {
        const SZrInferredType *elementType = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)&type->elementTypes,
                index);
        TZrTypeId elementTypeId = canonical_type_from_inferred_recursive(
                context,
                elementType,
                genericBindings,
                genericBindingCount,
                depth + 1U);

        if (elementTypeId == ZR_SEMANTIC_ID_INVALID) {
            ZrCore_Array_Free(context->state, &elementTypeIds);
            return ZR_SEMANTIC_ID_INVALID;
        }
        ZrCore_Array_Push(context->state, &elementTypeIds, &elementTypeId);
    }
    result = ZrParser_CanonicalType_InternTuple(
            context,
            (const TZrTypeId *)elementTypeIds.head,
            elementTypeIds.length);
    ZrCore_Array_Free(context->state, &elementTypeIds);
    return result;
}

static TZrTypeId canonical_type_apply_legacy_qualifiers(
        SZrSemanticContext *context,
        const SZrInferredType *type,
        TZrTypeId typeId) {
    switch (type->ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_NONE:
            break;
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            typeId = ZrParser_CanonicalType_InternOwner(context, typeId, ZR_CANONICAL_OWNER_UNIQUE);
            break;
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            typeId = ZrParser_CanonicalType_InternOwner(context, typeId, ZR_CANONICAL_OWNER_SHARED);
            break;
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            typeId = ZrParser_CanonicalType_InternOwner(context, typeId, ZR_CANONICAL_OWNER_WEAK);
            break;
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            typeId = ZrParser_CanonicalType_InternRef(context, typeId, ZR_CANONICAL_REF_READONLY);
            break;
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            typeId = ZrParser_CanonicalType_InternRef(context, typeId, ZR_CANONICAL_REF_WRITABLE);
            break;
        default:
            return ZR_SEMANTIC_ID_INVALID;
    }
    if (typeId != ZR_SEMANTIC_ID_INVALID && type->isReadonlyView) {
        typeId = ZrParser_CanonicalType_InternReadonlyView(context, typeId);
    }
    if (typeId != ZR_SEMANTIC_ID_INVALID && type->isNullable) {
        typeId = ZrParser_CanonicalType_InternNullable(context, typeId);
    }
    return typeId;
}

static TZrTypeId canonical_type_from_inferred_recursive(
        SZrSemanticContext *context,
        const SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        TZrSize depth) {
    TZrTypeId typeId;

    if (context == ZR_NULL ||
        type == ZR_NULL ||
        type->genericArgumentKind != ZR_INFERRED_GENERIC_ARGUMENT_TYPE ||
        depth > 256U) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    typeId = ZR_SEMANTIC_ID_INVALID;
    if (type->baseType == ZR_VALUE_TYPE_OBJECT &&
        type->typeName != ZR_NULL &&
        type->elementTypes.length == 0U) {
        const SZrCanonicalGenericBinding *binding =
                canonical_type_find_generic_binding(
                        genericBindings,
                        genericBindingCount,
                        type->typeName);
        if (binding != ZR_NULL && binding->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE) {
            typeId = binding->typeId;
        }
    }
    if (typeId != ZR_SEMANTIC_ID_INVALID) {
        return canonical_type_apply_legacy_qualifiers(context, type, typeId);
    }

    if (type->baseType == ZR_VALUE_TYPE_ARRAY) {
        typeId = canonical_type_from_array_inferred(
                context,
                type,
                genericBindings,
                genericBindingCount,
                depth);
    } else if (type->baseType == ZR_VALUE_TYPE_OBJECT &&
               type->typeName == ZR_NULL &&
               type->elementTypes.length > 0) {
        typeId = canonical_type_from_tuple_inferred(
                context,
                type,
                genericBindings,
                genericBindingCount,
                depth);
    } else if (type->baseType == ZR_VALUE_TYPE_OBJECT &&
               type->typeName != ZR_NULL &&
               type->elementTypes.length > 0) {
        typeId = canonical_type_from_generic_inferred(
                context,
                type,
                genericBindings,
                genericBindingCount,
                depth);
    } else if (type->baseType == ZR_VALUE_TYPE_OBJECT && type->typeName != ZR_NULL) {
        typeId = ZrParser_CanonicalType_FromName(context, type->typeName);
    } else {
        typeId = ZrParser_CanonicalType_InternPrimitive(context, type->baseType);
    }
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return typeId;
    }
    return canonical_type_apply_legacy_qualifiers(context, type, typeId);
}

TZrTypeId ZrParser_CanonicalType_FromInferred(
        SZrSemanticContext *context,
        const SZrInferredType *type) {
    return ZrParser_CanonicalType_FromInferredWithGenericBindings(
            context,
            type,
            ZR_NULL,
            0U);
}

TZrTypeId ZrParser_CanonicalType_FromInferredWithGenericBindings(
        SZrSemanticContext *context,
        const SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount) {
    if (genericBindingCount > 0U && genericBindings == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return canonical_type_from_inferred_recursive(
            context,
            type,
            genericBindings,
            genericBindingCount,
            0U);
}

static TZrBool canonical_parameter_contract_from_legacy(
        SZrSemanticContext *context,
        const SZrInferredType *parameterType,
        EZrParameterPassingMode passingMode,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        SZrCanonicalParameterContract *outContract) {
    const SZrCanonicalTypeNode *typeNode;
    TZrTypeId typeId;

    if (context == ZR_NULL || parameterType == ZR_NULL || outContract == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outContract, 0, sizeof(*outContract));
    typeId = ZrParser_CanonicalType_FromInferredWithGenericBindings(
            context,
            parameterType,
            genericBindings,
            genericBindingCount);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    typeNode = ZrParser_CanonicalType_Find(context, typeId);

    outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    outContract->entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    outContract->exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    outContract->acceptsTemporary = ZR_TRUE;
    outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;

    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_VALUE:
            outContract->passingForm = ZR_CANONICAL_PASSING_VALUE;
            break;
        case ZR_PARAMETER_PASSING_MODE_IN:
            if (typeNode != ZR_NULL && typeNode->kind == ZR_CANONICAL_TYPE_REF) {
                typeId = typeNode->data.refType.pointeeTypeId;
            }
            typeId = ZrParser_CanonicalType_InternRef(context, typeId, ZR_CANONICAL_REF_READONLY);
            outContract->passingForm = ZR_CANONICAL_PASSING_IN;
            break;
        case ZR_PARAMETER_PASSING_MODE_REF:
            if (typeNode != ZR_NULL && typeNode->kind == ZR_CANONICAL_TYPE_REF) {
                typeId = typeNode->data.refType.pointeeTypeId;
            }
            typeId = ZrParser_CanonicalType_InternRef(context, typeId, ZR_CANONICAL_REF_WRITABLE);
            outContract->passingForm = ZR_CANONICAL_PASSING_REF;
            outContract->escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;
            break;
        case ZR_PARAMETER_PASSING_MODE_OUT:
            if (typeNode != ZR_NULL && typeNode->kind == ZR_CANONICAL_TYPE_REF) {
                typeId = typeNode->data.refType.pointeeTypeId;
            }
            typeId = ZrParser_CanonicalType_InternRef(context, typeId, ZR_CANONICAL_REF_WRITABLE);
            outContract->passingForm = ZR_CANONICAL_PASSING_OUT;
            outContract->entryInitialization = ZR_CANONICAL_ENTRY_UNINITIALIZED;
            outContract->exitInitialization = ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED;
            outContract->acceptsTemporary = ZR_FALSE;
            outContract->callSiteMarker = ZR_CANONICAL_CALL_SITE_OUT;
            break;
        default:
            return ZR_FALSE;
    }
    outContract->typeId = typeId;
    return typeId != ZR_SEMANTIC_ID_INVALID;
}

TZrTypeId ZrParser_CanonicalType_FromFunctionSignature(
        SZrSemanticContext *context,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        const SZrInferredType *returnType,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags) {
    return ZrParser_CanonicalType_FromFunctionSignatureWithGenericBindings(
            context,
            parameterTypes,
            parameterPassingModes,
            returnType,
            receiverEffect,
            effectFlags,
            ZR_NULL,
            0U);
}

TZrTypeId ZrParser_CanonicalType_FromFunctionSignatureWithGenericBindings(
        SZrSemanticContext *context,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        const SZrInferredType *returnType,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount) {
    SZrArray contracts;
    TZrTypeId returnTypeId;
    TZrTypeId functionTypeId;
    TZrSize parameterCount;
    TZrSize index;

    if (context == ZR_NULL ||
        returnType == ZR_NULL ||
        (genericBindingCount > 0U && genericBindings == ZR_NULL)) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    parameterCount = parameterTypes != ZR_NULL ? parameterTypes->length : 0U;
    returnTypeId = ZrParser_CanonicalType_FromInferredWithGenericBindings(
            context,
            returnType,
            genericBindings,
            genericBindingCount);
    if (returnTypeId == ZR_SEMANTIC_ID_INVALID) {
        return returnTypeId;
    }

    ZrCore_Array_Init(
            context->state,
            &contracts,
            sizeof(SZrCanonicalParameterContract),
            parameterCount > 0 ? parameterCount : ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (index = 0; index < parameterCount; index++) {
        const SZrInferredType *parameterType = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)parameterTypes,
                index);
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        SZrCanonicalParameterContract contract;

        if (parameterPassingModes != ZR_NULL && index < parameterPassingModes->length) {
            const EZrParameterPassingMode *storedMode =
                    (const EZrParameterPassingMode *)ZrCore_Array_Get(
                            (SZrArray *)parameterPassingModes,
                            index);
            if (storedMode != ZR_NULL) {
                passingMode = *storedMode;
            }
        }
        if (!canonical_parameter_contract_from_legacy(
                    context,
                    parameterType,
                    passingMode,
                    genericBindings,
                    genericBindingCount,
                    &contract)) {
            ZrCore_Array_Free(context->state, &contracts);
            return ZR_SEMANTIC_ID_INVALID;
        }
        ZrCore_Array_Push(context->state, &contracts, &contract);
    }

    functionTypeId = ZrParser_CanonicalType_InternFunction(
            context,
            (const SZrCanonicalParameterContract *)contracts.head,
            contracts.length,
            returnTypeId,
            receiverEffect,
            effectFlags);
    ZrCore_Array_Free(context->state, &contracts);
    return functionTypeId;
}
