#include "compiler_internal.h"
#include "compiler_ffi_callable_contract.h"

#include "zr_vm_parser/syntax_contract.h"
#include "zr_vm_core/type_layout.h"

#include <string.h>

static SZrString *ffi_callable_syntax_type_name(const SZrType *type) {
    if (type == ZR_NULL || type->name == ZR_NULL) {
        return ZR_NULL;
    }
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return type->name->data.identifier.name;
    }
    if (type->name->type == ZR_AST_GENERIC_TYPE &&
        type->name->data.genericType.name != ZR_NULL) {
        return type->name->data.genericType.name->name;
    }
    return ZR_NULL;
}

static SZrString *ffi_callable_declaration_name(const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    switch (declaration->type) {
        case ZR_AST_STRUCT_DECLARATION:
            return declaration->data.structDeclaration.name != ZR_NULL
                           ? declaration->data.structDeclaration.name->name
                           : ZR_NULL;
        case ZR_AST_ENUM_DECLARATION:
            return declaration->data.enumDeclaration.name != ZR_NULL
                           ? declaration->data.enumDeclaration.name->name
                           : ZR_NULL;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return declaration->data.externDelegateDeclaration.name != ZR_NULL
                           ? declaration->data.externDelegateDeclaration.name->name
                           : ZR_NULL;
        default:
            return ZR_NULL;
    }
}

static const SZrAstNode *ffi_callable_find_local_declaration(
        const SZrExternBlock *externBlock,
        SZrString *name,
        TZrUInt32 *outDefinitionToken) {
    if (outDefinitionToken != ZR_NULL) {
        *outDefinitionToken = 0u;
    }
    if (externBlock == ZR_NULL || externBlock->declarations == ZR_NULL ||
        name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < externBlock->declarations->count; index++) {
        const SZrAstNode *declaration = externBlock->declarations->nodes[index];
        SZrString *declarationName = ffi_callable_declaration_name(declaration);

        if (declarationName != ZR_NULL &&
            ZrCore_String_Equal(declarationName, name)) {
            if (outDefinitionToken != ZR_NULL) {
                *outDefinitionToken = (TZrUInt32)index + 1u;
            }
            return declaration;
        }
    }
    return ZR_NULL;
}

static EZrValueType ffi_callable_primitive_value_type(EZrFfiTypeKind kind) {
    switch (kind) {
        case ZR_FFI_CONTRACT_TYPE_VOID:
            return ZR_VALUE_TYPE_NULL;
        case ZR_FFI_CONTRACT_TYPE_BOOL:
            return ZR_VALUE_TYPE_BOOL;
        case ZR_FFI_CONTRACT_TYPE_I8:
            return ZR_VALUE_TYPE_INT8;
        case ZR_FFI_CONTRACT_TYPE_U8:
            return ZR_VALUE_TYPE_UINT8;
        case ZR_FFI_CONTRACT_TYPE_I16:
            return ZR_VALUE_TYPE_INT16;
        case ZR_FFI_CONTRACT_TYPE_U16:
            return ZR_VALUE_TYPE_UINT16;
        case ZR_FFI_CONTRACT_TYPE_I32:
            return ZR_VALUE_TYPE_INT32;
        case ZR_FFI_CONTRACT_TYPE_U32:
            return ZR_VALUE_TYPE_UINT32;
        case ZR_FFI_CONTRACT_TYPE_I64:
            return ZR_VALUE_TYPE_INT64;
        case ZR_FFI_CONTRACT_TYPE_U64:
            return ZR_VALUE_TYPE_UINT64;
        case ZR_FFI_CONTRACT_TYPE_F32:
            return ZR_VALUE_TYPE_FLOAT;
        case ZR_FFI_CONTRACT_TYPE_F64:
            return ZR_VALUE_TYPE_DOUBLE;
        case ZR_FFI_CONTRACT_TYPE_USIZE:
            return sizeof(TZrSize) == sizeof(TZrUInt64)
                           ? ZR_VALUE_TYPE_UINT64
                           : ZR_VALUE_TYPE_UINT32;
        case ZR_FFI_CONTRACT_TYPE_ISIZE:
            return sizeof(TZrPtr) == sizeof(TZrInt64)
                           ? ZR_VALUE_TYPE_INT64
                           : ZR_VALUE_TYPE_INT32;
        case ZR_FFI_CONTRACT_TYPE_POINTER:
            return ZR_VALUE_TYPE_NATIVE_POINTER;
        default:
            return ZR_VALUE_TYPE_UNKNOWN;
    }
}

