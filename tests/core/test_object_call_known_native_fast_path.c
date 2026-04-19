#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library.h"

#include "object/object_call_internal.h"
#include "native_binding/native_binding_dispatch_lanes.h"
#include "native_binding/native_binding_internal.h"

extern struct SZrCallInfo *ZrCore_Function_PreCallResolvedNativeFunction(struct SZrState *state,
                                                                         TZrStackValuePointer stackPointer,
                                                                         FZrNativeFunction nativeFunction,
                                                                         TZrSize resultCount,
                                                                         TZrStackValuePointer returnDestination);
extern TZrSize ZrCore_Function_CallPreparedResolvedNativeFunction(struct SZrState *state,
                                                                  TZrStackValuePointer stackPointer,
                                                                  TZrStackValuePointer preparedFunctionTop,
                                                                  FZrNativeFunction nativeFunction,
                                                                  TZrSize resultCount,
                                                                  TZrStackValuePointer returnDestination);
extern TZrSize ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        TZrStackValuePointer preparedFunctionTop,
        FZrNativeFunction nativeFunction,
        TZrStackValuePointer returnDestination);
extern TZrBool ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(struct SZrState *state,
                                                                     struct SZrFunction *function,
                                                                     const SZrTypeValue *receiver,
                                                                     const SZrTypeValue *argument0,
                                                                     SZrTypeValue *result);
extern TZrBool ZrCore_Object_CallFunctionWithReceiverTwoArgumentsFast(struct SZrState *state,
                                                                      struct SZrFunction *function,
                                                                      const SZrTypeValue *receiver,
                                                                      const SZrTypeValue *argument0,
                                                                      const SZrTypeValue *argument1,
                                                                      SZrTypeValue *result);

typedef struct TestPoisoningAllocatorContext {
    TZrUInt32 moveCount;
} TestPoisoningAllocatorContext;

static SZrObject *gExpectedReceiverObject = ZR_NULL;
static SZrString *gExpectedArgumentString = ZR_NULL;
static TZrInt64 gExpectedAssignedInt = 0;
static TZrUInt32 gNativeCallCount = 0;
static TZrBool gObservedCorruption = ZR_FALSE;
static TZrSize gObservedIgnoredObjectCount = 0;
static const SZrTypeValue *gExpectedReceiverValuePointer = ZR_NULL;
static const SZrTypeValue *gExpectedArgumentValuePointer = ZR_NULL;
static const SZrTypeValue *gExpectedAssignedValuePointer = ZR_NULL;
static SZrClosureValue *gCapturedClosureValue = ZR_NULL;
static TZrInt64 gExpectedCapturedInt = 0;
static TZrUInt32 gCloseMetaInvocationCount = 0;
static SZrObject *gExpectedCloseMetaObject = ZR_NULL;
static SZrTypeValue gReboundSelfValue;
static TZrInt64 gExpectedReboundSelfInt = 0;
static TZrUInt32 gNestedGenericOuterNativeCallCount = 0;
static TZrUInt32 gNestedGenericInnerNativeCallCount = 0;
static TZrUInt32 gWrappedCachedStackRootDispatchCount = 0;
static TZrUInt32 gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
static TZrUInt32 gReadonlyInlineGetFastCallCount = 0;
static TZrUInt32 gReadonlyInlineGetFallbackCallCount = 0;
static TZrUInt32 gReadonlyInlineSetFastCallCount = 0;
static TZrUInt32 gReadonlyInlineSetFallbackCallCount = 0;
static SZrString *gExpectedPrefilledResultString = ZR_NULL;

static TZrBool test_binding_cache_callback(ZrLibCallContext *context, SZrTypeValue *result) {
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(result);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool test_bound_verify_stack_rooted_receiver_and_argument(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *argumentValue = ZrLib_CallContext_Argument(context, 0);

    gNativeCallCount++;
    gObservedIgnoredObjectCount = (context != ZR_NULL && context->state != ZR_NULL && context->state->global != ZR_NULL &&
                                   context->state->global->garbageCollector != ZR_NULL)
                                          ? context->state->global->garbageCollector->ignoredObjectCount
                                          : 0u;
    if (result == ZR_NULL ||
        receiverValue == ZR_NULL ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        argumentValue == ZR_NULL ||
        argumentValue->type != ZR_VALUE_TYPE_STRING ||
        argumentValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString)) {
        gObservedCorruption = ZR_TRUE;
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(context->state, result, 4242);
    return ZR_TRUE;
}

static TZrBool test_bound_verify_stack_rooted_receiver_key_and_value(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *keyValue = ZrLib_CallContext_Argument(context, 0);
    SZrTypeValue *assignedValue = ZrLib_CallContext_Argument(context, 1);

    gNativeCallCount++;
    gObservedIgnoredObjectCount = (context != ZR_NULL && context->state != ZR_NULL && context->state->global != ZR_NULL &&
                                   context->state->global->garbageCollector != ZR_NULL)
                                          ? context->state->global->garbageCollector->ignoredObjectCount
                                          : 0u;
    if (result == ZR_NULL ||
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
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(context->state, result, 2024);
    return ZR_TRUE;
}

static TZrBool test_bound_verify_receiver_key_and_value_then_grow_stack(ZrLibCallContext *context,
                                                                        SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *keyValue = ZrLib_CallContext_Argument(context, 0);
    SZrTypeValue *assignedValue = ZrLib_CallContext_Argument(context, 1);
    TZrSize grownSize;

    gNativeCallCount++;
    gObservedIgnoredObjectCount = (context != ZR_NULL && context->state != ZR_NULL && context->state->global != ZR_NULL &&
                                   context->state->global->garbageCollector != ZR_NULL)
                                          ? context->state->global->garbageCollector->ignoredObjectCount
                                          : 0u;
    if (result == ZR_NULL ||
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
        ZrCore_Value_ResetAsNull(result);
        return ZR_TRUE;
    }

    grownSize = (TZrSize)(context->state->stackTail.valuePointer - context->state->stackBase.valuePointer) + 32u;
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(context->state, grownSize, ZR_TRUE));

    receiverValue = ZrLib_CallContext_Self(context);
    keyValue = ZrLib_CallContext_Argument(context, 0);
    assignedValue = ZrLib_CallContext_Argument(context, 1);
    if (receiverValue == ZR_NULL ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        keyValue == ZR_NULL ||
        keyValue->type != ZR_VALUE_TYPE_STRING ||
        keyValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString) ||
        assignedValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(assignedValue->type) ||
        assignedValue->value.nativeObject.nativeInt64 != gExpectedAssignedInt) {
        gObservedCorruption = ZR_TRUE;
        ZrCore_Value_ResetAsNull(result);
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(context->state, result, 2024);
    return ZR_TRUE;
}

static TZrBool test_bound_verify_prefilled_result_then_set_null(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *argumentValue = ZrLib_CallContext_Argument(context, 0);

    gNativeCallCount++;
    if (result == ZR_NULL ||
        result->type != ZR_VALUE_TYPE_STRING ||
        result->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedPrefilledResultString) ||
        receiverValue == ZR_NULL ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        argumentValue == ZR_NULL ||
        argumentValue->type != ZR_VALUE_TYPE_STRING ||
        argumentValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString)) {
        gObservedCorruption = ZR_TRUE;
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        }
        return ZR_TRUE;
    }

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static TZrBool test_bound_verify_prefilled_result_then_copy_assigned_value(ZrLibCallContext *context,
                                                                           SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *keyValue = ZrLib_CallContext_Argument(context, 0);
    SZrTypeValue *assignedValue = ZrLib_CallContext_Argument(context, 1);

    gNativeCallCount++;
    if (result == ZR_NULL ||
        result->type != ZR_VALUE_TYPE_STRING ||
        result->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedPrefilledResultString) ||
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
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        }
        return ZR_TRUE;
    }

    ZrCore_Value_Copy(context->state, result, assignedValue);
    return context->state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static TZrBool test_bound_verify_readonly_inline_value_context_reuses_input_pointers(ZrLibCallContext *context,
                                                                                     SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *argumentValue = ZrLib_CallContext_Argument(context, 0);

    gNativeCallCount++;
    if (result == ZR_NULL ||
        receiverValue == ZR_NULL ||
        receiverValue != gExpectedReceiverValuePointer ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        argumentValue == ZR_NULL ||
        argumentValue != gExpectedArgumentValuePointer ||
        argumentValue->type != ZR_VALUE_TYPE_STRING ||
        argumentValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString)) {
        gObservedCorruption = ZR_TRUE;
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(context->state, result, 5150);
    return ZR_TRUE;
}

static TZrBool test_bound_verify_readonly_inline_two_argument_value_context_reuses_input_pointers(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *keyValue = ZrLib_CallContext_Argument(context, 0);
    SZrTypeValue *assignedValue = ZrLib_CallContext_Argument(context, 1);

    gNativeCallCount++;
    if (result == ZR_NULL ||
        receiverValue == ZR_NULL ||
        receiverValue != gExpectedReceiverValuePointer ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        keyValue == ZR_NULL ||
        keyValue != gExpectedArgumentValuePointer ||
        keyValue->type != ZR_VALUE_TYPE_STRING ||
        keyValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString) ||
        assignedValue == ZR_NULL ||
        assignedValue != gExpectedAssignedValuePointer ||
        !ZR_VALUE_IS_TYPE_INT(assignedValue->type) ||
        assignedValue->value.nativeObject.nativeInt64 != gExpectedAssignedInt) {
        gObservedCorruption = ZR_TRUE;
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(context->state, result, 6160);
    return ZR_TRUE;
}

static TZrBool test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *keyValue = ZrLib_CallContext_Argument(context, 0);
    SZrTypeValue *assignedValue = ZrLib_CallContext_Argument(context, 1);

    gNativeCallCount++;
    if (result != ZR_NULL ||
        receiverValue == ZR_NULL ||
        receiverValue != gExpectedReceiverValuePointer ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        keyValue == ZR_NULL ||
        keyValue != gExpectedArgumentValuePointer ||
        keyValue->type != ZR_VALUE_TYPE_STRING ||
        keyValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString) ||
        assignedValue == ZR_NULL ||
        assignedValue != gExpectedAssignedValuePointer ||
        !ZR_VALUE_IS_TYPE_INT(assignedValue->type) ||
        assignedValue->value.nativeObject.nativeInt64 != gExpectedAssignedInt) {
        gObservedCorruption = ZR_TRUE;
        return ZR_TRUE;
    }

    return ZR_TRUE;
}

static TZrBool test_bound_readonly_inline_get_fallback_should_not_run(ZrLibCallContext *context, SZrTypeValue *result) {
    gReadonlyInlineGetFallbackCallCount++;
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(result);
    ZrCore_Value_InitAsInt(context->state, result, -111);
    return ZR_TRUE;
}

static TZrBool test_bound_readonly_inline_set_fallback_should_not_run(ZrLibCallContext *context, SZrTypeValue *result) {
    gReadonlyInlineSetFallbackCallCount++;
    ZR_UNUSED_PARAMETER(result);
    TEST_ASSERT_NOT_NULL(context);
    return ZR_TRUE;
}

static TZrBool test_direct_readonly_inline_get_fast_callback(SZrState *state,
                                                             const SZrTypeValue *selfValue,
                                                             const SZrTypeValue *argument0,
                                                             SZrTypeValue *result) {
    gReadonlyInlineGetFastCallCount++;
    gObservedIgnoredObjectCount = (state != ZR_NULL && state->global != ZR_NULL && state->global->garbageCollector != ZR_NULL)
                                          ? state->global->garbageCollector->ignoredObjectCount
                                          : 0u;
    if (state == ZR_NULL ||
        selfValue == ZR_NULL ||
        argument0 == ZR_NULL ||
        result == ZR_NULL ||
        selfValue != gExpectedReceiverValuePointer ||
        argument0 != gExpectedArgumentValuePointer ||
        selfValue->type != ZR_VALUE_TYPE_OBJECT ||
        selfValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        argument0->type != ZR_VALUE_TYPE_STRING ||
        argument0->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString)) {
        gObservedCorruption = ZR_TRUE;
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        }
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsInt(state, result, 9191);
    return ZR_TRUE;
}

