#include "compiler_call_binding.h"

#include "compiler_internal.h"
#include "compile_expression_internal.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "compiler_native_call_binding.h"
#include "compiler_typed_call_binding.h"

static TZrUInt32 binding_next_rid(const SZrFunction *function, TZrUInt32 table);

TZrUInt64 compiler_typed_call_signature_hash(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature) {
    SZrFunction temporary;
    SZrFunctionMetadataParameter *parameters;
    SZrFunctionTypedLocalBinding *locals;
    TZrUInt64 hash;
    TZrSize count;
    if (compiler == ZR_NULL || signature == ZR_NULL ||
        signature->parameterTypes.length != signature->parameterPassingModes.length ||
        signature->parameterTypes.length > UINT16_MAX) return 0u;
    memset(&temporary, 0, sizeof(temporary));
    count = signature->parameterTypes.length;
    parameters = count == 0u ? ZR_NULL : ZrCore_Memory_RawMallocWithType(compiler->state->global,
            sizeof(*parameters) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    locals = count == 0u ? ZR_NULL : ZrCore_Memory_RawMallocWithType(compiler->state->global,
            sizeof(*locals) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (count != 0u && (parameters == ZR_NULL || locals == ZR_NULL)) {
        hash = 0u;
        goto cleanup;
    }
    if (count != 0u) {
        memset(parameters, 0, sizeof(*parameters) * count);
        memset(locals, 0, sizeof(*locals) * count);
    }
    temporary.parameterCount = (TZrUInt16)count;
    temporary.parameterMetadataCount = (TZrUInt32)count;
    temporary.parameterMetadata = parameters;
    temporary.hasCallableReturnType = ZR_TRUE;
    compiler_typed_type_ref_from_inferred(&temporary.callableReturnType, &signature->returnType);
    temporary.typedLocalBindings = locals;
    temporary.typedLocalBindingLength = (TZrUInt32)count;
    for (TZrSize index = 0u; index < count; ++index) {
        const SZrInferredType *type = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)&signature->parameterTypes, index);
        const EZrParameterPassingMode *mode = (const EZrParameterPassingMode *)ZrCore_Array_Get(
                (SZrArray *)&signature->parameterPassingModes, index);
        if (type == ZR_NULL || mode == ZR_NULL) {
            hash = 0u;
            goto cleanup;
        }
        compiler_typed_type_ref_from_inferred(&parameters[index].type, type);
        parameters[index].name = ZR_NULL;
        locals[index].stackSlot = (TZrUInt32)index;
        locals[index].roleFlags = (*mode == ZR_PARAMETER_PASSING_MODE_IN)
                ? ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN
                : (*mode == ZR_PARAMETER_PASSING_MODE_REF)
                ? ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF
                : (*mode == ZR_PARAMETER_PASSING_MODE_OUT)
                ? ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_OUT
                : ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    }
    hash = ZrCore_CallBinding_FunctionSignatureHash(&temporary);
cleanup:
    if (parameters != ZR_NULL) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            parameters, sizeof(*parameters) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (locals != ZR_NULL) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            locals, sizeof(*locals) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return hash;
}

static TZrBool binding_publish_typed_signature(SZrCompilerState *compiler,
                                               SZrFunction *function,
                                               SZrFunctionCallSiteCacheEntry *entry) {
    TZrUInt32 rid;
    SZrMetadataTokenRecord *records;
    TZrSize oldSize;
    if (entry->binding.contract.signatureToken != 0u) return ZR_TRUE;
    rid = binding_next_rid(function, ZR_METADATA_TABLE_SIGNATURE);
    if (rid > ZR_METADATA_TOKEN_RID_MASK) return ZR_FALSE;
    oldSize = sizeof(*records) * function->metadataTokenRecordLength;
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global,
            oldSize + sizeof(*records), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    if (oldSize != 0u) memcpy(records, function->metadataTokenRecords, oldSize);
    memset(&records[function->metadataTokenRecordLength], 0, sizeof(*records));
    records[function->metadataTokenRecordLength].token = ZR_METADATA_TOKEN_MAKE(
            ZR_METADATA_TABLE_SIGNATURE, rid);
    records[function->metadataTokenRecordLength].signatureHash = entry->binding.contract.signatureHash;
    records[function->metadataTokenRecordLength].reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_SIGNATURE;
    records[function->metadataTokenRecordLength].ownerIndex = ZR_CALL_BINDING_SLOT_NONE;
    entry->binding.contract.signatureToken = records[function->metadataTokenRecordLength].token;
    if (oldSize != 0u) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            function->metadataTokenRecords, oldSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = records;
    ++function->metadataTokenRecordLength;
    return ZR_TRUE;
}

