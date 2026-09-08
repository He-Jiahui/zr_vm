/* Stable native provider identity and call-binding resolution. */

#include "native_binding_internal.h"
#include "zr_vm_library/native_binding_call_binding.h"

#include "zr_vm_core/hash.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_common/zr_ast_constants.h"

#include <string.h>

#ifndef ZR_MEMBER_PARAMETER_COUNT_UNKNOWN
#define ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ((TZrUInt32)-1)
#endif

static const TZrByte CZrNativeCallBindingSignaturePrefix[] = {
        'z', 'r', '.', 'm', 'd', '.', 'n', 'a', 't', 'i', 'v', 'e',
        '.', 's', 'i', 'g', '.', 'v', '1', '\0'
};

static TZrSize native_cb_string_length(const TZrChar *value) {
    return value != ZR_NULL ? strlen(value) : 0u;
}

static TZrBool native_cb_size_add(TZrSize *size, TZrSize increment) {
    if (size == ZR_NULL || increment > ZR_MAX_SIZE - *size) {
        return ZR_FALSE;
    }
    *size += increment;
    return ZR_TRUE;
}

static TZrBool native_cb_size_string(TZrSize *size, const TZrChar *value) {
    TZrSize length = native_cb_string_length(value);
    if (length > (TZrSize)UINT32_MAX ||
        !native_cb_size_add(size, sizeof(TZrUInt32)) ||
        !native_cb_size_add(size, length)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void native_cb_write_u32(TZrByte *buffer, TZrSize *offset, TZrUInt32 value) {
    buffer[(*offset)++] = (TZrByte)(value & 0xffu);
    buffer[(*offset)++] = (TZrByte)((value >> 8u) & 0xffu);
    buffer[(*offset)++] = (TZrByte)((value >> 16u) & 0xffu);
    buffer[(*offset)++] = (TZrByte)((value >> 24u) & 0xffu);
}

static void native_cb_write_string(TZrByte *buffer, TZrSize *offset, const TZrChar *value) {
    TZrSize length = native_cb_string_length(value);
    native_cb_write_u32(buffer, offset, (TZrUInt32)length);
    if (length != 0u) {
        memcpy(buffer + *offset, value, length);
        *offset += length;
    }
}

static TZrUInt64 native_cb_signature_hash(const TZrChar *ownerName,
                                          const TZrChar *memberName,
                                          const TZrChar *returnTypeName,
                                          TZrUInt32 memberType,
                                          TZrUInt32 parameterCount,
                                          TZrUInt32 minArgumentCount,
                                          const ZrLibParameterDescriptor *parameters,
                                          TZrSize parameterLength) {
    TZrSize size = 0u;
    TZrSize offset = 0u;
    TZrByte *buffer;
    TZrUInt64 hash;

    if (memberName == ZR_NULL) {
        return 0u;
    }
    if (!native_cb_size_add(&size, 2u * sizeof(TZrUInt32)) ||
        !native_cb_size_string(&size, ownerName) ||
        !native_cb_size_string(&size, memberName) ||
        !native_cb_size_string(&size, returnTypeName) ||
        /* The native signature writer always includes the arity fields,
         * including the unknown-arity sentinel, before parameter types. */
        !native_cb_size_add(&size, 3u * sizeof(TZrUInt32))) {
        return 0u;
    }
    if (parameterLength != 0u && parameters == ZR_NULL) {
        return 0u;
    }
    for (TZrSize index = 0u; index < parameterLength; ++index) {
        if (!native_cb_size_string(&size, parameters[index].typeName)) {
            return 0u;
        }
    }
    buffer = (TZrByte *)malloc(size);
    if (buffer == ZR_NULL) {
        return 0u;
    }
    native_cb_write_u32(buffer, &offset, (TZrUInt32)ZR_METADATA_SIGNATURE_NODE_METHOD_SIG);
    native_cb_write_u32(buffer, &offset, memberType);
    native_cb_write_string(buffer, &offset, ownerName);
    native_cb_write_string(buffer, &offset, memberName);
    native_cb_write_string(buffer, &offset, returnTypeName);
    native_cb_write_u32(buffer, &offset, parameterCount);
    native_cb_write_u32(buffer, &offset, minArgumentCount);
    native_cb_write_u32(buffer, &offset, (TZrUInt32)parameterLength);
    for (TZrSize index = 0u; index < parameterLength; ++index) {
        native_cb_write_string(buffer, &offset,
                                parameters != ZR_NULL ? parameters[index].typeName : ZR_NULL);
    }
    hash = ZrCore_Hash_CreateStable64WithPrefix(CZrNativeCallBindingSignaturePrefix,
                                                sizeof(CZrNativeCallBindingSignaturePrefix),
                                                buffer, offset);
    free(buffer);
    return hash;
}

static TZrUInt32 native_cb_method_parameter_count(const ZrLibMethodDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    }
    if (descriptor->parameters != ZR_NULL ||
        (descriptor->minArgumentCount == 0u && descriptor->maxArgumentCount == 0u)) {
        return (TZrUInt32)descriptor->parameterCount;
    }
    return descriptor->minArgumentCount == descriptor->maxArgumentCount
                   ? descriptor->minArgumentCount
                   : ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
}

static TZrUInt32 native_cb_function_parameter_count(const ZrLibFunctionDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    }
    if (descriptor->parameters != ZR_NULL ||
        (descriptor->minArgumentCount == 0u && descriptor->maxArgumentCount == 0u)) {
        return (TZrUInt32)descriptor->parameterCount;
    }
    return descriptor->minArgumentCount == descriptor->maxArgumentCount
                   ? descriptor->minArgumentCount
                   : ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
}

