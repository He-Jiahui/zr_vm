//
// `%type` runtime reflection implementation.
//

#include "zr_vm_core/reflection.h"

#include "module/module_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/runtime_decorator.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const TZrChar *kReflectionCacheFieldName = "__zr_reflection_cache";
static const TZrChar *kReflectionMarkerFieldName = "__zr_isReflection";
static const TZrChar *kReflectionOwnerModuleFieldName = "__zr_reflection_module";
static const TZrChar *kReflectionEntryFunctionFieldName = "__zr_reflection_entry_function";
static const TZrChar *kReflectionCollectionOrderFieldName = "__zr_reflection_order";

#define ZR_RUNTIME_DECLARATION_MODIFIER_ABSTRACT ((TZrUInt32)(1u << 0))
#define ZR_RUNTIME_DECLARATION_MODIFIER_VIRTUAL ((TZrUInt32)(1u << 1))
#define ZR_RUNTIME_DECLARATION_MODIFIER_OVERRIDE ((TZrUInt32)(1u << 2))
#define ZR_RUNTIME_DECLARATION_MODIFIER_FINAL ((TZrUInt32)(1u << 3))
#define ZR_RUNTIME_DECLARATION_MODIFIER_SHADOW ((TZrUInt32)(1u << 4))

static SZrObject *reflection_build_module_reflection(SZrState *state, SZrObjectModule *module);
static SZrObject *reflection_build_type_reflection(SZrState *state,
                                                   SZrObjectPrototype *prototype,
                                                   SZrObjectModule *module,
                                                   SZrFunction *entryFunction,
                                                   SZrObject *nativeTypeEntry);
static void reflection_populate_function_metadata(SZrState *state, SZrObject *reflectionObject, SZrFunction *function);
static void reflection_init_object_value(SZrState *state,
                                         SZrTypeValue *value,
                                         SZrRawObject *objectValue,
                                         EZrValueType valueType);
static TZrBool reflection_pin_raw_object(SZrState *state, SZrRawObject *object, TZrBool *addedByCaller);
static void reflection_unpin_raw_object(SZrGlobalState *global, SZrRawObject *object, TZrBool addedByCaller);
static TZrBool reflection_pin_value_object(SZrState *state, const SZrTypeValue *value, TZrBool *addedByCaller);
static void reflection_unpin_value_object(SZrGlobalState *global, const SZrTypeValue *value, TZrBool addedByCaller);
static void reflection_set_field_value(SZrState *state,
                                       SZrObject *object,
                                       const TZrChar *fieldName,
                                       const SZrTypeValue *value);
static void reflection_set_field_null(SZrState *state, SZrObject *object, const TZrChar *fieldName);
static void reflection_set_field_object(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *fieldName,
                                        SZrObject *fieldObject,
                                        EZrValueType valueType);
static SZrObject *reflection_get_field_object(SZrState *state,
                                              SZrObject *object,
                                              const TZrChar *fieldName,
                                              EZrValueType expectedType);
static SZrFunction *reflection_extract_function_from_value(SZrState *state, const SZrTypeValue *value);
static SZrObject *reflection_build_member_info(SZrState *state,
                                               const TZrChar *name,
                                               const TZrChar *qualifiedName,
                                               const TZrChar *kind,
                                               TZrUInt64 hash);
static SZrObject *reflection_build_callable_reflection(SZrState *state,
                                                       const TZrChar *name,
                                                       const TZrChar *qualifiedName,
                                                       const TZrChar *returnTypeName,
                                                       TZrUInt32 parameterCount,
                                                       TZrBool isStatic,
                                                       SZrObject *ownerReflection,
                                                       SZrObject *moduleReflection,
                                                       TZrUInt64 hash);
static void reflection_assign_owner_links(SZrState *state,
                                          SZrObject *memberReflection,
                                          SZrObject *ownerReflection,
                                          SZrObject *moduleReflection);
static SZrString *reflection_make_string(SZrState *state, const TZrChar *text);
static TZrBool reflection_build_type_of_value(SZrState *state,
                                              const SZrTypeValue *targetValue,
                                              SZrTypeValue *result);
static void reflection_populate_compiled_member_decorator_metadata(SZrState *state,
                                                                   SZrObject *memberReflection,
                                                                   SZrFunction *entryFunction,
                                                                   const SZrCompiledMemberInfo *member);
static SZrObject *reflection_build_type_literal_object_internal(SZrState *state, const TZrChar *typeName);
static const TZrChar *reflection_string_from_constant(SZrState *state,
                                                      SZrFunction *entryFunction,
                                                      TZrUInt32 constantIndex,
                                                      const TZrChar *fallback);
static TZrInt64 reflection_optional_slot_to_int(TZrUInt32 value);
static TZrBool reflection_modifier_flag_enabled(TZrUInt32 modifierFlags, TZrUInt32 modifierFlag);
static void reflection_apply_modifier_flags(SZrState *state,
                                            SZrObject *reflectionObject,
                                            TZrUInt32 modifierFlags);
static void reflection_populate_type_oop_metadata(SZrState *state,
                                                  SZrObject *typeReflection,
                                                  const SZrCompiledPrototypeInfo *prototypeInfo);
static void reflection_populate_compiled_member_oop_metadata(SZrState *state,
                                                             SZrObject *memberReflection,
                                                             SZrFunction *entryFunction,
                                                             const SZrCompiledMemberInfo *member);
static void reflection_get_prototype_metadata_context(SZrState *state,
                                                      SZrObjectPrototype *prototype,
                                                      SZrObjectModule **outModule,
                                                      SZrFunction **outEntryFunction);
static const TZrChar *reflection_prototype_qualified_name(SZrState *state,
                                                          SZrObjectPrototype *prototype,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize);

static SZrObjectPrototype *reflection_lookup_builtin_type_info_prototype(SZrState *state) {
    SZrObject *zrObject;
    struct SZrObjectModule *builtinModule;
    SZrString *moduleName;
    SZrString *exportName;
    SZrString *typeName;
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return ZR_NULL;
    }

    typeName = ZrCore_String_CreateFromNative(state, "TypeInfo");
    if (typeName == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    value = ZrCore_Object_GetValue(state, zrObject, &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        moduleName = ZrCore_String_CreateFromNative(state, "zr.builtin");
        exportName = ZrCore_String_CreateFromNative(state, "TypeInfo");
        if (moduleName == ZR_NULL || exportName == ZR_NULL) {
            return ZR_NULL;
        }

        builtinModule = ZrCore_Module_ImportByPath(state, moduleName);
        if (builtinModule == ZR_NULL) {
            return ZR_NULL;
        }

        value = ZrCore_Module_GetPubExport(state, builtinModule, exportName);
    }
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (ZR_CAST_OBJECT(state, value->value.object)->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)ZR_CAST_OBJECT(state, value->value.object);
}

static SZrObject *reflection_new_object(SZrState *state) {
    SZrObject *object;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(state, object);
    }
    return object;
}

static SZrObject *reflection_new_array(SZrState *state) {
    SZrObject *array;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(state, array);
    }
    return array;
}

static SZrObject *reflection_new_collection(SZrState *state) {
    SZrObject *collection = reflection_new_object(state);
    SZrObject *orderArray;

    if (collection == ZR_NULL) {
        return ZR_NULL;
    }

    orderArray = reflection_new_array(state);
    if (orderArray != ZR_NULL) {
        SZrTypeValue orderValue;
        reflection_init_object_value(state,
                                     &orderValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(orderArray),
                                     ZR_VALUE_TYPE_ARRAY);
        reflection_set_field_value(state, collection, kReflectionCollectionOrderFieldName, &orderValue);
    }

    return collection;
}

static SZrString *reflection_make_string(SZrState *state, const TZrChar *text) {
    TZrSize length;

    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    if (length == 0) {
        return ZrCore_String_CreateFromNative(state, "");
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, length);
}

static void reflection_init_string_key(SZrState *state, SZrTypeValue *key, SZrString *fieldName) {
    if (key == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key->type = ZR_VALUE_TYPE_STRING;
}

static void reflection_init_object_value(SZrState *state,
                                         SZrTypeValue *value,
                                         SZrRawObject *objectValue,
                                         EZrValueType valueType) {
    if (value == ZR_NULL || objectValue == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, value, objectValue);
    value->type = valueType;
}

static TZrBool reflection_pin_raw_object(SZrState *state, SZrRawObject *object, TZrBool *addedByCaller) {
    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrCore_GarbageCollector_IsObjectIgnored(state->global, object)) {
        return ZR_TRUE;
    }

    if (!ZrCore_GarbageCollector_IgnoreObject(state, object)) {
        return ZR_FALSE;
    }

    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_TRUE;
    }
    return ZR_TRUE;
}

static void reflection_unpin_raw_object(SZrGlobalState *global, SZrRawObject *object, TZrBool addedByCaller) {
    if (!addedByCaller || global == ZR_NULL || object == ZR_NULL) {
        return;
    }

    ZrCore_GarbageCollector_UnignoreObject(global, object);
}

static TZrBool reflection_pin_value_object(SZrState *state, const SZrTypeValue *value, TZrBool *addedByCaller) {
    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_TRUE;
    }

    return reflection_pin_raw_object(state, ZrCore_Value_GetRawObject(value), addedByCaller);
}

static void reflection_unpin_value_object(SZrGlobalState *global, const SZrTypeValue *value, TZrBool addedByCaller) {
    if (!addedByCaller || global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    reflection_unpin_raw_object(global, ZrCore_Value_GetRawObject(value), ZR_TRUE);
}

static void reflection_set_field_value(SZrState *state,
                                       SZrObject *object,
                                       const TZrChar *fieldName,
                                       const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;
    TZrBool objectPinned = ZR_FALSE;
    TZrBool keyPinned = ZR_FALSE;
    TZrBool valuePinned = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldString = reflection_make_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return;
    }

    reflection_init_string_key(state, &key, fieldString);
    if (!reflection_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &objectPinned) ||
        !reflection_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &keyPinned) ||
        !reflection_pin_value_object(state, value, &valuePinned)) {
        reflection_unpin_value_object(state->global, value, valuePinned);
        reflection_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), keyPinned);
        reflection_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinned);
        return;
    }

    ZrCore_Object_SetValue(state, object, &key, value);
    reflection_unpin_value_object(state->global, value, valuePinned);
    reflection_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), keyPinned);
    reflection_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinned);
}

static void reflection_set_field_string(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *fieldName,
                                        const TZrChar *value) {
    SZrString *fieldStringValue;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    if (value[0] == '\0') {
        reflection_set_field_null(state, object, fieldName);
        return;
    }

    fieldStringValue = reflection_make_string(state, value);
    if (fieldStringValue == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state,
                                 &fieldValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldStringValue),
                                 ZR_VALUE_TYPE_STRING);
    reflection_set_field_value(state, object, fieldName, &fieldValue);
}

static void reflection_set_field_bool(SZrState *state,
                                      SZrObject *object,
                                      const TZrChar *fieldName,
                                      TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZR_VALUE_FAST_SET(&fieldValue, nativeBool, value ? ZR_TRUE : ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    reflection_set_field_value(state, object, fieldName, &fieldValue);
}

static void reflection_set_field_int(SZrState *state,
                                     SZrObject *object,
                                     const TZrChar *fieldName,
                                     TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    reflection_set_field_value(state, object, fieldName, &fieldValue);
}

static void reflection_set_field_uint(SZrState *state,
                                      SZrObject *object,
                                      const TZrChar *fieldName,
                                      TZrUInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsUInt(state, &fieldValue, value);
    reflection_set_field_value(state, object, fieldName, &fieldValue);
}

static void reflection_set_field_null(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(&fieldValue);
    reflection_set_field_value(state, object, fieldName, &fieldValue);
}

static void reflection_set_field_object(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *fieldName,
                                        SZrObject *fieldObject,
                                        EZrValueType valueType) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || fieldObject == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state,
                                 &fieldValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject),
                                 valueType);
    reflection_set_field_value(state, object, fieldName, &fieldValue);
}

static TZrInt64 reflection_optional_slot_to_int(TZrUInt32 value) {
    return value == (TZrUInt32)-1 ? (TZrInt64)-1 : (TZrInt64)value;
}

static TZrBool reflection_modifier_flag_enabled(TZrUInt32 modifierFlags, TZrUInt32 modifierFlag) {
    return (modifierFlags & modifierFlag) != 0 ? ZR_TRUE : ZR_FALSE;
}

static void reflection_apply_modifier_flags(SZrState *state,
                                            SZrObject *reflectionObject,
                                            TZrUInt32 modifierFlags) {
    if (state == ZR_NULL || reflectionObject == ZR_NULL) {
        return;
    }

    reflection_set_field_int(state, reflectionObject, "modifierFlags", modifierFlags);
    reflection_set_field_bool(state,
                              reflectionObject,
                              "isAbstract",
                              reflection_modifier_flag_enabled(modifierFlags,
                                                               ZR_RUNTIME_DECLARATION_MODIFIER_ABSTRACT));
    reflection_set_field_bool(state,
                              reflectionObject,
                              "isVirtual",
                              reflection_modifier_flag_enabled(modifierFlags,
                                                               ZR_RUNTIME_DECLARATION_MODIFIER_VIRTUAL));
    reflection_set_field_bool(state,
                              reflectionObject,
                              "isOverride",
                              reflection_modifier_flag_enabled(modifierFlags,
                                                               ZR_RUNTIME_DECLARATION_MODIFIER_OVERRIDE));
    reflection_set_field_bool(state,
                              reflectionObject,
                              "isFinal",
                              reflection_modifier_flag_enabled(modifierFlags,
                                                               ZR_RUNTIME_DECLARATION_MODIFIER_FINAL));
    reflection_set_field_bool(state,
                              reflectionObject,
                              "isShadow",
                              reflection_modifier_flag_enabled(modifierFlags,
                                                               ZR_RUNTIME_DECLARATION_MODIFIER_SHADOW));
}

static void reflection_populate_type_oop_metadata(SZrState *state,
                                                  SZrObject *typeReflection,
                                                  const SZrCompiledPrototypeInfo *prototypeInfo) {
    if (state == ZR_NULL || typeReflection == ZR_NULL || prototypeInfo == ZR_NULL) {
        return;
    }

    reflection_apply_modifier_flags(state, typeReflection, prototypeInfo->modifierFlags);
    reflection_set_field_int(state, typeReflection, "nextVirtualSlotIndex", prototypeInfo->nextVirtualSlotIndex);
    reflection_set_field_int(state, typeReflection, "nextPropertyIdentity", prototypeInfo->nextPropertyIdentity);
}

static void reflection_populate_compiled_member_oop_metadata(SZrState *state,
                                                             SZrObject *memberReflection,
                                                             SZrFunction *entryFunction,
                                                             const SZrCompiledMemberInfo *member) {
    const TZrChar *ownerTypeName;
    const TZrChar *baseDefinitionOwnerTypeName;
    const TZrChar *baseDefinitionName;

    if (state == ZR_NULL || memberReflection == ZR_NULL || entryFunction == ZR_NULL || member == ZR_NULL) {
        return;
    }

    ownerTypeName =
            reflection_string_from_constant(state, entryFunction, member->ownerTypeNameStringIndex, "");
    baseDefinitionOwnerTypeName =
            reflection_string_from_constant(state,
                                            entryFunction,
                                            member->baseDefinitionOwnerTypeNameStringIndex,
                                            "");
    baseDefinitionName =
            reflection_string_from_constant(state, entryFunction, member->baseDefinitionNameStringIndex, "");

    reflection_apply_modifier_flags(state, memberReflection, member->modifierFlags);
    reflection_set_field_string(state, memberReflection, "ownerTypeName", ownerTypeName != ZR_NULL ? ownerTypeName : "");
    reflection_set_field_string(state,
                                memberReflection,
                                "baseDefinitionOwnerTypeName",
                                baseDefinitionOwnerTypeName != ZR_NULL ? baseDefinitionOwnerTypeName : "");
    reflection_set_field_string(state,
                                memberReflection,
                                "baseDefinitionName",
                                baseDefinitionName != ZR_NULL ? baseDefinitionName : "");
    reflection_set_field_int(state,
                             memberReflection,
                             "virtualSlotIndex",
                             reflection_optional_slot_to_int(member->virtualSlotIndex));
    reflection_set_field_int(state,
                             memberReflection,
                             "interfaceContractSlot",
                             reflection_optional_slot_to_int(member->interfaceContractSlot));
    reflection_set_field_int(state,
                             memberReflection,
                             "propertyIdentity",
                             reflection_optional_slot_to_int(member->propertyIdentity));
    reflection_set_field_int(state, memberReflection, "accessorRole", member->accessorRole);
}

static const SZrTypeValue *reflection_get_field_value(SZrState *state,
                                                      SZrObject *object,
                                                      const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = reflection_make_string(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_init_string_key(state, &key, fieldString);
    return ZrCore_Object_GetValue(state, object, &key);
}

static SZrObject *reflection_get_field_object(SZrState *state,
                                              SZrObject *object,
                                              const TZrChar *fieldName,
                                              EZrValueType expectedType) {
    const SZrTypeValue *fieldValue = reflection_get_field_value(state, object, fieldName);

    if (fieldValue == ZR_NULL || fieldValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (expectedType != ZR_VALUE_TYPE_NULL && fieldValue->type != expectedType) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, fieldValue->value.object);
}

static const TZrChar *reflection_get_field_string_native(SZrState *state,
                                                         SZrObject *object,
                                                         const TZrChar *fieldName,
                                                         const TZrChar *fallback) {
    const SZrTypeValue *fieldValue = reflection_get_field_value(state, object, fieldName);

    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_STRING || fieldValue->value.object == ZR_NULL) {
        return fallback;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, fieldValue->value.object));
}

static TZrBool reflection_get_field_bool_value(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               TZrBool fallback) {
    const SZrTypeValue *fieldValue = reflection_get_field_value(state, object, fieldName);

    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_BOOL) {
        return fallback;
    }

    return fieldValue->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
}

static TZrUInt32 reflection_array_length(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }
    return (TZrUInt32)array->nodeMap.elementCount;
}

static const SZrTypeValue *reflection_array_get(SZrState *state, SZrObject *array, TZrUInt32 index) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

static TZrBool reflection_array_push(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue key;
    TZrBool arrayPinned = ZR_FALSE;
    TZrBool valuePinned = ZR_FALSE;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)reflection_array_length(array));
    if (!reflection_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(array), &arrayPinned) ||
        !reflection_pin_value_object(state, value, &valuePinned)) {
        reflection_unpin_value_object(state->global, value, valuePinned);
        reflection_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(array), arrayPinned);
        return ZR_FALSE;
    }
    ZrCore_Object_SetValue(state, array, &key, value);
    reflection_unpin_value_object(state->global, value, valuePinned);
    reflection_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(array), arrayPinned);
    return ZR_TRUE;
}

static SZrObject *reflection_get_collection_order_array(SZrState *state,
                                                        SZrObject *collectionObject,
                                                        TZrBool createIfMissing) {
    SZrObject *orderArray;

    if (state == ZR_NULL || collectionObject == ZR_NULL) {
        return ZR_NULL;
    }

    orderArray = reflection_get_field_object(state, collectionObject, kReflectionCollectionOrderFieldName, ZR_VALUE_TYPE_ARRAY);
    if (orderArray == ZR_NULL && createIfMissing) {
        orderArray = reflection_new_array(state);
        if (orderArray != ZR_NULL) {
            reflection_set_field_object(state,
                                        collectionObject,
                                        kReflectionCollectionOrderFieldName,
                                        orderArray,
                                        ZR_VALUE_TYPE_ARRAY);
        }
    }

    return orderArray;
}

