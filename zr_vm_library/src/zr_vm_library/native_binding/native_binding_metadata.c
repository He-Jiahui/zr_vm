#include "native_binding/native_binding_internal.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/reflection.h"

typedef struct ZrNativeMetadataPin {
    SZrRawObject *object;
    TZrBool addedByCaller;
} ZrNativeMetadataPin;

static TZrBool native_metadata_pin_raw_object(SZrState *state,
                                              SZrRawObject *object,
                                              ZrNativeMetadataPin *pin) {
    if (pin != ZR_NULL) {
        pin->object = object;
        pin->addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL || pin == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(state->global, state, object, &pin->addedByCaller)) {
        pin->object = ZR_NULL;
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool native_metadata_pin_object(SZrState *state, SZrObject *object, ZrNativeMetadataPin *pin) {
    if (object == ZR_NULL || pin == ZR_NULL) {
        return ZR_FALSE;
    }

    return native_metadata_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), pin);
}

static void native_metadata_unpin_object(SZrGlobalState *global, ZrNativeMetadataPin *pin) {
    if (global == ZR_NULL || pin == ZR_NULL || pin->object == ZR_NULL) {
        return;
    }

    if (pin->addedByCaller) {
        ZrCore_GarbageCollector_UnignoreObject(global, pin->object);
    }

    pin->object = ZR_NULL;
    pin->addedByCaller = ZR_FALSE;
}

void native_metadata_set_value_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

void native_metadata_set_string_field(SZrState *state,
                                             SZrObject *object,
                                             const TZrChar *fieldName,
                                             const TZrChar *value) {
    SZrTypeValue fieldValue;
    ZrNativeMetadataPin objectPin = {0};

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    if (!native_metadata_pin_object(state, object, &objectPin)) {
        return;
    }

    if (value == ZR_NULL) {
        ZrLib_Value_SetNull(&fieldValue);
    } else {
        ZrLib_Value_SetString(state, &fieldValue, value);
    }
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
    native_metadata_unpin_object(state->global, &objectPin);
}

static void native_metadata_set_optional_string_field(SZrState *state,
                                                      SZrObject *object,
                                                      const TZrChar *fieldName,
                                                      const TZrChar *value) {
    if (value == ZR_NULL || value[0] == '\0') {
        return;
    }

    native_metadata_set_string_field(state, object, fieldName, value);
}

void native_metadata_set_int_field(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

void native_metadata_set_float_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            TZrFloat64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetFloat(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

void native_metadata_set_bool_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrBool value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

const TZrChar *native_metadata_constant_type_name(const ZrLibConstantDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return "value";
    }

    if (descriptor->typeName != ZR_NULL) {
        return descriptor->typeName;
    }

    switch (descriptor->kind) {
        case ZR_LIB_CONSTANT_KIND_NULL:
            return "null";
        case ZR_LIB_CONSTANT_KIND_BOOL:
            return "bool";
        case ZR_LIB_CONSTANT_KIND_INT:
            return "int";
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            return "float";
        case ZR_LIB_CONSTANT_KIND_STRING:
            return "string";
        case ZR_LIB_CONSTANT_KIND_ARRAY:
            return "array";
        default:
            return "value";
    }
}

TZrBool native_descriptor_allows_value_construction(const ZrLibTypeDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }
    return descriptor->allowValueConstruction;
}

TZrBool native_descriptor_allows_boxed_construction(const ZrLibTypeDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }
    return descriptor->allowBoxedConstruction;
}

void native_metadata_set_constant_value_fields(SZrState *state,
                                                      SZrObject *object,
                                                      EZrLibConstantKind kind,
                                                      TZrInt64 intValue,
                                                      TZrFloat64 floatValue,
                                                      const TZrChar *stringValue,
                                                      TZrBool boolValue) {
    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    native_metadata_set_int_field(state, object, "kind", kind);
    switch (kind) {
        case ZR_LIB_CONSTANT_KIND_BOOL:
            native_metadata_set_bool_field(state, object, "boolValue", boolValue);
            break;
        case ZR_LIB_CONSTANT_KIND_INT:
            native_metadata_set_int_field(state, object, "intValue", intValue);
            break;
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            native_metadata_set_float_field(state, object, "floatValue", floatValue);
            break;
        case ZR_LIB_CONSTANT_KIND_STRING:
            native_metadata_set_string_field(state, object, "stringValue", stringValue);
            break;
        default:
            break;
    }
}

TZrBool native_metadata_push_string_value(SZrState *state, SZrObject *array, const TZrChar *value) {
    SZrTypeValue entryValue;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(state, &entryValue, value);
    return ZrLib_Array_PushValue(state, array, &entryValue);
}

SZrObject *native_metadata_make_string_array(SZrState *state,
                                                    const TZrChar *const *values,
                                                    TZrSize valueCount) {
    SZrObject *array;
    ZrNativeMetadataPin arrayPin = {0};

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL || !native_metadata_pin_object(state, array, &arrayPin)) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < valueCount; index++) {
        if (values[index] != ZR_NULL) {
            native_metadata_push_string_value(state, array, values[index]);
        }
    }

    native_metadata_unpin_object(state->global, &arrayPin);
    return array;
}

SZrObject *native_metadata_make_field_entry(SZrState *state, const ZrLibFieldDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "typeName", descriptor->typeName);
    native_metadata_set_int_field(state, object, "contractRole", (TZrInt64)descriptor->contractRole);
    return object;
}

static SZrObject *native_metadata_make_parameter_entry(SZrState *state, const ZrLibParameterDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "typeName", descriptor->typeName);
    native_metadata_set_string_field(state, object, "documentation", descriptor->documentation);
    return object;
}

static SZrObject *native_metadata_make_parameter_array(SZrState *state,
                                                       const ZrLibParameterDescriptor *parameters,
                                                       TZrSize parameterCount) {
    SZrObject *array;
    ZrNativeMetadataPin arrayPin = {0};

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL || !native_metadata_pin_object(state, array, &arrayPin)) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < parameterCount; index++) {
        SZrObject *parameterEntry = native_metadata_make_parameter_entry(state, &parameters[index]);
        if (parameterEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, parameterEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, array, &entryValue);
        }
    }

    native_metadata_unpin_object(state->global, &arrayPin);
    return array;
}

static SZrObject *native_metadata_make_generic_parameter_entry(SZrState *state,
                                                               const ZrLibGenericParameterDescriptor *descriptor) {
    SZrObject *object;
    SZrObject *constraintsArray;
    SZrTypeValue constraintsValue;
    ZrNativeMetadataPin objectPin = {0};
    ZrNativeMetadataPin constraintsPin = {0};

    if (state == ZR_NULL || descriptor == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !native_metadata_pin_object(state, object, &objectPin)) {
        return ZR_NULL;
    }

    constraintsArray = native_metadata_make_string_array(state,
                                                         descriptor->constraintTypeNames,
                                                         descriptor->constraintTypeCount);
    if (constraintsArray == ZR_NULL || !native_metadata_pin_object(state, constraintsArray, &constraintsPin)) {
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "documentation", descriptor->documentation);
    ZrLib_Value_SetObject(state, &constraintsValue, constraintsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "constraints", &constraintsValue);
    native_metadata_unpin_object(state->global, &constraintsPin);
    native_metadata_unpin_object(state->global, &objectPin);
    return object;
}

static SZrObject *native_metadata_make_generic_parameter_array(SZrState *state,
                                                               const ZrLibGenericParameterDescriptor *parameters,
                                                               TZrSize parameterCount) {
    SZrObject *array;
    ZrNativeMetadataPin arrayPin = {0};

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL || !native_metadata_pin_object(state, array, &arrayPin)) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < parameterCount; index++) {
        SZrObject *parameterEntry = native_metadata_make_generic_parameter_entry(state, &parameters[index]);
        if (parameterEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, parameterEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, array, &entryValue);
        }
    }

    native_metadata_unpin_object(state->global, &arrayPin);
    return array;
}

