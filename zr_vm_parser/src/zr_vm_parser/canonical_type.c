#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include "zr_vm_core/array.h"

#include <string.h>

#define ZR_CANONICAL_TYPE_HASH_OFFSET ((TZrUInt64)1469598103934665603ULL)
#define ZR_CANONICAL_TYPE_HASH_PRIME ((TZrUInt64)1099511628211ULL)

static TZrUInt64 canonical_type_hash_word(TZrUInt64 hash, TZrUInt64 value) {
    TZrSize byteIndex;

    for (byteIndex = 0; byteIndex < sizeof(value); byteIndex++) {
        hash ^= (TZrByte)(value & 0xffU);
        hash *= ZR_CANONICAL_TYPE_HASH_PRIME;
        value >>= 8U;
    }
    return hash;
}

static TZrUInt64 canonical_type_hash_string(TZrUInt64 hash, SZrString *value) {
    TZrNativeString text;
    TZrSize index;
    TZrSize length;

    if (value == ZR_NULL) {
        return canonical_type_hash_word(hash, 0U);
    }

    text = ZrCore_String_GetNativeString(value);
    length = value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                     ? value->shortStringLength
                     : value->longStringLength;
    hash = canonical_type_hash_word(hash, length);
    for (index = 0; text != ZR_NULL && index < length; index++) {
        hash ^= (TZrByte)text[index];
        hash *= ZR_CANONICAL_TYPE_HASH_PRIME;
    }
    return hash;
}