static void reflection_record_collection_key(SZrState *state, SZrObject *collectionObject, const TZrChar *memberName) {
    SZrObject *orderArray;
    SZrString *nameString;
    SZrTypeValue entryValue;

    if (state == ZR_NULL || collectionObject == ZR_NULL || memberName == ZR_NULL) {
        return;
    }

    orderArray = reflection_get_collection_order_array(state, collectionObject, ZR_TRUE);
    if (orderArray == ZR_NULL) {
        return;
    }

    nameString = reflection_make_string(state, memberName);
    if (nameString == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state,
                                 &entryValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(nameString),
                                 ZR_VALUE_TYPE_STRING);
    reflection_array_push(state, orderArray, &entryValue);
}

static SZrObject *reflection_get_or_create_member_bucket(SZrState *state,
                                                         SZrObject *membersObject,
                                                         const TZrChar *memberName) {
    const SZrTypeValue *existingValue;
    SZrObject *entriesArray;

    if (state == ZR_NULL || membersObject == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    existingValue = reflection_get_field_value(state, membersObject, memberName);
    if (existingValue != ZR_NULL && existingValue->type == ZR_VALUE_TYPE_ARRAY && existingValue->value.object != ZR_NULL) {
        return ZR_CAST_OBJECT(state, existingValue->value.object);
    }

    entriesArray = reflection_new_array(state);
    if (entriesArray == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_set_field_object(state, membersObject, memberName, entriesArray, ZR_VALUE_TYPE_ARRAY);
    reflection_record_collection_key(state, membersObject, memberName);
    return entriesArray;
}

static void reflection_add_named_entry(SZrState *state,
                                       SZrObject *membersObject,
                                       const TZrChar *memberName,
                                       SZrObject *entryObject) {
    SZrObject *entriesArray;
    SZrTypeValue entryValue;

    if (state == ZR_NULL || membersObject == ZR_NULL || memberName == ZR_NULL || entryObject == ZR_NULL) {
        return;
    }

    entriesArray = reflection_get_or_create_member_bucket(state, membersObject, memberName);
    if (entriesArray == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state, &entryValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entryObject), ZR_VALUE_TYPE_OBJECT);
    reflection_array_push(state, entriesArray, &entryValue);
}

static SZrObject *reflection_get_cache(SZrState *state) {
    SZrObject *zrObject;
    const SZrTypeValue *cacheValue;
    SZrObject *cacheObject;

    if (state == ZR_NULL || state->global == ZR_NULL || state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return ZR_NULL;
    }

    cacheValue = reflection_get_field_value(state, zrObject, kReflectionCacheFieldName);
    if (cacheValue != ZR_NULL && cacheValue->type == ZR_VALUE_TYPE_OBJECT && cacheValue->value.object != ZR_NULL) {
        return ZR_CAST_OBJECT(state, cacheValue->value.object);
    }

    cacheObject = reflection_new_object(state);
    if (cacheObject == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_set_field_object(state, zrObject, kReflectionCacheFieldName, cacheObject, ZR_VALUE_TYPE_OBJECT);
    return cacheObject;
}

static SZrObject *reflection_cache_get(SZrState *state, const SZrTypeValue *key) {
    SZrObject *cache = reflection_get_cache(state);
    const SZrTypeValue *cachedValue;

    if (cache == ZR_NULL || key == ZR_NULL) {
        return ZR_NULL;
    }

    cachedValue = ZrCore_Object_GetValue(state, cache, key);
    if (cachedValue == ZR_NULL || cachedValue->type != ZR_VALUE_TYPE_OBJECT || cachedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, cachedValue->value.object);
}

static void reflection_cache_put(SZrState *state, const SZrTypeValue *key, SZrObject *value) {
    SZrObject *cache = reflection_get_cache(state);
    SZrTypeValue cachedValue;

    if (cache == ZR_NULL || key == ZR_NULL || value == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state, &cachedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value), ZR_VALUE_TYPE_OBJECT);
    ZrCore_Object_SetValue(state, cache, key, &cachedValue);
}

static void reflection_init_default_sections(SZrState *state, SZrObject *reflectionObject) {
    SZrObject *membersObject = reflection_new_collection(state);
    SZrObject *metadataObject = reflection_new_object(state);
    SZrObject *sourceObject = reflection_new_object(state);
    SZrObject *compileTimeObject = reflection_new_object(state);
    SZrObject *layoutObject = reflection_new_object(state);
    SZrObject *ownershipObject = reflection_new_object(state);
    SZrObject *irObject = reflection_new_object(state);
    SZrObject *nativeOriginObject = reflection_new_object(state);

    reflection_set_field_object(state, reflectionObject, "members", membersObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, reflectionObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, reflectionObject, "decorators", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);
    reflection_set_field_object(state, reflectionObject, "genericParameters", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);
    reflection_set_field_object(state, reflectionObject, "source", sourceObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, reflectionObject, "tests", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);
    reflection_set_field_object(state, reflectionObject, "compileTime", compileTimeObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, reflectionObject, "layout", layoutObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, reflectionObject, "ownership", ownershipObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, reflectionObject, "ir", irObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, reflectionObject, "codeBlocks", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);
    reflection_set_field_object(state, reflectionObject, "nativeOrigin", nativeOriginObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_bool(state, reflectionObject, "mutable", ZR_TRUE);
    reflection_set_field_string(state, reflectionObject, "phase", "runtime");
    reflection_set_field_int(state, reflectionObject, "modifierFlags", 0);
    reflection_set_field_bool(state, reflectionObject, "isAbstract", ZR_FALSE);
    reflection_set_field_bool(state, reflectionObject, "isVirtual", ZR_FALSE);
    reflection_set_field_bool(state, reflectionObject, "isOverride", ZR_FALSE);
    reflection_set_field_bool(state, reflectionObject, "isFinal", ZR_FALSE);
    reflection_set_field_bool(state, reflectionObject, "isShadow", ZR_FALSE);
    reflection_set_field_null(state, reflectionObject, "ownerTypeName");
    reflection_set_field_null(state, reflectionObject, "baseDefinitionOwnerTypeName");
    reflection_set_field_null(state, reflectionObject, "baseDefinitionName");
    reflection_set_field_int(state, reflectionObject, "virtualSlotIndex", -1);
    reflection_set_field_int(state, reflectionObject, "interfaceContractSlot", -1);
    reflection_set_field_int(state, reflectionObject, "propertyIdentity", -1);
    reflection_set_field_int(state, reflectionObject, "accessorRole", 0);
    reflection_set_field_int(state, reflectionObject, "nextVirtualSlotIndex", 0);
    reflection_set_field_int(state, reflectionObject, "nextPropertyIdentity", 0);

    reflection_set_field_int(state, sourceObject, "startLine", 0);
    reflection_set_field_int(state, sourceObject, "endLine", 0);
    reflection_set_field_string(state, sourceObject, "functionName", "");

    reflection_set_field_bool(state, compileTimeObject, "hasTypedMetadata", ZR_FALSE);
    reflection_set_field_int(state, compileTimeObject, "typedLocalBindingCount", 0);
    reflection_set_field_int(state, compileTimeObject, "variableCount", 0);
    reflection_set_field_int(state, compileTimeObject, "functionCount", 0);
    reflection_set_field_object(state, compileTimeObject, "variables", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);
    reflection_set_field_object(state, compileTimeObject, "functions", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);

    reflection_set_field_int(state, layoutObject, "fieldCount", 0);
    reflection_set_field_int(state, layoutObject, "size", 0);
    reflection_set_field_int(state, layoutObject, "alignment", 0);

    reflection_set_field_int(state, ownershipObject, "qualifier", 0);
    reflection_set_field_bool(state, ownershipObject, "callsClose", ZR_FALSE);
    reflection_set_field_bool(state, ownershipObject, "callsDestructor", ZR_FALSE);

    reflection_set_field_int(state, irObject, "instructionCount", 0);
    reflection_set_field_int(state, irObject, "constantCount", 0);
    reflection_set_field_int(state, irObject, "localCount", 0);
    reflection_set_field_int(state, irObject, "closureCount", 0);
}

static void reflection_init_common_fields(SZrState *state,
                                          SZrObject *reflectionObject,
                                          const TZrChar *name,
                                          const TZrChar *qualifiedName,
                                          const TZrChar *kind,
                                          TZrUInt64 hash) {
    SZrObjectPrototype *typeInfoPrototype;

    if (state == ZR_NULL || reflectionObject == ZR_NULL) {
        return;
    }

    typeInfoPrototype = reflection_lookup_builtin_type_info_prototype(state);
    if (typeInfoPrototype != ZR_NULL) {
        reflectionObject->prototype = typeInfoPrototype;
    }

    reflection_set_field_bool(state, reflectionObject, kReflectionMarkerFieldName, ZR_TRUE);
    reflection_set_field_string(state, reflectionObject, "name", name != ZR_NULL ? name : "");
    reflection_set_field_string(state,
                                reflectionObject,
                                "qualifiedName",
                                qualifiedName != ZR_NULL ? qualifiedName : (name != ZR_NULL ? name : ""));
    reflection_set_field_uint(state, reflectionObject, "hash", hash);
    reflection_set_field_string(state, reflectionObject, "kind", kind != ZR_NULL ? kind : "object");
    reflection_set_field_null(state, reflectionObject, "owner");
    reflection_set_field_null(state, reflectionObject, "module");
    reflection_init_default_sections(state, reflectionObject);
}

static const TZrChar *reflection_prototype_qualified_name(SZrState *state,
                                                          SZrObjectPrototype *prototype,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    SZrObjectModule *module = ZR_NULL;
    SZrFunction *entryFunction = ZR_NULL;
    const TZrChar *typeName;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (state == ZR_NULL || prototype == ZR_NULL || prototype->name == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_NULL;
    }

    typeName = ZrCore_String_GetNativeString(prototype->name);
    if (typeName == ZR_NULL || typeName[0] == '\0') {
        return ZR_NULL;
    }

    reflection_get_prototype_metadata_context(state, prototype, &module, &entryFunction);
    ZR_UNUSED_PARAMETER(entryFunction);

    if (module != ZR_NULL && module->moduleName != ZR_NULL) {
        snprintf(buffer, bufferSize, "%s.%s", ZrCore_String_GetNativeString(module->moduleName), typeName);
    } else {
        snprintf(buffer, bufferSize, "%s", typeName);
    }

    return buffer;
}

static const TZrChar *reflection_prototype_kind_name(EZrObjectPrototypeType prototypeType) {
    switch (prototypeType) {
        case ZR_OBJECT_PROTOTYPE_TYPE_MODULE:
            return "module";
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "struct";
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return "interface";
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return "enum";
        default:
            return "class";
    }
}

static const TZrChar *reflection_builtin_type_name(EZrValueType valueType) {
    switch (valueType) {
        case ZR_VALUE_TYPE_NULL: return "null";
        case ZR_VALUE_TYPE_BOOL: return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: return "int";
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: return "uint";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE: return "float";
        case ZR_VALUE_TYPE_STRING: return "string";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE: return "callable";
        case ZR_VALUE_TYPE_ARRAY: return "array";
        case ZR_VALUE_TYPE_OBJECT: return "object";
        default: return "value";
    }
}

static const TZrChar *reflection_type_name_from_typed_type_ref(SZrState *state,
                                                               const SZrFunctionTypedTypeRef *typeRef,
                                                               char *buffer,
                                                               TZrSize bufferSize) {
    const TZrChar *elementTypeName;

    ZR_UNUSED_PARAMETER(state);

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }

    if (typeRef == ZR_NULL) {
        return "any";
    }

    if (!typeRef->isArray) {
        if (typeRef->typeName != ZR_NULL) {
            const TZrChar *typeName = ZrCore_String_GetNativeString(typeRef->typeName);
            if (typeName != ZR_NULL && typeName[0] != '\0') {
                return typeName;
            }
        }
        return reflection_builtin_type_name(typeRef->baseType);
    }

    if (buffer == ZR_NULL || bufferSize == 0) {
        return "array";
    }

    if (typeRef->elementTypeName != ZR_NULL) {
        elementTypeName = ZrCore_String_GetNativeString(typeRef->elementTypeName);
    } else {
        elementTypeName = reflection_builtin_type_name(typeRef->elementBaseType);
    }

    if (elementTypeName == ZR_NULL || elementTypeName[0] == '\0') {
        elementTypeName = "any";
    }

    snprintf(buffer, bufferSize, "%s[]", elementTypeName);
    return buffer;
}

static const SZrFunctionTypedLocalBinding *reflection_find_typed_local_binding(const SZrFunction *function,
                                                                               TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding != ZR_NULL && binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static SZrFunction *reflection_extract_function_from_constant_index(SZrState *state,
                                                                    SZrFunction *entryFunction,
                                                                    TZrUInt32 constantIndex) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || entryFunction->constantValueList == ZR_NULL ||
        constantIndex >= entryFunction->constantValueLength) {
        return ZR_NULL;
    }

    return reflection_extract_function_from_value(state, &entryFunction->constantValueList[constantIndex]);
}

static SZrObject *reflection_build_parameter_info(SZrState *state,
                                                  const TZrChar *name,
                                                  const TZrChar *typeName,
                                                  TZrUInt32 position,
                                                  SZrObject *ownerReflection,
                                                  SZrObject *moduleReflection,
                                                  TZrUInt64 hash) {
    SZrObject *parameterReflection = reflection_build_member_info(state,
                                                                  name != ZR_NULL ? name : "arg",
                                                                  name != ZR_NULL ? name : "arg",
                                                                  "parameter",
                                                                  hash);
    if (parameterReflection == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_assign_owner_links(state, parameterReflection, ownerReflection, moduleReflection);
    reflection_set_field_string(state, parameterReflection, "typeName", typeName != ZR_NULL ? typeName : "any");
    reflection_set_field_int(state, parameterReflection, "position", position);
    reflection_set_field_string(state, parameterReflection, "passingMode", "value");
    reflection_set_field_null(state, parameterReflection, "type");
    return parameterReflection;
}

static void reflection_attach_parameters_array(SZrState *state,
                                               SZrObject *callableReflection,
                                               SZrObject *parametersArray) {
    if (state == ZR_NULL || callableReflection == ZR_NULL || parametersArray == ZR_NULL) {
        return;
    }

    reflection_set_field_object(state, callableReflection, "parameters", parametersArray, ZR_VALUE_TYPE_ARRAY);
}

static void reflection_merge_object_fields(SZrState *state, SZrObject *target, SZrObject *source) {
    if (state == ZR_NULL || target == ZR_NULL || source == ZR_NULL || !source->nodeMap.isValid ||
        source->nodeMap.buckets == ZR_NULL) {
        return;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < source->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = source->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            ZrCore_Object_SetValue(state, target, &pair->key, &pair->value);
            pair = pair->next;
        }
    }
}

static void reflection_array_push_object(SZrState *state, SZrObject *array, SZrObject *object, EZrValueType valueType) {
    SZrTypeValue value;

    if (state == ZR_NULL || array == ZR_NULL || object == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(object), valueType);
    reflection_array_push(state, array, &value);
}

static void reflection_array_push_string_value(SZrState *state, SZrObject *array, const TZrChar *text) {
    SZrString *stringValue;
    SZrTypeValue value;

    if (state == ZR_NULL || array == ZR_NULL || text == ZR_NULL) {
        return;
    }

    stringValue = reflection_make_string(state, text);
    if (stringValue == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue), ZR_VALUE_TYPE_STRING);
    reflection_array_push(state, array, &value);
}

static void reflection_populate_parameters_from_typed_refs(SZrState *state,
                                                           SZrObject *callableReflection,
                                                           const SZrFunctionTypedTypeRef *parameterTypes,
                                                           TZrUInt32 parameterCount) {
    SZrObject *parametersArray;
    TZrUInt64 ownerHash;

    if (state == ZR_NULL || callableReflection == ZR_NULL) {
        return;
    }

    parametersArray = reflection_new_array(state);
    if (parametersArray == ZR_NULL) {
        return;
    }

    reflection_attach_parameters_array(state, callableReflection, parametersArray);
    ownerHash = reflection_get_field_string_native(state, callableReflection, "name", "")[0] != '\0'
                        ? XXH3_64bits(reflection_get_field_string_native(state, callableReflection, "qualifiedName", ""),
                                      strlen(reflection_get_field_string_native(state, callableReflection, "qualifiedName", "")))
                        : 0;

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        TZrChar paramName[ZR_RUNTIME_MEMBER_NAME_BUFFER_LENGTH];
        TZrChar typeNameBuffer[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
        SZrObject *parameterReflection;
        SZrTypeValue parameterValue;

        snprintf(paramName, sizeof(paramName), "arg%u", (unsigned int)index);
        parameterReflection = reflection_build_parameter_info(state,
                                                              paramName,
                                                              reflection_type_name_from_typed_type_ref(state,
                                                                                                       parameterTypes != ZR_NULL
                                                                                                               ? &parameterTypes[index]
                                                                                                               : ZR_NULL,
                                                                                                       typeNameBuffer,
                                                                                                       sizeof(typeNameBuffer)),
                                                              index,
                                                              callableReflection,
                                                              reflection_get_field_object(state, callableReflection, "module", ZR_VALUE_TYPE_OBJECT),
                                                              ownerHash ^ ((TZrUInt64)index + ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE));
        if (parameterReflection == ZR_NULL) {
            continue;
        }

        reflection_init_object_value(state,
                                     &parameterValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(parameterReflection),
                                     ZR_VALUE_TYPE_OBJECT);
        reflection_array_push(state, parametersArray, &parameterValue);
    }
}

static void reflection_populate_parameters_from_metadata(SZrState *state,
                                                         SZrObject *callableReflection,
                                                         const SZrFunctionMetadataParameter *parameters,
                                                         TZrUInt32 parameterCount) {
    SZrObject *parametersArray;
    SZrObject *moduleReflection;
    TZrUInt64 ownerHash;

    if (state == ZR_NULL || callableReflection == ZR_NULL) {
        return;
    }

    parametersArray = reflection_new_array(state);
    if (parametersArray == ZR_NULL) {
        return;
    }

    reflection_attach_parameters_array(state, callableReflection, parametersArray);
    moduleReflection = reflection_get_field_object(state, callableReflection, "module", ZR_VALUE_TYPE_OBJECT);
    ownerHash = XXH3_64bits(reflection_get_field_string_native(state, callableReflection, "qualifiedName", ""),
                            strlen(reflection_get_field_string_native(state, callableReflection, "qualifiedName", "")));

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        TZrChar fallbackName[ZR_RUNTIME_MEMBER_NAME_BUFFER_LENGTH];
        TZrChar typeNameBuffer[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
        const TZrChar *parameterName = fallbackName;
        SZrObject *parameterReflection;

        snprintf(fallbackName, sizeof(fallbackName), "arg%u", (unsigned int)index);
        if (parameters != ZR_NULL && parameters[index].name != ZR_NULL) {
            const TZrChar *nativeName = ZrCore_String_GetNativeString(parameters[index].name);
            if (nativeName != ZR_NULL && nativeName[0] != '\0') {
                parameterName = nativeName;
            }
        }

        parameterReflection = reflection_build_parameter_info(state,
                                                              parameterName,
                                                              reflection_type_name_from_typed_type_ref(state,
                                                                                                       parameters != ZR_NULL
                                                                                                               ? &parameters[index].type
                                                                                                               : ZR_NULL,
                                                                                                       typeNameBuffer,
                                                                                                       sizeof(typeNameBuffer)),
                                                              index,
                                                              callableReflection,
                                                              moduleReflection,
                                                              ownerHash ^ ((TZrUInt64)index + ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE));
        if (parameterReflection != ZR_NULL) {
            if (parameters != ZR_NULL) {
                if (parameters[index].hasDecoratorMetadata &&
                    parameters[index].decoratorMetadataValue.type == ZR_VALUE_TYPE_OBJECT &&
                    parameters[index].decoratorMetadataValue.value.object != ZR_NULL) {
                    SZrObject *metadataObject =
                            reflection_get_field_object(state, parameterReflection, "metadata", ZR_VALUE_TYPE_OBJECT);
                    if (metadataObject == ZR_NULL) {
                        reflection_set_field_object(state,
                                                    parameterReflection,
                                                    "metadata",
                                                    ZR_CAST_OBJECT(state, parameters[index].decoratorMetadataValue.value.object),
                                                    ZR_VALUE_TYPE_OBJECT);
                    } else {
                        reflection_merge_object_fields(state,
                                                       metadataObject,
                                                       ZR_CAST_OBJECT(state, parameters[index].decoratorMetadataValue.value.object));
                    }
                }

                reflection_set_field_bool(state,
                                          parameterReflection,
                                          "hasDefaultValue",
                                          parameters[index].hasDefaultValue ? ZR_TRUE : ZR_FALSE);
                if (parameters[index].hasDefaultValue) {
                    reflection_set_field_value(state,
                                               parameterReflection,
                                               "defaultValue",
                                               &parameters[index].defaultValue);
                }

                if (parameters[index].decoratorNames != ZR_NULL && parameters[index].decoratorCount > 0) {
                    SZrObject *decoratorsArray =
                            reflection_get_field_object(state, parameterReflection, "decorators", ZR_VALUE_TYPE_ARRAY);
                    for (TZrUInt32 decoratorIndex = 0;
                         decoratorsArray != ZR_NULL && decoratorIndex < parameters[index].decoratorCount;
                         decoratorIndex++) {
                        SZrObject *decoratorEntry;
                        SZrTypeValue decoratorNameValue;

                        if (parameters[index].decoratorNames[decoratorIndex] == ZR_NULL) {
                            continue;
                        }

                        decoratorEntry = reflection_new_object(state);
                        if (decoratorEntry == ZR_NULL) {
                            continue;
                        }

                        reflection_init_object_value(state,
                                                     &decoratorNameValue,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(parameters[index].decoratorNames[decoratorIndex]),
                                                     ZR_VALUE_TYPE_STRING);
                        reflection_set_field_value(state, decoratorEntry, "name", &decoratorNameValue);
                        reflection_array_push_object(state, decoratorsArray, decoratorEntry, ZR_VALUE_TYPE_OBJECT);
                    }
                }
            }
            reflection_array_push_object(state, parametersArray, parameterReflection, ZR_VALUE_TYPE_OBJECT);
        }
    }
}