static SZrObject *native_metadata_make_type_hint_entry(SZrState *state, const ZrLibTypeHintDescriptor *descriptor) {
    SZrObject *object;
    ZrNativeMetadataPin objectPin = {0};

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !native_metadata_pin_object(state, object, &objectPin)) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "symbolName", descriptor->symbolName);
    native_metadata_set_string_field(state, object, "symbolKind", descriptor->symbolKind);
    native_metadata_set_string_field(state, object, "signature", descriptor->signature);
    native_metadata_set_string_field(state, object, "documentation", descriptor->documentation);
    native_metadata_unpin_object(state->global, &objectPin);
    return object;
}

static SZrObject *native_metadata_make_type_hint_array(SZrState *state,
                                                       const ZrLibTypeHintDescriptor *descriptors,
                                                       TZrSize descriptorCount) {
    SZrObject *array;
    ZrNativeMetadataPin arrayPin = {0};

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL || !native_metadata_pin_object(state, array, &arrayPin)) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < descriptorCount; index++) {
        SZrObject *entry = native_metadata_make_type_hint_entry(state, &descriptors[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, array, &entryValue);
        }
    }

    native_metadata_unpin_object(state->global, &arrayPin);
    return array;
}

SZrObject *native_metadata_make_method_entry(SZrState *state, const ZrLibMethodDescriptor *descriptor) {
    SZrObject *object;
    SZrObject *parametersArray;
    SZrObject *genericParametersArray;
    SZrTypeValue parametersValue;
    SZrTypeValue genericParametersValue;
    TZrBool hasParameterMetadata;
    ZrNativeMetadataPin objectPin = {0};
    ZrNativeMetadataPin parametersPin = {0};
    ZrNativeMetadataPin genericParametersPin = {0};

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !native_metadata_pin_object(state, object, &objectPin)) {
        return ZR_NULL;
    }

    hasParameterMetadata = descriptor->parameters != ZR_NULL ||
                           (descriptor->minArgumentCount == 0 && descriptor->maxArgumentCount == 0);
    parametersArray = hasParameterMetadata
                              ? native_metadata_make_parameter_array(state, descriptor->parameters, descriptor->parameterCount)
                              : ZR_NULL;
    genericParametersArray = native_metadata_make_generic_parameter_array(state,
                                                                          descriptor->genericParameters,
                                                                          descriptor->genericParameterCount);
    if ((hasParameterMetadata && parametersArray == ZR_NULL) || genericParametersArray == ZR_NULL ||
        (hasParameterMetadata && !native_metadata_pin_object(state, parametersArray, &parametersPin)) ||
        !native_metadata_pin_object(state, genericParametersArray, &genericParametersPin)) {
        native_metadata_unpin_object(state->global, &genericParametersPin);
        native_metadata_unpin_object(state->global, &parametersPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "returnTypeName", descriptor->returnTypeName);
    native_metadata_set_int_field(state, object, "minArgumentCount", descriptor->minArgumentCount);
    native_metadata_set_int_field(state, object, "maxArgumentCount", descriptor->maxArgumentCount);
    native_metadata_set_bool_field(state, object, "isStatic", descriptor->isStatic);
    native_metadata_set_int_field(state, object, "contractRole", (TZrInt64)descriptor->contractRole);
    if (hasParameterMetadata) {
        native_metadata_set_int_field(state, object, "parameterCount", (TZrInt64)descriptor->parameterCount);
        ZrLib_Value_SetObject(state, &parametersValue, parametersArray, ZR_VALUE_TYPE_ARRAY);
        native_metadata_set_value_field(state, object, "parameters", &parametersValue);
    }
    ZrLib_Value_SetObject(state, &genericParametersValue, genericParametersArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "genericParameters", &genericParametersValue);
    native_metadata_unpin_object(state->global, &genericParametersPin);
    native_metadata_unpin_object(state->global, &parametersPin);
    native_metadata_unpin_object(state->global, &objectPin);
    return object;
}

SZrObject *native_metadata_make_meta_method_entry(SZrState *state,
                                                          const ZrLibMetaMethodDescriptor *descriptor) {
    SZrObject *object;
    SZrObject *parametersArray;
    SZrObject *genericParametersArray;
    SZrTypeValue parametersValue;
    SZrTypeValue genericParametersValue;
    TZrBool hasParameterMetadata;
    ZrNativeMetadataPin objectPin = {0};
    ZrNativeMetadataPin parametersPin = {0};
    ZrNativeMetadataPin genericParametersPin = {0};

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->metaType >= ZR_META_ENUM_MAX) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !native_metadata_pin_object(state, object, &objectPin)) {
        return ZR_NULL;
    }

    hasParameterMetadata = descriptor->parameters != ZR_NULL ||
                           (descriptor->minArgumentCount == 0 && descriptor->maxArgumentCount == 0);
    parametersArray = hasParameterMetadata
                              ? native_metadata_make_parameter_array(state, descriptor->parameters, descriptor->parameterCount)
                              : ZR_NULL;
    genericParametersArray = native_metadata_make_generic_parameter_array(state,
                                                                          descriptor->genericParameters,
                                                                          descriptor->genericParameterCount);
    if ((hasParameterMetadata && parametersArray == ZR_NULL) || genericParametersArray == ZR_NULL ||
        (hasParameterMetadata && !native_metadata_pin_object(state, parametersArray, &parametersPin)) ||
        !native_metadata_pin_object(state, genericParametersArray, &genericParametersPin)) {
        native_metadata_unpin_object(state->global, &genericParametersPin);
        native_metadata_unpin_object(state->global, &parametersPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    native_metadata_set_int_field(state, object, "metaType", descriptor->metaType);
    native_metadata_set_string_field(state, object, "name", CZrMetaName[descriptor->metaType]);
    native_metadata_set_string_field(state, object, "returnTypeName", descriptor->returnTypeName);
    native_metadata_set_int_field(state, object, "minArgumentCount", descriptor->minArgumentCount);
    native_metadata_set_int_field(state, object, "maxArgumentCount", descriptor->maxArgumentCount);
    if (hasParameterMetadata) {
        native_metadata_set_int_field(state, object, "parameterCount", (TZrInt64)descriptor->parameterCount);
        ZrLib_Value_SetObject(state, &parametersValue, parametersArray, ZR_VALUE_TYPE_ARRAY);
        native_metadata_set_value_field(state, object, "parameters", &parametersValue);
    }
    ZrLib_Value_SetObject(state, &genericParametersValue, genericParametersArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "genericParameters", &genericParametersValue);
    native_metadata_unpin_object(state->global, &genericParametersPin);
    native_metadata_unpin_object(state->global, &parametersPin);
    native_metadata_unpin_object(state->global, &objectPin);
    return object;
}

SZrObject *native_metadata_make_function_entry(SZrState *state, const ZrLibFunctionDescriptor *descriptor) {
    SZrObject *object;
    SZrObject *parametersArray;
    SZrObject *genericParametersArray;
    SZrTypeValue parametersValue;
    SZrTypeValue genericParametersValue;
    TZrBool hasParameterMetadata;
    ZrNativeMetadataPin objectPin = {0};
    ZrNativeMetadataPin parametersPin = {0};
    ZrNativeMetadataPin genericParametersPin = {0};

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !native_metadata_pin_object(state, object, &objectPin)) {
        return ZR_NULL;
    }

    hasParameterMetadata = descriptor->parameters != ZR_NULL ||
                           (descriptor->minArgumentCount == 0 && descriptor->maxArgumentCount == 0);
    parametersArray = hasParameterMetadata
                              ? native_metadata_make_parameter_array(state, descriptor->parameters, descriptor->parameterCount)
                              : ZR_NULL;
    genericParametersArray = native_metadata_make_generic_parameter_array(state,
                                                                          descriptor->genericParameters,
                                                                          descriptor->genericParameterCount);
    if ((hasParameterMetadata && parametersArray == ZR_NULL) || genericParametersArray == ZR_NULL ||
        (hasParameterMetadata && !native_metadata_pin_object(state, parametersArray, &parametersPin)) ||
        !native_metadata_pin_object(state, genericParametersArray, &genericParametersPin)) {
        native_metadata_unpin_object(state->global, &genericParametersPin);
        native_metadata_unpin_object(state->global, &parametersPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "returnTypeName", descriptor->returnTypeName);
    native_metadata_set_int_field(state, object, "minArgumentCount", descriptor->minArgumentCount);
    native_metadata_set_int_field(state, object, "maxArgumentCount", descriptor->maxArgumentCount);
    native_metadata_set_int_field(state, object, "contractRole", (TZrInt64)descriptor->contractRole);
    if (hasParameterMetadata) {
        native_metadata_set_int_field(state, object, "parameterCount", (TZrInt64)descriptor->parameterCount);
        ZrLib_Value_SetObject(state, &parametersValue, parametersArray, ZR_VALUE_TYPE_ARRAY);
        native_metadata_set_value_field(state, object, "parameters", &parametersValue);
    }
    ZrLib_Value_SetObject(state, &genericParametersValue, genericParametersArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "genericParameters", &genericParametersValue);
    native_metadata_unpin_object(state->global, &genericParametersPin);
    native_metadata_unpin_object(state->global, &parametersPin);
    native_metadata_unpin_object(state->global, &objectPin);
    return object;
}

SZrObject *native_metadata_make_constant_entry(SZrState *state, const ZrLibConstantDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "typeName", native_metadata_constant_type_name(descriptor));
    native_metadata_set_constant_value_fields(state,
                                              object,
                                              descriptor->kind,
                                              descriptor->intValue,
                                              descriptor->floatValue,
                                              descriptor->stringValue,
                                              descriptor->boolValue);
    return object;
}

