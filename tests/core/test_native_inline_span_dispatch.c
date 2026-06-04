#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library.h"

#include "native_binding/native_binding_dispatch_lanes.h"
#include "native_binding/native_binding_internal.h"

typedef struct TestPoisoningAllocatorContext {
    TZrUInt32 moveCount;
} TestPoisoningAllocatorContext;

static TZrUInt32 gNativeCallCount = 0u;
static TZrBool gObservedInlineSpanResult = ZR_FALSE;
static ZrLibInlineSpan gObservedInlineSpan;

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

static TZrBool test_native_grow_stack_then_observe_inline_argument_span_callback(ZrLibCallContext *context,
                                                                                 SZrTypeValue *result) {
    TZrSize grownSize;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(context->state);
    TEST_ASSERT_NOT_NULL(result);

    grownSize = (TZrSize)(context->state->stackTail.valuePointer - context->state->stackBase.valuePointer) + 32u;
    TEST_ASSERT_TRUE(ZrCore_Stack_GrowTo(context->state, grownSize, ZR_TRUE));

    memset(&gObservedInlineSpan, 0, sizeof(gObservedInlineSpan));
    gObservedInlineSpanResult = ZrLib_CallContext_InlineArgumentSpan(context, 0u, &gObservedInlineSpan);
    gNativeCallCount++;
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

static const ZrLibParameterDescriptor kInlineSpanParameters[] = {
        {"value", "InlineProbe", ZR_NULL},
};

static const ZrLibFunctionDescriptor kInlineSpanFastLaneFunctionDescriptor = {
        .name = "observeInlineAfterFastLaneGrow",
        .minArgumentCount = 1,
        .maxArgumentCount = 1,
        .callback = test_native_grow_stack_then_observe_inline_argument_span_callback,
        .returnTypeName = "null",
        .documentation = ZR_NULL,
        .parameters = kInlineSpanParameters,
        .parameterCount = ZR_ARRAY_COUNT(kInlineSpanParameters),
        .genericParameters = ZR_NULL,
        .genericParameterCount = 0u,
        .contractRole = 0u,
        .dispatchFlags = 0u,
};

static const ZrLibFunctionDescriptor kInlineSpanGenericDispatcherFunctionDescriptor = {
        .name = "observeInlineAfterGenericDispatcherGrow",
        .minArgumentCount = ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY + 1u,
        .maxArgumentCount = ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY + 1u,
        .callback = test_native_grow_stack_then_observe_inline_argument_span_callback,
        .returnTypeName = "null",
        .documentation = ZR_NULL,
        .parameters = kInlineSpanParameters,
        .parameterCount = ZR_ARRAY_COUNT(kInlineSpanParameters),
        .genericParameters = ZR_NULL,
        .genericParameterCount = 0u,
        .contractRole = 0u,
        .dispatchFlags = 0u,
};

static const ZrLibModuleDescriptor kInlineSpanModule = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "probe.native_inline_span_dispatch",
        .constants = ZR_NULL,
        .constantCount = 0u,
        .functions = ZR_NULL,
        .functionCount = 0u,
        .types = ZR_NULL,
        .typeCount = 0u,
        .typeHints = ZR_NULL,
        .typeHintCount = 0u,
        .typeHintsJson = ZR_NULL,
        .documentation = ZR_NULL,
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0u,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0u,
        .onMaterialize = ZR_NULL,
};

static void test_reset_observed_span(void) {
    gNativeCallCount = 0u;
    gObservedInlineSpanResult = ZR_FALSE;
    memset(&gObservedInlineSpan, 0, sizeof(gObservedInlineSpan));
}

