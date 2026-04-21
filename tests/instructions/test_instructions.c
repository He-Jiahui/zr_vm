//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

static TZrInt64 g_hidden_meta_static_storage = 0;

// 测试日志宏（符合测试规范）
#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_INFO(summary, details)                                                                                    \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                    \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                      \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_MODULE_DIVIDER()                                                                                          \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

// 简单的测试分配器
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr) pointer >= (TZrPtr) 0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    } else {
        if ((TZrPtr) pointer >= (TZrPtr) 0x1000) {
            return realloc(pointer, newSize);
        } else {
            return malloc(newSize);
        }
    }
}

// 创建测试用的SZrState
static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (!global)
        return ZR_NULL;

    SZrState *mainState = global->mainThreadState;
    if (mainState) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        ZrCore_StringTable_Init(mainState);
        ZrCore_Meta_GlobalStaticsInit(mainState);
    }

    return mainState;
}

// 销毁测试用的SZrState
static void destroy_test_state(SZrState *state) {
    if (!state)
        return;

    SZrGlobalState *global = state->global;
    if (global) {
        ZrCore_GlobalState_Free(global);
    }
}

static SZrFunction *create_native_callable(SZrState *state, FZrNativeFunction nativeFunction) {
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

static SZrFunction *create_young_native_callable(SZrState *state, FZrNativeFunction nativeFunction) {
    SZrClosureNative *closure;

    if (state == ZR_NULL || nativeFunction == ZR_NULL) {
        return ZR_NULL;
    }

    closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_NULL;
    }

    closure->nativeFunction = nativeFunction;
    return ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
}

static void set_object_field_cstring(SZrState *state,
                                     SZrObject *object,
                                     const TZrChar *fieldName,
                                     const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

static const SZrTypeValue *get_object_field_cstring(SZrState *state,
                                                    SZrObject *object,
                                                    const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static TZrUInt32 count_remembered_object_occurrences(SZrGarbageCollector *gc, SZrRawObject *target) {
    TZrUInt32 count = 0u;

    if (gc == ZR_NULL || target == ZR_NULL || gc->rememberedObjects == ZR_NULL) {
        return 0u;
    }

    for (TZrSize index = 0; index < gc->rememberedObjectCount; index++) {
        if (gc->rememberedObjects[index] == target) {
            count++;
        }
    }

    return count;
}

static TZrBool add_named_fields_until_object_rehashes(SZrState *state,
                                                      SZrObject *object,
                                                      const TZrChar *prefix,
                                                      TZrUInt32 startingValue,
                                                      TZrUInt32 maxFields) {
    SZrHashKeyValuePair **originalBuckets;

    if (state == ZR_NULL || object == ZR_NULL || prefix == ZR_NULL || maxFields == 0u) {
        return ZR_FALSE;
    }

    originalBuckets = object->nodeMap.buckets;
    for (TZrUInt32 index = 0; index < maxFields; index++) {
        TZrChar fieldName[64];
        SZrTypeValue value;

        snprintf(fieldName, sizeof(fieldName), "%s_%u", prefix, index);
        ZrCore_Value_InitAsInt(state, &value, (TZrInt64)(startingValue + index));
        set_object_field_cstring(state, object, fieldName, &value);
        if (object->nodeMap.buckets != originalBuckets) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void init_object_constant_with_shared_field(SZrState *state,
                                                   const TZrChar *typeNameNative,
                                                   SZrString *memberName,
                                                   TZrInt64 storedValue,
                                                   SZrObjectPrototype **outPrototype,
                                                   SZrObject **outInstance,
                                                   SZrTypeValue *outConstantValue) {
    SZrString *typeName;
    SZrMemberDescriptor descriptor;
    SZrObject *instance;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(typeNameNative);
    TEST_ASSERT_NOT_NULL(memberName);
    TEST_ASSERT_NOT_NULL(outPrototype);
    TEST_ASSERT_NOT_NULL(outInstance);
    TEST_ASSERT_NOT_NULL(outConstantValue);

    typeName = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeNameNative);
    TEST_ASSERT_NOT_NULL(typeName);

    *outPrototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(*outPrototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
    descriptor.isWritable = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, *outPrototype, &descriptor));

    instance = ZrCore_Object_New(state, *outPrototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(state, instance);

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &value, storedValue);
    ZrCore_Object_SetValue(state, instance, &key, &value);

    ZrCore_Value_InitAsRawObject(state, outConstantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    outConstantValue->type = ZR_VALUE_TYPE_OBJECT;

    *outInstance = instance;
}

static void init_object_constant_with_existing_field_prototype(SZrState *state,
                                                               SZrObjectPrototype *prototype,
                                                               SZrString *memberName,
                                                               TZrInt64 storedValue,
                                                               SZrObject **outInstance,
                                                               SZrTypeValue *outConstantValue) {
    SZrObject *instance;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_NOT_NULL(memberName);
    TEST_ASSERT_NOT_NULL(outInstance);
    TEST_ASSERT_NOT_NULL(outConstantValue);

    instance = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(instance);
    ZrCore_Object_Init(state, instance);

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &value, storedValue);
    ZrCore_Object_SetValue(state, instance, &key, &value);

    ZrCore_Value_InitAsRawObject(state, outConstantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
    outConstantValue->type = ZR_VALUE_TYPE_OBJECT;

    *outInstance = instance;
}

static void init_static_method_prototype_constant(SZrState *state,
                                                  const TZrChar *typeNameNative,
                                                  SZrString *memberName,
                                                  FZrNativeFunction nativeFunction,
                                                  SZrObjectPrototype **outPrototype,
                                                  SZrFunction **outCallable,
                                                  SZrTypeValue *outConstantValue) {
    SZrString *typeName;
    SZrMemberDescriptor descriptor;
    SZrTypeValue key;
    SZrTypeValue functionValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(typeNameNative);
    TEST_ASSERT_NOT_NULL(memberName);
    TEST_ASSERT_NOT_NULL(nativeFunction);
    TEST_ASSERT_NOT_NULL(outPrototype);
    TEST_ASSERT_NOT_NULL(outCallable);
    TEST_ASSERT_NOT_NULL(outConstantValue);

    typeName = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeNameNative);
    TEST_ASSERT_NOT_NULL(typeName);

    *outPrototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(*outPrototype);

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberName;
    descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
    descriptor.isStatic = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, *outPrototype, &descriptor));

    *outCallable = create_native_callable(state, nativeFunction);
    TEST_ASSERT_NOT_NULL(*outCallable);

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(*outCallable));
    ZrCore_Object_SetValue(state, &(*outPrototype)->super, &key, &functionValue);

    ZrCore_Value_InitAsRawObject(state, outConstantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(*outPrototype));
    outConstantValue->type = ZR_VALUE_TYPE_OBJECT;
}

static TZrInt64 test_member_property_getter_native(struct SZrState *state) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), 77);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_hidden_meta_getter_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;
    SZrObject *receiverObject;
    const SZrTypeValue *storedValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    if (receiverValue == ZR_NULL ||
        (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiverValue->value.object);
    storedValue = get_object_field_cstring(state, receiverObject, "__meta_value");
    if (storedValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    } else {
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), storedValue);
    }
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_hidden_meta_setter_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;
    SZrTypeValue *assignedValue;
    SZrObject *receiverObject;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    assignedValue = ZrCore_Stack_GetValue(base + 2);
    if (receiverValue == ZR_NULL || assignedValue == ZR_NULL ||
        (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiverValue->value.object);
    set_object_field_cstring(state, receiverObject, "__meta_value", assignedValue);
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), assignedValue);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_hidden_meta_static_getter_native(struct SZrState *state) {
    TZrStackValuePointer base;
    TZrStackValuePointer stackTop;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    stackTop = state->stackTop.valuePointer;
    ZrCore_Value_InitAsInt(state,
                           ZrCore_Stack_GetValue(base),
                           stackTop == (base + 1) ? 91 : -1);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_hidden_meta_static_cached_getter_native(struct SZrState *state) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), g_hidden_meta_static_storage);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_hidden_meta_static_cached_setter_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *assignedValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    assignedValue = ZrCore_Stack_GetValue(base + 1);
    if (assignedValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    if (ZR_VALUE_IS_TYPE_INT(assignedValue->type)) {
        g_hidden_meta_static_storage = assignedValue->value.nativeObject.nativeInt64;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), assignedValue);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_hidden_meta_cached_monomorphic_a_getter_native(struct SZrState *state) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), 101);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_hidden_meta_cached_monomorphic_b_getter_native(struct SZrState *state) {
    TZrStackValuePointer base;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), 202);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_meta_call_cached_add_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrObject *receiverObject;
    const SZrTypeValue *storedValue;
    TZrInt64 baseValue = 0;
    TZrInt64 argument = 0;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    argumentValue = ZrCore_Stack_GetValue(base + 2);
    if (receiverValue == ZR_NULL || argumentValue == ZR_NULL ||
        (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiverValue->value.object);
    storedValue = get_object_field_cstring(state, receiverObject, "__call_base");
    if (storedValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(storedValue->type)) {
        baseValue = storedValue->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_INT(argumentValue->type)) {
        argument = argumentValue->value.nativeObject.nativeInt64;
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), baseValue + argument);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_meta_call_cached_mul_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrObject *receiverObject;
    const SZrTypeValue *storedValue;
    TZrInt64 baseValue = 0;
    TZrInt64 argument = 0;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    argumentValue = ZrCore_Stack_GetValue(base + 2);
    if (receiverValue == ZR_NULL || argumentValue == ZR_NULL ||
        (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiverValue->value.object);
    storedValue = get_object_field_cstring(state, receiverObject, "__call_base");
    if (storedValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(storedValue->type)) {
        baseValue = storedValue->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_INT(argumentValue->type)) {
        argument = argumentValue->value.nativeObject.nativeInt64;
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), baseValue * argument);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_index_contract_get_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;
    const SZrTypeValue *storedValue;
    SZrObject *receiverObject;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    if (receiverValue == ZR_NULL ||
        (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiverValue->value.object);
    storedValue = get_object_field_cstring(state, receiverObject, "__contract_index_value");
    if (storedValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    } else {
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), storedValue);
    }
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_index_contract_set_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;
    SZrTypeValue *inputValue;
    SZrObject *receiverObject;
    SZrTypeValue storedValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    inputValue = ZrCore_Stack_GetValue(base + 3);
    if (receiverValue == ZR_NULL || inputValue == ZR_NULL ||
        (receiverValue->type != ZR_VALUE_TYPE_OBJECT && receiverValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return 0;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiverValue->value.object);
    if (ZR_VALUE_IS_TYPE_INT(inputValue->type)) {
        ZrCore_Value_InitAsInt(state, &storedValue, inputValue->value.nativeObject.nativeInt64 * 2);
    } else {
        ZrCore_Value_ResetAsNull(&storedValue);
    }
    set_object_field_cstring(state, receiverObject, "__contract_index_value", &storedValue);
    state->stackTop.valuePointer = base;
    return 0;
}

static TZrInt64 test_iter_contract_init_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *receiverValue;
    const SZrTypeValue *iteratorValue;
    SZrObject *receiverObject;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    receiverValue = ZrCore_Stack_GetValue(base + 1);
    if (receiverValue == ZR_NULL || receiverValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    receiverObject = ZR_CAST_OBJECT(state, receiverValue->value.object);
    iteratorValue = get_object_field_cstring(state, receiverObject, "__contract_iterator");
    if (iteratorValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    } else {
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), iteratorValue);
    }
    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_iter_contract_move_next_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *iteratorValue;
    SZrObject *iteratorObject;
    const SZrTypeValue *stepValue;
    TZrInt64 nextStep = 0;
    SZrTypeValue storedStep;
    SZrTypeValue currentValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    iteratorValue = ZrCore_Stack_GetValue(base + 1);
    if (iteratorValue == ZR_NULL || iteratorValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrCore_Value_InitAsBool(state, ZrCore_Stack_GetValue(base), ZR_FALSE);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    iteratorObject = ZR_CAST_OBJECT(state, iteratorValue->value.object);
    stepValue = get_object_field_cstring(state, iteratorObject, "__iter_step");
    if (stepValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(stepValue->type)) {
        nextStep = stepValue->value.nativeObject.nativeInt64 + 1;
    }

    ZrCore_Value_InitAsInt(state, &storedStep, nextStep);
    set_object_field_cstring(state, iteratorObject, "__iter_step", &storedStep);

    if (nextStep == 0) {
        ZrCore_Value_InitAsInt(state, &currentValue, 10);
        set_object_field_cstring(state, iteratorObject, "__iter_current", &currentValue);
        ZrCore_Value_InitAsBool(state, ZrCore_Stack_GetValue(base), ZR_TRUE);
    } else if (nextStep == 1) {
        ZrCore_Value_InitAsInt(state, &currentValue, 20);
        set_object_field_cstring(state, iteratorObject, "__iter_current", &currentValue);
        ZrCore_Value_InitAsBool(state, ZrCore_Stack_GetValue(base), ZR_TRUE);
    } else {
        ZrCore_Value_ResetAsNull(&currentValue);
        set_object_field_cstring(state, iteratorObject, "__iter_current", &currentValue);
        ZrCore_Value_InitAsBool(state, ZrCore_Stack_GetValue(base), ZR_FALSE);
    }

    state->stackTop.valuePointer = base + 1;
    return 1;
}

static TZrInt64 test_iter_contract_current_native(struct SZrState *state) {
    TZrStackValuePointer base;
    SZrTypeValue *iteratorValue;
    SZrObject *iteratorObject;
    const SZrTypeValue *currentValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    base = state->callInfoList->functionBase.valuePointer;
    iteratorValue = ZrCore_Stack_GetValue(base + 1);
    if (iteratorValue == ZR_NULL || iteratorValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    iteratorObject = ZR_CAST_OBJECT(state, iteratorValue->value.object);
    currentValue = get_object_field_cstring(state, iteratorObject, "__iter_current");
    if (currentValue == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    } else {
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), currentValue);
    }
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 创建指令的辅助函数（内联定义，不依赖parser模块）
static TZrInstruction create_instruction_0(EZrInstructionCode opcode, TZrUInt16 operandExtra) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = 0;
    return instruction;
}

static TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operand) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

static TZrInstruction create_instruction_2(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operand1,
                                           TZrUInt16 operand2) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand1;
    instruction.instruction.operand.operand1[1] = operand2;
    return instruction;
}

// 获取指令名称的辅助函数
static const char *get_instruction_name(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
            return "GET_STACK";
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            return "SET_STACK";
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return "GET_CONSTANT";
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            return "SET_CONSTANT";
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            return "GET_CLOSURE";
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            return "SET_CLOSURE";
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
            return "GETUPVAL";
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
            return "SETUPVAL";
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            return "GET_MEMBER";
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            return "SET_MEMBER";
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            return "GET_MEMBER_SLOT";
        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
            return "SET_MEMBER_SLOT";
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
            return "GET_BY_INDEX";
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
            return "SET_BY_INDEX";
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
            return "ITER_INIT";
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
            return "ITER_MOVE_NEXT";
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
            return "ITER_CURRENT";
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
            return "TO_BOOL";
        case ZR_INSTRUCTION_ENUM(TO_INT):
            return "TO_INT";
        case ZR_INSTRUCTION_ENUM(TO_UINT):
            return "TO_UINT";
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            return "TO_FLOAT";
        case ZR_INSTRUCTION_ENUM(TO_STRING):
            return "TO_STRING";
        case ZR_INSTRUCTION_ENUM(ADD):
            return "ADD";
        case ZR_INSTRUCTION_ENUM(ADD_INT):
            return "ADD_INT";
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
            return "ADD_FLOAT";
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
            return "ADD_STRING";
        case ZR_INSTRUCTION_ENUM(SUB):
            return "SUB";
        case ZR_INSTRUCTION_ENUM(SUB_INT):
            return "SUB_INT";
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
            return "SUB_FLOAT";
        case ZR_INSTRUCTION_ENUM(MUL):
            return "MUL";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            return "MUL_SIGNED";
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            return "MUL_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
            return "MUL_FLOAT";
        case ZR_INSTRUCTION_ENUM(NEG):
            return "NEG";
        case ZR_INSTRUCTION_ENUM(DIV):
            return "DIV";
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            return "DIV_SIGNED";
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            return "DIV_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
            return "DIV_FLOAT";
        case ZR_INSTRUCTION_ENUM(MOD):
            return "MOD";
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return "MOD_SIGNED";
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            return "MOD_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            return "MOD_FLOAT";
        case ZR_INSTRUCTION_ENUM(POW):
            return "POW";
        case ZR_INSTRUCTION_ENUM(POW_SIGNED):
            return "POW_SIGNED";
        case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
            return "POW_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(POW_FLOAT):
            return "POW_FLOAT";
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
            return "SHIFT_LEFT";
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
            return "SHIFT_LEFT_INT";
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
            return "SHIFT_RIGHT";
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
            return "SHIFT_RIGHT_INT";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
            return "LOGICAL_NOT";
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
            return "LOGICAL_AND";
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
            return "LOGICAL_OR";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
            return "LOGICAL_GREATER_SIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
            return "LOGICAL_GREATER_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
            return "LOGICAL_GREATER_FLOAT";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
            return "LOGICAL_LESS_SIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
            return "LOGICAL_LESS_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
            return "LOGICAL_LESS_FLOAT";
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            return "LOGICAL_EQUAL";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            return "LOGICAL_NOT_EQUAL";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
            return "LOGICAL_GREATER_EQUAL_SIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
            return "LOGICAL_GREATER_EQUAL_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            return "LOGICAL_GREATER_EQUAL_FLOAT";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
            return "LOGICAL_LESS_EQUAL_SIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
            return "LOGICAL_LESS_EQUAL_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
            return "LOGICAL_LESS_EQUAL_FLOAT";
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            return "BITWISE_NOT";
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
            return "BITWISE_AND";
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
            return "BITWISE_OR";
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            return "BITWISE_XOR";
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
            return "BITWISE_SHIFT_LEFT";
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            return "BITWISE_SHIFT_RIGHT";
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            return "FUNCTION_CALL";
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            return "FUNCTION_TAIL_CALL";
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            return "FUNCTION_RETURN";
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            return "SUPER_FUNCTION_CALL_NO_ARGS";
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
            return "SUPER_DYN_CALL_NO_ARGS";
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
            return "SUPER_META_CALL_NO_ARGS";
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
            return "SUPER_DYN_CALL_CACHED";
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
            return "SUPER_META_CALL_CACHED";
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            return "SUPER_FUNCTION_TAIL_CALL_NO_ARGS";
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
            return "SUPER_DYN_TAIL_CALL_NO_ARGS";
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
            return "SUPER_META_TAIL_CALL_NO_ARGS";
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
            return "SUPER_DYN_TAIL_CALL_CACHED";
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return "SUPER_META_TAIL_CALL_CACHED";
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
            return "SUPER_META_GET_CACHED";
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
            return "SUPER_META_SET_CACHED";
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
            return "SUPER_META_GET_STATIC_CACHED";
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            return "SUPER_META_SET_STATIC_CACHED";
        case ZR_INSTRUCTION_ENUM(META_GET):
            return "META_GET";
        case ZR_INSTRUCTION_ENUM(META_SET):
            return "META_SET";
        case ZR_INSTRUCTION_ENUM(JUMP):
            return "JUMP";
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            return "JUMP_IF";
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            return "JUMP_IF_GREATER_SIGNED";
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            return "CREATE_CLOSURE";
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            return "CREATE_OBJECT";
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            return "CREATE_ARRAY";
        case ZR_INSTRUCTION_ENUM(TRY):
            return "TRY";
        case ZR_INSTRUCTION_ENUM(THROW):
            return "THROW";
        case ZR_INSTRUCTION_ENUM(CATCH):
            return "CATCH";
        default:
            return "UNKNOWN";
    }
}

