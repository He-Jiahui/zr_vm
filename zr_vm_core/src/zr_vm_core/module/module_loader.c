//
// Module import and source loading helpers.
//

#include "module/module_internal.h"
#include "module/module_import_signature.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/reflection.h"

#include <stdio.h>
#include <string.h>

#define ZR_MODULE_RUNTIME_PENDING_ENTRY_EXPORTS ((TZrUInt8)1)

typedef struct SZrModuleLoaderExecuteRequest {
    const SZrFunctionStackAnchor *anchor;
    TZrStackValuePointer resultBase;
} SZrModuleLoaderExecuteRequest;

static ZR_FORCE_INLINE SZrRawObject *module_loader_refresh_forwarded_raw_object(SZrRawObject *rawObject) {
    SZrRawObject *forwardedObject;

    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    return forwardedObject != ZR_NULL ? forwardedObject : rawObject;
}

static ZR_FORCE_INLINE SZrFunction *module_loader_refresh_forwarded_function(SZrFunction *function) {
    return function != ZR_NULL ? (SZrFunction *)module_loader_refresh_forwarded_raw_object(
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(function))
                               : ZR_NULL;
}

static ZR_FORCE_INLINE SZrClosure *module_loader_refresh_forwarded_closure(SZrClosure *closure) {
    return closure != ZR_NULL ? (SZrClosure *)module_loader_refresh_forwarded_raw_object(
                                        ZR_CAST_RAW_OBJECT_AS_SUPER(closure))
                              : ZR_NULL;
}

static ZR_FORCE_INLINE TZrStackValuePointer module_loader_resolve_scratch_base(TZrStackValuePointer savedStackTop,
                                                                               SZrCallInfo *savedCallInfo) {
    TZrStackValuePointer scratchBase = savedStackTop;

    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer != ZR_NULL &&
        (scratchBase == ZR_NULL || scratchBase < savedCallInfo->functionTop.valuePointer)) {
        scratchBase = savedCallInfo->functionTop.valuePointer;
    }

    return scratchBase;
}

static ZR_FORCE_INLINE TZrStackValuePointer module_loader_entry_stack_slot_pointer(SZrState *state,
                                                                                   const SZrFunction *function,
                                                                                   TZrStackValuePointer frameBase,
                                                                                   TZrUInt32 stackSlot);

static ZR_FORCE_INLINE TZrStackValuePointer module_loader_entry_stack_slot_pointer(SZrState *state,
                                                                                   const SZrFunction *function,
                                                                                   TZrStackValuePointer frameBase,
                                                                                   TZrUInt32 stackSlot) {
    const SZrFunctionFrameSlotLayout *slotLayout;
    SZrStackFramePlace place;

    if (frameBase == ZR_NULL) {
        return ZR_NULL;
    }
    if (function == ZR_NULL || function->frameSlotLayouts == ZR_NULL || function->frameSlotLayoutLength == 0u) {
        return frameBase + stackSlot;
    }

    slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
    if (slotLayout == ZR_NULL ||
        slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
        slotLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue) ||
        !ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, stackSlot, &place)) {
        return frameBase + stackSlot;
    }

    return ZR_CAST_STACK_VALUE(place.address);
}

static ZR_FORCE_INLINE SZrTypeValue *module_loader_entry_value_slot(SZrState *state,
                                                                    const SZrFunction *function,
                                                                    TZrStackValuePointer frameBase,
                                                                    TZrUInt32 stackSlot) {
    TZrStackValuePointer slotPointer =
            module_loader_entry_stack_slot_pointer(state, function, frameBase, stackSlot);
    return slotPointer != ZR_NULL ? ZrCore_Stack_GetValue(slotPointer) : ZR_NULL;
}

static TZrBool refill_io_chunk(SZrIo *io) {
    SZrState *state;
    TZrSize readSize;
    TZrBytePtr buffer;

    if (io == ZR_NULL || io->read == ZR_NULL || io->state == ZR_NULL) {
        return ZR_FALSE;
    }

    state = io->state;
    readSize = 0;
    ZR_THREAD_UNLOCK(state);
    buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }

    io->pointer = buffer;
    io->remained = readSize;
    return ZR_TRUE;
}