static TZrUInt32 ffi_callable_capabilities_from_type(
        const SZrFfiTypeContract *type) {
    TZrUInt32 capabilities = ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE;

    if (type == ZR_NULL) {
        return ZR_CANONICAL_TYPE_CAPABILITY_NONE;
    }
    if ((type->flags & ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE) != 0u ||
        type->typeKind == ZR_FFI_CONTRACT_TYPE_VOID) {
        capabilities |= ZR_CANONICAL_TYPE_CAPABILITY_BLITTABLE;
    }
    if ((type->flags & ZR_FFI_CONTRACT_TYPE_FLAG_GC_REFERENCE) != 0u) {
        capabilities |= ZR_CANONICAL_TYPE_CAPABILITY_HAS_GC_REFERENCES;
    }
    if ((type->flags & ZR_FFI_CONTRACT_TYPE_FLAG_REF_LIKE) != 0u) {
        capabilities |= ZR_CANONICAL_TYPE_CAPABILITY_REF_LIKE;
    }
    if ((type->flags & ZR_FFI_CONTRACT_TYPE_FLAG_RESOURCE) != 0u) {
        capabilities |= ZR_CANONICAL_TYPE_CAPABILITY_RESOURCE_CLASS;
    }
    if ((type->flags & ZR_FFI_CONTRACT_TYPE_FLAG_OWNER) != 0u) {
        capabilities |= ZR_CANONICAL_TYPE_CAPABILITY_HAS_OWNERSHIP_FIELDS;
    }
    return capabilities;
}