// 打印指令的辅助函数（用于调试）
static void print_instruction(const char *label, TZrInstruction *inst, TZrSize index) {
    EZrInstructionCode opcode = (EZrInstructionCode) inst->instruction.operationCode;
    TZrUInt16 operandExtra = inst->instruction.operandExtra;
    ZR_UNUSED_PARAMETER(label);

    printf("  [%zu] ", index);

    // 打印指令名称
    printf("%s (extra=%u", get_instruction_name(opcode), operandExtra);

    // 根据指令类型打印操作数
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(TRY):
            printf(", operand2[0]=%d", inst->instruction.operand.operand2[0]);
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            printf(", left=%u, right=%u, jump_offset=%d",
                   inst->instruction.operandExtra,
                   inst->instruction.operand.operand1[0],
                   (TZrInt16)inst->instruction.operand.operand1[1]);
            break;
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(META_GET):
        case ZR_INSTRUCTION_ENUM(META_SET):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            printf(", operand1[0]=%u, operand1[1]=%u", inst->instruction.operand.operand1[0],
                   inst->instruction.operand.operand1[1]);
            break;
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
            printf(", operand1[0]=%u", inst->instruction.operand.operand1[0]);
            break;
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
        case ZR_INSTRUCTION_ENUM(NEG):
            // 只有operandExtra
            break;
        default:
            printf(", operand1[0]=%u, operand1[1]=%u, operand2[0]=%d", inst->instruction.operand.operand1[0],
                   inst->instruction.operand.operand1[1], inst->instruction.operand.operand2[0]);
            break;
    }
    printf(")\n");
}

// 打印指令列表的辅助函数
static void print_instructions(const char *label, TZrInstruction *instructions, TZrSize instructionCount) {
    printf("=== %s: Generated Instructions (%zu) ===\n", label, instructionCount);
    for (TZrSize i = 0; i < instructionCount; i++) {
        print_instruction(label, &instructions[i], i);
    }
    printf("========================================\n");
    fflush(stdout);
}

// 创建测试函数并执行的辅助函数
static SZrFunction *create_test_function(SZrState *state, TZrInstruction *instructions, TZrSize instructionCount,
                                       SZrTypeValue *constants, TZrSize constantCount, TZrSize stackSize) {
    SZrFunction *function = ZrCore_Function_New(state);
    if (!function)
        return ZR_NULL;

    SZrGlobalState *global = state->global;

    // 设置指令列表
    if (instructionCount > 0) {
        TZrSize instSize = instructionCount * sizeof(TZrInstruction);
        function->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (!function->instructionsList) {
            ZrCore_Function_Free(state, function);
            return ZR_NULL;
        }
        memcpy(function->instructionsList, instructions, instSize);
        function->instructionsLength = (TZrUInt32) instructionCount;
    }

    // 设置常量列表
    if (constantCount > 0) {
        TZrSize constSize = constantCount * sizeof(SZrTypeValue);
        function->constantValueList =
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (!function->constantValueList) {
            ZrCore_Function_Free(state, function);
            return ZR_NULL;
        }
        memcpy(function->constantValueList, constants, constSize);
        function->constantValueLength = (TZrUInt32) constantCount;
    }

    function->stackSize = (TZrUInt32) stackSize;
    function->parameterCount = 0;
    function->hasVariableArguments = ZR_FALSE;

    return function;
}

// 执行测试函数的辅助函数
static TZrBool execute_test_function(SZrState *state, SZrFunction *function) {
    // 创建闭包
    SZrClosure *closure = ZrCore_Closure_New(state, 0);
    if (!closure)
        return ZR_FALSE;
    closure->function = function;

    // 准备栈
    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = base + 1 + function->stackSize;

    // 创建CallInfo
    SZrCallInfo *callInfo = ZrCore_CallInfo_Extend(state);
    if (!callInfo)
        return ZR_FALSE;

    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1;

    state->callInfoList = callInfo;
    // 确保线程状态为正常
    state->threadStatus = ZR_THREAD_STATUS_FINE;

    // 执行函数
    ZrCore_Execute(state, callInfo);

    // 检查状态
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

// 测试初始化和清理
void setUp(void) {}

void tearDown(void) {}

// ==================== 栈操作指令测试 ====================

static void test_get_stack(void) {
    TEST_START("GET_STACK Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建测试函数：将栈槽0的值复制到栈槽1
    // GET_CONSTANT 0 -> stack[0] (值为42)
    // GET_STACK 1, 0 (将stack[0]复制到stack[1])
    // FUNCTION_RETURN 1, 1, 0 (返回1个值，从stack[1]返回)
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 42);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), 1, 0); // dest=1, src=0
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 1,
                                           0); // returnCount=1, resultSlot=1, variableArgs=0

    print_instructions("test_get_stack", instructions, 3);

    SZrFunction *function = create_test_function(state, instructions, 3, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（返回值在 resultSlot=1 位置，即 base + 2）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(42, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_STACK Instruction");
    TEST_DIVIDER();
}

static void test_set_stack(void) {
    TEST_START("SET_STACK Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建测试函数：设置栈槽1的值为100
    // GET_CONSTANT 0 -> stack[0] (值为100)
    // SET_STACK 1, 0 (将stack[0]的值复制到stack[1])
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 100);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] =
            create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 1, 0); // E=1, A2=0 (将stack[0]的值复制到stack[1])

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *source = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *destination = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(source->type));
    TEST_ASSERT_EQUAL_INT64(100, source->value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(destination->type));
    TEST_ASSERT_EQUAL_INT64(100, destination->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_STACK Instruction");
    TEST_DIVIDER();
}

// ==================== 常量操作指令测试 ====================

static void test_get_constant(void) {
    TEST_START("GET_CONSTANT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建测试函数：从常量池获取常量
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 123);
    ZrCore_Value_InitAsInt(state, &constants[1], 456);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1

    SZrFunction *function = create_test_function(state, instructions, 2, constants, 2, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（dest=0 在 base+1, dest=1 在 base+2）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result0 = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *result1 = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result0->type));
    TEST_ASSERT_EQUAL_INT64(123, result0->value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result1->type));
    TEST_ASSERT_EQUAL_INT64(456, result1->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_CONSTANT Instruction");
    TEST_DIVIDER();
}

// ==================== 类型转换指令测试 ====================

static void test_to_bool(void) {
    TEST_START("TO_BOOL Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：将整数1转换为布尔值
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 1);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(TO_BOOL), 1, 0, 0); // dest=1, src=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（dest=1 在 base+2）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "TO_BOOL Instruction");
    TEST_DIVIDER();
}

static void test_to_int(void) {
    TEST_START("TO_INT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：将浮点数3.14转换为整数
    SZrTypeValue constant;
    ZrCore_Value_InitAsFloat(state, &constant, 3.14);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(TO_INT), 1, 0, 0); // dest=1, src=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(3, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "TO_INT Instruction");
    TEST_DIVIDER();
}

static void test_to_string(void) {
    TEST_START("TO_STRING Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：将整数42转换为字符串
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 42);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(TO_STRING), 1, 0, 0); // dest=1, src=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_STRING(result->type));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "TO_STRING Instruction");
    TEST_DIVIDER();
}

// ==================== 算术运算指令测试 ====================

static void test_add_int(void) {
    TEST_START("ADD_INT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：10 + 20 = 30
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 10);
    ZrCore_Value_InitAsInt(state, &constants[1], 20);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (10)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (20)
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD_INT), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(30, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "ADD_INT Instruction");
    TEST_DIVIDER();
}

static void test_sub_int(void) {
    TEST_START("SUB_INT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：20 - 10 = 10
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 20);
    ZrCore_Value_InitAsInt(state, &constants[1], 10);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (20)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (10)
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUB_INT), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(10, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUB_INT Instruction");
    TEST_DIVIDER();
}

static void test_mul_signed(void) {
    TEST_START("MUL_SIGNED Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：6 * 7 = 42
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 6);
    ZrCore_Value_InitAsInt(state, &constants[1], 7);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (6)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (7)
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL_SIGNED), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(42, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MUL_SIGNED Instruction");
    TEST_DIVIDER();
}


static void test_logical_not(void) {
    TEST_START("LOGICAL_NOT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：!true = false
    SZrTypeValue constant;
    ZR_VALUE_FAST_SET(&constant, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_NOT), 1, 0, 0); // dest=1, src=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_FALSE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_NOT Instruction");
    TEST_DIVIDER();
}

static void test_to_uint(void) {
    TEST_START("TO_UINT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：将整数42转换为无符号整数
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 42);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(TO_UINT), 1, 0, 0); // dest=1, src=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_UNSIGNED_INT(result->type));
    TEST_ASSERT_EQUAL_UINT64(42, result->value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "TO_UINT Instruction");
    TEST_DIVIDER();
}

static void test_to_float(void) {
    TEST_START("TO_FLOAT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：将整数42转换为浮点数
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 42);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(TO_FLOAT), 1, 0, 0); // dest=1, src=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result->type));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 42.0, result->value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "TO_FLOAT Instruction");
    TEST_DIVIDER();
}

static void test_add_float(void) {
    TEST_START("ADD_FLOAT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：1.5 + 2.5 = 4.0
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsFloat(state, &constants[0], 1.5);
    ZrCore_Value_InitAsFloat(state, &constants[1], 2.5);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (1.5)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (2.5)
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD_FLOAT), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result->type));
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.0, result->value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "ADD_FLOAT Instruction");
    TEST_DIVIDER();
}

static void test_add_string(void) {
    TEST_START("ADD_STRING Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：字符串连接 "hello" + "world" = "helloworld"
    SZrTypeValue constants[2];
    SZrString *str1 = ZrCore_String_CreateFromNative(state, "hello");
    SZrString *str2 = ZrCore_String_CreateFromNative(state, "world");
    ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(str1));
    ZrCore_Value_InitAsRawObject(state, &constants[1], ZR_CAST_RAW_OBJECT_AS_SUPER(str2));

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD_STRING), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_STRING(result->type));
    SZrString *resultStr = ZR_CAST_STRING(state, result->value.object);
    TZrNativeString resultNative = ZrCore_String_GetNativeString(resultStr);
    TEST_ASSERT_EQUAL_STRING("helloworld", resultNative);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "ADD_STRING Instruction");
    TEST_DIVIDER();
}

static void test_div_signed(void) {
    TEST_START("DIV_SIGNED Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：20 / 4 = 5
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 20);
    ZrCore_Value_InitAsInt(state, &constants[1], 4);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (20)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (4)
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(DIV_SIGNED), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(5, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "DIV_SIGNED Instruction");
    TEST_DIVIDER();
}

static void test_mod_signed(void) {
    TEST_START("MOD_SIGNED Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：17 % 5 = 2
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 17);
    ZrCore_Value_InitAsInt(state, &constants[1], 5);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (17)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (5)
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MOD_SIGNED), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(2, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MOD_SIGNED Instruction");
    TEST_DIVIDER();
}

static void test_logical_and(void) {
    TEST_START("LOGICAL_AND Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：true && false = false
    SZrTypeValue constants[2];
    ZR_VALUE_FAST_SET(&constants[0], nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    ZR_VALUE_FAST_SET(&constants[1], nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_AND), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_FALSE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_AND Instruction");
    TEST_DIVIDER();
}

static void test_logical_or(void) {
    TEST_START("LOGICAL_OR Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：true || false = true
    SZrTypeValue constants[2];
    ZR_VALUE_FAST_SET(&constants[0], nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    ZR_VALUE_FAST_SET(&constants[1], nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_OR), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_OR Instruction");
    TEST_DIVIDER();
}

static void test_logical_equal(void) {
    TEST_START("LOGICAL_EQUAL Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：5 == 5 = true
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 5);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_EQUAL Instruction");
    TEST_DIVIDER();
}

static void test_logical_greater_signed(void) {
    TEST_START("LOGICAL_GREATER_SIGNED Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：10 > 5 = true
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 10);
    ZrCore_Value_InitAsInt(state, &constants[1], 5);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] =
            create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_GREATER_SIGNED Instruction");
    TEST_DIVIDER();
}

static void test_bitwise_and(void) {
    TEST_START("BITWISE_AND Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：5 & 3 = 1 (二进制: 101 & 011 = 001)
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 3);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_AND), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(1, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "BITWISE_AND Instruction");
    TEST_DIVIDER();
}

static void test_bitwise_or(void) {
    TEST_START("BITWISE_OR Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：5 | 3 = 7 (二进制: 101 | 011 = 111)
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 3);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_OR), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(7, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "BITWISE_OR Instruction");
    TEST_DIVIDER();
}

static void test_bitwise_xor(void) {
    TEST_START("BITWISE_XOR Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：5 ^ 3 = 6 (二进制: 101 ^ 011 = 110)
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 3);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_XOR), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(6, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "BITWISE_XOR Instruction");
    TEST_DIVIDER();
}

static void test_bitwise_not(void) {
    TEST_START("BITWISE_NOT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：~5 (假设是8位，~00000101 = 11111010 = -6 in two's complement)
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 5);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_NOT), 1, 0, 0); // dest=1, src=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果 (~5 = -6 in two's complement)
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(-6, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "BITWISE_NOT Instruction");
    TEST_DIVIDER();
}

static void test_bitwise_shift_left(void) {
    TEST_START("BITWISE_SHIFT_LEFT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：5 << 2 = 20
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 2);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(20, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "BITWISE_SHIFT_LEFT Instruction");
    TEST_DIVIDER();
}

static void test_bitwise_shift_right(void) {
    TEST_START("BITWISE_SHIFT_RIGHT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：20 >> 2 = 5
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 20);
    ZrCore_Value_InitAsInt(state, &constants[1], 2);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT), 2, 0, 1); // dest=2, opA=0, opB=1

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(5, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "BITWISE_SHIFT_RIGHT Instruction");
    TEST_DIVIDER();
}

static void test_create_object(void) {
    TEST_START("CREATE_OBJECT Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：创建空对象
    TZrInstruction instructions[1];
    instructions[0] = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), 0); // dest=0

    SZrFunction *function = create_test_function(state, instructions, 1, ZR_NULL, 0, 2);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 1);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_OBJECT(result->type));
    SZrObject *object = ZR_CAST_OBJECT(state, result->value.object);
    TEST_ASSERT_NOT_NULL(object);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "CREATE_OBJECT Instruction");
    TEST_DIVIDER();
}

static void test_create_array(void) {
    TEST_START("CREATE_ARRAY Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：创建空数组
    TZrInstruction instructions[1];
    instructions[0] = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_ARRAY), 0); // dest=0

    SZrFunction *function = create_test_function(state, instructions, 1, ZR_NULL, 0, 2);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 1);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_ARRAY(result->type));
    SZrObject *array = ZR_CAST_OBJECT(state, result->value.object);
    TEST_ASSERT_NOT_NULL(array);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "CREATE_ARRAY Instruction");
    TEST_DIVIDER();
}

// ==================== 表操作指令测试 ====================

static void test_get_by_index(void) {
    TEST_START("GET_BY_INDEX Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：创建对象，设置键值对，然后获取值
    // CREATE_OBJECT -> stack[0]
    // GET_CONSTANT "key" -> stack[1]
    // GET_CONSTANT 42 -> stack[2]
    // SET_BY_INDEX stack[0], stack[1], stack[2]
    // GET_BY_INDEX stack[3], stack[0], stack[1]
    SZrString *keyStr = ZrCore_String_CreateFromNative(state, "key");
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(keyStr));
    ZrCore_Value_InitAsInt(state, &constants[1], 42);

    TZrInstruction instructions[5];
    instructions[0] = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), 0); // dest=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 0); // dest=1, const=0 ("key")
    instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 1); // dest=2, const=1 (42)
    instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_BY_INDEX), 2, 0, 1); // value=2, receiver=0, key=1
    instructions[4] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_BY_INDEX), 3, 0, 1); // dest=3, receiver=0, key=1

    SZrFunction *function = create_test_function(state, instructions, 5, constants, 2, 5);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（dest=3 在 base+4）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 4);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(42, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_BY_INDEX Instruction");
    TEST_DIVIDER();
}