static TZrBytePtr read_all_from_io(SZrState *state, SZrIo *io, TZrSize *outSize) {
    SZrGlobalState *global;
    TZrSize capacity;
    TZrBytePtr buffer;
    TZrSize totalSize;

    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    capacity = (io->remained > 0) ? io->remained : ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY;
    buffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    totalSize = 0;
    while (io->remained > 0 || refill_io_chunk(io)) {
        if (totalSize + io->remained + 1 > capacity) {
            TZrSize newCapacity = capacity;
            TZrBytePtr newBuffer;

            while (totalSize + io->remained + 1 > newCapacity) {
                newCapacity *= 2;
            }

            newBuffer =
                    (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, newCapacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (newBuffer == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrCore_Memory_RawCopy(newBuffer, buffer, totalSize);
            }

            ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = newBuffer;
            capacity = newCapacity;
        }

        ZrCore_Memory_RawCopy(buffer + totalSize, io->pointer, io->remained);
        totalSize += io->remained;
        io->pointer += io->remained;
        io->remained = 0;
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

static const SZrFunctionExportedVariable *module_loader_find_exported_variable(const SZrFunction *function,
                                                                               SZrString *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL || function->exportedVariables == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->exportedVariableLength; ++index) {
        const SZrFunctionExportedVariable *exported = &function->exportedVariables[index];
        if (exported->name == name || (exported->name != ZR_NULL && ZrCore_String_Equal(exported->name, name))) {
            return exported;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionTypedExportSymbol *module_loader_find_typed_export_symbol(const SZrFunction *function,
                                                                                  SZrString *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL || function->typedExportedSymbols == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedExportedSymbolLength; ++index) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        if (symbol->name == name || (symbol->name != ZR_NULL && ZrCore_String_Equal(symbol->name, name))) {
            return symbol;
        }
    }

    return ZR_NULL;
}

static void module_loader_set_entry_export_descriptors_ready(SZrObjectModule *module, TZrBool isReady) {
    TZrUInt32 index;

    if (module == ZR_NULL || module->exportDescriptors == ZR_NULL) {
        return;
    }

    for (index = 0; index < module->exportDescriptorLength; ++index) {
        SZrModuleExportDescriptor *descriptor = &module->exportDescriptors[index];
        if (descriptor->readiness == ZR_MODULE_EXPORT_READY_ENTRY) {
            descriptor->isReady = isReady ? 1 : 0;
        }
    }
}

static TZrBool module_loader_registry_has_other_initializing_modules(SZrState *state, const SZrObjectModule *exclude) {
    SZrObject *registry;
    TZrSize bucketIndex;

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL || !registry->nodeMap.isValid || registry->nodeMap.buckets == ZR_NULL) {
        return ZR_FALSE;
    }

    for (bucketIndex = 0; bucketIndex < registry->nodeMap.capacity; ++bucketIndex) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_OBJECT && pair->value.value.object != ZR_NULL) {
                SZrObject *object = ZR_CAST_OBJECT(state, pair->value.value.object);
                if (object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE &&
                    object != &exclude->super &&
                    ((SZrObjectModule *)object)->initState == ZR_MODULE_INIT_STATE_INITIALIZING) {
                    return ZR_TRUE;
                }
            }
            pair = pair->next;
        }
    }

    return ZR_FALSE;
}

static void module_loader_finalize_pending_entry_exports(SZrState *state) {
    SZrObject *registry;
    TZrSize bucketIndex;

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL || !registry->nodeMap.isValid || registry->nodeMap.buckets == ZR_NULL) {
        return;
    }

    for (bucketIndex = 0; bucketIndex < registry->nodeMap.capacity; ++bucketIndex) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_OBJECT && pair->value.value.object != ZR_NULL) {
                SZrObject *object = ZR_CAST_OBJECT(state, pair->value.value.object);
                if (object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                    SZrObjectModule *module = (SZrObjectModule *)object;
                    if ((module->reserved0 & ZR_MODULE_RUNTIME_PENDING_ENTRY_EXPORTS) != 0) {
                        module_loader_set_entry_export_descriptors_ready(module, ZR_TRUE);
                        module->reserved0 = (TZrUInt8)(module->reserved0 & (~ZR_MODULE_RUNTIME_PENDING_ENTRY_EXPORTS));
                    }
                }
            }
            pair = pair->next;
        }
    }
}

static TZrBool module_loader_register_export_descriptors(SZrState *state,
                                                         SZrObjectModule *module,
                                                         const SZrFunction *function) {
    TZrUInt32 index;

    if (state == ZR_NULL || module == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->typedExportedSymbolLength; ++index) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        SZrModuleExportDescriptor descriptor;

        if (symbol->name == ZR_NULL) {
            continue;
        }

        ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
        descriptor.name = symbol->name;
        descriptor.accessModifier = symbol->accessModifier;
        descriptor.exportKind = symbol->exportKind;
        descriptor.readiness = symbol->readiness;
        descriptor.isReady =
                (TZrUInt8)((symbol->exportKind == ZR_MODULE_EXPORT_KIND_TYPE &&
                            symbol->readiness == ZR_MODULE_EXPORT_READY_DECLARATION)
                                   ? 1
                                   : 0);
        if (!ZrCore_Module_RegisterExportDescriptor(state, module, &descriptor)) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < function->exportedVariableLength; ++index) {
        const SZrFunctionExportedVariable *exported = &function->exportedVariables[index];
        SZrModuleExportDescriptor descriptor;

        if (exported->name == ZR_NULL || module_loader_find_typed_export_symbol(function, exported->name) != ZR_NULL) {
            continue;
        }

        ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
        descriptor.name = exported->name;
        descriptor.accessModifier = exported->accessModifier;
        descriptor.exportKind = exported->exportKind;
        descriptor.readiness = exported->readiness;
        descriptor.isReady = 0;
        if (!ZrCore_Module_RegisterExportDescriptor(state, module, &descriptor)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static SZrFunction *module_loader_find_import_caller_function(SZrState *state) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_NULL;
    }

    for (callInfo = state->callInfoList->previous; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
        if (function != ZR_NULL) {
            return module_loader_refresh_forwarded_function(function);
        }
    }

    return ZR_NULL;
}