static void test_prepare_inline_native_frame(SZrState *state,
                                             SZrClosureNative *closure,
                                             SZrFunction *function,
                                             TZrSize rawArgumentCount,
                                             const TZrByte *payload,
                                             TZrSize payloadSize,
                                             TZrUInt32 typeLayoutId,
                                             TZrStackValuePointer *outFunctionBase,
                                             SZrStackFramePlace *outOriginalPlace) {
    SZrFunctionFrameSlotLayout *layouts;
    TZrStackValuePointer functionBase;
    TZrSize frameStorageSlotCount;
    TZrStackValuePointer slot;

    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionFrameSlotLayout),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(layouts);
    memset(layouts, 0, sizeof(*layouts));
    layouts[0].stackSlot = 0u;
    layouts[0].byteOffset = (TZrUInt32)(rawArgumentCount * sizeof(SZrTypeValueOnStack));
    layouts[0].byteSize = (TZrUInt32)payloadSize;
    layouts[0].byteAlign = 4u;
    layouts[0].typeLayoutId = typeLayoutId;
    layouts[0].slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layouts[0].isParameter = ZR_TRUE;

    function->frameSlotLayouts = layouts;
    function->frameSlotLayoutLength = 1u;
    function->frameByteSize = layouts[0].byteOffset + layouts[0].byteSize;
    function->frameByteAlign = 4u;

    functionBase = state->stackBase.valuePointer + 8;
    frameStorageSlotCount = ZrCore_Function_GetFrameStorageSlotCount(function);
    TEST_ASSERT_TRUE(functionBase + frameStorageSlotCount + 1u < state->stackTail.valuePointer);

    for (slot = functionBase; slot < functionBase + frameStorageSlotCount + 1u; slot++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
    }

    ZrCore_Stack_SetRawObjectValue(state, functionBase, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, function, functionBase + 1, 0u, outOriginalPlace));
    memcpy(outOriginalPlace->address, payload, payloadSize);

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + frameStorageSlotCount + 1u;
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + 1u + rawArgumentCount;

    *outFunctionBase = functionBase;
}

static void test_assert_observed_relocated_inline_span(SZrState *state,
                                                       const SZrFunction *function,
                                                       const SZrFunctionFrameSlotLayout *layout,
                                                       const TZrByte *payload,
                                                       TZrSize payloadSize,
                                                       const SZrFunctionStackAnchor *functionBaseAnchor,
                                                       const SZrStackFramePlace *originalPlace) {
    ZrLibInlineSpan span = gObservedInlineSpan;
    TZrStackValuePointer relocatedFunctionBase;
    SZrStackFramePlace relocatedPlace;

    TEST_ASSERT_TRUE(gObservedInlineSpanResult);
    TEST_ASSERT_TRUE(span.available);
    TEST_ASSERT_EQUAL_UINT32(layout->byteSize, span.byteSize);
    TEST_ASSERT_EQUAL_UINT32(layout->byteAlign, span.byteAlign);
    TEST_ASSERT_EQUAL_UINT32(layout->typeLayoutId, span.typeLayoutId);

    relocatedFunctionBase = ZrCore_Function_StackAnchorRestore(state, functionBaseAnchor);
    TEST_ASSERT_NOT_NULL(relocatedFunctionBase);
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, function, relocatedFunctionBase + 1, 0u, &relocatedPlace));
    TEST_ASSERT_TRUE(originalPlace->address != relocatedPlace.address);
    TEST_ASSERT_EQUAL_PTR(relocatedPlace.address, span.address);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, span.address, payloadSize);
}