static void test_set_by_index(void) {
    TEST_START("SET_BY_INDEX Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：创建对象，设置键值对
    SZrString *keyStr = ZrCore_String_CreateFromNative(state, "value");
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(keyStr));
    ZrCore_Value_InitAsInt(state, &constants[1], 100);

    TZrInstruction instructions[4];
    instructions[0] = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), 0); // dest=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 0); // dest=1, const=0 ("value")
    instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 1); // dest=2, const=1 (100)
    instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_BY_INDEX), 2, 0, 1); // value=2, receiver=0, key=1

    SZrFunction *function = create_test_function(state, instructions, 4, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证：通过 index contract 获取值
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *tableValue = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *keyValue = ZrCore_Stack_GetValue(base + 2);
    SZrTypeValue result;
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndex(state, tableValue, keyValue, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(100, result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_BY_INDEX Instruction");
    TEST_DIVIDER();
}

static void test_get_member_uses_property_descriptor(void) {
    TEST_START("GET_MEMBER Property Descriptor");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "ContractBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrObject *instance;
        SZrTypeValue receiver;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(prototype);

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_PROPERTY;
        descriptor.getterFunction = create_native_callable(state, test_member_property_getter_native);
        TEST_ASSERT_NOT_NULL(descriptor.getterFunction);
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        receiver.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_ResetAsNull(&result);

        TEST_ASSERT_TRUE(ZrCore_Object_GetMember(state, &receiver, memberName, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(77, result.value.nativeObject.nativeInt64);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER Property Descriptor");
    TEST_DIVIDER();
}

static void test_builtin_array_length_member_uses_native_contract(void) {
    TEST_START("GET_MEMBER Builtin Array Length");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrObject *array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
        SZrTypeValue receiver;
        SZrTypeValue key;
        SZrTypeValue inputValue;
        SZrTypeValue result;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "length");

        TEST_ASSERT_NOT_NULL(array);
        TEST_ASSERT_NOT_NULL(memberName);
        ZrCore_Object_Init(state, array);

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
        receiver.type = ZR_VALUE_TYPE_ARRAY;

        ZrCore_Value_InitAsInt(state, &key, 0);
        ZrCore_Value_InitAsInt(state, &inputValue, 11);
        TEST_ASSERT_TRUE(ZrCore_Object_SetByIndex(state, &receiver, &key, &inputValue));

        ZrCore_Value_InitAsInt(state, &key, 1);
        ZrCore_Value_InitAsInt(state, &inputValue, 22);
        TEST_ASSERT_TRUE(ZrCore_Object_SetByIndex(state, &receiver, &key, &inputValue));

        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_TRUE(ZrCore_Object_GetMember(state, &receiver, memberName, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(2, result.value.nativeObject.nativeInt64);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER Builtin Array Length");
    TEST_DIVIDER();
}

static void test_meta_get_and_meta_set_instructions_dispatch_hidden_accessors(void) {
    TEST_START("META_GET And META_SET Instructions");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "MetaBox");
        SZrString *hiddenGetterName = ZrCore_String_CreateFromNative(state, "__get_value");
        SZrString *hiddenSetterName = ZrCore_String_CreateFromNative(state, "__set_value");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObject *instance;
        SZrTypeValue functionValue;
        SZrTypeValue storedValue;
        SZrTypeValue constants[3];
        TZrInstruction instructions[5];
        SZrFunction *function;
        TZrBool success;
        TZrStackValuePointer base;
        const SZrTypeValue *updatedValue;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(hiddenGetterName);
        TEST_ASSERT_NOT_NULL(hiddenSetterName);
        TEST_ASSERT_NOT_NULL(prototype);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_hidden_meta_getter_native)));
        set_object_field_cstring(state, &prototype->super, "__get_value", &functionValue);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_hidden_meta_setter_native)));
        set_object_field_cstring(state, &prototype->super, "__set_value", &functionValue);

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        ZrCore_Value_InitAsInt(state, &storedValue, 5);
        set_object_field_cstring(state, instance, "__meta_value", &storedValue);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        constants[1] = constants[0];
        ZrCore_Value_InitAsInt(state, &constants[2], 13);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(META_GET), 0, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_2(ZR_INSTRUCTION_ENUM(META_SET), 1, 2, 1);

        function = create_test_function(state, instructions, 5, constants, 3, 3);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry) * 2);
        function->memberEntries[0].symbol = hiddenGetterName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntries[1].symbol = hiddenSetterName;
        function->memberEntries[1].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 2;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);

        base = state->callInfoList->functionBase.valuePointer;
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(ZrCore_Stack_GetValue(base + 1)->type));
        TEST_ASSERT_EQUAL_INT64(5, ZrCore_Stack_GetValue(base + 1)->value.nativeObject.nativeInt64);
        updatedValue = get_object_field_cstring(state, instance, "__meta_value");
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(ZrCore_Stack_GetValue(base + 2)->type));
        TEST_ASSERT_EQUAL_INT64(13, ZrCore_Stack_GetValue(base + 2)->value.nativeObject.nativeInt64);

        TEST_ASSERT_NOT_NULL(updatedValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(updatedValue->type));
        TEST_ASSERT_EQUAL_INT64(13, updatedValue->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "META_GET And META_SET Instructions");
    TEST_DIVIDER();
}

static void test_super_meta_get_and_meta_set_cached_instructions_fill_and_hit_callsite_cache(void) {
    TEST_START("SUPER_META_GET_CACHED And SUPER_META_SET_CACHED Instructions");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "CachedMetaBox");
        SZrString *hiddenGetterName = ZrCore_String_CreateFromNative(state, "__get_value");
        SZrString *hiddenSetterName = ZrCore_String_CreateFromNative(state, "__set_value");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObject *instance;
        SZrTypeValue functionValue;
        SZrTypeValue storedValue;
        SZrTypeValue constants[2];
        TZrInstruction instructions[9];
        SZrFunction *function;
        TZrBool success;
        const SZrTypeValue *updatedValue;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(hiddenGetterName);
        TEST_ASSERT_NOT_NULL(hiddenSetterName);
        TEST_ASSERT_NOT_NULL(prototype);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_hidden_meta_getter_native)));
        set_object_field_cstring(state, &prototype->super, "__get_value", &functionValue);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_hidden_meta_setter_native)));
        set_object_field_cstring(state, &prototype->super, "__set_value", &functionValue);

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        ZrCore_Value_InitAsInt(state, &storedValue, 5);
        set_object_field_cstring(state, instance, "__meta_value", &storedValue);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[1], 13);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED), 0, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 0);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED), 1, 1, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 0);
        instructions[5] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 1);
        instructions[6] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED), 2, 3, 1);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 0);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED), 2, 3, 1);

        function = create_test_function(state, instructions, 9, constants, 2, 4);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry) * 2);
        function->memberEntries[0].symbol = hiddenGetterName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntries[1].symbol = hiddenSetterName;
        function->memberEntries[1].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 2;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry) * 2);
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCaches[1].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET;
        function->callSiteCaches[1].instructionIndex = 6;
        function->callSiteCaches[1].memberEntryIndex = 1;
        function->callSiteCacheLength = 2;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_TRUE(function->callSiteCaches[0].runtimeMissCount > 0);
        TEST_ASSERT_TRUE(function->callSiteCaches[1].runtimeMissCount > 0);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedFunction);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[1].picSlots[0].cachedFunction);
        TEST_ASSERT_TRUE(function->callSiteCaches[0].runtimeHitCount > 0);
        TEST_ASSERT_TRUE(function->callSiteCaches[1].runtimeHitCount > 0);

        updatedValue = get_object_field_cstring(state, instance, "__meta_value");
        TEST_ASSERT_NOT_NULL(updatedValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(updatedValue->type));
        TEST_ASSERT_EQUAL_INT64(13, updatedValue->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUPER_META_GET_CACHED And SUPER_META_SET_CACHED Instructions");
    TEST_DIVIDER();
}

static void test_get_member_slot_and_set_member_slot_instructions_fill_and_hit_callsite_cache(void) {
    TEST_START("GET_MEMBER_SLOT And SET_MEMBER_SLOT Instructions");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "CachedFieldBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrObject *instance;
        SZrTypeValue constants[3];
        TZrInstruction instructions[10];
        SZrFunction *function;
        TZrBool success;
        const SZrTypeValue *updatedValue;
        TZrStackValuePointer base;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(prototype);

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        ZrCore_Value_InitAsInt(state, &constants[0], 5);
        set_object_field_cstring(state, instance, "value", &constants[0]);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[1], 13);
        ZrCore_Value_InitAsInt(state, &constants[2], 21);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 1);
        instructions[4] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 1);
        instructions[5] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 0);
        instructions[6] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 5, 4, 0);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 0);
        instructions[8] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 7, 2);
        instructions[9] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 7, 6, 1);

        function = create_test_function(state, instructions, 10, constants, 3, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry) * 2);
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCaches[1].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[1].instructionIndex = 4;
        function->callSiteCaches[1].memberEntryIndex = 0;
        function->callSiteCacheLength = 2;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);

        base = state->callInfoList->functionBase.valuePointer;
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(ZrCore_Stack_GetValue(base + 2)->type));
        TEST_ASSERT_EQUAL_INT64(5, ZrCore_Stack_GetValue(base + 2)->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(ZrCore_Stack_GetValue(base + 6)->type));
        TEST_ASSERT_EQUAL_INT64(13, ZrCore_Stack_GetValue(base + 6)->value.nativeObject.nativeInt64);

        TEST_ASSERT_TRUE(function->callSiteCaches[0].runtimeMissCount > 0);
        TEST_ASSERT_TRUE(function->callSiteCaches[1].runtimeMissCount > 0);
        TEST_ASSERT_TRUE(function->callSiteCaches[0].runtimeHitCount > 0);
        TEST_ASSERT_TRUE(function->callSiteCaches[1].runtimeHitCount > 0);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_UINT32(0, function->callSiteCaches[0].picSlots[0].cachedDescriptorIndex);
        TEST_ASSERT_EQUAL_UINT32(0, function->callSiteCaches[1].picSlots[0].cachedDescriptorIndex);

        updatedValue = get_object_field_cstring(state, instance, "value");
        TEST_ASSERT_NOT_NULL(updatedValue);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(updatedValue->type));
        TEST_ASSERT_EQUAL_INT64(21, updatedValue->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT And SET_MEMBER_SLOT Instructions");
    TEST_DIVIDER();
}

static void test_get_member_slot_instruction_records_remembered_set_for_young_receiver_metadata_even_with_permanent_owner(
        void) {
    TEST_START("GET_MEMBER_SLOT Remembered Young Receiver Metadata");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "RememberedFieldBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrObject *instance;
        SZrTypeValue constants[1];
        TZrInstruction instructions[3];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(prototype);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        {
            SZrTypeValue initialFieldValue;
            ZrCore_Value_InitAsInt(state, &initialFieldValue, 0);
            set_object_field_cstring(state, instance, "value", &initialFieldValue);
        }

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 2, 0, 1);

        function = create_test_function(state, instructions, 3, constants, 1, 3);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry) * 2);
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCaches[1].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[1].instructionIndex = 2;
        function->callSiteCaches[1].memberEntryIndex = 0;
        function->callSiteCacheLength = 2;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)->garbageCollectMark.regionKind);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(instance, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(instance, function->callSiteCaches[1].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[1].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[1].picSlots[0].cachedMemberName);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT Remembered Young Receiver Metadata");
    TEST_DIVIDER();
}

static void test_get_member_slot_instruction_pic_replacement_keeps_single_remembered_entry_for_young_receiver_metadata(
        void) {
    TEST_START("GET_MEMBER_SLOT PIC Replacement Remembered Young Metadata");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObject *instanceC;
        SZrTypeValue constants[3];
        TZrInstruction instructions[6];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;

        init_object_constant_with_shared_field(
                state, "RememberedFieldBoxA", memberName, 11, &prototypeA, &instanceA, &constants[0]);
        init_object_constant_with_shared_field(
                state, "RememberedFieldBoxB", memberName, 22, &prototypeB, &instanceB, &constants[1]);
        init_object_constant_with_shared_field(
                state, "RememberedFieldBoxC", memberName, 33, &prototypeC, &instanceC, &constants[2]);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 1);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 2);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);

        function = create_test_function(state, instructions, 6, constants, 3, 2);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceB, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[1].cachedMemberName);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT PIC Replacement Remembered Young Metadata");
    TEST_DIVIDER();
}

static void test_get_member_slot_instruction_skips_remembered_set_when_cache_only_keeps_permanent_targets(void) {
    TEST_START("GET_MEMBER_SLOT Permanent Cache Targets");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "PermanentMethodBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "__get_shared");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrTypeValue constants[1];
        TZrInstruction instructions[3];
        SZrFunction *function;
        SZrFunction *callable;
        SZrRawObject *functionObject;
        SZrTypeValue functionValue;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(prototype);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
        descriptor.isStatic = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

        callable = create_native_callable(state, test_hidden_meta_static_getter_native);
        TEST_ASSERT_NOT_NULL(callable);
        ZrCore_Value_InitAsRawObject(state, &functionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(callable));
        {
            SZrTypeValue key;
            ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
            key.type = ZR_VALUE_TYPE_STRING;
            ZrCore_Object_SetValue(state, &prototype->super, &key, &functionValue);
        }

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 2, 0, 1);

        function = create_test_function(state, instructions, 3, constants, 1, 3);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry) * 2);
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCaches[1].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[1].instructionIndex = 2;
        function->callSiteCaches[1].memberEntryIndex = 0;
        function->callSiteCacheLength = 2;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)->garbageCollectMark.regionKind);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(callable, function->callSiteCaches[0].picSlots[0].cachedFunction);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_NULL(function->callSiteCaches[1].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(callable, function->callSiteCaches[1].picSlots[0].cachedFunction);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[1].picSlots[0].cachedMemberName);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT Permanent Cache Targets");
    TEST_DIVIDER();
}

static void test_get_member_slot_instruction_pic_replacement_stays_out_of_remembered_set_for_permanent_targets(void) {
    TEST_START("GET_MEMBER_SLOT PIC Replacement Permanent Targets");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "__get_shared");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrFunction *callableA;
        SZrFunction *callableB;
        SZrFunction *callableC;
        SZrTypeValue constants[3];
        TZrInstruction instructions[6];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_static_method_prototype_constant(
                state, "PermanentMethodBoxA", memberName, test_hidden_meta_static_getter_native, &prototypeA, &callableA, &constants[0]);
        init_static_method_prototype_constant(
                state, "PermanentMethodBoxB", memberName, test_hidden_meta_static_getter_native, &prototypeB, &callableB, &constants[1]);
        init_static_method_prototype_constant(
                state, "PermanentMethodBoxC", memberName, test_hidden_meta_static_getter_native, &prototypeC, &callableC, &constants[2]);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 1);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 2);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);

        function = create_test_function(state, instructions, 6, constants, 3, 2);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(callableC, function->callSiteCaches[0].picSlots[0].cachedFunction);
        TEST_ASSERT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(callableB, function->callSiteCaches[0].picSlots[1].cachedFunction);
        TEST_ASSERT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[1].cachedMemberName);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT PIC Replacement Permanent Targets");
    TEST_DIVIDER();
}

static void test_get_member_slot_instruction_minor_gc_prunes_remembered_set_after_replacing_young_targets_with_permanent_targets(
        void) {
    TEST_START("GET_MEMBER_SLOT Remembered Set Prunes After Permanent Replacement");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "shared");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrObjectPrototype *prototypeD;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrFunction *callableC;
        SZrFunction *callableD;
        SZrTypeValue constants[4];
        TZrInstruction instructions[8];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "RememberedReplaceYoungA", memberName, 11, &prototypeA, &instanceA, &constants[0]);
        init_object_constant_with_shared_field(
                state, "RememberedReplaceYoungB", memberName, 22, &prototypeB, &instanceB, &constants[1]);
        init_static_method_prototype_constant(
                state, "RememberedReplacePermanentC", memberName, test_hidden_meta_static_getter_native, &prototypeC, &callableC, &constants[2]);
        init_static_method_prototype_constant(
                state, "RememberedReplacePermanentD", memberName, test_hidden_meta_static_getter_native, &prototypeD, &callableD, &constants[3]);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 1);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 2);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 3);
        instructions[7] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);

        function = create_test_function(state, instructions, 8, constants, 4, 2);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(callableC, function->callSiteCaches[0].picSlots[0].cachedFunction);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(callableD, function->callSiteCaches[0].picSlots[1].cachedFunction);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        /*
         * The remembered entry was recorded when the PIC still held young receiver metadata.
         * Clear the old young constants so the next minor restart isolates stale-registry pruning.
         */
        ZrCore_Value_ResetAsNull(&function->constantValueList[0]);
        ZrCore_Value_ResetAsNull(&function->constantValueList[1]);

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT Remembered Set Prunes After Permanent Replacement");
    TEST_DIVIDER();
}

