//
// Split from execution.c: object/prototype helpers.
//

#include "execution_internal.h"

static const TZrChar *kNativeEnumValueFieldName = "__zr_enumValue";
static const TZrChar *kNativeEnumNameFieldName = "__zr_enumName";
static const TZrChar *kNativeEnumValueTypeFieldName = "__zr_enumValueTypeName";

static TZrBool prototype_type_matches(EZrObjectPrototypeType expectedType, EZrObjectPrototypeType actualType) {
    return expectedType == ZR_OBJECT_PROTOTYPE_TYPE_INVALID || expectedType == actualType;
}

static SZrFunction *execution_find_entry_function(SZrState *state,
                                                  SZrClosure *currentClosure,
                                                  SZrCallInfo *currentCallInfo) {
    if (currentClosure != ZR_NULL &&
        currentClosure->function != ZR_NULL &&
        currentClosure->function->prototypeData != ZR_NULL &&
        currentClosure->function->prototypeCount > 0) {
        return currentClosure->function;
    }

    while (state != ZR_NULL && currentCallInfo != ZR_NULL) {
        if (ZR_CALL_INFO_IS_VM(currentCallInfo) &&
            currentCallInfo->functionBase.valuePointer >= state->stackBase.valuePointer &&
            currentCallInfo->functionBase.valuePointer < state->stackTop.valuePointer) {
            SZrTypeValue *functionBaseValue = ZrCore_Stack_GetValue(currentCallInfo->functionBase.valuePointer);
            if (functionBaseValue != ZR_NULL && functionBaseValue->type == ZR_VALUE_TYPE_CLOSURE) {
                SZrClosure *stackClosure = ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object);
                if (stackClosure != ZR_NULL &&
                    stackClosure->function != ZR_NULL &&
                    stackClosure->function->prototypeData != ZR_NULL &&
                    stackClosure->function->prototypeCount > 0) {
                    return stackClosure->function;
                }
            }
        }
        currentCallInfo = currentCallInfo->previous;
    }

    return ZR_NULL;
}

