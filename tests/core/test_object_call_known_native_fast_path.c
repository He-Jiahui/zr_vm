#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library.h"

#include "object/object_call_internal.h"
#include "native_binding/native_binding_internal.h"

typedef struct TestPoisoningAllocatorContext {
    TZrUInt32 moveCount;
} TestPoisoningAllocatorContext;

static SZrObject *gExpectedReceiverObject = ZR_NULL;
static SZrString *gExpectedArgumentString = ZR_NULL;
static TZrInt64 gExpectedAssignedInt = 0;
static TZrUInt32 gNativeCallCount = 0;
static TZrBool gObservedCorruption = ZR_FALSE;

static TZrBool test_binding_cache_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(result);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static const ZrLibFunctionDescriptor kBindingCacheFunctions[] = {
        {
                .name = "probeA",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = test_binding_cache_callback,
                .returnTypeName = "null",
                .documentation = "Probe binding A.",
                .parameters = ZR_NULL,
                .parameterCount = 0,
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .contractRole = 0u,
        },
        {
                .name = "probeB",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = test_binding_cache_callback,
                .returnTypeName = "null",
                .documentation = "Probe binding B.",
                .parameters = ZR_NULL,
                .parameterCount = 0,
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
                .contractRole = 0u,
        },
};

static const ZrLibModuleDescriptor kBindingCacheModule = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "probe.binding_cache",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = kBindingCacheFunctions,
        .functionCount = ZR_ARRAY_COUNT(kBindingCacheFunctions),
        .types = ZR_NULL,
        .typeCount = 0,
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Probe module for native binding lookup cache tests.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
        .onMaterialize = ZR_NULL,
};

static TZrPtr test_poisoning_allocator(TZrPtr userData,
                                       TZrPtr pointer,
                                       TZrSize originalSize,
                                       TZrSize newSize,
                                       TZrInt64 flag) {
    TestPoisoningAllocatorContext *context = (TestPoisoningAllocatorContext *)userData;
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
        memset(pointer, 0xE5, originalSize);
    }

    if (context != ZR_NULL) {
        context->moveCount++;
    }

    return newPointer;
}

static SZrState *test_create_state_with_poisoning_allocator(TestPoisoningAllocatorContext *context) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_poisoning_allocator, context, 12345, &callbacks);
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

static TZrInt64 test_native_verify_stack_rooted_receiver_and_argument(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase = callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;
    SZrTypeValue *returnValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    SZrTypeValue *receiverValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;
    SZrTypeValue *argumentValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 2) : ZR_NULL;

    gNativeCallCount++;
    if (returnValue == ZR_NULL ||
        receiverValue == ZR_NULL ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        argumentValue == ZR_NULL ||
        argumentValue->type != ZR_VALUE_TYPE_STRING ||
        argumentValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString)) {
        gObservedCorruption = ZR_TRUE;
        if (returnValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(returnValue);
        }
        return 1;
    }

    ZrCore_Value_InitAsInt(state, returnValue, 4242);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrInt64 test_native_verify_stack_rooted_receiver_key_and_value(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase = callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;
    SZrTypeValue *returnValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    SZrTypeValue *receiverValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;
    SZrTypeValue *keyValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 2) : ZR_NULL;
    SZrTypeValue *assignedValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 3) : ZR_NULL;

    gNativeCallCount++;
    if (returnValue == ZR_NULL ||
        receiverValue == ZR_NULL ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        keyValue == ZR_NULL ||
        keyValue->type != ZR_VALUE_TYPE_STRING ||
        keyValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString) ||
        assignedValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(assignedValue->type) ||
        assignedValue->value.nativeObject.nativeInt64 != gExpectedAssignedInt) {
        gObservedCorruption = ZR_TRUE;
        if (returnValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(returnValue);
        }
        return 1;
    }

    ZrCore_Value_InitAsInt(state, returnValue, 2024);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

void setUp(void) {}

void tearDown(void) {}