static TZrBool test_direct_readonly_inline_set_fast_callback(SZrState *state,
                                                             const SZrTypeValue *selfValue,
                                                             const SZrTypeValue *argument0,
                                                             const SZrTypeValue *argument1) {
    gReadonlyInlineSetFastCallCount++;
    gObservedIgnoredObjectCount = (state != ZR_NULL && state->global != ZR_NULL && state->global->garbageCollector != ZR_NULL)
                                          ? state->global->garbageCollector->ignoredObjectCount
                                          : 0u;
    if (state == ZR_NULL ||
        selfValue == ZR_NULL ||
        argument0 == ZR_NULL ||
        argument1 == ZR_NULL ||
        selfValue != gExpectedReceiverValuePointer ||
        argument0 != gExpectedArgumentValuePointer ||
        argument1 != gExpectedAssignedValuePointer ||
        selfValue->type != ZR_VALUE_TYPE_OBJECT ||
        selfValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        argument0->type != ZR_VALUE_TYPE_STRING ||
        argument0->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString) ||
        !ZR_VALUE_IS_TYPE_INT(argument1->type) ||
        argument1->value.nativeObject.nativeInt64 != gExpectedAssignedInt) {
        gObservedCorruption = ZR_TRUE;
        return ZR_TRUE;
    }

    return ZR_TRUE;
}

static TZrInt64 test_wrapped_cached_stack_root_dispatch_one_argument(SZrState *state) {
    gWrappedCachedStackRootDispatchCount++;
    return native_binding_dispatch_cached_stack_root_one_argument(state);
}

static TZrInt64 test_wrapped_cached_stack_root_dispatch_two_arguments(SZrState *state) {
    gWrappedCachedStackRootDispatchTwoArgumentCount++;
    return native_binding_dispatch_cached_stack_root_two_arguments(state);
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
    gObservedIgnoredObjectCount = (state != ZR_NULL && state->global != ZR_NULL && state->global->garbageCollector != ZR_NULL)
                                          ? state->global->garbageCollector->ignoredObjectCount
                                          : 0u;
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

static TZrInt64 test_native_capture_open_upvalue_and_return_int(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase = callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;
    SZrTypeValue *returnValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    SZrTypeValue *capturedValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;

    gNativeCallCount++;
    gCapturedClosureValue = functionBase != ZR_NULL ? ZrCore_Closure_FindOrCreateValue(state, functionBase + 1) : ZR_NULL;
    if (returnValue == ZR_NULL ||
        capturedValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_SIGNED_INT(capturedValue->type) ||
        capturedValue->value.nativeObject.nativeInt64 != gExpectedCapturedInt ||
        gCapturedClosureValue == ZR_NULL) {
        gObservedCorruption = ZR_TRUE;
        if (returnValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(returnValue);
        }
        return 1;
    }

    ZrCore_Value_Copy(state, returnValue, capturedValue);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrInt64 test_native_register_to_be_closed_argument_and_return_int(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase = callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;
    SZrTypeValue *returnValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    SZrTypeValue *resourceValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;

    gNativeCallCount++;
    if (returnValue == ZR_NULL ||
        resourceValue == ZR_NULL ||
        resourceValue->type != ZR_VALUE_TYPE_OBJECT ||
        resourceValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedCloseMetaObject)) {
        gObservedCorruption = ZR_TRUE;
        if (returnValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(returnValue);
        }
        return 1;
    }

    ZrCore_Closure_ToBeClosedValueClosureNew(state, functionBase + 1);
    ZrCore_Value_InitAsInt(state, returnValue, 3030);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrInt64 test_native_observe_close_meta_invocation(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase = callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;
    SZrTypeValue *resourceValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase + 1) : ZR_NULL;

    if (resourceValue == ZR_NULL ||
        resourceValue->type != ZR_VALUE_TYPE_OBJECT ||
        resourceValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedCloseMetaObject)) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    gCloseMetaInvocationCount++;
    return 0;
}

static TZrInt64 test_native_return_marker_after_forced_stack_growth(SZrState *state) {
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase = callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;
    SZrTypeValue *returnValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    TZrSize grownSize;

    gNestedGenericInnerNativeCallCount++;
    if (state == ZR_NULL || callInfo == ZR_NULL || functionBase == ZR_NULL || returnValue == ZR_NULL) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    grownSize = (TZrSize)(state->stackTail.valuePointer - state->stackBase.valuePointer) + 32u;
    if (!ZrCore_Stack_GrowTo(state, grownSize, ZR_TRUE)) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    callInfo = state->callInfoList;
    functionBase = callInfo != ZR_NULL ? callInfo->functionBase.valuePointer : ZR_NULL;
    returnValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    if (callInfo == ZR_NULL || functionBase == ZR_NULL || returnValue == ZR_NULL) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    ZrCore_Value_InitAsInt(state, returnValue, 5150);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrInt64 test_native_issue_nested_generic_prepared_call_and_return_marker(SZrState *state) {
    SZrCallInfo *outerCallInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    TZrStackValuePointer functionBase = outerCallInfo != ZR_NULL ? outerCallInfo->functionBase.valuePointer : ZR_NULL;
    TZrStackValuePointer innerBase;
    SZrTypeValue *returnValue;

    gNestedGenericOuterNativeCallCount++;
    if (state == ZR_NULL || outerCallInfo == ZR_NULL || functionBase == ZR_NULL) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    innerBase = ZrCore_Function_CheckStackAndGc(state, 2u, state->stackTop.valuePointer);
    if (innerBase == ZR_NULL) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    if (ZrCore_Function_CallPreparedResolvedNativeFunction(state,
                                                           innerBase,
                                                           innerBase + 1,
                                                           test_native_return_marker_after_forced_stack_growth,
                                                           2u,
                                                           ZR_NULL) != 1u) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    if (state->callInfoList != outerCallInfo) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    functionBase = outerCallInfo->functionBase.valuePointer;
    returnValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    if (functionBase == ZR_NULL || returnValue == ZR_NULL) {
        gObservedCorruption = ZR_TRUE;
        return 0;
    }

    ZrCore_Value_InitAsInt(state, returnValue, 6060);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

static TZrBool test_native_binding_rebinds_self_to_local_int(ZrLibCallContext *context, SZrTypeValue *result) {
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(context->state);
    TEST_ASSERT_NOT_NULL(result);

    ZrCore_Value_InitAsInt(context->state, &gReboundSelfValue, gExpectedReboundSelfInt);
    context->selfValue = &gReboundSelfValue;
    ZrLib_Value_SetInt(context->state, result, 1);
    return ZR_TRUE;
}

static TZrBool test_native_binding_grows_stack_and_rebinds_self_to_local_int(ZrLibCallContext *context,
                                                                              SZrTypeValue *result) {
    TZrSize grownSize;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(context->state);
    TEST_ASSERT_NOT_NULL(result);

    grownSize = (TZrSize)(context->state->stackTail.valuePointer - context->state->stackBase.valuePointer) + 32u;
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(context->state, grownSize, ZR_TRUE));

    ZrCore_Value_InitAsInt(context->state, &gReboundSelfValue, gExpectedReboundSelfInt);
    context->selfValue = &gReboundSelfValue;
    ZrLib_Value_SetInt(context->state, result, 1);
    return ZR_TRUE;
}

static TZrBool test_native_binding_verifies_receiver_then_grows_stack_and_rebinds_self_to_local_int(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrTypeValue *receiverValue = ZrLib_CallContext_Self(context);
    SZrTypeValue *argumentValue = ZrLib_CallContext_Argument(context, 0);
    TZrSize grownSize;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(context->state);
    TEST_ASSERT_NOT_NULL(result);

    gNativeCallCount++;
    if (receiverValue == ZR_NULL ||
        receiverValue->type != ZR_VALUE_TYPE_OBJECT ||
        receiverValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedReceiverObject) ||
        argumentValue == ZR_NULL ||
        argumentValue->type != ZR_VALUE_TYPE_STRING ||
        argumentValue->value.object != ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedArgumentString)) {
        gObservedCorruption = ZR_TRUE;
        ZrCore_Value_ResetAsNull(result);
        return ZR_TRUE;
    }

    grownSize = (TZrSize)(context->state->stackTail.valuePointer - context->state->stackBase.valuePointer) + 32u;
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(context->state, grownSize, ZR_TRUE));

    ZrCore_Value_InitAsInt(context->state, &gReboundSelfValue, gExpectedReboundSelfInt);
    context->selfValue = &gReboundSelfValue;
    ZrLib_Value_SetInt(context->state, result, 1);
    return ZR_TRUE;
}