TZrBool compiler_record_typed_call_binding(SZrCompilerState *compiler,
                                           const SZrResolvedCallSignature *signature,
                                           TZrUInt32 argumentCount,
                                           SZrFileRange location) {
    SZrFunctionCallSiteCacheEntry *entry;
    TZrUInt16 cacheIndex;
    TZrUInt64 signatureHash;
    if (compiler == ZR_NULL || compiler->currentFunction == ZR_NULL || signature == ZR_NULL ||
        argumentCount != signature->parameterTypes.length) return ZR_FALSE;
    signatureHash = compiler_typed_call_signature_hash(compiler, signature);
    if (signatureHash == 0u) {
        ZrParser_Compiler_Error(compiler, "Typed callable has no complete structural signature", location);
        return ZR_FALSE;
    }
    if (!reserve_member_slot_get_cache(compiler, ZR_PARSER_MEMBER_ID_NONE, ZR_NULL,
                argumentCount, &cacheIndex, location)) return ZR_FALSE;
    entry = &compiler->currentFunction->callSiteCaches[cacheIndex];
    entry->kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL;
    entry->binding.contract.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
    entry->binding.contract.targetMetadataToken = 0u;
    entry->binding.contract.signatureHash = signatureHash;
    entry->binding.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    entry->binding.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    entry->bindingLocation.kind = ZR_CALL_BINDING_RELOCATION_NONE;
    entry->bindingLocation.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
    return ZR_TRUE;
}

void compiler_get_member_call_binding_fact(SZrCompilerState *compiler,
        const SZrTypeMemberInfo *member, SZrCallBindingContract *fact) {
    ZR_UNUSED_PARAMETER(compiler);
    if (member != ZR_NULL &&
        (member->callBindingLocationKind == ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
         compiler_native_call_binding_is_provider_contract(&member->callBindingFact))) {
        *fact = member->callBindingFact;
        return;
    }
    memset(fact, 0, sizeof(*fact));
    if (member == ZR_NULL) return;
    fact->bindingKind = ZR_CALL_BINDING_DIRECT;
    fact->dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    fact->targetMetadataToken = member->metadataToken;
    fact->signatureToken = member->signatureToken;
    fact->signatureHash = member->signatureHash;
    if (!member->isStatic && member->interfaceContractSlot != ZR_CALL_BINDING_SLOT_NONE &&
        member->compiledFunction == ZR_NULL) {
        fact->bindingKind = ZR_CALL_BINDING_INTERFACE;
        fact->dispatchSlot = member->interfaceContractSlot;
    } else if (!member->isStatic && member->virtualSlotIndex != ZR_CALL_BINDING_SLOT_NONE &&
        (member->modifierFlags & (ZR_DECLARATION_MODIFIER_VIRTUAL | ZR_DECLARATION_MODIFIER_OVERRIDE |
                                  ZR_DECLARATION_MODIFIER_ABSTRACT)) != 0u) {
        fact->bindingKind = ZR_CALL_BINDING_VIRTUAL;
        fact->dispatchSlot = member->virtualSlotIndex;
    }
    fact->operation = member->isMetaMethod ? ZR_CALL_BINDING_OPERATION_META : ZR_CALL_BINDING_OPERATION_CALL;
    if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET) fact->operation = ZR_CALL_BINDING_OPERATION_GET;
    if (member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_SET || member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_INIT)
        fact->operation = ZR_CALL_BINDING_OPERATION_SET;
}

static TZrUInt32 binding_next_rid(const SZrFunction *function, TZrUInt32 table) {
    TZrUInt32 maximum = 0u;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        TZrMetadataToken token = function->metadataTokenRecords[index].token;
        if (ZR_METADATA_TOKEN_TABLE(token) == table && ZR_METADATA_TOKEN_RID(token) > maximum)
            maximum = ZR_METADATA_TOKEN_RID(token);
    }
    return maximum + 1u;
}