static void reflection_populate_parameters_from_function(SZrState *state,
                                                         SZrObject *callableReflection,
                                                         SZrFunction *function,
                                                         TZrUInt32 visibleParameterCount) {
    SZrObject *parametersArray;
    SZrObject *moduleReflection;
    TZrUInt64 ownerHash;
    TZrUInt32 hiddenParameterCount = 0;

    if (state == ZR_NULL || callableReflection == ZR_NULL || function == ZR_NULL) {
        return;
    }

    if (function->parameterMetadata != ZR_NULL && function->parameterMetadataCount > 0) {
        reflection_populate_parameters_from_metadata(state,
                                                     callableReflection,
                                                     function->parameterMetadata,
                                                     function->parameterMetadataCount);
        return;
    }

    parametersArray = reflection_new_array(state);
    if (parametersArray == ZR_NULL) {
        return;
    }

    reflection_attach_parameters_array(state, callableReflection, parametersArray);
    moduleReflection = reflection_get_field_object(state, callableReflection, "module", ZR_VALUE_TYPE_OBJECT);
    ownerHash = XXH3_64bits(reflection_get_field_string_native(state, callableReflection, "qualifiedName", ""),
                            strlen(reflection_get_field_string_native(state, callableReflection, "qualifiedName", "")));

    if (function->parameterCount > visibleParameterCount) {
        hiddenParameterCount = function->parameterCount - visibleParameterCount;
    }

    for (TZrUInt32 index = 0; index < visibleParameterCount; index++) {
        TZrUInt32 localIndex = hiddenParameterCount + index;
        const TZrChar *parameterName = "arg";
        const TZrChar *typeName = "any";
        TZrChar fallbackName[ZR_RUNTIME_MEMBER_NAME_BUFFER_LENGTH];
        TZrChar typeNameBuffer[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
        SZrObject *parameterReflection;
        SZrTypeValue parameterValue;

        snprintf(fallbackName, sizeof(fallbackName), "arg%u", (unsigned int)index);
        parameterName = fallbackName;

        if (localIndex < function->localVariableLength) {
            const SZrFunctionLocalVariable *localVariable = &function->localVariableList[localIndex];
            const SZrFunctionTypedLocalBinding *binding;

            if (localVariable->name != ZR_NULL) {
                const TZrChar *localName = ZrCore_String_GetNativeString(localVariable->name);
                if (localName != ZR_NULL && localName[0] != '\0') {
                    parameterName = localName;
                }
            }

            binding = reflection_find_typed_local_binding(function, localVariable->stackSlot);
            if (binding != ZR_NULL) {
                typeName = reflection_type_name_from_typed_type_ref(state,
                                                                    &binding->type,
                                                                    typeNameBuffer,
                                                                    sizeof(typeNameBuffer));
            }
        }

        parameterReflection = reflection_build_parameter_info(state,
                                                              parameterName,
                                                              typeName,
                                                              index,
                                                              callableReflection,
                                                              moduleReflection,
                                                              ownerHash ^ ((TZrUInt64)index + ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE));
        if (parameterReflection == ZR_NULL) {
            continue;
        }

        reflection_init_object_value(state,
                                     &parameterValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(parameterReflection),
                                     ZR_VALUE_TYPE_OBJECT);
        reflection_array_push(state, parametersArray, &parameterValue);
    }
}

static SZrFunction *reflection_extract_function_from_value(SZrState *state, const SZrTypeValue *value) {
    return ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
}

static SZrFunction *reflection_find_entry_function_from_stack(SZrState *state) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    callInfo = state->callInfoList;
    while (callInfo != ZR_NULL) {
        if (ZR_CALL_INFO_IS_VM(callInfo) &&
            callInfo->functionBase.valuePointer >= state->stackBase.valuePointer &&
            callInfo->functionBase.valuePointer < state->stackTop.valuePointer) {
            SZrTypeValue *baseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
            SZrFunction *function = reflection_extract_function_from_value(state, baseValue);
            if (function != ZR_NULL && function->prototypeData != ZR_NULL && function->prototypeCount > 0) {
                return function;
            }
        }
        callInfo = callInfo->previous;
    }

    return ZR_NULL;
}

static SZrObjectModule *reflection_find_owner_module_from_registry(SZrState *state, SZrObjectPrototype *prototype) {
    SZrObject *registry;

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return ZR_NULL;
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL || !registry->nodeMap.isValid || registry->nodeMap.buckets == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < registry->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_OBJECT && pair->value.value.object != ZR_NULL) {
                SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                if (cachedObject != ZR_NULL && cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE && prototype->name != ZR_NULL) {
                    SZrObjectModule *module = (SZrObjectModule *)cachedObject;
                    const SZrTypeValue *exported = ZrCore_Module_GetPubExport(state, module, prototype->name);
                    if (exported != ZR_NULL &&
                        exported->type == ZR_VALUE_TYPE_OBJECT &&
                        exported->value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)) {
                        return module;
                    }
                }
            }
            pair = pair->next;
        }
    }

    return ZR_NULL;
}

static void reflection_get_prototype_metadata_context(SZrState *state,
                                                      SZrObjectPrototype *prototype,
                                                      SZrObjectModule **outModule,
                                                      SZrFunction **outEntryFunction) {
    const SZrTypeValue *moduleValue;
    const SZrTypeValue *entryValue;
    SZrObjectModule *module = ZR_NULL;
    SZrFunction *entryFunction = ZR_NULL;

    if (outModule != ZR_NULL) {
        *outModule = ZR_NULL;
    }
    if (outEntryFunction != ZR_NULL) {
        *outEntryFunction = ZR_NULL;
    }

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    moduleValue = reflection_get_field_value(state, &prototype->super, kReflectionOwnerModuleFieldName);
    if (moduleValue != ZR_NULL && moduleValue->type == ZR_VALUE_TYPE_OBJECT && moduleValue->value.object != ZR_NULL) {
        SZrObject *moduleObject = ZR_CAST_OBJECT(state, moduleValue->value.object);
        if (moduleObject != ZR_NULL && moduleObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
            module = (SZrObjectModule *)moduleObject;
        }
    }

    entryValue = reflection_get_field_value(state, &prototype->super, kReflectionEntryFunctionFieldName);
    entryFunction = reflection_extract_function_from_value(state, entryValue);

    if (module == ZR_NULL) {
        module = reflection_find_owner_module_from_registry(state, prototype);
    }
    if (entryFunction == ZR_NULL) {
        entryFunction = reflection_find_entry_function_from_stack(state);
    }

    if (outModule != ZR_NULL) {
        *outModule = module;
    }
    if (outEntryFunction != ZR_NULL) {
        *outEntryFunction = entryFunction;
    }
}

static TZrBool reflection_find_callable_export_context(SZrState *state,
                                                       const SZrTypeValue *targetValue,
                                                       SZrObjectModule **outModule,
                                                       const TZrChar **outExportName) {
    SZrObject *registry;
    SZrFunction *targetFunction;
    const TZrChar *targetFunctionName = ZR_NULL;

    if (outModule != ZR_NULL) {
        *outModule = ZR_NULL;
    }
    if (outExportName != ZR_NULL) {
        *outExportName = ZR_NULL;
    }

    if (state == ZR_NULL || targetValue == ZR_NULL || targetValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    targetFunction = reflection_extract_function_from_value(state, targetValue);
    if (targetFunction != ZR_NULL && targetFunction->functionName != ZR_NULL) {
        targetFunctionName = ZrCore_String_GetNativeString(targetFunction->functionName);
    }

    registry = zr_module_get_loaded_modules_registry(state);
    if (registry == ZR_NULL || !registry->nodeMap.isValid || registry->nodeMap.buckets == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize registryBucketIndex = 0; registryBucketIndex < registry->nodeMap.capacity; registryBucketIndex++) {
        SZrHashKeyValuePair *registryPair = registry->nodeMap.buckets[registryBucketIndex];
        while (registryPair != ZR_NULL) {
            if (registryPair->value.type == ZR_VALUE_TYPE_OBJECT && registryPair->value.value.object != ZR_NULL) {
                SZrObject *moduleObject = ZR_CAST_OBJECT(state, registryPair->value.value.object);
                if (moduleObject != ZR_NULL && moduleObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                    SZrObjectModule *module = (SZrObjectModule *)moduleObject;
                    if (module->super.nodeMap.isValid && module->super.nodeMap.buckets != ZR_NULL) {
                        for (TZrSize bucketIndex = 0; bucketIndex < module->super.nodeMap.capacity; bucketIndex++) {
                            SZrHashKeyValuePair *pair = module->super.nodeMap.buckets[bucketIndex];
                            while (pair != ZR_NULL) {
                                if (pair->value.type == targetValue->type &&
                                    pair->value.value.object == targetValue->value.object &&
                                    pair->key.type == ZR_VALUE_TYPE_STRING &&
                                    pair->key.value.object != ZR_NULL) {
                                    const TZrChar *exportName =
                                            ZrCore_String_GetNativeString(ZR_CAST_STRING(state, pair->key.value.object));
                                    if (outModule != ZR_NULL) {
                                        if (*outModule == ZR_NULL) {
                                            *outModule = module;
                                        }
                                    }
                                    if (outExportName != ZR_NULL) {
                                        if (*outExportName == ZR_NULL) {
                                            *outExportName = exportName;
                                        }
                                        if (targetFunctionName != ZR_NULL &&
                                            exportName != ZR_NULL &&
                                            strcmp(exportName, targetFunctionName) != 0) {
                                            *outExportName = exportName;
                                        }
                                    }
                                    if (targetFunctionName != ZR_NULL &&
                                        exportName != ZR_NULL &&
                                        strcmp(exportName, targetFunctionName) != 0) {
                                        if (outModule != ZR_NULL) {
                                            *outModule = module;
                                        }
                                        return ZR_TRUE;
                                    }
                                }
                                pair = pair->next;
                            }
                        }
                    }
                }
            }
            registryPair = registryPair->next;
        }
    }

    return (outModule != ZR_NULL && *outModule != ZR_NULL) ||
           (outExportName != ZR_NULL && *outExportName != ZR_NULL);
}

static TZrBool reflection_should_hide_duplicate_callable_export(SZrState *state,
                                                                SZrObjectModule *module,
                                                                const TZrChar *symbolName,
                                                                const SZrTypeValue *exportedValue,
                                                                SZrFunction *exportedFunction) {
    const TZrChar *functionName;

    if (state == ZR_NULL || module == ZR_NULL || symbolName == ZR_NULL || exportedValue == ZR_NULL ||
        exportedValue->value.object == ZR_NULL || exportedFunction == ZR_NULL || exportedFunction->functionName == ZR_NULL) {
        return ZR_FALSE;
    }

    functionName = ZrCore_String_GetNativeString(exportedFunction->functionName);
    if (functionName == ZR_NULL || strcmp(symbolName, functionName) != 0 ||
        !module->super.nodeMap.isValid || module->super.nodeMap.buckets == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < module->super.nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = module->super.nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->key.type == ZR_VALUE_TYPE_STRING &&
                pair->key.value.object != ZR_NULL &&
                pair->value.type == exportedValue->type &&
                pair->value.value.object == exportedValue->value.object) {
                const TZrChar *aliasName = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, pair->key.value.object));
                if (aliasName != ZR_NULL && strcmp(aliasName, symbolName) != 0) {
                    return ZR_TRUE;
                }
            }
            pair = pair->next;
        }
    }

    return ZR_FALSE;
}

static void reflection_populate_type_layout(SZrState *state,
                                            SZrObject *typeReflection,
                                            SZrObjectPrototype *prototype,
                                            SZrObject *nativeTypeEntry) {
    SZrObject *layoutObject;
    TZrUInt32 fieldCount = 0;
    TZrUInt32 size = 0;
    TZrUInt32 alignment = 0;

    if (state == ZR_NULL || typeReflection == ZR_NULL) {
        return;
    }

    layoutObject = reflection_get_field_object(state, typeReflection, "layout", ZR_VALUE_TYPE_OBJECT);
    if (layoutObject == ZR_NULL) {
        return;
    }

    if (prototype != ZR_NULL && prototype->managedFields != ZR_NULL && prototype->managedFieldCount > 0) {
        fieldCount = prototype->managedFieldCount;
        for (TZrUInt32 index = 0; index < prototype->managedFieldCount; index++) {
            const SZrManagedFieldInfo *fieldInfo = &prototype->managedFields[index];
            TZrUInt32 fieldEnd = fieldInfo->fieldOffset + fieldInfo->fieldSize;
            if (fieldEnd > size) {
                size = fieldEnd;
            }
            if (fieldInfo->fieldSize > alignment) {
                alignment = fieldInfo->fieldSize;
            }
        }
    } else if (nativeTypeEntry != ZR_NULL) {
        SZrObject *fieldsArray = reflection_get_field_object(state, nativeTypeEntry, "fields", ZR_VALUE_TYPE_ARRAY);
        if (fieldsArray != ZR_NULL) {
            fieldCount = reflection_array_length(fieldsArray);
            size = fieldCount;
            alignment = fieldCount > 0 ? 1u : 0u;
        }
    } else {
        const SZrTypeValue *fieldCountValue = reflection_get_field_value(state, layoutObject, "fieldCount");
        const SZrTypeValue *sizeValue = reflection_get_field_value(state, layoutObject, "size");
        const SZrTypeValue *alignmentValue = reflection_get_field_value(state, layoutObject, "alignment");

        if (fieldCountValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(fieldCountValue->type)) {
            fieldCount = (TZrUInt32)fieldCountValue->value.nativeObject.nativeInt64;
        }
        if (sizeValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(sizeValue->type)) {
            size = (TZrUInt32)sizeValue->value.nativeObject.nativeInt64;
        }
        if (alignmentValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(alignmentValue->type)) {
            alignment = (TZrUInt32)alignmentValue->value.nativeObject.nativeInt64;
        }
    }

    if (alignment == 0 && size > 0) {
        alignment = 1;
    }

    reflection_set_field_int(state, layoutObject, "fieldCount", fieldCount);
    reflection_set_field_int(state, layoutObject, "size", size);
    reflection_set_field_int(state, layoutObject, "alignment", alignment);
}

static void reflection_populate_function_metadata(SZrState *state, SZrObject *reflectionObject, SZrFunction *function) {
    SZrObject *sourceObject;
    SZrObject *irObject;
    SZrObject *compileTimeObject;
    SZrObject *codeBlocksArray;
    SZrObject *codeBlockObject;
    SZrObject *registerIdsArray;
    SZrObject *constantSlotsArray;
    SZrObject *decoratorsArray;

    if (state == ZR_NULL || reflectionObject == ZR_NULL || function == ZR_NULL) {
        return;
    }

    sourceObject = reflection_get_field_object(state, reflectionObject, "source", ZR_VALUE_TYPE_OBJECT);
    irObject = reflection_get_field_object(state, reflectionObject, "ir", ZR_VALUE_TYPE_OBJECT);
    compileTimeObject = reflection_get_field_object(state, reflectionObject, "compileTime", ZR_VALUE_TYPE_OBJECT);
    codeBlocksArray = reflection_get_field_object(state, reflectionObject, "codeBlocks", ZR_VALUE_TYPE_ARRAY);
    decoratorsArray = reflection_get_field_object(state, reflectionObject, "decorators", ZR_VALUE_TYPE_ARRAY);
    if (sourceObject == ZR_NULL || irObject == ZR_NULL || compileTimeObject == ZR_NULL || codeBlocksArray == ZR_NULL ||
        decoratorsArray == ZR_NULL) {
        return;
    }

    reflection_set_field_int(state, sourceObject, "startLine", function->lineInSourceStart);
    reflection_set_field_int(state, sourceObject, "endLine", function->lineInSourceEnd);
    reflection_set_field_string(state,
                                sourceObject,
                                "functionName",
                                function->functionName != ZR_NULL ? ZrCore_String_GetNativeString(function->functionName) : "");

    if (function->hasDecoratorMetadata &&
        function->decoratorMetadataValue.type == ZR_VALUE_TYPE_OBJECT &&
        function->decoratorMetadataValue.value.object != ZR_NULL) {
        reflection_set_field_object(state,
                                    reflectionObject,
                                    "metadata",
                                    ZR_CAST_OBJECT(state, function->decoratorMetadataValue.value.object),
                                    ZR_VALUE_TYPE_OBJECT);
    }

    if (function->decoratorNames != ZR_NULL && function->decoratorCount > 0) {
        for (TZrUInt32 index = 0; index < function->decoratorCount; index++) {
            SZrString *decoratorName = function->decoratorNames[index];
            SZrObject *decoratorEntry;
            SZrTypeValue decoratorNameValue;

            if (decoratorName == ZR_NULL) {
                continue;
            }

            decoratorEntry = reflection_new_object(state);
            if (decoratorEntry == ZR_NULL) {
                continue;
            }

            reflection_init_object_value(state,
                                         &decoratorNameValue,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorName),
                                         ZR_VALUE_TYPE_STRING);
            reflection_set_field_value(state, decoratorEntry, "name", &decoratorNameValue);
            reflection_array_push_object(state, decoratorsArray, decoratorEntry, ZR_VALUE_TYPE_OBJECT);
        }
    }

    reflection_set_field_int(state, irObject, "instructionCount", function->instructionsLength);
    reflection_set_field_int(state, irObject, "constantCount", function->constantValueLength);
    reflection_set_field_int(state, irObject, "localCount", function->localVariableLength);
    reflection_set_field_int(state, irObject, "closureCount", function->closureValueLength);

    reflection_set_field_bool(state,
                              compileTimeObject,
                              "hasTypedMetadata",
                              function->typedLocalBindingLength > 0 ? ZR_TRUE : ZR_FALSE);
    reflection_set_field_int(state, compileTimeObject, "typedLocalBindingCount", function->typedLocalBindingLength);

    if (reflection_array_length(codeBlocksArray) > 0) {
        return;
    }

    codeBlockObject = reflection_new_object(state);
    registerIdsArray = reflection_new_array(state);
    constantSlotsArray = reflection_new_array(state);
    if (codeBlockObject == ZR_NULL || registerIdsArray == ZR_NULL || constantSlotsArray == ZR_NULL) {
        return;
    }

    reflection_set_field_string(state,
                                codeBlockObject,
                                "name",
                                function->functionName != ZR_NULL ? ZrCore_String_GetNativeString(function->functionName) : "__anonymous");
    reflection_set_field_int(state, codeBlockObject, "registerCount", function->stackSize);
    reflection_set_field_int(state, codeBlockObject, "constantCount", function->constantValueLength);
    reflection_set_field_int(state, codeBlockObject, "instructionCount", function->instructionsLength);
    reflection_set_field_object(state, codeBlockObject, "registerIds", registerIdsArray, ZR_VALUE_TYPE_ARRAY);
    reflection_set_field_object(state, codeBlockObject, "constantSlots", constantSlotsArray, ZR_VALUE_TYPE_ARRAY);

    for (TZrUInt32 index = 0; index < function->stackSize; index++) {
        SZrTypeValue registerValue;
        ZrCore_Value_InitAsInt(state, &registerValue, index);
        reflection_array_push(state, registerIdsArray, &registerValue);
    }

    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        SZrObject *constantSlotObject = reflection_new_object(state);
        SZrTypeValue constantSlotValue;

        if (constantSlotObject == ZR_NULL) {
            continue;
        }

        reflection_set_field_int(state, constantSlotObject, "index", index);
        reflection_set_field_string(state,
                                    constantSlotObject,
                                    "typeName",
                                    reflection_builtin_type_name(function->constantValueList[index].type));
        reflection_init_object_value(state,
                                     &constantSlotValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(constantSlotObject),
                                     ZR_VALUE_TYPE_OBJECT);
        reflection_array_push(state, constantSlotsArray, &constantSlotValue);
    }

    {
        SZrTypeValue codeBlockValue;
        reflection_init_object_value(state,
                                     &codeBlockValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(codeBlockObject),
                                     ZR_VALUE_TYPE_OBJECT);
        reflection_array_push(state, codeBlocksArray, &codeBlockValue);
    }

    ZrCore_RuntimeDecorator_OverlayFunctionReflection(state, reflectionObject, function);
}