static void test_native_binding_cached_stack_root_dispatchers_match_fixed_arity_bindings(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "int", ZR_NULL}};
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "int", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_native_binding_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    static const ZrLibMetaMethodDescriptor kTwoArgumentStackRootDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_native_binding_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    static const ZrLibMetaMethodDescriptor kGenericDescriptor = {
            .metaType = ZR_META_CONSTRUCTOR,
            .minArgumentCount = 3,
            .maxArgumentCount = 3,
            .callback = test_native_binding_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = ZR_NULL,
            .parameterCount = 0,
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_NONE};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *oneArgumentClosure;
    SZrClosureNative *twoArgumentClosure;
    SZrClosureNative *genericClosure;

    TEST_ASSERT_NOT_NULL(state);

    oneArgumentClosure = ZrCore_ClosureNative_New(state, 0);
    twoArgumentClosure = ZrCore_ClosureNative_New(state, 0);
    genericClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(oneArgumentClosure);
    TEST_ASSERT_NOT_NULL(twoArgumentClosure);
    TEST_ASSERT_NOT_NULL(genericClosure);

    native_binding_closure_store_cached_binding(oneArgumentClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    native_binding_closure_store_cached_binding(twoArgumentClosure,
                                                1u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentStackRootDescriptor);
    native_binding_closure_store_cached_binding(genericClosure,
                                                2u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kGenericDescriptor);

    TEST_ASSERT_TRUE(oneArgumentClosure->nativeFunction == native_binding_dispatch_cached_stack_root_one_argument);
    TEST_ASSERT_TRUE(twoArgumentClosure->nativeFunction == native_binding_dispatch_cached_stack_root_two_arguments);
    TEST_ASSERT_TRUE(genericClosure->nativeFunction == native_binding_dispatcher);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_binding_cached_binding_primes_direct_dispatch_cache_and_clears_on_rebind(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_native_binding_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    static const ZrLibMetaMethodDescriptor kOneArgumentNoSelfRebindDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_stack_rooted_receiver_and_argument,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND};
    static const ZrLibMetaMethodDescriptor kOneArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_readonly_inline_value_context_reuses_input_pointers,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
            .readonlyInlineGetFastCallback = test_direct_readonly_inline_get_fast_callback};
    static const ZrLibMetaMethodDescriptor kGenericDescriptor = {
            .metaType = ZR_META_CONSTRUCTOR,
            .minArgumentCount = 3,
            .maxArgumentCount = 3,
            .callback = test_native_binding_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = ZR_NULL,
            .parameterCount = 0,
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_NONE};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *closure;
    SZrObjectKnownNativeDirectDispatch dispatch;

    TEST_ASSERT_NOT_NULL(state);

    closure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.callback);

    native_binding_closure_store_cached_binding(closure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);

    TEST_ASSERT_EQUAL_PTR(test_native_binding_rebinds_self_to_local_int,
                          closure->nativeBindingDirectDispatch.callback);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheModule, closure->nativeBindingDirectDispatch.moduleDescriptor);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.typeDescriptor);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.functionDescriptor);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.methodDescriptor);
    TEST_ASSERT_EQUAL_PTR(&kOneArgumentStackRootDescriptor,
                          closure->nativeBindingDirectDispatch.metaMethodDescriptor);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.readonlyInlineGetFastCallback);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.readonlyInlineSetNoResultFastCallback);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.ownerPrototype);
    TEST_ASSERT_EQUAL_UINT64(2u, closure->nativeBindingDirectDispatch.rawArgumentCount);
    TEST_ASSERT_TRUE(closure->nativeBindingDirectDispatch.usesReceiver);

    memset(&dispatch, 0xA5, sizeof(dispatch));
    TEST_ASSERT_TRUE(ZrCore_Object_TryResolveKnownNativeDirectDispatch(state,
                                                                       ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                                                                       1u,
                                                                       &dispatch));
    TEST_ASSERT_EQUAL_PTR(closure->nativeBindingDirectDispatch.callback, dispatch.callback);
    TEST_ASSERT_EQUAL_PTR(closure->nativeBindingDirectDispatch.moduleDescriptor, dispatch.moduleDescriptor);
    TEST_ASSERT_EQUAL_PTR(closure->nativeBindingDirectDispatch.metaMethodDescriptor, dispatch.metaMethodDescriptor);
    TEST_ASSERT_EQUAL_UINT64(closure->nativeBindingDirectDispatch.rawArgumentCount, dispatch.rawArgumentCount);
    TEST_ASSERT_EQUAL_UINT8(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NONE,
                            closure->nativeBindingDirectDispatch.reserved0);

    native_binding_closure_store_cached_binding(closure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentNoSelfRebindDescriptor);

    TEST_ASSERT_EQUAL_PTR(test_bound_verify_stack_rooted_receiver_and_argument,
                          closure->nativeBindingDirectDispatch.callback);
    TEST_ASSERT_EQUAL_UINT8(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND,
                            closure->nativeBindingDirectDispatch.reserved0);

    native_binding_closure_store_cached_binding(closure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentReadonlyInlineDescriptor);

    TEST_ASSERT_EQUAL_PTR(test_bound_verify_readonly_inline_value_context_reuses_input_pointers,
                          closure->nativeBindingDirectDispatch.callback);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT),
                            closure->nativeBindingDirectDispatch.reserved0);
    TEST_ASSERT_EQUAL_PTR(test_direct_readonly_inline_get_fast_callback,
                          closure->nativeBindingDirectDispatch.readonlyInlineGetFastCallback);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.readonlyInlineSetNoResultFastCallback);
    memset(&dispatch, 0xA5, sizeof(dispatch));
    TEST_ASSERT_TRUE(ZrCore_Object_TryResolveKnownNativeDirectDispatch(state,
                                                                       ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                                                                       1u,
                                                                       &dispatch));
    TEST_ASSERT_EQUAL_UINT8(closure->nativeBindingDirectDispatch.reserved0, dispatch.reserved0);
    TEST_ASSERT_EQUAL_PTR(closure->nativeBindingDirectDispatch.readonlyInlineGetFastCallback,
                          dispatch.readonlyInlineGetFastCallback);
    TEST_ASSERT_NULL(dispatch.readonlyInlineSetNoResultFastCallback);

    native_binding_closure_store_cached_binding(closure,
                                                1u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kGenericDescriptor);

    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.callback);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.moduleDescriptor);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.metaMethodDescriptor);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.readonlyInlineGetFastCallback);
    TEST_ASSERT_NULL(closure->nativeBindingDirectDispatch.readonlyInlineSetNoResultFastCallback);
    TEST_ASSERT_EQUAL_UINT64(0u, closure->nativeBindingDirectDispatch.rawArgumentCount);
    TEST_ASSERT_FALSE(closure->nativeBindingDirectDispatch.usesReceiver);
    TEST_ASSERT_EQUAL_UINT8(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NONE,
                            closure->nativeBindingDirectDispatch.reserved0);

    memset(&dispatch, 0xA5, sizeof(dispatch));
    TEST_ASSERT_FALSE(ZrCore_Object_TryResolveKnownNativeDirectDispatch(state,
                                                                        ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                                                                        3u,
                                                                        &dispatch));
    TEST_ASSERT_NULL(dispatch.callback);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_binding_init_cached_stack_root_context_overwrites_dirty_layout_state(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_native_binding_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *closure;
    ZrLibBindingEntry entry;
    ZrLibCallContext context;
    TZrStackValuePointer functionBase;
    SZrTypeValue *selfValue;
    SZrTypeValue *argumentValue;

    TEST_ASSERT_NOT_NULL(state);

    closure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(closure);
    native_binding_closure_store_cached_binding(closure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);

    memset(&entry, 0xA5, sizeof(entry));
    TEST_ASSERT_TRUE(native_binding_closure_try_build_cached_entry(closure, &entry));

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 2 < state->stackTail.valuePointer);

    selfValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(selfValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_InitAsInt(state, selfValue, 7);
    ZrCore_Value_InitAsInt(state, argumentValue, 9);

    memset(&context, 0xCC, sizeof(context));
    native_binding_init_cached_stack_root_context(&context, state, &entry, functionBase, 2u, ZR_TRUE);

    TEST_ASSERT_EQUAL_PTR(state, context.state);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheModule, context.moduleDescriptor);
    TEST_ASSERT_NULL(context.typeDescriptor);
    TEST_ASSERT_NULL(context.functionDescriptor);
    TEST_ASSERT_NULL(context.methodDescriptor);
    TEST_ASSERT_EQUAL_PTR(&kOneArgumentStackRootDescriptor, context.metaMethodDescriptor);
    TEST_ASSERT_NULL(context.ownerPrototype);
    TEST_ASSERT_NULL(context.constructTargetPrototype);
    TEST_ASSERT_EQUAL_PTR(functionBase, context.functionBase);
    TEST_ASSERT_EQUAL_PTR(functionBase + 2, context.argumentBase);
    TEST_ASSERT_EQUAL_PTR(selfValue, context.selfValue);
    TEST_ASSERT_NULL(context.argumentValues);
    TEST_ASSERT_EQUAL_UINT64(1u, context.argumentCount);
    TEST_ASSERT_EQUAL_UINT64(2u, context.rawArgumentCount);
    TEST_ASSERT_TRUE(context.stackLayoutUsesReceiver);
    TEST_ASSERT_FALSE(context.stackLayoutAnchored);
    TEST_ASSERT_NULL(context.stackBasePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_binding_init_cached_stack_root_context_from_closure_overwrites_dirty_layout_state(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_native_binding_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *closure;
    ZrLibCallContext context;
    TZrStackValuePointer functionBase;
    SZrTypeValue *selfValue;
    SZrTypeValue *argumentValue;
    FZrLibBoundCallback callback = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);

    closure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(closure);
    native_binding_closure_store_cached_binding(closure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    TEST_ASSERT_TRUE(native_binding_closure_try_get_cached_callback(closure, &callback));
    TEST_ASSERT_EQUAL_PTR(test_native_binding_rebinds_self_to_local_int, callback);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 2 < state->stackTail.valuePointer);

    selfValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(selfValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_InitAsInt(state, selfValue, 7);
    ZrCore_Value_InitAsInt(state, argumentValue, 9);

    memset(&context, 0xCC, sizeof(context));
    native_binding_init_cached_stack_root_context_from_closure(&context, state, closure, functionBase, 2u, ZR_TRUE);

    TEST_ASSERT_EQUAL_PTR(state, context.state);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheModule, context.moduleDescriptor);
    TEST_ASSERT_NULL(context.typeDescriptor);
    TEST_ASSERT_NULL(context.functionDescriptor);
    TEST_ASSERT_NULL(context.methodDescriptor);
    TEST_ASSERT_EQUAL_PTR(&kOneArgumentStackRootDescriptor, context.metaMethodDescriptor);
    TEST_ASSERT_NULL(context.ownerPrototype);
    TEST_ASSERT_NULL(context.constructTargetPrototype);
    TEST_ASSERT_EQUAL_PTR(functionBase, context.functionBase);
    TEST_ASSERT_EQUAL_PTR(functionBase + 2, context.argumentBase);
    TEST_ASSERT_EQUAL_PTR(selfValue, context.selfValue);
    TEST_ASSERT_NULL(context.argumentValues);
    TEST_ASSERT_EQUAL_UINT64(1u, context.argumentCount);
    TEST_ASSERT_EQUAL_UINT64(2u, context.rawArgumentCount);
    TEST_ASSERT_TRUE(context.stackLayoutUsesReceiver);
    TEST_ASSERT_FALSE(context.stackLayoutAnchored);
    TEST_ASSERT_NULL(context.stackBasePointer);

    ZrTests_Runtime_State_Destroy(state);
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

static void reset_profile_counters_from_state_only(SZrState *state, SZrProfileRuntime *profileRuntime) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(profileRuntime);

    memset(profileRuntime, 0, sizeof(*profileRuntime));
    profileRuntime->recordHelpers = ZR_TRUE;
    state->global->profileRuntime = profileRuntime;
    ZrCore_Profile_SetCurrentState(ZR_NULL);
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 2 < state->stackTail.valuePointer);

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