static TZrBool canonical_type_strings_equal(SZrString *left, SZrString *right) {
    if (left == right) {
        return ZR_TRUE;
    }
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrUInt64 canonical_primitive_hash(EZrValueType valueType) {
    TZrUInt64 hash = ZR_CANONICAL_TYPE_HASH_OFFSET;

    hash = canonical_type_hash_word(hash, (TZrUInt64)ZR_CANONICAL_TYPE_PRIMITIVE);
    return canonical_type_hash_word(hash, (TZrUInt64)valueType);
}

static TZrUInt64 canonical_nominal_hash(
        SZrString *moduleIdentity,
        SZrString *name,
        TZrUInt32 definitionToken) {
    TZrUInt64 hash = ZR_CANONICAL_TYPE_HASH_OFFSET;

    hash = canonical_type_hash_word(hash, (TZrUInt64)ZR_CANONICAL_TYPE_NOMINAL);
    hash = canonical_type_hash_string(hash, moduleIdentity);
    if (definitionToken == 0U) {
        hash = canonical_type_hash_string(hash, name);
    }
    return canonical_type_hash_word(hash, definitionToken);
}

static TZrUInt64 canonical_generic_instance_hash(
        TZrTypeId definitionTypeId,
        const SZrCanonicalGenericArgument *arguments,
        TZrSize argumentCount) {
    TZrUInt64 hash = ZR_CANONICAL_TYPE_HASH_OFFSET;
    TZrSize index;

    hash = canonical_type_hash_word(hash, (TZrUInt64)ZR_CANONICAL_TYPE_GENERIC_INSTANCE);
    hash = canonical_type_hash_word(hash, definitionTypeId);
    hash = canonical_type_hash_word(hash, argumentCount);
    for (index = 0; index < argumentCount; index++) {
        hash = canonical_type_hash_word(hash, (TZrUInt64)arguments[index].kind);
        switch (arguments[index].kind) {
            case ZR_CANONICAL_GENERIC_ARGUMENT_TYPE:
                hash = canonical_type_hash_word(hash, (TZrUInt64)arguments[index].data.typeId);
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT:
                hash = canonical_type_hash_word(hash, (TZrUInt64)arguments[index].data.constIntValue);
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER:
                hash = canonical_type_hash_word(
                        hash,
                        (TZrUInt64)arguments[index].data.constParameter.ownerSymbolId);
                hash = canonical_type_hash_word(
                        hash,
                        (TZrUInt64)arguments[index].data.constParameter.ordinal);
                break;
            default:
                return 0U;
        }
    }
    return hash;
}

static TZrUInt64 canonical_generic_parameter_hash(TZrSymbolId ownerSymbolId, TZrUInt32 ordinal) {
    TZrUInt64 hash = ZR_CANONICAL_TYPE_HASH_OFFSET;

    hash = canonical_type_hash_word(hash, (TZrUInt64)ZR_CANONICAL_TYPE_GENERIC_PARAMETER);
    hash = canonical_type_hash_word(hash, ownerSymbolId);
    return canonical_type_hash_word(hash, ordinal);
}

static TZrUInt64 canonical_array_hash(
        TZrTypeId elementTypeId,
        TZrUInt32 rank,
        EZrCanonicalArrayStorageKind storageKind) {
    TZrUInt64 hash = ZR_CANONICAL_TYPE_HASH_OFFSET;

    hash = canonical_type_hash_word(hash, (TZrUInt64)ZR_CANONICAL_TYPE_ARRAY);
    hash = canonical_type_hash_word(hash, elementTypeId);
    hash = canonical_type_hash_word(hash, rank);
    return canonical_type_hash_word(hash, (TZrUInt64)storageKind);
}

static TZrUInt64 canonical_type_list_hash(
        EZrCanonicalTypeKind kind,
        TZrTypeId definitionTypeId,
        const TZrTypeId *typeIds,
        TZrSize typeCount) {
    TZrUInt64 hash = ZR_CANONICAL_TYPE_HASH_OFFSET;
    TZrSize index;

    hash = canonical_type_hash_word(hash, (TZrUInt64)kind);
    hash = canonical_type_hash_word(hash, definitionTypeId);
    hash = canonical_type_hash_word(hash, typeCount);
    for (index = 0; index < typeCount; index++) {
        hash = canonical_type_hash_word(hash, typeIds[index]);
    }
    return hash;
}

static TZrUInt64 canonical_qualified_target_hash(
        EZrCanonicalTypeKind kind,
        TZrTypeId targetTypeId,
        TZrUInt32 qualifier) {
    TZrUInt64 hash = ZR_CANONICAL_TYPE_HASH_OFFSET;

    hash = canonical_type_hash_word(hash, (TZrUInt64)kind);
    hash = canonical_type_hash_word(hash, targetTypeId);
    return canonical_type_hash_word(hash, qualifier);
}

static TZrBool canonical_parameter_contract_is_valid(
        const SZrSemanticContext *context,
        const SZrCanonicalParameterContract *contract) {
    const SZrCanonicalTypeNode *typeNode;
    EZrCanonicalRefAccess requiredAccess;

    if (contract == ZR_NULL ||
        (TZrInt32)contract->passingForm < (TZrInt32)ZR_CANONICAL_PASSING_VALUE ||
        contract->passingForm > ZR_CANONICAL_PASSING_OUT) {
        return ZR_FALSE;
    }
    typeNode = ZrParser_CanonicalType_Find(context, contract->typeId);
    if (typeNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (contract->passingForm == ZR_CANONICAL_PASSING_VALUE) {
        return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION &&
               contract->entryInitialization == ZR_CANONICAL_ENTRY_INITIALIZED &&
               contract->exitInitialization == ZR_CANONICAL_EXIT_UNCHANGED &&
               contract->acceptsTemporary == ZR_TRUE &&
               contract->callSiteMarker == ZR_CANONICAL_CALL_SITE_NONE;
    }

    if (typeNode->kind != ZR_CANONICAL_TYPE_REF) {
        return ZR_FALSE;
    }
    requiredAccess = contract->passingForm == ZR_CANONICAL_PASSING_IN ||
                             contract->passingForm == ZR_CANONICAL_PASSING_REF_READONLY
                     ? ZR_CANONICAL_REF_READONLY
                     : ZR_CANONICAL_REF_WRITABLE;
    if (typeNode->data.refType.access != requiredAccess) {
        return ZR_FALSE;
    }

    switch (contract->passingForm) {
        case ZR_CANONICAL_PASSING_IN:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION &&
                   contract->entryInitialization == ZR_CANONICAL_ENTRY_INITIALIZED &&
                   contract->exitInitialization == ZR_CANONICAL_EXIT_UNCHANGED &&
                   contract->acceptsTemporary == ZR_TRUE &&
                   contract->callSiteMarker == ZR_CANONICAL_CALL_SITE_NONE;
        case ZR_CANONICAL_PASSING_REF:
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_CALLER &&
                   contract->entryInitialization == ZR_CANONICAL_ENTRY_INITIALIZED &&
                   contract->exitInitialization == ZR_CANONICAL_EXIT_UNCHANGED &&
                   contract->acceptsTemporary == ZR_FALSE &&
                   contract->callSiteMarker == ZR_CANONICAL_CALL_SITE_REF;
        case ZR_CANONICAL_PASSING_OUT:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION &&
                   contract->entryInitialization == ZR_CANONICAL_ENTRY_UNINITIALIZED &&
                   contract->exitInitialization == ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED &&
                   contract->acceptsTemporary == ZR_FALSE &&
                   contract->callSiteMarker == ZR_CANONICAL_CALL_SITE_OUT;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt64 canonical_parameter_contract_hash(
        TZrUInt64 hash,
        const SZrCanonicalParameterContract *contract) {
    hash = canonical_type_hash_word(hash, contract->typeId);
    hash = canonical_type_hash_word(hash, (TZrUInt64)contract->passingForm);
    hash = canonical_type_hash_word(hash, (TZrUInt64)contract->escapeUpperBound);
    hash = canonical_type_hash_word(hash, (TZrUInt64)contract->entryInitialization);
    hash = canonical_type_hash_word(hash, (TZrUInt64)contract->exitInitialization);
    hash = canonical_type_hash_word(hash, (TZrUInt64)contract->acceptsTemporary);
    return canonical_type_hash_word(hash, (TZrUInt64)contract->callSiteMarker);
}

static TZrBool canonical_parameter_contract_equal(
        const SZrCanonicalParameterContract *left,
        const SZrCanonicalParameterContract *right) {
    return left != ZR_NULL &&
           right != ZR_NULL &&
           left->typeId == right->typeId &&
           left->passingForm == right->passingForm &&
           left->escapeUpperBound == right->escapeUpperBound &&
           left->entryInitialization == right->entryInitialization &&
           left->exitInitialization == right->exitInitialization &&
           left->acceptsTemporary == right->acceptsTemporary &&
           left->callSiteMarker == right->callSiteMarker;
}

static TZrBool canonical_parameter_contract_array_equal(
        const SZrArray *stored,
        const SZrCanonicalParameterContract *contracts,
        TZrSize contractCount) {
    TZrSize index;

    if (stored == ZR_NULL || stored->length != contractCount) {
        return ZR_FALSE;
    }
    for (index = 0; index < contractCount; index++) {
        const SZrCanonicalParameterContract *storedContract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get((SZrArray *)stored, index);
        if (!canonical_parameter_contract_equal(storedContract, &contracts[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void canonical_type_store(SZrSemanticContext *context, const SZrCanonicalTypeNode *node) {
    TZrSize nodeIndex = context->canonicalTypes.length;

    ZrCore_Array_Push(context->state, &context->canonicalTypes, (TZrPtr)node);
    if (!ZrParser_CanonicalTypeIndex_Insert(context, nodeIndex)) {
        ZR_ASSERT(ZR_FALSE);
    }
}

static TZrTypeId canonical_type_intern_singleton(
        SZrSemanticContext *context,
        EZrCanonicalTypeKind kind) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL || (kind != ZR_CANONICAL_TYPE_ERROR && kind != ZR_CANONICAL_TYPE_NEVER)) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_qualified_target_hash(kind, ZR_SEMANTIC_ID_INVALID, 0U);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL && existing->kind == kind) {
            return existing->id;
        }
    }

    memset(&node, 0, sizeof(node));
    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = kind;
    node.structuralHash = structuralHash;
    canonical_type_store(context, &node);
    return node.id;
}

static TZrBool canonical_type_id_array_equal(
        const SZrArray *stored,
        const TZrTypeId *values,
        TZrSize valueCount) {
    TZrSize index;

    if (stored == ZR_NULL || stored->length != valueCount) {
        return ZR_FALSE;
    }
    for (index = 0; index < valueCount; index++) {
        const TZrTypeId *storedValue = (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)stored, index);
        if (storedValue == ZR_NULL || *storedValue != values[index]) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_type_ids_are_valid(
        const SZrSemanticContext *context,
        const TZrTypeId *typeIds,
        TZrSize typeCount) {
    TZrSize index;

    if (typeIds == ZR_NULL || typeCount == 0) {
        return ZR_FALSE;
    }
    for (index = 0; index < typeCount; index++) {
        if (ZrParser_CanonicalType_Find(context, typeIds[index]) == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_generic_arguments_are_valid(
        const SZrSemanticContext *context,
        const SZrCanonicalGenericArgument *arguments,
        TZrSize argumentCount) {
    TZrSize index;

    if (context == ZR_NULL || arguments == ZR_NULL || argumentCount == 0U) {
        return ZR_FALSE;
    }
    for (index = 0; index < argumentCount; index++) {
        if ((TZrInt32)arguments[index].kind <
                    (TZrInt32)ZR_CANONICAL_GENERIC_ARGUMENT_TYPE ||
            arguments[index].kind > ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER ||
            (arguments[index].kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE &&
             ZrParser_CanonicalType_Find(context, arguments[index].data.typeId) == ZR_NULL) ||
            (arguments[index].kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER &&
             arguments[index].data.constParameter.ownerSymbolId == ZR_SEMANTIC_ID_INVALID)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_generic_argument_array_equal(
        const SZrArray *stored,
        const SZrCanonicalGenericArgument *arguments,
        TZrSize argumentCount) {
    TZrSize index;

    if (stored == ZR_NULL || stored->length != argumentCount || arguments == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < argumentCount; index++) {
        const SZrCanonicalGenericArgument *storedArgument =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get((SZrArray *)stored, index);
        TZrBool equal;

        if (storedArgument == ZR_NULL || storedArgument->kind != arguments[index].kind) {
            return ZR_FALSE;
        }
        switch (storedArgument->kind) {
            case ZR_CANONICAL_GENERIC_ARGUMENT_TYPE:
                equal = storedArgument->data.typeId == arguments[index].data.typeId;
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT:
                equal = storedArgument->data.constIntValue == arguments[index].data.constIntValue;
                break;
            case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER:
                equal = storedArgument->data.constParameter.ownerSymbolId ==
                                arguments[index].data.constParameter.ownerSymbolId &&
                        storedArgument->data.constParameter.ordinal ==
                                arguments[index].data.constParameter.ordinal;
                break;
            default:
                equal = ZR_FALSE;
                break;
        }
        if (!equal) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static void canonical_type_init_generic_argument_array(
        SZrSemanticContext *context,
        SZrArray *destination,
        const SZrCanonicalGenericArgument *arguments,
        TZrSize argumentCount) {
    TZrSize index;

    ZrCore_Array_Init(
            context->state,
            destination,
            sizeof(SZrCanonicalGenericArgument),
            argumentCount);
    for (index = 0; index < argumentCount; index++) {
        SZrCanonicalGenericArgument argument = arguments[index];
        ZrCore_Array_Push(context->state, destination, &argument);
    }
}

static void canonical_type_init_id_array(
        SZrSemanticContext *context,
        SZrArray *destination,
        const TZrTypeId *typeIds,
        TZrSize typeCount) {
    TZrSize index;

    ZrCore_Array_Init(
            context->state,
            destination,
            sizeof(TZrTypeId),
            typeCount > 0U ? typeCount : ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (index = 0; index < typeCount; index++) {
        TZrTypeId typeId = typeIds[index];
        ZrCore_Array_Push(context->state, destination, &typeId);
    }
}

TZrTypeId ZrParser_CanonicalType_InternPrimitive(
        SZrSemanticContext *context,
        EZrValueType valueType) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        (TZrInt32)valueType < 0 ||
        valueType >= ZR_VALUE_TYPE_ENUM_MAX) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_primitive_hash(valueType);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            existing->data.primitive.valueType == valueType) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_PRIMITIVE;
    node.structuralHash = structuralHash;
    node.data.primitive.valueType = valueType;
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternNominal(
        SZrSemanticContext *context,
        SZrString *moduleIdentity,
        SZrString *name,
        TZrUInt32 definitionToken) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_nominal_hash(moduleIdentity, name, definitionToken);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_NOMINAL &&
            existing->data.nominal.definitionToken == definitionToken &&
            canonical_type_strings_equal(existing->data.nominal.moduleIdentity, moduleIdentity) &&
            (definitionToken != 0U ||
             canonical_type_strings_equal(existing->data.nominal.name, name))) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_NOMINAL;
    node.structuralHash = structuralHash;
    node.data.nominal.moduleIdentity = moduleIdentity;
    node.data.nominal.name = name;
    node.data.nominal.definitionToken = definitionToken;
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternGenericInstance(
        SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const TZrTypeId *argumentTypeIds,
        TZrSize argumentCount) {
    SZrArray arguments;
    TZrTypeId result;
    TZrSize index;

    if (!canonical_type_ids_are_valid(context, argumentTypeIds, argumentCount)) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    ZrCore_Array_Init(
            context->state,
            &arguments,
            sizeof(SZrCanonicalGenericArgument),
            argumentCount);
    for (index = 0; index < argumentCount; index++) {
        SZrCanonicalGenericArgument argument;
        argument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
        argument.data.typeId = argumentTypeIds[index];
        ZrCore_Array_Push(context->state, &arguments, &argument);
    }
    result = ZrParser_CanonicalType_InternGenericInstanceEx(
            context,
            definitionTypeId,
            (const SZrCanonicalGenericArgument *)arguments.head,
            arguments.length);
    ZrCore_Array_Free(context->state, &arguments);
    return result;
}

TZrTypeId ZrParser_CanonicalType_InternGenericInstanceEx(
        SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const SZrCanonicalGenericArgument *arguments,
        TZrSize argumentCount) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, definitionTypeId) == ZR_NULL ||
        !canonical_generic_arguments_are_valid(context, arguments, argumentCount)) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_generic_instance_hash(definitionTypeId, arguments, argumentCount);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE &&
            existing->data.genericInstance.definitionTypeId == definitionTypeId &&
            canonical_generic_argument_array_equal(
                    &existing->data.genericInstance.arguments,
                    arguments,
                    argumentCount)) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_GENERIC_INSTANCE;
    node.structuralHash = structuralHash;
    node.data.genericInstance.definitionTypeId = definitionTypeId;
    canonical_type_init_generic_argument_array(
            context,
            &node.data.genericInstance.arguments,
            arguments,
            argumentCount);
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternGenericParameter(
        SZrSemanticContext *context,
        TZrSymbolId ownerSymbolId,
        TZrUInt32 ordinal) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL || ownerSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_generic_parameter_hash(ownerSymbolId, ordinal);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_GENERIC_PARAMETER &&
            existing->data.genericParameter.ownerSymbolId == ownerSymbolId &&
            existing->data.genericParameter.ordinal == ordinal) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_GENERIC_PARAMETER;
    node.structuralHash = structuralHash;
    node.data.genericParameter.ownerSymbolId = ownerSymbolId;
    node.data.genericParameter.ordinal = ordinal;
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternArray(
        SZrSemanticContext *context,
        TZrTypeId elementTypeId,
        TZrUInt32 rank,
        EZrCanonicalArrayStorageKind storageKind) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, elementTypeId) == ZR_NULL ||
        rank == 0 ||
        (TZrInt32)storageKind < 0 ||
        storageKind > ZR_CANONICAL_ARRAY_STORAGE_NATIVE) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_array_hash(elementTypeId, rank, storageKind);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_ARRAY &&
            existing->data.array.elementTypeId == elementTypeId &&
            existing->data.array.rank == rank &&
            existing->data.array.storageKind == storageKind) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_ARRAY;
    node.structuralHash = structuralHash;
    node.data.array.elementTypeId = elementTypeId;
    node.data.array.rank = rank;
    node.data.array.storageKind = storageKind;
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternTuple(
        SZrSemanticContext *context,
        const TZrTypeId *elementTypeIds,
        TZrSize elementCount) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        (elementCount > 0U &&
         !canonical_type_ids_are_valid(context, elementTypeIds, elementCount))) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_type_list_hash(
            ZR_CANONICAL_TYPE_TUPLE,
            ZR_SEMANTIC_ID_INVALID,
            elementTypeIds,
            elementCount);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_TUPLE &&
            canonical_type_id_array_equal(
                    &existing->data.typeList.elementTypeIds,
                    elementTypeIds,
                    elementCount)) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_TUPLE;
    node.structuralHash = structuralHash;
    canonical_type_init_id_array(context, &node.data.typeList.elementTypeIds, elementTypeIds, elementCount);
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternUnion(
        SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const TZrTypeId *variantTypeIds,
        TZrSize variantCount) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, definitionTypeId) == ZR_NULL ||
        !canonical_type_ids_are_valid(context, variantTypeIds, variantCount)) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_type_list_hash(
            ZR_CANONICAL_TYPE_UNION,
            definitionTypeId,
            variantTypeIds,
            variantCount);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_UNION &&
            existing->data.unionType.definitionTypeId == definitionTypeId &&
            canonical_type_id_array_equal(
                    &existing->data.unionType.variantTypeIds,
                    variantTypeIds,
                    variantCount)) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_UNION;
    node.structuralHash = structuralHash;
    node.data.unionType.definitionTypeId = definitionTypeId;
    canonical_type_init_id_array(
            context,
            &node.data.unionType.variantTypeIds,
            variantTypeIds,
            variantCount);
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternError(SZrSemanticContext *context) {
    return canonical_type_intern_singleton(context, ZR_CANONICAL_TYPE_ERROR);
}

