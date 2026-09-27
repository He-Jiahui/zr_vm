#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

/* 直接调用 ADD 和其内部分支，观察临时栈扩容后目标槽是否仍可定位，
 * 并用 allocator / helper 计数锁住无需额外栈访问的快路径。 */
TZrBool try_builtin_add(SZrState *state,
                        SZrTypeValue *outResult,
                        const SZrTypeValue *opA,
                        const SZrTypeValue *opB);
TZrBool concat_values_to_destination(SZrState *state,
                                     SZrTypeValue *outResult,
                                     const SZrTypeValue *opA,
                                     const SZrTypeValue *opB,
                                     TZrBool safeMode);

/* 与移动 allocator 共用；计数只覆盖此测试状态创建后的原始内存请求。 */
typedef struct TestMovingAllocatorContext {
    TZrUInt32 moveCount;
    TZrUInt32 allocationCount;
    TZrUInt32 freeCount;
} TestMovingAllocatorContext;

/* 强制每次扩容换地址，使误用扩容前栈指针的路径在测试中可见。 */
static TZrPtr test_moving_allocator(TZrPtr userData,
                                    TZrPtr pointer,
                                    TZrSize originalSize,
                                    TZrSize newSize,
                                    TZrInt64 flag) {
    TestMovingAllocatorContext *context = (TestMovingAllocatorContext *)userData;
    TZrPtr newPointer;

    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && pointer >= (TZrPtr)0x1000) {
            if (context != ZR_NULL) {
                context->freeCount++;
            }
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL || pointer < (TZrPtr)0x1000) {
        TZrPtr freshPointer = malloc(newSize);
        if (freshPointer != ZR_NULL) {
            memset(freshPointer, 0xCD, newSize);
            if (context != ZR_NULL) {
                context->allocationCount++;
            }
        }
        return freshPointer;
    }

    newPointer = malloc(newSize);
    if (newPointer == ZR_NULL) {
        return ZR_NULL;
    }

    memset(newPointer, 0xCD, newSize);

    if (originalSize > 0) {
        memcpy(newPointer, pointer, originalSize < newSize ? originalSize : newSize);
    }
    free(pointer);
    if (context != ZR_NULL) {
        context->moveCount++;
    }
    return newPointer;
}

/* 和常规运行时夹具保持相同 registry 初始化，但替换底层 allocator。 */
static SZrState *test_create_state_with_moving_allocator(TestMovingAllocatorContext *context) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_moving_allocator, context, 12345, &callbacks);
    SZrState *state;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    state = global->mainThreadState;
    if (state != ZR_NULL) {
        ZrCore_GlobalState_InitRegistry(state, global);
    }
    return state;
}

static const TZrChar *string_value_native(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

/* 将字符串放入真实 VM 栈槽；拼接期间若栈移动，输入也需从新栈恢复。 */
static void init_stack_string_value(SZrState *state, SZrTypeValue *slotValue, SZrString *stringValue) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(slotValue);
    TEST_ASSERT_NOT_NULL(stringValue);

    ZrCore_Value_InitAsRawObject(state, slotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    slotValue->type = ZR_VALUE_TYPE_STRING;
}

void setUp(void) {}

void tearDown(void) {}

/* helper 计数是当前 state 的动态配置，测量前清零并安装到全局及当前线程。 */
static void reset_profile_counters(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    profileRuntime->recordHelpers = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
}

/* 必须在释放 state 前撤销指向测试局部 profileRuntime 的借用指针。 */
static void clear_profile_counters(SZrState *state) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);

    state->global->profileRuntime = ZR_NULL;
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

/* 栈增长不仅要重置值类型，也要清掉关闭标记和所有权元数据。 */
static void assert_stack_slot_is_reset(const SZrTypeValueOnStack *slot, const char *message) {
    TEST_ASSERT_NOT_NULL(slot);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, slot->toBeClosedValueOffset, message);
    TEST_ASSERT_TRUE_MESSAGE(ZR_VALUE_IS_TYPE_NULL(slot->value.type), message);
    TEST_ASSERT_FALSE_MESSAGE(slot->value.isGarbageCollectable, message);
    TEST_ASSERT_TRUE_MESSAGE(slot->value.isNative, message);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_OWNERSHIP_VALUE_KIND_NONE, slot->value.ownershipKind, message);
    TEST_ASSERT_NULL_MESSAGE(slot->value.ownershipControl, message);
    TEST_ASSERT_NULL_MESSAGE(slot->value.ownershipWeakRef, message);
}