static void
test_get_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune(void) {
    TEST_START("GET_MEMBER_SLOT Minor GC Rewrites Forwarded Young Receiver Pair");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeC;
        SZrObjectPrototype *prototypeD;
        SZrObjectPrototype *prototypeE;
        SZrObject *instanceC;
        SZrObject *instanceD;
        SZrObject *instanceE;
        SZrTypeValue permanentReceiverCConstant;
        SZrTypeValue permanentReceiverDConstant;
        SZrTypeValue youngReceiverEConstant;
        SZrTypeValue initialConstants[4];
        TZrInstruction instructions[8];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrStackValuePointer base;
        TZrBool success;
        TZrPtr originalYoungReceiverAddress;
        SZrObject *forwardedInstanceE;
        SZrTypeValue *lastResult;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "ForwardedGetPermanentC", memberName, 3, &prototypeC, &instanceC, &permanentReceiverCConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceC));
        init_object_constant_with_shared_field(
                state, "ForwardedGetPermanentD", memberName, 4, &prototypeD, &instanceD, &permanentReceiverDConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceD));
        init_object_constant_with_shared_field(
                state, "ForwardedGetYoungE", memberName, 5, &prototypeE, &instanceE, &youngReceiverEConstant);

        initialConstants[0] = youngReceiverEConstant;
        initialConstants[1] = permanentReceiverDConstant;
        initialConstants[2] = youngReceiverEConstant;
        initialConstants[3] = permanentReceiverDConstant;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 1);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 3, 2, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 2);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 5, 4, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 3);
        instructions[7] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 7, 6, 0);

        function = create_test_function(state, instructions, 8, initialConstants, 4, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        lastResult = ZrCore_Stack_GetValue(base + 8);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(lastResult->type));
        TEST_ASSERT_EQUAL_INT64(4, lastResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeE, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        originalYoungReceiverAddress = (TZrPtr)instanceE;
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        forwardedInstanceE = ZR_CAST_OBJECT(state, function->constantValueList[0].value.object);
        TEST_ASSERT_NOT_NULL(forwardedInstanceE);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, ZR_CAST_OBJECT(state, function->constantValueList[2].value.object));
        TEST_ASSERT_TRUE((TZrPtr)forwardedInstanceE != originalYoungReceiverAddress);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.storageKind);
        TEST_ASSERT_TRUE((ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.escapeFlags &
                          ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.promotionReason);
        TEST_ASSERT_EQUAL_PTR(prototypeE, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(forwardedInstanceE->cachedStringLookupPair);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE->cachedStringLookupPair,
                              function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        lastResult = ZrCore_Stack_GetValue(base + 8);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(lastResult->type));
        TEST_ASSERT_EQUAL_INT64(4, lastResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(6u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE->cachedStringLookupPair,
                              function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[0] = permanentReceiverCConstant;
        function->constantValueList[1] = permanentReceiverDConstant;
        function->constantValueList[2] = permanentReceiverCConstant;
        function->constantValueList[3] = permanentReceiverDConstant;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        lastResult = ZrCore_Stack_GetValue(base + 8);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(lastResult->type));
        TEST_ASSERT_EQUAL_INT64(4, lastResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(3u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(9u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT Minor GC Rewrites Forwarded Young Receiver Pair");
    TEST_DIVIDER();
}

static void test_get_member_slot_instruction_exact_pair_hit_survives_receiver_rehash_and_minor_gc_prune(
        void) {
    TEST_START("GET_MEMBER_SLOT Exact Pair Hit Survives Rehash And Minor GC Prune");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeD;
        SZrObjectPrototype *prototypeE;
        SZrObject *instanceD;
        SZrObject *instanceE;
        SZrTypeValue permanentReceiverDConstant;
        SZrTypeValue youngReceiverEConstant;
        SZrTypeValue initialConstants[4];
        TZrInstruction instructions[8];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrStackValuePointer base;
        TZrBool success;
        SZrHashKeyValuePair *cachedPairBeforeRehash;
        SZrObject *forwardedInstanceE;
        SZrTypeValue *firstResult;
        SZrTypeValue *lastResult;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "RehashGetPermanentD", memberName, 4, &prototypeD, &instanceD, &permanentReceiverDConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceD));
        init_object_constant_with_shared_field(
                state, "RehashGetYoungE", memberName, 5, &prototypeE, &instanceE, &youngReceiverEConstant);

        initialConstants[0] = youngReceiverEConstant;
        initialConstants[1] = permanentReceiverDConstant;
        initialConstants[2] = youngReceiverEConstant;
        initialConstants[3] = permanentReceiverDConstant;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 1);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 3, 2, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 2);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 5, 4, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 3);
        instructions[7] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 7, 6, 0);

        function = create_test_function(state, instructions, 8, initialConstants, 4, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        firstResult = ZrCore_Stack_GetValue(base + 2);
        lastResult = ZrCore_Stack_GetValue(base + 8);
        TEST_ASSERT_NOT_NULL(firstResult);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(firstResult->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(lastResult->type));
        TEST_ASSERT_EQUAL_INT64(5, firstResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(4, lastResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_PTR(instanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);

        cachedPairBeforeRehash = function->callSiteCaches[0].picSlots[0].cachedReceiverPair;
        TEST_ASSERT_TRUE(add_named_fields_until_object_rehashes(state, instanceE, "rehash_get", 3000u, 32u));
        TEST_ASSERT_EQUAL_PTR(cachedPairBeforeRehash, function->callSiteCaches[0].picSlots[0].cachedReceiverPair);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        firstResult = ZrCore_Stack_GetValue(base + 2);
        lastResult = ZrCore_Stack_GetValue(base + 8);
        TEST_ASSERT_NOT_NULL(firstResult);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_EQUAL_INT64(5, firstResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(4, lastResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(6u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_PTR(cachedPairBeforeRehash, function->callSiteCaches[0].picSlots[0].cachedReceiverPair);

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        forwardedInstanceE = ZR_CAST_OBJECT(state, function->constantValueList[0].value.object);
        TEST_ASSERT_NOT_NULL(forwardedInstanceE);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(cachedPairBeforeRehash, function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        firstResult = ZrCore_Stack_GetValue(base + 2);
        lastResult = ZrCore_Stack_GetValue(base + 8);
        TEST_ASSERT_NOT_NULL(firstResult);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_EQUAL_INT64(5, firstResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(4, lastResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(10u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        firstResult = ZrCore_Stack_GetValue(base + 2);
        lastResult = ZrCore_Stack_GetValue(base + 8);
        TEST_ASSERT_NOT_NULL(firstResult);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_EQUAL_INT64(5, firstResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(4, lastResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(14u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT Exact Pair Hit Survives Rehash And Minor GC Prune");
    TEST_DIVIDER();
}

static void test_get_member_slot_instruction_does_not_reuse_cached_pair_for_different_receiver_same_prototype(
        void) {
    TEST_START("GET_MEMBER_SLOT Different Receiver Same Prototype Avoids Pair Reuse");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrString *sharedTypeName = ZrCore_String_CreateFromNative(state, "ShapeOnlyGetSharedBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *sharedPrototype = ZrCore_ObjectPrototype_New(state,
                                                                         sharedTypeName,
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObjectPrototype *otherPrototype;
        SZrObject *otherInstance;
        SZrTypeValue receiverAConstant;
        SZrTypeValue receiverBConstant;
        SZrTypeValue otherReceiverConstant;
        SZrTypeValue initialConstants[2];
        TZrInstruction instructions[4];
        SZrFunction *function;
        TZrBool success;
        TZrStackValuePointer base;
        SZrTypeValue *firstResult;
        SZrTypeValue *secondResult;

        TEST_ASSERT_NOT_NULL(sharedTypeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(sharedPrototype);

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, sharedPrototype, &descriptor));

        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 111, &instanceA, &receiverAConstant);
        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 222, &instanceB, &receiverBConstant);
        init_object_constant_with_shared_field(
                state, "ShapeOnlyGetOtherBox", memberName, 333, &otherPrototype, &otherInstance, &otherReceiverConstant);

        initialConstants[0] = receiverAConstant;
        initialConstants[1] = otherReceiverConstant;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 1);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 3, 2, 0);

        function = create_test_function(state, instructions, 4, initialConstants, 2, 4);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        firstResult = ZrCore_Stack_GetValue(base + 2);
        secondResult = ZrCore_Stack_GetValue(base + 4);
        TEST_ASSERT_NOT_NULL(firstResult);
        TEST_ASSERT_NOT_NULL(secondResult);
        TEST_ASSERT_EQUAL_INT64(111, firstResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(333, secondResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(sharedPrototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceA, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(otherPrototype, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        function->constantValueList[0] = receiverBConstant;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        firstResult = ZrCore_Stack_GetValue(base + 2);
        secondResult = ZrCore_Stack_GetValue(base + 4);
        TEST_ASSERT_NOT_NULL(firstResult);
        TEST_ASSERT_NOT_NULL(secondResult);
        TEST_ASSERT_EQUAL_INT64(222, firstResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(333, secondResult->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT Different Receiver Same Prototype Avoids Pair Reuse");
    TEST_DIVIDER();
}

static void
test_get_member_slot_instruction_same_prototype_safe_hit_refreshes_cached_receiver_before_minor_gc_prune(void) {
    TEST_START("GET_MEMBER_SLOT Same Prototype Safe Hit Refreshes PIC Receiver");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *sharedTypeName = ZrCore_String_CreateFromNative(state, "ShapeOnlyGetRefreshBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *sharedPrototype = ZrCore_ObjectPrototype_New(state,
                                                                         sharedTypeName,
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrTypeValue receiverAConstant;
        SZrTypeValue receiverBConstant;
        SZrTypeValue constants[1];
        TZrInstruction instructions[2];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;
        TZrStackValuePointer base;
        SZrTypeValue *result;

        TEST_ASSERT_NOT_NULL(sharedTypeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(sharedPrototype);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, sharedPrototype, &descriptor));
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(sharedPrototype));

        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 111, &instanceA, &receiverAConstant);
        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 222, &instanceB, &receiverBConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceB));

        constants[0] = receiverAConstant;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 1, 0, 0);

        function = create_test_function(state, instructions, 2, constants, 1, 2);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        result = ZrCore_Stack_GetValue(base + 2);
        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_EQUAL_INT64(111, result->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(sharedPrototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceA, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[0] = receiverBConstant;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        result = ZrCore_Stack_GetValue(base + 2);
        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_EQUAL_INT64(222, result->value.nativeObject.nativeInt64);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        base = state->callInfoList->functionBase.valuePointer;
        result = ZrCore_Stack_GetValue(base + 2);
        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_EQUAL_INT64(222, result->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_PTR(sharedPrototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceB, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(instanceB->cachedStringLookupPair);
        TEST_ASSERT_EQUAL_PTR(instanceB->cachedStringLookupPair,
                              function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_MEMBER_SLOT Same Prototype Safe Hit Refreshes PIC Receiver");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_records_remembered_set_for_young_receiver_metadata_even_with_permanent_owner(
        void) {
    TEST_START("SET_MEMBER_SLOT Remembered Young Receiver Metadata");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototype;
        SZrObject *instance;
        SZrTypeValue constants[3];
        TZrInstruction instructions[6];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;
        const SZrTypeValue *updatedValue;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;

        init_object_constant_with_shared_field(
                state, "RememberedFieldSetBox", memberName, 0, &prototype, &instance, &constants[0]);
        ZrCore_Value_InitAsInt(state, &constants[1], 13);
        ZrCore_Value_InitAsInt(state, &constants[2], 21);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 2);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 1);

        function = create_test_function(state, instructions, 6, constants, 3, 4);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry) * 2);
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCaches[1].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[1].instructionIndex = 5;
        function->callSiteCaches[1].memberEntryIndex = 0;
        function->callSiteCacheLength = 2;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(prototype)->garbageCollectMark.regionKind);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValue = get_object_field_cstring(state, instance, "value");
        TEST_ASSERT_NOT_NULL(updatedValue);
        TEST_ASSERT_EQUAL_INT64(21, updatedValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(instance, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(instance, function->callSiteCaches[1].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[1].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[1].picSlots[0].cachedMemberName);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Remembered Young Receiver Metadata");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_pic_replacement_keeps_single_remembered_entry_for_young_receiver_metadata(
        void) {
    TEST_START("SET_MEMBER_SLOT PIC Replacement Remembered Young Metadata");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObject *instanceC;
        SZrTypeValue constants[6];
        TZrInstruction instructions[9];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;
        const SZrTypeValue *updatedValueA;
        const SZrTypeValue *updatedValueB;
        const SZrTypeValue *updatedValueC;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;

        init_object_constant_with_shared_field(
                state, "RememberedFieldSetBoxA", memberName, 1, &prototypeA, &instanceA, &constants[0]);
        ZrCore_Value_InitAsInt(state, &constants[1], 101);
        init_object_constant_with_shared_field(
                state, "RememberedFieldSetBoxB", memberName, 2, &prototypeB, &instanceB, &constants[2]);
        ZrCore_Value_InitAsInt(state, &constants[3], 202);
        init_object_constant_with_shared_field(
                state, "RememberedFieldSetBoxC", memberName, 3, &prototypeC, &instanceC, &constants[4]);
        ZrCore_Value_InitAsInt(state, &constants[5], 303);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);

        function = create_test_function(state, instructions, 9, constants, 6, 6);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueA = get_object_field_cstring(state, instanceA, "value");
        updatedValueB = get_object_field_cstring(state, instanceB, "value");
        updatedValueC = get_object_field_cstring(state, instanceC, "value");
        TEST_ASSERT_NOT_NULL(updatedValueA);
        TEST_ASSERT_NOT_NULL(updatedValueB);
        TEST_ASSERT_NOT_NULL(updatedValueC);
        TEST_ASSERT_EQUAL_INT64(101, updatedValueA->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(202, updatedValueB->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(303, updatedValueC->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceB, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[1].cachedMemberName);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT PIC Replacement Remembered Young Metadata");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_pic_replacement_records_remembered_set_when_young_receiver_replaces_permanent_receivers(
        void) {
    TEST_START("SET_MEMBER_SLOT PIC Replacement Young Replaces Permanent Receivers");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObject *instanceC;
        SZrTypeValue constants[6];
        TZrInstruction instructions[9];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;
        const SZrTypeValue *updatedValueA;
        const SZrTypeValue *updatedValueB;
        const SZrTypeValue *updatedValueC;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "PermanentThenYoungSetReceiverA", memberName, 1, &prototypeA, &instanceA, &constants[0]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceA));
        ZrCore_Value_InitAsInt(state, &constants[1], 101);
        init_object_constant_with_shared_field(
                state, "PermanentThenYoungSetReceiverB", memberName, 2, &prototypeB, &instanceB, &constants[2]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceB));
        ZrCore_Value_InitAsInt(state, &constants[3], 202);
        init_object_constant_with_shared_field(
                state, "PermanentThenYoungSetReceiverC", memberName, 3, &prototypeC, &instanceC, &constants[4]);
        ZrCore_Value_InitAsInt(state, &constants[5], 303);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);

        function = create_test_function(state, instructions, 9, constants, 6, 6);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueA = get_object_field_cstring(state, instanceA, "value");
        updatedValueB = get_object_field_cstring(state, instanceB, "value");
        updatedValueC = get_object_field_cstring(state, instanceC, "value");
        TEST_ASSERT_NOT_NULL(updatedValueA);
        TEST_ASSERT_NOT_NULL(updatedValueB);
        TEST_ASSERT_NOT_NULL(updatedValueC);
        TEST_ASSERT_EQUAL_INT64(101, updatedValueA->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(202, updatedValueB->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(303, updatedValueC->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceB, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[1].cachedMemberName);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT PIC Replacement Young Replaces Permanent Receivers");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_pic_replacement_stays_out_of_remembered_set_for_permanent_receivers(void) {
    TEST_START("SET_MEMBER_SLOT PIC Replacement Permanent Receivers");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "shared");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObject *instanceC;
        SZrTypeValue constants[6];
        TZrInstruction instructions[9];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;
        const SZrTypeValue *updatedValueA;
        const SZrTypeValue *updatedValueB;
        const SZrTypeValue *updatedValueC;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "PermanentSetReceiverA", memberName, 1, &prototypeA, &instanceA, &constants[0]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceA));
        ZrCore_Value_InitAsInt(state, &constants[1], 101);
        init_object_constant_with_shared_field(
                state, "PermanentSetReceiverB", memberName, 2, &prototypeB, &instanceB, &constants[2]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceB));
        ZrCore_Value_InitAsInt(state, &constants[3], 202);
        init_object_constant_with_shared_field(
                state, "PermanentSetReceiverC", memberName, 3, &prototypeC, &instanceC, &constants[4]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceC));
        ZrCore_Value_InitAsInt(state, &constants[5], 303);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);

        function = create_test_function(state, instructions, 9, constants, 6, 6);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueA = get_object_field_cstring(state, instanceA, "shared");
        updatedValueB = get_object_field_cstring(state, instanceB, "shared");
        updatedValueC = get_object_field_cstring(state, instanceC, "shared");
        TEST_ASSERT_NOT_NULL(updatedValueA);
        TEST_ASSERT_NOT_NULL(updatedValueB);
        TEST_ASSERT_NOT_NULL(updatedValueC);
        TEST_ASSERT_EQUAL_INT64(101, updatedValueA->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(202, updatedValueB->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(303, updatedValueC->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[0].cachedMemberName);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceB, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(memberName, function->callSiteCaches[0].picSlots[1].cachedMemberName);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT PIC Replacement Permanent Receivers");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_minor_gc_prunes_remembered_set_after_replacing_young_receivers_with_permanent_receivers(
        void) {
    TEST_START("SET_MEMBER_SLOT Remembered Set Prunes After Permanent Replacement");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "shared");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrObjectPrototype *prototypeD;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObject *instanceC;
        SZrObject *instanceD;
        SZrTypeValue constants[8];
        TZrInstruction instructions[12];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "RememberedSetYoungA", memberName, 1, &prototypeA, &instanceA, &constants[0]);
        ZrCore_Value_InitAsInt(state, &constants[1], 101);
        init_object_constant_with_shared_field(
                state, "RememberedSetYoungB", memberName, 2, &prototypeB, &instanceB, &constants[2]);
        ZrCore_Value_InitAsInt(state, &constants[3], 202);
        init_object_constant_with_shared_field(
                state, "RememberedSetPermanentC", memberName, 3, &prototypeC, &instanceC, &constants[4]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceC));
        ZrCore_Value_InitAsInt(state, &constants[5], 303);
        init_object_constant_with_shared_field(
                state, "RememberedSetPermanentD", memberName, 4, &prototypeD, &instanceD, &constants[6]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceD));
        ZrCore_Value_InitAsInt(state, &constants[7], 404);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);
        instructions[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 6);
        instructions[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 7, 7);
        instructions[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 7, 6, 0);

        function = create_test_function(state, instructions, 12, constants, 8, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        /*
         * Keep only the stale remembered entry: the PIC now references permanent receivers,
         * so clear direct young receiver constants before the next minor restart.
         */
        ZrCore_Value_ResetAsNull(&function->constantValueList[0]);
        ZrCore_Value_ResetAsNull(&function->constantValueList[2]);

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Remembered Set Prunes After Permanent Replacement");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_minor_gc_readds_and_reprunes_remembered_set_during_permanent_young_pic_oscillation(
        void) {
    TEST_START("SET_MEMBER_SLOT Remembered Set Oscillates Across Minor GC");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeA;
        SZrObjectPrototype *prototypeB;
        SZrObjectPrototype *prototypeC;
        SZrObjectPrototype *prototypeD;
        SZrObjectPrototype *prototypeE;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObject *instanceC;
        SZrObject *instanceD;
        SZrObject *instanceE;
        SZrTypeValue constants[8];
        SZrTypeValue youngReceiverEConstant;
        SZrTypeValue value3303;
        SZrTypeValue value606;
        SZrTypeValue value4404;
        SZrTypeValue value7303;
        SZrTypeValue value8204;
        SZrTypeValue value9303;
        SZrTypeValue value10204;
        TZrInstruction instructions[12];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;
        const SZrTypeValue *updatedValueC;
        const SZrTypeValue *updatedValueD;
        const SZrTypeValue *updatedValueE;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "OscillationYoungA", memberName, 1, &prototypeA, &instanceA, &constants[0]);
        ZrCore_Value_InitAsInt(state, &constants[1], 101);
        init_object_constant_with_shared_field(
                state, "OscillationYoungB", memberName, 2, &prototypeB, &instanceB, &constants[2]);
        ZrCore_Value_InitAsInt(state, &constants[3], 202);
        init_object_constant_with_shared_field(
                state, "OscillationPermanentC", memberName, 3, &prototypeC, &instanceC, &constants[4]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceC));
        ZrCore_Value_InitAsInt(state, &constants[5], 303);
        init_object_constant_with_shared_field(
                state, "OscillationPermanentD", memberName, 4, &prototypeD, &instanceD, &constants[6]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceD));
        ZrCore_Value_InitAsInt(state, &constants[7], 404);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);
        instructions[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 6);
        instructions[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 7, 7);
        instructions[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 7, 6, 0);

        function = create_test_function(state, instructions, 12, constants, 8, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        /*
         * Phase 1 -> off:
         * the PIC is permanent-only, so once the old young receiver constants disappear,
         * the next minor restart should prune the stale remembered entry.
         */
        ZrCore_Value_ResetAsNull(&function->constantValueList[0]);
        ZrCore_Value_ResetAsNull(&function->constantValueList[2]);
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        /*
         * Phase 2 -> on:
         * inject a fresh young receiver after the prune and make the full PIC oscillate
         * from permanent-only to mixed permanent/young again.
         */
        init_object_constant_with_shared_field(
                state, "OscillationYoungE", memberName, 5, &prototypeE, &instanceE, &youngReceiverEConstant);
        ZrCore_Value_InitAsInt(state, &value3303, 3303);
        ZrCore_Value_InitAsInt(state, &value606, 606);
        ZrCore_Value_InitAsInt(state, &value4404, 4404);

        function->constantValueList[0] = constants[4];
        function->constantValueList[1] = value3303;
        function->constantValueList[2] = youngReceiverEConstant;
        function->constantValueList[3] = value606;
        function->constantValueList[4] = constants[6];
        function->constantValueList[5] = value4404;
        function->constantValueList[6] = youngReceiverEConstant;
        function->constantValueList[7] = value606;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueC = get_object_field_cstring(state, instanceC, "value");
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, instanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueC);
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(3303, updatedValueC->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(4404, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(606, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeE, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        /*
         * Phase 3 -> off again:
         * switch back to permanent-only receivers, then let minor GC prune the newly stale
         * remembered entry a second time.
         */
        ZrCore_Value_InitAsInt(state, &value7303, 7303);
        ZrCore_Value_InitAsInt(state, &value8204, 8204);
        ZrCore_Value_InitAsInt(state, &value9303, 9303);
        ZrCore_Value_InitAsInt(state, &value10204, 10204);

        function->constantValueList[0] = constants[4];
        function->constantValueList[1] = value7303;
        function->constantValueList[2] = constants[6];
        function->constantValueList[3] = value8204;
        function->constantValueList[4] = constants[4];
        function->constantValueList[5] = value9303;
        function->constantValueList[6] = constants[6];
        function->constantValueList[7] = value10204;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Remembered Set Oscillates Across Minor GC");
    TEST_DIVIDER();
}

static void
test_set_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune(void) {
    TEST_START("SET_MEMBER_SLOT Minor GC Rewrites Forwarded Young Receiver Pair");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeC;
        SZrObjectPrototype *prototypeD;
        SZrObjectPrototype *prototypeE;
        SZrObject *instanceC;
        SZrObject *instanceD;
        SZrObject *instanceE;
        SZrTypeValue permanentReceiverCConstant;
        SZrTypeValue permanentReceiverDConstant;
        SZrTypeValue youngReceiverEConstant;
        SZrTypeValue initialConstants[8];
        SZrTypeValue value101;
        SZrTypeValue value202;
        SZrTypeValue value303;
        SZrTypeValue value404;
        SZrTypeValue value505;
        SZrTypeValue value606;
        SZrTypeValue value707;
        SZrTypeValue value808;
        SZrTypeValue value1303;
        SZrTypeValue value1404;
        SZrTypeValue value1503;
        SZrTypeValue value1604;
        TZrInstruction instructions[12];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;
        TZrPtr originalYoungReceiverAddress;
        SZrObject *forwardedInstanceE;
        const SZrTypeValue *updatedValueC;
        const SZrTypeValue *updatedValueD;
        const SZrTypeValue *updatedValueE;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "ForwardedPermanentC", memberName, 3, &prototypeC, &instanceC, &permanentReceiverCConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceC));
        init_object_constant_with_shared_field(
                state, "ForwardedPermanentD", memberName, 4, &prototypeD, &instanceD, &permanentReceiverDConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceD));
        init_object_constant_with_shared_field(
                state, "ForwardedYoungE", memberName, 5, &prototypeE, &instanceE, &youngReceiverEConstant);

        ZrCore_Value_InitAsInt(state, &value101, 101);
        ZrCore_Value_InitAsInt(state, &value202, 202);
        ZrCore_Value_InitAsInt(state, &value303, 303);
        ZrCore_Value_InitAsInt(state, &value404, 404);
        ZrCore_Value_InitAsInt(state, &value505, 505);
        ZrCore_Value_InitAsInt(state, &value606, 606);
        ZrCore_Value_InitAsInt(state, &value707, 707);
        ZrCore_Value_InitAsInt(state, &value808, 808);
        ZrCore_Value_InitAsInt(state, &value1303, 1303);
        ZrCore_Value_InitAsInt(state, &value1404, 1404);
        ZrCore_Value_InitAsInt(state, &value1503, 1503);
        ZrCore_Value_InitAsInt(state, &value1604, 1604);

        initialConstants[0] = youngReceiverEConstant;
        initialConstants[1] = value101;
        initialConstants[2] = permanentReceiverDConstant;
        initialConstants[3] = value202;
        initialConstants[4] = youngReceiverEConstant;
        initialConstants[5] = value303;
        initialConstants[6] = permanentReceiverDConstant;
        initialConstants[7] = value404;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);
        instructions[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 6);
        instructions[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 7, 7);
        instructions[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 7, 6, 0);

        function = create_test_function(state, instructions, 12, initialConstants, 8, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, instanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(404, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(303, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeE, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        originalYoungReceiverAddress = (TZrPtr)instanceE;
        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        forwardedInstanceE = ZR_CAST_OBJECT(state, function->constantValueList[0].value.object);
        TEST_ASSERT_NOT_NULL(forwardedInstanceE);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, ZR_CAST_OBJECT(state, function->constantValueList[4].value.object));
        TEST_ASSERT_TRUE((TZrPtr)forwardedInstanceE != originalYoungReceiverAddress);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.storageKind);
        TEST_ASSERT_TRUE((ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.escapeFlags &
                          ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.promotionReason);
        TEST_ASSERT_EQUAL_PTR(prototypeE, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(forwardedInstanceE->cachedStringLookupPair);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE->cachedStringLookupPair,
                              function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[1] = value505;
        function->constantValueList[3] = value606;
        function->constantValueList[4] = function->constantValueList[0];
        function->constantValueList[5] = value707;
        function->constantValueList[7] = value808;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, forwardedInstanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(808, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(707, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(6u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE->cachedStringLookupPair,
                              function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[0] = permanentReceiverCConstant;
        function->constantValueList[1] = value1303;
        function->constantValueList[2] = permanentReceiverDConstant;
        function->constantValueList[3] = value1404;
        function->constantValueList[4] = permanentReceiverCConstant;
        function->constantValueList[5] = value1503;
        function->constantValueList[6] = permanentReceiverDConstant;
        function->constantValueList[7] = value1604;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueC = get_object_field_cstring(state, instanceC, "value");
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        TEST_ASSERT_NOT_NULL(updatedValueC);
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_EQUAL_INT64(1503, updatedValueC->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(1604, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(3u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(9u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picNextInsertIndex);
        TEST_ASSERT_EQUAL_PTR(prototypeC, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceC, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(prototypeD, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceD, function->callSiteCaches[0].picSlots[1].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Minor GC Rewrites Forwarded Young Receiver Pair");
    TEST_DIVIDER();
}

static void
test_set_member_slot_instruction_second_minor_gc_prunes_stale_remembered_after_forwarded_receiver_promotes_old(void) {
    TEST_START("SET_MEMBER_SLOT Second Minor GC Prunes Stale Remembered After Promotion");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeD;
        SZrObjectPrototype *prototypeE;
        SZrObject *instanceD;
        SZrObject *instanceE;
        SZrTypeValue permanentReceiverDConstant;
        SZrTypeValue youngReceiverEConstant;
        SZrTypeValue initialConstants[8];
        SZrTypeValue value101;
        SZrTypeValue value202;
        SZrTypeValue value303;
        SZrTypeValue value404;
        SZrTypeValue value505;
        SZrTypeValue value606;
        SZrTypeValue value707;
        SZrTypeValue value808;
        SZrTypeValue value909;
        SZrTypeValue value1001;
        SZrTypeValue value1102;
        SZrTypeValue value1203;
        TZrInstruction instructions[12];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;
        SZrObject *forwardedInstanceE;
        const SZrTypeValue *updatedValueD;
        const SZrTypeValue *updatedValueE;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "PromotedPermanentD", memberName, 4, &prototypeD, &instanceD, &permanentReceiverDConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceD));
        init_object_constant_with_shared_field(
                state, "PromotedYoungE", memberName, 5, &prototypeE, &instanceE, &youngReceiverEConstant);

        ZrCore_Value_InitAsInt(state, &value101, 101);
        ZrCore_Value_InitAsInt(state, &value202, 202);
        ZrCore_Value_InitAsInt(state, &value303, 303);
        ZrCore_Value_InitAsInt(state, &value404, 404);
        ZrCore_Value_InitAsInt(state, &value505, 505);
        ZrCore_Value_InitAsInt(state, &value606, 606);
        ZrCore_Value_InitAsInt(state, &value707, 707);
        ZrCore_Value_InitAsInt(state, &value808, 808);
        ZrCore_Value_InitAsInt(state, &value909, 909);
        ZrCore_Value_InitAsInt(state, &value1001, 1001);
        ZrCore_Value_InitAsInt(state, &value1102, 1102);
        ZrCore_Value_InitAsInt(state, &value1203, 1203);

        initialConstants[0] = youngReceiverEConstant;
        initialConstants[1] = value101;
        initialConstants[2] = permanentReceiverDConstant;
        initialConstants[3] = value202;
        initialConstants[4] = youngReceiverEConstant;
        initialConstants[5] = value303;
        initialConstants[6] = permanentReceiverDConstant;
        initialConstants[7] = value404;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);
        instructions[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 6);
        instructions[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 7, 7);
        instructions[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 7, 6, 0);

        function = create_test_function(state, instructions, 12, initialConstants, 8, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        forwardedInstanceE = ZR_CAST_OBJECT(state, function->constantValueList[0].value.object);
        TEST_ASSERT_NOT_NULL(forwardedInstanceE);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE->cachedStringLookupPair,
                              function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[1] = value505;
        function->constantValueList[3] = value606;
        function->constantValueList[4] = function->constantValueList[0];
        function->constantValueList[5] = value707;
        function->constantValueList[7] = value808;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, forwardedInstanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(808, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(707, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(6u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[1] = value909;
        function->constantValueList[3] = value1001;
        function->constantValueList[4] = function->constantValueList[0];
        function->constantValueList[5] = value1102;
        function->constantValueList[7] = value1203;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, forwardedInstanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(1203, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(1102, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(10u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Second Minor GC Prunes Stale Remembered After Promotion");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_exact_pair_hit_survives_receiver_rehash_and_minor_gc_prune(
        void) {
    TEST_START("SET_MEMBER_SLOT Exact Pair Hit Survives Rehash And Minor GC Prune");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *prototypeD;
        SZrObjectPrototype *prototypeE;
        SZrObject *instanceD;
        SZrObject *instanceE;
        SZrTypeValue permanentReceiverDConstant;
        SZrTypeValue youngReceiverEConstant;
        SZrTypeValue initialConstants[8];
        SZrTypeValue value101;
        SZrTypeValue value202;
        SZrTypeValue value303;
        SZrTypeValue value404;
        SZrTypeValue value505;
        SZrTypeValue value606;
        SZrTypeValue value707;
        SZrTypeValue value808;
        SZrTypeValue value909;
        SZrTypeValue value1001;
        SZrTypeValue value1102;
        SZrTypeValue value1203;
        TZrInstruction instructions[12];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;
        SZrHashKeyValuePair *cachedPairBeforeRehash;
        SZrObject *forwardedInstanceE;
        const SZrTypeValue *updatedValueD;
        const SZrTypeValue *updatedValueE;

        TEST_ASSERT_NOT_NULL(memberName);
        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        init_object_constant_with_shared_field(
                state, "RehashPermanentD", memberName, 4, &prototypeD, &instanceD, &permanentReceiverDConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceD));
        init_object_constant_with_shared_field(
                state, "RehashYoungE", memberName, 5, &prototypeE, &instanceE, &youngReceiverEConstant);

        ZrCore_Value_InitAsInt(state, &value101, 101);
        ZrCore_Value_InitAsInt(state, &value202, 202);
        ZrCore_Value_InitAsInt(state, &value303, 303);
        ZrCore_Value_InitAsInt(state, &value404, 404);
        ZrCore_Value_InitAsInt(state, &value505, 505);
        ZrCore_Value_InitAsInt(state, &value606, 606);
        ZrCore_Value_InitAsInt(state, &value707, 707);
        ZrCore_Value_InitAsInt(state, &value808, 808);
        ZrCore_Value_InitAsInt(state, &value909, 909);
        ZrCore_Value_InitAsInt(state, &value1001, 1001);
        ZrCore_Value_InitAsInt(state, &value1102, 1102);
        ZrCore_Value_InitAsInt(state, &value1203, 1203);

        initialConstants[0] = youngReceiverEConstant;
        initialConstants[1] = value101;
        initialConstants[2] = permanentReceiverDConstant;
        initialConstants[3] = value202;
        initialConstants[4] = youngReceiverEConstant;
        initialConstants[5] = value303;
        initialConstants[6] = permanentReceiverDConstant;
        initialConstants[7] = value404;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4, 4);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 5);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 5, 4, 0);
        instructions[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6, 6);
        instructions[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 7, 7);
        instructions[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 7, 6, 0);

        function = create_test_function(state, instructions, 12, initialConstants, 8, 8);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_PTR(instanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);

        cachedPairBeforeRehash = function->callSiteCaches[0].picSlots[0].cachedReceiverPair;
        TEST_ASSERT_TRUE(add_named_fields_until_object_rehashes(state, instanceE, "rehash", 2000u, 32u));
        TEST_ASSERT_EQUAL_PTR(cachedPairBeforeRehash, function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        function->constantValueList[1] = value505;
        function->constantValueList[3] = value606;
        function->constantValueList[4] = function->constantValueList[0];
        function->constantValueList[5] = value707;
        function->constantValueList[7] = value808;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, instanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(808, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(707, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(6u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_PTR(cachedPairBeforeRehash, function->callSiteCaches[0].picSlots[0].cachedReceiverPair);

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;
        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);

        forwardedInstanceE = ZR_CAST_OBJECT(state, function->constantValueList[0].value.object);
        TEST_ASSERT_NOT_NULL(forwardedInstanceE);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(forwardedInstanceE)->garbageCollectMark.storageKind);
        TEST_ASSERT_EQUAL_PTR(forwardedInstanceE, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_EQUAL_PTR(cachedPairBeforeRehash, function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[1] = value909;
        function->constantValueList[3] = value1001;
        function->constantValueList[4] = function->constantValueList[0];
        function->constantValueList[5] = value1102;
        function->constantValueList[7] = value1203;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, forwardedInstanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(1203, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(1102, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(10u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[1] = value505;
        function->constantValueList[3] = value606;
        function->constantValueList[4] = function->constantValueList[0];
        function->constantValueList[5] = value707;
        function->constantValueList[7] = value808;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        updatedValueD = get_object_field_cstring(state, instanceD, "value");
        updatedValueE = get_object_field_cstring(state, forwardedInstanceE, "value");
        TEST_ASSERT_NOT_NULL(updatedValueD);
        TEST_ASSERT_NOT_NULL(updatedValueE);
        TEST_ASSERT_EQUAL_INT64(808, updatedValueD->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(707, updatedValueE->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(14u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Exact Pair Hit Survives Rehash And Minor GC Prune");
    TEST_DIVIDER();
}

static void test_set_member_slot_instruction_does_not_reuse_cached_pair_for_different_receiver_same_prototype(
        void) {
    TEST_START("SET_MEMBER_SLOT Different Receiver Same Prototype Avoids Pair Reuse");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrString *sharedTypeName = ZrCore_String_CreateFromNative(state, "ShapeOnlySetSharedBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *sharedPrototype = ZrCore_ObjectPrototype_New(state,
                                                                         sharedTypeName,
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrObjectPrototype *otherPrototype;
        SZrObject *otherInstance;
        SZrTypeValue receiverAConstant;
        SZrTypeValue receiverBConstant;
        SZrTypeValue otherReceiverConstant;
        SZrTypeValue initialConstants[4];
        SZrTypeValue updatedValueA;
        SZrTypeValue updatedValueD;
        SZrTypeValue updatedValueB;
        SZrTypeValue updatedValueOther;
        TZrInstruction instructions[6];
        SZrFunction *function;
        TZrBool success;
        const SZrTypeValue *currentValueA;
        const SZrTypeValue *currentValueB;
        const SZrTypeValue *currentValueOther;

        TEST_ASSERT_NOT_NULL(sharedTypeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(sharedPrototype);

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, sharedPrototype, &descriptor));

        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 111, &instanceA, &receiverAConstant);
        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 222, &instanceB, &receiverBConstant);
        init_object_constant_with_shared_field(
                state, "ShapeOnlySetOtherBox", memberName, 333, &otherPrototype, &otherInstance, &otherReceiverConstant);

        ZrCore_Value_InitAsInt(state, &updatedValueA, 501);
        ZrCore_Value_InitAsInt(state, &updatedValueD, 601);
        ZrCore_Value_InitAsInt(state, &updatedValueB, 777);
        ZrCore_Value_InitAsInt(state, &updatedValueOther, 888);

        initialConstants[0] = receiverAConstant;
        initialConstants[1] = updatedValueA;
        initialConstants[2] = otherReceiverConstant;
        initialConstants[3] = updatedValueD;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 3, 2, 0);

        function = create_test_function(state, instructions, 6, initialConstants, 4, 4);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        instanceA->cachedStringLookupPair = ZR_NULL;
        instanceB->cachedStringLookupPair = ZR_NULL;
        otherInstance->cachedStringLookupPair = ZR_NULL;
        currentValueA = get_object_field_cstring(state, instanceA, "value");
        currentValueB = get_object_field_cstring(state, instanceB, "value");
        currentValueOther = get_object_field_cstring(state, otherInstance, "value");
        TEST_ASSERT_NOT_NULL(currentValueA);
        TEST_ASSERT_NOT_NULL(currentValueB);
        TEST_ASSERT_NOT_NULL(currentValueOther);
        TEST_ASSERT_EQUAL_INT64(501, currentValueA->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(222, currentValueB->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(601, currentValueOther->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(sharedPrototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceA, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_EQUAL_PTR(otherPrototype, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        function->constantValueList[0] = receiverBConstant;
        function->constantValueList[1] = updatedValueB;
        function->constantValueList[3] = updatedValueOther;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        instanceA->cachedStringLookupPair = ZR_NULL;
        instanceB->cachedStringLookupPair = ZR_NULL;
        otherInstance->cachedStringLookupPair = ZR_NULL;
        currentValueA = get_object_field_cstring(state, instanceA, "value");
        currentValueB = get_object_field_cstring(state, instanceB, "value");
        currentValueOther = get_object_field_cstring(state, otherInstance, "value");
        TEST_ASSERT_NOT_NULL(currentValueA);
        TEST_ASSERT_NOT_NULL(currentValueB);
        TEST_ASSERT_NOT_NULL(currentValueOther);
        TEST_ASSERT_EQUAL_INT64(501, currentValueA->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(777, currentValueB->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(888, currentValueOther->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Different Receiver Same Prototype Avoids Pair Reuse");
    TEST_DIVIDER();
}

static void
test_set_member_slot_instruction_same_prototype_safe_hit_refreshes_cached_receiver_before_minor_gc_prune(void) {
    TEST_START("SET_MEMBER_SLOT Same Prototype Safe Hit Refreshes PIC Receiver");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *sharedTypeName = ZrCore_String_CreateFromNative(state, "ShapeOnlySetRefreshBox");
        SZrString *memberName = ZrCore_String_CreateFromNative(state, "value");
        SZrObjectPrototype *sharedPrototype = ZrCore_ObjectPrototype_New(state,
                                                                         sharedTypeName,
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrTypeValue receiverAConstant;
        SZrTypeValue receiverBConstant;
        SZrTypeValue constants[2];
        SZrTypeValue updatedValueA;
        SZrTypeValue updatedValueB;
        TZrInstruction instructions[3];
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrStackValuePointer rootSlot = state->stackBase.valuePointer;
        TZrBool success;
        const SZrTypeValue *currentValueA;
        const SZrTypeValue *currentValueB;

        TEST_ASSERT_NOT_NULL(sharedTypeName);
        TEST_ASSERT_NOT_NULL(memberName);
        TEST_ASSERT_NOT_NULL(sharedPrototype);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = memberName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
        descriptor.isWritable = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, sharedPrototype, &descriptor));
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(sharedPrototype));

        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 111, &instanceA, &receiverAConstant);
        init_object_constant_with_existing_field_prototype(
                state, sharedPrototype, memberName, 222, &instanceB, &receiverBConstant);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(instanceB));

        ZrCore_Value_InitAsInt(state, &updatedValueA, 501);
        ZrCore_Value_InitAsInt(state, &updatedValueB, 777);
        constants[0] = receiverAConstant;
        constants[1] = updatedValueA;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1, 0, 0);

        function = create_test_function(state, instructions, 3, constants, 2, 2);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = memberName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        instanceA->cachedStringLookupPair = ZR_NULL;
        instanceB->cachedStringLookupPair = ZR_NULL;
        currentValueA = get_object_field_cstring(state, instanceA, "value");
        currentValueB = get_object_field_cstring(state, instanceB, "value");
        TEST_ASSERT_NOT_NULL(currentValueA);
        TEST_ASSERT_NOT_NULL(currentValueB);
        TEST_ASSERT_EQUAL_INT64(501, currentValueA->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(222, currentValueB->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(sharedPrototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceA, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        function->constantValueList[0] = receiverBConstant;
        function->constantValueList[1] = updatedValueB;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);

        instanceA->cachedStringLookupPair = ZR_NULL;
        instanceB->cachedStringLookupPair = ZR_NULL;
        currentValueA = get_object_field_cstring(state, instanceA, "value");
        currentValueB = get_object_field_cstring(state, instanceB, "value");
        TEST_ASSERT_NOT_NULL(currentValueA);
        TEST_ASSERT_NOT_NULL(currentValueB);
        TEST_ASSERT_EQUAL_INT64(501, currentValueA->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(777, currentValueB->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_UINT32(1u, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2u, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_PTR(sharedPrototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(instanceB, function->callSiteCaches[0].picSlots[0].cachedReceiverObject);
        TEST_ASSERT_NOT_NULL(instanceB->cachedStringLookupPair);
        TEST_ASSERT_EQUAL_PTR(instanceB->cachedStringLookupPair,
                              function->callSiteCaches[0].picSlots[0].cachedReceiverPair);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Stack_SetRawObjectValue(state, rootSlot, functionObject);
        state->stackTop.valuePointer = rootSlot + 1;

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));

        gc->gcDebtSize = 4096;
        gc->gcLastStepWork = 0;
        ZrCore_GarbageCollector_GcStep(state);
        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, count_remembered_object_occurrences(gc, functionObject));

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_MEMBER_SLOT Same Prototype Safe Hit Refreshes PIC Receiver");
    TEST_DIVIDER();
}

static void test_object_invoke_member_omits_receiver_for_static_hidden_accessor(void) {
    TEST_START("Object InvokeMember Static Hidden Accessor");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "StaticMetaBox");
        SZrString *hiddenGetterName = ZrCore_String_CreateFromNative(state, "__get_shared");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrTypeValue functionValue;
        SZrTypeValue receiver;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(hiddenGetterName);
        TEST_ASSERT_NOT_NULL(prototype);

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = hiddenGetterName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
        descriptor.isStatic = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_hidden_meta_static_getter_native)));
        set_object_field_cstring(state, &prototype->super, "__get_shared", &functionValue);

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
        receiver.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_ResetAsNull(&result);

        TEST_ASSERT_TRUE(ZrCore_Object_InvokeMember(state, &receiver, hiddenGetterName, ZR_NULL, 0, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(91, result.value.nativeObject.nativeInt64);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Object InvokeMember Static Hidden Accessor");
    TEST_DIVIDER();
}

static void test_super_meta_get_cached_instruction_populates_two_slot_pic(void) {
    TEST_START("SUPER_META_GET_CACHED Two Slot PIC");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeNameA = ZrCore_String_CreateFromNative(state, "CachedMetaBoxA");
        SZrString *typeNameB = ZrCore_String_CreateFromNative(state, "CachedMetaBoxB");
        SZrString *hiddenGetterName = ZrCore_String_CreateFromNative(state, "__get_value");
        SZrObjectPrototype *prototypeA = ZrCore_ObjectPrototype_New(state, typeNameA, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObjectPrototype *prototypeB = ZrCore_ObjectPrototype_New(state, typeNameB, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrTypeValue functionValue;
        SZrTypeValue constants[2];
        TZrInstruction instructions[8];
        SZrFunction *function;
        TZrBool success;
        TZrStackValuePointer base;
        SZrTypeValue *lastResult;

        TEST_ASSERT_NOT_NULL(typeNameA);
        TEST_ASSERT_NOT_NULL(typeNameB);
        TEST_ASSERT_NOT_NULL(hiddenGetterName);
        TEST_ASSERT_NOT_NULL(prototypeA);
        TEST_ASSERT_NOT_NULL(prototypeB);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(
                                             state,
                                             test_hidden_meta_cached_monomorphic_a_getter_native)));
        set_object_field_cstring(state, &prototypeA->super, "__get_value", &functionValue);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(
                                             state,
                                             test_hidden_meta_cached_monomorphic_b_getter_native)));
        set_object_field_cstring(state, &prototypeB->super, "__get_value", &functionValue);

        instanceA = ZrCore_Object_New(state, prototypeA);
        instanceB = ZrCore_Object_New(state, prototypeB);
        TEST_ASSERT_NOT_NULL(instanceA);
        TEST_ASSERT_NOT_NULL(instanceB);
        ZrCore_Object_Init(state, instanceA);
        ZrCore_Object_Init(state, instanceB);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instanceA));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsRawObject(state, &constants[1], ZR_CAST_RAW_OBJECT_AS_SUPER(instanceB));
        constants[1].type = ZR_VALUE_TYPE_OBJECT;

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED), 0, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED), 1, 1, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 0);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED), 2, 2, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 1);
        instructions[7] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED), 2, 2, 0);

        function = create_test_function(state, instructions, 8, constants, 2, 3);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry));
        function->memberEntries[0].symbol = hiddenGetterName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntryLength = 1;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCacheLength = 1;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototypeA, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototypeA, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedFunction);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedOwnerPrototype);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[1].cachedFunction);

        base = state->callInfoList->functionBase.valuePointer;
        lastResult = ZrCore_Stack_GetValue(base + 3);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(lastResult->type));
        TEST_ASSERT_EQUAL_INT64(202, lastResult->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUPER_META_GET_CACHED Two Slot PIC");
    TEST_DIVIDER();
}

static void test_super_meta_call_cached_instruction_fills_and_hits_callsite_pic(void) {
    TEST_START("SUPER_META_CALL_CACHED Callsite PIC");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeNameA = ZrCore_String_CreateFromNative(state, "CallableBoxA");
        SZrString *typeNameB = ZrCore_String_CreateFromNative(state, "CallableBoxB");
        SZrObjectPrototype *prototypeA = ZrCore_ObjectPrototype_New(state, typeNameA, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObjectPrototype *prototypeB = ZrCore_ObjectPrototype_New(state, typeNameB, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrTypeValue constants[4];
        TZrInstruction instructions[12];
        SZrTypeValue storedValue;
        SZrFunction *function;
        TZrBool success;
        TZrStackValuePointer base;
        SZrTypeValue *lastResult;

        TEST_ASSERT_NOT_NULL(typeNameA);
        TEST_ASSERT_NOT_NULL(typeNameB);
        TEST_ASSERT_NOT_NULL(prototypeA);
        TEST_ASSERT_NOT_NULL(prototypeB);

        ZrCore_ObjectPrototype_AddMeta(state,
                                       prototypeA,
                                       ZR_META_CALL,
                                       create_native_callable(state, test_meta_call_cached_add_native));
        ZrCore_ObjectPrototype_AddMeta(state,
                                       prototypeB,
                                       ZR_META_CALL,
                                       create_native_callable(state, test_meta_call_cached_mul_native));

        instanceA = ZrCore_Object_New(state, prototypeA);
        instanceB = ZrCore_Object_New(state, prototypeB);
        TEST_ASSERT_NOT_NULL(instanceA);
        TEST_ASSERT_NOT_NULL(instanceB);
        ZrCore_Object_Init(state, instanceA);
        ZrCore_Object_Init(state, instanceB);

        ZrCore_Value_InitAsInt(state, &storedValue, 10);
        set_object_field_cstring(state, instanceA, "__call_base", &storedValue);
        ZrCore_Value_InitAsInt(state, &storedValue, 4);
        set_object_field_cstring(state, instanceB, "__call_base", &storedValue);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instanceA));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[1], 3);
        ZrCore_Value_InitAsRawObject(state, &constants[2], ZR_CAST_RAW_OBJECT_AS_SUPER(instanceB));
        constants[2].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[3], 5);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED), 0, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED), 0, 0, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED), 0, 0, 0);
        instructions[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 2);
        instructions[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 3);
        instructions[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED), 0, 0, 0);

        function = create_test_function(state, instructions, 12, constants, 4, 3);
        TEST_ASSERT_NOT_NULL(function);

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = UINT32_MAX;
        function->callSiteCaches[0].argumentCount = 1;
        function->callSiteCacheLength = 1;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototypeA, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);

        base = state->callInfoList->functionBase.valuePointer;
        lastResult = ZrCore_Stack_GetValue(base + 1);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(lastResult->type));
        TEST_ASSERT_EQUAL_INT64(20, lastResult->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUPER_META_CALL_CACHED Callsite PIC");
    TEST_DIVIDER();
}

