#include "zr_vm_core/module_call_binding.h"

#include <string.h>

#include "module/module_internal.h"
#include "call_binding_site.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/function_identity.h"

enum {
    MODULE_BINDING_GETTER = 1u,
    MODULE_BINDING_SETTER = 2u,
    MODULE_BINDING_INITIALIZER = 3u,
    MODULE_BINDING_VIRTUAL_MODIFIERS = (1u << 0u) | (1u << 1u) | (1u << 2u)
};

SZrFunction *ZrCore_CallBinding_GetModuleFunction(SZrState *state, SZrObjectModule *module) {
    SZrTypeValue key;
    SZrString *name;
    if (state == ZR_NULL || module == ZR_NULL) return ZR_NULL;
    if (module->hasMetadataRuntime) return module->metadataRuntime.metadataFunction;
    name = ZrCore_String_CreateFromNative(state, "__zr_reflection_entry_function");
    if (name == ZR_NULL) return ZR_NULL;
    zr_module_init_string_key(state, &key, name);
    return ZrCore_Closure_GetMetadataFunctionFromValue(state,
            ZrCore_Object_GetValue(state, &module->super, &key));
}

static TZrBool constant_contract(const SZrFunction *provider, const SZrMetadataTokenRecord *record,
                                 SZrCallBindingContract *contract) {
    TZrSize offset = sizeof(TZrUInt32);
    memset(contract, 0, sizeof(*contract));
    contract->bindingKind = ZR_CALL_BINDING_DIRECT;
    contract->targetMetadataToken = record->token;
    contract->signatureToken = record->relatedToken;
    contract->signatureHash = record->signatureHash;
    contract->moduleSignatureHash = provider->moduleSignatureHash;
    contract->ownerTypeToken = record->ownerToken;
    contract->layoutVersion = record->layoutVersion;
    contract->layoutHash = record->layoutHash;
    contract->dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    if (record->ownerToken == 0u) return ZR_TRUE;
    for (TZrUInt32 index = 0u; index < provider->prototypeCount; ++index) {
        SZrCompiledPrototypeInfo info;
        TZrUInt64 bytes;
        const SZrCompiledMemberInfo *members;
        if (offset > provider->prototypeDataLength || provider->prototypeDataLength - offset < sizeof(info))
            return ZR_FALSE;
        memcpy(&info, provider->prototypeData + offset, sizeof(info));
        bytes = sizeof(info) + ((TZrUInt64)info.inheritsCount + info.decoratorsCount) * sizeof(TZrUInt32) +
                (TZrUInt64)info.membersCount * sizeof(*members);
        if (bytes > provider->prototypeDataLength - offset) return ZR_FALSE;
        members = (const SZrCompiledMemberInfo *)(provider->prototypeData + offset + sizeof(info) +
                ((TZrSize)info.inheritsCount + info.decoratorsCount) * sizeof(TZrUInt32));
        for (TZrUInt32 member = 0u; member < info.membersCount; ++member) {
            const SZrCompiledMemberInfo *item = &members[member];
            if ((item->memberType != ZR_AST_CONSTANT_CLASS_METHOD &&
                 item->memberType != ZR_AST_CONSTANT_STRUCT_METHOD &&
                 item->memberType != ZR_AST_CONSTANT_CLASS_META_FUNCTION &&
                 item->memberType != ZR_AST_CONSTANT_STRUCT_META_FUNCTION) ||
                item->functionConstantIndex != record->ownerIndex) continue;
            contract->operation = item->isMetaMethod ? ZR_CALL_BINDING_OPERATION_META :
                    item->accessorRole == MODULE_BINDING_GETTER ? ZR_CALL_BINDING_OPERATION_GET :
                    (item->accessorRole == MODULE_BINDING_SETTER ||
                     item->accessorRole == MODULE_BINDING_INITIALIZER) ? ZR_CALL_BINDING_OPERATION_SET :
                    ZR_CALL_BINDING_OPERATION_CALL;
            if (!item->isStatic && item->virtualSlotIndex != ZR_CALL_BINDING_SLOT_NONE &&
                (item->modifierFlags & MODULE_BINDING_VIRTUAL_MODIFIERS) != 0u) {
                contract->bindingKind = ZR_CALL_BINDING_VIRTUAL;
                contract->dispatchSlot = item->virtualSlotIndex;
            }
            return ZR_TRUE;
        }
        offset += (TZrSize)bytes;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_CallBinding_ModuleConstantContract(const SZrFunction *provider,
        TZrUInt32 constantIndex, SZrCallBindingContract *contract) {
    const SZrMetadataTokenRecord *selected = ZR_NULL;
    if (provider == ZR_NULL || contract == ZR_NULL || constantIndex >= provider->constantValueLength) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < provider->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &provider->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_DEF ||
            record->reserved0 != ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT ||
            record->ownerIndex != constantIndex) continue;
        if (selected != ZR_NULL) return ZR_FALSE;
        selected = record;
    }
    if (selected == ZR_NULL) return ZR_FALSE;
    if (!constant_contract(provider, selected, contract)) return ZR_FALSE;
    return ZrCore_CallBinding_CheckContract(contract, ZR_NULL) == ZR_CALL_BINDING_OK;
}