/* 字符串与对象走通用拼接，需要临时栈；验证调用方保留的目标槽地址失效后仍写到新栈。 */
static void test_execution_add_restores_stack_destination_after_generic_string_concat_growth(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *leftString;
    SZrObject *rightObject;
    TZrStackValuePointer originalStackBase;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue *destinationValue;
    SZrTypeValue *movedDestinationValue;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightObject);
    ZrCore_Object_Init(state, rightObject);

    functionBase = state->stackTail.valuePointer - 4;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    leftValue = ZrCore_Stack_GetValue(functionBase + 1);
    rightValue = ZrCore_Stack_GetValue(functionBase + 2);
    destinationValue = ZrCore_Stack_GetValue(functionBase + 3);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);
    TEST_ASSERT_NOT_NULL(destinationValue);

    ZrCore_Value_InitAsRawObject(state, leftValue, ZR_CAST_RAW_OBJECT_AS_SUPER(leftString));
    leftValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, rightValue, ZR_CAST_RAW_OBJECT_AS_SUPER(rightObject));
    rightValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(destinationValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 4;

    allocatorContext.moveCount = 0;
    originalStackBase = state->stackBase.valuePointer;

    TEST_ASSERT_TRUE(ZrCore_Execution_Add(state, state->callInfoList, destinationValue, leftValue, rightValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);

    movedDestinationValue = ZrCore_Stack_GetValueNoProfile(state->callInfoList->functionBase.valuePointer + 3);
    TEST_ASSERT_NOT_NULL(movedDestinationValue);
    TEST_ASSERT_TRUE(destinationValue != movedDestinationValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, movedDestinationValue->type);
    TEST_ASSERT_EQUAL_STRING("left[object type=0]", string_value_native(state, movedDestinationValue));

    ZrCore_GlobalState_Free(state->global);
}

/* TODO: 名称称避免 helper，但断言允许最多一次；需核对通用拼接是否确实需要这次读取，
 * 再决定收紧断言还是更正测试名称。 */
static void test_execution_add_generic_string_concat_growth_avoids_stack_get_value_helper(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *leftString;
    SZrObject *rightObject;
    SZrProfileRuntime profileRuntime;
    TZrStackValuePointer originalStackBase;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue *destinationValue;
    SZrTypeValue *movedDestinationValue;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightObject);
    ZrCore_Object_Init(state, rightObject);

    functionBase = state->stackTail.valuePointer - 4;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    leftValue = ZrCore_Stack_GetValue(functionBase + 1);
    rightValue = ZrCore_Stack_GetValue(functionBase + 2);
    destinationValue = ZrCore_Stack_GetValue(functionBase + 3);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);
    TEST_ASSERT_NOT_NULL(destinationValue);

    ZrCore_Value_InitAsRawObject(state, leftValue, ZR_CAST_RAW_OBJECT_AS_SUPER(leftString));
    leftValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, rightValue, ZR_CAST_RAW_OBJECT_AS_SUPER(rightObject));
    rightValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(destinationValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 4;

    allocatorContext.moveCount = 0;
    originalStackBase = state->stackBase.valuePointer;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Execution_Add(state, state->callInfoList, destinationValue, leftValue, rightValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);

    movedDestinationValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer + 3);
    TEST_ASSERT_NOT_NULL(movedDestinationValue);
    TEST_ASSERT_TRUE(destinationValue != movedDestinationValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, movedDestinationValue->type);
    TEST_ASSERT_EQUAL_STRING("left[object type=0]", string_value_native(state, movedDestinationValue));
    TEST_ASSERT_TRUE_MESSAGE(
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE] <= 1u,
            "generic string concat growth should reduce stack restore bookkeeping to at most one remaining profiled stack_get_value read");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

/* 精确 string/string 路径绕过通用临时槽；输入占满逻辑栈时不应触发扩容。 */
static void test_try_builtin_add_exact_string_pair_avoids_scratch_stack_growth(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *leftString;
    SZrString *rightString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightString = ZrCore_String_CreateFromNative(state, "right");
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightString);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    init_stack_string_value(state, leftValue, leftString);
    init_stack_string_value(state, rightValue, rightString);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;

    allocatorContext.moveCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     allocatorContext.moveCount,
                                     "exact string pair builtin add should not grow scratch stack space");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("leftright", string_value_native(state, &result));

    ZrCore_GlobalState_Free(state->global);
}