TZrBool execution_try_materialize_global_prototypes(SZrState *state,
                                                    SZrClosure *currentClosure,
                                                    SZrCallInfo *currentCallInfo,
                                                    const SZrTypeValue *tableValue,
                                                    const SZrTypeValue *keyValue) {
    SZrObject *globalObject;
    SZrObject *tableObject;
    SZrFunction *entryFunction;

    if (state == ZR_NULL || state->global == ZR_NULL || tableValue == ZR_NULL || keyValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (tableValue->type != ZR_VALUE_TYPE_OBJECT || keyValue->type != ZR_VALUE_TYPE_STRING) {
        return ZR_FALSE;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_FALSE;
    }

    globalObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    tableObject = ZR_CAST_OBJECT(state, tableValue->value.object);
    if (globalObject == ZR_NULL || tableObject == ZR_NULL || globalObject != tableObject) {
        return ZR_FALSE;
    }

    entryFunction = execution_find_entry_function(state, currentClosure, currentCallInfo);
    if (entryFunction == ZR_NULL ||
        entryFunction->prototypeData == ZR_NULL ||
        entryFunction->prototypeDataLength == 0 ||
        entryFunction->prototypeCount == 0) {
        return ZR_FALSE;
    }

    ZrCore_Module_CreatePrototypesFromData(state, ZR_NULL, entryFunction);
    return ZR_TRUE;
}

// 辅助函数：从模块中查找类型原型
// 返回找到的原型对象，如果未找到返回 ZR_NULL
static SZrObjectPrototype *find_prototype_in_module(SZrState *state, struct SZrObjectModule *module, 
                                                     SZrString *typeName, EZrObjectPrototypeType expectedType) {
    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从模块的 pub 导出中查找类型
    const SZrTypeValue *typeValue = ZrCore_Module_GetPubExport(state, module, typeName);
    if (typeValue == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 检查值类型是否为对象
    if (typeValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    
    SZrObject *typeObject = ZR_CAST_OBJECT(state, typeValue->value.object);
    if (typeObject == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 检查对象是否为原型对象
    if (typeObject->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }
    
    SZrObjectPrototype *prototype = (SZrObjectPrototype *)typeObject;
    
    // 检查原型类型是否匹配
    if (prototype_type_matches(expectedType, prototype->type)) {
        return prototype;
    }
    
    return ZR_NULL;
}

// 辅助函数：解析类型名称，支持 "module.TypeName" 格式
// 返回解析后的模块名和类型名，如果类型名不包含模块路径，moduleName 返回 ZR_NULL
static void parse_type_name(SZrState *state, SZrString *fullTypeName, SZrString **moduleName, SZrString **typeName) {
    if (state == ZR_NULL || fullTypeName == ZR_NULL) {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = ZR_NULL;
        return;
    }
    
    // 获取类型名称字符串
    TZrNativeString typeNameStr;
    TZrSize nameLen;
    if (fullTypeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        typeNameStr = (TZrNativeString)ZrCore_String_GetNativeStringShort(fullTypeName);
        nameLen = fullTypeName->shortStringLength;
    } else {
        typeNameStr = (TZrNativeString)ZrCore_String_GetNativeString(fullTypeName);
        nameLen = fullTypeName->longStringLength;
    }
    
    if (typeNameStr == ZR_NULL || nameLen == 0) {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
        return;
    }
    
    // 查找 '.' 分隔符
    const TZrChar *dotPos = (const TZrChar *)memchr(typeNameStr, '.', nameLen);
    if (dotPos == ZR_NULL) {
        // 没有模块路径，类型名就是完整名称
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
        return;
    }
    
    // 解析模块名和类型名
    TZrSize moduleNameLen = (TZrSize)(dotPos - typeNameStr);
    TZrSize typeNameLen = nameLen - moduleNameLen - 1;
    const TZrChar *typeNameStart = dotPos + 1;
    
    if (moduleNameLen > 0 && typeNameLen > 0) {
        if (moduleName != ZR_NULL) {
            *moduleName = ZrCore_String_Create(state, typeNameStr, moduleNameLen);
        }
        if (typeName != ZR_NULL) {
            *typeName = ZrCore_String_Create(state, (TZrNativeString)typeNameStart, typeNameLen);
        }
    } else {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
    }
}

static SZrString *extract_open_generic_base_type_name(SZrState *state, SZrString *typeName) {
    TZrNativeString typeNameStr;
    TZrSize typeNameLen;
    const TZrChar *genericStart;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        typeNameStr = (TZrNativeString)ZrCore_String_GetNativeStringShort(typeName);
        typeNameLen = typeName->shortStringLength;
    } else {
        typeNameStr = (TZrNativeString)ZrCore_String_GetNativeString(typeName);
        typeNameLen = typeName->longStringLength;
    }

    if (typeNameStr == ZR_NULL || typeNameLen == 0) {
        return ZR_NULL;
    }

    genericStart = (const TZrChar *)memchr(typeNameStr, '<', typeNameLen);
    if (genericStart == ZR_NULL || genericStart == typeNameStr) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, typeNameStr, (TZrSize)(genericStart - typeNameStr));
}

static SZrObjectPrototype *find_prototype_in_loaded_modules_registry(SZrState *state,
                                                                     SZrString *typeName,
                                                                     EZrObjectPrototypeType expectedType) {
    SZrObject *registry;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (!ZrCore_Value_IsGarbageCollectable(&state->global->loadedModulesRegistry) ||
        state->global->loadedModulesRegistry.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    registry = ZR_CAST_OBJECT(state, state->global->loadedModulesRegistry.value.object);
    if (registry == ZR_NULL || !registry->nodeMap.isValid || registry->nodeMap.buckets == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                if (cachedObject != ZR_NULL &&
                    cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                    struct SZrObjectModule *module = (struct SZrObjectModule *)cachedObject;
                    SZrObjectPrototype *prototype = find_prototype_in_module(state, module, typeName, expectedType);
                    if (prototype != ZR_NULL) {
                        return prototype;
                    }
                }
            }
            pair = pair->next;
        }
    }

    return ZR_NULL;
}

static SZrObjectPrototype *find_prototype_in_global_zr_object(SZrState *state,
                                                              SZrString *typeName,
                                                              EZrObjectPrototypeType expectedType) {
    SZrObject *zrObject;
    SZrTypeValue key;
    const SZrTypeValue *prototypeValue;
    SZrObject *candidate;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL ||
        state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (zrObject == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
    key.type = ZR_VALUE_TYPE_STRING;
    prototypeValue = ZrCore_Object_GetValue(state, zrObject, &key);
    if (prototypeValue == ZR_NULL || prototypeValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    candidate = ZR_CAST_OBJECT(state, prototypeValue->value.object);
    if (candidate == ZR_NULL || candidate->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    prototype = (SZrObjectPrototype *)candidate;
    return prototype_type_matches(expectedType, prototype->type) ? prototype : ZR_NULL;
}

static TZrBool materialize_entry_function_prototypes_for_lookup(SZrState *state, SZrString *moduleName) {
    SZrFunction *entryFunction;
    struct SZrObjectModule *module = ZR_NULL;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    entryFunction = execution_find_entry_function(state, ZR_NULL, state->callInfoList);
    if (entryFunction == ZR_NULL ||
        entryFunction->prototypeData == ZR_NULL ||
        entryFunction->prototypeDataLength == 0 ||
        entryFunction->prototypeCount == 0) {
        return ZR_FALSE;
    }

    if (moduleName != ZR_NULL) {
        module = ZrCore_Module_GetFromCache(state, moduleName);
    }

    ZrCore_Module_CreatePrototypesFromData(state, module, entryFunction);
    return ZR_TRUE;
}


SZrObjectPrototype *find_type_prototype(SZrState *state,
                                        SZrString *typeName,
                                        EZrObjectPrototypeType expectedType) {
    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析类型名称，支持 "module.TypeName" 格式
    SZrString *moduleName = ZR_NULL;
    SZrString *actualTypeName = ZR_NULL;
    parse_type_name(state, typeName, &moduleName, &actualTypeName);
    
    if (actualTypeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果指定了模块名，从该模块中查找
    if (moduleName != ZR_NULL) {
        // 从模块注册表中获取模块
        struct SZrObjectModule *module = ZrCore_Module_GetFromCache(state, moduleName);
        if (module != ZR_NULL) {
            SZrObjectPrototype *prototype = find_prototype_in_module(state, module, actualTypeName, expectedType);
            return prototype;
        }
    } else {
        // 没有指定模块名，尝试从当前调用栈的闭包中查找模块
        // 如果函数有模块信息，从模块中查找类型
        // 通过查找调用栈中的entry function，然后查找对应的模块
        if (state->callInfoList != ZR_NULL) {
            // 查找当前调用栈中的entry function（包含prototypeData的函数）
            SZrCallInfo *callInfo = state->callInfoList;
            while (callInfo != ZR_NULL) {
                if (callInfo->functionBase.valuePointer >= state->stackBase.valuePointer &&
                    callInfo->functionBase.valuePointer < state->stackTop.valuePointer) {
                    SZrTypeValue *closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
                    if (closureValue != ZR_NULL && closureValue->type == ZR_VALUE_TYPE_CLOSURE) {
                        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, closureValue->value.object);
                        if (closure != ZR_NULL && closure->function != ZR_NULL) {
                            struct SZrFunction *func = closure->function;
                            // 检查是否是entry function（有prototypeData）
                            if (func->prototypeData != ZR_NULL && func->prototypeCount > 0) {
                                // 查找对应的模块
                                // TODO: 注意：这里需要遍历模块注册表查找，简化实现：遍历所有模块
                                if (state->global != ZR_NULL) {
                                    SZrGlobalState *registryGlobal = state->global;
                                    if (ZrCore_Value_IsGarbageCollectable(&registryGlobal->loadedModulesRegistry) &&
                                        registryGlobal->loadedModulesRegistry.type == ZR_VALUE_TYPE_OBJECT) {
                                        SZrObject *registry =
                                                ZR_CAST_OBJECT(state, registryGlobal->loadedModulesRegistry.value.object);
                                        if (registry != ZR_NULL && registry->nodeMap.isValid && 
                                            registry->nodeMap.buckets != ZR_NULL) {
                                            // 遍历模块注册表，查找包含该entry function的模块
                                            for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
                                                SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
                                                while (pair != ZR_NULL) {
                                                    if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                                                        SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                                                        if (cachedObject != ZR_NULL && 
                                                            cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                                                            struct SZrObjectModule *module = (struct SZrObjectModule *)cachedObject;
                                                            // 检查模块的导出中是否有该类型
                                                            SZrObjectPrototype *prototype = find_prototype_in_module(state, module, actualTypeName, expectedType);
                                                            if (prototype != ZR_NULL) {
                                                                return prototype;
                                                            }
                                                        }
                                                    }
                                                    pair = pair->next;
                                                }
                                            }
                                        }
                                    }
                                }
                                break;  // 找到entry function后，不再继续查找
                            }
                        }
                    }
                }
                callInfo = callInfo->previous;
            }
        }
        
        // 从全局模块注册表中查找（遍历所有已加载的模块）
        // 如果上面的查找失败，遍历所有模块查找类型
        {
            SZrObjectPrototype *prototype =
                    find_prototype_in_loaded_modules_registry(state, actualTypeName, expectedType);
            if (prototype != ZR_NULL) {
                return prototype;
            }
        }
    }

    {
        SZrObjectPrototype *prototype = find_prototype_in_global_zr_object(state, actualTypeName, expectedType);
        if (prototype != ZR_NULL) {
            return prototype;
        }
    }

    if (materialize_entry_function_prototypes_for_lookup(state, moduleName)) {
        if (moduleName != ZR_NULL) {
            struct SZrObjectModule *module = ZrCore_Module_GetFromCache(state, moduleName);
            if (module != ZR_NULL) {
                SZrObjectPrototype *prototype = find_prototype_in_module(state, module, actualTypeName, expectedType);
                if (prototype != ZR_NULL) {
                    return prototype;
                }
            }
        } else {
            SZrObjectPrototype *prototype =
                    find_prototype_in_loaded_modules_registry(state, actualTypeName, expectedType);
            if (prototype != ZR_NULL) {
                return prototype;
            }
        }

        {
            SZrObjectPrototype *prototype = find_prototype_in_global_zr_object(state, actualTypeName, expectedType);
            if (prototype != ZR_NULL) {
                return prototype;
            }
        }
    }

    {
        SZrString *openGenericBaseName = extract_open_generic_base_type_name(state, actualTypeName);
        if (openGenericBaseName != ZR_NULL && !ZrCore_String_Equal(openGenericBaseName, actualTypeName)) {
            SZrObjectPrototype *openPrototype = find_type_prototype(state, openGenericBaseName, expectedType);
            if (openPrototype != ZR_NULL) {
                return openPrototype;
            }
        }
    }

    // 如果找不到，返回 ZR_NULL（后续可以通过元方法或创建新原型）
    // 注意：完整的实现需要：
    // - 模块加载时将类型原型注册到全局类型表
    // - 或者通过类型名称的模块路径（如 "module.TypeName"）来查找
    return ZR_NULL;
}

// 辅助函数：执行 struct 类型转换
// 将源对象转换为目标 struct 类型
TZrBool convert_to_struct(SZrState *state,
                          SZrTypeValue *source,
                          SZrObjectPrototype *targetPrototype,
                          SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查目标原型类型
    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        return ZR_FALSE;
    }
    
    // 如果源值是对象，尝试转换
    if (ZR_VALUE_IS_TYPE_OBJECT(source->type)) {
        SZrObject *sourceObject = ZR_CAST_OBJECT(state, source->value.object);
        if (sourceObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 创建新的 struct 对象（值类型）
        SZrObject *structObject = ZrCore_Object_New(state, targetPrototype);
        if (structObject == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Object_Init(state, structObject);
        
        // 设置内部类型为 STRUCT
        structObject->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
        
        // 复制源对象的字段到新对象
        // 对于 struct，字段存储在 nodeMap 中（与普通对象相同）
        // 遍历源对象的 nodeMap，复制匹配的字段到新对象
        if (sourceObject->nodeMap.isValid && sourceObject->nodeMap.buckets != ZR_NULL && sourceObject->nodeMap.elementCount > 0) {
            // 注意：ZrHashSet 没有迭代接口，我们需要通过其他方式复制字段
            // 一个方案是：通过元方法 @to_struct 来处理字段复制
            // 或者：如果源对象已经是 struct 类型，直接复制其 nodeMap
            
            // TODO: 暂时先复制所有字段（后续需要根据 struct 定义进行字段验证和类型转换）
            // 由于无法直接迭代 nodeMap，我们依赖元方法或构造函数来处理字段复制
            // 如果源对象有 @to_struct 元方法，应该已经在上层调用了
            // 这里只是创建了新的 struct 对象，字段复制由元方法或构造函数完成
        }
        
        ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(structObject));
        destination->type = ZR_VALUE_TYPE_OBJECT;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

// 辅助函数：执行 class 类型转换
// 将源对象转换为目标 class 类型
TZrBool convert_to_class(SZrState *state,
                         SZrTypeValue *source,
                         SZrObjectPrototype *targetPrototype,
                         SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查目标原型类型
    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        return ZR_FALSE;
    }
    
    // 如果源值是对象，尝试转换
    if (ZR_VALUE_IS_TYPE_OBJECT(source->type)) {
        SZrObject *sourceObject = ZR_CAST_OBJECT(state, source->value.object);
        if (sourceObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 创建新的 class 对象（引用类型）
        SZrObject *classObject = ZrCore_Object_New(state, targetPrototype);
        if (classObject == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Object_Init(state, classObject);
        
        // 设置内部类型为 OBJECT（class 是引用类型）
        classObject->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
        
        // 复制源对象的字段到新对象
        // 对于 class，字段存储在 nodeMap 中
        // 遍历源对象的 nodeMap，复制匹配的字段到新对象
        if (sourceObject->nodeMap.isValid && sourceObject->nodeMap.buckets != ZR_NULL && sourceObject->nodeMap.elementCount > 0) {
            // 注意：ZrHashSet 没有迭代接口，我们需要通过其他方式复制字段
            // 一个方案是：通过元方法 @to_object 来处理字段复制
            // 或者：如果源对象已经是 class 类型，直接复制其 nodeMap
            
            // TODO: 暂时先复制所有字段（后续需要根据 class 定义进行字段验证和类型转换）
            // 由于无法直接迭代 nodeMap，我们依赖元方法或构造函数来处理字段复制
            // 如果源对象有 @to_object 元方法，应该已经在上层调用了
            // 这里只是创建了新的 class 对象，字段复制由元方法或构造函数完成
        }
        
        ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(classObject));
        destination->type = ZR_VALUE_TYPE_OBJECT;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

static const SZrTypeValue *execution_get_object_field_cstring(SZrState *state,
                                                              SZrObject *object,
                                                              const TZrChar *fieldName) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static void execution_set_object_field_cstring(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               const SZrTypeValue *value) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

static const TZrChar *execution_get_enum_value_type_name(SZrState *state, SZrObjectPrototype *prototype) {
    const SZrTypeValue *typeValue;

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return ZR_NULL;
    }

    typeValue = execution_get_object_field_cstring(state, &prototype->super, kNativeEnumValueTypeFieldName);
    if (typeValue == ZR_NULL || typeValue->type != ZR_VALUE_TYPE_STRING || typeValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, typeValue->value.object));
}

static TZrBool execution_extract_enum_underlying_value(SZrState *state,
                                                       const SZrTypeValue *source,
                                                       SZrObjectPrototype *targetPrototype,
                                                       SZrTypeValue *underlyingValue) {
    const SZrTypeValue *fieldValue;
    SZrObject *object;

    if (state == ZR_NULL || source == ZR_NULL || underlyingValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (source->type != ZR_VALUE_TYPE_OBJECT || source->value.object == ZR_NULL) {
        ZrCore_Value_Copy(state, underlyingValue, (SZrTypeValue *)source);
        return ZR_TRUE;
    }

    object = ZR_CAST_OBJECT(state, source->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetPrototype != ZR_NULL && object->prototype == targetPrototype) {
        ZrCore_Value_Copy(state, underlyingValue, (SZrTypeValue *)source);
        return ZR_TRUE;
    }

    fieldValue = execution_get_object_field_cstring(state, object, kNativeEnumValueFieldName);
    if (fieldValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, underlyingValue, (SZrTypeValue *)fieldValue);
    return ZR_TRUE;
}

static TZrBool execution_normalize_enum_underlying_value(SZrState *state,
                                                         const SZrTypeValue *source,
                                                         const TZrChar *expectedTypeName,
                                                         SZrTypeValue *normalizedValue) {
    if (state == ZR_NULL || source == ZR_NULL || normalizedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (expectedTypeName == ZR_NULL || strcmp(expectedTypeName, "int") == 0) {
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(source->type)) {
            ZrCore_Value_InitAsInt(state, normalizedValue, source->value.nativeObject.nativeInt64);
            return ZR_TRUE;
        }
        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(source->type)) {
            ZrCore_Value_InitAsInt(state, normalizedValue, (TZrInt64)source->value.nativeObject.nativeUInt64);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "float") == 0) {
        if (ZR_VALUE_IS_TYPE_NUMBER(source->type) || ZR_VALUE_IS_TYPE_BOOL(source->type)) {
            ZrCore_Value_InitAsFloat(state, normalizedValue, value_to_double(source));
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "bool") == 0) {
        if (ZR_VALUE_IS_TYPE_BOOL(source->type)) {
            normalizedValue->type = ZR_VALUE_TYPE_BOOL;
            normalizedValue->value.nativeObject.nativeBool = source->value.nativeObject.nativeBool;
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "string") == 0) {
        if (ZR_VALUE_IS_TYPE_STRING(source->type)) {
            ZrCore_Value_Copy(state, normalizedValue, (SZrTypeValue *)source);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "null") == 0) {
        if (source->type == ZR_VALUE_TYPE_NULL) {
            ZrCore_Value_ResetAsNull(normalizedValue);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, normalizedValue, (SZrTypeValue *)source);
    return ZR_TRUE;
}

static TZrBool execution_enum_values_equal(SZrState *state,
                                           const SZrTypeValue *left,
                                           const SZrTypeValue *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    if (left->type == ZR_VALUE_TYPE_NULL || right->type == ZR_VALUE_TYPE_NULL) {
        return left->type == right->type;
    }

    if (ZR_VALUE_IS_TYPE_STRING(left->type) && ZR_VALUE_IS_TYPE_STRING(right->type)) {
        return ZrCore_String_Equal(ZR_CAST_STRING(state, left->value.object), ZR_CAST_STRING(state, right->value.object));
    }

    if (ZR_VALUE_IS_TYPE_BOOL(left->type) && ZR_VALUE_IS_TYPE_BOOL(right->type)) {
        return left->value.nativeObject.nativeBool == right->value.nativeObject.nativeBool;
    }

    if ((ZR_VALUE_IS_TYPE_NUMBER(left->type) || ZR_VALUE_IS_TYPE_BOOL(left->type)) &&
        (ZR_VALUE_IS_TYPE_NUMBER(right->type) || ZR_VALUE_IS_TYPE_BOOL(right->type))) {
        if (ZR_VALUE_IS_TYPE_FLOAT(left->type) || ZR_VALUE_IS_TYPE_FLOAT(right->type)) {
            return value_to_double(left) == value_to_double(right);
        }
        return value_to_int64(left) == value_to_int64(right);
    }

    return ZR_FALSE;
}

static SZrString *execution_find_enum_member_name(SZrState *state,
                                                  SZrObjectPrototype *prototype,
                                                  const SZrTypeValue *underlyingValue) {
    TZrSize bucketIndex;

    if (state == ZR_NULL || prototype == ZR_NULL || underlyingValue == ZR_NULL ||
        !prototype->super.nodeMap.isValid || prototype->super.nodeMap.buckets == ZR_NULL) {
        return ZR_NULL;
    }

    for (bucketIndex = 0; bucketIndex < prototype->super.nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = prototype->super.nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->key.type == ZR_VALUE_TYPE_STRING &&
                pair->value.type == ZR_VALUE_TYPE_OBJECT &&
                pair->value.value.object != ZR_NULL) {
                SZrObject *candidate = ZR_CAST_OBJECT(state, pair->value.value.object);
                const SZrTypeValue *candidateValue;

                if (candidate != ZR_NULL && candidate->prototype == prototype) {
                    candidateValue = execution_get_object_field_cstring(state, candidate, kNativeEnumValueFieldName);
                    if (candidateValue != ZR_NULL && execution_enum_values_equal(state, candidateValue, underlyingValue)) {
                        return ZR_CAST_STRING(state, pair->key.value.object);
                    }
                }
            }

            pair = pair->next;
        }
    }

    return ZR_NULL;
}

TZrBool convert_to_enum(SZrState *state,
                        SZrTypeValue *source,
                        SZrObjectPrototype *targetPrototype,
                        SZrTypeValue *destination) {
    SZrObject *enumObject;
    SZrString *memberName;
    SZrTypeValue extractedValue;
    SZrTypeValue normalizedValue;
    const TZrChar *expectedTypeName;

    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(&extractedValue);
    ZrCore_Value_ResetAsNull(&normalizedValue);

    if (source->type == ZR_VALUE_TYPE_OBJECT && source->value.object != ZR_NULL) {
        SZrObject *object = ZR_CAST_OBJECT(state, source->value.object);
        if (object != ZR_NULL && object->prototype == targetPrototype) {
            ZrCore_Value_Copy(state, destination, source);
            return ZR_TRUE;
        }
    }

    if (!execution_extract_enum_underlying_value(state, source, targetPrototype, &extractedValue)) {
        return ZR_FALSE;
    }

    expectedTypeName = execution_get_enum_value_type_name(state, targetPrototype);
    if (!execution_normalize_enum_underlying_value(state, &extractedValue, expectedTypeName, &normalizedValue)) {
        return ZR_FALSE;
    }

    enumObject = ZrCore_Object_New(state, targetPrototype);
    if (enumObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Object_Init(state, enumObject);
    execution_set_object_field_cstring(state, enumObject, kNativeEnumValueFieldName, &normalizedValue);
    memberName = execution_find_enum_member_name(state, targetPrototype, &normalizedValue);
    if (memberName != ZR_NULL) {
        SZrTypeValue nameValue;
        ZrCore_Value_InitAsRawObject(state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        nameValue.type = ZR_VALUE_TYPE_STRING;
        execution_set_object_field_cstring(state, enumObject, kNativeEnumNameFieldName, &nameValue);
    }

    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(enumObject));
    destination->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