TZrTypeId ZrParser_CanonicalType_InternNever(SZrSemanticContext *context) {
    return canonical_type_intern_singleton(context, ZR_CANONICAL_TYPE_NEVER);
}

TZrTypeId ZrParser_CanonicalType_InternRef(
        SZrSemanticContext *context,
        TZrTypeId pointeeTypeId,
        EZrCanonicalRefAccess access) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, pointeeTypeId) == ZR_NULL ||
        (TZrInt32)access < 0 ||
        access > ZR_CANONICAL_REF_READONLY) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_qualified_target_hash(ZR_CANONICAL_TYPE_REF, pointeeTypeId, access);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_REF &&
            existing->data.refType.pointeeTypeId == pointeeTypeId &&
            existing->data.refType.access == access) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_REF;
    node.structuralHash = structuralHash;
    node.data.refType.pointeeTypeId = pointeeTypeId;
    node.data.refType.access = access;
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternOwner(
        SZrSemanticContext *context,
        TZrTypeId targetTypeId,
        EZrCanonicalOwnerKind ownerKind) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, targetTypeId) == ZR_NULL ||
        (TZrInt32)ownerKind < 0 ||
        ownerKind > ZR_CANONICAL_OWNER_ATOMIC_SHARED) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_qualified_target_hash(ZR_CANONICAL_TYPE_OWNER, targetTypeId, ownerKind);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_OWNER &&
            existing->data.owner.targetTypeId == targetTypeId &&
            existing->data.owner.ownerKind == ownerKind) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_OWNER;
    node.structuralHash = structuralHash;
    node.data.owner.targetTypeId = targetTypeId;
    node.data.owner.ownerKind = ownerKind;
    canonical_type_store(context, &node);
    return node.id;
}