static TZrTypeId ffi_callable_resolve_value_type(
        SZrSemanticContext *context,
        const SZrExternBlock *externBlock,
        const SZrType *syntaxType,
        const SZrFfiTypeContract *ffiType) {
    EZrValueType primitiveType;
    TZrTypeId typeId;
    TZrUInt32 capabilities;

    if (context == ZR_NULL || ffiType == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    primitiveType = ffi_callable_primitive_value_type(ffiType->typeKind);
    if (primitiveType != ZR_VALUE_TYPE_UNKNOWN) {
        typeId = ZrParser_CanonicalType_InternPrimitive(context, primitiveType);
        return typeId;
    } else {
        SZrString *name = ffi_callable_syntax_type_name(syntaxType);
        TZrUInt32 definitionToken = 0u;
        const SZrAstNode *localDeclaration =
                ffi_callable_find_local_declaration(
                        externBlock, name, &definitionToken);

        if (localDeclaration == ZR_NULL || name == ZR_NULL) {
            return ZR_SEMANTIC_ID_INVALID;
        }
        typeId = ZrParser_CanonicalType_InternNominal(
                context,
                localDeclaration->location.source,
                name,
                definitionToken);
    }
    capabilities = ffi_callable_capabilities_from_type(ffiType);
    if (!ZrParser_CanonicalType_RegisterDefinition(
                context, typeId, capabilities, ZR_CANONICAL_GC_SCAN_FREE) ||
        !ZrParser_CanonicalType_HasCapabilities(
                context, typeId, ZR_CANONICAL_TYPE_CAPABILITY_BLITTABLE)) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return typeId;
}

static TZrBool ffi_callable_store_parameter(
        SZrSemanticContext *context,
        const SZrParameter *parameter,
        TZrTypeId valueTypeId,
        SZrFfiCallableParameterContract *outParameter) {
    const SZrCanonicalTypeNode *valueType;
    SZrCanonicalParameterContract canonical;

    if (!ZrParser_SyntaxParameter_Normalize(
                context, parameter, valueTypeId, &canonical)) {
        return ZR_FALSE;
    }
    valueType = ZrParser_CanonicalType_Find(context, valueTypeId);
    if (valueType == ZR_NULL || valueType->structuralHash == 0u) {
        return ZR_FALSE;
    }
    memset(outParameter, 0, sizeof(*outParameter));
    switch (canonical.passingForm) {
        case ZR_CANONICAL_PASSING_VALUE:
            outParameter->passingForm = ZR_FFI_CALLABLE_PASSING_VALUE;
            break;
        case ZR_CANONICAL_PASSING_IN:
            outParameter->passingForm = ZR_FFI_CALLABLE_PASSING_IN;
            break;
        case ZR_CANONICAL_PASSING_REF:
            outParameter->passingForm = ZR_FFI_CALLABLE_PASSING_REF;
            break;
        case ZR_CANONICAL_PASSING_REF_READONLY:
            outParameter->passingForm = ZR_FFI_CALLABLE_PASSING_REF_READONLY;
            break;
        case ZR_CANONICAL_PASSING_OUT:
            outParameter->passingForm = ZR_FFI_CALLABLE_PASSING_OUT;
            break;
        default:
            return ZR_FALSE;
    }
    switch (canonical.escapeUpperBound) {
        case ZR_CANONICAL_ESCAPE_BLOCK:
            outParameter->escapeUpperBound = ZR_FFI_CALLABLE_ESCAPE_BLOCK;
            break;
        case ZR_CANONICAL_ESCAPE_FUNCTION:
            outParameter->escapeUpperBound = ZR_FFI_CALLABLE_ESCAPE_FUNCTION;
            break;
        case ZR_CANONICAL_ESCAPE_CALLER:
            outParameter->escapeUpperBound = ZR_FFI_CALLABLE_ESCAPE_CALLER;
            break;
        case ZR_CANONICAL_ESCAPE_HEAP_STATIC:
            outParameter->escapeUpperBound = ZR_FFI_CALLABLE_ESCAPE_HEAP_STATIC;
            break;
        case ZR_CANONICAL_ESCAPE_UNKNOWN:
            outParameter->escapeUpperBound = ZR_FFI_CALLABLE_ESCAPE_UNKNOWN;
            break;
        default:
            return ZR_FALSE;
    }
    switch (canonical.entryInitialization) {
        case ZR_CANONICAL_ENTRY_INITIALIZED:
            outParameter->entryInitialization =
                    ZR_FFI_CALLABLE_ENTRY_INITIALIZED;
            break;
        case ZR_CANONICAL_ENTRY_UNINITIALIZED:
            outParameter->entryInitialization =
                    ZR_FFI_CALLABLE_ENTRY_UNINITIALIZED;
            break;
        default:
            return ZR_FALSE;
    }
    switch (canonical.exitInitialization) {
        case ZR_CANONICAL_EXIT_UNCHANGED:
            outParameter->exitInitialization =
                    ZR_FFI_CALLABLE_EXIT_UNCHANGED;
            break;
        case ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED:
            outParameter->exitInitialization =
                    ZR_FFI_CALLABLE_EXIT_DEFINITELY_INITIALIZED;
            break;
        default:
            return ZR_FALSE;
    }
    switch (canonical.callSiteMarker) {
        case ZR_CANONICAL_CALL_SITE_NONE:
            outParameter->callSiteMarker = ZR_FFI_CALLABLE_CALL_SITE_NONE;
            break;
        case ZR_CANONICAL_CALL_SITE_REF:
            outParameter->callSiteMarker = ZR_FFI_CALLABLE_CALL_SITE_REF;
            break;
        case ZR_CANONICAL_CALL_SITE_OUT:
            outParameter->callSiteMarker = ZR_FFI_CALLABLE_CALL_SITE_OUT;
            break;
        default:
            return ZR_FALSE;
    }
    outParameter->canonicalTypeHash = valueType->structuralHash;
    outParameter->acceptsTemporary = canonical.acceptsTemporary;
    return ZR_TRUE;
}

static TZrBool ffi_callable_store_function_effects(
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags,
        SZrFfiCallableContract *outContract) {
    const TZrUInt32 canonicalEffectMask =
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS |
            ZR_CANONICAL_CALLABLE_EFFECT_ASYNC |
            ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR;

    if (outContract == ZR_NULL ||
        (effectFlags & ~canonicalEffectMask) != 0u) {
        return ZR_FALSE;
    }
    switch (receiverEffect) {
        case ZR_CANONICAL_RECEIVER_NONE:
            outContract->receiverEffect = ZR_FFI_CALLABLE_RECEIVER_NONE;
            break;
        case ZR_CANONICAL_RECEIVER_READONLY:
            outContract->receiverEffect = ZR_FFI_CALLABLE_RECEIVER_READONLY;
            break;
        case ZR_CANONICAL_RECEIVER_MUTABLE:
            outContract->receiverEffect = ZR_FFI_CALLABLE_RECEIVER_MUTABLE;
            break;
        default:
            return ZR_FALSE;
    }
    outContract->effectFlags = ZR_FFI_CALLABLE_EFFECT_NONE;
    if ((effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_THROWS) != 0u) {
        outContract->effectFlags |= ZR_FFI_CALLABLE_EFFECT_THROWS;
    }
    if ((effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0u) {
        outContract->effectFlags |= ZR_FFI_CALLABLE_EFFECT_ASYNC;
    }
    if ((effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR) != 0u) {
        outContract->effectFlags |= ZR_FFI_CALLABLE_EFFECT_GENERATOR;
    }
    return ZR_TRUE;
}

static TZrBool ffi_callable_canonicalize_aggregate_layout(
        SZrFfiSignatureContract *signature,
        SZrFfiTypeContract *type) {
    SZrTypeLayoutField fields[ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS];
    SZrTypeLayout layout;

    if (signature == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }
    if (type->typeKind != ZR_FFI_CONTRACT_TYPE_STRUCT &&
        type->typeKind != ZR_FFI_CONTRACT_TYPE_UNION) {
        return ZR_TRUE;
    }
    if (type->aggregateFieldCount == 0u ||
        type->aggregateFieldStart > signature->aggregateFieldCount ||
        type->aggregateFieldCount >
                signature->aggregateFieldCount - type->aggregateFieldStart) {
        return ZR_FALSE;
    }

    memset(fields, 0, sizeof(fields));
    for (TZrUInt32 index = 0u; index < type->aggregateFieldCount; index++) {
        const SZrFfiAggregateFieldContract *field =
                &signature->aggregateFields[type->aggregateFieldStart + index];

        fields[index].byteOffset = field->offset;
        fields[index].byteSize = field->size;
        fields[index].typeLayoutIndex = 0u;
        fields[index].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
        fields[index].activeTag = 0u;
    }

    if (type->typeKind == ZR_FFI_CONTRACT_TYPE_UNION) {
        ZrCore_TypeLayout_InitUnion(
                &layout,
                type->size,
                type->alignment,
                0u,
                0u,
                ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                fields,
                type->aggregateFieldCount);
    } else {
        ZrCore_TypeLayout_InitStruct(
                &layout,
                type->size,
                type->alignment,
                ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                fields,
                type->aggregateFieldCount);
    }
    if (!ZrCore_TypeLayout_Validate(&layout) || !layout.blittable ||
        layout.byteSize != type->size || layout.byteAlign != type->alignment) {
        return ZR_FALSE;
    }
    type->layoutHash = layout.layoutHash;
    return ZR_TRUE;
}

static TZrBool ffi_callable_canonicalize_signature_layouts(
        SZrFfiSignatureContract *signature) {
    if (signature == ZR_NULL ||
        !ffi_callable_canonicalize_aggregate_layout(
                signature, &signature->returnType)) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < signature->parameterCount; index++) {
        if (!ffi_callable_canonicalize_aggregate_layout(
                    signature, &signature->parameters[index].type)) {
            return ZR_FALSE;
        }
    }
    signature->signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(signature);
    return signature->signatureHash != 0u;
}

EZrFfiContractStatus compiler_ffi_callable_contract_build(
        SZrSemanticContext *context,
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrFfiSignatureContract *signature,
        SZrFfiCallableContract *outContract) {
    const SZrExternFunctionDeclaration *function;
    TZrTypeId parameterTypeIds[ZR_FFI_CONTRACT_MAX_PARAMETERS];
    TZrTypeId returnTypeId;
    TZrTypeId callableTypeId;
    const SZrCanonicalTypeNode *returnType;
    EZrCanonicalReceiverEffect receiverEffect;
    TZrUInt32 effectFlags;

    if (context == ZR_NULL || externBlock == ZR_NULL || declaration == ZR_NULL ||
        declaration->type != ZR_AST_EXTERN_FUNCTION_DECLARATION ||
        signature == ZR_NULL || outContract == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    function = &declaration->data.externFunctionDeclaration;
    if (signature->parameterCount > ZR_FFI_CONTRACT_MAX_PARAMETERS ||
        (signature->parameterCount > 0u &&
         (function->params == ZR_NULL ||
          function->params->count != signature->parameterCount))) {
        return ZR_FFI_CONTRACT_STATUS_PARAMETER_LIMIT;
    }
    if (!ffi_callable_canonicalize_signature_layouts(signature)) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }

    memset(outContract, 0, sizeof(*outContract));
    outContract->parameterCount = signature->parameterCount;
    for (TZrUInt32 index = 0u; index < signature->parameterCount; index++) {
        const SZrAstNode *parameterNode = function->params->nodes[index];

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
            return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
        }
        parameterTypeIds[index] = ffi_callable_resolve_value_type(
                context,
                externBlock,
                parameterNode->data.parameter.typeInfo,
                &signature->parameters[index].type);
        if (parameterTypeIds[index] == ZR_SEMANTIC_ID_INVALID ||
            !ffi_callable_store_parameter(
                    context,
                    &parameterNode->data.parameter,
                    parameterTypeIds[index],
                    &outContract->parameters[index])) {
            return ZR_FFI_CONTRACT_STATUS_FORBIDDEN_MANAGED_TYPE;
        }
    }

    returnTypeId = ffi_callable_resolve_value_type(
            context,
            externBlock,
            function->returnType,
            &signature->returnType);
    returnType = ZrParser_CanonicalType_Find(context, returnTypeId);
    if (returnType == ZR_NULL || returnType->structuralHash == 0u) {
        return ZR_FFI_CONTRACT_STATUS_FORBIDDEN_MANAGED_TYPE;
    }
    outContract->returnTypeHash = returnType->structuralHash;
    receiverEffect = ZrParser_SyntaxCallable_ReceiverEffectFromDeclaration(declaration);
    effectFlags = ZrParser_SyntaxCallable_EffectFlagsFromDeclaration(declaration);
    callableTypeId = ZrParser_SyntaxCallable_Intern(
            context,
            function->params,
            parameterTypeIds,
            returnTypeId,
            receiverEffect,
            effectFlags);
    if (callableTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    if (!ffi_callable_store_function_effects(
                receiverEffect, effectFlags, outContract)) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    outContract->isVariadic = signature->isVariadic;
    outContract->contractHash =
            ZrCommon_FfiCallableContract_ComputeHash(outContract);
    return ZrCommon_FfiCallableContract_Validate(outContract)
                   ? ZR_FFI_CONTRACT_STATUS_OK
                   : ZR_FFI_CONTRACT_STATUS_HASH_MISMATCH;
}