static void reflection_append_decorator_name_value(SZrState *state,
                                                   SZrObject *decoratorsArray,
                                                   const SZrTypeValue *decoratorNameValue) {
    SZrObject *decoratorEntry;

    if (state == ZR_NULL || decoratorsArray == ZR_NULL || decoratorNameValue == ZR_NULL ||
        decoratorNameValue->type != ZR_VALUE_TYPE_STRING || decoratorNameValue->value.object == ZR_NULL) {
        return;
    }

    decoratorEntry = reflection_new_object(state);
    if (decoratorEntry == ZR_NULL) {
        return;
    }

    reflection_set_field_value(state, decoratorEntry, "name", decoratorNameValue);
    reflection_array_push_object(state, decoratorsArray, decoratorEntry, ZR_VALUE_TYPE_OBJECT);
}

static void reflection_populate_compiled_member_decorator_metadata(SZrState *state,
                                                                   SZrObject *memberReflection,
                                                                   SZrFunction *entryFunction,
                                                                   const SZrCompiledMemberInfo *member) {
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;

    if (state == ZR_NULL || memberReflection == ZR_NULL || entryFunction == ZR_NULL || member == ZR_NULL ||
        entryFunction->constantValueList == ZR_NULL) {
        return;
    }

    metadataObject = reflection_get_field_object(state, memberReflection, "metadata", ZR_VALUE_TYPE_OBJECT);
    decoratorsArray = reflection_get_field_object(state, memberReflection, "decorators", ZR_VALUE_TYPE_ARRAY);

    if (member->hasDecoratorMetadata &&
        member->decoratorMetadataConstantIndex < entryFunction->constantValueLength) {
        const SZrTypeValue *metadataValue = &entryFunction->constantValueList[member->decoratorMetadataConstantIndex];
        if (metadataValue->type == ZR_VALUE_TYPE_OBJECT && metadataValue->value.object != ZR_NULL) {
            if (metadataObject == ZR_NULL) {
                reflection_set_field_object(state,
                                            memberReflection,
                                            "metadata",
                                            ZR_CAST_OBJECT(state, metadataValue->value.object),
                                            ZR_VALUE_TYPE_OBJECT);
            } else {
                reflection_merge_object_fields(state,
                                               metadataObject,
                                               ZR_CAST_OBJECT(state, metadataValue->value.object));
            }
        }
    }

    if (decoratorsArray != ZR_NULL && member->hasDecoratorNames &&
        member->decoratorNamesConstantIndex < entryFunction->constantValueLength) {
        const SZrTypeValue *decoratorNamesValue = &entryFunction->constantValueList[member->decoratorNamesConstantIndex];
        if (decoratorNamesValue->type == ZR_VALUE_TYPE_ARRAY && decoratorNamesValue->value.object != ZR_NULL) {
            SZrObject *nameArray = ZR_CAST_OBJECT(state, decoratorNamesValue->value.object);
            if (nameArray != ZR_NULL && nameArray->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                TZrUInt32 decoratorCount = reflection_array_length(nameArray);
                for (TZrUInt32 decoratorIndex = 0; decoratorIndex < decoratorCount; decoratorIndex++) {
                    reflection_append_decorator_name_value(state,
                                                           decoratorsArray,
                                                           reflection_array_get(state, nameArray, decoratorIndex));
                }
            }
        }
    }
}

static void reflection_populate_module_compile_time_metadata(SZrState *state,
                                                             SZrObject *moduleReflection,
                                                             SZrFunction *entryFunction) {
    SZrObject *compileTimeObject;
    SZrObject *variablesArray;
    SZrObject *functionsArray;
    const TZrChar *moduleQualifiedName;

    if (state == ZR_NULL || moduleReflection == ZR_NULL || entryFunction == ZR_NULL) {
        return;
    }

    compileTimeObject = reflection_get_field_object(state, moduleReflection, "compileTime", ZR_VALUE_TYPE_OBJECT);
    if (compileTimeObject == ZR_NULL) {
        return;
    }

    variablesArray = reflection_get_field_object(state, compileTimeObject, "variables", ZR_VALUE_TYPE_ARRAY);
    functionsArray = reflection_get_field_object(state, compileTimeObject, "functions", ZR_VALUE_TYPE_ARRAY);
    if (variablesArray == ZR_NULL || functionsArray == ZR_NULL) {
        return;
    }

    if (reflection_array_length(variablesArray) > 0 || reflection_array_length(functionsArray) > 0) {
        return;
    }

    moduleQualifiedName = reflection_get_field_string_native(state, moduleReflection, "qualifiedName", "");
    reflection_set_field_bool(state,
                              compileTimeObject,
                              "hasTypedMetadata",
                              (entryFunction->compileTimeVariableInfoLength + entryFunction->compileTimeFunctionInfoLength) > 0
                                      ? ZR_TRUE
                                      : ZR_FALSE);
    reflection_set_field_int(state,
                             compileTimeObject,
                             "variableCount",
                             entryFunction->compileTimeVariableInfoLength);
    reflection_set_field_int(state,
                             compileTimeObject,
                             "functionCount",
                             entryFunction->compileTimeFunctionInfoLength);

    for (TZrUInt32 index = 0; index < entryFunction->compileTimeVariableInfoLength; index++) {
        SZrFunctionCompileTimeVariableInfo *info = &entryFunction->compileTimeVariableInfos[index];
        TZrChar qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
        TZrChar typeNameBuffer[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
        const TZrChar *name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "compileTimeVar";
        SZrObject *infoObject;

        snprintf(qualifiedName,
                 sizeof(qualifiedName),
                 "%s.%%compileTime.%s",
                 moduleQualifiedName != ZR_NULL ? moduleQualifiedName : "",
                 name != ZR_NULL ? name : "compileTimeVar");
        infoObject = reflection_build_member_info(state,
                                                  name != ZR_NULL ? name : "compileTimeVar",
                                                  qualifiedName,
                                                  "compileTimeVariable",
                                                  XXH3_64bits(qualifiedName, strlen(qualifiedName)));
        if (infoObject == ZR_NULL) {
            continue;
        }

        reflection_assign_owner_links(state, infoObject, moduleReflection, moduleReflection);
        reflection_set_field_string(state,
                                    infoObject,
                                    "typeName",
                                    reflection_type_name_from_typed_type_ref(state,
                                                                             &info->type,
                                                                             typeNameBuffer,
                                                                             sizeof(typeNameBuffer)));
        reflection_set_field_int(state,
                                 reflection_get_field_object(state, infoObject, "source", ZR_VALUE_TYPE_OBJECT),
                                 "startLine",
                                 info->lineInSourceStart);
        reflection_set_field_int(state,
                                 reflection_get_field_object(state, infoObject, "source", ZR_VALUE_TYPE_OBJECT),
                                 "endLine",
                                 info->lineInSourceEnd);
        reflection_array_push_object(state, variablesArray, infoObject, ZR_VALUE_TYPE_OBJECT);
    }

    for (TZrUInt32 index = 0; index < entryFunction->compileTimeFunctionInfoLength; index++) {
        SZrFunctionCompileTimeFunctionInfo *info = &entryFunction->compileTimeFunctionInfos[index];
        TZrChar qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
        TZrChar returnTypeBuffer[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
        const TZrChar *name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "compileTimeFn";
        SZrObject *infoObject;

        snprintf(qualifiedName,
                 sizeof(qualifiedName),
                 "%s.%%compileTime.%s",
                 moduleQualifiedName != ZR_NULL ? moduleQualifiedName : "",
                 name != ZR_NULL ? name : "compileTimeFn");
        infoObject = reflection_build_callable_reflection(state,
                                                          name != ZR_NULL ? name : "compileTimeFn",
                                                          qualifiedName,
                                                          reflection_type_name_from_typed_type_ref(state,
                                                                                                   &info->returnType,
                                                                                                   returnTypeBuffer,
                                                                                                   sizeof(returnTypeBuffer)),
                                                          info->parameterCount,
                                                          ZR_FALSE,
                                                          moduleReflection,
                                                          moduleReflection,
                                                          XXH3_64bits(qualifiedName, strlen(qualifiedName)));
        if (infoObject == ZR_NULL) {
            continue;
        }

        reflection_set_field_string(state, infoObject, "kind", "compileTimeFunction");
        reflection_populate_parameters_from_metadata(state, infoObject, info->parameters, info->parameterCount);
        reflection_set_field_int(state,
                                 reflection_get_field_object(state, infoObject, "source", ZR_VALUE_TYPE_OBJECT),
                                 "startLine",
                                 info->lineInSourceStart);
        reflection_set_field_int(state,
                                 reflection_get_field_object(state, infoObject, "source", ZR_VALUE_TYPE_OBJECT),
                                 "endLine",
                                 info->lineInSourceEnd);
        reflection_array_push_object(state, functionsArray, infoObject, ZR_VALUE_TYPE_OBJECT);
    }
}

static void reflection_populate_module_tests_metadata(SZrState *state,
                                                      SZrObject *moduleReflection,
                                                      SZrFunction *entryFunction) {
    SZrObject *testsArray;
    const TZrChar *moduleQualifiedName;

    if (state == ZR_NULL || moduleReflection == ZR_NULL || entryFunction == ZR_NULL) {
        return;
    }

    testsArray = reflection_get_field_object(state, moduleReflection, "tests", ZR_VALUE_TYPE_ARRAY);
    if (testsArray == ZR_NULL || reflection_array_length(testsArray) > 0) {
        return;
    }

    moduleQualifiedName = reflection_get_field_string_native(state, moduleReflection, "qualifiedName", "");
    for (TZrUInt32 index = 0; index < entryFunction->testInfoLength; index++) {
        SZrFunctionTestInfo *info = &entryFunction->testInfos[index];
        TZrChar qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
        const TZrChar *name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "test";
        SZrObject *testObject;

        snprintf(qualifiedName,
                 sizeof(qualifiedName),
                 "%s.%%test.%s",
                 moduleQualifiedName != ZR_NULL ? moduleQualifiedName : "",
                 name != ZR_NULL ? name : "test");
        testObject = reflection_build_callable_reflection(state,
                                                          name != ZR_NULL ? name : "test",
                                                          qualifiedName,
                                                          "void",
                                                          info->parameterCount,
                                                          ZR_FALSE,
                                                          moduleReflection,
                                                          moduleReflection,
                                                          XXH3_64bits(qualifiedName, strlen(qualifiedName)));
        if (testObject == ZR_NULL) {
            continue;
        }

        reflection_set_field_string(state, testObject, "kind", "test");
        reflection_set_field_bool(state, testObject, "hasVariableArguments", info->hasVariableArguments);
        reflection_populate_parameters_from_metadata(state, testObject, info->parameters, info->parameterCount);
        reflection_set_field_int(state,
                                 reflection_get_field_object(state, testObject, "source", ZR_VALUE_TYPE_OBJECT),
                                 "startLine",
                                 info->lineInSourceStart);
        reflection_set_field_int(state,
                                 reflection_get_field_object(state, testObject, "source", ZR_VALUE_TYPE_OBJECT),
                                 "endLine",
                                 info->lineInSourceEnd);
        reflection_array_push_object(state, testsArray, testObject, ZR_VALUE_TYPE_OBJECT);
    }
}

static TZrBool reflection_get_compiled_prototype_info_by_index(SZrFunction *entryFunction,
                                                               TZrUInt32 targetIndex,
                                                               const SZrCompiledPrototypeInfo **outInfo) {
    const TZrByte *currentPos;
    TZrSize remainingDataSize;

    if (outInfo != ZR_NULL) {
        *outInfo = ZR_NULL;
    }

    if (entryFunction == ZR_NULL ||
        entryFunction->prototypeData == ZR_NULL ||
        entryFunction->prototypeDataLength <= sizeof(TZrUInt32) ||
        targetIndex >= entryFunction->prototypeCount) {
        return ZR_FALSE;
    }

    currentPos = entryFunction->prototypeData + sizeof(TZrUInt32);
    remainingDataSize = entryFunction->prototypeDataLength - sizeof(TZrUInt32);

    for (TZrUInt32 index = 0; index < entryFunction->prototypeCount; index++) {
        const SZrCompiledPrototypeInfo *info;
        TZrSize prototypeSize;

        if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
            return ZR_FALSE;
        }

        info = (const SZrCompiledPrototypeInfo *)currentPos;
        prototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                        ((TZrSize)info->inheritsCount * sizeof(TZrUInt32)) +
                        ((TZrSize)info->decoratorsCount * sizeof(TZrUInt32)) +
                        ((TZrSize)info->membersCount * sizeof(SZrCompiledMemberInfo));
        if (remainingDataSize < prototypeSize) {
            return ZR_FALSE;
        }

        if (index == targetIndex) {
            if (outInfo != ZR_NULL) {
                *outInfo = info;
            }
            return ZR_TRUE;
        }

        currentPos += prototypeSize;
        remainingDataSize -= prototypeSize;
    }

    return ZR_FALSE;
}

static const SZrCompiledPrototypeInfo *reflection_find_compiled_prototype_info(SZrFunction *entryFunction,
                                                                               SZrObjectPrototype *prototype) {
    if (entryFunction == ZR_NULL || prototype == ZR_NULL || entryFunction->prototypeInstances == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < entryFunction->prototypeInstancesLength; index++) {
        if (entryFunction->prototypeInstances[index] == prototype) {
            const SZrCompiledPrototypeInfo *info = ZR_NULL;
            if (reflection_get_compiled_prototype_info_by_index(entryFunction, index, &info)) {
                return info;
            }
            return ZR_NULL;
        }
    }

    return ZR_NULL;
}

static const TZrChar *reflection_string_from_constant(SZrState *state,
                                                      SZrFunction *entryFunction,
                                                      TZrUInt32 constantIndex,
                                                      const TZrChar *fallback) {
    if (state == ZR_NULL ||
        entryFunction == ZR_NULL ||
        constantIndex >= entryFunction->constantValueLength ||
        entryFunction->constantValueList[constantIndex].type != ZR_VALUE_TYPE_STRING ||
        entryFunction->constantValueList[constantIndex].value.object == ZR_NULL) {
        return fallback;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, entryFunction->constantValueList[constantIndex].value.object));
}