static TZrTypeId canonical_type_intern_target_wrapper(
        SZrSemanticContext *context,
        TZrTypeId targetTypeId,
        EZrCanonicalTypeKind kind) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash;

    if (context == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, targetTypeId) == ZR_NULL ||
        (kind != ZR_CANONICAL_TYPE_READONLY_VIEW && kind != ZR_CANONICAL_TYPE_NULLABLE)) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_qualified_target_hash(kind, targetTypeId, 0U);
    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == kind &&
            existing->data.target.targetTypeId == targetTypeId) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = kind;
    node.structuralHash = structuralHash;
    node.data.target.targetTypeId = targetTypeId;
    canonical_type_store(context, &node);
    return node.id;
}

TZrTypeId ZrParser_CanonicalType_InternReadonlyView(
        SZrSemanticContext *context,
        TZrTypeId targetTypeId) {
    return canonical_type_intern_target_wrapper(context, targetTypeId, ZR_CANONICAL_TYPE_READONLY_VIEW);
}

TZrTypeId ZrParser_CanonicalType_InternNullable(
        SZrSemanticContext *context,
        TZrTypeId targetTypeId) {
    return canonical_type_intern_target_wrapper(context, targetTypeId, ZR_CANONICAL_TYPE_NULLABLE);
}