static TZrBool binding_publish_definition(SZrCompilerState *compiler, SZrFunction *function,
                                          SZrFunctionCallSiteCacheEntry *entry,
                                          TZrUInt32 marker) {
    TZrUInt32 memberRid = binding_next_rid(function, ZR_METADATA_TABLE_MEMBER_DEF);
    TZrUInt32 signatureRid = binding_next_rid(function, ZR_METADATA_TABLE_SIGNATURE);
    SZrMetadataTokenRecord *records;
    SZrMetadataTokenRecord *record;
    TZrSize oldSize = sizeof(*records) * function->metadataTokenRecordLength;
    if (memberRid > ZR_METADATA_TOKEN_RID_MASK || signatureRid > ZR_METADATA_TOKEN_RID_MASK) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        record = &function->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            record->reserved0 == marker &&
            record->ownerIndex == entry->bindingLocation.targetIndex) {
            entry->binding.contract.targetMetadataToken = record->token;
            entry->binding.contract.signatureToken = record->relatedToken;
            return record->signatureHash == entry->binding.contract.signatureHash;
        }
    }
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global,
            oldSize + sizeof(*records) * 2u, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    if (oldSize != 0u) memcpy(records, function->metadataTokenRecords, oldSize);
    record = &records[function->metadataTokenRecordLength];
    memset(record, 0, sizeof(*record) * 2u);
    record->token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, memberRid);
    record->relatedToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, signatureRid);
    record->ownerIndex = entry->bindingLocation.targetIndex;
    record->reserved0 = marker;
    record->signatureHash = entry->binding.contract.signatureHash;
    record[1] = *record;
    record[1].token = record->relatedToken;
    record[1].relatedToken = record->token;
    entry->binding.contract.targetMetadataToken = record->token;
    entry->binding.contract.signatureToken = record->relatedToken;
    if (oldSize != 0u) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            function->metadataTokenRecords, oldSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = records;
    function->metadataTokenRecordLength += 2u;
    return ZR_TRUE;
}

static TZrBool binding_publish_owner(SZrCompilerState *compiler, SZrFunction *function,
                                     SZrFunctionCallSiteCacheEntry *entry, SZrFunction *target) {
    SZrFunctionMemberEntry *member;
    SZrFunction *owner = ZrCore_CallBinding_PrototypeOwner(function);
    SZrObjectPrototype *prototype;
    SZrMetadataTokenRecord *records;
    TZrSize oldSize;
    TZrUInt32 rid;
    if (entry->memberEntryIndex >= function->memberEntryLength) {
        return ZR_TRUE;
    }
    member = &function->memberEntries[entry->memberEntryIndex];
    if (member->entryKind != ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR) {
        return ZR_TRUE;
    }
    if (owner == ZR_NULL) {
        return ZR_FALSE;
    }
    if (owner->prototypeInstances == ZR_NULL) ZrCore_Module_CreatePrototypesFromData(compiler->state, ZR_NULL, owner);
    if (owner->prototypeInstances == ZR_NULL || member->prototypeIndex >= owner->prototypeInstancesLength ||
        (prototype = owner->prototypeInstances[member->prototypeIndex]) == ZR_NULL) return ZR_FALSE;
    if (target == ZR_NULL && entry->binding.contract.bindingKind == ZR_CALL_BINDING_INTERFACE) {
        if (member->descriptorIndex >= prototype->memberDescriptorCount) return ZR_FALSE;
    } else {
        member->descriptorIndex = (TZrUInt32)-1;
        for (TZrUInt32 index = 0u; index < prototype->memberDescriptorCount; ++index) {
            if (prototype->memberDescriptors[index].methodFunction == target) {
                if (member->descriptorIndex != (TZrUInt32)-1) return ZR_FALSE;
                member->descriptorIndex = index;
            }
        }
    }
    if (member->descriptorIndex == (TZrUInt32)-1) return ZR_FALSE;
    entry->binding.contract.layoutHash = ZrCore_CallBinding_PrototypeLayoutHash(owner, member->prototypeIndex);
    entry->binding.contract.layoutVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
    if (entry->binding.contract.layoutHash == 0u) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < function->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_TYPE_DEF &&
            record->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER &&
            record->ownerIndex == member->prototypeIndex) {
            entry->binding.contract.ownerTypeToken = record->token;
            return record->signatureHash == entry->binding.contract.layoutHash;
        }
    }
    rid = binding_next_rid(function, ZR_METADATA_TABLE_TYPE_DEF);
    if (rid > ZR_METADATA_TOKEN_RID_MASK) return ZR_FALSE;
    oldSize = sizeof(*records) * function->metadataTokenRecordLength;
    records = ZrCore_Memory_RawMallocWithType(compiler->state->global, oldSize + sizeof(*records),
                                            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (records == ZR_NULL) return ZR_FALSE;
    if (oldSize != 0u) memcpy(records, function->metadataTokenRecords, oldSize);
    memset(&records[function->metadataTokenRecordLength], 0, sizeof(*records));
    records[function->metadataTokenRecordLength].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, rid);
    records[function->metadataTokenRecordLength].ownerIndex = member->prototypeIndex;
    records[function->metadataTokenRecordLength].reserved0 = ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER;
    records[function->metadataTokenRecordLength].signatureHash = entry->binding.contract.layoutHash;
    entry->binding.contract.ownerTypeToken = records[function->metadataTokenRecordLength].token;
    if (oldSize != 0u) ZrCore_Memory_RawFreeWithType(compiler->state->global,
            function->metadataTokenRecords, oldSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->metadataTokenRecords = records;
    ++function->metadataTokenRecordLength;
    return ZR_TRUE;
}