static SZrObject *reflection_build_member_info(SZrState *state,
                                               const TZrChar *name,
                                               const TZrChar *qualifiedName,
                                               const TZrChar *kind,
                                               TZrUInt64 hash) {
    SZrObject *memberObject = reflection_new_object(state);

    if (memberObject == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_init_common_fields(state, memberObject, name, qualifiedName, kind, hash);
    return memberObject;
}

static void reflection_assign_owner_links(SZrState *state,
                                          SZrObject *memberReflection,
                                          SZrObject *ownerReflection,
                                          SZrObject *moduleReflection) {
    if (state == ZR_NULL || memberReflection == ZR_NULL) {
        return;
    }

    if (ownerReflection != ZR_NULL) {
        reflection_set_field_object(state, memberReflection, "owner", ownerReflection, ZR_VALUE_TYPE_OBJECT);
    }
    if (moduleReflection != ZR_NULL) {
        reflection_set_field_object(state, memberReflection, "module", moduleReflection, ZR_VALUE_TYPE_OBJECT);
    }
}

static TZrBool reflection_parse_hidden_property_accessor_name(const TZrChar *memberName,
                                                              const TZrChar **outPropertyName,
                                                              TZrBool *outIsSetter) {
    static const TZrChar *kGetterPrefix = "__get_";
    static const TZrChar *kSetterPrefix = "__set_";

    if (outPropertyName != ZR_NULL) {
        *outPropertyName = ZR_NULL;
    }
    if (outIsSetter != ZR_NULL) {
        *outIsSetter = ZR_FALSE;
    }

    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strncmp(memberName, kGetterPrefix, strlen(kGetterPrefix)) == 0) {
        if (outPropertyName != ZR_NULL) {
            *outPropertyName = memberName + strlen(kGetterPrefix);
        }
        return ZR_TRUE;
    }

    if (strncmp(memberName, kSetterPrefix, strlen(kSetterPrefix)) == 0) {
        if (outPropertyName != ZR_NULL) {
            *outPropertyName = memberName + strlen(kSetterPrefix);
        }
        if (outIsSetter != ZR_NULL) {
            *outIsSetter = ZR_TRUE;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrObject *reflection_find_named_entry_of_kind(SZrState *state,
                                                      SZrObject *membersObject,
                                                      const TZrChar *memberName,
                                                      const TZrChar *kind) {
    const SZrTypeValue *entriesValue;
    SZrObject *entriesArray;

    if (state == ZR_NULL || membersObject == ZR_NULL || memberName == ZR_NULL || kind == ZR_NULL) {
        return ZR_NULL;
    }

    entriesValue = reflection_get_field_value(state, membersObject, memberName);
    if (entriesValue == ZR_NULL || entriesValue->type != ZR_VALUE_TYPE_ARRAY || entriesValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    entriesArray = ZR_CAST_OBJECT(state, entriesValue->value.object);
    for (TZrUInt32 index = 0; index < reflection_array_length(entriesArray); index++) {
        const SZrTypeValue *entryValue = reflection_array_get(state, entriesArray, index);
        SZrObject *entryObject;

        if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
            continue;
        }

        entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
        if (strcmp(reflection_get_field_string_native(state, entryObject, "kind", ""), kind) == 0) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

static void reflection_record_property_member(SZrState *state,
                                              SZrObject *membersObject,
                                              SZrObject *typeReflection,
                                              SZrObject *moduleReflection,
                                              SZrObjectPrototype *prototype,
                                              const TZrChar *qualifiedTypeName,
                                              const TZrChar *propertyName,
                                              const SZrCompiledMemberInfo *member,
                                              SZrFunction *entryFunction,
                                              SZrFunction *memberFunction,
                                              TZrUInt64 hashSeed) {
    SZrObject *propertyReflection;
    TZrChar qualifiedMemberName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
    SZrString *propertyNameString;

    if (state == ZR_NULL || membersObject == ZR_NULL || typeReflection == ZR_NULL || prototype == ZR_NULL ||
        qualifiedTypeName == ZR_NULL || propertyName == ZR_NULL || propertyName[0] == '\0' || member == ZR_NULL) {
        return;
    }

    propertyReflection = reflection_find_named_entry_of_kind(state, membersObject, propertyName, "property");
    if (propertyReflection == ZR_NULL) {
        snprintf(qualifiedMemberName, sizeof(qualifiedMemberName), "%s.%s", qualifiedTypeName, propertyName);
        propertyReflection = reflection_build_member_info(state,
                                                          propertyName,
                                                          qualifiedMemberName,
                                                          "property",
                                                          hashSeed);
        if (propertyReflection == ZR_NULL) {
            return;
        }

        reflection_assign_owner_links(state, propertyReflection, typeReflection, moduleReflection);
        reflection_populate_compiled_member_oop_metadata(state, propertyReflection, entryFunction, member);
        reflection_set_field_bool(state, propertyReflection, "isStatic", member->isStatic ? ZR_TRUE : ZR_FALSE);
        reflection_set_field_bool(state, propertyReflection, "isConst", ZR_FALSE);
        reflection_add_named_entry(state, membersObject, propertyName, propertyReflection);
    }

    if (memberFunction != ZR_NULL) {
        reflection_set_field_int(state, propertyReflection, "parameterCount", member->parameterCount);
        reflection_populate_parameters_from_function(state, propertyReflection, memberFunction, member->parameterCount);
        reflection_populate_function_metadata(state, propertyReflection, memberFunction);
    } else if (member->parameterCount > 0) {
        reflection_set_field_int(state, propertyReflection, "parameterCount", member->parameterCount);
        reflection_populate_parameters_from_typed_refs(state, propertyReflection, ZR_NULL, member->parameterCount);
    }
    reflection_populate_compiled_member_decorator_metadata(state, propertyReflection, entryFunction, member);

    propertyNameString = ZrCore_String_Create(state, (TZrNativeString)propertyName, strlen(propertyName));
    if (propertyNameString != ZR_NULL) {
        ZrCore_RuntimeDecorator_OverlayMemberReflection(state,
                                                        propertyReflection,
                                                        prototype,
                                                        propertyNameString,
                                                        ZR_RUNTIME_DECORATOR_TARGET_KIND_PROPERTY);
    }
}

ZR_CORE_API SZrObject *ZrCore_Reflection_BuildDecoratorTargetMemberReflection(SZrState *state,
                                                                               SZrObjectPrototype *prototype,
                                                                               SZrString *memberName,
                                                                               TZrUInt32 targetKindValue) {
    SZrObjectModule *module = ZR_NULL;
    SZrFunction *entryFunction = ZR_NULL;
    const SZrCompiledPrototypeInfo *prototypeInfo;
    SZrObject *typeReflection;
    SZrObject *moduleReflection = ZR_NULL;
    SZrObject *membersObject = ZR_NULL;
    const TZrByte *membersBase;
    const TZrChar *qualifiedTypeName;
    const TZrChar *memberNameText;
    EZrRuntimeDecoratorTargetKind targetKind;

    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    targetKind = (EZrRuntimeDecoratorTargetKind)targetKindValue;
    if (targetKind != ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD &&
        targetKind != ZR_RUNTIME_DECORATOR_TARGET_KIND_METHOD &&
        targetKind != ZR_RUNTIME_DECORATOR_TARGET_KIND_PROPERTY) {
        return ZR_NULL;
    }

    memberNameText = ZrCore_String_GetNativeString(memberName);
    if (memberNameText == ZR_NULL || memberNameText[0] == '\0') {
        return ZR_NULL;
    }

    reflection_get_prototype_metadata_context(state, prototype, &module, &entryFunction);
    if (entryFunction == ZR_NULL) {
        return ZR_NULL;
    }

    prototypeInfo = reflection_find_compiled_prototype_info(entryFunction, prototype);
    if (prototypeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    typeReflection = reflection_build_type_reflection(state, prototype, module, entryFunction, ZR_NULL);
    if (typeReflection == ZR_NULL) {
        return ZR_NULL;
    }

    moduleReflection = reflection_get_field_object(state, typeReflection, "module", ZR_VALUE_TYPE_OBJECT);
    membersObject = reflection_get_field_object(state, typeReflection, "members", ZR_VALUE_TYPE_OBJECT);
    qualifiedTypeName = reflection_get_field_string_native(state, typeReflection, "qualifiedName", "");
    membersBase = (const TZrByte *)prototypeInfo + sizeof(SZrCompiledPrototypeInfo) +
                  ((TZrSize)prototypeInfo->inheritsCount * sizeof(TZrUInt32)) +
                  ((TZrSize)prototypeInfo->decoratorsCount * sizeof(TZrUInt32));

    if (targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_PROPERTY) {
        SZrObject *propertyReflection;

        if (membersObject == ZR_NULL) {
            return ZR_NULL;
        }

        propertyReflection = reflection_find_named_entry_of_kind(state, membersObject, memberNameText, "property");
        if (propertyReflection != ZR_NULL) {
            return propertyReflection;
        }

        for (TZrUInt32 memberIndex = 0; memberIndex < prototypeInfo->membersCount; memberIndex++) {
            const SZrCompiledMemberInfo *member = (const SZrCompiledMemberInfo *)(membersBase +
                                                                                  (sizeof(SZrCompiledMemberInfo) *
                                                                                   memberIndex));
            const SZrTypeValue *memberNameValue;
            SZrString *compiledName;
            const TZrChar *compiledNameText;
            const TZrChar *propertyName = ZR_NULL;

            if (member->nameStringIndex >= entryFunction->constantValueLength) {
                continue;
            }

            memberNameValue = &entryFunction->constantValueList[member->nameStringIndex];
            if (memberNameValue->type != ZR_VALUE_TYPE_STRING || memberNameValue->value.object == ZR_NULL) {
                continue;
            }

            compiledName = ZR_CAST_STRING(state, memberNameValue->value.object);
            compiledNameText = compiledName != ZR_NULL ? ZrCore_String_GetNativeString(compiledName) : ZR_NULL;
            if (compiledNameText == ZR_NULL ||
                !reflection_parse_hidden_property_accessor_name(compiledNameText, &propertyName, ZR_NULL) ||
                propertyName == ZR_NULL ||
                strcmp(propertyName, memberNameText) != 0) {
                continue;
            }

            reflection_record_property_member(state,
                                              membersObject,
                                              typeReflection,
                                              moduleReflection,
                                              prototype,
                                              qualifiedTypeName,
                                              memberNameText,
                                              member,
                                              entryFunction,
                                              reflection_extract_function_from_constant_index(state,
                                                                                            entryFunction,
                                                                                            member->functionConstantIndex),
                                              prototype->super.super.hash ^
                                                      ((TZrUInt64)memberIndex + ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE +
                                                       (TZrUInt64)0x1000u));
        }

        return reflection_find_named_entry_of_kind(state, membersObject, memberNameText, "property");
    }

    if (membersObject != ZR_NULL) {
        const TZrChar *expectedKind = targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD ? "field" : "method";
        SZrObject *cachedMemberReflection =
                reflection_find_named_entry_of_kind(state, membersObject, memberNameText, expectedKind);
        if (cachedMemberReflection != ZR_NULL) {
            return cachedMemberReflection;
        }
    }

    for (TZrUInt32 memberIndex = 0; memberIndex < prototypeInfo->membersCount; memberIndex++) {
        const SZrCompiledMemberInfo *member = (const SZrCompiledMemberInfo *)(membersBase +
                                                                              (sizeof(SZrCompiledMemberInfo) *
                                                                               memberIndex));
        const SZrTypeValue *memberNameValue;
        SZrString *compiledName;
        const TZrChar *compiledNameText;
        const TZrChar *kind;
        const TZrChar *fieldTypeName;
        const TZrChar *returnTypeName;
        TZrChar qualifiedMemberName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
        SZrObject *memberReflection;

        if (member->nameStringIndex >= entryFunction->constantValueLength) {
            continue;
        }

        memberNameValue = &entryFunction->constantValueList[member->nameStringIndex];
        if (memberNameValue->type != ZR_VALUE_TYPE_STRING || memberNameValue->value.object == ZR_NULL) {
            continue;
        }

        compiledName = ZR_CAST_STRING(state, memberNameValue->value.object);
        compiledNameText = compiledName != ZR_NULL ? ZrCore_String_GetNativeString(compiledName) : ZR_NULL;
        if (compiledNameText == ZR_NULL || strcmp(compiledNameText, memberNameText) != 0) {
            continue;
        }

        if (targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD &&
            member->memberType != ZR_AST_CONSTANT_STRUCT_FIELD &&
            member->memberType != ZR_AST_CONSTANT_CLASS_FIELD) {
            continue;
        }

        if (targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_METHOD &&
            member->memberType != ZR_AST_CONSTANT_STRUCT_METHOD &&
            member->memberType != ZR_AST_CONSTANT_CLASS_METHOD) {
            continue;
        }

        kind = (targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD) ? "field" : "method";
        fieldTypeName = reflection_string_from_constant(state, entryFunction, member->fieldTypeNameStringIndex, "any");
        returnTypeName = reflection_string_from_constant(state, entryFunction,
                                                         member->returnTypeNameStringIndex,
                                                         "void");
        snprintf(qualifiedMemberName, sizeof(qualifiedMemberName), "%s.%s", qualifiedTypeName, compiledNameText);

        memberReflection = reflection_build_member_info(state,
                                                        compiledNameText,
                                                        qualifiedMemberName,
                                                        kind,
                                                        prototype->super.super.hash ^
                                                                ((TZrUInt64)memberIndex +
                                                                 ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE));
        if (memberReflection == ZR_NULL) {
            return ZR_NULL;
        }

        reflection_assign_owner_links(state, memberReflection, typeReflection, moduleReflection);
        reflection_populate_compiled_member_oop_metadata(state, memberReflection, entryFunction, member);
        reflection_set_field_bool(state, memberReflection, "isStatic", member->isStatic ? ZR_TRUE : ZR_FALSE);
        reflection_set_field_bool(state, memberReflection, "isConst", member->isConst ? ZR_TRUE : ZR_FALSE);
        reflection_set_field_int(state, memberReflection, "parameterCount", member->parameterCount);
        reflection_set_field_int(state, memberReflection, "declarationOrder", member->declarationOrder);

        if (targetKind == ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD) {
            reflection_set_field_string(state,
                                        memberReflection,
                                        "typeName",
                                        fieldTypeName != ZR_NULL ? fieldTypeName : "any");
            reflection_set_field_int(state, memberReflection, "offset", member->fieldOffset);
            reflection_set_field_int(state, memberReflection, "size", member->fieldSize);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "layout", ZR_VALUE_TYPE_OBJECT),
                                     "offset",
                                     member->fieldOffset);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "layout", ZR_VALUE_TYPE_OBJECT),
                                     "size",
                                     member->fieldSize);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "layout", ZR_VALUE_TYPE_OBJECT),
                                     "alignment",
                                     member->fieldSize > 0 ? member->fieldSize : 0);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "ownership", ZR_VALUE_TYPE_OBJECT),
                                     "qualifier",
                                     member->ownershipQualifier);
            reflection_set_field_bool(state,
                                      reflection_get_field_object(state, memberReflection, "ownership", ZR_VALUE_TYPE_OBJECT),
                                      "callsClose",
                                      member->callsClose ? ZR_TRUE : ZR_FALSE);
            reflection_set_field_bool(state,
                                      reflection_get_field_object(state, memberReflection, "ownership", ZR_VALUE_TYPE_OBJECT),
                                      "callsDestructor",
                                      member->callsDestructor ? ZR_TRUE : ZR_FALSE);
            reflection_populate_compiled_member_decorator_metadata(state, memberReflection, entryFunction, member);
            ZrCore_RuntimeDecorator_OverlayMemberReflection(state,
                                                            memberReflection,
                                                            prototype,
                                                            memberName,
                                                            ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD);
        } else {
            SZrFunction *memberFunction = reflection_extract_function_from_constant_index(state,
                                                                                          entryFunction,
                                                                                          member->functionConstantIndex);
            if (member->isMetaMethod && compiledNameText[0] != '\0') {
                TZrChar metaName[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
                snprintf(metaName, sizeof(metaName), "@%s", compiledNameText);
                reflection_set_field_string(state, memberReflection, "name", metaName);
            }
            reflection_set_field_string(state,
                                        memberReflection,
                                        "returnTypeName",
                                        returnTypeName != ZR_NULL ? returnTypeName : "void");
            if (memberFunction != ZR_NULL) {
                reflection_populate_parameters_from_function(state,
                                                             memberReflection,
                                                             memberFunction,
                                                             member->parameterCount);
                reflection_populate_function_metadata(state, memberReflection, memberFunction);
            } else if (member->parameterCount > 0) {
                reflection_populate_parameters_from_typed_refs(state,
                                                               memberReflection,
                                                               ZR_NULL,
                                                               member->parameterCount);
            }
            reflection_populate_compiled_member_decorator_metadata(state, memberReflection, entryFunction, member);

            ZrCore_RuntimeDecorator_OverlayMemberReflection(state,
                                                            memberReflection,
                                                            prototype,
                                                            memberName,
                                                            ZR_RUNTIME_DECORATOR_TARGET_KIND_METHOD);
        }

        if (membersObject != ZR_NULL) {
            reflection_add_named_entry(state, membersObject, compiledNameText, memberReflection);
        }

        return memberReflection;
    }

    return ZR_NULL;
}

static void reflection_populate_type_decorator_metadata(SZrState *state,
                                                        SZrObject *typeReflection,
                                                        SZrFunction *entryFunction,
                                                        const SZrCompiledPrototypeInfo *prototypeInfo) {
    SZrObject *decoratorsArray;

    if (state == ZR_NULL || typeReflection == ZR_NULL || entryFunction == ZR_NULL || prototypeInfo == ZR_NULL) {
        return;
    }

    if (prototypeInfo->hasDecoratorMetadata &&
        prototypeInfo->decoratorMetadataConstantIndex < entryFunction->constantValueLength) {
        const SZrTypeValue *metadataValue = &entryFunction->constantValueList[prototypeInfo->decoratorMetadataConstantIndex];
        if (metadataValue->type == ZR_VALUE_TYPE_OBJECT && metadataValue->value.object != ZR_NULL) {
            reflection_set_field_object(state,
                                        typeReflection,
                                        "metadata",
                                        ZR_CAST_OBJECT(state, metadataValue->value.object),
                                        ZR_VALUE_TYPE_OBJECT);
        }
    }

    decoratorsArray = reflection_get_field_object(state, typeReflection, "decorators", ZR_VALUE_TYPE_ARRAY);
    if (decoratorsArray != ZR_NULL && prototypeInfo->decoratorsCount > 0) {
        const TZrUInt32 *decoratorIndices =
                (const TZrUInt32 *)((const TZrByte *)prototypeInfo + sizeof(SZrCompiledPrototypeInfo) +
                                    ((TZrSize)prototypeInfo->inheritsCount * sizeof(TZrUInt32)));
        for (TZrUInt32 index = 0; index < prototypeInfo->decoratorsCount; index++) {
            TZrUInt32 decoratorNameIndex = decoratorIndices[index];
            const SZrTypeValue *decoratorNameValue;
            SZrObject *decoratorEntry;
            SZrTypeValue decoratorNameFieldValue;

            if (decoratorNameIndex >= entryFunction->constantValueLength) {
                continue;
            }

            decoratorNameValue = &entryFunction->constantValueList[decoratorNameIndex];
            if (decoratorNameValue->type != ZR_VALUE_TYPE_STRING || decoratorNameValue->value.object == ZR_NULL) {
                continue;
            }

            decoratorEntry = reflection_new_object(state);
            if (decoratorEntry == ZR_NULL) {
                continue;
            }

            reflection_init_object_value(state,
                                         &decoratorNameFieldValue,
                                         decoratorNameValue->value.object,
                                         ZR_VALUE_TYPE_STRING);
            reflection_set_field_value(state, decoratorEntry, "name", &decoratorNameFieldValue);
            reflection_array_push_object(state, decoratorsArray, decoratorEntry, ZR_VALUE_TYPE_OBJECT);
        }
    }

}

