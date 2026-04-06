//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/global.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/module.h"
#include "zr_vm_common/zr_type_conf.h"

#include <string.h>

static SZrString *global_state_create_permanent_string(SZrState *state, const TZrChar *text) {
    SZrString *stringObject;

    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    stringObject = ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
    if (stringObject != ZR_NULL) {
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    }
    return stringObject;
}

static void global_state_register_zr_member(SZrState *state,
                                            SZrObject *zrObject,
                                            const TZrChar *memberName,
                                            const SZrTypeValue *memberValue) {
    SZrString *memberNameString;
    SZrTypeValue memberKey;

    if (state == ZR_NULL || zrObject == ZR_NULL || memberName == ZR_NULL || memberValue == ZR_NULL) {
        return;
    }

    memberNameString = global_state_create_permanent_string(state, memberName);
    if (memberNameString == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberNameString));
    memberKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, zrObject, &memberKey, memberValue);
}

static void global_state_register_named_prototype(SZrState *state,
                                                  SZrGlobalState *global,
                                                  SZrObject *zrObject,
                                                  SZrObjectPrototype *prototype) {
    SZrTypeValue prototypeValue;

    if (state == ZR_NULL || global == ZR_NULL || zrObject == ZR_NULL || prototype == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    global_state_register_zr_member(state, zrObject, ZrCore_String_GetNativeString(prototype->name), &prototypeValue);
}

static void global_state_register_builtin_array_members(SZrState *state, SZrGlobalState *global) {
    SZrObjectPrototype *arrayPrototype;
    SZrString *lengthName;
    SZrMemberDescriptor descriptor;

    if (state == ZR_NULL || global == ZR_NULL) {
        return;
    }

    arrayPrototype = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    if (arrayPrototype == ZR_NULL) {
        return;
    }

    lengthName = global_state_create_permanent_string(state, "length");
    if (lengthName == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
    descriptor.name = lengthName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY;
    descriptor.contractRole = ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH;
    ZrCore_ObjectPrototype_AddMemberDescriptor(state, arrayPrototype, &descriptor);
    ZrCore_ObjectPrototype_AddProtocol(arrayPrototype, ZR_PROTOCOL_ID_ARRAY_LIKE);
}

static SZrFunction *global_state_create_native_callable(SZrState *state, FZrNativeFunction nativeFunction) {
    SZrClosureNative *closure;

    if (state == ZR_NULL || nativeFunction == ZR_NULL) {
        return ZR_NULL;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_NULL;
    }

    closure->nativeFunction = nativeFunction;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    return ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
}

static SZrString *global_state_get_string_receiver(SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return ZR_NULL;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    if (receiverValue == ZR_NULL ||
        receiverValue->type != ZR_VALUE_TYPE_STRING ||
        receiverValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, receiverValue->value.object);
}

static TZrInt64 global_state_string_length_getter_native(SZrState *state) {
    TZrStackValuePointer base;
    SZrString *receiverString;
    TZrSize codePointLength = 0;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverString = global_state_get_string_receiver(state);
    if (receiverString == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    if (!ZrCore_String_GetCodePointLength(receiverString, &codePointLength)) {
        ZrCore_Debug_RunError(state, "invalid UTF-8 string");
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), (TZrInt64)codePointLength);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 global_state_string_byte_length_getter_native(SZrState *state) {
    TZrStackValuePointer base;
    SZrString *receiverString;
    TZrSize byteLength;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverString = global_state_get_string_receiver(state);
    if (receiverString == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    byteLength = ZrCore_String_GetByteLength(receiverString);
    if (!ZrCore_Utf8_IsValid(ZrCore_String_GetNativeString(receiverString), byteLength)) {
        ZrCore_Debug_RunError(state, "invalid UTF-8 string");
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), (TZrInt64)byteLength);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 global_state_string_to_array_native(SZrState *state) {
    TZrStackValuePointer base;
    SZrString *receiverString;
    SZrObject *byteArray = ZR_NULL;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverString = global_state_get_string_receiver(state);
    if (receiverString == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    if (!ZrCore_Utf8_IsValid(ZrCore_String_GetNativeString(receiverString),
                             ZrCore_String_GetByteLength(receiverString))) {
        ZrCore_Debug_RunError(state, "invalid UTF-8 string");
    }

    if (!ZrCore_String_ToByteArray(state, receiverString, &byteArray) || byteArray == ZR_NULL) {
        ZrCore_Debug_RunError(state, "failed to materialize string byte array");
    }

    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(byteArray));
    ZrCore_Stack_GetValue(base)->type = ZR_VALUE_TYPE_ARRAY;
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static void global_state_register_prototype_property(SZrState *state,
                                                     SZrObjectPrototype *prototype,
                                                     const TZrChar *memberName,
                                                     FZrNativeFunction getterFunction) {
    SZrFunction *getter;
    SZrString *memberNameString;
    SZrMemberDescriptor descriptor;

    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL || getterFunction == ZR_NULL) {
        return;
    }

    getter = global_state_create_native_callable(state, getterFunction);
    memberNameString = global_state_create_permanent_string(state, memberName);
    if (getter == ZR_NULL || memberNameString == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberNameString;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY;
    descriptor.getterFunction = getter;
    ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor);
}

static void global_state_register_prototype_method(SZrState *state,
                                                   SZrObjectPrototype *prototype,
                                                   const TZrChar *memberName,
                                                   FZrNativeFunction nativeFunction) {
    SZrFunction *callable;
    SZrString *memberNameString;
    SZrMemberDescriptor descriptor;
    SZrTypeValue key;
    SZrTypeValue value;

    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL || nativeFunction == ZR_NULL) {
        return;
    }

    callable = global_state_create_native_callable(state, nativeFunction);
    memberNameString = global_state_create_permanent_string(state, memberName);
    if (callable == ZR_NULL || memberNameString == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberNameString;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
    ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor);

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(callable));
    ZrCore_Object_SetValue(state, &prototype->super, &key, &value);
}

static void global_state_register_builtin_string_members(SZrState *state, SZrGlobalState *global) {
    SZrObjectPrototype *stringPrototype;

    if (state == ZR_NULL || global == ZR_NULL) {
        return;
    }

    stringPrototype = global->basicTypeObjectPrototype[ZR_VALUE_TYPE_STRING];
    if (stringPrototype == ZR_NULL) {
        return;
    }

    global_state_register_prototype_property(state,
                                             stringPrototype,
                                             "length",
                                             global_state_string_length_getter_native);
    global_state_register_prototype_property(state,
                                             stringPrototype,
                                             "byteLength",
                                             global_state_string_byte_length_getter_native);
    global_state_register_prototype_method(state,
                                           stringPrototype,
                                           "toArray",
                                           global_state_string_to_array_native);
}

static void global_state_init_builtin_exception_types(SZrState *state, SZrGlobalState *global, SZrObject *zrObject) {
    SZrObjectPrototype *errorPrototype;
    SZrStructPrototype *stackFramePrototype;
    static const TZrChar *kStackFrameFieldNames[] = {
            "functionName",
            "sourceFile",
            "sourceLine",
            "instructionOffset",
    };

    if (state == ZR_NULL || global == ZR_NULL || zrObject == ZR_NULL) {
        return;
    }

    errorPrototype = ZrCore_ObjectPrototype_New(state,
                                                global_state_create_permanent_string(state, "Error"),
                                                ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (errorPrototype != ZR_NULL) {
        global->errorPrototype = errorPrototype;
        global_state_register_named_prototype(state, global, zrObject, errorPrototype);
    }

    stackFramePrototype = ZrCore_StructPrototype_New(state, global_state_create_permanent_string(state, "StackFrame"));
    if (stackFramePrototype != ZR_NULL) {
        for (TZrSize index = 0; index < sizeof(kStackFrameFieldNames) / sizeof(kStackFrameFieldNames[0]); index++) {
            SZrString *fieldName = global_state_create_permanent_string(state, kStackFrameFieldNames[index]);
            if (fieldName != ZR_NULL) {
                ZrCore_StructPrototype_AddField(state, stackFramePrototype, fieldName, index);
            }
        }
        global->stackFramePrototype = &stackFramePrototype->super;
        global_state_register_named_prototype(state, global, zrObject, &stackFramePrototype->super);
    }
}

SZrGlobalState *ZrCore_GlobalState_New(FZrAllocator allocator, TZrPtr userAllocationArguments, TZrUInt64 uniqueNumber,
                                  SZrCallbackGlobal *callbacks) {
    SZrGlobalState *global =
            allocator(userAllocationArguments, ZR_NULL, 0, sizeof(SZrGlobalState), ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Memory_RawSet(global, 0, sizeof(SZrGlobalState));
    // when create and init global state, we make the global is not valid
    global->isValid = ZR_FALSE;
    global->allocator = allocator;
    global->userAllocationArguments = userAllocationArguments;
    SZrState *newState = ZrCore_State_New(global);
    global->mainThreadState = newState;
    global->threadWithStackClosures = ZR_NULL;
    // todo: main thread cannot yield

    // todo:
    global->logFunction = ZR_NULL;

    // generate seed
    global->hashSeed = ZrCore_HashSeed_Create(global, uniqueNumber);

    // gc
    ZrCore_GarbageCollector_New(global);

    // io
    global->sourceLoader = ZR_NULL;
    global->sourceLoaderUserData = ZR_NULL;
    global->aotModuleLoader = ZR_NULL;
    global->aotModuleLoaderUserData = ZR_NULL;
    global->nativeModuleLoader = ZR_NULL;
    global->nativeModuleLoaderUserData = ZR_NULL;

    // init string table
    ZrCore_StringTable_New(global);

    // init global module registry
    ZrCore_Value_ResetAsNull(&global->loadedModulesRegistry);
    // init global null value
    ZrCore_Value_ResetAsNull(&global->nullValue);
    // init global zr object
    ZrCore_Value_ResetAsNull(&global->zrObject);
    ZrCore_Array_Construct(&global->importCompileInfoStack);
    ZrCore_Array_Init(newState, &global->importCompileInfoStack, sizeof(SZrString *), 4);

    // exception
    global->panicHandlingFunction = ZR_NULL;
    global->errorPrototype = ZR_NULL;
    global->stackFramePrototype = ZR_NULL;
    ZrCore_Value_ResetAsNull(&global->unhandledExceptionHandler);
    global->hasUnhandledExceptionHandler = ZR_FALSE;
    global->registryInitialized = ZR_FALSE;

    // injected data
    global->userData = ZR_NULL;

    // loader
    global->sourceLoader = ZR_NULL;
    global->sourceLoaderUserData = ZR_NULL;
    global->aotModuleLoader = ZR_NULL;
    global->aotModuleLoaderUserData = ZR_NULL;
    global->nativeModuleLoader = ZR_NULL;
    global->nativeModuleLoaderUserData = ZR_NULL;
    
    // parser and compiler (初始化为NULL，需要外部注入)
    // 封装了从源代码解析到编译的全流程
    global->compileSource = ZR_NULL;
    global->emitCompileTimeRuntimeSupport = ZR_FALSE;

    // reset basic type object prototype
    for (TZrUInt64 i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        global->basicTypeObjectPrototype[i] = ZR_NULL;
    }
    // write callbacks to  global
    if (callbacks != ZR_NULL) {
        global->callbacks = *callbacks;
    } else {
        ZrCore_Memory_RawSet(&global->callbacks, 0, sizeof(SZrCallbackGlobal));
    }
    // launch new state with try-catch
    if (ZrCore_Exception_TryRun(newState, ZrCore_State_MainThreadLaunch, ZR_NULL) != ZR_THREAD_STATUS_FINE) {
        ZrCore_State_Exit(newState);
        ZrCore_GlobalState_Free(global);
        global = ZR_NULL;
        return ZR_NULL;
    }
    // set warning function

    // set panic function
    return global;
}

// 初始化基本类型对象原型
static void global_state_init_basic_type_object_prototypes(SZrState *state, SZrGlobalState *global) {
    if (state != ZR_NULL) {
        state->global = global;
    }
    // 为每个值类型创建 ObjectPrototype 对象
    for (TZrSize i = 0; i < ZR_VALUE_TYPE_ENUM_MAX; i++) {
        // 创建 ObjectPrototype 对象（ZrCore_Object_NewCustomized 已经设置了 internalType 并调用了 ZrCore_HashSet_Construct）
        SZrObject *objectBase = ZrCore_Object_NewCustomized(state, sizeof(SZrObjectPrototype), ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE);
        if (objectBase == ZR_NULL) {
            continue;
        }
        
        SZrObjectPrototype *prototype = (SZrObjectPrototype *)objectBase;
        
        // 初始化哈希集（ZrCore_Object_NewCustomized 只调用了 Construct，需要调用 Init）
        ZrCore_HashSet_Init(state, &prototype->super.nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
        
        // 初始化 ObjectPrototype 特定字段
        prototype->name = ZR_NULL;
        prototype->type = ZR_OBJECT_PROTOTYPE_TYPE_NATIVE;
        prototype->superPrototype = ZR_NULL;
        prototype->memberDescriptors = ZR_NULL;
        prototype->memberDescriptorCount = 0;
        prototype->memberDescriptorCapacity = 0;
        memset(&prototype->indexContract, 0, sizeof(prototype->indexContract));
        memset(&prototype->iterableContract, 0, sizeof(prototype->iterableContract));
        memset(&prototype->iteratorContract, 0, sizeof(prototype->iteratorContract));
        prototype->protocolMask = 0;
        prototype->dynamicMemberCapable = ZR_FALSE;
        prototype->reserved0 = 0;
        prototype->reserved1 = 0;
        prototype->managedFields = ZR_NULL;
        prototype->managedFieldCount = 0;
        prototype->managedFieldCapacity = 0;
        
        // 初始化 metaTable
        ZrCore_MetaTable_Construct(&prototype->metaTable);
        
        // 将原型存储到全局数组中
        global->basicTypeObjectPrototype[i] = prototype;
        
        // 标记为永久对象（避免被 GC 回收）
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
        
        // 为基本类型注册默认元方法
        ZrCore_Meta_InitBuiltinTypeMetaMethods(state, (EZrValueType)i);
    }
}

void ZrCore_GlobalState_InitRegistry(SZrState *state, SZrGlobalState *global) {
    if (state == ZR_NULL || global == ZR_NULL) {
        return;
    }
    if (global->registryInitialized) {
        return;
    }
    state->global = global;
    global->mainThreadState = state;

    SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
    ZrCore_Object_Init(state, object);  // 初始化对象的 nodeMap
    ZrCore_Value_InitAsRawObject(state, &global->loadedModulesRegistry, ZR_CAST_RAW_OBJECT(object));

    // 创建全局 zr 对象
    SZrObject *zrObject = ZrCore_Object_New(state, ZR_NULL);
    ZrCore_Object_Init(state, zrObject);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(zrObject));

    // 将 zr 对象存储到 global->zrObject 中，供 GET_GLOBAL 指令使用
    SZrTypeValue zrValue;
    ZrCore_Value_InitAsRawObject(state, &zrValue, ZR_CAST_RAW_OBJECT_AS_SUPER(zrObject));
    zrValue.type = ZR_VALUE_TYPE_OBJECT;
    global->zrObject = zrValue;

    // 初始化基本类型对象原型
    global_state_init_basic_type_object_prototypes(state, global);
    global_state_register_builtin_array_members(state, global);
    global_state_register_builtin_string_members(state, global);
    global_state_init_builtin_exception_types(state, global, zrObject);
    
    // 将 zr 对象添加到全局状态（TODO: 需要确认如何访问全局作用域）
    // 根据计划，zr 除非被局部作用域名字覆盖，否则全局只读
    // TODO: 这里暂时先创建对象，后续需要在全局作用域中注册
    
    // 注意：compileSource 函数指针由 parser 模块通过 ZrCore_GlobalState_SetCompileSource 设置
    // 这里不直接调用 parser 模块的函数，以避免循环依赖
    
    // todo: load state value
    // todo: load global value
    // todo: register zr object to global scope

    global->registryInitialized = ZR_TRUE;
}

// 设置 compileSource 函数指针（由 parser 模块调用）
void ZrCore_GlobalState_SetCompileSource(SZrGlobalState *global, 
    struct SZrFunction *(*compileSource)(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName)) {
    if (global != ZR_NULL) {
        global->compileSource = compileSource;
    }
}

void ZrCore_GlobalState_SetNativeModuleLoader(SZrGlobalState *global,
                                              FZrNativeModuleLoader loader,
                                              TZrPtr userData) {
    if (global == ZR_NULL) {
        return;
    }

    global->nativeModuleLoader = loader;
    global->nativeModuleLoaderUserData = userData;
}

void ZrCore_GlobalState_SetAotModuleLoader(SZrGlobalState *global,
                                           FZrAotModuleLoader loader,
                                           TZrPtr userData) {
    if (global == ZR_NULL) {
        return;
    }

    global->aotModuleLoader = loader;
    global->aotModuleLoaderUserData = userData;
}


void ZrCore_GlobalState_Free(SZrGlobalState *global) {
    FZrAllocator allocator = global->allocator;

    if (global->importCompileInfoStack.isValid &&
        global->importCompileInfoStack.head != ZR_NULL &&
        global->importCompileInfoStack.capacity > 0 &&
        global->importCompileInfoStack.elementSize > 0 &&
        global->mainThreadState != ZR_NULL) {
        ZrCore_Array_Free(global->mainThreadState, &global->importCompileInfoStack);
    }

    ZrCore_StringTable_Free(global, global->stringTable);
    global->stringTable = ZR_NULL;

    ZrCore_GarbageCollector_Free(global, global->garbageCollector);
    global->garbageCollector = ZR_NULL;

    ZrCore_State_Free(global, global->mainThreadState);
    // free global at last
    allocator(global->userAllocationArguments, global, sizeof(SZrGlobalState), 0, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
}