static void test_object_call_known_native_fast_path_overwrites_prefilled_future_scratch_slots(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrObject *staleObject;
    SZrString *argumentString;
    SZrString *staleString;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer futureScratchBase;
    SZrFunctionStackAnchor futureScratchBaseAnchor;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *staleCallableSlot;
    SZrTypeValue *staleReceiverSlot;
    SZrTypeValue *staleArgumentSlot;
    SZrTypeValue *movedResultValue;
    TZrBool callSucceeded;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_and_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    staleObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(staleObject);
    ZrCore_Object_Init(state, staleObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);
    staleString = ZrCore_String_CreateFromNative(state, "stale_marker");
    TEST_ASSERT_NOT_NULL(staleString);

    functionBase = state->stackTail.valuePointer - 6;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);
    futureScratchBase = functionBase + 3;

    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    staleCallableSlot = ZrCore_Stack_GetValue(futureScratchBase);
    staleReceiverSlot = ZrCore_Stack_GetValue(futureScratchBase + 1);
    staleArgumentSlot = ZrCore_Stack_GetValue(futureScratchBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);
    TEST_ASSERT_NOT_NULL(staleCallableSlot);
    TEST_ASSERT_NOT_NULL(staleReceiverSlot);
    TEST_ASSERT_NOT_NULL(staleArgumentSlot);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;

    ZrCore_Value_InitAsRawObject(state, staleCallableSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(staleString));
    staleCallableSlot->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, staleReceiverSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(staleObject));
    staleReceiverSlot->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsInt(state, staleArgumentSlot, -99);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = futureScratchBase;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;
    ZrCore_Function_StackAnchorInit(state, futureScratchBase, &futureScratchBaseAnchor);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    callSucceeded = ZrCore_Object_CallFunctionWithReceiver(state,
                                                           ZR_CAST(SZrFunction *, nativeClosure),
                                                           receiverValue,
                                                           argumentValue,
                                                           1,
                                                           resultValue);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_TRUE(callSucceeded);

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 3, state->stackTop.valuePointer);

    futureScratchBase = ZrCore_Function_StackAnchorRestore(state, &futureScratchBaseAnchor);
    staleCallableSlot = ZrCore_Stack_GetValue(futureScratchBase);
    staleReceiverSlot = ZrCore_Stack_GetValue(futureScratchBase + 1);
    staleArgumentSlot = ZrCore_Stack_GetValue(futureScratchBase + 2);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(staleCallableSlot->type));
    TEST_ASSERT_EQUAL_INT64(4242, staleCallableSlot->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, staleReceiverSlot->type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject), staleReceiverSlot->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, staleArgumentSlot->type);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString), staleArgumentSlot->value.object);

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
    gObservedIgnoredObjectCount = 0;
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
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "known-native object-call scratch staging should stay off the helper-profile value-copy path");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_known_native_fast_path_reuses_stack_roots_without_repinning_shallow_gc_values(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrString *argumentString;
    SZrTypeValue receiverValue;
    SZrTypeValue argumentValue;
    SZrTypeValue resultValue;
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);

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
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;

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
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "known-native fast path should reuse scratch stack roots instead of repinning shallow GC operands");

    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_function_with_receiver_one_argument_fast_reuses_stack_roots_without_precall_helper(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *resultValue;
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrSize ignoredObjectCountBefore;
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
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);
    TEST_ASSERT_NOT_NULL(resultValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(resultValue);

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
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(state,
                                                                           ZR_CAST(SZrFunction *, nativeClosure),
                                                                           receiverValue,
                                                                           argumentValue,
                                                                           resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "fixed-shape one-arg known-native fast call should bypass the generic PreCall helper");
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "fixed-shape one-arg known-native fast call should stay off helper-profile value-copy paths");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "fixed-shape one-arg known-native fast call should reuse stack roots without repinning shallow GC operands");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_native_binding_stack_root_one_argument_fast_bypasses_cached_dispatcher_wrapper(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_stack_rooted_receiver_and_argument,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *resultValue;
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrSize ignoredObjectCountBefore;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);
    TEST_ASSERT_NOT_NULL(resultValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(resultValue);

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
    gWrappedCachedStackRootDispatchCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(state,
                                                                           ZR_CAST(SZrFunction *, nativeClosure),
                                                                           receiverValue,
                                                                           argumentValue,
                                                                           resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "native-binding stack-root one-arg object-call fast path should bypass the generic PreCall helper");
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "native-binding stack-root one-arg object-call fast path should stay off helper-profile value-copy paths");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "native-binding stack-root one-arg object-call fast path should reuse stack roots without repinning shallow GC operands");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_native_binding_stack_root_one_argument_fast_syncs_rebound_self_across_stack_growth(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_native_binding_grows_stack_and_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer futureScratchBase;
    TZrStackValuePointer originalStackBase;
    SZrFunctionStackAnchor futureScratchBaseAnchor;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *movedResultValue;
    SZrTypeValue *movedScratchReceiverValue;
    SZrObject *receiverObject;
    SZrString *argumentString;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 5 < state->stackTail.valuePointer);
    futureScratchBase = functionBase + 3;

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
    state->baseCallInfo.functionTop.valuePointer = futureScratchBase;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = futureScratchBase;
    ZrCore_Function_StackAnchorInit(state, futureScratchBase, &futureScratchBaseAnchor);

    gExpectedReboundSelfInt = 909;
    gWrappedCachedStackRootDispatchCount = 0;
    originalStackBase = state->stackBase.valuePointer;

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(state,
                                                                           ZR_CAST(SZrFunction *, nativeClosure),
                                                                           receiverValue,
                                                                           argumentValue,
                                                                           resultValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(1, movedResultValue->value.nativeObject.nativeInt64);

    futureScratchBase = ZrCore_Function_StackAnchorRestore(state, &futureScratchBaseAnchor);
    TEST_ASSERT_NOT_NULL(futureScratchBase);
    movedScratchReceiverValue = ZrCore_Stack_GetValue(futureScratchBase + 1);
    TEST_ASSERT_NOT_NULL(movedScratchReceiverValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedScratchReceiverValue->type));
    TEST_ASSERT_EQUAL_INT64(gExpectedReboundSelfInt, movedScratchReceiverValue->value.nativeObject.nativeInt64);

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_native_binding_stack_root_one_argument_fast_preserves_aliased_receiver_across_stack_growth(
        void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_native_binding_verifies_receiver_then_grows_stack_and_rebinds_self_to_local_int,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer futureScratchBase;
    TZrStackValuePointer originalStackBase;
    SZrFunctionStackAnchor futureScratchBaseAnchor;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *movedAliasedResultValue;
    SZrTypeValue *movedScratchReceiverValue;
    SZrObject *receiverObject;
    SZrString *argumentString;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 5 < state->stackTail.valuePointer);
    futureScratchBase = functionBase + 3;

    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = futureScratchBase;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = futureScratchBase;
    ZrCore_Function_StackAnchorInit(state, futureScratchBase, &futureScratchBaseAnchor);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gExpectedReboundSelfInt = 909;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;
    originalStackBase = state->stackBase.valuePointer;

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(state,
                                                                           ZR_CAST(SZrFunction *, nativeClosure),
                                                                           receiverValue,
                                                                           argumentValue,
                                                                           receiverValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);

    movedAliasedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer + 1);
    TEST_ASSERT_NOT_NULL(movedAliasedResultValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedAliasedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(1, movedAliasedResultValue->value.nativeObject.nativeInt64);

    futureScratchBase = ZrCore_Function_StackAnchorRestore(state, &futureScratchBaseAnchor);
    TEST_ASSERT_NOT_NULL(futureScratchBase);
    movedScratchReceiverValue = ZrCore_Stack_GetValue(futureScratchBase + 1);
    TEST_ASSERT_NOT_NULL(movedScratchReceiverValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedScratchReceiverValue->type));
    TEST_ASSERT_EQUAL_INT64(gExpectedReboundSelfInt, movedScratchReceiverValue->value.nativeObject.nativeInt64);

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_known_native_fast_path_reuses_single_nested_call_info_node(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_UINT32(0u, state->callInfoListLength);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_issue_nested_generic_prepared_call_and_return_marker;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "nested_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 6 < state->stackTail.valuePointer);

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

    gObservedCorruption = ZR_FALSE;
    gNestedGenericOuterNativeCallCount = 0;
    gNestedGenericInnerNativeCallCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(state,
                                                                           ZR_CAST(SZrFunction *, nativeClosure),
                                                                           receiverValue,
                                                                           argumentValue,
                                                                           resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericOuterNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericInnerNativeCallCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(6060, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u,
                                     state->callInfoListLength,
                                     "outer known-native fast call should stay on a stack-local call-info and only extend the nested generic call");
    TEST_ASSERT_NOT_NULL(state->baseCallInfo.next);

    functionBase = state->callInfoList->functionBase.valuePointer;
    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);
    ZrCore_Value_ResetAsNull(resultValue);

    gObservedCorruption = ZR_FALSE;
    gNestedGenericOuterNativeCallCount = 0;
    gNestedGenericInnerNativeCallCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(state,
                                                                           ZR_CAST(SZrFunction *, nativeClosure),
                                                                           receiverValue,
                                                                           argumentValue,
                                                                           resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericOuterNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericInnerNativeCallCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(6060, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u,
                                     state->callInfoListLength,
                                     "nested generic call should reuse the relinked call-info node on later outer fast calls");

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
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.getByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeFunction ==
                     test_native_verify_stack_rooted_receiver_and_argument);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 1, state->stackTop.valuePointer);

    ZrCore_GlobalState_Free(state->global);
}

static void test_get_by_index_known_native_stack_root_fast_path_caches_direct_dispatch(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_stack_rooted_receiver_and_argument,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *resultValue;
    SZrTypeValue *movedResultValue;
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackTail.valuePointer - 3;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.getByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeFunction ==
                     test_wrapped_cached_stack_root_dispatch_one_argument);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_stack_rooted_receiver_and_argument);
    TEST_ASSERT_EQUAL_PTR(&kOneArgumentStackRootDescriptor,
                          prototype->indexContract.getByIndexKnownNativeDirectDispatch.metaMethodDescriptor);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheModule,
                          prototype->indexContract.getByIndexKnownNativeDirectDispatch.moduleDescriptor);
    TEST_ASSERT_EQUAL_UINT32(2u, prototype->indexContract.getByIndexKnownNativeDirectDispatch.rawArgumentCount);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeDirectDispatch.usesReceiver);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "cached get-by-index known-native direct-dispatch path should bypass generic PreCall");
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "cached get-by-index known-native direct-dispatch path should stay off helper-profile value-copy paths");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "cached get-by-index known-native direct-dispatch path should reuse stack roots without repinning shallow GC operands");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_get_by_index_known_native_cached_direct_dispatch_survives_resolver_data_loss(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_stack_rooted_receiver_and_argument,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_stack_rooted_receiver_and_argument);

    prototype->indexContract.getByIndexKnownNativeCallable = ZR_NULL;
    prototype->indexContract.getByIndexKnownNativeFunction = ZR_NULL;
    nativeClosure->nativeFunction = ZR_NULL;
    ZrCore_Value_ResetAsNull(resultValue);
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(2u, gNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_get_by_index_known_native_stack_root_fast_path_keeps_callable_scratch_slot_null(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentStackRootDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_stack_rooted_receiver_and_argument,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer scratchBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *resultValue;
    SZrTypeValue *scratchCallableValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackTail.valuePointer - 6;
    scratchBase = functionBase + 3;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);
    TEST_ASSERT_TRUE(scratchBase + 3 <= state->stackTail.valuePointer);

    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    scratchCallableValue = ZrCore_Stack_GetValue(scratchBase);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(scratchCallableValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, scratchCallableValue, 77);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL,
                          ZrCore_Stack_GetValue(scratchBase)->type);

    ZrCore_GlobalState_Free(state->global);
}