static const TZrChar *module_loader_string_text_or_unknown(SZrString *value) {
    TZrNativeString text;

    if (value == ZR_NULL) {
        return "<unknown>";
    }

    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL ? text : "<unknown>";
}

static void module_loader_append_import_signature_token_details(
        TZrChar *buffer,
        TZrSize bufferLength,
        const SZrModuleImportSignatureMismatch *mismatch) {
    TZrSize used;

    if (buffer == ZR_NULL || bufferLength == 0u || mismatch == ZR_NULL) {
        return;
    }

    used = strlen(buffer);
    if (used >= bufferLength) {
        return;
    }

    if (mismatch->hasMetadataTokenMismatch) {
        snprintf(buffer + used,
                 bufferLength - used,
                 " expectedMetadataToken=0x%x actualMetadataToken=0x%x",
                 (unsigned)mismatch->expectedMetadataToken,
                 (unsigned)mismatch->actualMetadataToken);
        used = strlen(buffer);
        if (used >= bufferLength) {
            return;
        }
    }

    if (mismatch->hasSignatureTokenMismatch) {
        snprintf(buffer + used,
                 bufferLength - used,
                 " expectedSignatureToken=0x%x actualSignatureToken=0x%x",
                 (unsigned)mismatch->expectedSignatureToken,
                 (unsigned)mismatch->actualSignatureToken);
    }
}

static const TZrChar *module_loader_version_text_or_unknown(SZrString *value) {
    TZrNativeString text;

    if (value == ZR_NULL) {
        return "<unknown>";
    }

    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL ? text : "<unknown>";
}

static ZR_NO_RETURN void module_loader_raise_import_signature_mismatch(
        SZrState *state,
        const SZrModuleImportSignatureMismatch *mismatch,
        SZrString *path,
        SZrObjectModule *module) {
    const SZrFunctionModuleEffect *effect;
    SZrString *moduleName;
    SZrString *symbolName;
    TZrChar message[512];

    effect = mismatch != ZR_NULL ? mismatch->effect : ZR_NULL;
    moduleName = effect != ZR_NULL ? effect->moduleName : ZR_NULL;
    if (moduleName == ZR_NULL && module != ZR_NULL) {
        moduleName = module->moduleName;
    }
    if (moduleName == ZR_NULL) {
        moduleName = path;
    }
    symbolName = effect != ZR_NULL ? effect->symbolName : ZR_NULL;

    if (mismatch != ZR_NULL &&
        mismatch->kind == ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_SIGNATURE) {
        if (mismatch->hasActualHash) {
            ZrCore_Debug_RunError(
                    state,
                    "assembly_signature_mismatch: module '%s' member '%s' expectedModuleHash=0x%llx actualModuleHash=0x%llx",
                    module_loader_string_text_or_unknown(moduleName),
                    module_loader_string_text_or_unknown(symbolName),
                    (unsigned long long)mismatch->expectedHash,
                    (unsigned long long)mismatch->actualHash);
        }

        ZrCore_Debug_RunError(state,
                              "assembly_signature_mismatch: module '%s' member '%s' expectedModuleHash=0x%llx",
                              module_loader_string_text_or_unknown(moduleName),
                              module_loader_string_text_or_unknown(symbolName),
                              (unsigned long long)mismatch->expectedHash);
    }
    if (mismatch != ZR_NULL &&
        mismatch->kind == ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_VERSION) {
        ZrCore_Debug_RunError(
                state,
                "assembly_version_mismatch: module '%s' member '%s' minVersionInclusive=%s maxVersionExclusive=%s actualVersion=%s",
                module_loader_string_text_or_unknown(moduleName),
                module_loader_string_text_or_unknown(symbolName),
                module_loader_version_text_or_unknown(mismatch->expectedMinVersionInclusive),
                module_loader_version_text_or_unknown(mismatch->expectedMaxVersionExclusive),
                module_loader_version_text_or_unknown(mismatch->actualModuleVersion));
    }

    if (mismatch != ZR_NULL && mismatch->hasActualHash) {
        snprintf(message,
                 sizeof(message),
                 "import signature mismatch: module '%s' member '%s' expectedHash=0x%llx actualHash=0x%llx",
                 module_loader_string_text_or_unknown(moduleName),
                 module_loader_string_text_or_unknown(symbolName),
                 (unsigned long long)mismatch->expectedHash,
                 (unsigned long long)mismatch->actualHash);
        module_loader_append_import_signature_token_details(message, sizeof(message), mismatch);
        ZrCore_Debug_RunError(state, "%s", message);
    }

    snprintf(message,
             sizeof(message),
             "import signature mismatch: module '%s' member '%s' expectedHash=0x%llx",
             module_loader_string_text_or_unknown(moduleName),
             module_loader_string_text_or_unknown(symbolName),
             (unsigned long long)(mismatch != ZR_NULL ? mismatch->expectedHash : 0u));
    module_loader_append_import_signature_token_details(message, sizeof(message), mismatch);
    ZrCore_Debug_RunError(state, "%s", message);
}

