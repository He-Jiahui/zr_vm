#include "native_binding/native_binding_internal.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/reflection.h"

/* 构造反射对象期间的临时 GC 根；只撤销本层新增的忽略标记。 */
typedef struct ZrNativeMetadataPin {
    SZrRawObject *object;
    TZrBool addedByCaller;
} ZrNativeMetadataPin;

/* 保护尚未挂到父对象的新对象，避免后续字段或子数组分配触发回收。 */
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

/* 以普通对象形式进入临时根协议，供所有元数据构造器共用。 */
static TZrBool native_metadata_pin_object(SZrState *state, SZrObject *object, ZrNativeMetadataPin *pin) {
    if (object == ZR_NULL || pin == ZR_NULL) {
        return ZR_FALSE;
    }

    return native_metadata_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), pin);
}

/* 保留调用方原先的根状态，构造器只清理自己取得的保护。 */
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

/* 为 ModuleInfo 及子条目统一写入动态字段；调用方目前无法观察写入结果。 */
/* BUG: 字段写入因固定对象或键创建失败可静默返回；make_module_info 仍导出
 * 缺字段对象，反射读取该字段时会得到空值。见 dispatch.c 的 SetFieldCString。 */
void native_metadata_set_value_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            const SZrTypeValue *value) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrLib_Object_SetFieldCString(state, object, fieldName, value);
}

/* 字符串创建会分配 VM 对象，因此先固定目标对象，再交给通用字段写入器。 */
/* BUG: SetString 分配失败会保持 fieldValue 未初始化，随后仍送入 SetFieldCString；
 * 反射字段写入可读取未定义值。 */
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

/* 未声明的 FFI 扩展字段保持缺席，供反射消费者区分默认值与显式值。 */
static void native_metadata_set_optional_string_field(SZrState *state,
                                                      SZrObject *object,
                                                      const TZrChar *fieldName,
                                                      const TZrChar *value) {
    if (value == ZR_NULL || value[0] == '\0') {
        return;
    }

    native_metadata_set_string_field(state, object, fieldName, value);
}

/* 整数描述符字段经通用 VM 值接口公开给 ModuleInfo 消费者。 */
void native_metadata_set_int_field(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

/* 浮点常量元数据保留数值类别，避免字符串化后改变反射结果。 */
void native_metadata_set_float_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            TZrFloat64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetFloat(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

/* 布尔契约字段保留 VM 布尔类型，供反射消费者直接判断。 */
void native_metadata_set_bool_field(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrBool value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    native_metadata_set_value_field(state, object, fieldName, &fieldValue);
}

/* 常量未声明类型名时按种类推导，运行时签名与 ModuleInfo 须复用同一映射。 */
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

/* 构造许可取描述符权威位，供可见元数据和原型隐藏字段共用。 */
TZrBool native_descriptor_allows_value_construction(const ZrLibTypeDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }
    return descriptor->allowValueConstruction;
}

/* 装箱构造许可与值构造许可独立，不能从原型类别推测。 */
TZrBool native_descriptor_allows_boxed_construction(const ZrLibTypeDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }
    return descriptor->allowBoxedConstruction;
}

/* 常量和枚举成员共用值投影；只公开与 kind 对应的载荷字段。 */
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

/* 把描述符字符串提升为 VM 数组元素，返回值表示追加是否成功。 */
/* BUG: SetString 失败时 entryValue 未初始化，PushValue 可能读取未定义值；
 * 返回布尔值不能覆盖这条字符串构造失败路径。 */
TZrBool native_metadata_push_string_value(SZrState *state, SZrObject *array, const TZrChar *value) {
    SZrTypeValue entryValue;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(state, &entryValue, value);
    return ZrLib_Array_PushValue(state, array, &entryValue);
}

/* 将可选描述符字符串序列交给 ModuleInfo；空项不出现在结果中。 */
/* BUG: PushValue 失败时仍返回数组；例如固定元素失败后，implements/constraints
 * 等反射序列会短于描述符，调用方无法从返回值识别截断。 */
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

/* 字段条目同时承载类型名、契约角色和可写性，供反射构造成员视图。 */
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
    native_metadata_set_bool_field(state, object, "runtimeOnly", descriptor->runtimeOnly);
    native_metadata_set_bool_field(state, object, "isReadonly", descriptor->isReadonly);
    return object;
}

