//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
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
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                     \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                    \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                     \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                      \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                         \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_MODULE_DIVIDER()                                                                                          \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

// 简单的测试分配器
static TZrPtr testAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
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
static SZrState *createTestState(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, &callbacks);
    if (!global)
        return ZR_NULL;

    SZrState *mainState = global->mainThreadState;
    if (mainState) {
        ZrGlobalStateInitRegistry(mainState, global);
    }

    return mainState;
}

// 销毁测试用的SZrState
static void destroyTestState(SZrState *state) {
    if (!state)
        return;

    SZrGlobalState *global = state->global;
    if (global) {
        ZrGlobalStateFree(global);
    }
}

// 调用元方法并获取结果的辅助函数
static TBool callMetaMethod(SZrState *state, SZrTypeValue *value, EZrMetaType metaType, SZrTypeValue *result,
                             SZrTypeValue *arg) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrMeta *meta = ZrValueGetMeta(state, value, metaType);
    if (meta == ZR_NULL || meta->function == ZR_NULL) {
        return ZR_FALSE;
    }

    // 保存当前栈状态
    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
    SZrCallInfo *savedCallInfo = state->callInfoList;

    // 准备调用元方法
    TZrStackValuePointer base = savedStackTop;
    TZrSize argCount = (arg != ZR_NULL) ? 1 : 0;
    TZrSize totalArgs = 1 + argCount; // self + 其他参数
    ZrFunctionCheckStackAndGc(state, totalArgs, base);

    // 将 meta->function 放到栈上
    ZrStackSetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));

    // 将 self 放到栈上
    ZrStackCopyValue(state, base + 1, value);

    // 如果有参数，放到栈上
    if (arg != ZR_NULL) {
        ZrStackCopyValue(state, base + 2, arg);
    }

    state->stackTop.valuePointer = base + 1 + totalArgs;

    // 调用元方法
    ZrFunctionCallWithoutYield(state, base, 1);

    // 检查执行状态
    TBool success = ZR_FALSE;
    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        // 获取返回值
        SZrTypeValue *returnValue = ZrStackGetValue(base);
        if (result != ZR_NULL) {
            ZrValueCopy(state, result, returnValue);
        }
        success = ZR_TRUE;
    }

    // 恢复栈状态
    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;

    return success;
}

// ==================== TO_STRING 元方法测试 ====================