static void test_get_by_index_known_native_readonly_inline_fast_path_reuses_input_pointers(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_readonly_inline_value_context_reuses_input_pointers,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(5150, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.getByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_readonly_inline_value_context_reuses_input_pointers);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT),
                            prototype->indexContract.getByIndexKnownNativeDirectDispatch.reserved0);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_get_by_index_known_native_stack_operands_readonly_inline_fast_path_reuses_input_pointers(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_readonly_inline_value_context_reuses_input_pointers,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUncheckedStackOperands(state, receiverValue, keyValue, resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(5150, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.getByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_readonly_inline_value_context_reuses_input_pointers);

    ZrTests_Runtime_State_Destroy(state);
}

static void
test_get_by_index_known_native_stack_operands_readonly_inline_fast_path_records_helper_from_state_without_tls_current(
        void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_verify_readonly_inline_value_context_reuses_input_pointers,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUncheckedStackOperands(state, receiverValue, keyValue, resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    UNITY_TEST_ASSERT_EQUAL_UINT64(1u,
                                   profileRuntime.helperCounts[ZR_PROFILE_HELPER_GET_BY_INDEX],
                                   __LINE__,
                                   "stack-operands get-by-index should record helper counts from state-local runtime");
    UNITY_TEST_ASSERT_EQUAL_UINT64(0u,
                                   profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
                                   __LINE__,
                                   "stack-operands get-by-index fast path should still bypass generic PreCall");

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_get_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback(void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_readonly_inline_get_fallback_should_not_run,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
            .readonlyInlineGetFastCallback = test_direct_readonly_inline_get_fast_callback};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *movedResultValue;
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;
    gReadonlyInlineGetFastCallCount = 0;
    gReadonlyInlineGetFallbackCallCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(1u, gReadonlyInlineGetFastCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gReadonlyInlineGetFallbackCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(9191, movedResultValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(prototype->indexContract.getByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_readonly_inline_get_fallback_should_not_run);
    TEST_ASSERT_EQUAL_PTR(&kOneArgumentReadonlyInlineDescriptor,
                          prototype->indexContract.getByIndexKnownNativeDirectDispatch.metaMethodDescriptor);
    TEST_ASSERT_EQUAL_PTR(test_direct_readonly_inline_get_fast_callback,
                          prototype->indexContract.getByIndexKnownNativeDirectDispatch.readonlyInlineGetFastCallback);
    TEST_ASSERT_NULL(prototype->indexContract.getByIndexKnownNativeDirectDispatch.readonlyInlineSetNoResultFastCallback);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "readonly-inline get fast callback should stay off generic PreCall");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "readonly-inline get fast callback should reuse stack roots without repinning shallow GC operands");

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_get_by_index_known_native_readonly_inline_fast_path_keeps_cached_fast_callback_without_callback_pointer(
        void) {
    static const ZrLibParameterDescriptor kOneArgumentParameter[] = {{"key", "string", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kOneArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_GET_ITEM,
            .minArgumentCount = 1,
            .maxArgumentCount = 1,
            .callback = test_bound_readonly_inline_get_fallback_should_not_run,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kOneArgumentParameter,
            .parameterCount = ZR_ARRAY_COUNT(kOneArgumentParameter),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
            .readonlyInlineGetFastCallback = test_direct_readonly_inline_get_fast_callback};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *resultValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kOneArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_one_argument;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(resultValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);

    ZrCore_Value_ResetAsNull(resultValue);
    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchCount = 0;
    gReadonlyInlineGetFastCallCount = 0;
    gReadonlyInlineGetFallbackCallCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));
    TEST_ASSERT_EQUAL_UINT32(1u, gReadonlyInlineGetFastCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gReadonlyInlineGetFallbackCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_NOT_NULL(prototype->indexContract.getByIndexKnownNativeDirectDispatch.readonlyInlineGetFastCallback);

    prototype->indexContract.getByIndexKnownNativeCallable = ZR_NULL;
    prototype->indexContract.getByIndexKnownNativeFunction = ZR_NULL;
    prototype->indexContract.getByIndexKnownNativeDirectDispatch.callback = ZR_NULL;
    nativeClosure->nativeFunction = ZR_NULL;
    ZrCore_Value_ResetAsNull(resultValue);
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_GetByIndexUnchecked(state, receiverValue, keyValue, resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(2u, gReadonlyInlineGetFastCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gReadonlyInlineGetFallbackCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(9191, movedResultValue->value.nativeObject.nativeInt64);

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
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
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeFunction ==
                     test_native_verify_stack_rooted_receiver_key_and_value);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 3, state->stackTop.valuePointer);

    ZrCore_GlobalState_Free(state->global);
}

static void test_set_by_index_known_native_stack_root_fast_path_caches_direct_dispatch(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentStackRootDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_stack_rooted_receiver_key_and_value,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
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
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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

    functionBase = state->stackTail.valuePointer - 4;
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
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));

    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeFunction ==
                     test_wrapped_cached_stack_root_dispatch_two_arguments);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_stack_rooted_receiver_key_and_value);
    TEST_ASSERT_EQUAL_PTR(&kTwoArgumentStackRootDescriptor,
                          prototype->indexContract.setByIndexKnownNativeDirectDispatch.metaMethodDescriptor);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheModule,
                          prototype->indexContract.setByIndexKnownNativeDirectDispatch.moduleDescriptor);
    TEST_ASSERT_EQUAL_UINT32(3u, prototype->indexContract.setByIndexKnownNativeDirectDispatch.rawArgumentCount);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.usesReceiver);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "cached set-by-index known-native direct-dispatch path should bypass generic PreCall");
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "cached set-by-index known-native direct-dispatch path should stay off helper-profile value-copy paths");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "cached set-by-index known-native direct-dispatch path should reuse stack roots without repinning shallow GC operands");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_set_by_index_known_native_cached_direct_dispatch_survives_resolver_data_loss(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentStackRootDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_stack_rooted_receiver_key_and_value,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
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
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 4 < state->stackTail.valuePointer);
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
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_stack_rooted_receiver_key_and_value);

    prototype->indexContract.setByIndexKnownNativeCallable = ZR_NULL;
    prototype->indexContract.setByIndexKnownNativeFunction = ZR_NULL;
    nativeClosure->nativeFunction = ZR_NULL;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));
    TEST_ASSERT_EQUAL_UINT32(2u, gNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_FALSE(gObservedCorruption);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_set_by_index_known_native_readonly_inline_fast_path_can_ignore_result(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *arguments;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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
    arguments = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(arguments);
    TEST_ASSERT_NOT_NULL(arguments + 1);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &arguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    arguments[0].type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &arguments[1], 91);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = &arguments[0];
    gExpectedAssignedValuePointer = &arguments[1];
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, &arguments[0], &arguments[1]));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL),
                            prototype->indexContract.setByIndexKnownNativeDirectDispatch.reserved0);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_set_by_index_known_native_stack_operands_readonly_inline_fast_path_can_ignore_result(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *arguments;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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
    arguments = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(arguments);
    TEST_ASSERT_NOT_NULL(arguments + 1);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &arguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    arguments[0].type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &arguments[1], 91);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = &arguments[0];
    gExpectedAssignedValuePointer = &arguments[1];
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUncheckedStackOperands(state, receiverValue, &arguments[0], &arguments[1]));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result);

    ZrTests_Runtime_State_Destroy(state);
}

static void
test_set_by_index_known_native_stack_operands_readonly_inline_fast_path_records_helper_from_state_without_tls_current(
        void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *arguments;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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
    arguments = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(arguments);
    TEST_ASSERT_NOT_NULL(arguments + 1);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &arguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    arguments[0].type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &arguments[1], 91);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 3;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = &arguments[0];
    gExpectedAssignedValuePointer = &arguments[1];
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
    reset_profile_counters_from_state_only(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUncheckedStackOperands(state, receiverValue, &arguments[0], &arguments[1]));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    UNITY_TEST_ASSERT_EQUAL_UINT64(1u,
                                   profileRuntime.helperCounts[ZR_PROFILE_HELPER_SET_BY_INDEX],
                                   __LINE__,
                                   "stack-operands set-by-index should record helper counts from state-local runtime");
    UNITY_TEST_ASSERT_EQUAL_UINT64(0u,
                                   profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
                                   __LINE__,
                                   "stack-operands set-by-index fast path should still bypass generic PreCall");

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    gExpectedAssignedValuePointer = ZR_NULL;
    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_set_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_readonly_inline_set_fallback_should_not_run,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL,
            .readonlyInlineSetNoResultFastCallback = test_direct_readonly_inline_set_fast_callback};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrProfileRuntime profileRuntime;
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
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 4 < state->stackTail.valuePointer);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 3);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(assignedValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, assignedValue, 2024);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 4;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 2024;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gExpectedAssignedValuePointer = assignedValue;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
    gReadonlyInlineSetFastCallCount = 0;
    gReadonlyInlineSetFallbackCallCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gReadonlyInlineSetFastCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gReadonlyInlineSetFallbackCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_readonly_inline_set_fallback_should_not_run);
    TEST_ASSERT_EQUAL_PTR(&kTwoArgumentReadonlyInlineDescriptor,
                          prototype->indexContract.setByIndexKnownNativeDirectDispatch.metaMethodDescriptor);
    TEST_ASSERT_NULL(prototype->indexContract.setByIndexKnownNativeDirectDispatch.readonlyInlineGetFastCallback);
    TEST_ASSERT_EQUAL_PTR(test_direct_readonly_inline_set_fast_callback,
                          prototype->indexContract.setByIndexKnownNativeDirectDispatch.readonlyInlineSetNoResultFastCallback);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "readonly-inline set fast callback should stay off generic PreCall");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "readonly-inline set fast callback should reuse stack roots without repinning shallow GC operands");

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    gExpectedAssignedValuePointer = ZR_NULL;
    clear_profile_counters(state);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_set_by_index_known_native_readonly_inline_fast_path_keeps_cached_fast_callback_without_callback_pointer(
        void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_readonly_inline_set_fallback_should_not_run,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL,
            .readonlyInlineSetNoResultFastCallback = test_direct_readonly_inline_set_fast_callback};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
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
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 4 < state->stackTail.valuePointer);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 3);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(assignedValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, assignedValue, 2024);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 4;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 2024;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gExpectedAssignedValuePointer = assignedValue;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
    gReadonlyInlineSetFastCallCount = 0;
    gReadonlyInlineSetFallbackCallCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));
    TEST_ASSERT_EQUAL_UINT32(1u, gReadonlyInlineSetFastCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gReadonlyInlineSetFallbackCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_NOT_NULL(prototype->indexContract.setByIndexKnownNativeDirectDispatch.readonlyInlineSetNoResultFastCallback);

    prototype->indexContract.setByIndexKnownNativeCallable = ZR_NULL;
    prototype->indexContract.setByIndexKnownNativeFunction = ZR_NULL;
    prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback = ZR_NULL;
    nativeClosure->nativeFunction = ZR_NULL;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));

    TEST_ASSERT_EQUAL_UINT32(2u, gReadonlyInlineSetFastCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gReadonlyInlineSetFallbackCallCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_FALSE(gObservedCorruption);

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    gExpectedAssignedValuePointer = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_set_by_index_known_native_readonly_inline_fast_path_can_ignore_result_with_non_contiguous_stack_inputs(
        void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentReadonlyInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *gapValue;
    SZrTypeValue *assignedValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentReadonlyInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 5 < state->stackTail.valuePointer);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    gapValue = ZrCore_Stack_GetValue(functionBase + 3);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 4);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(gapValue);
    TEST_ASSERT_NOT_NULL(assignedValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(gapValue);
    ZrCore_Value_InitAsInt(state, assignedValue, 91);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 5;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 5;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gExpectedAssignedValuePointer = assignedValue;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_readonly_inline_two_argument_value_context_accepts_null_result);
    TEST_ASSERT_EQUAL_UINT8((TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL),
                            prototype->indexContract.setByIndexKnownNativeDirectDispatch.reserved0);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_set_by_index_known_native_stack_root_fast_path_keeps_callable_scratch_slot_null(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentStackRootDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_stack_rooted_receiver_key_and_value,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrClosureNative *nativeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrIndexContract contract;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer scratchBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *assignedValue;
    SZrTypeValue *scratchCallableValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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

    functionBase = state->stackTail.valuePointer - 7;
    scratchBase = functionBase + 3;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);
    TEST_ASSERT_TRUE(scratchBase + 4 <= state->stackTail.valuePointer);

    receiverValue = ZrCore_Stack_GetValue(functionBase);
    keyValue = ZrCore_Stack_GetValue(functionBase + 1);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 2);
    scratchCallableValue = ZrCore_Stack_GetValue(scratchBase);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(assignedValue);
    TEST_ASSERT_NOT_NULL(scratchCallableValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, assignedValue, 91);
    ZrCore_Value_InitAsInt(state, scratchCallableValue, 77);

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
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));

    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL,
                          ZrCore_Stack_GetValue(scratchBase)->type);

    ZrCore_GlobalState_Free(state->global);
}

static void test_set_by_index_known_native_inline_value_context_survives_stack_growth(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_receiver_key_and_value_then_grow_stack,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
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
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, receiverValue, keyValue, assignedValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_receiver_key_and_value_then_grow_stack);
    TEST_ASSERT_TRUE((prototype->indexContract.setByIndexKnownNativeDirectDispatch.reserved0 &
                      ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT) != 0u);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "two-arg inline direct-dispatch path should still bypass the generic PreCall helper");
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "two-arg inline direct-dispatch path should stay off helper-profile value-copy paths");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "stack-rooted inline direct-dispatch path should reuse caller stack roots without extra GC ignore pins");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_set_by_index_known_native_inline_value_context_accepts_non_stack_gc_inputs(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentInlineDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_receiver_key_and_value_then_grow_stack,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                             ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND};
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
    TZrSize ignoredObjectCountBefore;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentInlineDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
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
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;

    TEST_ASSERT_TRUE(ZrCore_Object_SetByIndexUnchecked(state, &receiverValue, &keyValue, &assignedValue));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeDirectDispatch.callback ==
                     test_bound_verify_receiver_key_and_value_then_grow_stack);
    TEST_ASSERT_TRUE((prototype->indexContract.setByIndexKnownNativeDirectDispatch.reserved0 &
                      ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT) != 0u);
    TEST_ASSERT_GREATER_THAN_UINT32((TZrUInt32)ignoredObjectCountBefore,
                                    (TZrUInt32)gObservedIgnoredObjectCount);

    ZrCore_GlobalState_Free(state->global);
}

