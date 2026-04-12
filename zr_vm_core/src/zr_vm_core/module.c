//
// Created by HeJiahui on 2025/8/6.
//

#include "module_internal.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_core/gc.h"

static void zr_module_barrier_string_field(SZrState *state,
                                           struct SZrObjectModule *module,
                                           SZrString *stringValue) {
    if (state == ZR_NULL || module == ZR_NULL || stringValue == ZR_NULL) {
        return;
    }

    ZrCore_RawObject_Barrier(state,
                             ZR_CAST_RAW_OBJECT_AS_SUPER(module),
                             ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
}

static void zr_module_barrier_hash_pair(SZrState *state,
                                        struct SZrObjectModule *module,
                                        SZrHashKeyValuePair *pair) {
    if (state == ZR_NULL || module == ZR_NULL || pair == ZR_NULL) {
        return;
    }

    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module), &pair->key);
    ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module), &pair->value);
}

struct SZrObjectModule *ZrCore_Module_Create(SZrState *state) {
    SZrObject *object;
    struct SZrObjectModule *module;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrCore_Object_NewCustomized(state, sizeof(struct SZrObjectModule), ZR_OBJECT_INTERNAL_TYPE_MODULE);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    module = (struct SZrObjectModule *)object;
    module->moduleName = ZR_NULL;
    module->pathHash = 0;
    module->fullPath = ZR_NULL;
    module->initState = ZR_MODULE_INIT_STATE_UNINITIALIZED;
    module->reserved0 = 0;
    module->reserved1 = 0;
    module->exportDescriptors = ZR_NULL;
    module->exportDescriptorLength = 0;

    ZrCore_HashSet_Init(state, &module->super.nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    ZrCore_HashSet_Construct(&module->proNodeMap);
    ZrCore_HashSet_Init(state, &module->proNodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    return module;
}

void ZrCore_Module_SetInfo(SZrState *state,
                           struct SZrObjectModule *module,
                           SZrString *moduleName,
                           TZrUInt64 pathHash,
                           SZrString *fullPath) {
    if (state == ZR_NULL || module == ZR_NULL) {
        return;
    }

    module->moduleName = moduleName;
    module->pathHash = pathHash;
    module->fullPath = fullPath;
    zr_module_barrier_string_field(state, module, moduleName);
    zr_module_barrier_string_field(state, module, fullPath);
}

void ZrCore_Module_AddPubExport(SZrState *state,
                                struct SZrObjectModule *module,
                                SZrString *name,
                                const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, name);
    ZrCore_Object_SetValue(state, &module->super, &key, value);

    pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &module->proNodeMap, &key);
        if (pair == ZR_NULL) {
            return;
        }
    }
    ZrCore_Value_Copy(state, &pair->value, value);
    zr_module_barrier_hash_pair(state, module, pair);
    ZrCore_GarbageCollector_MarkValueEscaped(state,
                                             value,
                                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT,
                                             ZR_GC_SCOPE_DEPTH_NONE,
                                             ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT);
}

void ZrCore_Module_AddProExport(SZrState *state,
                                struct SZrObjectModule *module,
                                SZrString *name,
                                const SZrTypeValue *value) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL || value == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, name);
    pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrCore_HashSet_Add(state, &module->proNodeMap, &key);
        if (pair == ZR_NULL) {
            return;
        }
    }
    ZrCore_Value_Copy(state, &pair->value, value);
    zr_module_barrier_hash_pair(state, module, pair);
    ZrCore_GarbageCollector_MarkValueEscaped(state,
                                             value,
                                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT,
                                             ZR_GC_SCOPE_DEPTH_NONE,
                                             ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT);
}

const SZrTypeValue *ZrCore_Module_GetPubExport(SZrState *state,
                                               struct SZrObjectModule *module,
                                               SZrString *name) {
    SZrTypeValue key;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, name);
    return ZrCore_Object_GetValue(state, &module->super, &key);
}

const SZrTypeValue *ZrCore_Module_GetProExport(SZrState *state,
                                               struct SZrObjectModule *module,
                                               SZrString *name) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, name);
    pair = ZrCore_HashSet_Find(state, &module->proNodeMap, &key);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    return &pair->value;
}

TZrUInt64 ZrCore_Module_CalculatePathHash(SZrState *state, SZrString *fullPath) {
    TZrNativeString pathStr;
    TZrSize pathLen;

    ZR_UNUSED_PARAMETER(state);
    if (fullPath == ZR_NULL) {
        return 0;
    }

    if (fullPath->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrCore_String_GetNativeStringShort(fullPath);
        pathLen = fullPath->shortStringLength;
    } else {
        pathStr = *ZrCore_String_GetNativeStringLong(fullPath);
        pathLen = fullPath->longStringLength;
    }

    if (pathStr == ZR_NULL || pathLen == 0) {
        return 0;
    }

    return XXH3_64bits(pathStr, pathLen);
}

