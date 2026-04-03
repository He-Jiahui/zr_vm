//
// Created by HeJiahui on 2025/8/6.
//

#include "module_internal.h"

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
    }
    ZrCore_Value_Copy(state, &pair->value, value);
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
    }
    ZrCore_Value_Copy(state, &pair->value, value);
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