static void test_object_call_function_with_receiver_two_arguments_fast_reuses_stack_roots_without_precall_helper(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *assignedValue;
    SZrTypeValue *resultValue;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrSize ignoredObjectCountBefore;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = test_native_verify_stack_rooted_receiver_key_and_value;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    keyString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(keyString);

    functionBase = state->stackTail.valuePointer - 4;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 3);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(assignedValue);
    TEST_ASSERT_NOT_NULL(resultValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, assignedValue, 91);
    ZrCore_Value_ResetAsNull(resultValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 4;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverTwoArgumentsFast(state,
                                                                            ZR_CAST(SZrFunction *, nativeClosure),
                                                                            receiverValue,
                                                                            keyValue,
                                                                            assignedValue,
                                                                            resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(2024, movedResultValue->value.nativeObject.nativeInt64);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "fixed-shape two-arg known-native fast call should bypass the generic PreCall helper");
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "fixed-shape two-arg known-native fast call should stay off helper-profile value-copy paths");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "fixed-shape two-arg known-native fast call should reuse stack roots without repinning shallow GC operands");

    clear_profile_counters(state);
    ZrCore_GlobalState_Free(state->global);
}

static void test_direct_binding_one_argument_inline_value_context_preserves_prefilled_result_when_callback_writes_null(
        void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *receiverObject;
    SZrString *argumentString;
    SZrTypeValue receiverValue;
    SZrTypeValue argumentValue;
    SZrTypeValue resultValue;
    SZrObjectKnownNativeDirectDispatch dispatch;

    TEST_ASSERT_NOT_NULL(state);

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);
    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);
    gExpectedPrefilledResultString = ZrCore_String_CreateFromNative(state, "prefilled_result");
    TEST_ASSERT_NOT_NULL(gExpectedPrefilledResultString);

    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(state, &resultValue, ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedPrefilledResultString));
    resultValue.type = ZR_VALUE_TYPE_STRING;

    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.callback = test_bound_verify_prefilled_result_then_set_null;
    dispatch.rawArgumentCount = 2u;
    dispatch.usesReceiver = ZR_TRUE;
    dispatch.reserved0 = (TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                                    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                                    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(
            ZrCore_Object_CallDirectBindingFastOneArgument(state, &dispatch, &receiverValue, &argumentValue, &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, resultValue.type);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_direct_binding_one_argument_readonly_inline_value_context_reuses_input_pointers(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue resultValue;
    SZrObjectKnownNativeDirectDispatch dispatch;

    TEST_ASSERT_NOT_NULL(state);

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);
    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, argumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    argumentValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(&resultValue);

    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.callback = test_bound_verify_readonly_inline_value_context_reuses_input_pointers;
    dispatch.rawArgumentCount = 2u;
    dispatch.usesReceiver = ZR_TRUE;
    dispatch.reserved0 =
            (TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = argumentValue;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(
            ZrCore_Object_CallDirectBindingFastOneArgument(state, &dispatch, receiverValue, argumentValue, &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(5150, resultValue.value.nativeObject.nativeInt64);

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_direct_binding_one_argument_readonly_inline_value_context_reuses_stack_rooted_owned_argument(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue sourceArgumentValue;
    SZrTypeValue resultValue;
    SZrObjectKnownNativeDirectDispatch dispatch;

    TEST_ASSERT_NOT_NULL(state);

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);
    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 3 < state->stackTail.valuePointer);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;

    ZrCore_Value_InitAsRawObject(state, &sourceArgumentValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    sourceArgumentValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(argumentValue);
    TEST_ASSERT_TRUE(ZrCore_Ownership_UniqueValue(state, argumentValue, &sourceArgumentValue));
    TEST_ASSERT_NOT_NULL(argumentValue->ownershipControl);

    ZrCore_Value_ResetAsNull(&resultValue);

    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.callback = test_bound_verify_readonly_inline_value_context_reuses_input_pointers;
    dispatch.rawArgumentCount = 2u;
    dispatch.usesReceiver = ZR_TRUE;
    dispatch.reserved0 =
            (TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = argumentValue;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(
            ZrCore_Object_CallDirectBindingFastOneArgument(state, &dispatch, receiverValue, argumentValue, &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(5150, resultValue.value.nativeObject.nativeInt64);

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
}

static void
test_direct_binding_two_arguments_inline_value_context_preserves_prefilled_result_when_callback_copies_argument(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *receiverObject;
    SZrString *argumentString;
    SZrTypeValue receiverValue;
    SZrTypeValue arguments[2];
    SZrTypeValue resultValue;
    SZrObjectKnownNativeDirectDispatch dispatch;

    TEST_ASSERT_NOT_NULL(state);

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);
    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);
    gExpectedPrefilledResultString = ZrCore_String_CreateFromNative(state, "prefilled_result");
    TEST_ASSERT_NOT_NULL(gExpectedPrefilledResultString);

    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &arguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    arguments[0].type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &arguments[1], 2024);
    ZrCore_Value_InitAsRawObject(state, &resultValue, ZR_CAST_RAW_OBJECT_AS_SUPER(gExpectedPrefilledResultString));
    resultValue.type = ZR_VALUE_TYPE_STRING;

    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.callback = test_bound_verify_prefilled_result_then_copy_assigned_value;
    dispatch.rawArgumentCount = 3u;
    dispatch.usesReceiver = ZR_TRUE;
    dispatch.reserved0 = (TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                                    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                                    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                                    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gExpectedAssignedInt = 2024;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(ZrCore_Object_CallDirectBindingFastTwoArguments(
            state, &dispatch, &receiverValue, &arguments[0], &arguments[1], &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(2024, resultValue.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_direct_binding_two_arguments_readonly_inline_value_context_reuses_input_pointers(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *arguments;
    SZrTypeValue resultValue;
    SZrObjectKnownNativeDirectDispatch dispatch;

    TEST_ASSERT_NOT_NULL(state);

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);
    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 4 < state->stackTail.valuePointer);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    arguments = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(arguments);
    TEST_ASSERT_NOT_NULL(arguments + 1);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, &arguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    arguments[0].type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &arguments[1], 2024);
    ZrCore_Value_ResetAsNull(&resultValue);

    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.callback = test_bound_verify_readonly_inline_two_argument_value_context_reuses_input_pointers;
    dispatch.rawArgumentCount = 3u;
    dispatch.usesReceiver = ZR_TRUE;
    dispatch.reserved0 =
            (TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gExpectedAssignedInt = 2024;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = &arguments[0];
    gExpectedAssignedValuePointer = &arguments[1];
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(
            ZrCore_Object_CallDirectBindingFastTwoArguments(state, &dispatch, receiverValue, &arguments[0], &arguments[1], &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(6160, resultValue.value.nativeObject.nativeInt64);

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    gExpectedAssignedValuePointer = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_direct_binding_two_arguments_readonly_inline_value_context_reuses_non_contiguous_owned_input_pointers(
        void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *gapValue;
    SZrTypeValue *assignedValue;
    SZrTypeValue sourceKeyValue;
    SZrTypeValue resultValue;
    SZrObjectKnownNativeDirectDispatch dispatch;

    TEST_ASSERT_NOT_NULL(state);

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);
    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 5 < state->stackTail.valuePointer);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    gapValue = ZrCore_Stack_GetValue(functionBase + 3);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 4);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(gapValue);
    TEST_ASSERT_NOT_NULL(assignedValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;

    ZrCore_Value_InitAsRawObject(state, &sourceKeyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentString));
    sourceKeyValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_ResetAsNull(keyValue);
    TEST_ASSERT_TRUE(ZrCore_Ownership_UniqueValue(state, keyValue, &sourceKeyValue));
    TEST_ASSERT_NOT_NULL(keyValue->ownershipControl);

    ZrCore_Value_ResetAsNull(gapValue);
    ZrCore_Value_InitAsInt(state, assignedValue, 2024);
    ZrCore_Value_ResetAsNull(&resultValue);

    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.callback = test_bound_verify_readonly_inline_two_argument_value_context_reuses_input_pointers;
    dispatch.rawArgumentCount = 3u;
    dispatch.usesReceiver = ZR_TRUE;
    dispatch.reserved0 =
            (TZrUInt8)(ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT);

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gExpectedAssignedInt = 2024;
    gExpectedReceiverValuePointer = receiverValue;
    gExpectedArgumentValuePointer = keyValue;
    gExpectedAssignedValuePointer = assignedValue;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_TRUE(
            ZrCore_Object_CallDirectBindingFastTwoArguments(state, &dispatch, receiverValue, keyValue, assignedValue, &resultValue));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(6160, resultValue.value.nativeObject.nativeInt64);

    gExpectedReceiverValuePointer = ZR_NULL;
    gExpectedArgumentValuePointer = ZR_NULL;
    gExpectedAssignedValuePointer = ZR_NULL;
    ZrTests_Runtime_State_Destroy(state);
}

static void test_object_call_native_binding_stack_root_two_arguments_fast_bypasses_cached_dispatcher_wrapper(void) {
    static const ZrLibParameterDescriptor kTwoArgumentParameters[] = {{"key", "string", ZR_NULL},
                                                                      {"value", "int", ZR_NULL}};
    static const ZrLibMetaMethodDescriptor kTwoArgumentStackRootDescriptor = {
            .metaType = ZR_META_SET_ITEM,
            .minArgumentCount = 2,
            .maxArgumentCount = 2,
            .callback = test_bound_verify_stack_rooted_receiver_key_and_value,
            .returnTypeName = "int",
            .documentation = ZR_NULL,
            .parameters = kTwoArgumentParameters,
            .parameterCount = ZR_ARRAY_COUNT(kTwoArgumentParameters),
            .genericParameters = ZR_NULL,
            .genericParameterCount = 0,
            .dispatchFlags = ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT};
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrProfileRuntime profileRuntime;
    SZrClosureNative *nativeClosure;
    TZrStackValuePointer functionBase;
    SZrTypeValue *receiverValue;
    SZrTypeValue *keyValue;
    SZrTypeValue *assignedValue;
    SZrTypeValue *resultValue;
    SZrObject *receiverObject;
    SZrString *keyString;
    TZrSize ignoredObjectCountBefore;
    SZrTypeValue *movedResultValue;

    TEST_ASSERT_NOT_NULL(state);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    native_binding_closure_store_cached_binding(nativeClosure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_META_METHOD,
                                                &kBindingCacheModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kTwoArgumentStackRootDescriptor);
    nativeClosure->nativeFunction = test_wrapped_cached_stack_root_dispatch_two_arguments;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    keyString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(keyString);

    functionBase = state->stackTail.valuePointer - 4;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    resultValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    keyValue = ZrCore_Stack_GetValue(functionBase + 2);
    assignedValue = ZrCore_Stack_GetValue(functionBase + 3);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(keyValue);
    TEST_ASSERT_NOT_NULL(assignedValue);
    TEST_ASSERT_NOT_NULL(resultValue);

    ZrCore_Value_InitAsRawObject(state, receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiverObject));
    receiverValue->type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_InitAsRawObject(state, keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    keyValue->type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, assignedValue, 91);
    ZrCore_Value_ResetAsNull(resultValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 4;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = keyString;
    gExpectedAssignedInt = 91;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gWrappedCachedStackRootDispatchTwoArgumentCount = 0;
    ignoredObjectCountBefore = state->global->garbageCollector->ignoredObjectCount;
    gObservedIgnoredObjectCount = ignoredObjectCountBefore;
    reset_profile_counters(state, &profileRuntime);

    TEST_ASSERT_TRUE(ZrCore_Object_CallFunctionWithReceiverTwoArgumentsFast(state,
                                                                            ZR_CAST(SZrFunction *, nativeClosure),
                                                                            receiverValue,
                                                                            keyValue,
                                                                            assignedValue,
                                                                            resultValue));

    movedResultValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedResultValue);
    TEST_ASSERT_EQUAL_UINT32(0u, gWrappedCachedStackRootDispatchTwoArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedResultValue->type));
    TEST_ASSERT_EQUAL_INT64(2024, movedResultValue->value.nativeObject.nativeInt64);
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_PRECALL],
            __LINE__,
            "native-binding stack-root two-arg object-call fast path should bypass the generic PreCall helper");
    UNITY_TEST_ASSERT_EQUAL_UINT64(
            0u,
            profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY],
            __LINE__,
            "native-binding stack-root two-arg object-call fast path should stay off helper-profile value-copy paths");
    UNITY_TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)ignoredObjectCountBefore,
            (TZrUInt32)gObservedIgnoredObjectCount,
            __LINE__,
            "native-binding stack-root two-arg object-call fast path should reuse stack roots without repinning shallow GC operands");

    clear_profile_counters(state);
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
    TEST_ASSERT_EQUAL_PTR(ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure),
                          prototype->indexContract.setByIndexKnownNativeCallable);
    TEST_ASSERT_TRUE(prototype->indexContract.setByIndexKnownNativeFunction ==
                     test_native_verify_stack_rooted_receiver_key_and_value);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 1, state->stackTop.valuePointer);

    ZrCore_GlobalState_Free(state->global);
}