static void test_super_meta_call_cached_instruction_records_old_to_young_remembered_owner(void) {
    TEST_START("SUPER_META_CALL_CACHED Remembered Owner");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    {
        SZrGarbageCollector *gc = state->global->garbageCollector;
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "RememberedCallableBox");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrFunction *callable = create_young_native_callable(state, test_meta_call_cached_add_native);
        SZrObject *instance;
        SZrTypeValue constants[2];
        TZrInstruction instructions[3];
        SZrTypeValue storedValue;
        SZrFunction *function;
        SZrRawObject *functionObject;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(prototype);
        TEST_ASSERT_NOT_NULL(callable);

        gc->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;

        ZrCore_ObjectPrototype_AddMeta(state, prototype, ZR_META_CALL, callable);

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        ZrCore_Value_InitAsInt(state, &storedValue, 10);
        set_object_field_cstring(state, instance, "__call_base", &storedValue);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[1], 3);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED), 0, 0, 0);

        function = create_test_function(state, instructions, 3, constants, 2, 3);
        TEST_ASSERT_NOT_NULL(function);

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = UINT32_MAX;
        function->callSiteCaches[0].argumentCount = 1;
        function->callSiteCacheLength = 1;

        functionObject = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
        ZrCore_RawObject_MarkAsReferenced(functionObject);
        ZrCore_RawObject_SetStorageKind(functionObject, ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE);
        ZrCore_RawObject_SetRegionKind(functionObject, ZR_GARBAGE_COLLECT_REGION_KIND_OLD);

        TEST_ASSERT_FALSE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(0u, gc->rememberedObjectCount);
        TEST_ASSERT_EQUAL_UINT32(ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(callable)->garbageCollectMark.storageKind);

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(callable, function->callSiteCaches[0].picSlots[0].cachedFunction);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_HasRememberedObject(state->global, functionObject));
        TEST_ASSERT_EQUAL_UINT32(1u, gc->rememberedObjectCount);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUPER_META_CALL_CACHED Remembered Owner");
    TEST_DIVIDER();
}