static void test_concat_values_to_destination_exact_string_pair_avoids_scratch_stack_growth(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *leftString;
    SZrString *rightString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightString = ZrCore_String_CreateFromNative(state, "right");
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightString);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    init_stack_string_value(state, leftValue, leftString);
    init_stack_string_value(state, rightValue, rightString);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;

    allocatorContext.moveCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(concat_values_to_destination(state, &result, leftValue, rightValue, ZR_FALSE));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     allocatorContext.moveCount,
                                     "exact string pair concat helper should not grow scratch stack space");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("leftright", string_value_native(state, &result));

    ZrCore_GlobalState_Free(state->global);
}

static void test_execution_add_exact_string_pair_writes_directly_without_value_copy_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *leftString;
    SZrString *rightString;
    SZrProfileRuntime profileRuntime;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue *destinationValue;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightString = ZrCore_String_CreateFromNative(state, "right");
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 2 < state->stackTail.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    destinationValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);
    TEST_ASSERT_NOT_NULL(destinationValue);

    init_stack_string_value(state, leftValue, leftString);
    init_stack_string_value(state, rightValue, rightString);
    ZrCore_Value_ResetAsNull(destinationValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Execution_Add(state, state->callInfoList, destinationValue, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u,
                                     profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
                                     "exact string pair add should write directly to the destination without a copy-back bounce");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, destinationValue->type);
    TEST_ASSERT_EQUAL_STRING("leftright", string_value_native(state, destinationValue));

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_try_builtin_add_exact_string_pair_pinned_stack_inputs_avoid_stack_get_value_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *leftString;
    SZrString *rightString;
    SZrProfileRuntime profileRuntime;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightString = ZrCore_String_CreateFromNative(state, "right");
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightString);

    ZrCore_GarbageCollector_PinObject(state,
                                      ZR_CAST_RAW_OBJECT_AS_SUPER(leftString),
                                      ZR_GARBAGE_COLLECT_PIN_KIND_PERSISTENT_ROOT);
    ZrCore_GarbageCollector_PinObject(state,
                                      ZR_CAST_RAW_OBJECT_AS_SUPER(rightString),
                                      ZR_GARBAGE_COLLECT_PIN_KIND_PERSISTENT_ROOT);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 1 < state->stackTail.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    init_stack_string_value(state, leftValue, leftString);
    init_stack_string_value(state, rightValue, rightString);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    reset_profile_counters(state, &profileRuntime);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE],
            "pinned exact string pair should bypass stack slot restore/get-value helper work");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("leftright", string_value_native(state, &result));

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_try_builtin_add_exact_string_pair_stack_inputs_avoid_stack_get_value_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *leftString;
    SZrString *rightString;
    SZrProfileRuntime profileRuntime;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightString = ZrCore_String_CreateFromNative(state, "right");
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 1 < state->stackTail.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    init_stack_string_value(state, leftValue, leftString);
    init_stack_string_value(state, rightValue, rightString);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    reset_profile_counters(state, &profileRuntime);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE],
            "exact string pair should not restore stack slots when concat stays off the scratch stack");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("leftright", string_value_native(state, &result));

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

/* string/int 安全转换也应使用直接写入路径，避免通用拼接的临时栈。 */
static void test_try_builtin_add_mixed_string_int_avoids_scratch_stack_growth(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *leftString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    TEST_ASSERT_NOT_NULL(leftString);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    init_stack_string_value(state, leftValue, leftString);
    ZrCore_Value_InitAsInt(state, rightValue, 7);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;

    allocatorContext.moveCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     allocatorContext.moveCount,
                                     "mixed string/int builtin add should avoid scratch stack growth");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("left7", string_value_native(state, &result));

    ZrCore_GlobalState_Free(state->global);
}

static void test_try_builtin_add_mixed_int_string_avoids_scratch_stack_growth(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *rightString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    rightString = ZrCore_String_CreateFromNative(state, "right");
    TEST_ASSERT_NOT_NULL(rightString);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    ZrCore_Value_InitAsInt(state, leftValue, 7);
    init_stack_string_value(state, rightValue, rightString);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;

    allocatorContext.moveCount = 0;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     allocatorContext.moveCount,
                                     "mixed int/string builtin add should avoid scratch stack growth");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("7right", string_value_native(state, &result));

    ZrCore_GlobalState_Free(state->global);
}