static void test_precall_resolved_native_function_restores_stack_rooted_arguments_after_growth_without_callable_value(
        void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrObject *receiverObject;
    SZrString *argumentString;
    TZrStackValuePointer originalStackBase;
    TZrStackValuePointer functionBase;
    SZrTypeValue *returnValue;
    SZrTypeValue *receiverValue;
    SZrTypeValue *argumentValue;
    SZrTypeValue *movedReturnValue;

    TEST_ASSERT_NOT_NULL(state);

    receiverObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(receiverObject);
    ZrCore_Object_Init(state, receiverObject);

    argumentString = ZrCore_String_CreateFromNative(state, "member_key");
    TEST_ASSERT_NOT_NULL(argumentString);

    functionBase = state->stackTail.valuePointer - 3;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    returnValue = ZrCore_Stack_GetValue(functionBase);
    receiverValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentValue = ZrCore_Stack_GetValue(functionBase + 2);
    TEST_ASSERT_NOT_NULL(returnValue);
    TEST_ASSERT_NOT_NULL(receiverValue);
    TEST_ASSERT_NOT_NULL(argumentValue);

    ZrCore_Value_ResetAsNull(returnValue);
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
    originalStackBase = state->stackBase.valuePointer;

    gExpectedReceiverObject = receiverObject;
    gExpectedArgumentString = argumentString;
    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;

    TEST_ASSERT_NULL(ZrCore_Function_PreCallResolvedNativeFunction(state,
                                                                   functionBase,
                                                                   test_native_verify_stack_rooted_receiver_and_argument,
                                                                   1,
                                                                   ZR_NULL));

    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);

    movedReturnValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedReturnValue);
    TEST_ASSERT_TRUE(returnValue != movedReturnValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedReturnValue->type));
    TEST_ASSERT_EQUAL_INT64(4242, movedReturnValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(state->callInfoList->functionBase.valuePointer + 1, state->stackTop.valuePointer);

    ZrCore_GlobalState_Free(state->global);
}

static void test_call_prepared_resolved_native_function_closes_frame_open_upvalues(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer functionBase;
    SZrTypeValue *returnValue;
    SZrTypeValue *capturedValue;

    TEST_ASSERT_NOT_NULL(state);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 5 < state->stackTail.valuePointer);

    returnValue = ZrCore_Stack_GetValue(functionBase);
    capturedValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(returnValue);
    TEST_ASSERT_NOT_NULL(capturedValue);

    ZrCore_Value_ResetAsNull(returnValue);
    ZrCore_Value_InitAsInt(state, capturedValue, 77);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gCapturedClosureValue = ZR_NULL;
    gExpectedCapturedInt = 77;

    TEST_ASSERT_EQUAL_UINT64(1u,
                             ZrCore_Function_CallPreparedResolvedNativeFunction(state,
                                                                                functionBase,
                                                                                functionBase + 2,
                                                                                test_native_capture_open_upvalue_and_return_int,
                                                                                1,
                                                                                ZR_NULL));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_NOT_NULL(gCapturedClosureValue);
    TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(gCapturedClosureValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(ZrCore_ClosureValue_GetValue(gCapturedClosureValue)->type));
    TEST_ASSERT_EQUAL_INT64(77, ZrCore_ClosureValue_GetValue(gCapturedClosureValue)->value.nativeObject.nativeInt64);
    TEST_ASSERT_NULL(state->stackClosureValueList);
    TEST_ASSERT_EQUAL_PTR(functionBase + 1, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_call_prepared_resolved_native_function_closes_frame_to_be_closed_values(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *closeClosure;
    SZrString *prototypeName;
    SZrObjectPrototype *prototype;
    SZrObject *resourceObject;
    TZrStackValuePointer functionBase;
    SZrTypeValue *returnValue;
    SZrTypeValue *resourceValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(state,
                                         (TZrSize)(state->stackTail.valuePointer - state->stackBase.valuePointer) * 2u,
                                         ZR_TRUE));

    closeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(closeClosure);
    closeClosure->nativeFunction = test_native_observe_close_meta_invocation;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closeClosure));

    prototypeName = ZrCore_String_CreateFromNative(state, "KnownNativeCloseMetaObject");
    TEST_ASSERT_NOT_NULL(prototypeName);
    prototype = ZrCore_ObjectPrototype_New(state, prototypeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    ZrCore_ObjectPrototype_AddMeta(state,
                                   prototype,
                                   ZR_META_CLOSE,
                                   ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closeClosure)));

    resourceObject = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(resourceObject);
    ZrCore_Object_Init(state, resourceObject);

    functionBase = state->stackTail.valuePointer - 2;
    TEST_ASSERT_TRUE(functionBase >= state->stackBase.valuePointer);

    returnValue = ZrCore_Stack_GetValue(functionBase);
    resourceValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(returnValue);
    TEST_ASSERT_NOT_NULL(resourceValue);

    ZrCore_Value_ResetAsNull(returnValue);
    ZrCore_Value_InitAsRawObject(state, resourceValue, ZR_CAST_RAW_OBJECT_AS_SUPER(resourceObject));
    resourceValue->type = ZR_VALUE_TYPE_OBJECT;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    gNativeCallCount = 0;
    gObservedCorruption = ZR_FALSE;
    gCloseMetaInvocationCount = 0;
    gExpectedCloseMetaObject = resourceObject;

    TEST_ASSERT_EQUAL_UINT64(1u,
                             ZrCore_Function_CallPreparedResolvedNativeFunction(state,
                                                                                functionBase,
                                                                                functionBase + 2,
                                                                                test_native_register_to_be_closed_argument_and_return_int,
                                                                                1,
                                                                                ZR_NULL));

    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(1u, gCloseMetaInvocationCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(returnValue->type));
    TEST_ASSERT_EQUAL_INT64(3030, returnValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer, state->toBeClosedValueList.valuePointer);
    TEST_ASSERT_EQUAL_PTR(functionBase + 1, state->stackTop.valuePointer);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_call_prepared_resolved_native_function_single_result_fast_reuses_nested_call_info_node(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    TZrStackValuePointer functionBase;
    SZrTypeValue *returnValue;
    SZrTypeValue *movedReturnValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_UINT32(0u, state->callInfoListLength);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 2 < state->stackTail.valuePointer);

    returnValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(returnValue);
    ZrCore_Value_ResetAsNull(returnValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 1;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 1;

    gObservedCorruption = ZR_FALSE;
    gNestedGenericOuterNativeCallCount = 0;
    gNestedGenericInnerNativeCallCount = 0;

    TEST_ASSERT_EQUAL_UINT64(1u,
                             ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast(
                                     state,
                                     functionBase,
                                     functionBase + 1,
                                     test_native_issue_nested_generic_prepared_call_and_return_marker,
                                     ZR_NULL));

    movedReturnValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedReturnValue);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericOuterNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericInnerNativeCallCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedReturnValue->type));
    TEST_ASSERT_EQUAL_INT64(6060, movedReturnValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            1u,
            state->callInfoListLength,
            "single-result prepared native fast helper should stay on a stack-local call-info and only extend once for the nested generic call");
    TEST_ASSERT_NOT_NULL(state->baseCallInfo.next);

    functionBase = state->callInfoList->functionBase.valuePointer;
    returnValue = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(returnValue);
    ZrCore_Value_ResetAsNull(returnValue);

    gObservedCorruption = ZR_FALSE;
    gNestedGenericOuterNativeCallCount = 0;
    gNestedGenericInnerNativeCallCount = 0;

    TEST_ASSERT_EQUAL_UINT64(1u,
                             ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast(
                                     state,
                                     functionBase,
                                     functionBase + 1,
                                     test_native_issue_nested_generic_prepared_call_and_return_marker,
                                     ZR_NULL));

    movedReturnValue = ZrCore_Stack_GetValue(state->callInfoList->functionBase.valuePointer);
    TEST_ASSERT_NOT_NULL(movedReturnValue);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericOuterNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericInnerNativeCallCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(movedReturnValue->type));
    TEST_ASSERT_EQUAL_INT64(6060, movedReturnValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            1u,
            state->callInfoListLength,
            "single-result prepared native fast helper should relink and reuse the nested generic call-info node on later calls");

    ZrCore_GlobalState_Free(state->global);
}