typedef struct SZrCompilerCallBindingContext {
    SZrCompilerState *compiler;
    TZrUInt64 moduleSignatureHash;
} SZrCompilerCallBindingContext;

static TZrBool binding_finalize_function(SZrFunction *function, void *data) {
    SZrCompilerCallBindingContext *context = data;
    SZrCompilerState *compiler = context->compiler;
    if (function->moduleSignatureHash == 0u)
        function->moduleSignatureHash = context->moduleSignatureHash;
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        SZrFunction *target = ZR_NULL;
        TZrUInt32 marker;
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_NONE) continue;
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
            compiler_typed_call_use_generic_dispatch(function, entry->instructionIndex);
            if (function->moduleSignatureHash == 0u) {
                function->moduleSignatureHash = ZrCore_Hash_CreateStable64(
                        function->prototypeData, function->prototypeDataLength);
            }
            entry->binding.contract.moduleSignatureHash = function->moduleSignatureHash;
            if (entry->binding.contract.signatureHash == 0u ||
                !binding_publish_typed_signature(compiler, function, entry)) {
                ZrParser_Compiler_Error(compiler,
                        "Typed function call has no complete callable signature contract",
                        compiler->currentAst != ZR_NULL ? compiler->currentAst->location : (SZrFileRange){0});
                return ZR_FALSE;
            }
            entry->binding.generation = function->callBindingGeneration;
            continue;
        }
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
            (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE &&
             compiler_native_call_binding_is_provider_contract(&entry->binding.contract))) {
            /* Provider identities are already complete.  Keep their token,
             * signature and module hash intact; the native registry performs
             * relocation during linking. */
            entry->bindingLocation.ownerDepth = 0u;
            entry->bindingLocation.flags = 0u;
            entry->binding.generation = function->callBindingGeneration;
            continue;
        }
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_CONSTANT) {
            if (entry->bindingLocation.targetIndex >= function->constantValueLength) return ZR_FALSE;
            target = ZrCore_Closure_GetMetadataFunctionFromValue(compiler->state,
                    &function->constantValueList[entry->bindingLocation.targetIndex]);
            entry->binding.contract.signatureHash = ZrCore_CallBinding_FunctionSignatureHash(target);
            marker = ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT;
        } else if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_MODULE &&
                   (entry->binding.contract.bindingKind == ZR_CALL_BINDING_INTERFACE ||
                    entry->binding.contract.bindingKind == ZR_CALL_BINDING_VIRTUAL)) {
            marker = ZR_METADATA_TOKEN_RECORD_CALLABLE_MODULE;
        } else {
            return ZR_FALSE;
        }
        if (function->moduleSignatureHash == 0u && function->prototypeData != ZR_NULL &&
            function->prototypeDataLength != 0u) {
            function->moduleSignatureHash = ZrCore_Hash_CreateStable64(
                    function->prototypeData, function->prototypeDataLength);
        }
        entry->binding.contract.moduleSignatureHash = function->moduleSignatureHash;
        if (entry->binding.contract.signatureHash == 0u ||
            !binding_publish_definition(compiler, function, entry, marker) ||
            !binding_publish_owner(compiler, function, entry, target)) {
            ZrParser_Compiler_Error(compiler, "Static call binding has no complete callable contract",
                    compiler->currentAst != ZR_NULL ? compiler->currentAst->location : (SZrFileRange){0});
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool compiler_finalize_call_bindings(SZrCompilerState *compiler, SZrFunction *function) {
    SZrCompilerCallBindingContext context;
    if (compiler == ZR_NULL || function == ZR_NULL) return ZR_FALSE;
    context.compiler = compiler;
    context.moduleSignatureHash = function->moduleSignatureHash != 0u ? function->moduleSignatureHash :
            ZrCore_Hash_CreateStable64(function->prototypeData, function->prototypeDataLength);
    return ZrCore_CallBinding_VisitFunctions(function, binding_finalize_function, &context) &&
            compiler_publish_module_call_bindings(compiler, function) &&
            ZrCore_CallBinding_LinkFunction(compiler->state, function, &compiler->state->lastCallBindingError);
}