static void reflection_populate_script_members(SZrState *state,
                                               SZrObject *typeReflection,
                                               SZrObject *moduleReflection,
                                               SZrFunction *entryFunction,
                                               SZrObjectPrototype *prototype) {
    const SZrCompiledPrototypeInfo *prototypeInfo;
    const TZrByte *membersBase;
    SZrObject *membersObject;
    SZrObject *layoutObject;
    const TZrChar *qualifiedTypeName;
    TZrUInt32 scriptFieldCount = 0;
    TZrUInt32 scriptSize = 0;
    TZrUInt32 scriptAlignment = 0;

    if (state == ZR_NULL || typeReflection == ZR_NULL || entryFunction == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    prototypeInfo = reflection_find_compiled_prototype_info(entryFunction, prototype);
    if (prototypeInfo == ZR_NULL) {
        return;
    }

    reflection_populate_type_decorator_metadata(state, typeReflection, entryFunction, prototypeInfo);
    reflection_populate_type_oop_metadata(state, typeReflection, prototypeInfo);
    ZrCore_RuntimeDecorator_OverlayTypeReflection(state, typeReflection, prototype);

    membersObject = reflection_get_field_object(state, typeReflection, "members", ZR_VALUE_TYPE_OBJECT);
    layoutObject = reflection_get_field_object(state, typeReflection, "layout", ZR_VALUE_TYPE_OBJECT);
    qualifiedTypeName = reflection_get_field_string_native(state, typeReflection, "qualifiedName", "");
    membersBase = (const TZrByte *)prototypeInfo + sizeof(SZrCompiledPrototypeInfo) +
                  ((TZrSize)prototypeInfo->inheritsCount * sizeof(TZrUInt32)) +
                  ((TZrSize)prototypeInfo->decoratorsCount * sizeof(TZrUInt32));

    for (TZrUInt32 memberIndex = 0; memberIndex < prototypeInfo->membersCount; memberIndex++) {
        const SZrCompiledMemberInfo *member = (const SZrCompiledMemberInfo *)(membersBase +
                                                                              (sizeof(SZrCompiledMemberInfo) * memberIndex));
        const SZrTypeValue *memberNameValue;
        SZrString *memberNameString;
        const TZrChar *memberName;
        const TZrChar *fieldTypeName;
        const TZrChar *returnTypeName;
        const TZrChar *kind;
        const TZrChar *propertyName = ZR_NULL;
        TZrBool isPropertyAccessor = ZR_FALSE;
        TZrChar qualifiedMemberName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
        SZrObject *memberReflection;

        if (member->nameStringIndex >= entryFunction->constantValueLength) {
            continue;
        }

        memberNameValue = &entryFunction->constantValueList[member->nameStringIndex];
        memberNameString = (memberNameValue->type == ZR_VALUE_TYPE_STRING && memberNameValue->value.object != ZR_NULL)
                                   ? ZR_CAST_STRING(state, memberNameValue->value.object)
                                   : ZR_NULL;
        memberName = memberNameString != ZR_NULL ? ZrCore_String_GetNativeString(memberNameString) : ZR_NULL;
        if (memberName == ZR_NULL || memberName[0] == '\0') {
            continue;
        }

        fieldTypeName = reflection_string_from_constant(state, entryFunction, member->fieldTypeNameStringIndex, "any");
        returnTypeName = reflection_string_from_constant(state, entryFunction, member->returnTypeNameStringIndex, "void");
        isPropertyAccessor = reflection_parse_hidden_property_accessor_name(memberName, &propertyName, ZR_NULL);
        kind = (member->memberType == ZR_AST_CONSTANT_STRUCT_FIELD || member->memberType == ZR_AST_CONSTANT_CLASS_FIELD)
                       ? "field"
                       : "method";
        snprintf(qualifiedMemberName, sizeof(qualifiedMemberName), "%s.%s", qualifiedTypeName, memberName);

        memberReflection = reflection_build_member_info(state,
                                                        memberName,
                                                        qualifiedMemberName,
                                                        kind,
                                                        prototype->super.super.hash ^
                                                                ((TZrUInt64)memberIndex + ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE));
        if (memberReflection == ZR_NULL) {
            continue;
        }

        reflection_assign_owner_links(state, memberReflection, typeReflection, moduleReflection);
        reflection_populate_compiled_member_oop_metadata(state, memberReflection, entryFunction, member);
        reflection_set_field_bool(state, memberReflection, "isStatic", member->isStatic ? ZR_TRUE : ZR_FALSE);
        reflection_set_field_bool(state, memberReflection, "isConst", member->isConst ? ZR_TRUE : ZR_FALSE);
        reflection_set_field_int(state, memberReflection, "parameterCount", member->parameterCount);
        reflection_set_field_int(state, memberReflection, "declarationOrder", member->declarationOrder);

        if (strcmp(kind, "field") == 0) {
            if (!member->isStatic) {
                TZrUInt32 fieldSize = member->fieldSize > 0 ? member->fieldSize : 1u;
                TZrUInt32 fieldEnd = member->fieldOffset + fieldSize;
                scriptFieldCount++;
                if (fieldEnd > scriptSize) {
                    scriptSize = fieldEnd;
                }
                if (fieldSize > scriptAlignment) {
                    scriptAlignment = fieldSize;
                }
            }
            reflection_set_field_string(state, memberReflection, "typeName", fieldTypeName != ZR_NULL ? fieldTypeName : "any");
            reflection_set_field_int(state, memberReflection, "offset", member->fieldOffset);
            reflection_set_field_int(state, memberReflection, "size", member->fieldSize);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "layout", ZR_VALUE_TYPE_OBJECT),
                                     "offset",
                                     member->fieldOffset);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "layout", ZR_VALUE_TYPE_OBJECT),
                                     "size",
                                     member->fieldSize);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "layout", ZR_VALUE_TYPE_OBJECT),
                                     "alignment",
                                     member->fieldSize > 0 ? member->fieldSize : 0);
            reflection_set_field_int(state,
                                     reflection_get_field_object(state, memberReflection, "ownership", ZR_VALUE_TYPE_OBJECT),
                                     "qualifier",
                                     member->ownershipQualifier);
            reflection_set_field_bool(state,
                                      reflection_get_field_object(state, memberReflection, "ownership", ZR_VALUE_TYPE_OBJECT),
                                      "callsClose",
                                      member->callsClose ? ZR_TRUE : ZR_FALSE);
            reflection_set_field_bool(state,
                                      reflection_get_field_object(state, memberReflection, "ownership", ZR_VALUE_TYPE_OBJECT),
                                      "callsDestructor",
                                      member->callsDestructor ? ZR_TRUE : ZR_FALSE);
            reflection_populate_compiled_member_decorator_metadata(state, memberReflection, entryFunction, member);
            if (memberNameString != ZR_NULL) {
                ZrCore_RuntimeDecorator_OverlayMemberReflection(state,
                                                                memberReflection,
                                                                prototype,
                                                                memberNameString,
                                                                ZR_RUNTIME_DECORATOR_TARGET_KIND_FIELD);
            }
        } else {
            SZrFunction *memberFunction = reflection_extract_function_from_constant_index(state,
                                                                                          entryFunction,
                                                                                          member->functionConstantIndex);
            if (member->isMetaMethod && memberName != ZR_NULL && memberName[0] != '\0') {
                TZrChar metaName[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
                snprintf(metaName, sizeof(metaName), "@%s", memberName);
                reflection_set_field_string(state, memberReflection, "name", metaName);
            }
            reflection_set_field_string(state, memberReflection, "returnTypeName", returnTypeName != ZR_NULL ? returnTypeName : "void");
            if (memberFunction != ZR_NULL) {
                reflection_populate_parameters_from_function(state, memberReflection, memberFunction, member->parameterCount);
                reflection_populate_function_metadata(state, memberReflection, memberFunction);
            } else if (member->parameterCount > 0) {
                reflection_populate_parameters_from_typed_refs(state, memberReflection, ZR_NULL, member->parameterCount);
            }
            reflection_populate_compiled_member_decorator_metadata(state, memberReflection, entryFunction, member);

            if (memberNameString != ZR_NULL) {
                ZrCore_RuntimeDecorator_OverlayMemberReflection(state,
                                                                memberReflection,
                                                                prototype,
                                                                memberNameString,
                                                                ZR_RUNTIME_DECORATOR_TARGET_KIND_METHOD);
            }

            if (isPropertyAccessor && propertyName != ZR_NULL) {
                reflection_record_property_member(state,
                                                  membersObject,
                                                  typeReflection,
                                                  moduleReflection,
                                                  prototype,
                                                  qualifiedTypeName,
                                                  propertyName,
                                                  member,
                                                  entryFunction,
                                                  memberFunction,
                                                  prototype->super.super.hash ^
                                                          ((TZrUInt64)memberIndex + ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE +
                                                           (TZrUInt64)0x1000u));
            }
        }

        reflection_add_named_entry(state,
                                   membersObject,
                                   reflection_get_field_string_native(state, memberReflection, "name", memberName),
                                   memberReflection);
    }

    if (layoutObject != ZR_NULL && (scriptFieldCount > 0 || scriptSize > 0 || scriptAlignment > 0)) {
        reflection_set_field_int(state, layoutObject, "fieldCount", scriptFieldCount);
        reflection_set_field_int(state, layoutObject, "size", scriptSize);
        reflection_set_field_int(state, layoutObject, "alignment", scriptAlignment);
    }
}