static void test_super_dyn_call_cached_instruction_fills_and_hits_callsite_pic(void) {
    TEST_START("SUPER_DYN_CALL_CACHED Callsite PIC");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeNameA = ZrCore_String_CreateFromNative(state, "DynamicCallableBoxA");
        SZrString *typeNameB = ZrCore_String_CreateFromNative(state, "DynamicCallableBoxB");
        SZrObjectPrototype *prototypeA = ZrCore_ObjectPrototype_New(state, typeNameA, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObjectPrototype *prototypeB = ZrCore_ObjectPrototype_New(state, typeNameB, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObject *instanceA;
        SZrObject *instanceB;
        SZrTypeValue constants[4];
        TZrInstruction instructions[12];
        SZrTypeValue storedValue;
        SZrFunction *function;
        TZrBool success;
        TZrStackValuePointer base;
        SZrTypeValue *lastResult;

        TEST_ASSERT_NOT_NULL(typeNameA);
        TEST_ASSERT_NOT_NULL(typeNameB);
        TEST_ASSERT_NOT_NULL(prototypeA);
        TEST_ASSERT_NOT_NULL(prototypeB);

        ZrCore_ObjectPrototype_AddMeta(state,
                                       prototypeA,
                                       ZR_META_CALL,
                                       create_native_callable(state, test_meta_call_cached_add_native));
        ZrCore_ObjectPrototype_AddMeta(state,
                                       prototypeB,
                                       ZR_META_CALL,
                                       create_native_callable(state, test_meta_call_cached_mul_native));

        instanceA = ZrCore_Object_New(state, prototypeA);
        instanceB = ZrCore_Object_New(state, prototypeB);
        TEST_ASSERT_NOT_NULL(instanceA);
        TEST_ASSERT_NOT_NULL(instanceB);
        ZrCore_Object_Init(state, instanceA);
        ZrCore_Object_Init(state, instanceB);

        ZrCore_Value_InitAsInt(state, &storedValue, 10);
        set_object_field_cstring(state, instanceA, "__call_base", &storedValue);
        ZrCore_Value_InitAsInt(state, &storedValue, 4);
        set_object_field_cstring(state, instanceB, "__call_base", &storedValue);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(instanceA));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[1], 7);
        ZrCore_Value_InitAsRawObject(state, &constants[2], ZR_CAST_RAW_OBJECT_AS_SUPER(instanceB));
        constants[2].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[3], 11);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED), 0, 0, 0);
        instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 2);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 3);
        instructions[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED), 0, 0, 0);
        instructions[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED), 0, 0, 0);
        instructions[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 2);
        instructions[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 3);
        instructions[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED), 0, 0, 0);

        function = create_test_function(state, instructions, 12, constants, 4, 3);
        TEST_ASSERT_NOT_NULL(function);

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry));
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL;
        function->callSiteCaches[0].instructionIndex = 2;
        function->callSiteCaches[0].memberEntryIndex = UINT32_MAX;
        function->callSiteCaches[0].argumentCount = 1;
        function->callSiteCacheLength = 1;

        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].runtimeMissCount);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].runtimeHitCount);
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototypeA, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototypeB, function->callSiteCaches[0].picSlots[1].cachedReceiverPrototype);

        base = state->callInfoList->functionBase.valuePointer;
        lastResult = ZrCore_Stack_GetValue(base + 1);
        TEST_ASSERT_NOT_NULL(lastResult);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(lastResult->type));
        TEST_ASSERT_EQUAL_INT64(44, lastResult->value.nativeObject.nativeInt64);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUPER_DYN_CALL_CACHED Callsite PIC");
    TEST_DIVIDER();
}

static void test_super_meta_get_and_meta_set_static_cached_instructions_fill_and_hit_callsite_cache(void) {
    TEST_START("SUPER_META_GET_STATIC_CACHED And SUPER_META_SET_STATIC_CACHED Instructions");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "StaticCachedMetaBox");
        SZrString *hiddenGetterName = ZrCore_String_CreateFromNative(state, "__get_count");
        SZrString *hiddenSetterName = ZrCore_String_CreateFromNative(state, "__set_count");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrMemberDescriptor descriptor;
        SZrTypeValue functionValue;
        SZrTypeValue constants[2];
        TZrInstruction instructions[9];
        SZrFunction *function;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(hiddenGetterName);
        TEST_ASSERT_NOT_NULL(hiddenSetterName);
        TEST_ASSERT_NOT_NULL(prototype);

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = hiddenGetterName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
        descriptor.isStatic = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = hiddenSetterName;
        descriptor.kind = ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
        descriptor.isStatic = ZR_TRUE;
        TEST_ASSERT_TRUE(ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor));

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_hidden_meta_static_cached_getter_native)));
        set_object_field_cstring(state, &prototype->super, "__get_count", &functionValue);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_hidden_meta_static_cached_setter_native)));
        set_object_field_cstring(state, &prototype->super, "__set_count", &functionValue);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
        constants[0].type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &constants[1], 13);

        instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED), 0, 0, 0);
        instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 0);
        instructions[3] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED), 1, 1, 0);
        instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 0);
        instructions[5] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 1);
        instructions[6] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED), 2, 3, 1);
        instructions[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 0);
        instructions[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED), 2, 3, 1);

        function = create_test_function(state, instructions, 9, constants, 2, 4);
        TEST_ASSERT_NOT_NULL(function);

        function->memberEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionMemberEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->memberEntries);
        memset(function->memberEntries, 0, sizeof(SZrFunctionMemberEntry) * 2);
        function->memberEntries[0].symbol = hiddenGetterName;
        function->memberEntries[0].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntries[0].reserved0 = ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR;
        function->memberEntries[1].symbol = hiddenSetterName;
        function->memberEntries[1].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
        function->memberEntries[1].reserved0 = ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR;
        function->memberEntryLength = 2;

        function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionCallSiteCacheEntry) * 2,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches);
        memset(function->callSiteCaches, 0, sizeof(SZrFunctionCallSiteCacheEntry) * 2);
        function->callSiteCaches[0].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC;
        function->callSiteCaches[0].instructionIndex = 1;
        function->callSiteCaches[0].memberEntryIndex = 0;
        function->callSiteCaches[1].kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC;
        function->callSiteCaches[1].instructionIndex = 6;
        function->callSiteCaches[1].memberEntryIndex = 1;
        function->callSiteCacheLength = 2;

        g_hidden_meta_static_storage = 5;
        success = execute_test_function(state, function);
        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_TRUE(function->callSiteCaches[0].runtimeMissCount > 0);
        TEST_ASSERT_TRUE(function->callSiteCaches[1].runtimeMissCount > 0);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[0].picSlotCount);
        TEST_ASSERT_EQUAL_UINT32(1, function->callSiteCaches[1].picSlotCount);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedReceiverPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[0].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_EQUAL_PTR(prototype, function->callSiteCaches[1].picSlots[0].cachedOwnerPrototype);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[0].picSlots[0].cachedFunction);
        TEST_ASSERT_NOT_NULL(function->callSiteCaches[1].picSlots[0].cachedFunction);
        TEST_ASSERT_TRUE(function->callSiteCaches[0].picSlots[0].cachedIsStatic);
        TEST_ASSERT_TRUE(function->callSiteCaches[1].picSlots[0].cachedIsStatic);
        TEST_ASSERT_TRUE(function->callSiteCaches[0].runtimeHitCount > 0);
        TEST_ASSERT_TRUE(function->callSiteCaches[1].runtimeHitCount > 0);
        TEST_ASSERT_EQUAL_INT64(13, g_hidden_meta_static_storage);

        ZrCore_Function_Free(state, function);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUPER_META_GET_STATIC_CACHED And SUPER_META_SET_STATIC_CACHED Instructions");
    TEST_DIVIDER();
}