SZrObject *native_metadata_make_enum_member_entry(SZrState *state, const ZrLibEnumMemberDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_constant_value_fields(state,
                                              object,
                                              descriptor->kind,
                                              descriptor->intValue,
                                              descriptor->floatValue,
                                              descriptor->stringValue,
                                              descriptor->boolValue);
    native_metadata_set_string_field(state, object, "documentation", descriptor->documentation);
    return object;
}

SZrObject *native_metadata_make_module_link_entry(SZrState *state, const ZrLibModuleLinkDescriptor *descriptor) {
    SZrObject *object;

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_string_field(state, object, "moduleName", descriptor->moduleName);
    native_metadata_set_string_field(state, object, "documentation", descriptor->documentation);
    return object;
}

SZrObject *native_metadata_make_type_entry(SZrState *state, const ZrLibTypeDescriptor *descriptor) {
    SZrObject *object;
    SZrObject *fieldsArray;
    SZrObject *methodsArray;
    SZrObject *metaMethodsArray;
    SZrObject *implementsArray;
    SZrObject *enumMembersArray;
    SZrObject *genericParametersArray;
    TZrSize index;
    SZrTypeValue arrayValue;
    ZrNativeMetadataPin objectPin = {0};
    ZrNativeMetadataPin fieldsPin = {0};
    ZrNativeMetadataPin methodsPin = {0};
    ZrNativeMetadataPin metaMethodsPin = {0};
    ZrNativeMetadataPin implementsPin = {0};
    ZrNativeMetadataPin enumMembersPin = {0};
    ZrNativeMetadataPin genericParametersPin = {0};

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !native_metadata_pin_object(state, object, &objectPin)) {
        return ZR_NULL;
    }

    fieldsArray = ZrLib_Array_New(state);
    if (fieldsArray == ZR_NULL || !native_metadata_pin_object(state, fieldsArray, &fieldsPin)) {
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    methodsArray = ZrLib_Array_New(state);
    if (methodsArray == ZR_NULL || !native_metadata_pin_object(state, methodsArray, &methodsPin)) {
        native_metadata_unpin_object(state->global, &fieldsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    metaMethodsArray = ZrLib_Array_New(state);
    if (metaMethodsArray == ZR_NULL || !native_metadata_pin_object(state, metaMethodsArray, &metaMethodsPin)) {
        native_metadata_unpin_object(state->global, &methodsPin);
        native_metadata_unpin_object(state->global, &fieldsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    implementsArray = native_metadata_make_string_array(state,
                                                        descriptor->implementsTypeNames,
                                                        descriptor->implementsTypeCount);
    if (implementsArray == ZR_NULL || !native_metadata_pin_object(state, implementsArray, &implementsPin)) {
        native_metadata_unpin_object(state->global, &metaMethodsPin);
        native_metadata_unpin_object(state->global, &methodsPin);
        native_metadata_unpin_object(state->global, &fieldsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    enumMembersArray = ZrLib_Array_New(state);
    if (enumMembersArray == ZR_NULL || !native_metadata_pin_object(state, enumMembersArray, &enumMembersPin)) {
        native_metadata_unpin_object(state->global, &implementsPin);
        native_metadata_unpin_object(state->global, &metaMethodsPin);
        native_metadata_unpin_object(state->global, &methodsPin);
        native_metadata_unpin_object(state->global, &fieldsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    genericParametersArray = native_metadata_make_generic_parameter_array(state,
                                                                          descriptor->genericParameters,
                                                                          descriptor->genericParameterCount);
    if (genericParametersArray == ZR_NULL ||
        !native_metadata_pin_object(state, genericParametersArray, &genericParametersPin)) {
        native_metadata_unpin_object(state->global, &enumMembersPin);
        native_metadata_unpin_object(state->global, &implementsPin);
        native_metadata_unpin_object(state->global, &metaMethodsPin);
        native_metadata_unpin_object(state->global, &methodsPin);
        native_metadata_unpin_object(state->global, &fieldsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    native_metadata_set_string_field(state, object, "name", descriptor->name);
    native_metadata_set_int_field(state, object, "prototypeType", descriptor->prototypeType);
    native_metadata_set_string_field(state, object, "extendsTypeName", descriptor->extendsTypeName);
    native_metadata_set_string_field(state, object, "enumValueTypeName", descriptor->enumValueTypeName);
    native_metadata_set_bool_field(state, object, "allowValueConstruction", native_descriptor_allows_value_construction(descriptor));
    native_metadata_set_bool_field(state, object, "allowBoxedConstruction", native_descriptor_allows_boxed_construction(descriptor));
    native_metadata_set_int_field(state, object, "protocolMask", (TZrInt64)descriptor->protocolMask);
    native_metadata_set_string_field(state, object, "constructorSignature", descriptor->constructorSignature);
    native_metadata_set_optional_string_field(state, object, "ffiLoweringKind", descriptor->ffiLoweringKind);
    native_metadata_set_optional_string_field(state, object, "ffiViewTypeName", descriptor->ffiViewTypeName);
    native_metadata_set_optional_string_field(state, object, "ffiUnderlyingTypeName", descriptor->ffiUnderlyingTypeName);
    native_metadata_set_optional_string_field(state, object, "ffiOwnerMode", descriptor->ffiOwnerMode);
    native_metadata_set_optional_string_field(state, object, "ffiReleaseHook", descriptor->ffiReleaseHook);

    for (index = 0; index < descriptor->fieldCount; index++) {
        SZrObject *fieldEntry = native_metadata_make_field_entry(state, &descriptor->fields[index]);
        if (fieldEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, fieldEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, fieldsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->methodCount; index++) {
        SZrObject *methodEntry = native_metadata_make_method_entry(state, &descriptor->methods[index]);
        if (methodEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, methodEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, methodsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->metaMethodCount; index++) {
        SZrObject *metaMethodEntry = native_metadata_make_meta_method_entry(state, &descriptor->metaMethods[index]);
        if (metaMethodEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, metaMethodEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, metaMethodsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->enumMemberCount; index++) {
        SZrObject *enumMemberEntry = native_metadata_make_enum_member_entry(state, &descriptor->enumMembers[index]);
        if (enumMemberEntry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, enumMemberEntry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, enumMembersArray, &entryValue);
        }
    }

    ZrLib_Value_SetObject(state, &arrayValue, fieldsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "fields", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, methodsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "methods", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, metaMethodsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "metaMethods", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, implementsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "implements", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, enumMembersArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "enumMembers", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, genericParametersArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "genericParameters", &arrayValue);

    native_metadata_unpin_object(state->global, &genericParametersPin);
    native_metadata_unpin_object(state->global, &enumMembersPin);
    native_metadata_unpin_object(state->global, &implementsPin);
    native_metadata_unpin_object(state->global, &metaMethodsPin);
    native_metadata_unpin_object(state->global, &methodsPin);
    native_metadata_unpin_object(state->global, &fieldsPin);
    native_metadata_unpin_object(state->global, &objectPin);
    return object;
}

ZR_LIBRARY_API SZrObject *native_metadata_make_module_info(SZrState *state,
                                                           const ZrLibModuleDescriptor *descriptor,
                                                           const ZrLibRegisteredModuleRecord *record) {
    SZrObject *object;
    SZrObject *functionsArray;
    SZrObject *constantsArray;
    SZrObject *typesArray;
    SZrObject *modulesArray;
    SZrObject *typeHintsArray;
    TZrSize index;
    SZrTypeValue arrayValue;
    ZrNativeMetadataPin objectPin = {0};
    ZrNativeMetadataPin functionsPin = {0};
    ZrNativeMetadataPin constantsPin = {0};
    ZrNativeMetadataPin typesPin = {0};
    ZrNativeMetadataPin modulesPin = {0};
    ZrNativeMetadataPin typeHintsPin = {0};

    if (state == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Object_New(state);
    if (object == ZR_NULL || !native_metadata_pin_object(state, object, &objectPin)) {
        return ZR_NULL;
    }

    functionsArray = ZrLib_Array_New(state);
    if (functionsArray == ZR_NULL || !native_metadata_pin_object(state, functionsArray, &functionsPin)) {
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    constantsArray = ZrLib_Array_New(state);
    if (constantsArray == ZR_NULL || !native_metadata_pin_object(state, constantsArray, &constantsPin)) {
        native_metadata_unpin_object(state->global, &functionsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    typesArray = ZrLib_Array_New(state);
    if (typesArray == ZR_NULL || !native_metadata_pin_object(state, typesArray, &typesPin)) {
        native_metadata_unpin_object(state->global, &constantsPin);
        native_metadata_unpin_object(state->global, &functionsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    modulesArray = ZrLib_Array_New(state);
    if (modulesArray == ZR_NULL || !native_metadata_pin_object(state, modulesArray, &modulesPin)) {
        native_metadata_unpin_object(state->global, &typesPin);
        native_metadata_unpin_object(state->global, &constantsPin);
        native_metadata_unpin_object(state->global, &functionsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    typeHintsArray = native_metadata_make_type_hint_array(state, descriptor->typeHints, descriptor->typeHintCount);
    if (typeHintsArray == ZR_NULL || !native_metadata_pin_object(state, typeHintsArray, &typeHintsPin)) {
        native_metadata_unpin_object(state->global, &modulesPin);
        native_metadata_unpin_object(state->global, &typesPin);
        native_metadata_unpin_object(state->global, &constantsPin);
        native_metadata_unpin_object(state->global, &functionsPin);
        native_metadata_unpin_object(state->global, &objectPin);
        return ZR_NULL;
    }

    native_metadata_set_int_field(state, object, "version", ZR_NATIVE_MODULE_INFO_VERSION);
    native_metadata_set_string_field(state, object, "moduleName", descriptor->moduleName);
    native_metadata_set_string_field(state, object, "typeHintsJson", descriptor->typeHintsJson);
    native_metadata_set_string_field(state, object, "moduleVersion", descriptor->moduleVersion);
    native_metadata_set_int_field(state,
                                  object,
                                  "runtimeAbiVersion",
                                  descriptor->minRuntimeAbi != 0 ? descriptor->minRuntimeAbi : ZR_VM_NATIVE_RUNTIME_ABI_VERSION);
    native_metadata_set_int_field(state, object, "requiredCapabilities", (TZrInt64)descriptor->requiredCapabilities);
    native_metadata_set_string_field(state,
                                     object,
                                     "registrationKind",
                                     record != ZR_NULL &&
                                                     record->registrationKind ==
                                                             ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN
                                             ? "descriptor-plugin"
                                             : "builtin");
    native_metadata_set_bool_field(state,
                                   object,
                                   "isDescriptorPlugin",
                                   record != ZR_NULL ? record->isDescriptorPlugin : ZR_FALSE);
    native_metadata_set_string_field(state, object, "sourcePath", record != ZR_NULL ? record->sourcePath : ZR_NULL);

    for (index = 0; index < descriptor->functionCount; index++) {
        SZrObject *entry = native_metadata_make_function_entry(state, &descriptor->functions[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, functionsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->constantCount; index++) {
        SZrObject *entry = native_metadata_make_constant_entry(state, &descriptor->constants[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, constantsArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        SZrObject *entry = native_metadata_make_type_entry(state, &descriptor->types[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, typesArray, &entryValue);
        }
    }

    for (index = 0; index < descriptor->moduleLinkCount; index++) {
        SZrObject *entry = native_metadata_make_module_link_entry(state, &descriptor->moduleLinks[index]);
        if (entry != ZR_NULL) {
            SZrTypeValue entryValue;
            ZrLib_Value_SetObject(state, &entryValue, entry, ZR_VALUE_TYPE_OBJECT);
            ZrLib_Array_PushValue(state, modulesArray, &entryValue);
        }
    }

    ZrLib_Value_SetObject(state, &arrayValue, functionsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "functions", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, constantsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "constants", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, typesArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "types", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, modulesArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "modules", &arrayValue);
    ZrLib_Value_SetObject(state, &arrayValue, typeHintsArray, ZR_VALUE_TYPE_ARRAY);
    native_metadata_set_value_field(state, object, "typeHints", &arrayValue);

    native_metadata_unpin_object(state->global, &typeHintsPin);
    native_metadata_unpin_object(state->global, &modulesPin);
    native_metadata_unpin_object(state->global, &typesPin);
    native_metadata_unpin_object(state->global, &constantsPin);
    native_metadata_unpin_object(state->global, &functionsPin);
    native_metadata_unpin_object(state->global, &objectPin);
    return object;
}

TZrBool native_registry_add_constant(SZrState *state,
                                            SZrObjectModule *module,
                                            const ZrLibConstantDescriptor *descriptor) {
    SZrString *name;
    SZrTypeValue value;

    if (state == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL || descriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    name = native_binding_create_string(state, descriptor->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (descriptor->kind) {
        case ZR_LIB_CONSTANT_KIND_NULL:
            ZrLib_Value_SetNull(&value);
            break;
        case ZR_LIB_CONSTANT_KIND_BOOL:
            ZrLib_Value_SetBool(state, &value, descriptor->boolValue);
            break;
        case ZR_LIB_CONSTANT_KIND_INT:
            ZrLib_Value_SetInt(state, &value, descriptor->intValue);
            break;
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            ZrLib_Value_SetFloat(state, &value, descriptor->floatValue);
            break;
        case ZR_LIB_CONSTANT_KIND_STRING:
            ZrLib_Value_SetString(state, &value, descriptor->stringValue != ZR_NULL ? descriptor->stringValue : "");
            break;
        case ZR_LIB_CONSTANT_KIND_ARRAY: {
            SZrObject *array = ZrLib_Array_New(state);
            if (array == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrLib_Value_SetObject(state, &value, array, ZR_VALUE_TYPE_ARRAY);
            break;
        }
        default:
            ZrLib_Value_SetNull(&value);
            break;
    }

    ZrCore_Module_AddPubExport(state, module, name, &value);
    return ZR_TRUE;
}

TZrBool native_registry_add_function(SZrState *state,
                                            ZrLibrary_NativeRegistryState *registry,
                                            SZrObjectModule *module,
                                            const ZrLibModuleDescriptor *moduleDescriptor,
                                            const ZrLibFunctionDescriptor *functionDescriptor) {
    SZrString *name;
    SZrTypeValue value;

    if (state == ZR_NULL || registry == ZR_NULL || module == ZR_NULL || moduleDescriptor == ZR_NULL ||
        functionDescriptor == ZR_NULL || functionDescriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    native_binding_trace_import(state,
                                "[zr_native_import] add_function begin module=%s name=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                functionDescriptor->name);

    if (!native_binding_make_callable_value(state,
                                            registry,
                                            ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                            moduleDescriptor,
                                            ZR_NULL,
                                            ZR_NULL,
                                            functionDescriptor,
                                            &value)) {
        native_binding_trace_import(state,
                                    "[zr_native_import] add_function failed module=%s name=%s reason=make_callable\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    functionDescriptor->name);
        return ZR_FALSE;
    }

    name = native_binding_create_string(state, functionDescriptor->name);
    if (name == ZR_NULL) {
        native_binding_trace_import(state,
                                    "[zr_native_import] add_function failed module=%s name=%s reason=create_name\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    functionDescriptor->name);
        return ZR_FALSE;
    }

    native_binding_trace_import(state,
                                "[zr_native_import] add_function export module=%s name=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                functionDescriptor->name);
    ZrCore_Module_AddPubExport(state, module, name, &value);
    native_binding_trace_import(state,
                                "[zr_native_import] add_function success module=%s name=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                functionDescriptor->name);
    return ZR_TRUE;
}

TZrBool native_registry_add_module_link(SZrState *state,
                                               ZrLibrary_NativeRegistryState *registry,
                                               SZrObjectModule *module,
                                               const ZrLibModuleLinkDescriptor *descriptor) {
    SZrObjectModule *linkedModule;
    SZrString *name;
    SZrTypeValue value;

    if (state == ZR_NULL || registry == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL ||
        descriptor->name == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    linkedModule = native_registry_resolve_loaded_module(state, registry, descriptor->moduleName);
    if (linkedModule == ZR_NULL) {
        return ZR_FALSE;
    }

    name = native_binding_create_string(state, descriptor->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &value, &linkedModule->super, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Module_AddPubExport(state, module, name, &value);
    return ZR_TRUE;
}

static void native_registry_add_protocol_mask(SZrObjectPrototype *prototype, TZrUInt64 protocolMask) {
    if (prototype == ZR_NULL || protocolMask == 0) {
        return;
    }

    for (EZrProtocolId protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
         protocolId <= ZR_PROTOCOL_ID_ARRAY_LIKE;
         protocolId = (EZrProtocolId)(protocolId + 1)) {
        if ((protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0) {
            ZrCore_ObjectPrototype_AddProtocol(prototype, protocolId);
        }
    }
}

static void native_registry_add_declared_protocols(SZrObjectPrototype *prototype,
                                                   const ZrLibTypeDescriptor *typeDescriptor) {
    if (prototype == ZR_NULL || typeDescriptor == ZR_NULL) {
        return;
    }
    native_registry_add_protocol_mask(prototype, typeDescriptor->protocolMask);
}

static SZrObjectPrototype *native_registry_find_builtin_exception_prototype(
        SZrState *state,
        const ZrLibModuleDescriptor *moduleDescriptor,
        const ZrLibTypeDescriptor *typeDescriptor,
        EZrObjectPrototypeType expectedPrototypeType) {
    if (state == ZR_NULL || state->global == ZR_NULL || moduleDescriptor == ZR_NULL || typeDescriptor == ZR_NULL ||
        moduleDescriptor->moduleName == ZR_NULL || typeDescriptor->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (strcmp(moduleDescriptor->moduleName, "zr.system.exception") != 0) {
        return ZR_NULL;
    }

    if (strcmp(typeDescriptor->name, "Error") == 0 &&
        state->global->errorPrototype != ZR_NULL &&
        state->global->errorPrototype->type == expectedPrototypeType) {
        return state->global->errorPrototype;
    }

    if (strcmp(typeDescriptor->name, "StackFrame") == 0 &&
        state->global->stackFramePrototype != ZR_NULL &&
        state->global->stackFramePrototype->type == expectedPrototypeType) {
        return state->global->stackFramePrototype;
    }

    return ZR_NULL;
}

static void native_registry_add_field_descriptors(SZrState *state,
                                                  SZrObjectPrototype *prototype,
                                                  const ZrLibTypeDescriptor *typeDescriptor) {
    TZrSize index;

    if (state == ZR_NULL || prototype == ZR_NULL || typeDescriptor == ZR_NULL) {
        return;
    }

    for (index = 0; index < typeDescriptor->fieldCount; index++) {
        const ZrLibFieldDescriptor *fieldDescriptor = &typeDescriptor->fields[index];
        SZrString *fieldName;
        SZrMemberDescriptor descriptor;

        if (fieldDescriptor->name == ZR_NULL) {
            continue;
        }

        fieldName = native_binding_create_string(state, fieldDescriptor->name);
        if (fieldName == ZR_NULL) {
            continue;
        }

        ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
        descriptor.name = fieldName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor);

        if (fieldDescriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD) {
            prototype->iteratorContract.currentMemberName = fieldName;
        }
    }
}

static void native_registry_bind_standard_method_contract(SZrObjectPrototype *prototype,
                                                          const ZrLibMethodDescriptor *methodDescriptor,
                                                          SZrFunction *function) {
    if (prototype == ZR_NULL || methodDescriptor == ZR_NULL || methodDescriptor->name == ZR_NULL || function == ZR_NULL) {
        return;
    }

    switch ((EZrMemberContractRole)methodDescriptor->contractRole) {
        case ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT:
            prototype->iterableContract.iterInitFunction = function;
            break;
        case ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT:
            prototype->iteratorContract.moveNextFunction = function;
            break;
        case ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_METHOD:
            prototype->iteratorContract.currentFunction = function;
            break;
        default:
            break;
    }
}

TZrBool native_registry_add_methods(SZrState *state,
                                           ZrLibrary_NativeRegistryState *registry,
                                           const ZrLibModuleDescriptor *moduleDescriptor,
                                           const ZrLibTypeDescriptor *typeDescriptor,
                                           SZrObjectPrototype *prototype) {
    TZrSize index;

    if (state == ZR_NULL || registry == ZR_NULL || moduleDescriptor == ZR_NULL || typeDescriptor == ZR_NULL ||
        prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    native_binding_trace_import(state,
                                "[zr_native_import] add_methods begin module=%s type=%s methods=%llu meta=%llu prototype=%p\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "<null>",
                                (unsigned long long)typeDescriptor->methodCount,
                                (unsigned long long)typeDescriptor->metaMethodCount,
                                (void *)prototype);

    for (index = 0; index < typeDescriptor->methodCount; index++) {
        const ZrLibMethodDescriptor *methodDescriptor = &typeDescriptor->methods[index];
        SZrTypeValue methodValue;
        SZrString *methodName;
        SZrTypeValue methodKey;

        if (methodDescriptor->name == ZR_NULL || methodDescriptor->callback == ZR_NULL) {
            continue;
        }

        native_binding_trace_import(state,
                                    "[zr_native_import] add_methods method module=%s type=%s index=%llu name=%s static=%d\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "<null>",
                                    (unsigned long long)index,
                                    methodDescriptor->name,
                                    methodDescriptor->isStatic ? 1 : 0);

        if (!native_binding_make_callable_value(state,
                                                registry,
                                                ZR_LIB_RESOLVED_BINDING_METHOD,
                                                moduleDescriptor,
                                                typeDescriptor,
                                                prototype,
                                                methodDescriptor,
                                                &methodValue)) {
            return ZR_FALSE;
        }

        methodName = native_binding_create_string(state, methodDescriptor->name);
        if (methodName == ZR_NULL) {
            return ZR_FALSE;
        }

        {
            SZrMemberDescriptor descriptor;
            ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
            descriptor.name = methodName;
            descriptor.kind = methodDescriptor->isStatic ? ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER
                                                         : ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
            descriptor.isStatic = methodDescriptor->isStatic;
            descriptor.isWritable = ZR_FALSE;
            ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor);
        }

        native_registry_bind_standard_method_contract(prototype,
                                                      methodDescriptor,
                                                      ZR_CAST(SZrFunction *, methodValue.value.object));

        ZrCore_Value_InitAsRawObject(state, &methodKey, ZR_CAST_RAW_OBJECT_AS_SUPER(methodName));
        methodKey.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Object_SetValue(state, &prototype->super, &methodKey, &methodValue);
    }

    for (index = 0; index < typeDescriptor->metaMethodCount; index++) {
        const ZrLibMetaMethodDescriptor *metaDescriptor = &typeDescriptor->metaMethods[index];
        SZrTypeValue metaValue;
        SZrString *constructorName;
        SZrTypeValue constructorKey;

        if (metaDescriptor->callback == ZR_NULL || metaDescriptor->metaType >= ZR_META_ENUM_MAX) {
            continue;
        }

        native_binding_trace_import(state,
                                    "[zr_native_import] add_methods meta module=%s type=%s index=%llu meta=%d\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "<null>",
                                    (unsigned long long)index,
                                    (int)metaDescriptor->metaType);

        if (!native_binding_make_callable_value(state,
                                                registry,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                moduleDescriptor,
                                                typeDescriptor,
                                                prototype,
                                                metaDescriptor,
                                                &metaValue)) {
            return ZR_FALSE;
        }

        ZrCore_ObjectPrototype_AddMeta(state,
                                       prototype,
                                       metaDescriptor->metaType,
                                       (SZrFunction *)metaValue.value.object);

        if (metaDescriptor->metaType == ZR_META_GET_ITEM) {
            prototype->indexContract.getByIndexFunction = (SZrFunction *)metaValue.value.object;
        } else if (metaDescriptor->metaType == ZR_META_SET_ITEM) {
            prototype->indexContract.setByIndexFunction = (SZrFunction *)metaValue.value.object;
        }

        if (metaDescriptor->metaType == ZR_META_CONSTRUCTOR) {
            constructorName = native_binding_create_string(state, "__constructor");
            if (constructorName == ZR_NULL) {
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(state, &constructorKey, ZR_CAST_RAW_OBJECT_AS_SUPER(constructorName));
            constructorKey.type = ZR_VALUE_TYPE_STRING;
            ZrCore_Object_SetValue(state, &prototype->super, &constructorKey, &metaValue);
        }
    }

    native_binding_trace_import(state,
                                "[zr_native_import] add_methods success module=%s type=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "<null>");
    return ZR_TRUE;
}

void native_registry_set_hidden_string_metadata(SZrState *state,
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

void native_registry_set_hidden_bool_metadata(SZrState *state,
                                                     SZrObject *object,
                                                     const TZrChar *fieldName,
                                                     TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

TZrBool native_registry_init_enum_member_scalar(SZrState *state,
                                                       const ZrLibEnumMemberDescriptor *descriptor,
                                                       SZrTypeValue *value) {
    if (state == ZR_NULL || descriptor == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (descriptor->kind) {
        case ZR_LIB_CONSTANT_KIND_NULL:
            ZrLib_Value_SetNull(value);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_BOOL:
            ZrLib_Value_SetBool(state, value, descriptor->boolValue);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_INT:
            ZrLib_Value_SetInt(state, value, descriptor->intValue);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_FLOAT:
            ZrLib_Value_SetFloat(state, value, descriptor->floatValue);
            return ZR_TRUE;
        case ZR_LIB_CONSTANT_KIND_STRING:
            ZrLib_Value_SetString(state, value, descriptor->stringValue != ZR_NULL ? descriptor->stringValue : "");
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

SZrObject *native_registry_make_enum_instance(SZrState *state,
                                                     SZrObjectPrototype *prototype,
                                                     const SZrTypeValue *underlyingValue,
                                                     const TZrChar *memberName) {
    SZrObject *enumObject;

    if (state == ZR_NULL || prototype == ZR_NULL || underlyingValue == ZR_NULL) {
        return ZR_NULL;
    }

    enumObject = ZrCore_Object_New(state, prototype);
    if (enumObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Object_Init(state, enumObject);
    ZrLib_Object_SetFieldCString(state, enumObject, kNativeEnumValueFieldName, underlyingValue);
    if (memberName != ZR_NULL) {
        SZrTypeValue nameValue;
        ZrLib_Value_SetString(state, &nameValue, memberName);
        ZrLib_Object_SetFieldCString(state, enumObject, kNativeEnumNameFieldName, &nameValue);
    }

    return enumObject;
}

void native_registry_attach_type_runtime_metadata(SZrState *state,
                                                         const ZrLibTypeDescriptor *typeDescriptor,
                                                         SZrObjectPrototype *prototype) {
    if (state == ZR_NULL || typeDescriptor == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    native_registry_set_hidden_bool_metadata(state,
                                             &prototype->super,
                                             kNativeAllowValueConstructionFieldName,
                                             native_descriptor_allows_value_construction(typeDescriptor));
    native_registry_set_hidden_bool_metadata(state,
                                             &prototype->super,
                                             kNativeAllowBoxedConstructionFieldName,
                                             native_descriptor_allows_boxed_construction(typeDescriptor));
    if (typeDescriptor->ffiLoweringKind != ZR_NULL) {
        native_registry_set_hidden_string_metadata(state,
                                                   &prototype->super,
                                                   kNativeFfiLoweringKindFieldName,
                                                   typeDescriptor->ffiLoweringKind);
    }
    if (typeDescriptor->ffiViewTypeName != ZR_NULL) {
        native_registry_set_hidden_string_metadata(state,
                                                   &prototype->super,
                                                   kNativeFfiViewTypeFieldName,
                                                   typeDescriptor->ffiViewTypeName);
    }
    if (typeDescriptor->ffiUnderlyingTypeName != ZR_NULL) {
        native_registry_set_hidden_string_metadata(state,
                                                   &prototype->super,
                                                   kNativeFfiUnderlyingTypeFieldName,
                                                   typeDescriptor->ffiUnderlyingTypeName);
    }
    if (typeDescriptor->ffiOwnerMode != ZR_NULL) {
        native_registry_set_hidden_string_metadata(state,
                                                   &prototype->super,
                                                   kNativeFfiOwnerModeFieldName,
                                                   typeDescriptor->ffiOwnerMode);
    }
    if (typeDescriptor->ffiReleaseHook != ZR_NULL) {
        native_registry_set_hidden_string_metadata(state,
                                                   &prototype->super,
                                                   kNativeFfiReleaseHookFieldName,
                                                   typeDescriptor->ffiReleaseHook);
    }

    if (typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        native_registry_set_hidden_string_metadata(state,
                                                   &prototype->super,
                                                   kNativeEnumValueTypeFieldName,
                                                   typeDescriptor->enumValueTypeName);
    }
}

TZrBool native_registry_add_enum_members(SZrState *state,
                                                SZrObjectPrototype *prototype,
                                                const ZrLibTypeDescriptor *typeDescriptor) {
    TZrSize index;

    if (state == ZR_NULL || prototype == ZR_NULL || typeDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeDescriptor->prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        return ZR_TRUE;
    }

    for (index = 0; index < typeDescriptor->enumMemberCount; index++) {
        const ZrLibEnumMemberDescriptor *memberDescriptor = &typeDescriptor->enumMembers[index];
        SZrTypeValue scalarValue;
        SZrObject *enumObject;
        SZrTypeValue enumValue;

        if (memberDescriptor->name == ZR_NULL ||
            !native_registry_init_enum_member_scalar(state, memberDescriptor, &scalarValue)) {
            continue;
        }

        enumObject = native_registry_make_enum_instance(state, prototype, &scalarValue, memberDescriptor->name);
        if (enumObject == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrLib_Value_SetObject(state, &enumValue, enumObject, ZR_VALUE_TYPE_OBJECT);
        ZrLib_Object_SetFieldCString(state, &prototype->super, memberDescriptor->name, &enumValue);
    }

    return ZR_TRUE;
}

TZrBool native_registry_add_type(SZrState *state,
                                        ZrLibrary_NativeRegistryState *registry,
                                        SZrObjectModule *module,
                                        const ZrLibModuleDescriptor *moduleDescriptor,
                                        const ZrLibTypeDescriptor *typeDescriptor) {
    SZrString *typeName;
    SZrObjectPrototype *prototype;
    EZrObjectPrototypeType expectedPrototypeType;
    TZrSize index;
    SZrTypeValue prototypeValue;

    if (state == ZR_NULL || registry == ZR_NULL || module == ZR_NULL || moduleDescriptor == ZR_NULL ||
        typeDescriptor == ZR_NULL || typeDescriptor->name == ZR_NULL) {
        return ZR_FALSE;
    }

    native_binding_trace_import(state,
                                "[zr_native_import] add_type begin module=%s type=%s fields=%llu methods=%llu meta=%llu prototype_type=%d\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name,
                                (unsigned long long)typeDescriptor->fieldCount,
                                (unsigned long long)typeDescriptor->methodCount,
                                (unsigned long long)typeDescriptor->metaMethodCount,
                                (int)typeDescriptor->prototypeType);

    typeName = native_binding_create_string(state, typeDescriptor->name);
    if (typeName == ZR_NULL) {
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type failed module=%s type=%s reason=create_type_name\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name);
        return ZR_FALSE;
    }

    expectedPrototypeType = typeDescriptor->prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID
                                    ? typeDescriptor->prototypeType
                                    : ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototype = native_registry_get_module_prototype(state, module, typeDescriptor->name);
    native_binding_trace_import(state,
                                "[zr_native_import] add_type lookup module=%s type=%s existing_prototype=%p expected_type=%d\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name,
                                (void *)prototype,
                                (int)expectedPrototypeType);
    if (prototype == ZR_NULL) {
        prototype = native_registry_find_builtin_exception_prototype(state,
                                                                     moduleDescriptor,
                                                                     typeDescriptor,
                                                                     expectedPrototypeType);
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type builtin_lookup module=%s type=%s prototype=%p\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name,
                                    (void *)prototype);
    }
    if (prototype != ZR_NULL &&
        expectedPrototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID &&
        prototype->type != expectedPrototypeType) {
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type failed module=%s type=%s reason=prototype_type_mismatch actual=%d expected=%d\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name,
                                    (int)prototype->type,
                                    (int)expectedPrototypeType);
        return ZR_FALSE;
    }

    if (prototype == ZR_NULL && typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        prototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, typeName);
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type created_struct module=%s type=%s prototype=%p\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name,
                                    (void *)prototype);
        if (prototype != ZR_NULL) {
            SZrStructPrototype *structPrototype = (SZrStructPrototype *)prototype;
            for (index = 0; index < typeDescriptor->fieldCount; index++) {
                const ZrLibFieldDescriptor *fieldDescriptor = &typeDescriptor->fields[index];
                if (fieldDescriptor->name != ZR_NULL) {
                    SZrString *fieldName = native_binding_create_string(state, fieldDescriptor->name);
                    if (fieldName != ZR_NULL) {
                        ZrCore_StructPrototype_AddField(state, structPrototype, fieldName, index);
                    }
                }
            }
        }
    } else if (prototype == ZR_NULL) {
        prototype = ZrCore_ObjectPrototype_New(state,
                                               typeName,
                                               expectedPrototypeType);
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type created_object module=%s type=%s prototype=%p\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name,
                                    (void *)prototype);
    }

    if (prototype == ZR_NULL) {
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type failed module=%s type=%s reason=create_prototype\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name);
        return ZR_FALSE;
    }

    native_binding_trace_import(state,
                                "[zr_native_import] add_type attach_runtime module=%s type=%s prototype=%p\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name,
                                (void *)prototype);
    native_registry_attach_type_runtime_metadata(state, typeDescriptor, prototype);
    native_binding_trace_import(state,
                                "[zr_native_import] add_type attach_protocols module=%s type=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name);
    native_registry_add_declared_protocols(prototype, typeDescriptor);
    native_binding_trace_import(state,
                                "[zr_native_import] add_type attach_fields module=%s type=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name);
    native_registry_add_field_descriptors(state, prototype, typeDescriptor);
    native_binding_trace_import(state,
                                "[zr_native_import] add_type attach_reflection module=%s type=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name);
    ZrCore_Reflection_AttachPrototypeRuntimeMetadata(state, prototype, module, ZR_NULL);

    if (!native_registry_add_methods(state, registry, moduleDescriptor, typeDescriptor, prototype)) {
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type failed module=%s type=%s reason=add_methods\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name);
        return ZR_FALSE;
    }

    if (!native_registry_add_enum_members(state, prototype, typeDescriptor)) {
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type failed module=%s type=%s reason=add_enum_members\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    native_binding_trace_import(state,
                                "[zr_native_import] add_type export module=%s type=%s prototype=%p\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name,
                                (void *)prototype);
    ZrCore_Module_AddPubExport(state, module, typeName, &prototypeValue);
    native_binding_register_prototype_in_global_scope(state, typeName, &prototypeValue);
    native_binding_trace_import(state,
                                "[zr_native_import] add_type success module=%s type=%s\n",
                                moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                typeDescriptor->name);
    return ZR_TRUE;
}

SZrObjectPrototype *native_registry_get_module_prototype(SZrState *state,
                                                                SZrObjectModule *module,
                                                                const TZrChar *typeName) {
    SZrString *name;
    const SZrTypeValue *exportedValue;
    SZrObject *object;

    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    name = native_binding_create_string(state, typeName);
    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    exportedValue = ZrCore_Module_GetPubExport(state, module, name);
    if (exportedValue == ZR_NULL || exportedValue->type != ZR_VALUE_TYPE_OBJECT || exportedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)object;
}

static SZrObjectPrototype *native_registry_find_same_module_qualified_prototype(SZrState *state,
                                                                                SZrObjectModule *module,
                                                                                const TZrChar *moduleName,
                                                                                const TZrChar *qualifiedTypeName) {
    const TZrChar *genericStart;
    const TZrChar *lastDot;
    TZrSize moduleNameLength;
    TZrSize exportNameLength;
    TZrChar exportNameBuffer[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];

    if (state == ZR_NULL || module == ZR_NULL || moduleName == ZR_NULL || qualifiedTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    genericStart = strchr(qualifiedTypeName, '<');
    lastDot = strrchr(qualifiedTypeName, '.');
    if (lastDot == ZR_NULL || lastDot == qualifiedTypeName || lastDot[1] == '\0' ||
        (genericStart != ZR_NULL && lastDot > genericStart)) {
        return ZR_NULL;
    }

    moduleNameLength = (TZrSize)(lastDot - qualifiedTypeName);
    exportNameLength = genericStart != ZR_NULL
                               ? (TZrSize)(genericStart - (lastDot + 1))
                               : strlen(lastDot + 1);
    if (strncmp(moduleName, qualifiedTypeName, moduleNameLength) != 0 ||
        moduleName[moduleNameLength] != '\0' ||
        exportNameLength == 0 ||
        exportNameLength >= sizeof(exportNameBuffer)) {
        return ZR_NULL;
    }

    memcpy(exportNameBuffer, lastDot + 1, exportNameLength);
    exportNameBuffer[exportNameLength] = '\0';
    return native_registry_get_module_prototype(state, module, exportNameBuffer);
}

void native_registry_resolve_type_relationships(SZrState *state,
                                                       SZrObjectModule *module,
                                                       const ZrLibModuleDescriptor *descriptor) {
    TZrSize index;

    if (state == ZR_NULL || module == ZR_NULL || descriptor == ZR_NULL) {
        return;
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        SZrObjectPrototype *prototype;
        SZrObjectPrototype *superPrototype;

        if (typeDescriptor->name == ZR_NULL) {
            continue;
        }

        prototype = native_registry_get_module_prototype(state, module, typeDescriptor->name);
        if (prototype == ZR_NULL) {
            continue;
        }

        superPrototype = typeDescriptor->extendsTypeName != ZR_NULL
                                 ? native_registry_find_same_module_qualified_prototype(state,
                                                                                         module,
                                                                                         descriptor->moduleName,
                                                                                         typeDescriptor->extendsTypeName)
                                 : ZR_NULL;
        if (superPrototype == ZR_NULL && typeDescriptor->extendsTypeName != ZR_NULL) {
            superPrototype = ZrLib_Type_FindPrototype(state, typeDescriptor->extendsTypeName);
        }
        if (superPrototype == ZR_NULL &&
            typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS &&
            !(descriptor->moduleName != ZR_NULL &&
              strcmp(descriptor->moduleName, "zr.builtin") == 0 &&
              strcmp(typeDescriptor->name, "Object") == 0)) {
            superPrototype = native_registry_find_same_module_qualified_prototype(state,
                                                                                  module,
                                                                                  descriptor->moduleName,
                                                                                  "zr.builtin.Object");
            if (superPrototype == ZR_NULL) {
                superPrototype = ZrLib_Type_FindPrototype(state, "zr.builtin.Object");
            }
        }
        if (superPrototype == ZR_NULL || superPrototype == prototype) {
            continue;
        }

        ZrCore_ObjectPrototype_SetSuper(state, prototype, superPrototype);
    }

    /*
     * 继承链在 SetSuper 之后才能完整解析。若某类本应注册的元方法（例如 zr.ffi.SymbolHandle 的 @call）
     * 未出现在本类 metaTable 中，GetMeta 会沿 superPrototype 落到 zr.builtin.Object 的默认 @call，
     * 表现为 “object meta method is not implemented”。在关系解析完成后补注册缺失的元方法槽位。
     */
    {
        ZrLibrary_NativeRegistryState *registry = native_registry_get(state->global);
        TZrSize typeIndex;
        TZrSize metaIndex;

        if (registry != ZR_NULL) {
            for (typeIndex = 0; typeIndex < descriptor->typeCount; typeIndex++) {
                const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[typeIndex];
                SZrObjectPrototype *prototype;

                if (typeDescriptor->name == ZR_NULL || typeDescriptor->metaMethodCount == 0 ||
                    typeDescriptor->metaMethods == ZR_NULL) {
                    continue;
                }

                prototype = native_registry_get_module_prototype(state, module, typeDescriptor->name);
                if (prototype == ZR_NULL) {
                    continue;
                }

                for (metaIndex = 0; metaIndex < typeDescriptor->metaMethodCount; metaIndex++) {
                    const ZrLibMetaMethodDescriptor *metaDescriptor = &typeDescriptor->metaMethods[metaIndex];
                    SZrTypeValue metaValue;
                    SZrString *constructorName;
                    SZrTypeValue constructorKey;

                    if (metaDescriptor->callback == ZR_NULL || metaDescriptor->metaType >= ZR_META_ENUM_MAX) {
                        continue;
                    }

                    if (prototype->metaTable.metas[metaDescriptor->metaType] != ZR_NULL) {
                        continue;
                    }

                    native_binding_trace_import(state,
                                                "[zr_native_import] repair_missing_meta module=%s type=%s meta=%d "
                                                "prototype=%p\n",
                                                descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                                                typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "<null>",
                                                (int)metaDescriptor->metaType,
                                                (void *)prototype);

                    if (!native_binding_make_callable_value(state,
                                                            registry,
                                                            ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                            descriptor,
                                                            typeDescriptor,
                                                            prototype,
                                                            metaDescriptor,
                                                            &metaValue)) {
                        native_binding_trace_import(state,
                                                    "[zr_native_import] repair_missing_meta failed module=%s type=%s "
                                                    "meta=%d\n",
                                                    descriptor->moduleName != ZR_NULL ? descriptor->moduleName
                                                                                      : "<null>",
                                                    typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "<null>",
                                                    (int)metaDescriptor->metaType);
                        continue;
                    }

                    ZrCore_ObjectPrototype_AddMeta(state,
                                                     prototype,
                                                     metaDescriptor->metaType,
                                                     (SZrFunction *)metaValue.value.object);

                    if (metaDescriptor->metaType == ZR_META_GET_ITEM) {
                        prototype->indexContract.getByIndexFunction = (SZrFunction *)metaValue.value.object;
                    } else if (metaDescriptor->metaType == ZR_META_SET_ITEM) {
                        prototype->indexContract.setByIndexFunction = (SZrFunction *)metaValue.value.object;
                    }

                    if (metaDescriptor->metaType == ZR_META_CONSTRUCTOR) {
                        constructorName = native_binding_create_string(state, "__constructor");
                        if (constructorName == ZR_NULL) {
                            continue;
                        }

                        ZrCore_Value_InitAsRawObject(state, &constructorKey, ZR_CAST_RAW_OBJECT_AS_SUPER(constructorName));
                        constructorKey.type = ZR_VALUE_TYPE_STRING;
                        ZrCore_Object_SetValue(state, &prototype->super, &constructorKey, &metaValue);
                    }
                }
            }
        }
    }
}