static void test_try_builtin_add_mixed_string_int_stack_inputs_avoid_stack_get_value_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *leftString;
    SZrProfileRuntime profileRuntime;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    TEST_ASSERT_NOT_NULL(leftString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 1 < state->stackTail.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    init_stack_string_value(state, leftValue, leftString);
    ZrCore_Value_InitAsInt(state, rightValue, 7);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    reset_profile_counters(state, &profileRuntime);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_STACK_GET_VALUE],
            "mixed string/int concat should not restore stack slots through stack_get_value");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("left7", string_value_native(state, &result));

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

/* 计数只从调用 builtin 前开始，锁住最终字符串对象及字符串表项的两次分配预算。 */
static void test_try_builtin_add_mixed_string_int_only_allocates_final_result_string(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *leftString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue result;
    TZrUInt32 allocationCountBeforeAdd;
    TZrUInt32 freeCountBeforeAdd;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    TEST_ASSERT_NOT_NULL(leftString);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);

    init_stack_string_value(state, leftValue, leftString);
    ZrCore_Value_InitAsInt(state, rightValue, 7);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = state->stackTail.valuePointer;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = state->stackTail.valuePointer;

    ZrCore_Value_ResetAsNull(&result);
    allocationCountBeforeAdd = allocatorContext.allocationCount;
    freeCountBeforeAdd = allocatorContext.freeCount;

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            2u,
            allocatorContext.allocationCount - allocationCountBeforeAdd,
            "mixed string/int safe concat should allocate only the final result string object and its string-table entry");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            allocatorContext.freeCount - freeCountBeforeAdd,
            "mixed string/int safe concat should not allocate and free a temporary conversion buffer");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("left7", string_value_native(state, &result));

    ZrCore_GlobalState_Free(state->global);
}

static void test_execution_add_mixed_string_int_writes_directly_without_value_copy_helper(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *leftString;
    SZrProfileRuntime profileRuntime;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue *destinationValue;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    TEST_ASSERT_NOT_NULL(leftString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 2 < state->stackTail.valuePointer);

    leftValue = ZrCore_Stack_GetValue(functionBase);
    rightValue = ZrCore_Stack_GetValue(functionBase + 1);
    destinationValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(leftValue);
    TEST_ASSERT_NOT_NULL(rightValue);
    TEST_ASSERT_NOT_NULL(destinationValue);

    init_stack_string_value(state, leftValue, leftString);
    ZrCore_Value_InitAsInt(state, rightValue, 7);
    ZrCore_Value_ResetAsNull(destinationValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Execution_Add(state, state->callInfoList, destinationValue, leftValue, rightValue));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            "mixed string/int add should write directly to the destination without a copy-back bounce");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, destinationValue->type);
    TEST_ASSERT_EQUAL_STRING("left7", string_value_native(state, destinationValue));

    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

/* 混合数字测试直接约束 builtin 分派的结果类型；与执行器中的 typed opcode 测试互补。 */
static void test_try_builtin_add_signed_bool_returns_int64_sum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsInt(state, &leftValue, 7);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(8, result.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_try_builtin_add_unsigned_bool_returns_int64_sum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &leftValue, 9u);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(10, result.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_try_builtin_add_signed_unsigned_returns_int64_sum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsInt(state, &leftValue, -4);
    ZrCore_Value_InitAsUInt(state, &rightValue, 9u);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(5, result.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_try_builtin_add_bool_pair_returns_int64_sum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsBool(state, &leftValue, ZR_TRUE);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(2, result.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

/* allocator 以脏字节填充新内存，暴露只调整逻辑边界而未初始化槽位的回归。 */
static void test_stack_grow_initializes_newly_exposed_logical_slots(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    TZrSize previousStackSize;
    SZrTypeValueOnStack *newLogicalSlot;

    TEST_ASSERT_NOT_NULL(state);

    previousStackSize = ZrCore_State_StackGetSize(state);
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(state, previousStackSize + 1, ZR_TRUE));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);

    newLogicalSlot = state->stackBase.valuePointer + previousStackSize;
    assert_stack_slot_is_reset(newLogicalSlot, "first newly exposed logical slot must be reset after growth");

    ZrCore_GlobalState_Free(state->global);
}

static void test_state_stack_init_clears_all_initial_logical_slots_and_metadata(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    TZrSize stackSize;

    TEST_ASSERT_NOT_NULL(state);

    stackSize = ZrCore_State_StackGetSize(state);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)stackSize);
    for (TZrSize index = 0; index < stackSize; index++) {
        assert_stack_slot_is_reset(state->stackBase.valuePointer + index,
                                   "initial logical stack slots must be reset even when allocator returns dirty memory");
    }

    ZrCore_GlobalState_Free(state->global);
}

