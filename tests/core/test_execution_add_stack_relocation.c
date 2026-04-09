#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "zr_vm_core/execution.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct TestMovingAllocatorContext {
    TZrUInt32 moveCount;
} TestMovingAllocatorContext;

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
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL || pointer < (TZrPtr)0x1000) {
        return malloc(newSize);
    }

    newPointer = malloc(newSize);
    if (newPointer == ZR_NULL) {
        return ZR_NULL;
    }

    if (originalSize > 0) {
        memcpy(newPointer, pointer, originalSize < newSize ? originalSize : newSize);
    }
    free(pointer);
    if (context != ZR_NULL) {
        context->moveCount++;
    }
    return newPointer;
}

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

void setUp(void) {}

void tearDown(void) {}

static void test_execution_add_restores_stack_destination_after_string_concat_growth(void) {
    TestMovingAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_moving_allocator(&allocatorContext);
    SZrString *leftString;
    SZrString *rightString;
    TZrStackValuePointer originalStackBase;
    TZrStackValuePointer functionBase;
    SZrTypeValue *leftValue;
    SZrTypeValue *rightValue;
    SZrTypeValue *destinationValue;
    SZrTypeValue *movedDestinationValue;

    TEST_ASSERT_NOT_NULL(state);

    leftString = ZrCore_String_CreateFromNative(state, "left");
    rightString = ZrCore_String_CreateFromNative(state, "right");
    TEST_ASSERT_NOT_NULL(leftString);
    TEST_ASSERT_NOT_NULL(rightString);

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
    ZrCore_Value_InitAsRawObject(state, rightValue, ZR_CAST_RAW_OBJECT_AS_SUPER(rightString));
    rightValue->type = ZR_VALUE_TYPE_STRING;
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

    movedDestinationValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer + 3);
    TEST_ASSERT_NOT_NULL(movedDestinationValue);
    TEST_ASSERT_TRUE(destinationValue != movedDestinationValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, movedDestinationValue->type);
    TEST_ASSERT_EQUAL_STRING("leftright", string_value_native(state, movedDestinationValue));

    ZrCore_GlobalState_Free(state->global);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_execution_add_restores_stack_destination_after_string_concat_growth);

    return UNITY_END();
}