/* 参数条目保留 passingMode，避免反射把 ref/out 误认为普通值参数。 */
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
    native_metadata_set_int_field(state, object, "passingMode", (TZrInt64)descriptor->passingMode);
    return object;
}

/* 参数顺序来自声明；供方法与函数元数据使用同一反射布局。 */
/* BUG: 条目创建或追加失败后继续循环并返回短数组，外层仍公布原 parameterCount。 */
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

/* 泛型参数及其约束作为独立对象，构造期间须固定两个尚未链接的对象。 */
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

/* 泛型参数数组供类型、方法和函数的 ModuleInfo 条目共用。 */
/* BUG: 子条目或 PushValue 失败时仍返回不完整数组；反射签名遗漏约束。 */
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

/* 类型提示独立于实际导出，供工具显示补充签名和说明。 */
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

/* 按描述符顺序保留工具侧提示，与公共契约的 symbolName 对齐。 */
/* BUG: 条目或数组追加失败时仍返回成功，ModuleInfo.typeHints 会静默丢项。 */
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

/* 方法条目把调用元数、接收者约束、属性桥接和泛型形参投影到 ModuleInfo。 */
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

    /* 没有显式参数描述符且元数非零时，保留“参数信息未知”而非空签名。 */
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
    native_metadata_set_bool_field(state, object, "isStatic", descriptor->isStatic);
    native_metadata_set_bool_field(
            state,
            object,
            "isReadonlyReceiver",
            !descriptor->isStatic &&
                    (descriptor->dispatchFlags &
                     ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER) != 0u);
    native_metadata_set_optional_string_field(
            state, object, "propertyName", descriptor->propertyName);
    native_metadata_set_int_field(
            state,
            object,
            "propertyReferenceAccess",
            (TZrInt64)descriptor->propertyReferenceAccess);
    native_metadata_set_bool_field(
            state,
            object,
            "propertyExportsWritableRef",
            descriptor->propertyExportsWritableRef);
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

/* 元方法借用与普通方法相同的参数模型，名称由 VM metaType 表决定。 */
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
    native_metadata_set_bool_field(
            state,
            object,
            "isReadonlyReceiver",
            descriptor->metaType != ZR_META_CONSTRUCTOR &&
                    (descriptor->dispatchFlags &
                     ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER) != 0u);
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

/* 模块函数导出的反射条目；未知参数详情时仍保留可调用元数边界。 */
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

/* 常量条目与真实导出共享类型推导，供反射按名称与值类别配对。 */
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

/* 枚举成员的反射视图保存声明值与说明，不持有运行时枚举实例。 */
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

/* 模块链接的名字与目标模块名均留给反射，物化时另行解析实际模块对象。 */
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

/* 类型反射条目聚合成员、协议、枚举和 FFI 扩展契约；各数组在挂接前固定。 */
/* BUG: 子条目创建或 PushValue 失败后仍返回对象，ModuleInfo.types 可暴露
 * 缺成员的类型描述；反射按该数组构造声明视图。 */
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

/* 生成原生模块的公开契约快照；物化器将其作为 moduleInfo 导出供反射读取。 */
/* BUG: functions/constants/types/modules 任一子条目或追加失败后仍返回对象，
 * 反射消费者会看到少于已注册导出的条目。 */
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

    /* 版本与来源字段约定了工具和反射对这份对象的解释方式。 */
    native_metadata_set_int_field(state, object, "version", ZR_NATIVE_MODULE_INFO_VERSION);
    native_metadata_set_string_field(state, object, "moduleName", descriptor->moduleName);
    native_metadata_set_string_field(state, object, "typeHintsJson", descriptor->typeHintsJson);
    native_metadata_set_string_field(state, object, "moduleVersion", descriptor->moduleVersion);
    native_metadata_set_int_field(state,
                                  object,
                                  "runtimeAbiVersion",
                                  descriptor->minRuntimeAbi != 0 ? descriptor->minRuntimeAbi : ZR_VM_NATIVE_RUNTIME_ABI_VERSION);
    native_metadata_set_int_field(state, object, "requiredCapabilities", (TZrInt64)descriptor->requiredCapabilities);
    native_metadata_set_int_field(state, object, "providerPhase", (TZrInt64)descriptor->providerPhase);
    native_metadata_set_string_field(state, object, "publicContractHash", descriptor->publicContractHash);
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

