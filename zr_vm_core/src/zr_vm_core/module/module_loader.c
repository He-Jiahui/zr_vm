//
// Module import and source loading helpers.
//

#include "module/module_internal.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/reflection.h"

#include <stdio.h>

#define ZR_MODULE_RUNTIME_PENDING_ENTRY_EXPORTS ((TZrUInt8)1)

typedef struct SZrModuleLoaderExecuteRequest {
    const SZrFunctionStackAnchor *anchor;
    TZrStackValuePointer resultBase;
} SZrModuleLoaderExecuteRequest;

static ZR_FORCE_INLINE TZrStackValuePointer module_loader_resolve_scratch_base(TZrStackValuePointer savedStackTop,
                                                                               SZrCallInfo *savedCallInfo) {
    TZrStackValuePointer scratchBase = savedStackTop;

    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer != ZR_NULL &&
        (scratchBase == ZR_NULL || scratchBase < savedCallInfo->functionTop.valuePointer)) {
        scratchBase = savedCallInfo->functionTop.valuePointer;
    }

    return scratchBase;
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

static TZrBool module_loader_slot_matches_function(SZrState *state,
                                                   SZrTypeValue *value,
                                                   SZrFunction *function) {
    SZrFunction *existingFunction;

    if (state == ZR_NULL || value == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    existingFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
    return existingFunction == function ? ZR_TRUE : ZR_FALSE;
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

    slotPointer = base + exported->stackSlot;
    slotValue = ZrCore_Stack_GetValue(slotPointer);
    childFunction = &function->childFunctionList[exported->callableChildIndex];
    if (slotValue == ZR_NULL || childFunction == ZR_NULL) {
        return ZR_FALSE;
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

        slotPointer = base + binding->stackSlot;
        slotValue = ZrCore_Stack_GetValue(slotPointer);
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
    TZrStackValuePointer exportedValuesTop;

    if (state == ZR_NULL || module == ZR_NULL || function == ZR_NULL || callBase == ZR_NULL ||
        function->exportedVariables == ZR_NULL || function->exportedVariableLength == 0) {
        return;
    }

    exportedValuesTop = callBase + 1 + function->stackSize;
    for (index = 0; index < function->exportedVariableLength; ++index) {
        const SZrFunctionExportedVariable *exportVar = &function->exportedVariables[index];
        TZrStackValuePointer varPointer;
        SZrTypeValue *varValue;

        if (exportVar->name == ZR_NULL || exportVar->readiness != ZR_MODULE_EXPORT_READY_ENTRY) {
            continue;
        }

        varPointer = callBase + 1 + exportVar->stackSlot;
        if (varPointer >= exportedValuesTop) {
            continue;
        }

        varValue = ZrCore_Stack_GetValue(varPointer);
        if (varValue == ZR_NULL) {
            continue;
        }

        {
            SZrFunction *childFunction = module_loader_find_child_function_for_value(state, function, varValue);
            if (childFunction != ZR_NULL) {
                ZrCore_Closure_PushToStack(state, childFunction, ZR_NULL, callBase + 1, varPointer);
                varValue = ZrCore_Stack_GetValue(varPointer);
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

TZrInt64 ZrCore_Module_ImportNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue *result;
    SZrTypeValue *pathValue;
    SZrString *path;
    struct SZrObjectModule *module;

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
    module = ZrCore_Module_ImportByPath(state, path);

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
    }

    if (global->sourceLoader == ZR_NULL) {
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
        return ZR_NULL;
    }

    if (!global->sourceLoader(state, pathStr, ZR_NULL, &io)) {
        return ZR_NULL;
    }

    func = ZR_NULL;
    if (io.isBinary) {
        SZrIoSource *ioSource = ZrCore_Io_ReadSourceNew(&io);
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (ioSource == ZR_NULL) {
            return ZR_NULL;
        }

        func = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
        ZrCore_Io_ReadSourceFree(global, ioSource);
        if (func == ZR_NULL) {
            return ZR_NULL;
        }
    } else {
        TZrSize sourceSize = 0;
        TZrBytePtr sourceBuffer = read_all_from_io(state, &io, &sourceSize);
        if (sourceBuffer == ZR_NULL) {
            if (io.close != ZR_NULL) {
                io.close(state, io.customData);
            }
            return ZR_NULL;
        }

        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (sourceSize == 0) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            return ZR_NULL;
        }

        if (global->compileSource == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            return ZR_NULL;
        }

        {
            SZrString *sourceName = ZrCore_String_Create(state, pathStr, pathLen);
            func = global->compileSource(state, (const TZrChar *)sourceBuffer, sourceSize, sourceName);
        }
        ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);

        if (func == ZR_NULL) {
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
    closure->function = func;

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
    callBase = ZrCore_Function_ReserveScratchSlots(state, func->stackSize + 1, scratchBase);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    callBase = ZrCore_Function_StackAnchorRestore(state, &scratchBaseAnchor);
    if (hasSavedCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &savedCallInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &savedCallInfoTopAnchor);
    }
    ZrCore_Function_StackAnchorInit(state, callBase, &callBaseAnchor);
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase));
    ZrCore_Stack_SetRawObjectValue(state, callBase, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = callBase + 1 + func->stackSize;

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
    if (state->stackTop.valuePointer < callBase + 1 + func->stackSize) {
        state->stackTop.valuePointer = callBase + 1 + func->stackSize;
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