static void test_call_prepared_resolved_native_function_single_result_fast_supports_stack_return_destination(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    TZrStackValuePointer functionBase;
    TZrStackValuePointer returnDestination;
    SZrTypeValue *returnValue;
    SZrTypeValue *destinationValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_UINT32(0u, state->callInfoListLength);

    functionBase = state->stackBase.valuePointer + 8;
    returnDestination = functionBase + 3;
    TEST_ASSERT_TRUE(returnDestination < state->stackTail.valuePointer);

    returnValue = ZrCore_Stack_GetValue(functionBase);
    destinationValue = ZrCore_Stack_GetValue(returnDestination);
    TEST_ASSERT_NOT_NULL(returnValue);
    TEST_ASSERT_NOT_NULL(destinationValue);
    ZrCore_Value_ResetAsNull(returnValue);
    ZrCore_Value_ResetAsNull(destinationValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 1;

    gObservedCorruption = ZR_FALSE;
    gNestedGenericOuterNativeCallCount = 0;
    gNestedGenericInnerNativeCallCount = 0;

    TEST_ASSERT_EQUAL_UINT64(1u,
                             ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFast(
                                     state,
                                     functionBase,
                                     functionBase + 1,
                                     test_native_issue_nested_generic_prepared_call_and_return_marker,
                                     returnDestination));

    functionBase = state->callInfoList->functionBase.valuePointer;
    returnDestination = functionBase + 3;
    destinationValue = ZrCore_Stack_GetValue(returnDestination);
    TEST_ASSERT_NOT_NULL(destinationValue);
    TEST_ASSERT_FALSE(gObservedCorruption);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericOuterNativeCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNestedGenericInnerNativeCallCount);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(destinationValue->type));
    TEST_ASSERT_EQUAL_INT64(6060, destinationValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
    TEST_ASSERT_EQUAL_PTR(returnDestination + 1, state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            1u,
            state->callInfoListLength,
            "single-result prepared native fast helper should still reuse only the nested generic call-info node when a stack return destination is provided");

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

static void test_native_binding_detects_detached_gc_owned_values(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *object;
    SZrTypeValue sourceValue;
    SZrTypeValue ownedValue;

    TEST_ASSERT_NOT_NULL(state);

    object = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_Object_Init(state, object);

    ZrCore_Value_InitAsRawObject(state, &sourceValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    sourceValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&ownedValue);

    TEST_ASSERT_TRUE(ZrCore_Ownership_UniqueValue(state, &ownedValue, &sourceValue));
    TEST_ASSERT_NOT_NULL(ownedValue.ownershipControl);
    TEST_ASSERT_TRUE(ownedValue.ownershipControl->isDetachedFromGc);
    TEST_ASSERT_EQUAL_PTR(ownedValue.value.object, ownedValue.ownershipControl->object);

    TEST_ASSERT_FALSE(native_binding_value_has_detached_gc_ownership_inline(&sourceValue));
    TEST_ASSERT_TRUE(native_binding_value_has_detached_gc_ownership_inline(&ownedValue));

    ZrCore_Ownership_ReleaseValue(state, &ownedValue);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_binding_stack_root_callback_lane_syncs_rebound_self_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrStackValuePointer functionBase;
    SZrTypeValue *closureValue;
    SZrTypeValue *selfValue;
    SZrFunctionStackAnchor functionBaseAnchor;
    ZrLibCallContext context = {0};
    SZrTypeValue resultValue;
    TZrBool success;

    TEST_ASSERT_NOT_NULL(state);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 1 < state->stackTail.valuePointer);

    closureValue = ZrCore_Stack_GetValue(functionBase);
    selfValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(closureValue);
    TEST_ASSERT_NOT_NULL(selfValue);

    ZrCore_Value_ResetAsNull(closureValue);
    ZrCore_Value_InitAsInt(state, selfValue, 42);
    ZrCore_Value_ResetAsNull(&resultValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    context.state = state;
    context.functionBase = functionBase;
    native_binding_init_call_context_layout_cached(&context, functionBase, 1u, ZR_TRUE);
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);

    gExpectedReboundSelfInt = 909;
    success = native_binding_dispatch_stack_root_callback_lane(state,
                                                               test_native_binding_rebinds_self_to_local_int,
                                                               &context,
                                                               &functionBaseAnchor,
                                                               &resultValue);

    TEST_ASSERT_TRUE(success);

    functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
    TEST_ASSERT_NOT_NULL(functionBase);
    selfValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(selfValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(selfValue->type));
    TEST_ASSERT_EQUAL_INT64(gExpectedReboundSelfInt, selfValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(1, resultValue.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_binding_stack_root_callback_lane_syncs_rebound_self_value_across_stack_growth(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    TZrStackValuePointer functionBase;
    TZrStackValuePointer originalStackBase;
    SZrTypeValue *closureValue;
    SZrTypeValue *selfValue;
    SZrFunctionStackAnchor functionBaseAnchor;
    ZrLibCallContext context = {0};
    SZrTypeValue resultValue;
    TZrBool success;

    TEST_ASSERT_NOT_NULL(state);

    functionBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(functionBase + 1 < state->stackTail.valuePointer);

    closureValue = ZrCore_Stack_GetValue(functionBase);
    selfValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(closureValue);
    TEST_ASSERT_NOT_NULL(selfValue);

    ZrCore_Value_ResetAsNull(closureValue);
    ZrCore_Value_InitAsInt(state, selfValue, 42);
    ZrCore_Value_ResetAsNull(&resultValue);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 2;

    context.state = state;
    context.functionBase = functionBase;
    native_binding_init_call_context_layout_cached(&context, functionBase, 1u, ZR_TRUE);
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);

    gExpectedReboundSelfInt = 909;
    originalStackBase = state->stackBase.valuePointer;
    success = native_binding_dispatch_stack_root_callback_lane(state,
                                                               test_native_binding_grows_stack_and_rebinds_self_to_local_int,
                                                               &context,
                                                               &functionBaseAnchor,
                                                               &resultValue);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    TEST_ASSERT_TRUE(originalStackBase != state->stackBase.valuePointer);

    functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
    TEST_ASSERT_NOT_NULL(functionBase);
    selfValue = ZrCore_Stack_GetValue(functionBase + 1);
    TEST_ASSERT_NOT_NULL(selfValue);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(selfValue->type));
    TEST_ASSERT_EQUAL_INT64(gExpectedReboundSelfInt, selfValue->value.nativeObject.nativeInt64);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(resultValue.type));
    TEST_ASSERT_EQUAL_INT64(1, resultValue.value.nativeObject.nativeInt64);

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
    ZrLibBindingEntry cachedEntry = {0};
    TZrSize baseIndex;
    TZrSize cacheSlot;

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
    TEST_ASSERT_EQUAL_UINT64(baseIndex, closureA->nativeBindingLookupIndex);
    TEST_ASSERT_EQUAL_UINT64(baseIndex + 1u, closureB->nativeBindingLookupIndex);
    TEST_ASSERT_FALSE(closureA->nativeBindingUsesReceiver);
    TEST_ASSERT_TRUE(native_binding_closure_try_build_cached_entry(closureA, &cachedEntry));
    TEST_ASSERT_EQUAL_PTR(closureA, cachedEntry.closure);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheModule, cachedEntry.moduleDescriptor);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheFunctions[0], cachedEntry.descriptor.functionDescriptor);

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

    for (cacheSlot = 0; cacheSlot < ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_CAPACITY; cacheSlot++) {
        registry->bindingLookupHotIndices[cacheSlot] = ZR_LIBRARY_NATIVE_BINDING_LOOKUP_CACHE_INVALID_INDEX;
    }

    entryB = native_registry_find_binding(registry, closureB);
    TEST_ASSERT_NOT_NULL(entryB);
    TEST_ASSERT_EQUAL_PTR(&kBindingCacheFunctions[1], entryB->descriptor.functionDescriptor);
    TEST_ASSERT_EQUAL_UINT64(baseIndex + 1u, registry->bindingLookupHotIndices[0]);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_object_call_known_native_fast_path_restores_stack_rooted_inputs_after_growth);
    RUN_TEST(test_object_call_known_native_fast_path_restores_outer_frame_bounds_after_growth);
    RUN_TEST(test_object_call_known_native_fast_path_preserves_receiver_when_result_aliases_receiver_slot);
    RUN_TEST(test_object_call_known_native_fast_path_overwrites_prefilled_future_scratch_slots);
    RUN_TEST(test_object_call_known_native_fast_path_accepts_non_stack_gc_inputs);
    RUN_TEST(test_object_call_known_native_fast_path_reuses_stack_roots_without_repinning_shallow_gc_values);
    RUN_TEST(test_object_call_function_with_receiver_one_argument_fast_reuses_stack_roots_without_precall_helper);
    RUN_TEST(test_object_call_native_binding_stack_root_one_argument_fast_bypasses_cached_dispatcher_wrapper);
    RUN_TEST(test_object_call_native_binding_stack_root_one_argument_fast_syncs_rebound_self_across_stack_growth);
    RUN_TEST(test_object_call_native_binding_stack_root_one_argument_fast_preserves_aliased_receiver_across_stack_growth);
    RUN_TEST(test_object_call_known_native_fast_path_reuses_single_nested_call_info_node);
    RUN_TEST(test_library_call_value_known_native_path_bypasses_generic_precall_dispatcher);
    RUN_TEST(test_get_by_index_known_native_fast_path_accepts_non_stack_gc_inputs);
    RUN_TEST(test_get_by_index_known_native_stack_root_fast_path_caches_direct_dispatch);
    RUN_TEST(test_get_by_index_known_native_cached_direct_dispatch_survives_resolver_data_loss);
    RUN_TEST(test_get_by_index_known_native_stack_root_fast_path_keeps_callable_scratch_slot_null);
    RUN_TEST(test_get_by_index_known_native_readonly_inline_fast_path_reuses_input_pointers);
    RUN_TEST(test_get_by_index_known_native_stack_operands_readonly_inline_fast_path_reuses_input_pointers);
    RUN_TEST(test_get_by_index_known_native_stack_operands_readonly_inline_fast_path_records_helper_from_state_without_tls_current);
    RUN_TEST(test_get_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback);
    RUN_TEST(test_get_by_index_known_native_readonly_inline_fast_path_keeps_cached_fast_callback_without_callback_pointer);
    RUN_TEST(test_set_by_index_known_native_fast_path_accepts_two_stack_rooted_arguments);
    RUN_TEST(test_object_call_function_with_receiver_two_arguments_fast_reuses_stack_roots_without_precall_helper);
    RUN_TEST(test_object_call_native_binding_stack_root_two_arguments_fast_bypasses_cached_dispatcher_wrapper);
    RUN_TEST(test_set_by_index_known_native_stack_root_fast_path_caches_direct_dispatch);
    RUN_TEST(test_set_by_index_known_native_cached_direct_dispatch_survives_resolver_data_loss);
    RUN_TEST(test_set_by_index_known_native_stack_root_fast_path_keeps_callable_scratch_slot_null);
    RUN_TEST(test_set_by_index_known_native_inline_value_context_survives_stack_growth);
    RUN_TEST(test_set_by_index_known_native_inline_value_context_accepts_non_stack_gc_inputs);
    RUN_TEST(test_direct_binding_one_argument_inline_value_context_preserves_prefilled_result_when_callback_writes_null);
    RUN_TEST(test_direct_binding_one_argument_readonly_inline_value_context_reuses_input_pointers);
    RUN_TEST(test_direct_binding_one_argument_readonly_inline_value_context_reuses_stack_rooted_owned_argument);
    RUN_TEST(test_direct_binding_two_arguments_inline_value_context_preserves_prefilled_result_when_callback_copies_argument);
    RUN_TEST(test_direct_binding_two_arguments_readonly_inline_value_context_reuses_input_pointers);
    RUN_TEST(test_direct_binding_two_arguments_readonly_inline_value_context_reuses_non_contiguous_owned_input_pointers);
    RUN_TEST(test_set_by_index_known_native_readonly_inline_fast_path_can_ignore_result);
    RUN_TEST(test_set_by_index_known_native_stack_operands_readonly_inline_fast_path_can_ignore_result);
    RUN_TEST(test_set_by_index_known_native_stack_operands_readonly_inline_fast_path_records_helper_from_state_without_tls_current);
    RUN_TEST(test_set_by_index_known_native_readonly_inline_fast_path_prefers_meta_method_fast_callback);
    RUN_TEST(test_set_by_index_known_native_readonly_inline_fast_path_keeps_cached_fast_callback_without_callback_pointer);
    RUN_TEST(test_set_by_index_known_native_readonly_inline_fast_path_can_ignore_result_with_non_contiguous_stack_inputs);
    RUN_TEST(test_set_by_index_known_native_fast_path_accepts_non_stack_gc_inputs);
    RUN_TEST(test_precall_resolved_native_function_restores_stack_rooted_arguments_after_growth_without_callable_value);
    RUN_TEST(test_call_prepared_resolved_native_function_closes_frame_open_upvalues);
    RUN_TEST(test_call_prepared_resolved_native_function_closes_frame_to_be_closed_values);
    RUN_TEST(test_call_prepared_resolved_native_function_single_result_fast_reuses_nested_call_info_node);
    RUN_TEST(test_call_prepared_resolved_native_function_single_result_fast_supports_stack_return_destination);
    RUN_TEST(test_native_binding_prepare_stable_value_reuses_plain_heap_object_without_release);
    RUN_TEST(test_native_binding_prepare_stable_value_clones_struct_and_marks_release);
    RUN_TEST(test_native_binding_detects_detached_gc_owned_values);
    RUN_TEST(test_native_binding_stack_root_callback_lane_syncs_rebound_self_value);
    RUN_TEST(test_native_binding_stack_root_callback_lane_syncs_rebound_self_value_across_stack_growth);
    RUN_TEST(test_native_binding_cached_stack_root_dispatchers_match_fixed_arity_bindings);
    RUN_TEST(test_native_binding_cached_binding_primes_direct_dispatch_cache_and_clears_on_rebind);
    RUN_TEST(test_native_binding_init_cached_stack_root_context_overwrites_dirty_layout_state);
    RUN_TEST(test_native_binding_init_cached_stack_root_context_from_closure_overwrites_dirty_layout_state);
    RUN_TEST(test_native_registry_find_binding_promotes_hot_closures_into_two_slot_cache);

    return UNITY_END();
}