TZrTypeId ZrParser_CanonicalType_InternFunction(
        SZrSemanticContext *context,
        const SZrCanonicalParameterContract *parameterContracts,
        TZrSize parameterCount,
        TZrTypeId returnTypeId,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags) {
    SZrCanonicalTypeNode node;
    TZrSize index;
    TZrUInt64 structuralHash = ZR_CANONICAL_TYPE_HASH_OFFSET;

    if (context == ZR_NULL ||
        (parameterCount > 0 && parameterContracts == ZR_NULL) ||
        ZrParser_CanonicalType_Find(context, returnTypeId) == ZR_NULL ||
        (TZrInt32)receiverEffect < 0 ||
        receiverEffect > ZR_CANONICAL_RECEIVER_MUTABLE ||
        (effectFlags & ~(ZR_CANONICAL_CALLABLE_EFFECT_THROWS |
                         ZR_CANONICAL_CALLABLE_EFFECT_ASYNC |
                         ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR)) != 0U) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    structuralHash = canonical_type_hash_word(structuralHash, (TZrUInt64)ZR_CANONICAL_TYPE_FUNCTION);
    structuralHash = canonical_type_hash_word(structuralHash, parameterCount);
    for (index = 0; index < parameterCount; index++) {
        if (!canonical_parameter_contract_is_valid(context, &parameterContracts[index])) {
            return ZR_SEMANTIC_ID_INVALID;
        }
        structuralHash = canonical_parameter_contract_hash(structuralHash, &parameterContracts[index]);
    }
    structuralHash = canonical_type_hash_word(structuralHash, returnTypeId);
    structuralHash = canonical_type_hash_word(structuralHash, (TZrUInt64)receiverEffect);
    structuralHash = canonical_type_hash_word(structuralHash, effectFlags);

    for (index = ZrParser_CanonicalTypeIndex_First(context, structuralHash);
         index != ZR_MAX_SIZE;
         index = ZrParser_CanonicalTypeIndex_Next(context, index)) {
        const SZrCanonicalTypeNode *existing = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (existing != ZR_NULL &&
            existing->structuralHash == structuralHash &&
            existing->kind == ZR_CANONICAL_TYPE_FUNCTION &&
            existing->data.function.returnTypeId == returnTypeId &&
            existing->data.function.receiverEffect == receiverEffect &&
            existing->data.function.effectFlags == effectFlags &&
            canonical_parameter_contract_array_equal(
                    &existing->data.function.parameterContracts,
                    parameterContracts,
                    parameterCount)) {
            return existing->id;
        }
    }

    node.id = ZrParser_Semantic_ReserveTypeId(context);
    node.kind = ZR_CANONICAL_TYPE_FUNCTION;
    node.structuralHash = structuralHash;
    node.data.function.returnTypeId = returnTypeId;
    node.data.function.receiverEffect = receiverEffect;
    node.data.function.effectFlags = effectFlags;
    ZrCore_Array_Init(
            context->state,
            &node.data.function.parameterContracts,
            sizeof(SZrCanonicalParameterContract),
            parameterCount > 0 ? parameterCount : ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (index = 0; index < parameterCount; index++) {
        SZrCanonicalParameterContract contract = parameterContracts[index];
        ZrCore_Array_Push(context->state, &node.data.function.parameterContracts, &contract);
    }
    canonical_type_store(context, &node);
    return node.id;
}