static TZrUInt32 native_cb_meta_parameter_count(const ZrLibMetaMethodDescriptor *descriptor) {
    if (descriptor == ZR_NULL) return ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    if (descriptor->parameters != ZR_NULL ||
        (descriptor->minArgumentCount == 0u && descriptor->maxArgumentCount == 0u))
        return (TZrUInt32)descriptor->parameterCount;
    return descriptor->minArgumentCount == descriptor->maxArgumentCount
                   ? descriptor->minArgumentCount : ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
}

static TZrBool native_cb_advance_rid(TZrUInt32 *rid, TZrSize count) {
    if (count > (TZrSize)ZR_METADATA_TOKEN_RID_MASK ||
        *rid > ZR_METADATA_TOKEN_RID_MASK - count) return ZR_FALSE;
    *rid += (TZrUInt32)count;
    return ZR_TRUE;
}

static const void *native_cb_entry_descriptor(const ZrLibBindingEntry *entry) {
    if (entry == ZR_NULL) return ZR_NULL;
    switch (entry->bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION: return entry->descriptor.functionDescriptor;
        case ZR_LIB_RESOLVED_BINDING_METHOD: return entry->descriptor.methodDescriptor;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD: return entry->descriptor.metaMethodDescriptor;
        default: return ZR_NULL;
    }
}