static void test_stack_grow_initializes_every_new_logical_slot_when_growing_by_multiple_slots(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    TZrSize previousStackSize;
    TZrSize grownSize;

    TEST_ASSERT_NOT_NULL(state);

    previousStackSize = ZrCore_State_StackGetSize(state);
    grownSize = previousStackSize + 5u;
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(state, grownSize, ZR_TRUE));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);

    for (TZrSize index = previousStackSize; index < grownSize; index++) {
        assert_stack_slot_is_reset(state->stackBase.valuePointer + index,
                                   "every newly exposed logical slot must be reset after a multi-slot growth");
    }

    ZrCore_GlobalState_Free(state->global);
}

/* 第二次增长只能清理新暴露部分，第一次增长后写入的值和关闭元数据必须保留。 */
static void test_stack_grow_preserves_existing_newly_exposed_slots_across_repeated_growth(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    TZrSize initialStackSize;
    SZrTypeValueOnStack *preservedSlot;

    TEST_ASSERT_NOT_NULL(state);

    initialStackSize = ZrCore_State_StackGetSize(state);
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(state, initialStackSize + 2u, ZR_TRUE));
    preservedSlot = state->stackBase.valuePointer + initialStackSize;
    TEST_ASSERT_NOT_NULL(preservedSlot);

    ZrCore_Value_InitAsInt(state, &preservedSlot->value, 1234);
    preservedSlot->toBeClosedValueOffset = 77u;

    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(state, initialStackSize + 5u, ZR_TRUE));

    preservedSlot = state->stackBase.valuePointer + initialStackSize;
    TEST_ASSERT_EQUAL_INT64(1234, preservedSlot->value.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(77u, preservedSlot->toBeClosedValueOffset);

    for (TZrSize index = initialStackSize + 2u; index < initialStackSize + 5u; index++) {
        assert_stack_slot_is_reset(state->stackBase.valuePointer + index,
                                   "second growth must only reset newly exposed slots beyond the previous logical size");
    }

    ZrCore_GlobalState_Free(state->global);
}

int main(void) {
    /* BUG: tests/CMakeLists.txt 构建此目标却未加入 core_runtime 或其他 add_test；
     * 常规 ctest 不执行这些栈移动与拼接回归（ctest -N 中无此目标）。 */
    UNITY_BEGIN();

    RUN_TEST(test_execution_add_restores_stack_destination_after_generic_string_concat_growth);
    RUN_TEST(test_execution_add_generic_string_concat_growth_avoids_stack_get_value_helper);
    RUN_TEST(test_try_builtin_add_exact_string_pair_avoids_scratch_stack_growth);
    RUN_TEST(test_concat_values_to_destination_exact_string_pair_avoids_scratch_stack_growth);
    RUN_TEST(test_execution_add_exact_string_pair_writes_directly_without_value_copy_helper);
    RUN_TEST(test_try_builtin_add_exact_string_pair_stack_inputs_avoid_stack_get_value_helper);
    RUN_TEST(test_try_builtin_add_exact_string_pair_pinned_stack_inputs_avoid_stack_get_value_helper);
    RUN_TEST(test_try_builtin_add_mixed_string_int_avoids_scratch_stack_growth);
    RUN_TEST(test_try_builtin_add_mixed_int_string_avoids_scratch_stack_growth);
    RUN_TEST(test_try_builtin_add_mixed_string_int_stack_inputs_avoid_stack_get_value_helper);
    RUN_TEST(test_try_builtin_add_mixed_string_int_only_allocates_final_result_string);
    RUN_TEST(test_execution_add_mixed_string_int_writes_directly_without_value_copy_helper);
    RUN_TEST(test_try_builtin_add_signed_bool_returns_int64_sum);
    RUN_TEST(test_try_builtin_add_unsigned_bool_returns_int64_sum);
    RUN_TEST(test_try_builtin_add_signed_unsigned_returns_int64_sum);
    RUN_TEST(test_try_builtin_add_bool_pair_returns_int64_sum);
    RUN_TEST(test_stack_grow_initializes_newly_exposed_logical_slots);
    RUN_TEST(test_state_stack_init_clears_all_initial_logical_slots_and_metadata);
    RUN_TEST(test_stack_grow_initializes_every_new_logical_slot_when_growing_by_multiple_slots);
    RUN_TEST(test_stack_grow_preserves_existing_newly_exposed_slots_across_repeated_growth);

    return UNITY_END();
}