typedef struct SZrImportedBindingContext {
    SZrState *state;
    SZrObjectModule *module;
    SZrFunction *provider;
    SZrCallBindingDiagnostic *diagnostic;
} SZrImportedBindingContext;

static TZrBool imported_fail(SZrImportedBindingContext *context, SZrFunctionCallSiteCacheEntry *entry,
                            EZrCallBindingStatus status) {
    ZrCore_CallBinding_Invalidate(&entry->binding);
    if (context->diagnostic != ZR_NULL) {
        context->diagnostic->status = status;
        context->diagnostic->targetMetadataToken = entry->binding.contract.targetMetadataToken;
        context->diagnostic->instructionIndex = entry->instructionIndex;
    }
    return ZR_FALSE;
}

static TZrBool imported_owner(SZrImportedBindingContext *context, SZrFunction *consumer,
        SZrFunctionCallSiteCacheEntry *entry, SZrCallBindingCandidate *candidate) {
    const SZrMetadataTokenRecord *owner = ZR_NULL;
    SZrObjectPrototype *prototype;
    SZrFunctionCallSitePicSlot *guard = &entry->picSlots[0];
    if (candidate->contract.ownerTypeToken == 0u) return ZR_TRUE;
    for (TZrUInt32 index = 0u; index < context->provider->metadataTokenRecordLength; ++index) {
        const SZrMetadataTokenRecord *record = &context->provider->metadataTokenRecords[index];
        if (record->token != candidate->contract.ownerTypeToken) continue;
        if (owner != ZR_NULL) return imported_fail(context, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
        owner = record;
    }
    if (owner == ZR_NULL || ZR_METADATA_TOKEN_TABLE(owner->token) != ZR_METADATA_TABLE_TYPE_DEF ||
        owner->reserved0 != ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER)
        return imported_fail(context, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
    if (owner->signatureHash != candidate->contract.layoutHash ||
        ZrCore_CallBinding_PrototypeLayoutHash(context->provider, owner->ownerIndex) != owner->signatureHash ||
        owner->ownerIndex >= context->provider->prototypeInstancesLength ||
        context->provider->prototypeInstances == ZR_NULL ||
        (prototype = context->provider->prototypeInstances[owner->ownerIndex]) == ZR_NULL)
        return imported_fail(context, entry, ZR_CALL_BINDING_LAYOUT_MISMATCH);
    guard->cachedOwnerPrototype = prototype;
    guard->cachedOwnerShapeId = prototype->shapeId;
    guard->cachedOwnerShapeGeneration = prototype->shapeGeneration;
    guard->cachedOwnerVersion = prototype->super.memberVersion;
    guard->cachedReceiverPrototype = prototype;
    guard->cachedReceiverShapeId = prototype->shapeId;
    guard->cachedReceiverShapeGeneration = prototype->shapeGeneration;
    guard->cachedReceiverVersion = prototype->super.memberVersion;
    guard->cachedFunction = candidate->target.vm.function;
    guard->cachedDescriptorIndex = ZR_CALL_BINDING_SLOT_NONE;
    for (TZrUInt32 index = 0u; index < prototype->memberDescriptorCount; ++index) {
        const SZrMemberDescriptor *descriptor = &prototype->memberDescriptors[index];
        if (descriptor->methodFunction != candidate->target.vm.function) continue;
        if (!zr_call_binding_descriptor_matches(consumer, entry, descriptor))
            return imported_fail(context, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
        if (guard->cachedDescriptorIndex != ZR_CALL_BINDING_SLOT_NONE)
            return imported_fail(context, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
        guard->cachedDescriptorIndex = index;
        guard->cachedIsStatic = descriptor->isStatic;
        if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_VIRTUAL &&
            entry->binding.contract.dispatchSlot != descriptor->virtualSlotIndex)
            return imported_fail(context, entry, ZR_CALL_BINDING_INVALID_SLOT);
    }
    if (guard->cachedDescriptorIndex == ZR_CALL_BINDING_SLOT_NONE)
        return imported_fail(context, entry, ZR_CALL_BINDING_TARGET_NOT_FOUND);
    entry->picSlotCount = 1u;
    candidate->target.ownerLayoutGeneration = prototype->layoutGeneration;
    candidate->target.dispatchSlotCount = prototype->nextVirtualSlotIndex;
    ZrCore_RawObject_Barrier(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(consumer),
            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    return ZR_TRUE;
}

static TZrBool imported_aot_target(SZrImportedBindingContext *context,
        SZrFunctionCallSiteCacheEntry *entry, SZrCallBindingCandidate *candidate) {
    const SZrAotCodeRegistration *registration = context->module->metadataRuntime.codeRegistration;
    TZrUInt32 selected = UINT32_MAX;
    for (TZrUInt32 index = 0u; index < registration->functionCount; ++index) {
        SZrFunction *function = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                context->state, context->provider, index);
        if (!ZrCore_Function_HasSameDefinition(candidate->target.vm.function, function)) continue;
        if (selected != UINT32_MAX) return imported_fail(context, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
        selected = index;
    }
    if (selected == UINT32_MAX || selected >= registration->methodInfoCount ||
        registration->functionPointers == ZR_NULL || registration->methodInfos == ZR_NULL ||
        registration->functionPointers[selected] == ZR_NULL || registration->methodInfos[selected] == ZR_NULL ||
        registration->methodInfos[selected]->functionIndex != selected ||
        registration->methodInfos[selected]->invoker == ZR_NULL)
        return imported_fail(context, entry, ZR_CALL_BINDING_TARGET_NOT_FOUND);
    candidate->target.targetKind = ZR_CALL_BINDING_TARGET_AOT;
    candidate->target.aot.thunk = registration->functionPointers[selected];
    candidate->target.aot.methodInfo = registration->methodInfos[selected];
    candidate->target.aot.invoker = registration->methodInfos[selected]->invoker;
    return ZR_TRUE;
}

static TZrBool link_imported_function(SZrFunction *consumer, void *data) {
    SZrImportedBindingContext *context = data;
    for (TZrUInt32 index = 0u; index < consumer->callSiteCacheLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = &consumer->callSiteCaches[index];
        SZrCallBindingCandidate candidate = {0};
        const SZrMetadataTokenRecord *definition = ZR_NULL;
        const SZrTypeValue *callable = ZR_NULL;
        if (entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
            entry->binding.contract.moduleSignatureHash != context->provider->moduleSignatureHash) continue;
        if (!zr_call_binding_site_matches(consumer, index))
            return imported_fail(context, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
        if (entry->binding.target.targetKind != ZR_CALL_BINDING_TARGET_NONE) {
            EZrCallBindingStatus status = ZrCore_CallBinding_Validate(&entry->binding,
                    consumer->callBindingGeneration, context->diagnostic);
            if (status == ZR_CALL_BINDING_OK) continue;
            /* A provider can be removed and loaded again under the same
             * module contract.  The old target generation is then stale, so
             * clear only the process-local target and resolve the new
             * provider entry below.  Structural contract failures remain
             * link errors and never fall back to name lookup. */
            if (status != ZR_CALL_BINDING_STALE_GENERATION) return ZR_FALSE;
            ZrCore_CallBinding_Invalidate(&entry->binding);
        }
        for (TZrUInt32 row = 0u; row < context->provider->metadataTokenRecordLength; ++row) {
            const SZrMetadataTokenRecord *record = &context->provider->metadataTokenRecords[row];
            if (record->token != entry->binding.contract.targetMetadataToken) continue;
            if (definition != ZR_NULL) return imported_fail(context, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
            definition = record;
        }
        if (definition == ZR_NULL) return imported_fail(context, entry, ZR_CALL_BINDING_TARGET_NOT_FOUND);
        if (definition->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT) {
            if (definition->ownerIndex >= context->provider->constantValueLength)
                return imported_fail(context, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
            callable = &context->provider->constantValueList[definition->ownerIndex];
            if (!constant_contract(context->provider, definition, &candidate.contract))
                return imported_fail(context, entry, ZR_CALL_BINDING_INVALID_RELOCATION);
        } else {
            const SZrFunctionTypedExportSymbol *symbol = ZR_NULL;
            for (TZrUInt32 row = 0u; row < context->provider->typedExportedSymbolLength; ++row) {
                const SZrFunctionTypedExportSymbol *item = &context->provider->typedExportedSymbols[row];
                if (item->metadataToken != definition->token ||
                    item->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) continue;
                if (symbol != ZR_NULL) return imported_fail(context, entry, ZR_CALL_BINDING_AMBIGUOUS_TARGET);
                symbol = item;
            }
            if (symbol == ZR_NULL) return imported_fail(context, entry, ZR_CALL_BINDING_TARGET_NOT_FOUND);
            /* Export installation owns the closure. Resolve that value once,
             * after module initialization has closed its captured locals. */
            callable = ZrCore_Module_GetPubExport(context->state, context->module, symbol->name);
            candidate.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
            candidate.contract.targetMetadataToken = symbol->metadataToken;
            candidate.contract.signatureToken = symbol->signatureToken;
            candidate.contract.signatureHash = symbol->signatureHash;
            candidate.contract.moduleSignatureHash = context->provider->moduleSignatureHash;
            candidate.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
        }
        candidate.target.vm.function = ZrCore_Closure_GetMetadataFunctionFromValue(context->state, callable);
        if (candidate.target.vm.function == ZR_NULL)
            return imported_fail(context, entry, ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
        if (definition->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT &&
            ZrCore_CallBinding_FunctionSignatureHash(candidate.target.vm.function) != definition->signatureHash)
            return imported_fail(context, entry, ZR_CALL_BINDING_SIGNATURE_MISMATCH);
        candidate.generation = consumer->callBindingGeneration;
        candidate.target.targetKind = ZR_CALL_BINDING_TARGET_VM;
        candidate.target.callableObject = callable->value.object;
        candidate.target.targetGeneration = candidate.target.vm.function->callBindingGeneration;
        if (!imported_owner(context, consumer, entry, &candidate)) return ZR_FALSE;
        if (context->module->hasMetadataRuntime &&
            context->module->metadataRuntime.codeRegistration != ZR_NULL) {
            if (!imported_aot_target(context, entry, &candidate)) return ZR_FALSE;
        } else if (callable->isNative) {
            return imported_fail(context, entry, ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
        }
        if (ZrCore_CallBinding_Resolve(&entry->binding.contract, &candidate, 1u,
                consumer->callBindingGeneration, &entry->binding, context->diagnostic) != ZR_CALL_BINDING_OK)
            return ZR_FALSE;
        if (candidate.target.targetKind == ZR_CALL_BINDING_TARGET_VM)
            ZrCore_RawObject_Barrier(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(consumer),
                    ZR_CAST_RAW_OBJECT_AS_SUPER(candidate.target.vm.function));
        ZrCore_RawObject_Barrier(context->state, ZR_CAST_RAW_OBJECT_AS_SUPER(consumer), candidate.target.callableObject);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_CallBinding_LinkImportedModule(SZrState *state, SZrFunction *consumer,
        SZrObjectModule *module, SZrCallBindingDiagnostic *diagnostic) {
    SZrImportedBindingContext context = {state, module, ZR_NULL, diagnostic};
    if (consumer == ZR_NULL || module == ZR_NULL) return ZR_TRUE;
    context.provider = ZrCore_CallBinding_GetModuleFunction(state, module);
    if (context.provider == ZR_NULL) return ZR_TRUE;
    while (consumer->ownerFunction != ZR_NULL) consumer = consumer->ownerFunction;
    return ZrCore_CallBinding_VisitFunctions(consumer, link_imported_function, &context);
}