static void native_cb_assign_identity(const ZrLibModuleDescriptor *module,
                                      const ZrLibTypeDescriptor *type,
                                      EZrLibResolvedBindingKind kind,
                                      const void *descriptor,
                                      TZrMetadataToken *metadataToken,
                                      TZrMetadataToken *signatureToken,
                                      TZrUInt64 *signatureHash) {
    TZrUInt32 memberRid = 1u;
    TZrUInt32 signatureRid = 1u;
    if (module == ZR_NULL || descriptor == ZR_NULL || metadataToken == ZR_NULL ||
        signatureToken == ZR_NULL || signatureHash == ZR_NULL) {
        return;
    }
    *metadataToken = 0u;
    *signatureToken = 0u;
    *signatureHash = 0u;
    if ((module->functionCount != 0u && module->functions == ZR_NULL) ||
        (module->typeCount != 0u && module->types == ZR_NULL) ||
        module->functionCount > ZR_METADATA_TOKEN_RID_MASK ||
        module->typeCount > ZR_METADATA_TOKEN_RID_MASK) return;

    /* Module-level functions precede constants, links and type members. */
    for (TZrSize index = 0u; index < module->functionCount; ++index) {
        const ZrLibFunctionDescriptor *candidate = &module->functions[index];
        if (candidate->name == ZR_NULL) continue;
        if (kind == ZR_LIB_RESOLVED_BINDING_FUNCTION && candidate == descriptor) {
            TZrUInt32 parameterCount = native_cb_function_parameter_count(candidate);
            *metadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid);
            *signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRid);
            *signatureHash = native_cb_signature_hash(module->moduleName,
                                                      candidate->name,
                                                      candidate->returnTypeName,
                                                      ZR_AST_CONSTANT_CLASS_METHOD,
                                                      parameterCount,
                                                      candidate->minArgumentCount,
                                                      candidate->parameters,
                                                      candidate->parameters != ZR_NULL ? candidate->parameterCount : 0u);
            return;
        }
        if (!native_cb_advance_rid(&memberRid, 1u) ||
            !native_cb_advance_rid(&signatureRid, 1u)) return;
    }
    if ((module->constantCount != 0u && module->constants == ZR_NULL) ||
        (module->moduleLinkCount != 0u && module->moduleLinks == ZR_NULL) ||
        module->constantCount > ZR_METADATA_TOKEN_RID_MASK ||
        module->moduleLinkCount > ZR_METADATA_TOKEN_RID_MASK) return;
    for (TZrSize index = 0u; index < module->constantCount; ++index) {
        if (module->constants[index].name != ZR_NULL &&
            (!native_cb_advance_rid(&memberRid, 1u) ||
             !native_cb_advance_rid(&signatureRid, 1u))) return;
    }
    for (TZrSize index = 0u; index < module->moduleLinkCount; ++index) {
        if (module->moduleLinks[index].name != ZR_NULL && module->moduleLinks[index].moduleName != ZR_NULL &&
            (!native_cb_advance_rid(&memberRid, 1u) ||
             !native_cb_advance_rid(&signatureRid, 1u))) return;
    }

    for (TZrSize typeIndex = 0u; typeIndex < module->typeCount; ++typeIndex) {
        const ZrLibTypeDescriptor *typeDescriptor = &module->types[typeIndex];
        if (typeDescriptor->name == ZR_NULL) continue;
        if ((typeDescriptor->methodCount != 0u && typeDescriptor->methods == ZR_NULL) ||
            typeDescriptor->methodCount > ZR_METADATA_TOKEN_RID_MASK) return;
        TZrUInt32 memberType = typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                        ? ZR_AST_CONSTANT_STRUCT_METHOD
                                        : ZR_AST_CONSTANT_CLASS_METHOD;
        if (!native_cb_advance_rid(&memberRid, 1u) ||
            !native_cb_advance_rid(&signatureRid, 1u)) return;
        for (TZrSize methodIndex = 0u; methodIndex < typeDescriptor->methodCount; ++methodIndex) {
            const ZrLibMethodDescriptor *candidate = &typeDescriptor->methods[methodIndex];
            if (candidate->name == ZR_NULL) continue;
            if (kind == ZR_LIB_RESOLVED_BINDING_METHOD && candidate == descriptor && type == typeDescriptor) {
                TZrUInt32 parameterCount = native_cb_method_parameter_count(candidate);
                *metadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid);
                *signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRid);
                *signatureHash = native_cb_signature_hash(typeDescriptor->name,
                                                          candidate->name,
                                                          candidate->returnTypeName,
                                                          memberType,
                                                          parameterCount,
                                                          candidate->minArgumentCount,
                                                          candidate->parameters,
                                                          candidate->parameters != ZR_NULL ? candidate->parameterCount : 0u);
                return;
            }
            if (!native_cb_advance_rid(&memberRid, 1u) ||
                !native_cb_advance_rid(&signatureRid, 1u)) return;
        }
    }

    /* Preserve existing module/function/method RIDs; meta identities append
     * after the complete imported member sequence. */
    for (TZrSize typeIndex = 0u; typeIndex < module->typeCount; ++typeIndex) {
        const ZrLibTypeDescriptor *typeDescriptor = &module->types[typeIndex];
        if (typeDescriptor->name == ZR_NULL) continue;
        if ((typeDescriptor->metaMethodCount != 0u && typeDescriptor->metaMethods == ZR_NULL) ||
            typeDescriptor->metaMethodCount > ZR_METADATA_TOKEN_RID_MASK) return;
        for (TZrSize metaIndex = 0u; metaIndex < typeDescriptor->metaMethodCount; ++metaIndex) {
            const ZrLibMetaMethodDescriptor *candidate = &typeDescriptor->metaMethods[metaIndex];
            const TZrChar *name;
            if (candidate->metaType >= ZR_META_ENUM_MAX) continue;
            name = candidate->metaType == ZR_META_CONSTRUCTOR ? "__constructor" : CZrMetaName[candidate->metaType];
            if (name == ZR_NULL) continue;
            if (kind == ZR_LIB_RESOLVED_BINDING_META_METHOD && candidate == descriptor && type == typeDescriptor) {
                *metadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid);
                *signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRid);
                *signatureHash = native_cb_signature_hash(typeDescriptor->name, name,
                        candidate->returnTypeName,
                        typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                ? ZR_AST_CONSTANT_STRUCT_META_FUNCTION : ZR_AST_CONSTANT_CLASS_META_FUNCTION,
                        native_cb_meta_parameter_count(candidate), candidate->minArgumentCount,
                        candidate->parameters, candidate->parameters != ZR_NULL ? candidate->parameterCount : 0u);
                return;
            }
            if (!native_cb_advance_rid(&memberRid, 1u) ||
                !native_cb_advance_rid(&signatureRid, 1u)) return;
        }
    }
}

