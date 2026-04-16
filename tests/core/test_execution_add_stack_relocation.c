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
        TZrPtr freshPointer = malloc(newSize);
        if (freshPointer != ZR_NULL) {
            memset(freshPointer, 0xCD, newSize);
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
    UNITY_BEGIN();

    RUN_TEST(test_execution_add_restores_stack_destination_after_string_concat_growth);
    RUN_TEST(test_stack_grow_initializes_newly_exposed_logical_slots);
    RUN_TEST(test_state_stack_init_clears_all_initial_logical_slots_and_metadata);
    RUN_TEST(test_stack_grow_initializes_every_new_logical_slot_when_growing_by_multiple_slots);
    RUN_TEST(test_stack_grow_preserves_existing_newly_exposed_slots_across_repeated_growth);

    return UNITY_END();
}