static void test_index_contract_dispatches_without_storage_fallback(void) {
    TEST_START("Index Contract Dispatch");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "IndexedBox");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrIndexContract contract;
        SZrObject *instance;
        SZrTypeValue receiver;
        SZrTypeValue key;
        SZrTypeValue inputValue;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(prototype);

        memset(&contract, 0, sizeof(contract));
        contract.getByIndexFunction = create_native_callable(state, test_index_contract_get_native);
        contract.setByIndexFunction = create_native_callable(state, test_index_contract_set_native);
        TEST_ASSERT_NOT_NULL(contract.getByIndexFunction);
        TEST_ASSERT_NOT_NULL(contract.setByIndexFunction);
        ZrCore_ObjectPrototype_SetIndexContract(prototype, &contract);

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        receiver.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &key, 5);
        ZrCore_Value_InitAsInt(state, &inputValue, 21);
        ZrCore_Value_ResetAsNull(&result);

        TEST_ASSERT_TRUE(ZrCore_Object_SetByIndex(state, &receiver, &key, &inputValue));
        TEST_ASSERT_TRUE(ZrCore_Object_GetByIndex(state, &receiver, &key, &result));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Index Contract Dispatch");
    TEST_DIVIDER();
}

static void test_index_contract_get_preserves_receiver_when_destination_aliases(void) {
    TEST_START("Index Contract Aliased Destination");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *typeName = ZrCore_String_CreateFromNative(state, "IndexedAliasBox");
        SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrIndexContract contract;
        SZrObject *instance;
        SZrTypeValue receiver;
        SZrTypeValue key;
        SZrTypeValue storedValue;

        TEST_ASSERT_NOT_NULL(typeName);
        TEST_ASSERT_NOT_NULL(prototype);

        memset(&contract, 0, sizeof(contract));
        contract.getByIndexFunction = create_native_callable(state, test_index_contract_get_native);
        TEST_ASSERT_NOT_NULL(contract.getByIndexFunction);
        ZrCore_ObjectPrototype_SetIndexContract(prototype, &contract);

        instance = ZrCore_Object_New(state, prototype);
        TEST_ASSERT_NOT_NULL(instance);
        ZrCore_Object_Init(state, instance);

        ZrCore_Value_InitAsInt(state, &storedValue, 33);
        set_object_field_cstring(state, instance, "__contract_index_value", &storedValue);

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(instance));
        receiver.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_InitAsInt(state, &key, 0);

        TEST_ASSERT_TRUE(ZrCore_Object_GetByIndex(state, &receiver, &key, &receiver));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(receiver.type));
        TEST_ASSERT_EQUAL_INT64(33, receiver.value.nativeObject.nativeInt64);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Index Contract Aliased Destination");
    TEST_DIVIDER();
}

static void test_iterator_contract_dispatches_without_named_protocol_members(void) {
    TEST_START("Iterator Contract Dispatch");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *iteratorTypeName = ZrCore_String_CreateFromNative(state, "ContractIterator");
        SZrString *iterableTypeName = ZrCore_String_CreateFromNative(state, "ContractIterable");
        SZrObjectPrototype *iteratorPrototype =
                ZrCore_ObjectPrototype_New(state, iteratorTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObjectPrototype *iterablePrototype =
                ZrCore_ObjectPrototype_New(state, iterableTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrIteratorContract iteratorContract;
        SZrIterableContract iterableContract;
        SZrObject *iteratorObject;
        SZrObject *iterableObject;
        SZrTypeValue iteratorStoredValue;
        SZrTypeValue iteratorValue;
        SZrTypeValue receiver;
        SZrTypeValue moveResult;
        SZrTypeValue currentValue;
        SZrTypeValue stepValue;

        TEST_ASSERT_NOT_NULL(iteratorPrototype);
        TEST_ASSERT_NOT_NULL(iterablePrototype);

        memset(&iteratorContract, 0, sizeof(iteratorContract));
        iteratorContract.moveNextFunction = create_native_callable(state, test_iter_contract_move_next_native);
        iteratorContract.currentFunction = create_native_callable(state, test_iter_contract_current_native);
        TEST_ASSERT_NOT_NULL(iteratorContract.moveNextFunction);
        TEST_ASSERT_NOT_NULL(iteratorContract.currentFunction);
        ZrCore_ObjectPrototype_SetIteratorContract(iteratorPrototype, &iteratorContract);

        memset(&iterableContract, 0, sizeof(iterableContract));
        iterableContract.iterInitFunction = create_native_callable(state, test_iter_contract_init_native);
        TEST_ASSERT_NOT_NULL(iterableContract.iterInitFunction);
        ZrCore_ObjectPrototype_SetIterableContract(iterablePrototype, &iterableContract);

        iteratorObject = ZrCore_Object_New(state, iteratorPrototype);
        iterableObject = ZrCore_Object_New(state, iterablePrototype);
        TEST_ASSERT_NOT_NULL(iteratorObject);
        TEST_ASSERT_NOT_NULL(iterableObject);
        ZrCore_Object_Init(state, iteratorObject);
        ZrCore_Object_Init(state, iterableObject);

        ZrCore_Value_InitAsInt(state, &stepValue, -1);
        set_object_field_cstring(state, iteratorObject, "__iter_step", &stepValue);
        ZrCore_Value_InitAsRawObject(state, &iteratorStoredValue, ZR_CAST_RAW_OBJECT_AS_SUPER(iteratorObject));
        iteratorStoredValue.type = ZR_VALUE_TYPE_OBJECT;
        set_object_field_cstring(state, iterableObject, "__contract_iterator", &iteratorStoredValue);

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(iterableObject));
        receiver.type = ZR_VALUE_TYPE_OBJECT;

        TEST_ASSERT_TRUE(ZrCore_Object_IterInit(state, &receiver, &iteratorValue));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_OBJECT(iteratorValue.type));

        TEST_ASSERT_TRUE(ZrCore_Object_IterMoveNext(state, &iteratorValue, &moveResult));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(moveResult.type));
        TEST_ASSERT_TRUE(moveResult.value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(ZrCore_Object_IterCurrent(state, &iteratorValue, &currentValue));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(currentValue.type));
        TEST_ASSERT_EQUAL_INT64(10, currentValue.value.nativeObject.nativeInt64);

        TEST_ASSERT_TRUE(ZrCore_Object_IterMoveNext(state, &iteratorValue, &moveResult));
        TEST_ASSERT_TRUE(moveResult.value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(ZrCore_Object_IterCurrent(state, &iteratorValue, &currentValue));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(currentValue.type));
        TEST_ASSERT_EQUAL_INT64(20, currentValue.value.nativeObject.nativeInt64);

        TEST_ASSERT_TRUE(ZrCore_Object_IterMoveNext(state, &iteratorValue, &moveResult));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(moveResult.type));
        TEST_ASSERT_FALSE(moveResult.value.nativeObject.nativeBool);
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Iterator Contract Dispatch");
    TEST_DIVIDER();
}

static void test_iteration_requires_explicit_contracts_not_named_members(void) {
    TEST_START("Iteration Requires Explicit Contracts");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        SZrString *iteratorTypeName = ZrCore_String_CreateFromNative(state, "NamedOnlyIterator");
        SZrString *iterableTypeName = ZrCore_String_CreateFromNative(state, "NamedOnlyIterable");
        SZrObjectPrototype *iteratorPrototype =
                ZrCore_ObjectPrototype_New(state, iteratorTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObjectPrototype *iterablePrototype =
                ZrCore_ObjectPrototype_New(state, iterableTypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        SZrObject *iteratorObject;
        SZrObject *iterableObject;
        SZrTypeValue receiver;
        SZrTypeValue iteratorValue;
        SZrTypeValue moveResult;
        SZrTypeValue currentValue;
        SZrTypeValue iteratorStoredValue;
        SZrTypeValue stepValue;
        SZrTypeValue functionValue;

        TEST_ASSERT_NOT_NULL(iteratorPrototype);
        TEST_ASSERT_NOT_NULL(iterablePrototype);

        iteratorObject = ZrCore_Object_New(state, iteratorPrototype);
        iterableObject = ZrCore_Object_New(state, iterablePrototype);
        TEST_ASSERT_NOT_NULL(iteratorObject);
        TEST_ASSERT_NOT_NULL(iterableObject);
        ZrCore_Object_Init(state, iteratorObject);
        ZrCore_Object_Init(state, iterableObject);

        ZrCore_Value_InitAsInt(state, &stepValue, -1);
        set_object_field_cstring(state, iteratorObject, "__iter_step", &stepValue);

        ZrCore_Value_InitAsRawObject(state, &iteratorStoredValue, ZR_CAST_RAW_OBJECT_AS_SUPER(iteratorObject));
        iteratorStoredValue.type = ZR_VALUE_TYPE_OBJECT;
        set_object_field_cstring(state, iterableObject, "__contract_iterator", &iteratorStoredValue);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_iter_contract_init_native)));
        set_object_field_cstring(state, iterableObject, "getIterator", &functionValue);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_iter_contract_move_next_native)));
        set_object_field_cstring(state, iteratorObject, "moveNext", &functionValue);

        ZrCore_Value_InitAsRawObject(state,
                                     &functionValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(create_native_callable(state,
                                                                                         test_iter_contract_current_native)));
        set_object_field_cstring(state, iteratorObject, "current", &functionValue);

        ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(iterableObject));
        receiver.type = ZR_VALUE_TYPE_OBJECT;

        TEST_ASSERT_FALSE(ZrCore_Object_IterInit(state, &receiver, &iteratorValue));

        ZrCore_Value_InitAsRawObject(state, &iteratorValue, ZR_CAST_RAW_OBJECT_AS_SUPER(iteratorObject));
        iteratorValue.type = ZR_VALUE_TYPE_OBJECT;
        TEST_ASSERT_FALSE(ZrCore_Object_IterMoveNext(state, &iteratorValue, &moveResult));
        TEST_ASSERT_FALSE(ZrCore_Object_IterCurrent(state, &iteratorValue, &currentValue));
    }

    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Iteration Requires Explicit Contracts");
    TEST_DIVIDER();
}

// ==================== 闭包操作指令测试 ====================

static void test_get_closure(void) {
    TEST_START("GET_CLOSURE Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：创建闭包，设置闭包值，然后获取
    // 创建函数
    SZrFunction *function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->stackSize = 1;
    function->parameterCount = 0;

    // 创建闭包（带1个闭包值）
    SZrClosure *closure = ZrCore_Closure_New(state, 1);
    TEST_ASSERT_NOT_NULL(closure);
    closure->function = function;
    ZrCore_Closure_InitValue(state, closure);

    // 设置闭包值
    SZrClosureValue *closureValue = closure->closureValuesExtend[0];
    ZrCore_Value_InitAsInt(state, ZrCore_ClosureValue_GetValue(closureValue), 99);

    // 将闭包放入常量池
    SZrTypeValue constant;
    ZrCore_Value_InitAsRawObject(state, &constant, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

    // 创建测试函数：GET_CLOSURE 0 -> stack[0]
    TZrInstruction instructions[1];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CLOSURE), 0, 0); // dest=0, closureIndex=0

    SZrFunction *testFunction = create_test_function(state, instructions, 1, &constant, 1, 2);
    TEST_ASSERT_NOT_NULL(testFunction);

    // 创建带闭包值的闭包，使用 testFunction
    SZrClosure *closureWithValue = ZrCore_Closure_New(state, 1);
    closureWithValue->function = testFunction;
    ZrCore_Closure_InitValue(state, closureWithValue);
    ZrCore_Value_InitAsInt(state, ZrCore_ClosureValue_GetValue(closureWithValue->closureValuesExtend[0]), 99);

    // 准备栈
    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(closureWithValue));
    state->stackTop.valuePointer = base + 1 + testFunction->stackSize;

    SZrCallInfo *callInfo = ZrCore_CallInfo_Extend(state);
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = testFunction->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->expectedReturnCount = 1;

    state->callInfoList = callInfo;
    ZrCore_Execute(state, callInfo);

    // 验证结果
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 1);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(99, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, testFunction);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GET_CLOSURE Instruction");
    TEST_DIVIDER();
}

static void test_set_closure(void) {
    TEST_START("SET_CLOSURE Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建测试函数：SET_CONSTANT 0 -> stack[0], SET_CLOSURE 0, stack[0]
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 88);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (88)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_CLOSURE), 0, 0); // closureIndex=0, src=0

    SZrFunction *testFunction = create_test_function(state, instructions, 2, &constant, 1, 2);

    // 创建带闭包值的闭包，使用 testFunction
    SZrClosure *closure = ZrCore_Closure_New(state, 1);
    closure->function = testFunction;
    ZrCore_Closure_InitValue(state, closure);

    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = base + 1 + testFunction->stackSize;

    SZrCallInfo *callInfo = ZrCore_CallInfo_Extend(state);
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = testFunction->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->expectedReturnCount = 1;
    state->callInfoList = callInfo;

    ZrCore_Execute(state, callInfo);

    // 验证闭包值被设置
    SZrClosureValue *closureValue = closure->closureValuesExtend[0];
    SZrTypeValue *value = ZrCore_ClosureValue_GetValue(closureValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
    TEST_ASSERT_EQUAL_INT64(88, value->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, testFunction);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SET_CLOSURE Instruction");
    TEST_DIVIDER();
}

static void test_getupval(void) {
    TEST_START("GETUPVAL Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建测试函数：GETUPVAL 0 -> stack[0]
    TZrInstruction instructions[1];
    instructions[0] = create_instruction_2(ZR_INSTRUCTION_ENUM(GETUPVAL), 0, 0, 0); // dest=0, upvalIndex=0

    SZrFunction *testFunction = create_test_function(state, instructions, 1, ZR_NULL, 0, 2);
    
    // 创建带upvalue的闭包，使用 testFunction
    SZrClosure *closure = ZrCore_Closure_New(state, 1);
    TEST_ASSERT_NOT_NULL(closure);
    closure->function = testFunction;
    ZrCore_Closure_InitValue(state, closure);
    TEST_ASSERT_NOT_NULL(closure->closureValuesExtend[0]);
    ZrCore_Value_InitAsInt(state, ZrCore_ClosureValue_GetValue(closure->closureValuesExtend[0]), 77);

    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    
    // 验证栈上的闭包对象是否正确设置
    SZrTypeValue *stackValue = ZrCore_Stack_GetValue(base);
    SZrClosure *stackClosure = ZR_CAST_VM_CLOSURE(state, stackValue->value.object);
    TEST_ASSERT_EQUAL_PTR(closure, stackClosure);
    TEST_ASSERT_EQUAL_PTR(testFunction, stackClosure->function);
    TEST_ASSERT_EQUAL_UINT(1, stackClosure->closureValueCount);
    TEST_ASSERT_NOT_NULL(stackClosure->closureValuesExtend[0]);
    
    state->stackTop.valuePointer = base + 1 + testFunction->stackSize;

    SZrCallInfo *callInfo = ZrCore_CallInfo_Extend(state);
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = testFunction->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->expectedReturnCount = 1;
    state->callInfoList = callInfo;

    // 验证执行前闭包值仍然有效
    SZrTypeValue *functionBaseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    SZrClosure *closureBeforeExecute = ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object);
    TEST_ASSERT_NOT_NULL(closureBeforeExecute);
    TEST_ASSERT_EQUAL_PTR(closure, closureBeforeExecute);
    TEST_ASSERT_NOT_NULL(closureBeforeExecute->closureValuesExtend[0]);

    ZrCore_Execute(state, callInfo);

    // 验证结果
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 1);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(77, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, testFunction);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "GETUPVAL Instruction");
    TEST_DIVIDER();
}

static void test_close_stack_value_closes_matching_upvalue(void) {
    TEST_START("CloseStackValue closes matching upvalue");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TZrStackValuePointer slot = state->stackTop.valuePointer;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(slot), 123);
    state->stackTop.valuePointer = slot + 1;

    SZrClosureValue *closureValue = ZrCore_Closure_FindOrCreateValue(state, slot);
    TEST_ASSERT_NOT_NULL(closureValue);
    TEST_ASSERT_FALSE(ZrCore_ClosureValue_IsClosed(closureValue));
    TEST_ASSERT_EQUAL_PTR(ZrCore_Stack_GetValue(slot), ZrCore_ClosureValue_GetValue(closureValue));

    ZrCore_Closure_CloseStackValue(state, slot);

    TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(closureValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(ZrCore_ClosureValue_GetValue(closureValue)->type));
    TEST_ASSERT_EQUAL_INT64(123, ZrCore_ClosureValue_GetValue(closureValue)->value.nativeObject.nativeInt64);

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "CloseStackValue closes matching upvalue");
    TEST_DIVIDER();
}

static void test_setupval(void) {
    TEST_START("SETUPVAL Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建测试函数：GET_CONSTANT 0 -> stack[0], SETUPVAL 0, stack[0]
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 66);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (66)
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL), 0, 0, 0); // upvalIndex=0, src=0

    SZrFunction *testFunction = create_test_function(state, instructions, 2, &constant, 1, 3);
    
    // 创建带upvalue的闭包，使用 testFunction
    SZrClosure *closure = ZrCore_Closure_New(state, 1);
    closure->function = testFunction;
    ZrCore_Closure_InitValue(state, closure);

    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = base + 1 + testFunction->stackSize;

    SZrCallInfo *callInfo = ZrCore_CallInfo_Extend(state);
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = testFunction->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->expectedReturnCount = 1;
    state->callInfoList = callInfo;

    ZrCore_Execute(state, callInfo);

    // 验证upvalue被设置
    SZrClosureValue *closureValue = closure->closureValuesExtend[0];
    SZrTypeValue *value = ZrCore_ClosureValue_GetValue(closureValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
    TEST_ASSERT_EQUAL_INT64(66, value->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, testFunction);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SETUPVAL Instruction");
    TEST_DIVIDER();
}

static void test_create_closure(void) {
    TEST_START("CREATE_CLOSURE Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建函数对象
    SZrFunction *function = ZrCore_Function_New(state);
    function->stackSize = 1;
    function->parameterCount = 0;

    // 将函数放入常量池
    SZrTypeValue constant;
    ZrCore_Value_InitAsRawObject(state, &constant, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    constant.type = ZR_VALUE_TYPE_FUNCTION;

    // 创建测试函数：CREATE_CLOSURE 0, 1 -> stack[0] (从常量0创建闭包，带1个闭包值)
    TZrInstruction instructions[1];
    instructions[0] = create_instruction_2(ZR_INSTRUCTION_ENUM(CREATE_CLOSURE), 0, 0,
                                           1); // dest=0, funcConst=0, closureVarCount=1

    SZrFunction *testFunction = create_test_function(state, instructions, 1, &constant, 1, 2);
    TEST_ASSERT_NOT_NULL(testFunction);

    TZrBool success = execute_test_function(state, testFunction);
    TEST_ASSERT_TRUE(success);

    // 验证结果
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 1);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_CLOSURE(result->type));
    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, result->value.object);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_EQUAL_PTR(function, closure->function);
    TEST_ASSERT_EQUAL_UINT(function->closureValueLength, closure->closureValueCount);

    ZrCore_Function_Free(state, testFunction);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "CREATE_CLOSURE Instruction");
    TEST_DIVIDER();
}

// ==================== 通用算术运算指令测试（带元方法）====================