/* 在模块物化阶段把常量描述符转成真实导出值，与 ModuleInfo 的常量条目对应。 */
/* BUG: AddPubExport 在导出表扩容失败时静默返回，本函数仍报告成功；
 * 物化后的模块可能缺常量，而 ModuleInfo 已列出该常量。字符串常量创建
 * 失败时 SetString 还会留下未初始化的 value，随后被送入 AddPubExport。 */
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

/* 先生成原生回调绑定，再将 VM 函数作为模块的公开导出。 */
/* BUG: AddPubExport 无成功状态；导出表扩容失败后仍返回成功，函数不可见。 */
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

/* 链接只指向已装载模块；解析失败应阻止当前模块物化。 */
/* BUG: AddPubExport 静默失败时模块仍物化，但链接名不能从导出表解析。 */
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

/* 把声明的协议位落实到原型，使运行时协议查询与描述符一致。 */
static void native_registry_add_protocol_mask(SZrObjectPrototype *prototype, TZrUInt64 protocolMask) {
    if (prototype == ZR_NULL || protocolMask == 0) {
        return;
    }

    for (EZrProtocolId protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
         protocolId <= ZR_PROTOCOL_ID_MAX;
         protocolId = (EZrProtocolId)(protocolId + 1)) {
        if ((protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0) {
            ZrCore_ObjectPrototype_AddProtocol(prototype, protocolId);
        }
    }
}

/* 类型注册仅接受描述符显式声明的协议，避免由方法名称推断语义。 */
static void native_registry_add_declared_protocols(SZrObjectPrototype *prototype,
                                                   const ZrLibTypeDescriptor *typeDescriptor) {
    if (prototype == ZR_NULL || typeDescriptor == ZR_NULL) {
        return;
    }
    native_registry_add_protocol_mask(prototype, typeDescriptor->protocolMask);
}

/* 系统异常类型复用全局预建原型，保持 Error/StackFrame 的 VM 身份稳定。 */
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

/* 字段契约写入原型成员表，迭代器 current 字段同时绑定标准协议槽位。 */
/* BUG: 创建字段名或 AddMemberDescriptor 分配失败时跳过字段；add_type
 * 仍成功并导出缺成员描述的类型，运行时成员访问可能与声明不一致。 */
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
        descriptor.isWritable = fieldDescriptor->isReadonly ? ZR_FALSE : ZR_TRUE;
        ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor);

        if (fieldDescriptor->contractRole == ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD) {
            prototype->iteratorContract.currentMemberName = fieldName;
        }
    }
}

/* 把显式角色方法挂到 iterable/iterator 快速槽位，不依赖字符串方法名。 */
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

