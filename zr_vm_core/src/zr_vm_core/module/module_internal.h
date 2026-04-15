//
// Internal helpers shared by split module translation units.
//

#ifndef ZR_VM_CORE_MODULE_INTERNAL_H
#define ZR_VM_CORE_MODULE_INTERNAL_H

#include "zr_vm_core/module.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"
#include "zr_vm_common/zr_string_conf.h"
#include "xxHash/xxhash.h"

typedef TZrUInt8 EZrAccessModifier;

typedef struct {
    SZrObjectPrototype *prototype;
    SZrString *typeName;
    EZrObjectPrototypeType prototypeType;
    EZrAccessModifier accessModifier;
    TZrUInt64 protocolMask;
    TZrUInt32 modifierFlags;
    TZrUInt32 nextVirtualSlotIndex;
    TZrUInt32 nextPropertyIdentity;
    TZrBool hasDecoratorMetadata;
    SZrTypeValue decoratorMetadataValue;
    SZrArray inheritTypeNames;
    const SZrCompiledMemberInfo *members;
    TZrUInt32 membersCount;
    TZrBool needsPostCreateSetup;
} SZrPrototypeCreationInfo;

static inline void zr_module_init_string_key(SZrState *state, SZrTypeValue *key, SZrString *stringValue) {
    ZrCore_Value_InitAsRawObject(state, key, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    key->type = ZR_VALUE_TYPE_STRING;
}

static inline void zr_module_init_object_value(SZrState *state, SZrTypeValue *value, SZrRawObject *objectValue) {
    ZrCore_Value_InitAsRawObject(state, value, objectValue);
    value->type = ZR_VALUE_TYPE_OBJECT;
}

static inline SZrObject *zr_module_get_loaded_modules_registry(SZrState *state) {
    SZrGlobalState *global;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    if (!ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) ||
        global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
}

#endif // ZR_VM_CORE_MODULE_INTERNAL_H
