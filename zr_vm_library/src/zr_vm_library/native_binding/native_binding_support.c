#include "native_binding/native_binding_internal.h"

#include "zr_vm_core/log.h"

/* 导入诊断由环境变量按进程读取一次，避免每个模块查询都反复访问环境。 */
TZrBool native_binding_trace_import_enabled(void) {
    /* TODO: 两个进程级缓存位非原子；需核查多线程首次导入是否可并发进入此处。 */
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_NATIVE_IMPORT");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

/* 读取 Native 闭包元数据前跟随 GC 转发地址，避免在旧对象头取描述符。 */
static ZR_FORCE_INLINE SZrRawObject *native_binding_refresh_forwarded_raw_object(SZrRawObject *rawObject) {
    SZrRawObject *forwardedObject;

    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    return forwardedObject != ZR_NULL ? forwardedObject : rawObject;
}

/* FFI 等调用方从已绑定闭包恢复返回类型，类型名仍由模块描述符持有。 */
const TZrChar *ZrLib_CallableValue_GetNativeBindingReturnTypeName(SZrState *state, const SZrTypeValue *callableValue) {
    SZrRawObject *rawObject;
    SZrClosureNative *closure;

    if (state == ZR_NULL || callableValue == ZR_NULL || callableValue->type != ZR_VALUE_TYPE_CLOSURE ||
        !callableValue->isNative || callableValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    rawObject = native_binding_refresh_forwarded_raw_object(callableValue->value.object);
    if (rawObject == ZR_NULL || rawObject->type != ZR_RAW_OBJECT_TYPE_CLOSURE) {
        return ZR_NULL;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, rawObject);
    if (closure == ZR_NULL || closure->nativeBindingDescriptor == ZR_NULL) {
        return ZR_NULL;
    }

    switch ((EZrLibResolvedBindingKind)closure->nativeBindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            return ((const ZrLibFunctionDescriptor *)closure->nativeBindingDescriptor)->returnTypeName;
        case ZR_LIB_RESOLVED_BINDING_METHOD:
            return ((const ZrLibMethodDescriptor *)closure->nativeBindingDescriptor)->returnTypeName;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            return ((const ZrLibMetaMethodDescriptor *)closure->nativeBindingDescriptor)->returnTypeName;
        default:
            return ZR_NULL;
    }
}

/* 统一函数、实例方法和元方法的接收者位置，让后续实参访问使用同一栈布局。
 * TODO: 首方调用点已改用 cached/inline 初始化器；需核实此内部入口是否仍需保留。 */
void native_binding_init_call_context_layout(ZrLibCallContext *context,
                                             TZrStackValuePointer functionBase,
                                             TZrSize rawArgumentCount) {
    TZrBool usesReceiver;

    if (context == ZR_NULL) {
        return;
    }

    usesReceiver = ZR_FALSE;
    if (context->methodDescriptor != ZR_NULL) {
        usesReceiver = !context->methodDescriptor->isStatic;
    } else if (context->metaMethodDescriptor != ZR_NULL) {
        usesReceiver = ZR_TRUE;
    }
    native_binding_init_call_context_layout_cached(context,
                                                   context->state,
                                                   functionBase,
                                                   rawArgumentCount,
                                                   usesReceiver);
}

/* 插件导入的可选诊断通道；不得改变加载成败，也不向用户结果注入信息。 */
void native_binding_trace_import(SZrState *state, const TZrChar *format, ...) {
    va_list arguments;
    va_list copyArguments;
    int requiredLength;
    TZrChar stackBuffer[512];
    TZrChar *heapBuffer = ZR_NULL;
    TZrNativeString message = stackBuffer;

    if (!native_binding_trace_import_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    va_copy(copyArguments, arguments);
    requiredLength = vsnprintf(stackBuffer, sizeof(stackBuffer), format, copyArguments);
    va_end(copyArguments);
    if (requiredLength < 0) {
        va_end(arguments);
        return;
    }
    if ((size_t)requiredLength >= sizeof(stackBuffer)) {
        heapBuffer = (TZrChar *)malloc((size_t)requiredLength + 1u);
        if (heapBuffer == ZR_NULL) {
            va_end(arguments);
            return;
        }
        vsnprintf(heapBuffer, (size_t)requiredLength + 1u, format, arguments);
        message = heapBuffer;
    }
    va_end(arguments);
    ZrCore_Log_Write(state, ZR_LOG_LEVEL_DEBUG, ZR_OUTPUT_CHANNEL_STDERR, ZR_OUTPUT_KIND_DIAGNOSTIC, message);
    if (heapBuffer != ZR_NULL) {
        free(heapBuffer);
    }
}

/* 成功注册或成功失效后撤销上一轮错误，供 CLI 获取当前操作诊断。 */
void native_registry_clear_error(ZrLibrary_NativeRegistryState *registry) {
    if (registry == ZR_NULL) {
        return;
    }

    registry->lastErrorCode = ZR_LIB_NATIVE_REGISTRY_ERROR_NONE;
    registry->lastErrorMessage[0] = '\0';
}

/* 统一记录插件和描述符校验错误；返回的消息只在下次注册表操作前有效。 */
void native_registry_set_error(ZrLibrary_NativeRegistryState *registry,
                                      EZrLibNativeRegistryErrorCode errorCode,
                                      const TZrChar *format,
                                      ...) {
    va_list arguments;

    if (registry == ZR_NULL) {
        return;
    }

    registry->lastErrorCode = errorCode;
    registry->lastErrorMessage[0] = '\0';
    if (format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(registry->lastErrorMessage, sizeof(registry->lastErrorMessage), format, arguments);
    va_end(arguments);
}

/* 来源路径匹配只做分隔符与 Windows 大小写归一，不解析文件系统别名。 */
static TZrChar native_registry_normalize_path_char(TZrChar value) {
    if (value == '\\') {
        value = '/';
    }
#if defined(ZR_PLATFORM_WIN)
    value = (TZrChar)tolower((unsigned char)value);
#endif
    return value;
}

/* 失效与记录查询共用来源路径判等，确保两条路径遵守相同平台规则。 */
TZrBool native_registry_source_paths_equal(const TZrChar *left, const TZrChar *right) {
    TZrSize index = 0;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        if (native_registry_normalize_path_char(left[index]) != native_registry_normalize_path_char(right[index])) {
            return ZR_FALSE;
        }
        index++;
    }

    return left[index] == '\0' && right[index] == '\0';
}

/* 全局卸载前查找仍被模块对象强持有的插件记录。 */
const ZrLibRegisteredModuleRecord *native_registry_find_live_descriptor_plugin_record(
        ZrLibrary_NativeRegistryState *registry) {
    TZrSize index;

    if (registry == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL && record->isDescriptorPlugin && record->ownerRefCount > 0u) {
            return record;
        }
    }

    return ZR_NULL;
}

/* 同名插件重载前只检查目标模块的强引用，以免替换存活闭包使用的描述符。 */
const ZrLibRegisteredModuleRecord *native_registry_find_live_descriptor_plugin_record_for_module(
        ZrLibrary_NativeRegistryState *registry,
        const TZrChar *moduleName) {
    const ZrLibRegisteredModuleRecord *record;

    if (registry == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    record = native_registry_find_record(registry, moduleName);
    if (record == ZR_NULL || !record->isDescriptorPlugin || record->ownerRefCount == 0u) {
        return ZR_NULL;
    }

    return record;
}

/* 为卸载和同名重载共用的拒绝路径保留模块、来源与引用数。 */
void native_registry_set_descriptor_plugin_in_use_error(ZrLibrary_NativeRegistryState *registry,
                                                        const ZrLibRegisteredModuleRecord *record,
                                                        const TZrChar *operation) {
    native_registry_set_error(registry,
                              ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE,
                              "cannot %s descriptor plugin module '%s' from '%s' while ownerRefCount=%u",
                              operation != ZR_NULL ? operation : "update",
                              record != ZR_NULL && record->moduleName != ZR_NULL ? record->moduleName : "<unknown>",
                              record != ZR_NULL && record->sourcePath != ZR_NULL ? record->sourcePath : "<unknown>",
                              record != ZR_NULL ? record->ownerRefCount : 0u);
}

/* 注册记录独立持有模块名和来源路径；调用方负责用同一 global allocator 释放。 */
TZrChar *native_registry_duplicate_string(SZrGlobalState *global, const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (global == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    length = strlen(text);
    copy = (TZrChar *)global->allocator(global->userAllocationArguments,
                                        ZR_NULL,
                                        0,
                                        length + 1,
                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

/* Native 类型错误使用稳定的公开类别名，不泄漏 VM 内部值编码。 */
const TZrChar *native_binding_value_type_name(SZrState *state, const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return "null";
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return "int";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return "function";
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return "nativePointer";
        case ZR_VALUE_TYPE_OBJECT: {
            ZR_UNUSED_PARAMETER(state);
            return "object";
        }
        default:
            return "value";
    }
}

/* 错误消息按当前描述符选取用户可见调用名，元方法由核心名称表表示。 */
const TZrChar *native_binding_call_name(const ZrLibCallContext *context) {
    if (context == ZR_NULL) {
        return "native";
    }
    if (context->functionDescriptor != ZR_NULL && context->functionDescriptor->name != ZR_NULL) {
        return context->functionDescriptor->name;
    }
    if (context->methodDescriptor != ZR_NULL && context->methodDescriptor->name != ZR_NULL) {
        return context->methodDescriptor->name;
    }
    if (context->metaMethodDescriptor != ZR_NULL &&
        context->metaMethodDescriptor->metaType < ZR_META_ENUM_MAX) {
        return CZrMetaName[context->metaMethodDescriptor->metaType];
    }
    return "native";
}

/* 多个物化/导入路径共用字符串缓存；返回 VM 对象而非持久 C 缓冲。 */
SZrString *native_binding_create_string(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_CreateTryHitCache(state, (TZrNativeString)text);
}

/* C 调用辅助入口按模块名复用 core 导入器，保留缓存及循环导入语义。 */
SZrObjectModule *native_binding_import_module(SZrState *state, const TZrChar *moduleName) {
    SZrString *moduleNameString;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNameString = native_binding_create_string(state, moduleName);
    if (moduleNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_ImportByPath(state, moduleNameString);
}

/* 物化原生类型后在全局 zr 对象暴露原型，使语言层能按类型名访问。 */
void native_binding_register_prototype_in_global_scope(SZrState *state,
                                                              SZrString *typeName,
                                                              const SZrTypeValue *prototypeValue) {
    SZrObject *zrObject;
    SZrTypeValue key;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL || prototypeValue == ZR_NULL) {
        return;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, zrObject, &key, prototypeValue);
}

/* 仅取回由 Attach 装入 global 的私有状态；并不隐式初始化注册表。 */
ZrLibrary_NativeRegistryState *native_registry_get(SZrGlobalState *global) {
    return global != ZR_NULL
                   ? (ZrLibrary_NativeRegistryState *)global->nativeRegistryState
                   : ZR_NULL;
}

/* 编译工具借用 VM 执行时切换阶段，恢复原阶段由调用方负责。 */
void ZrLibrary_State_SetProviderPhase(SZrState *state,
                                      EZrLibrary_ProviderPhase phase) {
    if (state == ZR_NULL) {
        return;
    }
    state->nativeProviderPhase = (TZrUInt8)phase;
}

/* 损坏或缺失阶段按运行时处理，确保普通导入不依赖编译工具环境。 */
EZrLibrary_ProviderPhase ZrLibrary_State_GetProviderPhase(const SZrState *state) {
    if (state == ZR_NULL || state->nativeProviderPhase > ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL) {
        return ZR_LIBRARY_PROVIDER_PHASE_RUNTIME;
    }
    return (EZrLibrary_ProviderPhase)state->nativeProviderPhase;
}

/* 阶段门禁允许普通运行时提供者，限制编译工具提供者的误用。 */
TZrBool ZrLibrary_ProviderPhase_CanConsume(
        EZrLibrary_ProviderPhase hostPhase,
        EZrLibrary_ProviderPhase providerPhase) {
    return providerPhase == ZR_LIBRARY_PROVIDER_PHASE_RUNTIME ||
           providerPhase == hostPhase;
}

/* 索引缓存的安全读取边界，避免旧 closure 缓存越过当前表长。 */
static ZrLibBindingEntry *native_registry_binding_entry_at(ZrLibrary_NativeRegistryState *registry, TZrSize index) {
    if (registry == ZR_NULL || !registry->bindingEntries.isValid ||
        index == ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_INVALID_INDEX ||
        index >= registry->bindingEntries.length) {
        return ZR_NULL;
    }

    return (ZrLibBindingEntry *)ZrCore_Array_Get(&registry->bindingEntries, index);
}

/* 将最近命中的绑定提升到两槽热点缓存，减少通用分派的线性查找。 */
static void native_registry_promote_binding_lookup_index(ZrLibrary_NativeRegistryState *registry, TZrSize index) {
    TZrSize currentHotIndex;

    if (registry == ZR_NULL) {
        return;
    }

    currentHotIndex = registry->bindingLookupHotIndices[0];
    if (currentHotIndex == index) {
        return;
    }

    registry->bindingLookupHotIndices[0] = index;
    registry->bindingLookupHotIndices[1] = currentHotIndex;
}

/* 按 closure 身份取描述符绑定；先验证闭包缓存，再尝试热点与完整表。 */
ZrLibBindingEntry *native_registry_find_binding(ZrLibrary_NativeRegistryState *registry,
                                                       SZrClosureNative *closure) {
    TZrSize cacheSlot;
    TZrSize index;
    ZrLibBindingEntry *entry;

    if (registry == ZR_NULL || closure == ZR_NULL || !registry->bindingEntries.isValid) {
        return ZR_NULL;
    }

    if (closure->nativeBindingLookupIndex != ZR_MAX_SIZE) {
        entry = native_registry_binding_entry_at(registry, closure->nativeBindingLookupIndex);
        if (entry != ZR_NULL && entry->closure == closure) {
            native_registry_promote_binding_lookup_index(registry, closure->nativeBindingLookupIndex);
            native_binding_closure_store_cached_binding(closure,
                                                        closure->nativeBindingLookupIndex,
                                                        entry->bindingKind,
                                                        entry->moduleDescriptor,
                                                        entry->typeDescriptor,
                                                        entry->ownerPrototype,
                                                        entry->bindingKind == ZR_LIB_RESOLVED_BINDING_FUNCTION
                                                                ? (const void *)entry->descriptor.functionDescriptor
                                                                : (entry->bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD
                                                                           ? (const void *)entry->descriptor.methodDescriptor
                                                                           : (const void *)entry->descriptor.metaMethodDescriptor));
            return entry;
        }

        native_binding_closure_invalidate_cached_lookup(closure);
    }

    for (cacheSlot = 0; cacheSlot < ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_CAPACITY; cacheSlot++) {
        TZrSize cachedIndex = registry->bindingLookupHotIndices[cacheSlot];
        entry = native_registry_binding_entry_at(registry, cachedIndex);
        if (entry == ZR_NULL) {
            registry->bindingLookupHotIndices[cacheSlot] = ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_INVALID_INDEX;
            continue;
        }
        if (entry->closure == closure) {
            native_registry_promote_binding_lookup_index(registry, cachedIndex);
            native_binding_closure_store_cached_binding(closure,
                                                        cachedIndex,
                                                        entry->bindingKind,
                                                        entry->moduleDescriptor,
                                                        entry->typeDescriptor,
                                                        entry->ownerPrototype,
                                                        entry->bindingKind == ZR_LIB_RESOLVED_BINDING_FUNCTION
                                                                ? (const void *)entry->descriptor.functionDescriptor
                                                                : (entry->bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD
                                                                           ? (const void *)entry->descriptor.methodDescriptor
                                                                           : (const void *)entry->descriptor.metaMethodDescriptor));
            return entry;
        }
    }

    for (index = 0; index < registry->bindingEntries.length; index++) {
        entry = (ZrLibBindingEntry *)ZrCore_Array_Get(&registry->bindingEntries, index);
        if (entry != ZR_NULL && entry->closure == closure) {
            native_registry_promote_binding_lookup_index(registry, index);
            native_binding_closure_store_cached_binding(closure,
                                                        index,
                                                        entry->bindingKind,
                                                        entry->moduleDescriptor,
                                                        entry->typeDescriptor,
                                                        entry->ownerPrototype,
                                                        entry->bindingKind == ZR_LIB_RESOLVED_BINDING_FUNCTION
                                                                ? (const void *)entry->descriptor.functionDescriptor
                                                                : (entry->bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD
                                                                           ? (const void *)entry->descriptor.methodDescriptor
                                                                           : (const void *)entry->descriptor.metaMethodDescriptor));
            return entry;
        }
    }

    return ZR_NULL;
}

/* 模块名是注册表的可见键；插件重载时返回当前世代记录。 */
const ZrLibRegisteredModuleRecord *native_registry_find_record(ZrLibrary_NativeRegistryState *registry,
                                                                      const TZrChar *moduleName) {
    TZrSize index;

    if (registry == ZR_NULL || moduleName == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL &&
            record->moduleName != ZR_NULL &&
            strcmp(record->moduleName, moduleName) == 0) {
            return record;
        }
    }

    return ZR_NULL;
}

/* 插件物化以实际描述符指针回查其注册记录和来源，避免名称重载混淆。 */
const ZrLibRegisteredModuleRecord *native_registry_find_record_by_descriptor(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *descriptor) {
    TZrSize index;

    if (registry == ZR_NULL || descriptor == ZR_NULL || !registry->moduleRecords.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        const ZrLibRegisteredModuleRecord *record =
                (const ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL && record->descriptor == descriptor) {
            return record;
        }
    }

    return ZR_NULL;
}

/* 注册时先阻止缺表或非法传递模式进入编译器与 ref/out 调用路径。 */
static TZrBool native_registry_validate_parameter_descriptors(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *module,
        const TZrChar *callableName,
        const ZrLibParameterDescriptor *parameters,
        TZrSize parameterCount) {
    if (parameterCount > 0u && parameters == ZR_NULL) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                "module '%s' callable '%s' declares %llu parameters without descriptors",
                module->moduleName != ZR_NULL ? module->moduleName : "<null>",
                callableName != ZR_NULL ? callableName : "<unnamed>",
                (unsigned long long)parameterCount);
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < parameterCount; index++) {
        if (parameters[index].passingMode < ZR_LIB_PARAMETER_PASSING_MODE_VALUE ||
            parameters[index].passingMode > ZR_LIB_PARAMETER_PASSING_MODE_REF) {
            native_registry_set_error(
                    registry,
                    ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                    "module '%s' callable '%s' parameter %llu uses invalid passing mode %d",
                    module->moduleName != ZR_NULL ? module->moduleName : "<null>",
                    callableName != ZR_NULL ? callableName : "<unnamed>",
                    (unsigned long long)index,
                    (int)parameters[index].passingMode);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

/* 遍历模块函数、方法和元方法的共享参数约束，统一拒绝畸形描述符。 */
static TZrBool native_registry_validate_parameter_passing_modes(
        ZrLibrary_NativeRegistryState *registry,
        const ZrLibModuleDescriptor *descriptor) {
    if (descriptor->functionCount > 0u && descriptor->functions == ZR_NULL) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                "module '%s' declares functions without descriptors",
                descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>");
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *function = &descriptor->functions[index];

        if (!native_registry_validate_parameter_descriptors(
                    registry,
                    descriptor,
                    function->name,
                    function->parameters,
                    function->parameterCount)) {
            return ZR_FALSE;
        }
    }

    if (descriptor->typeCount > 0u && descriptor->types == ZR_NULL) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                "module '%s' declares types without descriptors",
                descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>");
        return ZR_FALSE;
    }
    for (TZrSize typeIndex = 0u; typeIndex < descriptor->typeCount; typeIndex++) {
        const ZrLibTypeDescriptor *type = &descriptor->types[typeIndex];

        if (type->methodCount > 0u && type->methods == ZR_NULL) {
            native_registry_set_error(
                    registry,
                    ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                    "module '%s' type '%s' declares methods without descriptors",
                    descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                    type->name != ZR_NULL ? type->name : "<unnamed>");
            return ZR_FALSE;
        }
        for (TZrSize methodIndex = 0u; methodIndex < type->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *method = &type->methods[methodIndex];

            if (!native_registry_validate_parameter_descriptors(
                        registry,
                        descriptor,
                        method->name,
                        method->parameters,
                        method->parameterCount)) {
                return ZR_FALSE;
            }
        }

        if (type->metaMethodCount > 0u && type->metaMethods == ZR_NULL) {
            native_registry_set_error(
                    registry,
                    ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                    "module '%s' type '%s' declares meta methods without descriptors",
                    descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                    type->name != ZR_NULL ? type->name : "<unnamed>");
            return ZR_FALSE;
        }
        for (TZrSize methodIndex = 0u; methodIndex < type->metaMethodCount; methodIndex++) {
            const ZrLibMetaMethodDescriptor *method = &type->metaMethods[methodIndex];

            if (!native_registry_validate_parameter_descriptors(
                        registry,
                        descriptor,
                        "<meta-method>",
                        method->parameters,
                        method->parameterCount)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

/* 插件加载与直接注册共用 ABI、运行时能力及声明合法性门禁。 */
TZrBool native_registry_validate_descriptor_compatibility(ZrLibrary_NativeRegistryState *registry,
                                                           const ZrLibModuleDescriptor *descriptor) {
    TZrUInt32 minimumRuntimeAbi;

    if (descriptor == ZR_NULL) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_LOAD,
                                  "native descriptor is null");
        return ZR_FALSE;
    }

    if (descriptor->abiVersion != ZR_VM_NATIVE_PLUGIN_ABI_VERSION) {
        native_registry_set_error(
                registry,
                ZR_LIB_NATIVE_REGISTRY_ERROR_ABI_MISMATCH,
                "module '%s' uses ABI %u but runtime expects %u",
                descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                descriptor->abiVersion,
                ZR_VM_NATIVE_PLUGIN_ABI_VERSION);
        return ZR_FALSE;
    }

    minimumRuntimeAbi = descriptor->minRuntimeAbi != 0 ? descriptor->minRuntimeAbi : ZR_VM_NATIVE_RUNTIME_ABI_VERSION;
    if (minimumRuntimeAbi > ZR_VM_NATIVE_RUNTIME_ABI_VERSION) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_VERSION_MISMATCH,
                                  "module '%s' requires runtime ABI %u but runtime provides %u",
                                  descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                                  minimumRuntimeAbi,
                                  ZR_VM_NATIVE_RUNTIME_ABI_VERSION);
        return ZR_FALSE;
    }

    if ((descriptor->requiredCapabilities & ~ZR_VM_NATIVE_RUNTIME_CAPABILITIES) != 0) {
        native_registry_set_error(registry,
                                  ZR_LIB_NATIVE_REGISTRY_ERROR_CAPABILITY_MISMATCH,
                                  "module '%s' requires unsupported capabilities 0x%llx",
                                  descriptor->moduleName != ZR_NULL ? descriptor->moduleName : "<null>",
                                  (unsigned long long)(descriptor->requiredCapabilities & ~ZR_VM_NATIVE_RUNTIME_CAPABILITIES));
        return ZR_FALSE;
    }

    /* BUG: 此处未校验 constants/moduleLinks/typeHints 及类型 fields、枚举成员、
     * 继承名和泛型等数组与计数配对；非零计数配 NULL 可注册成功，后续运行时元数据
     * 或反射/类型物化仍按下标读取这些空指针。用畸形描述符逐组核验。 */
    if (!native_registry_validate_parameter_passing_modes(registry, descriptor)) {
        return ZR_FALSE;
    }

    if (!native_registry_validate_canonical_type_roles(registry, descriptor)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 注册表借用描述符但复制定位字符串；调用者须保证插件句柄覆盖整个借用期。 */
TZrBool native_registry_register_module_record(SZrGlobalState *global,
                                                      const ZrLibModuleDescriptor *descriptor,
                                                      EZrLibNativeModuleRegistrationKind registrationKind,
                                                      const TZrChar *sourcePath,
                                                      TZrBool isDescriptorPlugin) {
    ZrLibrary_NativeRegistryState *registry;
    TZrSize index;

    if (global == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        return ZR_FALSE;
    }

    registry = native_registry_get(global);
    if (registry == ZR_NULL ||
        !native_registry_validate_official_descriptor(registry, descriptor) ||
        !native_registry_validate_descriptor_compatibility(registry, descriptor)) {
        return ZR_FALSE;
    }

    for (index = 0; index < registry->moduleRecords.length; index++) {
        ZrLibRegisteredModuleRecord *record =
                (ZrLibRegisteredModuleRecord *)ZrCore_Array_Get(&registry->moduleRecords, index);
        if (record != ZR_NULL && record->descriptor != ZR_NULL &&
            record->descriptor != descriptor &&
            descriptor->providerContractRole != ZR_PROVIDER_CONTRACT_ROLE_NONE &&
            record->descriptor->providerContractRole == descriptor->providerContractRole) {
            native_registry_set_error(
                    registry,
                    ZR_LIB_NATIVE_REGISTRY_ERROR_DUPLICATE_PROVIDER_CONTRACT,
                    "provider contract role %u is already owned by module '%s'",
                    (unsigned)descriptor->providerContractRole,
                    record->descriptor->moduleName);
            return ZR_FALSE;
        }
        if (record != ZR_NULL &&
            record->moduleName != ZR_NULL &&
            strcmp(record->moduleName, descriptor->moduleName) == 0) {
            if (!native_registry_validate_official_duplicate(
                        registry, record->descriptor, descriptor)) {
                return ZR_FALSE;
            }
            TZrChar *moduleNameCopy = native_registry_duplicate_string(global, descriptor->moduleName);
            TZrChar *sourcePathCopy = native_registry_duplicate_string(global, sourcePath);

            if (moduleNameCopy == ZR_NULL || sourcePathCopy == ZR_NULL) {
                if (moduleNameCopy != ZR_NULL) {
                    global->allocator(global->userAllocationArguments,
                                      moduleNameCopy,
                                      strlen(moduleNameCopy) + 1,
                                      0,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                }
                if (sourcePathCopy != ZR_NULL) {
                    global->allocator(global->userAllocationArguments,
                                      sourcePathCopy,
                                      strlen(sourcePathCopy) + 1,
                                      0,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                }
                return ZR_FALSE;
            }

            if (record->moduleName != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->moduleName,
                                  strlen(record->moduleName) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            if (record->sourcePath != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record->sourcePath,
                                  strlen(record->sourcePath) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            /* 同名重注册改变提供者世代；保留旧闭包的 GC 根，但撤销直接分派和签名缓存，
             * 下一次调用必须重新解析到新描述符。 */
            if (record->descriptor != descriptor && registry->bindingEntries.isValid) {
                for (TZrSize bindingIndex = 0u; bindingIndex < registry->bindingEntries.length; ++bindingIndex) {
                    ZrLibBindingEntry *binding = (ZrLibBindingEntry *)ZrCore_Array_Get(
                            &registry->bindingEntries, bindingIndex);
                    if (binding != ZR_NULL && binding->moduleDescriptor == record->descriptor &&
                        binding->closure != ZR_NULL) {
                        binding->closure->nativeBindingDirectDispatch.callback = ZR_NULL;
                        binding->closure->nativeBindingLookupIndex = ZR_MAX_SIZE;
                        if (++binding->closure->callBindingGeneration == 0u)
                            ++binding->closure->callBindingGeneration;
                        binding->moduleSignatureHash = 0u;
                    }
                }
            }
            /* BUG: 插件加载器在这里提交记录后才分配 handleRecord 字符串；
             * 任一分配失败会卸载动态库却保留本记录，后续 FindModule 得到悬垂描述符。
             * 证据：native_binding_registry_plugin.c 中注册后构造句柄及失败释放路径。 */
            record->descriptor = descriptor;
            record->moduleName = moduleNameCopy;
            record->registrationKind = registrationKind;
            record->isDescriptorPlugin = isDescriptorPlugin;
            record->sourcePath = sourcePathCopy;
            native_registry_clear_error(registry);
            return ZR_TRUE;
        }
    }

    {
        ZrLibRegisteredModuleRecord record;
        memset(&record, 0, sizeof(record));
        record.descriptor = descriptor;
        record.moduleName = native_registry_duplicate_string(global, descriptor->moduleName);
        record.registrationKind = registrationKind;
        record.isDescriptorPlugin = isDescriptorPlugin;
        record.sourcePath = native_registry_duplicate_string(global, sourcePath);
        if (record.moduleName == ZR_NULL || record.sourcePath == ZR_NULL) {
            if (record.moduleName != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record.moduleName,
                                  strlen(record.moduleName) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            if (record.sourcePath != ZR_NULL) {
                global->allocator(global->userAllocationArguments,
                                  record.sourcePath,
                                  strlen(record.sourcePath) + 1,
                                  0,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            return ZR_FALSE;
        }
        /* BUG: 新插件记录也先于句柄元数据提交；后续分配失败会留下已卸载库的描述符。 */
        /* BUG: moduleRecords 容量满且分配器返回 NULL 时，Array_Push 会覆盖 head
         * 并在后续 RawCopy 解引用空指针；注册入口无法报告内存不足。 */
        ZrCore_Array_Push(global->mainThreadState, &registry->moduleRecords, &record);
    }

    native_registry_clear_error(registry);
    return ZR_TRUE;
}