static ZR_NO_RETURN void module_loader_raise_import_load_unavailable(SZrState *state, SZrString *path) {
    const TZrChar *diagnostic =
            state != ZR_NULL ? ZrCore_GlobalState_GetModuleLoadDiagnostic(state->global) : ZR_NULL;

    if (diagnostic != ZR_NULL) {
        ZrCore_Debug_RunError(state,
                              "import_load_unavailable: module '%s' (%s)",
                              module_loader_string_text_or_unknown(path),
                              diagnostic);
    }

    ZrCore_Debug_RunError(state,
                          "import_load_unavailable: module '%s'",
                          module_loader_string_text_or_unknown(path));
}

static TZrBool module_loader_slot_matches_function(SZrState *state,
                                                   const SZrTypeValue *value,
                                                   SZrFunction *function) {
    SZrFunction *existingFunction;

    if (state == ZR_NULL || value == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    existingFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
    return existingFunction == function ? ZR_TRUE : ZR_FALSE;
}

static const SZrTypeValue *module_loader_get_existing_export_value(SZrState *state,
                                                                   SZrObjectModule *module,
                                                                   const SZrFunctionExportedVariable *exported) {
    if (state == ZR_NULL || module == ZR_NULL || exported == ZR_NULL || exported->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (exported->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
        return ZrCore_Module_GetPubExport(state, module, exported->name);
    }
    if (exported->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
        return ZrCore_Module_GetProExport(state, module, exported->name);
    }

    return ZR_NULL;
}

static SZrFunction *module_loader_find_child_function_for_value(SZrState *state,
                                                                SZrFunction *entryFunction,
                                                                const SZrTypeValue *value) {
    SZrFunction *metadataFunction;
    TZrUInt32 index;

    if (state == ZR_NULL || entryFunction == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
    if (metadataFunction == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < entryFunction->childFunctionLength; ++index) {
        SZrFunction *childFunction = &entryFunction->childFunctionList[index];
        if (childFunction == metadataFunction) {
            return childFunction;
        }
    }

    return ZR_NULL;
}

static TZrBool module_loader_bind_exported_function(SZrState *state,
                                                    SZrObjectModule *module,
                                                    SZrFunction *function,
                                                    const SZrFunctionExportedVariable *exported,
                                                    TZrStackValuePointer base,
                                                    TZrBool forceRecreate) {
    TZrStackValuePointer slotPointer;
    SZrTypeValue *slotValue;
    SZrFunction *childFunction;

    if (state == ZR_NULL || module == ZR_NULL || function == ZR_NULL || exported == ZR_NULL || base == ZR_NULL ||
        exported->name == ZR_NULL || exported->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION ||
        exported->readiness != ZR_MODULE_EXPORT_READY_DECLARATION || exported->stackSlot >= function->stackSize ||
        exported->callableChildIndex >= function->childFunctionLength) {
        return ZR_TRUE;
    }

    slotPointer = module_loader_entry_stack_slot_pointer(state, function, base, exported->stackSlot);
    slotValue = module_loader_entry_value_slot(state, function, base, exported->stackSlot);
    childFunction = &function->childFunctionList[exported->callableChildIndex];
    if (slotValue == ZR_NULL || childFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    {
        const SZrTypeValue *existingExport = module_loader_get_existing_export_value(state, module, exported);

        /*
         * Declaration exports are preinstalled before module entry runs. If the
         * entry finishes through a tail-call path, the frame slots can be
         * repurposed before this refresh step runs. Rebinding from the current
         * slot would then capture the reused tail-call frame state instead of
         * the original exported closure. Keep the preinstalled export when it
         * already resolves to the same child function.
         */
        if (!forceRecreate &&
            existingExport != ZR_NULL &&
            module_loader_slot_matches_function(state, existingExport, childFunction)) {
            ZrCore_Module_SetExportDescriptorReady(module, exported->name, ZR_TRUE);
            return ZR_TRUE;
        }
    }

    if (forceRecreate || !module_loader_slot_matches_function(state, slotValue, childFunction)) {
        ZrCore_Closure_PushToStack(state, childFunction, ZR_NULL, base, slotPointer);
        slotValue->type = ZR_VALUE_TYPE_CLOSURE;
        slotValue->isGarbageCollectable = ZR_TRUE;
        slotValue->isNative = ZR_FALSE;
    }

    if (exported->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
        ZrCore_Module_AddPubExport(state, module, exported->name, slotValue);
    } else if (exported->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
        ZrCore_Module_AddProExport(state, module, exported->name, slotValue);
    }
    ZrCore_Module_SetExportDescriptorReady(module, exported->name, ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool module_loader_stack_slot_is_preinstalled_callable(const SZrFunction *entryFunction, TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (entryFunction == ZR_NULL || entryFunction->topLevelCallableBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < entryFunction->topLevelCallableBindingLength; ++index) {
        const SZrFunctionTopLevelCallableBinding *binding = &entryFunction->topLevelCallableBindings[index];
        if (binding->stackSlot == stackSlot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool module_loader_callable_can_be_preinstalled(const SZrFunction *entryFunction,
                                                          const SZrFunction *childFunction) {
    TZrUInt32 index;

    if (entryFunction == ZR_NULL || childFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < childFunction->closureValueLength; ++index) {
        const SZrFunctionClosureVariable *closureValue = &childFunction->closureValueList[index];
        if (!closureValue->inStack) {
            continue;
        }

        if (!module_loader_stack_slot_is_preinstalled_callable(entryFunction, closureValue->index)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool module_loader_preinstall_exported_function(SZrState *state,
                                                          SZrObjectModule *module,
                                                          SZrFunction *function,
                                                          const SZrFunctionExportedVariable *exported,
                                                          TZrStackValuePointer base) {
    return module_loader_bind_exported_function(state, module, function, exported, base, ZR_FALSE);
}

static TZrBool module_loader_preinstall_top_level_callables(SZrState *state,
                                                            SZrObjectModule *module,
                                                            SZrFunction *function,
                                                            TZrStackValuePointer base) {
    TZrUInt32 index;

    if (state == ZR_NULL || module == ZR_NULL || function == ZR_NULL || base == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->exportedVariableLength; ++index) {
        if (!module_loader_preinstall_exported_function(state, module, function, &function->exportedVariables[index], base)) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < function->topLevelCallableBindingLength; ++index) {
        const SZrFunctionTopLevelCallableBinding *binding = &function->topLevelCallableBindings[index];
        const SZrFunctionExportedVariable *exported = ZR_NULL;
        TZrStackValuePointer slotPointer;
        SZrTypeValue *slotValue;
        SZrFunction *childFunction;

        if (binding->name == ZR_NULL || binding->stackSlot >= function->stackSize ||
            binding->callableChildIndex >= function->childFunctionLength) {
            continue;
        }

        slotPointer = module_loader_entry_stack_slot_pointer(state, function, base, binding->stackSlot);
        slotValue = module_loader_entry_value_slot(state, function, base, binding->stackSlot);
        childFunction = &function->childFunctionList[binding->callableChildIndex];
        if (slotValue == ZR_NULL || childFunction == ZR_NULL) {
            continue;
        }

        if (!module_loader_callable_can_be_preinstalled(function, childFunction)) {
            continue;
        }

        if (!module_loader_slot_matches_function(state, slotValue, childFunction)) {
            ZrCore_Closure_PushToStack(state, childFunction, ZR_NULL, base, slotPointer);
            slotValue->type = ZR_VALUE_TYPE_CLOSURE;
            slotValue->isGarbageCollectable = ZR_TRUE;
            slotValue->isNative = ZR_FALSE;
        }

        exported = module_loader_find_exported_variable(function, binding->name);
        if (exported == ZR_NULL || exported->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION ||
            exported->readiness != ZR_MODULE_EXPORT_READY_DECLARATION) {
            continue;
        }

        if (exported->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
            ZrCore_Module_AddPubExport(state, module, exported->name, slotValue);
        } else if (exported->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
            ZrCore_Module_AddProExport(state, module, exported->name, slotValue);
        }
        ZrCore_Module_SetExportDescriptorReady(module, exported->name, ZR_TRUE);
    }

    return ZR_TRUE;
}

static TZrBool module_loader_refresh_declaration_exports(SZrState *state,
                                                         SZrObjectModule *module,
                                                         SZrFunction *function,
                                                         TZrStackValuePointer callBase) {
    TZrUInt32 index;
    TZrStackValuePointer base;

    if (state == ZR_NULL || module == ZR_NULL || function == ZR_NULL || callBase == ZR_NULL) {
        return ZR_FALSE;
    }

    base = callBase + 1;
    for (index = 0; index < function->exportedVariableLength; ++index) {
        const SZrFunctionExportedVariable *exported = &function->exportedVariables[index];
        if (exported->readiness != ZR_MODULE_EXPORT_READY_DECLARATION ||
            exported->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION) {
            continue;
        }

        if (!module_loader_bind_exported_function(state, module, function, exported, base, ZR_TRUE)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void module_loader_backfill_entry_exports(SZrState *state,
                                                 SZrObjectModule *module,
                                                 SZrFunction *function,
                                                 TZrStackValuePointer callBase) {
    TZrUInt32 index;

    if (state == ZR_NULL || module == ZR_NULL || function == ZR_NULL || callBase == ZR_NULL ||
        function->exportedVariables == ZR_NULL || function->exportedVariableLength == 0) {
        return;
    }

    for (index = 0; index < function->exportedVariableLength; ++index) {
        const SZrFunctionExportedVariable *exportVar = &function->exportedVariables[index];
        TZrStackValuePointer varPointer;
        SZrTypeValue *varValue;

        if (exportVar->name == ZR_NULL || exportVar->readiness != ZR_MODULE_EXPORT_READY_ENTRY) {
            continue;
        }

        if (exportVar->stackSlot >= function->stackSize) {
            continue;
        }

        varPointer = module_loader_entry_stack_slot_pointer(state, function, callBase + 1, exportVar->stackSlot);
        if (varPointer == ZR_NULL) {
            continue;
        }

        varValue = module_loader_entry_value_slot(state, function, callBase + 1, exportVar->stackSlot);
        if (varValue == ZR_NULL) {
            continue;
        }

        {
            SZrFunction *childFunction = module_loader_find_child_function_for_value(state, function, varValue);
            if (childFunction != ZR_NULL) {
                ZrCore_Closure_PushToStack(state, childFunction, ZR_NULL, callBase + 1, varPointer);
                varValue = module_loader_entry_value_slot(state, function, callBase + 1, exportVar->stackSlot);
                if (varValue == ZR_NULL) {
                    continue;
                }
                varValue->type = ZR_VALUE_TYPE_CLOSURE;
                varValue->isGarbageCollectable = ZR_TRUE;
                varValue->isNative = ZR_FALSE;
            }
        }

        if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
            ZrCore_Module_AddPubExport(state, module, exportVar->name, varValue);
        } else if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
            ZrCore_Module_AddProExport(state, module, exportVar->name, varValue);
        }
        ZrCore_Module_SetExportDescriptorReady(module, exportVar->name, ZR_TRUE);
    }
}

static void module_loader_execute_entry_body(SZrState *state, TZrPtr arguments) {
    SZrModuleLoaderExecuteRequest *request = (SZrModuleLoaderExecuteRequest *)arguments;

    if (state == ZR_NULL || request == ZR_NULL || request->anchor == ZR_NULL) {
        return;
    }

    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, request->anchor, 0);
}

static TZrInt64 module_loader_import_native_entry(SZrState *state, TZrBool allowSignatureMismatchFallback) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue *result;
    SZrTypeValue *pathValue;
    SZrString *path;
    struct SZrObjectModule *module;
    SZrFunction *callerFunction = ZR_NULL;
    SZrModuleImportSignatureMismatch signatureMismatch;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    argBase = functionBase + 1;
    result = ZrCore_Stack_GetValue(functionBase);

    if (state->stackTop.valuePointer <= argBase) {
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        result = ZrCore_Stack_GetValue(functionBase);
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    pathValue = ZrCore_Stack_GetValue(argBase);
    if (pathValue == ZR_NULL || pathValue->type != ZR_VALUE_TYPE_STRING) {
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        result = ZrCore_Stack_GetValue(functionBase);
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    path = ZR_CAST_STRING(state, pathValue->value.object);
    callerFunction = module_loader_find_import_caller_function(state);
    module = ZrCore_Module_ImportByPath(state, path);
    ZrCore_Memory_RawSet(&signatureMismatch, 0, sizeof(signatureMismatch));
    if (module == ZR_NULL && !allowSignatureMismatchFallback && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        state->stackTop.valuePointer = functionBase + 1;
        module_loader_raise_import_load_unavailable(state, path);
    }
    if (module != ZR_NULL &&
        !zr_module_import_signature_verify(state, callerFunction, path, module, &signatureMismatch)) {
        if (!allowSignatureMismatchFallback) {
            functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
            state->stackTop.valuePointer = functionBase + 1;
            module_loader_raise_import_signature_mismatch(state, &signatureMismatch, path, module);
        }
        module = ZR_NULL;
    }

    functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
    result = ZrCore_Stack_GetValue(functionBase);
    if (module == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    } else {
        ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
        result->type = ZR_VALUE_TYPE_OBJECT;
    }

    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

TZrInt64 ZrCore_Module_ImportNativeEntry(SZrState *state) {
    return module_loader_import_native_entry(state, ZR_FALSE);
}

TZrInt64 ZrCore_Module_ImportGuardNativeEntry(SZrState *state) {
    return module_loader_import_native_entry(state, ZR_TRUE);
}

struct SZrObjectModule *ZrCore_Module_ImportByPath(SZrState *state, SZrString *path) {
    struct SZrObjectModule *cachedModule;
    SZrGlobalState *global;
    TZrNativeString pathStr;
    TZrSize pathLen;
    SZrIo io;
    SZrFunction *func;
    SZrClosure *closure;
    struct SZrObjectModule *module;
    TZrUInt64 pathHash;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer scratchBase;
    TZrStackValuePointer callBase;
    TZrSize frameStorageSlotCount;
    SZrFunctionStackAnchor callBaseAnchor;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor scratchBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    TZrBool hasSavedCallInfoAnchors = ZR_FALSE;
    SZrCallInfo *savedCallInfo;

    if (state == ZR_NULL || state->global == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    cachedModule = ZrCore_Module_GetFromCache(state, path);
    if (cachedModule != ZR_NULL) {
        return cachedModule;
    }

    global = state->global;
    ZrCore_GlobalState_ClearModuleLoadDiagnostic(global);
    if (global->aotModuleLoader != ZR_NULL) {
        struct SZrObjectModule *aotModule =
                global->aotModuleLoader(state, path, global->aotModuleLoaderUserData);
        if (aotModule != ZR_NULL) {
            if (aotModule->fullPath == ZR_NULL || aotModule->moduleName == ZR_NULL) {
                TZrUInt64 aotPathHash = ZrCore_Module_CalculatePathHash(state, path);
                ZrCore_Module_SetInfo(state, aotModule, path, aotPathHash, path);
            }

            ZrCore_Module_AddToCache(state, path, aotModule);
            return aotModule;
        }

        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_NULL;
        }

    }

    if (global->nativeModuleLoader != ZR_NULL) {
        struct SZrObjectModule *nativeModule =
                global->nativeModuleLoader(state, path, global->nativeModuleLoaderUserData);
        if (nativeModule != ZR_NULL) {
            if (nativeModule->fullPath == ZR_NULL || nativeModule->moduleName == ZR_NULL) {
                TZrUInt64 nativePathHash = ZrCore_Module_CalculatePathHash(state, path);
                ZrCore_Module_SetInfo(state, nativeModule, path, nativePathHash, path);
            }

            ZrCore_Module_AddToCache(state, path, nativeModule);
            return nativeModule;
        }

        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_NULL;
        }

    }

    if (global->sourceLoader == ZR_NULL) {
        if (ZrCore_GlobalState_GetModuleLoadDiagnostic(global) == ZR_NULL) {
            ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=source result=unconfigured");
        }
        return ZR_NULL;
    }

    if (path->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrCore_String_GetNativeStringShort(path);
        pathLen = path->shortStringLength;
    } else {
        pathStr = *ZrCore_String_GetNativeStringLong(path);
        pathLen = path->longStringLength;
    }

    if (pathStr == ZR_NULL || pathLen == 0) {
        ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=source result=empty-path");
        return ZR_NULL;
    }

    if (!global->sourceLoader(state, pathStr, ZR_NULL, &io)) {
        if (ZrCore_GlobalState_GetModuleLoadDiagnostic(global) == ZR_NULL) {
            ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=source result=unavailable");
        }
        return ZR_NULL;
    }

    func = ZR_NULL;
    if (io.isBinary) {
        SZrIoSource *ioSource = ZrCore_Io_ReadSourceNew(&io);
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (ioSource == ZR_NULL) {
            ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=binary-reader result=read-failed");
            return ZR_NULL;
        }

        func = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
        ZrCore_Io_ReadSourceFree(global, ioSource);
        if (func == ZR_NULL) {
            if (ZrCore_GlobalState_GetModuleLoadDiagnostic(global) == ZR_NULL) {
                ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=binary-runtime result=load-failed");
            }
            return ZR_NULL;
        }
    } else {
        TZrSize sourceSize = 0;
        TZrBytePtr sourceBuffer = read_all_from_io(state, &io, &sourceSize);
        if (sourceBuffer == ZR_NULL) {
            if (io.close != ZR_NULL) {
                io.close(state, io.customData);
            }
            ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=source-reader result=read-failed");
            return ZR_NULL;
        }

        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (sourceSize == 0) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=source-reader result=empty-source");
            return ZR_NULL;
        }

        if (global->compileSource == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=source-compiler result=unconfigured");
            return ZR_NULL;
        }

        {
            SZrString *sourceName = ZrCore_String_Create(state, pathStr, pathLen);
            func = global->compileSource(state, (const TZrChar *)sourceBuffer, sourceSize, sourceName);
        }
        ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);

        if (func == ZR_NULL) {
            if (ZrCore_GlobalState_GetModuleLoadDiagnostic(global) == ZR_NULL) {
                ZrCore_GlobalState_SetModuleLoadDiagnostic(global, "loader=source-compiler result=compile-failed");
            }
            return ZR_NULL;
        }
    }

    module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    pathHash = ZrCore_Module_CalculatePathHash(state, path);
    ZrCore_Module_SetInfo(state, module, path, pathHash, path);
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, module, func);

    if (func != ZR_NULL) {
        ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
    }

    if (!module_loader_register_export_descriptors(state, module, func)) {
        return ZR_NULL;
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        ZrCore_Function_Free(state, func);
        return ZR_NULL;
    }

    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    scratchBase = module_loader_resolve_scratch_base(savedStackTop, savedCallInfo);
    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    ZrCore_Function_StackAnchorInit(state, scratchBase, &scratchBaseAnchor);
    if (savedCallInfo != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &savedCallInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &savedCallInfoTopAnchor);
        hasSavedCallInfoAnchors = ZR_TRUE;
    }
    frameStorageSlotCount = ZrCore_Function_GetFrameStorageSlotCount(func);
    callBase = ZrCore_Function_ReserveScratchSlots(state, frameStorageSlotCount + 1, scratchBase);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    callBase = ZrCore_Function_StackAnchorRestore(state, &scratchBaseAnchor);
    if (hasSavedCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &savedCallInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &savedCallInfoTopAnchor);
    }

    func = module_loader_refresh_forwarded_function(func);
    closure = module_loader_refresh_forwarded_closure(closure);
    if (func == ZR_NULL || closure == ZR_NULL) {
        state->stackTop.valuePointer = savedStackTop;
        return ZR_NULL;
    }
    frameStorageSlotCount = ZrCore_Function_GetFrameStorageSlotCount(func);
    closure->function = func;

    ZrCore_Function_StackAnchorInit(state, callBase, &callBaseAnchor);
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase));
    ZrCore_Stack_SetRawObjectValue(state, callBase, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = callBase + 1 + frameStorageSlotCount;
    ZrCore_Function_InitializeFrameLayoutStorage(state, callBase, func, 0u);

    if (!module_loader_preinstall_top_level_callables(state, module, func, callBase + 1)) {
        state->stackTop.valuePointer = savedStackTop;
        return ZR_NULL;
    }

    ZrCore_Module_SetInitializationState(module, ZR_MODULE_INIT_STATE_INITIALIZING);
    ZrCore_Module_AddToCache(state, path, module);

    {
        SZrModuleLoaderExecuteRequest request;
        EZrThreadStatus status;

        request.anchor = &callBaseAnchor;
        request.resultBase = ZR_NULL;
        status = ZrCore_Exception_TryRun(state, module_loader_execute_entry_body, &request);
        if (status != ZR_THREAD_STATUS_FINE) {
            ZrCore_Module_SetInitializationState(module, ZR_MODULE_INIT_STATE_FAILED);
            state->stackTop.valuePointer = savedStackTop;
            ZrCore_Exception_Throw(state, status);
            return ZR_NULL;
        }

        callBase = request.resultBase;
    }

    if (!module_loader_refresh_declaration_exports(state, module, func, callBase)) {
        ZrCore_Module_SetInitializationState(module, ZR_MODULE_INIT_STATE_FAILED);
        state->stackTop.valuePointer = savedStackTop;
        return ZR_NULL;
    }
    module_loader_backfill_entry_exports(state, module, func, callBase);
    if (state->stackTop.valuePointer < callBase + 1 + frameStorageSlotCount) {
        state->stackTop.valuePointer = callBase + 1 + frameStorageSlotCount;
    }
    // Export refresh/backfill can synthesize new closures after the entry frame
    // has already returned. Close any remaining module-frame upvalues now so
    // exported closures do not keep dangling references into reusable stack slots.
    ZrCore_Closure_CloseClosure(state,
                                callBase + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);
    if (module_loader_registry_has_other_initializing_modules(state, module)) {
        module->reserved0 = (TZrUInt8)(module->reserved0 | ZR_MODULE_RUNTIME_PENDING_ENTRY_EXPORTS);
        module_loader_set_entry_export_descriptors_ready(module, ZR_FALSE);
    } else {
        module_loader_set_entry_export_descriptors_ready(module, ZR_TRUE);
        module_loader_finalize_pending_entry_exports(state);
    }
    ZrCore_Module_SetInitializationState(module, ZR_MODULE_INIT_STATE_READY);

    state->stackTop.valuePointer = savedStackTop;
    return module;
}
