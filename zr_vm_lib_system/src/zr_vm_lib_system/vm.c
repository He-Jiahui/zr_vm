//
// zr.system.vm callbacks.
//

#include "zr_vm_lib_system/vm.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/native_registry.h"

#include <stdlib.h>
#include <string.h>

static void system_vm_write_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void system_vm_write_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void system_vm_write_string_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const TZrChar *value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetString(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static TZrBool system_vm_read_string_argument(const ZrLibCallContext *context,
                                              TZrSize index,
                                              const TZrChar **outText) {
    SZrString *textString = ZR_NULL;

    if (context == ZR_NULL || outText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadString(context, index, &textString) || textString == ZR_NULL) {
        return ZR_FALSE;
    }

    *outText = ZrCore_String_GetNativeString(textString);
    return *outText != ZR_NULL;
}

static TZrInt64 system_vm_loaded_module_count(SZrState *state) {
    SZrObject *registry;
    TZrSize bucket;
    TZrSize count = 0;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        !ZrCore_Value_IsGarbageCollectable(&state->global->loadedModulesRegistry) ||
        state->global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return 0;
    }

    registry = ZR_CAST_OBJECT(state, state->global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL || !registry->nodeMap.isValid) {
        return 0;
    }

    for (bucket = 0; bucket < registry->nodeMap.capacity; bucket++) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucket];
        while (pair != ZR_NULL) {
            count++;
            pair = pair->next;
        }
    }

    return (TZrInt64)count;
}

static SZrObject *system_vm_make_state(SZrState *state, TZrInt64 loadedModuleCount) {
    SZrObject *object;
    SZrGarbageCollector *garbageCollector;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, "SystemVmState");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    garbageCollector = state->global != ZR_NULL ? state->global->garbageCollector : ZR_NULL;
    system_vm_write_int_field(state, object, "loadedModuleCount", loadedModuleCount);
    system_vm_write_int_field(state,
                              object,
                              "garbageCollectionMode",
                              garbageCollector != ZR_NULL ? (TZrInt64)garbageCollector->gcMode : 0);
    system_vm_write_int_field(state,
                              object,
                              "garbageCollectionDebt",
                              garbageCollector != ZR_NULL ? (TZrInt64)garbageCollector->gcDebtSize : 0);
    system_vm_write_int_field(state,
                              object,
                              "garbageCollectionThreshold",
                              garbageCollector != ZR_NULL ? (TZrInt64)garbageCollector->gcPauseThresholdPercent : 0);
    system_vm_write_int_field(state,
                              object,
                              "stackDepth",
                              (TZrInt64)(state->stackTop.valuePointer - state->stackBase.valuePointer));
    system_vm_write_int_field(state, object, "frameDepth", (TZrInt64)state->callInfoListLength);
    return object;
}

static SZrObject *system_vm_make_loaded_module_info(SZrState *state,
                                                    const TZrChar *name,
                                                    const TZrChar *sourceKind,
                                                    const TZrChar *sourcePath,
                                                    TZrBool hasTypeHints) {
    SZrObject *object;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, "SystemLoadedModuleInfo");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    if (name != ZR_NULL) {
        system_vm_write_string_field(state, object, "name", name);
    }
    if (sourceKind != ZR_NULL) {
        system_vm_write_string_field(state, object, "sourceKind", sourceKind);
    }
    if (sourcePath != ZR_NULL) {
        system_vm_write_string_field(state, object, "sourcePath", sourcePath);
    }
    system_vm_write_bool_field(state, object, "hasTypeHints", hasTypeHints);
    return object;
}

TZrBool ZrSystem_Vm_LoadedModules(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *array;
    SZrObject *registry;
    TZrSize bucket;

    if (context == ZR_NULL || result == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    array = ZrLib_Array_New(context->state);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_Value_IsGarbageCollectable(&context->state->global->loadedModulesRegistry) ||
        context->state->global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        ZrLib_Value_SetObject(context->state, result, array, ZR_VALUE_TYPE_ARRAY);
        return ZR_TRUE;
    }

    registry = ZR_CAST_OBJECT(context->state, context->state->global->loadedModulesRegistry.value.object);
    for (bucket = 0; registry != ZR_NULL && bucket < registry->nodeMap.capacity; bucket++) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucket];
        while (pair != ZR_NULL) {
            const TZrChar *name =
                    pair->key.type == ZR_VALUE_TYPE_STRING
                            ? ZrCore_String_GetNativeString(ZR_CAST_STRING(context->state, pair->key.value.object))
                            : ZR_NULL;
            SZrObjectModule *module =
                    pair->value.type == ZR_VALUE_TYPE_OBJECT ? (SZrObjectModule *)pair->value.value.object : ZR_NULL;
            const TZrChar *sourcePath =
                    module != ZR_NULL && module->fullPath != ZR_NULL ? ZrCore_String_GetNativeString(module->fullPath) : name;
            const TZrChar *sourceKind =
                    (module != ZR_NULL &&
                     module->moduleName != ZR_NULL &&
                     module->fullPath != ZR_NULL &&
                     strcmp(ZrCore_String_GetNativeString(module->moduleName),
                            ZrCore_String_GetNativeString(module->fullPath)) == 0)
                            ? "native"
                            : "source";
            const ZrLibModuleDescriptor *descriptor =
                    name != ZR_NULL ? ZrLibrary_NativeRegistry_FindModule(context->state->global, name) : ZR_NULL;
            SZrObject *info = system_vm_make_loaded_module_info(
                    context->state,
                    name,
                    sourceKind,
                    sourcePath,
                    descriptor != ZR_NULL && (descriptor->typeHintsJson != ZR_NULL || descriptor->typeHintCount > 0));

            if (info != ZR_NULL) {
                SZrTypeValue entryValue;
                ZrLib_Value_SetObject(context->state, &entryValue, info, ZR_VALUE_TYPE_OBJECT);
                ZrLib_Array_PushValue(context->state, array, &entryValue);
            }

            pair = pair->next;
        }
    }

    ZrLib_Value_SetObject(context->state, result, array, ZR_VALUE_TYPE_ARRAY);
    return ZR_TRUE;
}

TZrBool ZrSystem_Vm_State(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *vmState;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    vmState = system_vm_make_state(context->state, system_vm_loaded_module_count(context->state));
    if (vmState == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, vmState, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrSystem_Vm_CallModuleExport(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *moduleName = ZR_NULL;
    const TZrChar *exportName = ZR_NULL;
    SZrObject *argumentsArray = ZR_NULL;
    SZrTypeValue *arguments = ZR_NULL;
    TZrSize argumentCount;
    TZrSize index;
    TZrBool success;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_vm_read_string_argument(context, 0, &moduleName) ||
        !system_vm_read_string_argument(context, 1, &exportName) ||
        !ZrLib_CallContext_ReadArray(context, 2, &argumentsArray)) {
        return ZR_FALSE;
    }

    argumentCount = ZrLib_Array_Length(argumentsArray);
    if (argumentCount > 0) {
        arguments = (SZrTypeValue *)malloc(sizeof(SZrTypeValue) * argumentCount);
        if (arguments == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < argumentCount; index++) {
        const SZrTypeValue *item = ZrLib_Array_Get(context->state, argumentsArray, index);
        if (item == ZR_NULL) {
            free(arguments);
            return ZR_FALSE;
        }
        arguments[index] = *item;
    }

    success = ZrLib_CallModuleExport(context->state, moduleName, exportName, arguments, argumentCount, result);
    free(arguments);
    return success;
}