static void test_native_fast_lane_inline_argument_span_refreshes_after_stack_growth(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrFunction *function;
    SZrClosureNative *closure;
    ZrLibBindingEntry entry = {0};
    ZrLibCallContext context = {0};
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue result;
    SZrStackFramePlace originalPlace;
    TZrStackValuePointer functionBase;
    TZrBool success;
    TZrByte payload[12] = {
            0x10u, 0x21u, 0x32u, 0x43u, 0x54u, 0x65u,
            0x76u, 0x87u, 0x98u, 0xa9u, 0xbau, 0xcbu};

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    closure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(closure);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->aotShimFunction = function;
    native_binding_closure_store_cached_binding(closure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                                &kInlineSpanModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kInlineSpanFastLaneFunctionDescriptor);

    test_prepare_inline_native_frame(state,
                                     closure,
                                     function,
                                     1u,
                                     payload,
                                     sizeof(payload),
                                     73u,
                                     &functionBase,
                                     &originalPlace);

    native_binding_init_cached_stack_root_context_from_closure(&context, state, closure, functionBase, 1u, ZR_FALSE);
    TEST_ASSERT_EQUAL_PTR(function, context.inlineFrameFunction);
    TEST_ASSERT_EQUAL_PTR(functionBase + 1, context.inlineFrameBase);

    entry.closure = closure;
    entry.bindingKind = ZR_LIB_RESOLVED_BINDING_FUNCTION;
    entry.moduleDescriptor = &kInlineSpanModule;
    entry.descriptor.functionDescriptor = &kInlineSpanFastLaneFunctionDescriptor;

    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    ZrLib_Value_SetNull(&result);
    test_reset_observed_span();

    success = native_binding_dispatch_fast_lane(state, &entry, &context, &functionBaseAnchor, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    test_assert_observed_relocated_inline_span(state,
                                               function,
                                               &function->frameSlotLayouts[0],
                                               payload,
                                               sizeof(payload),
                                               &functionBaseAnchor,
                                               &originalPlace);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_generic_dispatcher_inline_argument_span_refreshes_after_stack_growth(void) {
    TestPoisoningAllocatorContext allocatorContext = {0};
    SZrState *state = test_create_state_with_poisoning_allocator(&allocatorContext);
    SZrFunction *function;
    SZrClosureNative *closure;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrStackFramePlace originalPlace;
    TZrStackValuePointer functionBase;
    TZrInt64 resultCount;
    TZrByte payload[12] = {
            0xd0u, 0xc1u, 0xb2u, 0xa3u, 0x94u, 0x85u,
            0x76u, 0x67u, 0x58u, 0x49u, 0x3au, 0x2bu};

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    closure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(closure);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closure->aotShimFunction = function;
    native_binding_closure_store_cached_binding(closure,
                                                0u,
                                                ZR_LIB_RESOLVED_BINDING_FUNCTION,
                                                &kInlineSpanModule,
                                                ZR_NULL,
                                                ZR_NULL,
                                                &kInlineSpanGenericDispatcherFunctionDescriptor);

    test_prepare_inline_native_frame(state,
                                     closure,
                                     function,
                                     ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY + 1u,
                                     payload,
                                     sizeof(payload),
                                     91u,
                                     &functionBase,
                                     &originalPlace);
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    test_reset_observed_span();

    resultCount = native_binding_dispatcher(state);

    TEST_ASSERT_EQUAL_INT64(1, resultCount);
    TEST_ASSERT_EQUAL_UINT32(1u, gNativeCallCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, allocatorContext.moveCount);
    test_assert_observed_relocated_inline_span(state,
                                               function,
                                               &function->frameSlotLayouts[0],
                                               payload,
                                               sizeof(payload),
                                               &functionBaseAnchor,
                                               &originalPlace);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_inline_argument_span_rejects_inline_non_parameter_slot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    ZrLibCallContext context = {0};
    ZrLibInlineSpan span;
    SZrStackFramePlace place;
    TZrStackValuePointer frameBase;
    TZrByte payload[8] = {0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xabu, 0xcdu, 0xefu};

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    layout.stackSlot = 0u;
    layout.byteOffset = 0u;
    layout.byteSize = sizeof(payload);
    layout.byteAlign = 4u;
    layout.typeLayoutId = 109u;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layout.isParameter = ZR_FALSE;

    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = layout.byteSize;
    function.frameByteAlign = layout.byteAlign;

    frameBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, &function, frameBase, 0u, &place));
    memcpy(place.address, payload, sizeof(payload));

    context.state = state;
    context.argumentCount = 1u;
    context.inlineFrameFunction = &function;
    context.inlineFrameBase = frameBase;
    context.inlineArgumentStartSlot = 0u;

    memset(&span, 0xcc, sizeof(span));
    TEST_ASSERT_FALSE(ZrLib_CallContext_InlineArgumentSpan(&context, 0u, &span));
    TEST_ASSERT_FALSE(span.available);
    TEST_ASSERT_NULL(span.address);
    TEST_ASSERT_EQUAL_UINT32(0u, span.byteSize);
    TEST_ASSERT_EQUAL_UINT32(0u, span.byteAlign);
    TEST_ASSERT_EQUAL_UINT32(0u, span.typeLayoutId);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, place.address, sizeof(payload));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_inline_parameter_requires_span_not_plain_argument_value(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    ZrLibCallContext context = {0};
    ZrLibInlineSpan span;
    SZrStackFramePlace place;
    SZrTypeValue stableArgumentCopy;
    TZrStackValuePointer frameBase;
    TZrByte payload[8] = {0xfeu, 0xdcu, 0xbau, 0x98u, 0x76u, 0x54u, 0x32u, 0x10u};

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    layout.stackSlot = 0u;
    layout.byteOffset = 0u;
    layout.byteSize = sizeof(payload);
    layout.byteAlign = 4u;
    layout.typeLayoutId = 211u;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    layout.isParameter = ZR_TRUE;

    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = layout.byteSize;
    function.frameByteAlign = layout.byteAlign;

    frameBase = state->stackBase.valuePointer + 8;
    TEST_ASSERT_TRUE(ZrCore_Function_MakeFrameSlotPlace(state, &function, frameBase, 0u, &place));
    memcpy(place.address, payload, sizeof(payload));

    ZrLib_Value_SetInt(state, &stableArgumentCopy, 42);

    context.state = state;
    context.argumentCount = 1u;
    context.argumentValues = &stableArgumentCopy;
    context.inlineFrameFunction = &function;
    context.inlineFrameBase = frameBase;
    context.inlineArgumentStartSlot = 0u;

    memset(&span, 0, sizeof(span));
    TEST_ASSERT_TRUE(ZrLib_CallContext_InlineArgumentSpan(&context, 0u, &span));
    TEST_ASSERT_TRUE(span.available);
    TEST_ASSERT_EQUAL_PTR(place.address, span.address);
    TEST_ASSERT_EQUAL_UINT32(layout.byteSize, span.byteSize);
    TEST_ASSERT_NULL(ZrLib_CallContext_Argument(&context, 0u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, place.address, sizeof(payload));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_boxed_argument_stays_available_when_no_inline_frame_layout(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrLibCallContext context = {0};
    ZrLibInlineSpan span;
    SZrTypeValue stableArgumentCopy;
    SZrTypeValue *argument;
    TZrInt64 readValue = 0;

    TEST_ASSERT_NOT_NULL(state);

    ZrLib_Value_SetInt(state, &stableArgumentCopy, 64);

    context.state = state;
    context.argumentCount = 1u;
    context.argumentValues = &stableArgumentCopy;

    memset(&span, 0xcc, sizeof(span));
    TEST_ASSERT_FALSE(ZrLib_CallContext_InlineArgumentSpan(&context, 0u, &span));
    TEST_ASSERT_FALSE(span.available);
    TEST_ASSERT_NULL(span.address);
    TEST_ASSERT_EQUAL_UINT32(0u, span.byteSize);
    argument = ZrLib_CallContext_Argument(&context, 0u);
    TEST_ASSERT_EQUAL_PTR(&stableArgumentCopy, argument);
    TEST_ASSERT_TRUE(ZrLib_CallContext_ReadInt(&context, 0u, &readValue));
    TEST_ASSERT_EQUAL_INT64(64, readValue);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_native_plain_value_frame_slot_stays_boxed_argument(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction function = {0};
    SZrFunctionFrameSlotLayout layout = {0};
    ZrLibCallContext context = {0};
    ZrLibInlineSpan span;
    SZrTypeValue stableArgumentCopy;
    SZrTypeValue *argument;
    TZrStackValuePointer frameBase;
    TZrInt64 readValue = 0;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->stackBase.valuePointer);

    layout.stackSlot = 0u;
    layout.byteOffset = 0u;
    layout.byteSize = sizeof(SZrTypeValue);
    layout.byteAlign = 8u;
    layout.typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout.slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout.isParameter = ZR_TRUE;

    function.frameSlotLayouts = &layout;
    function.frameSlotLayoutLength = 1u;
    function.frameByteSize = layout.byteSize;
    function.frameByteAlign = layout.byteAlign;

    frameBase = state->stackBase.valuePointer + 8;
    ZrLib_Value_SetInt(state, &stableArgumentCopy, 77);

    context.state = state;
    context.argumentCount = 1u;
    context.argumentValues = &stableArgumentCopy;
    context.inlineFrameFunction = &function;
    context.inlineFrameBase = frameBase;
    context.inlineArgumentStartSlot = 0u;

    memset(&span, 0xcc, sizeof(span));
    TEST_ASSERT_FALSE(ZrLib_CallContext_InlineArgumentSpan(&context, 0u, &span));
    TEST_ASSERT_FALSE(span.available);
    TEST_ASSERT_NULL(span.address);
    TEST_ASSERT_EQUAL_UINT32(0u, span.typeLayoutId);
    argument = ZrLib_CallContext_Argument(&context, 0u);
    TEST_ASSERT_EQUAL_PTR(&stableArgumentCopy, argument);
    TEST_ASSERT_TRUE(ZrLib_CallContext_ReadInt(&context, 0u, &readValue));
    TEST_ASSERT_EQUAL_INT64(77, readValue);

    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_native_fast_lane_inline_argument_span_refreshes_after_stack_growth);
    RUN_TEST(test_native_generic_dispatcher_inline_argument_span_refreshes_after_stack_growth);
    RUN_TEST(test_native_inline_argument_span_rejects_inline_non_parameter_slot);
    RUN_TEST(test_native_inline_parameter_requires_span_not_plain_argument_value);
    RUN_TEST(test_native_boxed_argument_stays_available_when_no_inline_frame_layout);
    RUN_TEST(test_native_plain_value_frame_slot_stays_boxed_argument);

    return UNITY_END();
}