TZrBool native_registry_assign_call_binding_identity(const ZrLibModuleDescriptor *moduleDescriptor,
        const ZrLibTypeDescriptor *typeDescriptor,
        EZrLibResolvedBindingKind bindingKind,
        const void *descriptor,
        ZrLibBindingEntry *entry) {
    if (entry == ZR_NULL) return ZR_FALSE;
    entry->metadataToken = 0u;
    entry->signatureToken = 0u;
    entry->signatureHash = 0u;
    entry->moduleSignatureHash = ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(moduleDescriptor);
    native_cb_assign_identity(moduleDescriptor, typeDescriptor, bindingKind, descriptor,
                              &entry->metadataToken, &entry->signatureToken, &entry->signatureHash);
    return entry->metadataToken != 0u && entry->signatureToken != 0u &&
           entry->signatureHash != 0u && entry->moduleSignatureHash != 0u;
}

TZrBool ZrLibrary_NativeCallBinding_GetDescriptorContract(
        const ZrLibModuleDescriptor *module,
        const ZrLibTypeDescriptor *type,
        EZrNativeCallBindingDescriptorKind kind,
        const void *descriptor,
        SZrCallBindingContract *outContract) {
    EZrLibResolvedBindingKind resolvedKind;
    TZrMetadataToken metadataToken = 0u;
    TZrMetadataToken signatureToken = 0u;
    TZrUInt64 signatureHash = 0u;
    TZrUInt64 moduleHash;

    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (module == ZR_NULL || descriptor == ZR_NULL || outContract == ZR_NULL ||
        module->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (kind) {
        case ZR_NATIVE_CALL_BINDING_FUNCTION:
            resolvedKind = ZR_LIB_RESOLVED_BINDING_FUNCTION;
            break;
        case ZR_NATIVE_CALL_BINDING_METHOD:
            resolvedKind = ZR_LIB_RESOLVED_BINDING_METHOD;
            break;
        case ZR_NATIVE_CALL_BINDING_META_METHOD:
            resolvedKind = ZR_LIB_RESOLVED_BINDING_META_METHOD;
            break;
        default:
            return ZR_FALSE;
    }
    native_cb_assign_identity(module, type, resolvedKind, descriptor,
                              &metadataToken, &signatureToken, &signatureHash);
    moduleHash = ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(module);
    if (metadataToken == 0u || signatureToken == 0u || signatureHash == 0u || moduleHash == 0u) {
        return ZR_FALSE;
    }
    outContract->bindingKind = ZR_CALL_BINDING_DIRECT;
    outContract->targetMetadataToken = metadataToken;
    outContract->signatureToken = signatureToken;
    outContract->signatureHash = signatureHash;
    outContract->moduleSignatureHash = moduleHash;
    outContract->ownerTypeToken = 0u;
    outContract->layoutVersion = 0u;
    outContract->layoutHash = 0u;
    outContract->dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    outContract->operation = kind == ZR_NATIVE_CALL_BINDING_META_METHOD
                                     ? ZR_CALL_BINDING_OPERATION_META
                                     : ZR_CALL_BINDING_OPERATION_CALL;
    if (kind == ZR_NATIVE_CALL_BINDING_METHOD &&
        ((const ZrLibMethodDescriptor *)descriptor)->propertyName != ZR_NULL &&
        ((const ZrLibMethodDescriptor *)descriptor)->propertyReferenceAccess != ZR_LIB_REFERENCE_ACCESS_NONE)
        outContract->operation = ZR_CALL_BINDING_OPERATION_GET;
    if (type != ZR_NULL) {
        for (TZrSize index = 0u; index < module->typeCount; ++index) {
            if (&module->types[index] != type) continue;
            if (index >= ZR_METADATA_TOKEN_RID_MASK) return ZR_FALSE;
            outContract->ownerTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, index + 1u);
            outContract->layoutVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
            outContract->layoutHash = native_registry_call_binding_type_hash(type);
            if (outContract->layoutHash == 0u) return ZR_FALSE;
            break;
        }
    }
    return ZR_TRUE;
}