/* 注册原型方法和元方法，并同步成员描述、属性访问和标准协议槽位。 */
/* BUG: AddMemberDescriptor 的失败返回值和 AddMeta 的分配失败都未传播；
 * 模块可发布有调用值但缺成员描述或元方法槽位的原型。 */
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

        /* 成员描述用于反射与访问控制，运行时调用值另入原型字典。 */
        {
            SZrMemberDescriptor descriptor;
            ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
            descriptor.name = methodName;
            descriptor.kind = methodDescriptor->isStatic ? ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER
                                                         : ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
            descriptor.isStatic = methodDescriptor->isStatic;
            descriptor.isWritable = ZR_FALSE;
            descriptor.contractRole = methodDescriptor->contractRole;
            descriptor.methodFunction = ZR_CAST(SZrFunction *, methodValue.value.object);
            ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor);
        }

        /* 属性 getter 与普通方法共用回调，但公开为独立成员契约。 */
        if (methodDescriptor->propertyName != ZR_NULL &&
            methodDescriptor->propertyReferenceAccess !=
                    ZR_LIB_REFERENCE_ACCESS_NONE) {
            SZrMemberDescriptor descriptor;
            SZrString *propertyName = native_binding_create_string(
                    state, methodDescriptor->propertyName);

            if (propertyName == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
            descriptor.name = propertyName;
            descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY;
            descriptor.isStatic = methodDescriptor->isStatic;
            descriptor.isWritable = ZR_FALSE;
            descriptor.getterFunction =
                    ZR_CAST(SZrFunction *, methodValue.value.object);
            descriptor.contractRole = methodDescriptor->contractRole;
            descriptor.receiverEffect =
                    methodDescriptor->propertyReferenceAccess ==
                                    ZR_LIB_REFERENCE_ACCESS_WRITABLE
                            ? ZR_MEMBER_RECEIVER_EFFECT_MUTABLE
                            : ZR_MEMBER_RECEIVER_EFFECT_READONLY;
            descriptor.referenceAccess =
                    (TZrUInt32)methodDescriptor->propertyReferenceAccess;
            descriptor.exportsWritableRef =
                    methodDescriptor->propertyExportsWritableRef;
            descriptor.accessModifier = 0u;
            descriptor.getterAccessModifier = 0u;
            descriptor.setterAccessModifier =
                    ZR_MEMBER_ACCESS_MODIFIER_UNAVAILABLE;
            descriptor.initializerAccessModifier =
                    ZR_MEMBER_ACCESS_MODIFIER_UNAVAILABLE;
            ZrCore_ObjectPrototype_AddMemberDescriptor(
                    state, prototype, &descriptor);
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

/* 原型隐藏字段保留 FFI 降级与枚举信息，供运行时反射/interop 查询。 */
/* BUG: SetString 失败后 fieldValue 未初始化，仍被传给 SetFieldCString。 */
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

/* 原型隐藏布尔位与可见 ModuleInfo 使用相同描述符来源。 */
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

/* 枚举声明值先规范为 VM 标量，再包装为带原型身份的成员实例。 */
/* BUG: 字符串成员创建失败时 SetString 不写 value，本函数仍返回成功；
 * 后续 make_enum_instance 将未初始化底层值写入枚举对象。 */
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

/* 运行时枚举成员携带底层值和声明名，原型决定其类型身份。 */
/* BUG: memberName 的 SetString 失败会留下未初始化 nameValue，随后写入对象。 */
/* TODO: enumObject 在创建后未显式固定；需结合 GC 栈根扫描与故障注入
 * 验证连续 SetFieldCString 分配期间该对象的存活性。 */
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

/* 将构造、FFI 与枚举契约复制到真实原型，供不经 ModuleInfo 的运行时路径查询。 */
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

/* 枚举成员以同一原型的对象导出，普通类型无需执行此路径。 */
/* BUG: SetFieldCString 可能静默失败，本函数仍返回成功，枚举成员缺席。 */
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

/* 优先复用同模块或预建异常原型，再补齐元数据、方法与模块导出。 */
/* BUG: AddPubExport 可能静默失败，本函数仍返回成功；后续同模块父类
 * 查找也会因类型不在公开导出表而失败。 */
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

    /* 结构体字段索引属于其专用原型；普通类型改由 ObjectPrototype 承载。 */
    if (prototype == ZR_NULL && typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        prototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, typeName);
        native_binding_trace_import(state,
                                    "[zr_native_import] add_type created_struct module=%s type=%s prototype=%p\n",
                                    moduleDescriptor->moduleName != ZR_NULL ? moduleDescriptor->moduleName : "<null>",
                                    typeDescriptor->name,
                                    (void *)prototype);
        if (prototype != ZR_NULL) {
            SZrStructPrototype *structPrototype = (SZrStructPrototype *)prototype;
            /* BUG: 字段名创建或 AddField 内部分配失败会静默跳过映射，
             * 物化仍继续，struct 的 keyOffsetMap 可少于描述符字段表。 */
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

/* 只从当前模块的公开导出取原型，防止同名全局类型混入本模块关系解析。 */
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

/* 同模块限定名需还原为本地导出名，泛型实参不参与原型导出键。 */
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

/* 全部类型导出后再连继承边；前向声明的同模块父类此时才可查找。 */
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
        /* BUG: class 显式声明的 extendsTypeName 无法解析时（zr.builtin.Object 自身除外），
         * 仍尝试回退到 Object 并继续物化；回退成功后实际父类与 ModuleInfo 声明不一致。 */
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

    /* BUG: 修复路径的回调创建失败被忽略，AddMeta 的分配失败也无返回值；
     * 模块可在声明元方法缺席时完成物化。
     *
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