struct SZrObjectModule *ZrCore_Module_GetFromCache(SZrState *state, SZrString *path) {
    SZrObject *registry;
    SZrTypeValue key;
    const SZrTypeValue *cachedValue;
    SZrObject *cachedObject;

    if (state == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, path);
    cachedValue = ZrCore_Object_GetValue(state, registry, &key);
    if (cachedValue == ZR_NULL || cachedValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    cachedObject = ZR_CAST_OBJECT(state, cachedValue->value.object);
    if (cachedObject == ZR_NULL || cachedObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_NULL;
    }

    return (struct SZrObjectModule *)cachedObject;
}

void ZrCore_Module_AddToCache(SZrState *state, SZrString *path, struct SZrObjectModule *module) {
    SZrObject *registry;
    SZrTypeValue key;
    SZrTypeValue moduleValue;

    if (state == ZR_NULL || path == ZR_NULL || module == ZR_NULL) {
        return;
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, path);
    zr_module_init_object_value(state, &moduleValue, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
    ZrCore_Object_SetValue(state, registry, &key, &moduleValue);
}

void ZrCore_Module_RemoveFromCache(SZrState *state, SZrString *path) {
    SZrObject *registry;
    SZrTypeValue key;

    if (state == ZR_NULL || path == ZR_NULL) {
        return;
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, path);
    ZrCore_HashSet_Remove(state, &registry->nodeMap, &key);
}

const SZrModuleExportDescriptor *ZrCore_Module_FindExportDescriptor(struct SZrObjectModule *module, SZrString *name) {
    TZrUInt32 index;

    if (module == ZR_NULL || name == ZR_NULL || module->exportDescriptors == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < module->exportDescriptorLength; ++index) {
        SZrModuleExportDescriptor *descriptor = &module->exportDescriptors[index];
        if (descriptor->name == name || (descriptor->name != ZR_NULL && ZrCore_String_Equal(descriptor->name, name))) {
            return descriptor;
        }
    }

    return ZR_NULL;
}

SZrModuleExportDescriptor *ZrCore_Module_FindExportDescriptorMutable(struct SZrObjectModule *module, SZrString *name) {
    return (SZrModuleExportDescriptor *)ZrCore_Module_FindExportDescriptor(module, name);
}

TZrBool ZrCore_Module_RegisterExportDescriptor(SZrState *state,
                                               struct SZrObjectModule *module,
                                               const SZrModuleExportDescriptor *descriptor) {
    SZrModuleExportDescriptor *newDescriptors;
    TZrUInt32 newLength;
    TZrUInt32 index;

    if (state == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < module->exportDescriptorLength; ++index) {
        SZrModuleExportDescriptor *existing = &module->exportDescriptors[index];
        if (existing->name == descriptor->name ||
            (existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, descriptor->name))) {
            *existing = *descriptor;
            zr_module_barrier_string_field(state, module, existing->name);
            return ZR_TRUE;
        }
    }

    newLength = module->exportDescriptorLength + 1;
    newDescriptors = (SZrModuleExportDescriptor *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                                   sizeof(*newDescriptors) * newLength,
                                                                                   ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (newDescriptors == ZR_NULL) {
        return ZR_FALSE;
    }

    if (module->exportDescriptors != ZR_NULL && module->exportDescriptorLength > 0) {
        ZrCore_Memory_RawCopy(newDescriptors,
                              module->exportDescriptors,
                              sizeof(*newDescriptors) * module->exportDescriptorLength);
        ZrCore_Memory_RawFreeWithType(state->global,
                                      module->exportDescriptors,
                                      sizeof(*newDescriptors) * module->exportDescriptorLength,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }

    newDescriptors[module->exportDescriptorLength] = *descriptor;
    module->exportDescriptors = newDescriptors;
    module->exportDescriptorLength = newLength;
    zr_module_barrier_string_field(state, module, newDescriptors[newLength - 1].name);
    return ZR_TRUE;
}

void ZrCore_Module_SetExportDescriptorReady(struct SZrObjectModule *module, struct SZrString *name, TZrBool isReady) {
    SZrModuleExportDescriptor *descriptor;

    if (module == ZR_NULL || name == ZR_NULL) {
        return;
    }

    descriptor = ZrCore_Module_FindExportDescriptorMutable(module, name);
    if (descriptor != ZR_NULL) {
        descriptor->isReady = isReady ? 1 : 0;
    }
}

void ZrCore_Module_SetInitializationState(struct SZrObjectModule *module, EZrModuleInitializationState state) {
    if (module == ZR_NULL) {
        return;
    }

    module->initState = (TZrUInt8)state;
}