EZrCallBindingStatus ZrLibrary_NativeRegistry_ResolveCallBinding(
        SZrState *state,
        const SZrCallBindingContract *contract,
        SZrCallBindingTarget *outTarget,
        SZrCallBindingDiagnostic *diagnostic) {
    ZrLibrary_NativeRegistryState *registry;
    ZrLibBindingEntry *selected = ZR_NULL;
    TZrUInt32 matches = 0u;
    TZrBool moduleFound = ZR_FALSE;
    TZrBool identityFound = ZR_FALSE;
    if (outTarget != ZR_NULL) memset(outTarget, 0, sizeof(*outTarget));
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (state == ZR_NULL || state->global == ZR_NULL || contract == ZR_NULL || outTarget == ZR_NULL) {
        return ZR_CALL_BINDING_INVALID_ARGUMENT;
    }
    if (ZrCore_CallBinding_CheckContract(contract, diagnostic) != ZR_CALL_BINDING_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status : ZR_CALL_BINDING_MISSING_CONTRACT;
    }
    registry = native_registry_get(state->global);
    if (registry == ZR_NULL || !registry->bindingEntries.isValid) {
        return ZR_CALL_BINDING_TARGET_NOT_FOUND;
    }
    /* A contract carries only the provider hash.  Materialize a matching
     * registered provider before consulting closure entries; this keeps link
     * resolution independent from export/member names. */
    if (registry->moduleRecords.isValid) {
        for (TZrSize index = 0u; index < registry->moduleRecords.length; ++index) {
            const ZrLibRegisteredModuleRecord *record =
                    (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(
                            &registry->moduleRecords, index);
            if (record == ZR_NULL || record->descriptor == ZR_NULL ||
                ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(record->descriptor) !=
                        contract->moduleSignatureHash) {
                continue;
            }
            if (moduleFound) {
                if (diagnostic != ZR_NULL) {
                    diagnostic->status = ZR_CALL_BINDING_AMBIGUOUS_TARGET;
                    diagnostic->expected = contract->moduleSignatureHash;
                }
                return ZR_CALL_BINDING_AMBIGUOUS_TARGET;
            }
            moduleFound = ZR_TRUE;
            if (record->moduleName != ZR_NULL) {
                (void)native_registry_resolve_loaded_module(state, registry, record->moduleName);
            }
        }
    }
    for (TZrSize index = 0u; index < registry->bindingEntries.length; ++index) {
        ZrLibBindingEntry *entry = (ZrLibBindingEntry *)ZrCore_Array_Get(&registry->bindingEntries, index);
        if (entry == ZR_NULL || entry->moduleSignatureHash != contract->moduleSignatureHash) {
            continue;
        }
        moduleFound = ZR_TRUE;
        if (entry->metadataToken != contract->targetMetadataToken ||
            entry->signatureToken != contract->signatureToken) {
            continue;
        }
        identityFound = ZR_TRUE;
        if (entry->signatureHash != contract->signatureHash) continue;
        if (contract->bindingKind == ZR_CALL_BINDING_VIRTUAL ||
            contract->bindingKind == ZR_CALL_BINDING_INTERFACE) {
            /* Native providers materialize fixed descriptors; dispatch slots
             * are validated by the receiver prototype at the call site. */
            if (contract->dispatchSlot == ZR_CALL_BINDING_SLOT_NONE) {
                if (diagnostic != ZR_NULL) diagnostic->status = ZR_CALL_BINDING_INVALID_SLOT;
                return ZR_CALL_BINDING_INVALID_SLOT;
            }
        }
        selected = entry;
        ++matches;
    }
    if (matches == 0u) {
        EZrCallBindingStatus status = identityFound ? ZR_CALL_BINDING_SIGNATURE_MISMATCH
                                                     : (moduleFound ? ZR_CALL_BINDING_TARGET_NOT_FOUND
                                                                    : ZR_CALL_BINDING_MODULE_MISMATCH);
        if (diagnostic != ZR_NULL) {
            diagnostic->status = status;
            diagnostic->expected = contract->signatureHash;
        }
        return status;
    }
    if (matches != 1u) return ZR_CALL_BINDING_AMBIGUOUS_TARGET;
    {
        SZrCallBindingContract actual;
        EZrCallBindingStatus status;
        if (!ZrLibrary_NativeCallBinding_GetDescriptorContract(selected->moduleDescriptor,
                selected->typeDescriptor, (EZrNativeCallBindingDescriptorKind)selected->bindingKind,
                native_cb_entry_descriptor(selected), &actual)) return ZR_CALL_BINDING_MISSING_CONTRACT;
        status = ZrCore_CallBinding_CompareContracts(contract, &actual, diagnostic);
        if (status != ZR_CALL_BINDING_OK) return status;
    }
    outTarget->targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    outTarget->native.function = selected->closure != ZR_NULL ? selected->closure->nativeFunction : ZR_NULL;
    outTarget->targetGeneration = selected->closure != ZR_NULL ? selected->closure->callBindingGeneration : 0u;
    outTarget->callableObject = selected->closure != ZR_NULL
                                        ? ZR_CAST_RAW_OBJECT_AS_SUPER(selected->closure)
                                        : ZR_NULL;
    if (outTarget->native.function == ZR_NULL || outTarget->callableObject == ZR_NULL) {
        memset(outTarget, 0, sizeof(*outTarget));
        return ZR_CALL_BINDING_TARGET_NOT_FOUND;
    }
    outTarget->dispatchSlotCount = 1u;
    outTarget->ownerLayoutGeneration = selected->ownerPrototype != ZR_NULL
                                               ? selected->ownerPrototype->layoutGeneration
                                               : 0u;
    return ZR_CALL_BINDING_OK;
}

TZrBool native_registry_link_call_binding(SZrState *state, SZrFunction *function,
        TZrUInt32 cacheIndex, SZrCallBindingDiagnostic *diagnostic, TZrPtr userData) {
    ZrLibrary_NativeRegistryState *registry = (ZrLibrary_NativeRegistryState *)userData;
    SZrFunctionCallSiteCacheEntry *cache;
    SZrCallBindingTarget target;
    ZrLibBindingEntry *entry;
    SZrObjectPrototype *owner;
    EZrCallBindingStatus status;
    if (state == ZR_NULL || function == ZR_NULL || cacheIndex >= function->callSiteCacheLength)
        return ZR_FALSE;
    cache = &function->callSiteCaches[cacheIndex];
    status = ZrLibrary_NativeRegistry_ResolveCallBinding(state, &cache->binding.contract, &target, diagnostic);
    if (status != ZR_CALL_BINDING_OK) {
        if (status == ZR_CALL_BINDING_MODULE_MISMATCH && registry != ZR_NULL &&
            registry->hostCallBindingModuleResolver != ZR_NULL)
            return registry->hostCallBindingModuleResolver(state, function, cacheIndex, diagnostic,
                    registry->hostCallBindingModuleResolverUserData);
        ZrCore_CallBinding_Invalidate(&cache->binding);
        if (diagnostic != ZR_NULL) {
            diagnostic->status = status;
            diagnostic->targetMetadataToken = cache->binding.contract.targetMetadataToken;
            diagnostic->instructionIndex = cache->instructionIndex;
        }
        return ZR_FALSE;
    }
    entry = native_registry_find_binding(registry, (SZrClosureNative *)target.callableObject);
    owner = entry != ZR_NULL ? entry->ownerPrototype : ZR_NULL;
    if (cache->binding.contract.ownerTypeToken != 0u && owner == ZR_NULL) return ZR_FALSE;
    cache->binding.target = target;
    cache->binding.generation = function->callBindingGeneration;
    if (owner != ZR_NULL) {
        SZrFunctionCallSitePicSlot *guard = &cache->picSlots[0];
        memset(guard, 0, sizeof(*guard));
        guard->cachedOwnerPrototype = owner;
        guard->cachedOwnerShapeId = owner->shapeId;
        guard->cachedOwnerShapeGeneration = owner->shapeGeneration;
        guard->cachedOwnerVersion = owner->super.memberVersion;
        guard->cachedReceiverPrototype = owner;
        guard->cachedReceiverShapeId = owner->shapeId;
        guard->cachedReceiverShapeGeneration = owner->shapeGeneration;
        guard->cachedReceiverVersion = owner->super.memberVersion;
        guard->cachedDescriptorIndex = ZR_CALL_BINDING_SLOT_NONE;
        guard->cachedIsStatic = entry->bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD &&
                                entry->descriptor.methodDescriptor->isStatic;
        for (TZrUInt32 index = 0u; index < owner->memberDescriptorCount; ++index) {
            if (ZR_CAST_RAW_OBJECT_AS_SUPER(owner->memberDescriptors[index].methodFunction) == target.callableObject) {
                guard->cachedDescriptorIndex = index;
                break;
            }
        }
        cache->picSlotCount = 1u;
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), ZR_CAST_RAW_OBJECT_AS_SUPER(owner));
    }
    ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), target.callableObject);
    return ZR_TRUE;
}

TZrBool ZrLibrary_NativeRegistry_GetCallBindingIdentity(
        SZrGlobalState *global,
        struct SZrClosureNative *closure,
        SZrCallBindingContract *outContract) {
    ZrLibrary_NativeRegistryState *registry;
    ZrLibBindingEntry *entry;
    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (global == ZR_NULL || closure == ZR_NULL || outContract == ZR_NULL) {
        return ZR_FALSE;
    }
    registry = native_registry_get(global);
    entry = native_registry_find_binding(registry, closure);
    if (entry == ZR_NULL || entry->metadataToken == 0u || entry->signatureToken == 0u ||
        entry->signatureHash == 0u || entry->moduleSignatureHash == 0u) {
        return ZR_FALSE;
    }
    return ZrLibrary_NativeCallBinding_GetDescriptorContract(entry->moduleDescriptor,
            entry->typeDescriptor, (EZrNativeCallBindingDescriptorKind)entry->bindingKind,
            native_cb_entry_descriptor(entry), outContract);
}