static void test_add_generic(void) {
    TEST_START("ADD Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：整数相加（int类型有默认ADD元方法，应该返回int结果）
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 10);
    ZrCore_Value_InitAsInt(state, &constants[1], 20);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（int类型有默认ADD元方法，返回int结果）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(30, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "ADD Generic Instruction");
    TEST_DIVIDER();
}

static void test_sub_generic(void) {
    TEST_START("SUB Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 20);
    ZrCore_Value_InitAsInt(state, &constants[1], 10);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUB), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（int类型有默认SUB元方法，返回int结果）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(10, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SUB Generic Instruction");
    TEST_DIVIDER();
}

static void test_mul_generic(void) {
    TEST_START("MUL Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 6);
    ZrCore_Value_InitAsInt(state, &constants[1], 7);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（int类型有默认MUL元方法，返回int结果）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(42, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MUL Generic Instruction");
    TEST_DIVIDER();
}

static void test_mul_generic_bool_bool_xor_semantics(void) {
    TEST_START("MUL Generic Bool XOR Semantics");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsBool(state, &constants[0], ZR_TRUE);
    ZrCore_Value_InitAsBool(state, &constants[1], ZR_FALSE);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result->type);
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MUL Generic Bool XOR Semantics");
    TEST_DIVIDER();
}

static void test_mul_generic_signed_bool_returns_int64_product(void) {
    TEST_START("MUL Generic Signed Bool Product");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 6);
    ZrCore_Value_InitAsBool(state, &constants[1], ZR_TRUE);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(6, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MUL Generic Signed Bool Product");
    TEST_DIVIDER();
}

static void test_mul_generic_unsigned_bool_returns_int64_product(void) {
    TEST_START("MUL Generic Unsigned Bool Product");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsUInt(state, &constants[0], 9u);
    ZrCore_Value_InitAsBool(state, &constants[1], ZR_TRUE);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(9, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MUL Generic Unsigned Bool Product");
    TEST_DIVIDER();
}

static void test_mul_generic_signed_unsigned_returns_int64_product(void) {
    TEST_START("MUL Generic Signed Unsigned Product");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], -4);
    ZrCore_Value_InitAsUInt(state, &constants[1], 9u);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result->type);
    TEST_ASSERT_EQUAL_INT64(-36, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MUL Generic Signed Unsigned Product");
    TEST_DIVIDER();
}

static void test_div_generic(void) {
    TEST_START("DIV Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 20);
    ZrCore_Value_InitAsInt(state, &constants[1], 4);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(DIV), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果（int类型有默认DIV元方法，返回int结果）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(5, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "DIV Generic Instruction");
    TEST_DIVIDER();
}

static void test_mod_generic(void) {
    TEST_START("MOD Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 17);
    ZrCore_Value_InitAsInt(state, &constants[1], 5);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MOD), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(2, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MOD Generic Instruction");
    TEST_DIVIDER();
}

static void test_mod_generic_negative_divisor_normalizes_sign(void) {
    TEST_START("MOD Generic Negative Divisor");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 17);
    ZrCore_Value_InitAsInt(state, &constants[1], -5);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(MOD), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(2, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "MOD Generic Negative Divisor");
    TEST_DIVIDER();
}

static void test_pow_generic(void) {
    TEST_START("POW Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 2);
    ZrCore_Value_InitAsInt(state, &constants[1], 3);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(POW), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result->type));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "POW Generic Instruction");
    TEST_DIVIDER();
}

static void test_neg_generic(void) {
    TEST_START("NEG Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 5);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), 1, 0, 0);

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 3);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(-5, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "NEG Generic Instruction");
    TEST_DIVIDER();
}

static void test_shift_left_generic(void) {
    TEST_START("SHIFT_LEFT Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 2);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SHIFT_LEFT), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result->type));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SHIFT_LEFT Generic Instruction");
    TEST_DIVIDER();
}

static void test_shift_right_generic(void) {
    TEST_START("SHIFT_RIGHT Generic Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 20);
    ZrCore_Value_InitAsInt(state, &constants[1], 2);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(SHIFT_RIGHT), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result->type));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "SHIFT_RIGHT Generic Instruction");
    TEST_DIVIDER();
}

// ==================== 其他比较指令测试 ====================

static void test_logical_less_signed(void) {
    TEST_START("LOGICAL_LESS_SIGNED Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 10);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_LESS_SIGNED Instruction");
    TEST_DIVIDER();
}

static void test_logical_greater_equal_signed(void) {
    TEST_START("LOGICAL_GREATER_EQUAL_SIGNED Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 10);
    ZrCore_Value_InitAsInt(state, &constants[1], 10);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_GREATER_EQUAL_SIGNED Instruction");
    TEST_DIVIDER();
}

static void test_logical_less_equal_signed(void) {
    TEST_START("LOGICAL_LESS_EQUAL_SIGNED Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 5);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_LESS_EQUAL_SIGNED Instruction");
    TEST_DIVIDER();
}

static void test_logical_not_equal(void) {
    TEST_START("LOGICAL_NOT_EQUAL Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 5);
    ZrCore_Value_InitAsInt(state, &constants[1], 10);

    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    instructions[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL), 2, 0, 1);

    SZrFunction *function = create_test_function(state, instructions, 3, constants, 2, 4);
    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 3);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "LOGICAL_NOT_EQUAL Instruction");
    TEST_DIVIDER();
}

// ==================== 控制流指令测试 ====================

static void test_jump(void) {
    TEST_START("JUMP Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：JUMP跳过一条指令
    // GET_CONSTANT 0 -> stack[0] (值42)
    // JUMP +1 (跳过下一条指令)
    // GET_CONSTANT 1 -> stack[1] (这条指令被跳过)
    // GET_CONSTANT 0 -> stack[1] (最终值)
    SZrTypeValue constants[2];
    ZrCore_Value_InitAsInt(state, &constants[0], 42);
    ZrCore_Value_InitAsInt(state, &constants[1], 100);

    TZrInstruction instructions[4];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (42)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 1); // JUMP +1
    instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (100) - 被跳过
    instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 0); // dest=1, const=0 (42)

    SZrFunction *function = create_test_function(state, instructions, 4, constants, 2, 3);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证结果：stack[1]应该是42，而不是100
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 1);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result->type));
    TEST_ASSERT_EQUAL_INT64(42, result->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "JUMP Instruction");
    TEST_DIVIDER();
}

static void test_jump_if(void) {
    TEST_START("JUMP_IF Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：JUMP_IF true跳过，JUMP_IF false不跳过
    SZrTypeValue constants[3];
    ZR_VALUE_FAST_SET(&constants[0], nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    ZR_VALUE_FAST_SET(&constants[1], nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    ZrCore_Value_InitAsInt(state, &constants[2], 42);

    TZrInstruction instructions[5];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (true)
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), 0, 1); // JUMP_IF true +1 (跳过下一条)
    instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1); // dest=1, const=1 (false) - 被跳过
    instructions[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 0); // dest=1, const=0 (true)
    instructions[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2); // dest=2, const=2 (42)

    SZrFunction *function = create_test_function(state, instructions, 5, constants, 3, 4);
    TEST_ASSERT_NOT_NULL(function);

    TZrBool success = execute_test_function(state, function);
    TEST_ASSERT_TRUE(success);

    // 验证：stack[1]应该是true（因为JUMP_IF跳过了false）
    TZrStackValuePointer base = state->callInfoList->functionBase.valuePointer;
    SZrTypeValue *result = ZrCore_Stack_GetValue(base + 1);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_BOOL(result->type));
    TEST_ASSERT_TRUE(result->value.nativeObject.nativeBool);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "JUMP_IF Instruction");
    TEST_DIVIDER();
}

// ==================== 函数调用指令测试 ====================

static void test_function_call(void) {
    TEST_START("FUNCTION_CALL Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建一个简单的被调用函数：返回常量42
    SZrTypeValue calleeConstant;
    ZrCore_Value_InitAsInt(state, &calleeConstant, 42);

    TZrInstruction calleeInstructions[2];
    calleeInstructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (42)
    calleeInstructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 0,
                                                 0); // returnCount=1, resultSlot=0, variableArgs=0

    SZrFunction *calleeFunction = create_test_function(state, calleeInstructions, 2, &calleeConstant, 1, 1);
    TEST_ASSERT_NOT_NULL(calleeFunction);
    calleeFunction->parameterCount = 0;

    // 创建调用者函数
    SZrTypeValue callerConstants[3];
    SZrClosure *calleeClosure = ZrCore_Closure_New(state, 0);
    calleeClosure->function = calleeFunction;
    ZrCore_Value_InitAsRawObject(state, &callerConstants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(calleeClosure));
    ZrCore_Value_InitAsUInt(state, &callerConstants[1], 0); // parameterCount = 0
    ZrCore_Value_InitAsUInt(state, &callerConstants[2], 1); // returnCount = 1

    TZrInstruction callerInstructions[2];
    callerInstructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (closure)
    callerInstructions[1] =
            create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 1, 0,
                                 1); // call stack[0] with params from stack[1], returnCount from stack[2]
    ZR_UNUSED_PARAMETER(callerInstructions);

    // 注意：FUNCTION_CALL的格式需要根据实际指令定义调整
    // 这里简化测试，实际指令格式可能需要调整
    // 由于FUNCTION_CALL指令的复杂性，这里只做基本测试框架

    ZrCore_Function_Free(state, calleeFunction);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "FUNCTION_CALL Instruction");
    TEST_DIVIDER();
}

static void test_function_tail_call(void) {
    TEST_START("FUNCTION_TAIL_CALL Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    // FUNCTION_TAIL_CALL测试与FUNCTION_CALL类似，但使用TAIL_CALL指令
    // 由于实现复杂性，这里只做占位测试
    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "FUNCTION_TAIL_CALL Instruction");
    TEST_DIVIDER();
}

static void test_function_return(void) {
    TEST_START("FUNCTION_RETURN Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 创建函数：返回常量42
    SZrTypeValue constant;
    ZrCore_Value_InitAsInt(state, &constant, 42);

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (42)
    instructions[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 0,
                                           0); // returnCount=1, resultSlot=0, variableArgs=0

    SZrFunction *function = create_test_function(state, instructions, 2, &constant, 1, 2);
    TEST_ASSERT_NOT_NULL(function);
    function->parameterCount = 0;

    TZrBool success = execute_test_function(state, function);
    // FUNCTION_RETURN会导致函数返回，所以executeTestFunction会返回true
    TEST_ASSERT_TRUE(success);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "FUNCTION_RETURN Instruction");
    TEST_DIVIDER();
}

// ==================== 异常处理指令测试 ====================

static void test_try_throw_catch(void) {
    TEST_START("TRY THROW CATCH Instructions");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    // 测试：TRY块中THROW异常，然后CATCH捕获
    // 由于异常处理机制复杂，这里只做基本框架
    SZrTypeValue constant;
    SZrString *errorMsg = ZrCore_String_CreateFromNative(state, "test error");
    ZrCore_Value_InitAsRawObject(state, &constant, ZR_CAST_RAW_OBJECT_AS_SUPER(errorMsg));

    // TRY指令主要是标记，实际异常处理由底层机制处理
    // THROW指令抛出异常
    // CATCH指令捕获异常
    TZrInstruction instructions[3];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(TRY), 0, 0); // TRY
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // dest=0, const=0 (error message)
    instructions[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(THROW), 0, 0); // THROW from slot 0

    SZrFunction *function = create_test_function(state, instructions, 3, &constant, 1, 2);
    TEST_ASSERT_NOT_NULL(function);

    // 由于异常处理使用setjmp/longjmp，测试可能会比较复杂
    // 这里只做基本框架，实际测试需要更完善的异常处理设置

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "TRY THROW CATCH Instructions");
    TEST_DIVIDER();
}

// ==================== Main函数 ====================

int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Instruction Tests\n");
    TEST_MODULE_DIVIDER();

    // 栈操作指令测试
    RUN_TEST(test_get_stack);
    RUN_TEST(test_set_stack);

    // 常量操作指令测试
    RUN_TEST(test_get_constant);

    // 类型转换指令测试
    RUN_TEST(test_to_bool);
    RUN_TEST(test_to_int);
    RUN_TEST(test_to_uint);
    RUN_TEST(test_to_float);
    RUN_TEST(test_to_string);

    // 算术运算指令测试
    RUN_TEST(test_add_int);
    RUN_TEST(test_sub_int);
    RUN_TEST(test_mul_signed);
    RUN_TEST(test_add_float);
    RUN_TEST(test_add_string);
    RUN_TEST(test_div_signed);
    RUN_TEST(test_mod_signed);

    // 逻辑运算指令测试
    RUN_TEST(test_logical_not);
    RUN_TEST(test_logical_and);
    RUN_TEST(test_logical_or);
    RUN_TEST(test_logical_equal);
    RUN_TEST(test_logical_greater_signed);

    // 位运算指令测试
    RUN_TEST(test_bitwise_and);
    RUN_TEST(test_bitwise_or);
    RUN_TEST(test_bitwise_xor);
    RUN_TEST(test_bitwise_not);
    RUN_TEST(test_bitwise_shift_left);
    RUN_TEST(test_bitwise_shift_right);

    // 对象/数组创建指令测试
    RUN_TEST(test_create_object);
    RUN_TEST(test_create_array);

    // 表操作指令测试
    RUN_TEST(test_get_by_index);
    RUN_TEST(test_set_by_index);
    RUN_TEST(test_get_member_uses_property_descriptor);
    RUN_TEST(test_builtin_array_length_member_uses_native_contract);
    RUN_TEST(test_meta_get_and_meta_set_instructions_dispatch_hidden_accessors);
    RUN_TEST(test_super_meta_get_and_meta_set_cached_instructions_fill_and_hit_callsite_cache);
    RUN_TEST(test_get_member_slot_and_set_member_slot_instructions_fill_and_hit_callsite_cache);
    RUN_TEST(test_get_member_slot_instruction_records_remembered_set_for_young_receiver_metadata_even_with_permanent_owner);
    RUN_TEST(test_get_member_slot_instruction_pic_replacement_keeps_single_remembered_entry_for_young_receiver_metadata);
    RUN_TEST(test_get_member_slot_instruction_skips_remembered_set_when_cache_only_keeps_permanent_targets);
    RUN_TEST(test_get_member_slot_instruction_pic_replacement_stays_out_of_remembered_set_for_permanent_targets);
    RUN_TEST(test_get_member_slot_instruction_minor_gc_prunes_remembered_set_after_replacing_young_targets_with_permanent_targets);
    RUN_TEST(test_get_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune);
    RUN_TEST(test_get_member_slot_instruction_exact_pair_hit_survives_receiver_rehash_and_minor_gc_prune);
    RUN_TEST(test_get_member_slot_instruction_does_not_reuse_cached_pair_for_different_receiver_same_prototype);
    RUN_TEST(test_get_member_slot_instruction_same_prototype_safe_hit_refreshes_cached_receiver_before_minor_gc_prune);
    RUN_TEST(test_set_member_slot_instruction_records_remembered_set_for_young_receiver_metadata_even_with_permanent_owner);
    RUN_TEST(test_set_member_slot_instruction_pic_replacement_keeps_single_remembered_entry_for_young_receiver_metadata);
    RUN_TEST(test_set_member_slot_instruction_pic_replacement_records_remembered_set_when_young_receiver_replaces_permanent_receivers);
    RUN_TEST(test_set_member_slot_instruction_pic_replacement_stays_out_of_remembered_set_for_permanent_receivers);
    RUN_TEST(test_set_member_slot_instruction_minor_gc_prunes_remembered_set_after_replacing_young_receivers_with_permanent_receivers);
    RUN_TEST(test_set_member_slot_instruction_minor_gc_readds_and_reprunes_remembered_set_during_permanent_young_pic_oscillation);
    RUN_TEST(test_set_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune);
    RUN_TEST(test_set_member_slot_instruction_second_minor_gc_prunes_stale_remembered_after_forwarded_receiver_promotes_old);
    RUN_TEST(test_set_member_slot_instruction_exact_pair_hit_survives_receiver_rehash_and_minor_gc_prune);
    RUN_TEST(test_set_member_slot_instruction_does_not_reuse_cached_pair_for_different_receiver_same_prototype);
    RUN_TEST(test_set_member_slot_instruction_same_prototype_safe_hit_refreshes_cached_receiver_before_minor_gc_prune);
    RUN_TEST(test_object_invoke_member_omits_receiver_for_static_hidden_accessor);
    RUN_TEST(test_super_meta_get_cached_instruction_populates_two_slot_pic);
    RUN_TEST(test_super_meta_call_cached_instruction_fills_and_hits_callsite_pic);
    RUN_TEST(test_super_meta_call_cached_instruction_records_old_to_young_remembered_owner);
    RUN_TEST(test_super_dyn_call_cached_instruction_fills_and_hits_callsite_pic);
    RUN_TEST(test_super_meta_get_and_meta_set_static_cached_instructions_fill_and_hit_callsite_cache);
    RUN_TEST(test_index_contract_dispatches_without_storage_fallback);
    RUN_TEST(test_index_contract_get_preserves_receiver_when_destination_aliases);
    RUN_TEST(test_iterator_contract_dispatches_without_named_protocol_members);
    RUN_TEST(test_iteration_requires_explicit_contracts_not_named_members);

    // 闭包操作指令测试
    RUN_TEST(test_get_closure);
    RUN_TEST(test_set_closure);
    RUN_TEST(test_getupval);
    RUN_TEST(test_close_stack_value_closes_matching_upvalue);
    RUN_TEST(test_setupval);
    RUN_TEST(test_create_closure);

    // 通用算术运算指令测试（带元方法）
    RUN_TEST(test_add_generic);
    RUN_TEST(test_sub_generic);
    RUN_TEST(test_mul_generic);
    RUN_TEST(test_mul_generic_bool_bool_xor_semantics);
    RUN_TEST(test_mul_generic_signed_bool_returns_int64_product);
    RUN_TEST(test_mul_generic_unsigned_bool_returns_int64_product);
    RUN_TEST(test_mul_generic_signed_unsigned_returns_int64_product);
    RUN_TEST(test_div_generic);
    RUN_TEST(test_mod_generic);
    RUN_TEST(test_mod_generic_negative_divisor_normalizes_sign);
    RUN_TEST(test_pow_generic);
    RUN_TEST(test_neg_generic);
    RUN_TEST(test_shift_left_generic);
    RUN_TEST(test_shift_right_generic);

    // 其他比较指令测试
    RUN_TEST(test_logical_less_signed);
    RUN_TEST(test_logical_greater_equal_signed);
    RUN_TEST(test_logical_less_equal_signed);
    RUN_TEST(test_logical_not_equal);

    // 控制流指令测试
    RUN_TEST(test_jump);
    RUN_TEST(test_jump_if);

    // 函数调用指令测试
    RUN_TEST(test_function_call);
    RUN_TEST(test_function_tail_call);
    RUN_TEST(test_function_return);

    // 异常处理指令测试
    RUN_TEST(test_try_throw_catch);

    UNITY_END();
    return 0;
}