static SZrObject *reflection_find_native_type_entry(SZrState *state,
                                                    SZrObjectModule *module,
                                                    const TZrChar *typeName) {
    const SZrTypeValue *moduleInfoValue;
    SZrString *infoName;
    SZrObject *moduleInfo;
    SZrObject *typesArray;

    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    infoName = reflection_make_string(state, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    moduleInfoValue = infoName != ZR_NULL ? ZrCore_Module_GetPubExport(state, module, infoName) : ZR_NULL;
    if (moduleInfoValue == ZR_NULL || moduleInfoValue->type != ZR_VALUE_TYPE_OBJECT || moduleInfoValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    typesArray = reflection_get_field_object(state, moduleInfo, "types", ZR_VALUE_TYPE_ARRAY);
    if (typesArray == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < reflection_array_length(typesArray); index++) {
        const SZrTypeValue *entryValue = reflection_array_get(state, typesArray, index);
        SZrObject *entryObject;

        if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
            continue;
        }

        entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
        if (strcmp(reflection_get_field_string_native(state, entryObject, "name", ""), typeName) == 0) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

static void reflection_populate_native_members(SZrState *state,
                                               SZrObject *typeReflection,
                                               SZrObject *moduleReflection,
                                               SZrObject *nativeTypeEntry) {
    SZrObject *membersObject;
    SZrObject *fieldsArray;
    SZrObject *methodsArray;
    const TZrChar *qualifiedTypeName;

    if (state == ZR_NULL || typeReflection == ZR_NULL || nativeTypeEntry == ZR_NULL) {
        return;
    }

    membersObject = reflection_get_field_object(state, typeReflection, "members", ZR_VALUE_TYPE_OBJECT);
    qualifiedTypeName = reflection_get_field_string_native(state, typeReflection, "qualifiedName", "");
    fieldsArray = reflection_get_field_object(state, nativeTypeEntry, "fields", ZR_VALUE_TYPE_ARRAY);
    methodsArray = reflection_get_field_object(state, nativeTypeEntry, "methods", ZR_VALUE_TYPE_ARRAY);

    if (fieldsArray != ZR_NULL) {
        for (TZrUInt32 index = 0; index < reflection_array_length(fieldsArray); index++) {
            const SZrTypeValue *entryValue = reflection_array_get(state, fieldsArray, index);
            SZrObject *entryObject;
            const TZrChar *fieldName;
            TZrChar qualifiedMemberName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
            SZrObject *fieldReflection;

            if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
                continue;
            }

            entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
            fieldName = reflection_get_field_string_native(state, entryObject, "name", "");
            if (fieldName[0] == '\0') {
                continue;
            }

            snprintf(qualifiedMemberName, sizeof(qualifiedMemberName), "%s.%s", qualifiedTypeName, fieldName);
            fieldReflection = reflection_build_member_info(state,
                                                           fieldName,
                                                           qualifiedMemberName,
                                                           "field",
                                                           (TZrUInt64)index + ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE);
            if (fieldReflection == ZR_NULL) {
                continue;
            }

            reflection_assign_owner_links(state, fieldReflection, typeReflection, moduleReflection);
            reflection_set_field_string(state,
                                        fieldReflection,
                                        "typeName",
                                        reflection_get_field_string_native(state, entryObject, "typeName", "any"));
            reflection_add_named_entry(state, membersObject, fieldName, fieldReflection);
        }
    }

    if (methodsArray != ZR_NULL) {
        for (TZrUInt32 index = 0; index < reflection_array_length(methodsArray); index++) {
            const SZrTypeValue *entryValue = reflection_array_get(state, methodsArray, index);
            SZrObject *entryObject;
            const SZrTypeValue *maxArgsValue;
            const TZrChar *methodName;
            TZrChar qualifiedMemberName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
            SZrObject *methodReflection;

            if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
                continue;
            }

            entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
            methodName = reflection_get_field_string_native(state, entryObject, "name", "");
            if (methodName[0] == '\0') {
                continue;
            }

            snprintf(qualifiedMemberName, sizeof(qualifiedMemberName), "%s.%s", qualifiedTypeName, methodName);
            methodReflection = reflection_build_member_info(state,
                                                            methodName,
                                                            qualifiedMemberName,
                                                            "method",
                                                            (TZrUInt64)index + ZR_RUNTIME_REFLECTION_METHOD_HASH_BASE);
            if (methodReflection == ZR_NULL) {
                continue;
            }

            reflection_assign_owner_links(state, methodReflection, typeReflection, moduleReflection);
            reflection_set_field_bool(state,
                                      methodReflection,
                                      "isStatic",
                                      reflection_get_field_bool_value(state, entryObject, "isStatic", ZR_FALSE));
            reflection_set_field_string(state,
                                        methodReflection,
                                        "returnTypeName",
                                        reflection_get_field_string_native(state, entryObject, "returnTypeName", "void"));
            maxArgsValue = reflection_get_field_value(state, entryObject, "maxArgumentCount");
            if (maxArgsValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(maxArgsValue->type)) {
                reflection_set_field_int(state, methodReflection, "parameterCount", maxArgsValue->value.nativeObject.nativeInt64);
            }
            reflection_add_named_entry(state, membersObject, methodName, methodReflection);
        }
    }
}

static SZrObject *reflection_build_variable_reflection(SZrState *state,
                                                       const TZrChar *name,
                                                       const TZrChar *qualifiedName,
                                                       const TZrChar *typeName,
                                                       SZrObject *moduleReflection) {
    SZrObject *variableReflection = reflection_build_member_info(state,
                                                                 name,
                                                                 qualifiedName,
                                                                 "variable",
                                                                 XXH3_64bits(name, strlen(name)));
    if (variableReflection == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_assign_owner_links(state, variableReflection, ZR_NULL, moduleReflection);
    reflection_set_field_string(state, variableReflection, "typeName", typeName != ZR_NULL ? typeName : "any");
    return variableReflection;
}

static SZrObject *reflection_build_callable_reflection(SZrState *state,
                                                       const TZrChar *name,
                                                       const TZrChar *qualifiedName,
                                                       const TZrChar *returnTypeName,
                                                       TZrUInt32 parameterCount,
                                                       TZrBool isStatic,
                                                       SZrObject *ownerReflection,
                                                       SZrObject *moduleReflection,
                                                       TZrUInt64 hash) {
    SZrObject *callableReflection = reflection_build_member_info(state,
                                                                 name,
                                                                 qualifiedName,
                                                                 "function",
                                                                 hash);
    if (callableReflection == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_assign_owner_links(state, callableReflection, ownerReflection, moduleReflection);
    reflection_set_field_string(state,
                                callableReflection,
                                "returnTypeName",
                                returnTypeName != ZR_NULL ? returnTypeName : "void");
    reflection_set_field_int(state, callableReflection, "parameterCount", parameterCount);
    reflection_set_field_bool(state, callableReflection, "isStatic", isStatic);
    reflection_set_field_bool(state, callableReflection, "isVariadic", ZR_FALSE);
    reflection_set_field_null(state, callableReflection, "returnType");
    reflection_set_field_object(state, callableReflection, "parameters", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);
    reflection_set_field_object(state, callableReflection, "parameterModes", reflection_new_array(state), ZR_VALUE_TYPE_ARRAY);
    return callableReflection;
}

static SZrObject *reflection_build_type_literal_object_internal(SZrState *state, const TZrChar *typeName) {
    SZrObject *reflectionObject;
    const TZrChar *normalizedName = typeName != ZR_NULL && typeName[0] != '\0' ? typeName : "type";

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    reflectionObject = reflection_new_object(state);
    if (reflectionObject == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_init_common_fields(state,
                                  reflectionObject,
                                  normalizedName,
                                  normalizedName,
                                  "type",
                                  XXH3_64bits(normalizedName, strlen(normalizedName)));
    return reflectionObject;
}

ZR_CORE_API SZrObject *ZrCore_Reflection_BuildTypeLiteralObject(SZrState *state, SZrString *typeName) {
    const TZrChar *nativeTypeName = typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeName) : ZR_NULL;
    return reflection_build_type_literal_object_internal(state, nativeTypeName);
}

ZR_CORE_API SZrObject *ZrCore_Reflection_BuildCallableTypeLiteralObject(
        SZrState *state,
        SZrString *callableName,
        SZrString *returnTypeName,
        SZrString *const *parameterNames,
        SZrString *const *parameterTypeNames,
        SZrString *const *parameterModeNames,
        TZrUInt32 parameterCount,
        SZrString *const *genericParameterNames,
        TZrUInt32 genericParameterCount,
        TZrBool isVariadic) {
    const TZrChar *nativeCallableName;
    const TZrChar *nativeReturnTypeName;
    SZrObject *callableReflection;
    SZrObject *returnTypeObject;
    SZrObject *parameterModesArray;
    SZrObject *genericParametersArray;
    TZrUInt64 ownerHash;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    nativeCallableName = callableName != ZR_NULL ? ZrCore_String_GetNativeString(callableName) : ZR_NULL;
    nativeReturnTypeName = returnTypeName != ZR_NULL ? ZrCore_String_GetNativeString(returnTypeName) : "void";
    callableReflection = reflection_build_callable_reflection(state,
                                                              nativeCallableName != ZR_NULL ? nativeCallableName : "callable",
                                                              nativeCallableName != ZR_NULL ? nativeCallableName : "callable",
                                                              nativeReturnTypeName,
                                                              parameterCount,
                                                              ZR_FALSE,
                                                              ZR_NULL,
                                                              ZR_NULL,
                                                              XXH3_64bits(nativeCallableName != ZR_NULL
                                                                                  ? nativeCallableName
                                                                                  : "callable",
                                                                          strlen(nativeCallableName != ZR_NULL
                                                                                         ? nativeCallableName
                                                                                         : "callable")));
    if (callableReflection == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_set_field_bool(state, callableReflection, "isVariadic", isVariadic ? ZR_TRUE : ZR_FALSE);

    returnTypeObject = reflection_build_type_literal_object_internal(state, nativeReturnTypeName);
    if (returnTypeObject != ZR_NULL) {
        reflection_set_field_object(state, callableReflection, "returnType", returnTypeObject, ZR_VALUE_TYPE_OBJECT);
    }

    parameterModesArray = reflection_get_field_object(state, callableReflection, "parameterModes", ZR_VALUE_TYPE_ARRAY);
    genericParametersArray =
            reflection_get_field_object(state, callableReflection, "genericParameters", ZR_VALUE_TYPE_ARRAY);
    ownerHash = XXH3_64bits(nativeCallableName != ZR_NULL ? nativeCallableName : "callable",
                            strlen(nativeCallableName != ZR_NULL ? nativeCallableName : "callable"));

    if (parameterCount > 0) {
        SZrObject *parametersArray = reflection_get_field_object(state, callableReflection, "parameters", ZR_VALUE_TYPE_ARRAY);
        for (TZrUInt32 index = 0; index < parameterCount; index++) {
            const TZrChar *nativeParameterName =
                    parameterNames != ZR_NULL && parameterNames[index] != ZR_NULL
                            ? ZrCore_String_GetNativeString(parameterNames[index])
                            : ZR_NULL;
            const TZrChar *nativeParameterTypeName =
                    parameterTypeNames != ZR_NULL && parameterTypeNames[index] != ZR_NULL
                            ? ZrCore_String_GetNativeString(parameterTypeNames[index])
                            : "any";
            const TZrChar *nativeParameterMode =
                    parameterModeNames != ZR_NULL && parameterModeNames[index] != ZR_NULL
                            ? ZrCore_String_GetNativeString(parameterModeNames[index])
                            : "value";
            TZrChar fallbackName[ZR_RUNTIME_MEMBER_NAME_BUFFER_LENGTH];
            SZrObject *parameterReflection;
            SZrObject *parameterTypeObject;

            snprintf(fallbackName, sizeof(fallbackName), "arg%u", (unsigned int)index);
            parameterReflection = reflection_build_parameter_info(state,
                                                                  nativeParameterName != ZR_NULL &&
                                                                                  nativeParameterName[0] != '\0'
                                                                          ? nativeParameterName
                                                                          : fallbackName,
                                                                  nativeParameterTypeName,
                                                                  index,
                                                                  callableReflection,
                                                                  ZR_NULL,
                                                                  ownerHash ^
                                                                          ((TZrUInt64)index +
                                                                           ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE));
            if (parameterReflection == ZR_NULL) {
                continue;
            }

            reflection_set_field_string(state,
                                        parameterReflection,
                                        "passingMode",
                                        nativeParameterMode != ZR_NULL ? nativeParameterMode : "value");
            parameterTypeObject =
                    reflection_build_type_literal_object_internal(state, nativeParameterTypeName != ZR_NULL
                                                                              ? nativeParameterTypeName
                                                                              : "any");
            if (parameterTypeObject != ZR_NULL) {
                reflection_set_field_object(state, parameterReflection, "type", parameterTypeObject, ZR_VALUE_TYPE_OBJECT);
            }

            if (parametersArray != ZR_NULL) {
                reflection_array_push_object(state, parametersArray, parameterReflection, ZR_VALUE_TYPE_OBJECT);
            }
            if (parameterModesArray != ZR_NULL) {
                reflection_array_push_string_value(state,
                                                  parameterModesArray,
                                                  nativeParameterMode != ZR_NULL ? nativeParameterMode : "value");
            }
        }
    }

    if (genericParametersArray != ZR_NULL && genericParameterCount > 0) {
        for (TZrUInt32 index = 0; index < genericParameterCount; index++) {
            const TZrChar *nativeGenericName =
                    genericParameterNames != ZR_NULL && genericParameterNames[index] != ZR_NULL
                            ? ZrCore_String_GetNativeString(genericParameterNames[index])
                            : ZR_NULL;
            if (nativeGenericName != ZR_NULL && nativeGenericName[0] != '\0') {
                reflection_array_push_string_value(state, genericParametersArray, nativeGenericName);
            }
        }
    }

    return callableReflection;
}

static SZrObject *reflection_build_module_reflection(SZrState *state, SZrObjectModule *module) {
    SZrTypeValue cacheKey;
    SZrObject *cached;
    SZrObject *moduleReflection;
    SZrObject *declarationsObject;
    SZrObject *variablesObject;
    SZrObject *membersObject;
    SZrString *infoName;
    const SZrTypeValue *moduleInfoValue;
    SZrObject *moduleInfo = ZR_NULL;
    const TZrChar *moduleName;

    if (state == ZR_NULL || module == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_init_object_value(state, &cacheKey, ZR_CAST_RAW_OBJECT_AS_SUPER(module), ZR_VALUE_TYPE_OBJECT);
    cached = reflection_cache_get(state, &cacheKey);
    if (cached != ZR_NULL) {
        return cached;
    }

    moduleName = module->moduleName != ZR_NULL ? ZrCore_String_GetNativeString(module->moduleName) :
                 (module->fullPath != ZR_NULL ? ZrCore_String_GetNativeString(module->fullPath) : "");
    moduleReflection = reflection_new_object(state);
    if (moduleReflection == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_init_common_fields(state,
                                  moduleReflection,
                                  moduleName != ZR_NULL ? moduleName : "",
                                  moduleName != ZR_NULL ? moduleName : "",
                                  "module",
                                  module->pathHash);
    declarationsObject = reflection_new_collection(state);
    variablesObject = reflection_new_collection(state);
    membersObject = reflection_get_field_object(state, moduleReflection, "members", ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, moduleReflection, "declarations", declarationsObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_object(state, moduleReflection, "variables", variablesObject, ZR_VALUE_TYPE_OBJECT);
    reflection_set_field_null(state, moduleReflection, "entry");
    reflection_set_field_null(state, moduleReflection, "__entry");
    reflection_set_field_string(state,
                                reflection_get_field_object(state, moduleReflection, "source", ZR_VALUE_TYPE_OBJECT),
                                "moduleName",
                                moduleName != ZR_NULL ? moduleName : "");

    reflection_cache_put(state, &cacheKey, moduleReflection);

    infoName = reflection_make_string(state, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    moduleInfoValue = infoName != ZR_NULL ? ZrCore_Module_GetPubExport(state, module, infoName) : ZR_NULL;
    if (moduleInfoValue != ZR_NULL && moduleInfoValue->type == ZR_VALUE_TYPE_OBJECT && moduleInfoValue->value.object != ZR_NULL) {
        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    }

    if (moduleInfo != ZR_NULL) {
        SZrObject *typesArray = reflection_get_field_object(state, moduleInfo, "types", ZR_VALUE_TYPE_ARRAY);
        SZrObject *functionsArray = reflection_get_field_object(state, moduleInfo, "functions", ZR_VALUE_TYPE_ARRAY);
        SZrObject *constantsArray = reflection_get_field_object(state, moduleInfo, "constants", ZR_VALUE_TYPE_ARRAY);

        reflection_set_field_object(state,
                                    reflection_get_field_object(state, moduleReflection, "nativeOrigin", ZR_VALUE_TYPE_OBJECT),
                                    "moduleInfo",
                                    moduleInfo,
                                    ZR_VALUE_TYPE_OBJECT);
        reflection_set_field_string(state,
                                    reflection_get_field_object(state, moduleReflection, "nativeOrigin", ZR_VALUE_TYPE_OBJECT),
                                    "registrationKind",
                                    reflection_get_field_string_native(state, moduleInfo, "registrationKind", ""));
        reflection_set_field_string(state,
                                    reflection_get_field_object(state, moduleReflection, "nativeOrigin", ZR_VALUE_TYPE_OBJECT),
                                    "sourcePath",
                                    reflection_get_field_string_native(state, moduleInfo, "sourcePath", ""));
        reflection_set_field_string(state,
                                    reflection_get_field_object(state, moduleReflection, "nativeOrigin", ZR_VALUE_TYPE_OBJECT),
                                    "moduleVersion",
                                    reflection_get_field_string_native(state, moduleInfo, "moduleVersion", ""));
        {
            const SZrTypeValue *runtimeAbiValue = reflection_get_field_value(state, moduleInfo, "runtimeAbiVersion");
            if (runtimeAbiValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(runtimeAbiValue->type)) {
                reflection_set_field_int(state,
                                         reflection_get_field_object(state, moduleReflection, "nativeOrigin", ZR_VALUE_TYPE_OBJECT),
                                         "runtimeAbiVersion",
                                         runtimeAbiValue->value.nativeObject.nativeInt64);
            }
        }

        if (typesArray != ZR_NULL) {
            for (TZrUInt32 index = 0; index < reflection_array_length(typesArray); index++) {
                const SZrTypeValue *entryValue = reflection_array_get(state, typesArray, index);
                SZrObject *entryObject;
                const TZrChar *typeName;
                SZrString *typeNameString;
                const SZrTypeValue *prototypeValue;
                SZrObjectPrototype *prototype = ZR_NULL;
                SZrObject *typeReflection;

                if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
                    continue;
                }

                entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
                typeName = reflection_get_field_string_native(state, entryObject, "name", "");
                typeNameString = reflection_make_string(state, typeName);
                prototypeValue = typeNameString != ZR_NULL ? ZrCore_Module_GetPubExport(state, module, typeNameString) : ZR_NULL;
                if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT && prototypeValue->value.object != ZR_NULL) {
                    SZrObject *prototypeObject = ZR_CAST_OBJECT(state, prototypeValue->value.object);
                    if (prototypeObject != ZR_NULL && prototypeObject->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
                        prototype = (SZrObjectPrototype *)prototypeObject;
                    }
                }

                typeReflection = reflection_build_type_reflection(state, prototype, module, ZR_NULL, entryObject);
                if (typeReflection != ZR_NULL) {
                    reflection_set_field_object(state, typeReflection, "module", moduleReflection, ZR_VALUE_TYPE_OBJECT);
                    reflection_add_named_entry(state, declarationsObject, typeName, typeReflection);
                    reflection_add_named_entry(state, membersObject, typeName, typeReflection);
                }
            }
        }

        if (functionsArray != ZR_NULL) {
            for (TZrUInt32 index = 0; index < reflection_array_length(functionsArray); index++) {
                const SZrTypeValue *entryValue = reflection_array_get(state, functionsArray, index);
                SZrObject *entryObject;
                const SZrTypeValue *maxArgsValue;
                const TZrChar *functionName;
                TZrChar qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
                TZrUInt32 maxArgs = 0;
                SZrObject *functionReflection;

                if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
                    continue;
                }

                entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
                functionName = reflection_get_field_string_native(state, entryObject, "name", "");
                maxArgsValue = reflection_get_field_value(state, entryObject, "maxArgumentCount");
                if (maxArgsValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(maxArgsValue->type)) {
                    maxArgs = (TZrUInt32)maxArgsValue->value.nativeObject.nativeInt64;
                }

                snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", moduleName != ZR_NULL ? moduleName : "", functionName);
                functionReflection = reflection_build_callable_reflection(state,
                                                                         functionName,
                                                                         qualifiedName,
                                                                         reflection_get_field_string_native(state, entryObject, "returnTypeName", "void"),
                                                                         maxArgs,
                                                                         ZR_FALSE,
                                                                         ZR_NULL,
                                                                         moduleReflection,
                                                                         module->pathHash ^ (TZrUInt64)index);
                if (functionReflection != ZR_NULL) {
                    {
                        const SZrTypeValue *placeholderArgsValue =
                                reflection_get_field_value(state, entryObject, "maxArgumentCount");
                        TZrUInt32 placeholderCount = 0;
                        if (placeholderArgsValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(placeholderArgsValue->type)) {
                            placeholderCount = (TZrUInt32)placeholderArgsValue->value.nativeObject.nativeInt64;
                        }
                        if (placeholderCount > 0) {
                            reflection_populate_parameters_from_typed_refs(state, functionReflection, ZR_NULL, placeholderCount);
                        }
                    }
                    reflection_add_named_entry(state, declarationsObject, functionName, functionReflection);
                    reflection_add_named_entry(state, membersObject, functionName, functionReflection);
                }
            }
        }

        if (constantsArray != ZR_NULL) {
            for (TZrUInt32 index = 0; index < reflection_array_length(constantsArray); index++) {
                const SZrTypeValue *entryValue = reflection_array_get(state, constantsArray, index);
                SZrObject *entryObject;
                const TZrChar *variableName;
                const TZrChar *typeName;
                TZrChar qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
                SZrObject *variableReflection;

                if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
                    continue;
                }

                entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
                variableName = reflection_get_field_string_native(state, entryObject, "name", "");
                typeName = reflection_get_field_string_native(state, entryObject, "typeName", "any");
                snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", moduleName != ZR_NULL ? moduleName : "", variableName);
                variableReflection = reflection_build_variable_reflection(state, variableName, qualifiedName, typeName, moduleReflection);
                if (variableReflection != ZR_NULL) {
                    reflection_add_named_entry(state, variablesObject, variableName, variableReflection);
                    reflection_add_named_entry(state, membersObject, variableName, variableReflection);
                }
            }
        }
    }

    if (moduleInfo == ZR_NULL) {
        const SZrTypeValue *entryValue = reflection_get_field_value(state, &module->super, kReflectionEntryFunctionFieldName);
        SZrFunction *entryFunction = reflection_extract_function_from_value(state, entryValue);

        if (entryFunction != ZR_NULL && entryFunction->prototypeInstances != ZR_NULL) {
            TZrUInt32 prototypeLimit = entryFunction->prototypeInstancesLength;
            if (prototypeLimit > entryFunction->prototypeCount) {
                prototypeLimit = entryFunction->prototypeCount;
            }

            for (TZrUInt32 index = 0; index < prototypeLimit; index++) {
                const SZrCompiledPrototypeInfo *prototypeInfo = ZR_NULL;
                SZrObjectPrototype *prototype = entryFunction->prototypeInstances[index];
                const TZrChar *typeName;
                SZrObject *typeReflection;

                if (prototype == ZR_NULL || !reflection_get_compiled_prototype_info_by_index(entryFunction, index, &prototypeInfo) ||
                    prototypeInfo == ZR_NULL || prototypeInfo->accessModifier != ZR_ACCESS_CONSTANT_PUBLIC) {
                    continue;
                }

                typeName = prototype->name != ZR_NULL ? ZrCore_String_GetNativeString(prototype->name) : ZR_NULL;
                if (typeName == ZR_NULL || typeName[0] == '\0' ||
                    reflection_get_field_value(state, declarationsObject, typeName) != ZR_NULL) {
                    continue;
                }

                typeReflection = reflection_build_type_reflection(state, prototype, module, entryFunction, ZR_NULL);
                if (typeReflection != ZR_NULL) {
                    reflection_set_field_object(state, typeReflection, "module", moduleReflection, ZR_VALUE_TYPE_OBJECT);
                    reflection_add_named_entry(state, declarationsObject, typeName, typeReflection);
                    reflection_add_named_entry(state, membersObject, typeName, typeReflection);
                }
            }
        }

        if (entryFunction != ZR_NULL && entryFunction->typedExportedSymbols != ZR_NULL) {
            for (TZrUInt32 index = 0; index < entryFunction->typedExportedSymbolLength; index++) {
                SZrFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
                const SZrTypeValue *exportedValue;
                SZrFunction *exportedFunction;
                const TZrChar *symbolName;
                TZrChar qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];

                if (symbol == ZR_NULL || symbol->name == ZR_NULL) {
                    continue;
                }

                symbolName = ZrCore_String_GetNativeString(symbol->name);
                if (symbolName == ZR_NULL) {
                    continue;
                }

                exportedValue = ZrCore_Module_GetPubExport(state, module, symbol->name);
                snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", moduleName != ZR_NULL ? moduleName : "", symbolName);

                if (exportedValue != ZR_NULL &&
                    exportedValue->type == ZR_VALUE_TYPE_OBJECT &&
                    exportedValue->value.object != ZR_NULL &&
                    ZR_CAST_OBJECT(state, exportedValue->value.object)->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
                    if (reflection_get_field_value(state, declarationsObject, symbolName) != ZR_NULL) {
                        continue;
                    }
                    SZrObject *typeReflection = reflection_build_type_reflection(state,
                                                                                 (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exportedValue->value.object),
                                                                                 module,
                                                                                 entryFunction,
                                                                                 ZR_NULL);
                    if (typeReflection != ZR_NULL) {
                        reflection_set_field_object(state, typeReflection, "module", moduleReflection, ZR_VALUE_TYPE_OBJECT);
                        reflection_add_named_entry(state, declarationsObject, symbolName, typeReflection);
                        reflection_add_named_entry(state, membersObject, symbolName, typeReflection);
                    }
                    continue;
                }

                if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
                    TZrChar returnTypeBuffer[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
                    SZrObject *callableReflection = reflection_build_callable_reflection(state,
                                                                                         symbolName,
                                                                                         qualifiedName,
                                                                                         reflection_type_name_from_typed_type_ref(state,
                                                                                                                                  &symbol->valueType,
                                                                                                                                  returnTypeBuffer,
                                                                                                                                  sizeof(returnTypeBuffer)),
                                                                                         symbol->parameterCount,
                                                                                         ZR_FALSE,
                                                                                         ZR_NULL,
                                                                                         moduleReflection,
                                                                                         module->pathHash ^ (TZrUInt64)index);
                    if (callableReflection != ZR_NULL) {
                        exportedFunction = reflection_extract_function_from_value(state, exportedValue);
                        if (reflection_should_hide_duplicate_callable_export(state,
                                                                             module,
                                                                             symbolName,
                                                                             exportedValue,
                                                                             exportedFunction)) {
                            continue;
                        }
                        if (exportedFunction != ZR_NULL) {
                            reflection_populate_parameters_from_function(state,
                                                                         callableReflection,
                                                                         exportedFunction,
                                                                         symbol->parameterCount);
                            reflection_populate_function_metadata(state, callableReflection, exportedFunction);
                        } else {
                            reflection_populate_parameters_from_typed_refs(state,
                                                                           callableReflection,
                                                                           symbol->parameterTypes,
                                                                           symbol->parameterCount);
                        }
                        reflection_add_named_entry(state, declarationsObject, symbolName, callableReflection);
                        reflection_add_named_entry(state, membersObject, symbolName, callableReflection);
                    }
                } else {
                    const TZrChar *typeName = symbol->valueType.typeName != ZR_NULL
                                                      ? ZrCore_String_GetNativeString(symbol->valueType.typeName)
                                                      : reflection_builtin_type_name(symbol->valueType.baseType);
                    SZrObject *variableReflection = reflection_build_variable_reflection(state,
                                                                                         symbolName,
                                                                                         qualifiedName,
                                                                                         typeName,
                                                                                         moduleReflection);
                    if (variableReflection != ZR_NULL) {
                        reflection_add_named_entry(state, variablesObject, symbolName, variableReflection);
                        reflection_add_named_entry(state, membersObject, symbolName, variableReflection);
                    }
                }
            }
        }

        if (entryFunction != ZR_NULL) {
            reflection_populate_module_compile_time_metadata(state, moduleReflection, entryFunction);
            reflection_populate_module_tests_metadata(state, moduleReflection, entryFunction);

            TZrChar entryQualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
            snprintf(entryQualifiedName,
                     sizeof(entryQualifiedName),
                     "%s.__entry",
                     moduleName != ZR_NULL ? moduleName : "");
            SZrObject *entryReflection = reflection_build_callable_reflection(state,
                                                                              "__entry",
                                                                              entryQualifiedName,
                                                                              "void",
                                                                              entryFunction->parameterCount,
                                                                              ZR_FALSE,
                                                                              ZR_NULL,
                                                                              moduleReflection,
                                                                              module->pathHash ^ ZR_RUNTIME_REFLECTION_ENTRY_HASH_SALT);
            if (entryReflection != ZR_NULL) {
                reflection_populate_parameters_from_function(state, entryReflection, entryFunction, entryFunction->parameterCount);
                reflection_populate_function_metadata(state, entryReflection, entryFunction);
                reflection_set_field_object(state, moduleReflection, "entry", entryReflection, ZR_VALUE_TYPE_OBJECT);
                reflection_set_field_object(state, moduleReflection, "__entry", entryReflection, ZR_VALUE_TYPE_OBJECT);
                reflection_add_named_entry(state, membersObject, "__entry", entryReflection);
            }
        }
    }

    return moduleReflection;
}

static SZrObject *reflection_build_type_reflection(SZrState *state,
                                                   SZrObjectPrototype *prototype,
                                                   SZrObjectModule *module,
                                                   SZrFunction *entryFunction,
                                                   SZrObject *nativeTypeEntry) {
    SZrTypeValue cacheKey;
    SZrObject *cached = ZR_NULL;
    SZrObject *typeReflection;
    SZrObject *moduleReflection = ZR_NULL;
    const TZrChar *name;
    const TZrChar *kind;
    const TZrChar *extendsTypeName = ZR_NULL;
    TZrChar qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
    TZrChar extendsTypeNameBuffer[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];

    if (state == ZR_NULL || (prototype == ZR_NULL && nativeTypeEntry == ZR_NULL)) {
        return ZR_NULL;
    }

    if (prototype != ZR_NULL) {
        reflection_init_object_value(state, &cacheKey, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype), ZR_VALUE_TYPE_OBJECT);
        cached = reflection_cache_get(state, &cacheKey);
        if (cached != ZR_NULL) {
            return cached;
        }
    }

    if (prototype != ZR_NULL && (module == ZR_NULL || entryFunction == ZR_NULL)) {
        reflection_get_prototype_metadata_context(state, prototype, &module, &entryFunction);
    }

    name = prototype != ZR_NULL && prototype->name != ZR_NULL
                   ? ZrCore_String_GetNativeString(prototype->name)
                   : reflection_get_field_string_native(state, nativeTypeEntry, "name", "type");
    if (prototype != ZR_NULL) {
        kind = reflection_prototype_kind_name(prototype->type);
    } else {
        const SZrTypeValue *prototypeTypeValue = reflection_get_field_value(state, nativeTypeEntry, "prototypeType");
        TZrInt64 prototypeType = (prototypeTypeValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(prototypeTypeValue->type))
                                         ? prototypeTypeValue->value.nativeObject.nativeInt64
                                         : ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
        kind = reflection_prototype_kind_name((EZrObjectPrototypeType)prototypeType);
    }

    if (module != ZR_NULL && module->moduleName != ZR_NULL) {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", ZrCore_String_GetNativeString(module->moduleName), name);
    } else {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s", name);
    }

    typeReflection = reflection_new_object(state);
    if (typeReflection == ZR_NULL) {
        return ZR_NULL;
    }

    reflection_init_common_fields(state,
                                  typeReflection,
                                  name,
                                  qualifiedName,
                                  kind,
                                  prototype != ZR_NULL ? prototype->super.super.hash : XXH3_64bits(qualifiedName, strlen(qualifiedName)));
    if (prototype != ZR_NULL) {
        reflection_cache_put(state, &cacheKey, typeReflection);
    }

    if (module != ZR_NULL) {
        moduleReflection = reflection_build_module_reflection(state, module);
        if (moduleReflection != ZR_NULL) {
            reflection_set_field_object(state, typeReflection, "module", moduleReflection, ZR_VALUE_TYPE_OBJECT);
        }
        reflection_set_field_string(state,
                                    reflection_get_field_object(state, typeReflection, "source", ZR_VALUE_TYPE_OBJECT),
                                    "moduleName",
                                    module->moduleName != ZR_NULL ? ZrCore_String_GetNativeString(module->moduleName) : "");
    }

    if (nativeTypeEntry == ZR_NULL && module != ZR_NULL && name != ZR_NULL) {
        nativeTypeEntry = reflection_find_native_type_entry(state, module, name);
    }

    if (prototype != ZR_NULL && prototype->superPrototype != ZR_NULL) {
        extendsTypeName = reflection_prototype_qualified_name(state,
                                                              prototype->superPrototype,
                                                              extendsTypeNameBuffer,
                                                              sizeof(extendsTypeNameBuffer));
    } else if (nativeTypeEntry != ZR_NULL) {
        extendsTypeName = reflection_get_field_string_native(state, nativeTypeEntry, "extendsTypeName", ZR_NULL);
    }
    if (extendsTypeName != ZR_NULL && extendsTypeName[0] != '\0') {
        reflection_set_field_string(state, typeReflection, "extendsTypeName", extendsTypeName);
    }

    if (nativeTypeEntry != ZR_NULL) {
        reflection_populate_native_members(state, typeReflection, moduleReflection, nativeTypeEntry);
        reflection_set_field_object(state,
                                    reflection_get_field_object(state, typeReflection, "nativeOrigin", ZR_VALUE_TYPE_OBJECT),
                                    "typeEntry",
                                    nativeTypeEntry,
                                    ZR_VALUE_TYPE_OBJECT);
    } else if (prototype != ZR_NULL && entryFunction != ZR_NULL) {
        reflection_populate_script_members(state, typeReflection, moduleReflection, entryFunction, prototype);
    }

    reflection_populate_type_layout(state, typeReflection, prototype, nativeTypeEntry);

    return typeReflection;
}

static void reflection_append(char *buffer, TZrSize bufferSize, TZrSize *offset, const TZrChar *format, ...) {
    va_list args;
    TZrInt32 written;

    if (buffer == ZR_NULL || offset == ZR_NULL || *offset >= bufferSize) {
        return;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, bufferSize - *offset, format, args);
    va_end(args);

    if (written <= 0) {
        return;
    }

    *offset += (TZrSize)written;
    if (*offset >= bufferSize) {
        *offset = bufferSize - 1;
    }
}

static void reflection_format_member_signature(SZrState *state,
                                               SZrObject *memberReflection,
                                               char *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *offset) {
    const TZrChar *kind = reflection_get_field_string_native(state, memberReflection, "kind", "member");
    const TZrChar *name = reflection_get_field_string_native(state, memberReflection, "name", "member");
    TZrBool isStatic = reflection_get_field_bool_value(state, memberReflection, "isStatic", ZR_FALSE);
    const SZrTypeValue *parametersValue = reflection_get_field_value(state, memberReflection, "parameters");

    if (strcmp(kind, "field") == 0 || strcmp(kind, "variable") == 0) {
        reflection_append(buffer,
                          bufferSize,
                          offset,
                          "%s:%s;\n",
                          name,
                          reflection_get_field_string_native(state, memberReflection, "typeName", "any"));
        return;
    }

    if (strcmp(kind, "class") == 0 || strcmp(kind, "struct") == 0 ||
        strcmp(kind, "interface") == 0 || strcmp(kind, "enum") == 0) {
        reflection_append(buffer, bufferSize, offset, "%s %s;\n", kind, name);
        return;
    }

    if (isStatic) {
        reflection_append(buffer, bufferSize, offset, "static ");
    }

    if (strcmp(kind, "function") == 0) {
        reflection_append(buffer,
                          bufferSize,
                          offset,
                          "%s(",
                          name);
    } else {
        reflection_append(buffer, bufferSize, offset, "%s(", name);
    }

    if (parametersValue != ZR_NULL && parametersValue->type == ZR_VALUE_TYPE_ARRAY && parametersValue->value.object != ZR_NULL) {
        SZrObject *parametersArray = ZR_CAST_OBJECT(state, parametersValue->value.object);
        TZrUInt32 parameterCount = reflection_array_length(parametersArray);
        for (TZrUInt32 index = 0; index < parameterCount; index++) {
            const SZrTypeValue *parameterEntryValue = reflection_array_get(state, parametersArray, index);
            SZrObject *parameterReflection = (parameterEntryValue != ZR_NULL &&
                                             parameterEntryValue->type == ZR_VALUE_TYPE_OBJECT &&
                                             parameterEntryValue->value.object != ZR_NULL)
                                                    ? ZR_CAST_OBJECT(state, parameterEntryValue->value.object)
                                                    : ZR_NULL;
            if (parameterReflection == ZR_NULL) {
                continue;
            }
            if (index > 0) {
                reflection_append(buffer, bufferSize, offset, ", ");
            }
            reflection_append(buffer,
                              bufferSize,
                              offset,
                              "%s:%s",
                              reflection_get_field_string_native(state, parameterReflection, "name", "arg"),
                              reflection_get_field_string_native(state, parameterReflection, "typeName", "any"));
        }
    } else {
        const SZrTypeValue *parameterCountValue = reflection_get_field_value(state, memberReflection, "parameterCount");
        TZrUInt32 parameterCount = 0;
        if (parameterCountValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(parameterCountValue->type)) {
            parameterCount = (TZrUInt32)parameterCountValue->value.nativeObject.nativeInt64;
        }
        for (TZrUInt32 index = 0; index < parameterCount; index++) {
            if (index > 0) {
                reflection_append(buffer, bufferSize, offset, ", ");
            }
            reflection_append(buffer, bufferSize, offset, "arg%u:any", (unsigned int)index);
        }
    }
    reflection_append(buffer,
                      bufferSize,
                      offset,
                      "): %s;\n",
                      reflection_get_field_string_native(state, memberReflection, "returnTypeName", "void"));
}

static void reflection_format_members_one_level(SZrState *state,
                                                SZrObject *membersObject,
                                                char *buffer,
                                                TZrSize bufferSize,
                                                TZrSize *offset) {
    SZrObject *orderArray;

    if (state == ZR_NULL || membersObject == ZR_NULL) {
        return;
    }

    orderArray = reflection_get_collection_order_array(state, membersObject, ZR_FALSE);
    if (orderArray != ZR_NULL) {
        for (TZrUInt32 orderIndex = 0; orderIndex < reflection_array_length(orderArray); orderIndex++) {
            const SZrTypeValue *nameValue = reflection_array_get(state, orderArray, orderIndex);
            const TZrChar *memberName;
            const SZrTypeValue *entriesValue;

            if (nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING || nameValue->value.object == ZR_NULL) {
                continue;
            }

            memberName = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, nameValue->value.object));
            entriesValue = reflection_get_field_value(state, membersObject, memberName);
            if (entriesValue == ZR_NULL || entriesValue->type != ZR_VALUE_TYPE_ARRAY || entriesValue->value.object == ZR_NULL) {
                continue;
            }

            {
                SZrObject *entriesArray = ZR_CAST_OBJECT(state, entriesValue->value.object);
                for (TZrUInt32 entryIndex = 0; entryIndex < reflection_array_length(entriesArray); entryIndex++) {
                    const SZrTypeValue *entryValue = reflection_array_get(state, entriesArray, entryIndex);
                    SZrObject *entryObject = (entryValue != ZR_NULL &&
                                              entryValue->type == ZR_VALUE_TYPE_OBJECT &&
                                              entryValue->value.object != ZR_NULL)
                                                     ? ZR_CAST_OBJECT(state, entryValue->value.object)
                                                     : ZR_NULL;
                    if (entryObject != ZR_NULL) {
                        reflection_format_member_signature(state, entryObject, buffer, bufferSize, offset);
                    }
                }
            }
        }
        return;
    }

    if (!membersObject->nodeMap.isValid || membersObject->nodeMap.buckets == ZR_NULL) {
        return;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < membersObject->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = membersObject->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_ARRAY && pair->value.value.object != ZR_NULL) {
                SZrObject *entriesArray = ZR_CAST_OBJECT(state, pair->value.value.object);
                for (TZrUInt32 entryIndex = 0; entryIndex < reflection_array_length(entriesArray); entryIndex++) {
                    const SZrTypeValue *entryValue = reflection_array_get(state, entriesArray, entryIndex);
                    SZrObject *entryObject = (entryValue != ZR_NULL &&
                                              entryValue->type == ZR_VALUE_TYPE_OBJECT &&
                                              entryValue->value.object != ZR_NULL)
                                                     ? ZR_CAST_OBJECT(state, entryValue->value.object)
                                                     : ZR_NULL;
                    if (entryObject != ZR_NULL) {
                        reflection_format_member_signature(state, entryObject, buffer, bufferSize, offset);
                    }
                }
            }
            pair = pair->next;
        }
    }
}