static void test_meta_to_string_null(void) {
    TEST_START("TO_STRING meta method for NULL type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value;
    ZrValueResetAsNull(&value);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value, ZR_META_TO_STRING, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *str = ZR_CAST_STRING(state, result.value.object);
        TNativeString strStr = ZrStringGetNativeString(str);
        TEST_ASSERT_NOT_NULL(strStr);
        TEST_ASSERT_EQUAL_STRING("null", strStr);
        TEST_PASS_CUSTOM(timer, "TO_STRING meta method for NULL type");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_STRING meta method for NULL type", "Failed to call meta method or invalid result");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_string_bool(void) {
    TEST_START("TO_STRING meta method for BOOL type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 true
    SZrTypeValue valueTrue;
    ZR_VALUE_FAST_SET(&valueTrue, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    SZrTypeValue resultTrue;
    TBool successTrue = callMetaMethod(state, &valueTrue, ZR_META_TO_STRING, &resultTrue, ZR_NULL);

    // 测试 false
    SZrTypeValue valueFalse;
    ZR_VALUE_FAST_SET(&valueFalse, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    SZrTypeValue resultFalse;
    TBool successFalse = callMetaMethod(state, &valueFalse, ZR_META_TO_STRING, &resultFalse, ZR_NULL);

    timer.endTime = clock();

    if (successTrue && resultTrue.type == ZR_VALUE_TYPE_STRING) {
        SZrString *str = ZR_CAST_STRING(state, resultTrue.value.object);
        TNativeString strStr = ZrStringGetNativeString(str);
        TEST_ASSERT_EQUAL_STRING("true", strStr);
    }

    if (successFalse && resultFalse.type == ZR_VALUE_TYPE_STRING) {
        SZrString *str = ZR_CAST_STRING(state, resultFalse.value.object);
        TNativeString strStr = ZrStringGetNativeString(str);
        TEST_ASSERT_EQUAL_STRING("false", strStr);
    }

    if (successTrue && successFalse) {
        TEST_PASS_CUSTOM(timer, "TO_STRING meta method for BOOL type");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_STRING meta method for BOOL type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_string_number(void) {
    TEST_START("TO_STRING meta method for number types");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 INT64
    SZrTypeValue valueInt;
    ZrValueInitAsInt(state, &valueInt, 12345);
    SZrTypeValue resultInt;
    TBool successInt = callMetaMethod(state, &valueInt, ZR_META_TO_STRING, &resultInt, ZR_NULL);

    // 测试 UINT64
    SZrTypeValue valueUInt;
    ZrValueInitAsUInt(state, &valueUInt, 67890);
    SZrTypeValue resultUInt;
    TBool successUInt = callMetaMethod(state, &valueUInt, ZR_META_TO_STRING, &resultUInt, ZR_NULL);

    // 测试 FLOAT
    SZrTypeValue valueFloat;
    ZrValueInitAsFloat(state, &valueFloat, 3.14159);
    SZrTypeValue resultFloat;
    TBool successFloat = callMetaMethod(state, &valueFloat, ZR_META_TO_STRING, &resultFloat, ZR_NULL);

    timer.endTime = clock();

    if (successInt && resultInt.type == ZR_VALUE_TYPE_STRING) {
        SZrString *str = ZR_CAST_STRING(state, resultInt.value.object);
        TNativeString strStr = ZrStringGetNativeString(str);
        TEST_ASSERT_NOT_NULL(strStr);
        // 验证包含数字
        TEST_ASSERT(strstr(strStr, "12345") != ZR_NULL);
    }

    if (successUInt && resultUInt.type == ZR_VALUE_TYPE_STRING) {
        SZrString *str = ZR_CAST_STRING(state, resultUInt.value.object);
        TNativeString strStr = ZrStringGetNativeString(str);
        TEST_ASSERT_NOT_NULL(strStr);
        TEST_ASSERT(strstr(strStr, "67890") != ZR_NULL);
    }

    if (successFloat && resultFloat.type == ZR_VALUE_TYPE_STRING) {
        SZrString *str = ZR_CAST_STRING(state, resultFloat.value.object);
        TNativeString strStr = ZrStringGetNativeString(str);
        TEST_ASSERT_NOT_NULL(strStr);
        TEST_ASSERT(strstr(strStr, "3.14") != ZR_NULL);
    }

    if (successInt && successUInt && successFloat) {
        TEST_PASS_CUSTOM(timer, "TO_STRING meta method for number types");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_STRING meta method for number types", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_string_string(void) {
    TEST_START("TO_STRING meta method for STRING type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrString *originalStr = ZrStringCreateFromNative(state, "Hello World");
    SZrTypeValue value;
    ZrValueInitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(originalStr));

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value, ZR_META_TO_STRING, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *str = ZR_CAST_STRING(state, result.value.object);
        TNativeString strStr = ZrStringGetNativeString(str);
        TEST_ASSERT_EQUAL_STRING("Hello World", strStr);
        // 验证返回的是同一个对象（字符串的 TO_STRING 应该直接返回自身）
        TEST_ASSERT_EQUAL_PTR(originalStr, str);
        TEST_PASS_CUSTOM(timer, "TO_STRING meta method for STRING type");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_STRING meta method for STRING type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== TO_BOOL 元方法测试 ====================

static void test_meta_to_bool_null(void) {
    TEST_START("TO_BOOL meta method for NULL type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value;
    ZrValueResetAsNull(&value);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value, ZR_META_TO_BOOL, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_FALSE(result.value.nativeObject.nativeBool);
        TEST_PASS_CUSTOM(timer, "TO_BOOL meta method for NULL type");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_BOOL meta method for NULL type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_bool_number(void) {
    TEST_START("TO_BOOL meta method for number types");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试非零整数
    SZrTypeValue valueNonZero;
    ZrValueInitAsInt(state, &valueNonZero, 42);
    SZrTypeValue resultNonZero;
    TBool successNonZero = callMetaMethod(state, &valueNonZero, ZR_META_TO_BOOL, &resultNonZero, ZR_NULL);

    // 测试零
    SZrTypeValue valueZero;
    ZrValueInitAsInt(state, &valueZero, 0);
    SZrTypeValue resultZero;
    TBool successZero = callMetaMethod(state, &valueZero, ZR_META_TO_BOOL, &resultZero, ZR_NULL);

    timer.endTime = clock();

    if (successNonZero && resultNonZero.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_TRUE(resultNonZero.value.nativeObject.nativeBool);
    }

    if (successZero && resultZero.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_FALSE(resultZero.value.nativeObject.nativeBool);
    }

    if (successNonZero && successZero) {
        TEST_PASS_CUSTOM(timer, "TO_BOOL meta method for number types");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_BOOL meta method for number types", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_bool_string(void) {
    TEST_START("TO_BOOL meta method for STRING type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试非空字符串
    SZrString *nonEmptyStr = ZrStringCreateFromNative(state, "test");
    SZrTypeValue valueNonEmpty;
    ZrValueInitAsRawObject(state, &valueNonEmpty, ZR_CAST_RAW_OBJECT_AS_SUPER(nonEmptyStr));
    SZrTypeValue resultNonEmpty;
    TBool successNonEmpty = callMetaMethod(state, &valueNonEmpty, ZR_META_TO_BOOL, &resultNonEmpty, ZR_NULL);

    // 测试空字符串
    SZrString *emptyStr = ZrStringCreateFromNative(state, "");
    SZrTypeValue valueEmpty;
    ZrValueInitAsRawObject(state, &valueEmpty, ZR_CAST_RAW_OBJECT_AS_SUPER(emptyStr));
    SZrTypeValue resultEmpty;
    TBool successEmpty = callMetaMethod(state, &valueEmpty, ZR_META_TO_BOOL, &resultEmpty, ZR_NULL);

    timer.endTime = clock();

    if (successNonEmpty && resultNonEmpty.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_TRUE(resultNonEmpty.value.nativeObject.nativeBool);
    }

    if (successEmpty && resultEmpty.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_FALSE(resultEmpty.value.nativeObject.nativeBool);
    }

    if (successNonEmpty && successEmpty) {
        TEST_PASS_CUSTOM(timer, "TO_BOOL meta method for STRING type");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_BOOL meta method for STRING type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== TO_INT/TO_UINT/TO_FLOAT 元方法测试 ====================

static void test_meta_to_int(void) {
    TEST_START("TO_INT meta method");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试从 UINT 转换
    SZrTypeValue valueUInt;
    ZrValueInitAsUInt(state, &valueUInt, 123);
    SZrTypeValue result;
    TBool success = callMetaMethod(state, &valueUInt, ZR_META_TO_INT, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT_EQUAL_INT64(123, result.value.nativeObject.nativeInt64);
        TEST_PASS_CUSTOM(timer, "TO_INT meta method");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_INT meta method", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_uint(void) {
    TEST_START("TO_UINT meta method");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试从 INT 转换
    SZrTypeValue valueInt;
    ZrValueInitAsInt(state, &valueInt, 456);
    SZrTypeValue result;
    TBool success = callMetaMethod(state, &valueInt, ZR_META_TO_UINT, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(456, result.value.nativeObject.nativeUInt64);
        TEST_PASS_CUSTOM(timer, "TO_UINT meta method");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_UINT meta method", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_float(void) {
    TEST_START("TO_FLOAT meta method");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试从 INT 转换
    SZrTypeValue valueInt;
    ZrValueInitAsInt(state, &valueInt, 789);
    SZrTypeValue result;
    TBool success = callMetaMethod(state, &valueInt, ZR_META_TO_FLOAT, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        TEST_ASSERT_EQUAL_DOUBLE(789.0, result.value.nativeObject.nativeDouble);
        TEST_PASS_CUSTOM(timer, "TO_FLOAT meta method");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_FLOAT meta method", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== ADD 元方法测试 ====================

static void test_meta_add_int(void) {
    TEST_START("ADD meta method for INT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsInt(state, &value1, 10);
    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 20);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_ADD, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT_EQUAL_INT64(30, result.value.nativeObject.nativeInt64);
        TEST_PASS_CUSTOM(timer, "ADD meta method for INT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "ADD meta method for INT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_add_uint(void) {
    TEST_START("ADD meta method for UINT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsUInt(state, &value1, 15);
    SZrTypeValue value2;
    ZrValueInitAsUInt(state, &value2, 25);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_ADD, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(40, result.value.nativeObject.nativeUInt64);
        TEST_PASS_CUSTOM(timer, "ADD meta method for UINT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "ADD meta method for UINT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_add_float(void) {
    TEST_START("ADD meta method for FLOAT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsFloat(state, &value1, 1.5);
    SZrTypeValue value2;
    ZrValueInitAsFloat(state, &value2, 2.5);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_ADD, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        TEST_ASSERT_EQUAL_DOUBLE(4.0, result.value.nativeObject.nativeDouble);
        TEST_PASS_CUSTOM(timer, "ADD meta method for FLOAT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "ADD meta method for FLOAT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_add_string(void) {
    TEST_START("ADD meta method for STRING type (concatenation)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrString *str1 = ZrStringCreateFromNative(state, "Hello");
    SZrTypeValue value1;
    ZrValueInitAsRawObject(state, &value1, ZR_CAST_RAW_OBJECT_AS_SUPER(str1));

    SZrString *str2 = ZrStringCreateFromNative(state, " World");
    SZrTypeValue value2;
    ZrValueInitAsRawObject(state, &value2, ZR_CAST_RAW_OBJECT_AS_SUPER(str2));

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_ADD, &result, &value2);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *resultStr = ZR_CAST_STRING(state, result.value.object);
        TNativeString resultNative = ZrStringGetNativeString(resultStr);
        TEST_ASSERT_EQUAL_STRING("Hello World", resultNative);
        TEST_PASS_CUSTOM(timer, "ADD meta method for STRING type (concatenation)");
    } else {
        TEST_FAIL_CUSTOM(timer, "ADD meta method for STRING type (concatenation)", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_add_bool(void) {
    TEST_START("ADD meta method for BOOL type (logical OR)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 true || false
    SZrTypeValue value1;
    ZR_VALUE_FAST_SET(&value1, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    SZrTypeValue value2;
    ZR_VALUE_FAST_SET(&value2, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_ADD, &result, &value2);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);
        TEST_PASS_CUSTOM(timer, "ADD meta method for BOOL type (logical OR)");
    } else {
        TEST_FAIL_CUSTOM(timer, "ADD meta method for BOOL type (logical OR)", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== SUB 元方法测试 ====================

static void test_meta_sub_int(void) {
    TEST_START("SUB meta method for INT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsInt(state, &value1, 30);
    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 10);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_SUB, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT_EQUAL_INT64(20, result.value.nativeObject.nativeInt64);
        TEST_PASS_CUSTOM(timer, "SUB meta method for INT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "SUB meta method for INT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_sub_uint(void) {
    TEST_START("SUB meta method for UINT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsUInt(state, &value1, 50);
    SZrTypeValue value2;
    ZrValueInitAsUInt(state, &value2, 20);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_SUB, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(30, result.value.nativeObject.nativeUInt64);
        TEST_PASS_CUSTOM(timer, "SUB meta method for UINT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "SUB meta method for UINT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_sub_float(void) {
    TEST_START("SUB meta method for FLOAT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsFloat(state, &value1, 5.5);
    SZrTypeValue value2;
    ZrValueInitAsFloat(state, &value2, 2.5);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_SUB, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        TEST_ASSERT_EQUAL_DOUBLE(3.0, result.value.nativeObject.nativeDouble);
        TEST_PASS_CUSTOM(timer, "SUB meta method for FLOAT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "SUB meta method for FLOAT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_sub_string_int(void) {
    TEST_START("SUB meta method for STRING type (subtract integer)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrString *str = ZrStringCreateFromNative(state, "Hello World");
    SZrTypeValue value1;
    ZrValueInitAsRawObject(state, &value1, ZR_CAST_RAW_OBJECT_AS_SUPER(str));

    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 5); // 删除后5个字符

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_SUB, &result, &value2);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *resultStr = ZR_CAST_STRING(state, result.value.object);
        TNativeString resultNative = ZrStringGetNativeString(resultStr);
        TEST_ASSERT_EQUAL_STRING("Hello ", resultNative);
        TEST_PASS_CUSTOM(timer, "SUB meta method for STRING type (subtract integer)");
    } else {
        TEST_FAIL_CUSTOM(timer, "SUB meta method for STRING type (subtract integer)", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_sub_string_string(void) {
    TEST_START("SUB meta method for STRING type (subtract string)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrString *str1 = ZrStringCreateFromNative(state, "Hello World");
    SZrTypeValue value1;
    ZrValueInitAsRawObject(state, &value1, ZR_CAST_RAW_OBJECT_AS_SUPER(str1));

    SZrString *str2 = ZrStringCreateFromNative(state, "World");
    SZrTypeValue value2;
    ZrValueInitAsRawObject(state, &value2, ZR_CAST_RAW_OBJECT_AS_SUPER(str2));

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_SUB, &result, &value2);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *resultStr = ZR_CAST_STRING(state, result.value.object);
        TNativeString resultNative = ZrStringGetNativeString(resultStr);
        TEST_ASSERT_EQUAL_STRING("Hello ", resultNative);
        TEST_PASS_CUSTOM(timer, "SUB meta method for STRING type (subtract string)");
    } else {
        TEST_FAIL_CUSTOM(timer, "SUB meta method for STRING type (subtract string)", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_sub_bool(void) {
    TEST_START("SUB meta method for BOOL type (logical AND)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 true && false
    SZrTypeValue value1;
    ZR_VALUE_FAST_SET(&value1, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    SZrTypeValue value2;
    ZR_VALUE_FAST_SET(&value2, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_SUB, &result, &value2);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_FALSE(result.value.nativeObject.nativeBool);
        TEST_PASS_CUSTOM(timer, "SUB meta method for BOOL type (logical AND)");
    } else {
        TEST_FAIL_CUSTOM(timer, "SUB meta method for BOOL type (logical AND)", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== MUL 元方法测试 ====================

static void test_meta_mul_int(void) {
    TEST_START("MUL meta method for INT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsInt(state, &value1, 6);
    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 7);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_MUL, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT_EQUAL_INT64(42, result.value.nativeObject.nativeInt64);
        TEST_PASS_CUSTOM(timer, "MUL meta method for INT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "MUL meta method for INT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_mul_uint(void) {
    TEST_START("MUL meta method for UINT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsUInt(state, &value1, 8);
    SZrTypeValue value2;
    ZrValueInitAsUInt(state, &value2, 9);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_MUL, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(72, result.value.nativeObject.nativeUInt64);
        TEST_PASS_CUSTOM(timer, "MUL meta method for UINT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "MUL meta method for UINT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_mul_float(void) {
    TEST_START("MUL meta method for FLOAT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsFloat(state, &value1, 2.5);
    SZrTypeValue value2;
    ZrValueInitAsFloat(state, &value2, 4.0);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_MUL, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        TEST_ASSERT_EQUAL_DOUBLE(10.0, result.value.nativeObject.nativeDouble);
        TEST_PASS_CUSTOM(timer, "MUL meta method for FLOAT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "MUL meta method for FLOAT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_mul_string_int(void) {
    TEST_START("MUL meta method for STRING type (multiply integer)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrString *str = ZrStringCreateFromNative(state, "Hi");
    SZrTypeValue value1;
    ZrValueInitAsRawObject(state, &value1, ZR_CAST_RAW_OBJECT_AS_SUPER(str));

    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 3); // 复制3次

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_MUL, &result, &value2);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *resultStr = ZR_CAST_STRING(state, result.value.object);
        TNativeString resultNative = ZrStringGetNativeString(resultStr);
        TEST_ASSERT_EQUAL_STRING("HiHiHi", resultNative);
        TEST_PASS_CUSTOM(timer, "MUL meta method for STRING type (multiply integer)");
    } else {
        TEST_FAIL_CUSTOM(timer, "MUL meta method for STRING type (multiply integer)", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_mul_bool(void) {
    TEST_START("MUL meta method for BOOL type (XOR)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 true XOR false
    SZrTypeValue value1;
    ZR_VALUE_FAST_SET(&value1, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    SZrTypeValue value2;
    ZR_VALUE_FAST_SET(&value2, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_MUL, &result, &value2);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool); // true XOR false = true
        TEST_PASS_CUSTOM(timer, "MUL meta method for BOOL type (XOR)");
    } else {
        TEST_FAIL_CUSTOM(timer, "MUL meta method for BOOL type (XOR)", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== DIV 元方法测试 ====================

static void test_meta_div_int(void) {
    TEST_START("DIV meta method for INT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsInt(state, &value1, 20);
    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 4);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_DIV, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT_EQUAL_INT64(5, result.value.nativeObject.nativeInt64);
        TEST_PASS_CUSTOM(timer, "DIV meta method for INT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "DIV meta method for INT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_div_int_zero(void) {
    TEST_START("DIV meta method for INT type (divide by zero)");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsInt(state, &value1, 20);
    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 0);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_DIV, &result, &value2);

    timer.endTime = clock();

    // 除零应该返回 null
    if (success && result.type == ZR_VALUE_TYPE_NULL) {
        TEST_PASS_CUSTOM(timer, "DIV meta method for INT type (divide by zero)");
    } else {
        TEST_FAIL_CUSTOM(timer, "DIV meta method for INT type (divide by zero)", "Should return null for division by zero");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_div_uint(void) {
    TEST_START("DIV meta method for UINT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsUInt(state, &value1, 30);
    SZrTypeValue value2;
    ZrValueInitAsUInt(state, &value2, 5);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_DIV, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type)) {
        TEST_ASSERT_EQUAL_UINT64(6, result.value.nativeObject.nativeUInt64);
        TEST_PASS_CUSTOM(timer, "DIV meta method for UINT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "DIV meta method for UINT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_div_float(void) {
    TEST_START("DIV meta method for FLOAT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrTypeValue value1;
    ZrValueInitAsFloat(state, &value1, 15.0);
    SZrTypeValue value2;
    ZrValueInitAsFloat(state, &value2, 3.0);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_DIV, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_FLOAT(result.type)) {
        TEST_ASSERT_EQUAL_DOUBLE(5.0, result.value.nativeObject.nativeDouble);
        TEST_PASS_CUSTOM(timer, "DIV meta method for FLOAT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "DIV meta method for FLOAT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== COMPARE 元方法测试 ====================

static void test_meta_compare_int(void) {
    TEST_START("COMPARE meta method for INT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 10 > 5
    SZrTypeValue value1;
    ZrValueInitAsInt(state, &value1, 10);
    SZrTypeValue value2;
    ZrValueInitAsInt(state, &value2, 5);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_COMPARE, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT(result.value.nativeObject.nativeInt64 > 0); // 10 > 5, 应该返回正数
        TEST_PASS_CUSTOM(timer, "COMPARE meta method for INT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "COMPARE meta method for INT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_compare_uint(void) {
    TEST_START("COMPARE meta method for UINT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 20 < 30
    SZrTypeValue value1;
    ZrValueInitAsUInt(state, &value1, 20);
    SZrTypeValue value2;
    ZrValueInitAsUInt(state, &value2, 30);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_COMPARE, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT(result.value.nativeObject.nativeInt64 < 0); // 20 < 30, 应该返回负数
        TEST_PASS_CUSTOM(timer, "COMPARE meta method for UINT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "COMPARE meta method for UINT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_compare_float(void) {
    TEST_START("COMPARE meta method for FLOAT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 3.14 == 3.14
    SZrTypeValue value1;
    ZrValueInitAsFloat(state, &value1, 3.14);
    SZrTypeValue value2;
    ZrValueInitAsFloat(state, &value2, 3.14);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_COMPARE, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT_EQUAL_INT64(0, result.value.nativeObject.nativeInt64); // 相等应该返回0
        TEST_PASS_CUSTOM(timer, "COMPARE meta method for FLOAT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "COMPARE meta method for FLOAT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_compare_string(void) {
    TEST_START("COMPARE meta method for STRING type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 "abc" < "def" (字典序)
    SZrString *str1 = ZrStringCreateFromNative(state, "abc");
    SZrTypeValue value1;
    ZrValueInitAsRawObject(state, &value1, ZR_CAST_RAW_OBJECT_AS_SUPER(str1));

    SZrString *str2 = ZrStringCreateFromNative(state, "def");
    SZrTypeValue value2;
    ZrValueInitAsRawObject(state, &value2, ZR_CAST_RAW_OBJECT_AS_SUPER(str2));

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_COMPARE, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT(result.value.nativeObject.nativeInt64 < 0); // "abc" < "def", 应该返回负数
        TEST_PASS_CUSTOM(timer, "COMPARE meta method for STRING type");
    } else {
        TEST_FAIL_CUSTOM(timer, "COMPARE meta method for STRING type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_compare_bool(void) {
    TEST_START("COMPARE meta method for BOOL type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 true > false (true=1, false=0)
    SZrTypeValue value1;
    ZR_VALUE_FAST_SET(&value1, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    SZrTypeValue value2;
    ZR_VALUE_FAST_SET(&value2, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value1, ZR_META_COMPARE, &result, &value2);

    timer.endTime = clock();

    if (success && ZR_VALUE_IS_TYPE_INT(result.type)) {
        TEST_ASSERT(result.value.nativeObject.nativeInt64 > 0); // true > false, 应该返回正数
        TEST_PASS_CUSTOM(timer, "COMPARE meta method for BOOL type");
    } else {
        TEST_FAIL_CUSTOM(timer, "COMPARE meta method for BOOL type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== NEG 元方法测试 ====================

static void test_meta_neg_bool(void) {
    TEST_START("NEG meta method for BOOL type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 测试 !true
    SZrTypeValue value;
    ZR_VALUE_FAST_SET(&value, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value, ZR_META_NEG, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_FALSE(result.value.nativeObject.nativeBool); // !true = false
        TEST_PASS_CUSTOM(timer, "NEG meta method for BOOL type");
    } else {
        TEST_FAIL_CUSTOM(timer, "NEG meta method for BOOL type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== OBJECT 元方法测试 ====================

static void test_meta_to_string_object(void) {
    TEST_START("TO_STRING meta method for OBJECT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 创建一个简单的对象
    SZrObject *object = ZrObjectNewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(object);

    SZrTypeValue value;
    ZrValueInitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value, ZR_META_TO_STRING, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        SZrString *resultStr = ZR_CAST_STRING(state, result.value.object);
        TNativeString resultNative = ZrStringGetNativeString(resultStr);
        TEST_ASSERT_NOT_NULL(resultNative);
        // 应该包含 "[object type="
        TEST_ASSERT(strstr(resultNative, "[object type=") != ZR_NULL);
        TEST_PASS_CUSTOM(timer, "TO_STRING meta method for OBJECT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_STRING meta method for OBJECT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

static void test_meta_to_bool_object(void) {
    TEST_START("TO_BOOL meta method for OBJECT type");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 创建一个简单的对象
    SZrObject *object = ZrObjectNewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(object);

    SZrTypeValue value;
    ZrValueInitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));

    SZrTypeValue result;
    TBool success = callMetaMethod(state, &value, ZR_META_TO_BOOL, &result, ZR_NULL);

    timer.endTime = clock();

    if (success && result.type == ZR_VALUE_TYPE_BOOL) {
        // 对象默认应该返回 true
        TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);
        TEST_PASS_CUSTOM(timer, "TO_BOOL meta method for OBJECT type");
    } else {
        TEST_FAIL_CUSTOM(timer, "TO_BOOL meta method for OBJECT type", "Failed to call meta method");
    }

    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== 主测试函数 ====================

int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Meta Method Tests\n");
    TEST_MODULE_DIVIDER();

    // TO_STRING 元方法测试
    RUN_TEST(test_meta_to_string_null);
    RUN_TEST(test_meta_to_string_bool);
    RUN_TEST(test_meta_to_string_number);
    RUN_TEST(test_meta_to_string_string);
    RUN_TEST(test_meta_to_string_object);

    // TO_BOOL 元方法测试
    RUN_TEST(test_meta_to_bool_null);
    RUN_TEST(test_meta_to_bool_number);
    RUN_TEST(test_meta_to_bool_string);
    RUN_TEST(test_meta_to_bool_object);

    // TO_INT/TO_UINT/TO_FLOAT 元方法测试
    RUN_TEST(test_meta_to_int);
    RUN_TEST(test_meta_to_uint);
    RUN_TEST(test_meta_to_float);

    // ADD 元方法测试
    RUN_TEST(test_meta_add_int);
    RUN_TEST(test_meta_add_uint);
    RUN_TEST(test_meta_add_float);
    RUN_TEST(test_meta_add_string);
    RUN_TEST(test_meta_add_bool);

    // SUB 元方法测试
    RUN_TEST(test_meta_sub_int);
    RUN_TEST(test_meta_sub_uint);
    RUN_TEST(test_meta_sub_float);
    RUN_TEST(test_meta_sub_string_int);
    RUN_TEST(test_meta_sub_string_string);
    RUN_TEST(test_meta_sub_bool);

    // MUL 元方法测试
    RUN_TEST(test_meta_mul_int);
    RUN_TEST(test_meta_mul_uint);
    RUN_TEST(test_meta_mul_float);
    RUN_TEST(test_meta_mul_string_int);
    RUN_TEST(test_meta_mul_bool);

    // DIV 元方法测试
    RUN_TEST(test_meta_div_int);
    RUN_TEST(test_meta_div_int_zero);
    RUN_TEST(test_meta_div_uint);
    RUN_TEST(test_meta_div_float);

    // COMPARE 元方法测试
    RUN_TEST(test_meta_compare_int);
    RUN_TEST(test_meta_compare_uint);
    RUN_TEST(test_meta_compare_float);
    RUN_TEST(test_meta_compare_string);
    RUN_TEST(test_meta_compare_bool);

    // NEG 元方法测试
    RUN_TEST(test_meta_neg_bool);

    // OBJECT 元方法测试
    RUN_TEST(test_meta_to_string_object);
    RUN_TEST(test_meta_to_bool_object);

    return UNITY_END();
}