static void reset_profile_counters(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    profileRuntime->recordHelpers = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
}

static void clear_profile_counters(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        state->global->profileRuntime = ZR_NULL;
    }
    ZrCore_Profile_SetCurrentState(ZR_NULL);
}

static void test_object_call_known_native_fast_path_restores_stack_rooted_inputs_after_growth(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer originalStackBase;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_and_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackTail.valuePointer - 3;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    originalStackBase = state->stackBase.valuePointer;

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiver(state,
                                                            ZR_CAST(SZrFunction *, nativeClosure),
                                                            receiverValue,
                                                            argumentValue,
                                                            1,
                                                            resultValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_TRUE(resultValue != movedResultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 3, state->stackTop.valuePointer);

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_known_native_fast_path_restores_outer_frame_bounds_after_growth(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer originalStackBase;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *reservedReturnValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_and_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackTail.valuePointer - 4;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    reservedReturnValue = ZrCore_Stack_GetValue(functionBase + 3);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);
    TEST_ASSERT_NOT_NULL(reservedReturnValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(reservedReturnValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->baseCallInfo.returnDestination = functionBase + 3;
    state->baseCallInfo.returnDestinationReusableOffset = 0;
    state->baseCallInfo.hasReturnDestination = ZR_TRUE;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    originalStackBase = state->stackBase.valuePointer;

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiver(state,
                                                            ZR_CAST(SZrFunction *, nativeClosure),
                                                            receiverValue,
                                                            argumentValue,
                                                            1,
                                                            resultValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 3, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 4,
                          state->callInfoList->functionTop.valuePointer);
    TEST_ASSERT_TRUE(state->callInfoList->hasReturnDestination);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 3,
                          state->callInfoList->returnDestination);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL,
                          ZrCore_Stack_GetValue(state->callInfoList->returnDestination)->type);

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_known_native_fast_path_preserves_receiver_when_result_aliases_receiver_slot(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_and_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    receiverValue = ZrCore_Stack_GetValue(functionBase);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiver(state,
                                                            ZR_CAST(SZrFunction *, nativeClosure),
                                                            receiverValue,
                                                            argumentValue,
                                                            1,
                                                            receiverValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 2, state->stackTop.valuePointer);

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_known_native_fast_path_accepts_non_stack_gc_inputs(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrString *argumentString;
    SZrTypeValue receiverValue;
    SZrTypeValue argumentValue;
    SZrTypeValue resultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_and_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(&resultValue);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiver(state,
                                                            ZR_CAST(SZrFunction *, nativeClosure),
                                                            &receiverValue,
                                                            &argumentValue,
                                                            1,
                                                            &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(4242, resultValue.value.nativeObject.nativeInt64);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "fallback object-call on a known native callable should bypass the generic PreCall dispatcher");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_library_call_value_known_native_path_bypasses_generic_precall_dispatcher(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrString *argumentString;
    SZrTypeValue callableValue;
    SZrTypeValue receiverValue;
    SZrTypeValue argumentValue;
    SZrTypeValue resultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_and_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    ZrCore_Value_InitAsRawObject(state, &callableValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
    callableValue.type = ZR_VALUE_TYPE_CLOSURE;
    callableValue.isNative = ZR_TRUE;
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(&resultValue);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrLib_CallValue(state, &callableValue, &receiverValue, &argumentValue, 1, &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(4242, resultValue.value.nativeObject.nativeInt64);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "library call-value known native path should bypass the generic PreCall dispatcher");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_get_by_index_known_native_fast_path_accepts_non_stack_gc_inputs(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue receiverValue;
    SZrTypeValue keyValue;
    SZrTypeValue resultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_and_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    prototypeName = ZrCore_String_CreateFromNative(state, "IndexedBox");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    memset(&contract, 0, sizeof(contract));
    contract.getByIndexFunction = ZR_CAST(SZrFunction *, nativeClosure);
    ZrCore_ObjectPrototype_SetIndexContract(prototype, &contract);

    receiverObject = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    keyString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(keyString);

    functionBase = state->stackTail.valuePointer - 1;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);
    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 1;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 1;

    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(&resultValue);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, &receiverValue, &keyValue, &resultValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(4242, resultValue.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 1, state->stackTop.valuePointer);

    ZrCore_GlobalState_Free(state->global);
}

static void test_set_by_index_known_native_fast_path_accepts_two_stack_rooted_arguments(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *assignedValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_key_and_value;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    prototypeName = ZrCore_String_CreateFromNative(state, "IndexedBox");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    memset(&contract, 0, sizeof(contract));
    contract.setByIndexFunction = ZR_CAST(SZrFunction *, nativeClosure);
    ZrCore_ObjectPrototype_SetIndexContract(prototype, &contract);

    receiverObject = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    keyString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(keyString);

    functionBase = state->stackTail.valuePointer - 3;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    receiverValue = ZrCore_Stack_GetValue(functionBase);
    keyValue = ZrCore_Stack_GetValue(functionBase + 1);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(assignedValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, assignedValue, 91);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 3, state->stackTop.valuePointer);

    ZrCore_GlobalState_Free(state->global);
}

static void test_set_by_index_known_native_fast_path_accepts_non_stack_gc_inputs(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue receiverValue;
    SZrTypeValue keyValue;
    SZrTypeValue assignedValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_key_and_value;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    prototypeName = ZrCore_String_CreateFromNative(state, "IndexedBox");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    memset(&contract, 0, sizeof(contract));
    contract.setByIndexFunction = ZR_CAST(SZrFunction *, nativeClosure);
    ZrCore_ObjectPrototype_SetIndexContract(prototype, &contract);

    receiverObject = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    keyString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(keyString);

    functionBase = state->stackTail.valuePointer - 1;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);
    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 1;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 1;

    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &assignedValue, 91);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, &receiverValue, &keyValue, &assignedValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 1, state->stackTop.valuePointer);

    ZrCore_GlobalState_Free(state->global);
}

static void test_native_binding_prepare_stable_value_reuses_plain_heap_object_without_release(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrTypeValue sourceValue;
    ZrLibStableValueCopy stableCopy = {0};

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    ZrCore_Value_InitAsRawObject(state, &sourceValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    sourceValue.type = ZR_VALUE_TYPE_OBJECT;

    TEST_ASSERT_TRUE(native_binding_prepare_stable_value(state, &stableCopy, &sourceValue));
    TEST_ASSERT_FALSE(stableCopy.needsRelease);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, stableCopy.value.type);
    TEST_ASSERT_EQUAL_PTR(sourceValue.value.object, stableCopy.value.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, stableCopy.value.ownershipKind);

    native_binding_release_stable_value(state, &stableCopy);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_binding_prepare_stable_value_clones_struct_and_marks_release(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *prototypeName;
    SZrStructPrototype *prototype;
    SZrObject *sourceObject;
    ZrLibStableValueCopy stableCopy = {0};
    SZrTypeValue sourceValue;

    TEST_ASSERT_NOT_NULL(state);

    prototypeName = ZrCore_String_CreateFromNative(state, "StableCopyStruct");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_StructPrototype_New(state, prototypeName);
    TEST_ASSERT_NOT_NULL(prototype);

    sourceObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_STRUCT);
    TEST_ASSERT_NOT_NULL(sourceObject);
    sourceObject->prototype = &prototype->super;
    ZrCore_Object_Init(state, sourceObject);

    ZrCore_Value_InitAsRawObject(state, &sourceValue, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceObject));
    sourceValue.type = ZR_VALUE_TYPE_OBJECT;

    TEST_ASSERT_TRUE(native_binding_prepare_stable_value(state, &stableCopy, &sourceValue));
    TEST_ASSERT_TRUE(stableCopy.needsRelease);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, stableCopy.value.type);
    TEST_ASSERT_NOT_EQUAL(sourceValue.value.object, stableCopy.value.value.object);

    native_binding_release_stable_value(state, &stableCopy);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_registry_find_binding_promotes_hot_closures_into_two_slot_cache(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrLibrary_NativeRegistryState *registry;
    SZrTypeValue callableA;
    SZrTypeValue callableB;
    SZrClosureNative *closureA;
    SZrClosureNative *closureB;
    ZrLibBindingEntry *entryA;
    ZrLibBindingEntry *entryB;
    TZrSize baseIndex;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_Attach(state->global));

    registry = native_registry_get(state->global);
    TEST_ASSERT_NOT_NULL(registry);

    baseIndex = registry->bindingEntries.length;
    TEST_ASSERT_TRUE(native_binding_make_callable_value(state,
                                                        registry,
                                                        ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                                        &kBindingCacheModule,
                                                        ZR_NULL,
                                                        ZR_NULL,
                                                        &kBindingCacheFunctions[0],
                                                        &callableA));
    TEST_ASSERT_TRUE(native_binding_make_callable_value(state,
                                                        registry,
                                                        ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                                        &kBindingCacheModule,
                                                        ZR_NULL,
                                                        ZR_NULL,
                                                        &kBindingCacheFunctions[1],
                                                        &callableB));

    closureA = ZR_CAST_NATIVE_CLOSURE(state, callableA.value.object);
    closureB = ZR_CAST_NATIVE_CLOSURE(state, callableB.value.object);
    TEST_ASSERT_NOT_NULL(closureA);
    TEST_ASSERT_NOT_NULL(closureB);

    entryA = native_registry_find_binding(registry, closureA);
    TEST_ASSERT_NOT_NULL(entryA);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheFunctions[0], entryA->descriptor.functionDescriptor);
    TEST_ASSERT_EQUAL_UINT64(baseIndex, registry->bindingLookupHotIndices[0]);
    TEST_ASSERT_EQUAL_UINT64(ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_INVALID_INDEX, registry->bindingLookupHotIndices[1]);

    entryB = native_registry_find_binding(registry, closureB);
    TEST_ASSERT_NOT_NULL(entryB);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheFunctions[1], entryB->descriptor.functionDescriptor);
    TEST_ASSERT_EQUAL_UINT64(baseIndex + 1u, registry->bindingLookupHotIndices[0]);
    TEST_ASSERT_EQUAL_UINT64(baseIndex, registry->bindingLookupHotIndices[1]);

    entryA = native_registry_find_binding(registry, closureA);
    TEST_ASSERT_NOT_NULL(entryA);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheFunctions[0], entryA->descriptor.functionDescriptor);
    TEST_ASSERT_EQUAL_UINT64(baseIndex, registry->bindingLookupHotIndices[0]);
    TEST_ASSERT_EQUAL_UINT64(baseIndex + 1u, registry->bindingLookupHotIndices[1]);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_object_call_known_native_fast_path_restores_stack_rooted_inputs_after_growth);
    RUN_TEST(test_object_call_known_native_fast_path_restores_outer_frame_bounds_after_growth);
    RUN_TEST(test_object_call_known_native_fast_path_preserves_receiver_when_result_aliases_receiver_slot);
    RUN_TEST(test_object_call_known_native_fast_path_accepts_non_stack_gc_inputs);
    RUN_TEST(test_library_call_value_known_native_path_bypasses_generic_precall_dispatcher);
    RUN_TEST(test_get_by_index_known_native_fast_path_accepts_non_stack_gc_inputs);
    RUN_TEST(test_set_by_index_known_native_fast_path_accepts_two_stack_rooted_arguments);
    RUN_TEST(test_set_by_index_known_native_fast_path_accepts_non_stack_gc_inputs);
    RUN_TEST(test_native_binding_prepare_stable_value_reuses_plain_heap_object_without_release);
    RUN_TEST(test_native_binding_prepare_stable_value_clones_struct_and_marks_release);
    RUN_TEST(test_native_registry_find_binding_promotes_hot_closures_into_two_slot_cache);

    return UNITY_END();
}