TZrBool ZrCore_Reflection_IsReflectionObject(SZrState *state, SZrObject *object) {
    const SZrTypeValue *markerValue;

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    markerValue = reflection_get_field_value(state, object, kReflectionMarkerFieldName);
    if (markerValue == ZR_NULL || markerValue->type != ZR_VALUE_TYPE_BOOL) {
        return ZR_FALSE;
    }

    return markerValue->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
}

SZrString *ZrCore_Reflection_FormatObject(SZrState *state, SZrObject *object) {
    char buffer[ZR_RUNTIME_REFLECTION_FORMAT_BUFFER_LENGTH];
    TZrSize offset = 0;
    const TZrChar *kind;
    const TZrChar *name;
    SZrObject *membersObject;

    if (!ZrCore_Reflection_IsReflectionObject(state, object)) {
        return ZR_NULL;
    }

    kind = reflection_get_field_string_native(state, object, "kind", "object");
    name = reflection_get_field_string_native(state,
                                              object,
                                              "qualifiedName",
                                              reflection_get_field_string_native(state, object, "name", "object"));
    membersObject = reflection_get_field_object(state, object, "members", ZR_VALUE_TYPE_OBJECT);

    if (strcmp(kind, "module") == 0) {
        reflection_append(buffer, sizeof(buffer), &offset, "module %s{\n", name);
        reflection_format_members_one_level(state, membersObject, buffer, sizeof(buffer), &offset);
        reflection_append(buffer, sizeof(buffer), &offset, "}");
    } else if (strcmp(kind, "class") == 0 || strcmp(kind, "struct") == 0 ||
               strcmp(kind, "interface") == 0 || strcmp(kind, "enum") == 0) {
        reflection_append(buffer,
                          sizeof(buffer),
                          &offset,
                          "%s %s{\n",
                          kind,
                          reflection_get_field_string_native(state, object, "name", name));
        reflection_format_members_one_level(state, membersObject, buffer, sizeof(buffer), &offset);
        reflection_append(buffer, sizeof(buffer), &offset, "}");
    } else if (strcmp(kind, "function") == 0) {
        const SZrTypeValue *parametersValue = reflection_get_field_value(state, object, "parameters");

        reflection_append(buffer,
                          sizeof(buffer),
                          &offset,
                          "function %s(",
                          reflection_get_field_string_native(state,
                                                             object,
                                                             "qualifiedName",
                                                             reflection_get_field_string_native(state, object, "name", name)));
        if (parametersValue != ZR_NULL && parametersValue->type == ZR_VALUE_TYPE_ARRAY && parametersValue->value.object != ZR_NULL) {
            SZrObject *parametersArray = ZR_CAST_OBJECT(state, parametersValue->value.object);
            for (TZrUInt32 index = 0; index < reflection_array_length(parametersArray); index++) {
                const SZrTypeValue *parameterEntryValue = reflection_array_get(state, parametersArray, index);
                SZrObject *parameterReflection = (parameterEntryValue != ZR_NULL &&
                                                 parameterEntryValue->type == ZR_VALUE_TYPE_OBJECT &&
                                                 parameterEntryValue->value.object != ZR_NULL)
                                                        ? ZR_CAST_OBJECT(state, parameterEntryValue->value.object)
                                                        : ZR_NULL;
                if (parameterReflection == ZR_NULL) {
                    continue;
                }
                if (index > 0) {
                    reflection_append(buffer, sizeof(buffer), &offset, ", ");
                }
                reflection_append(buffer,
                                  sizeof(buffer),
                                  &offset,
                                  "%s:%s",
                                  reflection_get_field_string_native(state, parameterReflection, "name", "arg"),
                                  reflection_get_field_string_native(state, parameterReflection, "typeName", "any"));
            }
        }
        reflection_append(buffer,
                          sizeof(buffer),
                          &offset,
                          "): %s",
                          reflection_get_field_string_native(state, object, "returnTypeName", "void"));
    } else if (strcmp(kind, "method") == 0 || strcmp(kind, "field") == 0 || strcmp(kind, "variable") == 0) {
        reflection_format_member_signature(state, object, buffer, sizeof(buffer), &offset);
        if (offset > 0 && buffer[offset - 1] == '\n') {
            offset--;
            buffer[offset] = '\0';
        }
    } else {
        reflection_append(buffer, sizeof(buffer), &offset, "%%type %s", name);
    }

    return ZrCore_String_Create(state, buffer, offset);
}

void ZrCore_Reflection_AttachModuleRuntimeMetadata(SZrState *state,
                                                   SZrObjectModule *module,
                                                   SZrFunction *entryFunction) {
    SZrTypeValue entryValue;

    if (state == ZR_NULL || module == ZR_NULL || entryFunction == ZR_NULL) {
        return;
    }

    reflection_init_object_value(state, &entryValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction), ZR_VALUE_TYPE_FUNCTION);
    reflection_set_field_value(state, &module->super, kReflectionEntryFunctionFieldName, &entryValue);
}

void ZrCore_Reflection_AttachPrototypeRuntimeMetadata(SZrState *state,
                                                      SZrObjectPrototype *prototype,
                                                      SZrObjectModule *module,
                                                      SZrFunction *entryFunction) {
    if (state == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    if (module != ZR_NULL) {
        reflection_set_field_object(state,
                                    &prototype->super,
                                    kReflectionOwnerModuleFieldName,
                                    &module->super,
                                    ZR_VALUE_TYPE_OBJECT);
    }

    if (entryFunction != ZR_NULL) {
        SZrTypeValue entryValue;
        reflection_init_object_value(state,
                                     &entryValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction),
                                     ZR_VALUE_TYPE_FUNCTION);
        reflection_set_field_value(state, &prototype->super, kReflectionEntryFunctionFieldName, &entryValue);
    }
}

static TZrBool reflection_build_type_of_value(SZrState *state,
                                              const SZrTypeValue *targetValue,
                                              SZrTypeValue *result) {
    SZrObject *reflectionObject = ZR_NULL;
    TZrBool targetValuePinned = ZR_FALSE;

    if (state == ZR_NULL || targetValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reflection_pin_value_object(state, targetValue, &targetValuePinned)) {
        ZrCore_Value_ResetAsNull(result);
        return ZR_FALSE;
    }

    if (targetValue->type == ZR_VALUE_TYPE_OBJECT && targetValue->value.object != ZR_NULL) {
        SZrObject *targetObject = ZR_CAST_OBJECT(state, targetValue->value.object);
        if (targetObject != ZR_NULL && ZrCore_Reflection_IsReflectionObject(state, targetObject)) {
            reflectionObject = targetObject;
        } else if (targetObject != ZR_NULL && targetObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
            reflectionObject = reflection_build_module_reflection(state, (SZrObjectModule *)targetObject);
        } else if (targetObject != ZR_NULL && targetObject->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
            reflectionObject = reflection_build_type_reflection(state,
                                                                (SZrObjectPrototype *)targetObject,
                                                                ZR_NULL,
                                                                ZR_NULL,
                                                                ZR_NULL);
        } else if (targetObject != ZR_NULL && targetObject->prototype != ZR_NULL) {
            reflectionObject = reflection_build_type_reflection(state,
                                                                targetObject->prototype,
                                                                ZR_NULL,
                                                                ZR_NULL,
                                                                ZR_NULL);
        }
    }

    if (reflectionObject == ZR_NULL &&
        (targetValue->type == ZR_VALUE_TYPE_FUNCTION || targetValue->type == ZR_VALUE_TYPE_CLOSURE)) {
        SZrFunction *function = reflection_extract_function_from_value(state, targetValue);
        char returnTypeBuffer[ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH];
        const TZrChar *returnTypeName = "void";
        SZrObjectModule *ownerModule = ZR_NULL;
        SZrObject *moduleReflection = ZR_NULL;
        const TZrChar *exportName = ZR_NULL;
        const TZrChar *callableName = function != ZR_NULL && function->functionName != ZR_NULL
                                              ? ZrCore_String_GetNativeString(function->functionName)
                                              : "callable";
        char qualifiedName[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];

        if (function != ZR_NULL && function->hasCallableReturnType) {
            returnTypeName = reflection_type_name_from_typed_type_ref(state,
                                                                     &function->callableReturnType,
                                                                     returnTypeBuffer,
                                                                     sizeof(returnTypeBuffer));
        }

        if (reflection_find_callable_export_context(state, targetValue, &ownerModule, &exportName) &&
            exportName != ZR_NULL) {
            callableName = exportName;
        }

        if (ownerModule != ZR_NULL) {
            const SZrTypeValue *entryValue = reflection_get_field_value(state, &ownerModule->super, kReflectionEntryFunctionFieldName);
            SZrFunction *entryFunction = reflection_extract_function_from_value(state, entryValue);
            const TZrChar *ownerModuleName = ownerModule->moduleName != ZR_NULL
                                                     ? ZrCore_String_GetNativeString(ownerModule->moduleName)
                                                     : (ownerModule->fullPath != ZR_NULL
                                                                ? ZrCore_String_GetNativeString(ownerModule->fullPath)
                                                                : "");
            moduleReflection = reflection_build_module_reflection(state, ownerModule);
            if (entryFunction != ZR_NULL && entryFunction->typedExportedSymbols != ZR_NULL && exportName != ZR_NULL) {
                for (TZrUInt32 index = 0; index < entryFunction->typedExportedSymbolLength; index++) {
                    SZrFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
                    if (symbol != ZR_NULL &&
                        symbol->name != ZR_NULL &&
                        ZrCore_String_GetNativeString(symbol->name) != ZR_NULL &&
                        strcmp(ZrCore_String_GetNativeString(symbol->name), exportName) == 0) {
                        if (function == ZR_NULL || !function->hasCallableReturnType) {
                            returnTypeName = reflection_type_name_from_typed_type_ref(state,
                                                                                     &symbol->valueType,
                                                                                     returnTypeBuffer,
                                                                                     sizeof(returnTypeBuffer));
                        }
                        break;
                    }
                }
            }
            snprintf(qualifiedName,
                     sizeof(qualifiedName),
                     "%s.%s",
                     ownerModuleName != ZR_NULL ? ownerModuleName : "",
                     callableName != ZR_NULL ? callableName : "callable");
        } else {
            snprintf(qualifiedName, sizeof(qualifiedName), "%s", callableName != ZR_NULL ? callableName : "callable");
        }
        reflectionObject = reflection_build_callable_reflection(state,
                                                                callableName,
                                                                qualifiedName,
                                                                returnTypeName,
                                                                function != ZR_NULL ? function->parameterCount : 0,
                                                                ZR_FALSE,
                                                                ZR_NULL,
                                                                moduleReflection,
                                                                ZrCore_Value_GetHash(state, targetValue));
        if (reflectionObject != ZR_NULL && function != ZR_NULL) {
            reflection_populate_parameters_from_function(state, reflectionObject, function, function->parameterCount);
            reflection_populate_function_metadata(state, reflectionObject, function);
        }
    }

    if (reflectionObject == ZR_NULL) {
        const TZrChar *typeName = reflection_builtin_type_name(targetValue->type);
        reflectionObject = reflection_new_object(state);
        if (reflectionObject != ZR_NULL) {
            reflection_init_common_fields(state,
                                          reflectionObject,
                                          typeName,
                                          typeName,
                                          "type",
                                          XXH3_64bits(typeName, strlen(typeName)));
        }
    }

    if (reflectionObject == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    } else {
        reflection_init_object_value(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(reflectionObject), ZR_VALUE_TYPE_OBJECT);
    }

    reflection_unpin_value_object(state->global, targetValue, targetValuePinned);
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_TypeOfValue(SZrState *state,
                                      const SZrTypeValue *targetValue,
                                      SZrTypeValue *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
        return ZR_TRUE;
    }

    return reflection_build_type_of_value(state, targetValue, result);
}

TZrInt64 ZrCore_Reflection_TypeOfNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrTypeValue *result;
    SZrTypeValue *targetValue;
    SZrFunction *callerFunction;
    const TZrChar *callerName;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    argBase = functionBase + 1;
    result = ZrCore_Stack_GetValue(functionBase);

    if (state->stackTop.valuePointer <= argBase) {
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    targetValue = ZrCore_Stack_GetValue(argBase);
    if (targetValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    callerFunction = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, state->callInfoList);
    callerName = callerFunction != ZR_NULL && callerFunction->functionName != ZR_NULL
                         ? ZrCore_String_GetNativeString(callerFunction->functionName)
                         : "<anonymous>";
    ZrCore_Reflection_TypeOfValue(state, targetValue, result);

    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}
