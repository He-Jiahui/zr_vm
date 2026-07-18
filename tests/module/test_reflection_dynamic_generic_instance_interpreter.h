#ifndef ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_INSTANCE_INTERPRETER_H
#define ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_INSTANCE_INTERPRETER_H

static void test_interpreter_generic_instance_materializes_reference_object_context(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_BOOL,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrReflectionDynamicGenericTypeInstance aotInstance;
    SZrString *openName;
    SZrString *markerName;
    SZrObjectPrototype *openPrototype;
    SZrObject *object;
    SZrObject *typeObject;
    SZrTypeValue markerKey;
    SZrTypeValue markerValue;
    const SZrTypeValue *inheritedValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    openName = ZrCore_String_CreateFromNative(state, "Generic");
    markerName = ZrCore_String_CreateFromNative(state, "openMarker");
    TEST_ASSERT_NOT_NULL(openName);
    TEST_ASSERT_NOT_NULL(markerName);
    openPrototype = ZrCore_ObjectPrototype_New(state, openName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(openPrototype);
    ZrCore_Value_InitAsRawObject(state, &markerKey, ZR_CAST_RAW_OBJECT_AS_SUPER(markerName));
    markerKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &markerValue, 73);
    ZrCore_Object_SetValue(state, &openPrototype->super, &markerKey, &markerValue);

    object = ZrCore_Reflection_NewInterpreterGenericInstanceObject(
            state, runtime, &instance, openPrototype);
    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_EQUAL_PTR(openPrototype, object->prototype);
    TEST_ASSERT_TRUE(ZrCore_Object_IsInstanceOfPrototype(object, openPrototype));
    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, object);
    TEST_ASSERT_NOT_NULL(typeObject);
    assert_object_string_field(state, typeObject, "genericInstanceRoute", "interpreter-deopt");
    assert_object_int_field(state, typeObject, "genericBaseToken", TEST_TYPE_DEF_TOKEN);
    inheritedValue = ZrCore_Object_GetValue(state, object, &markerKey);
    TEST_ASSERT_NOT_NULL(inheritedValue);
    TEST_ASSERT_EQUAL_INT64(73, inheritedValue->value.nativeObject.nativeInt64);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            runtime, TEST_TYPE_SPEC_TOKEN, &aotInstance));
    TEST_ASSERT_NULL(ZrCore_Reflection_NewInterpreterGenericInstanceObject(
            state, runtime, &aotInstance, openPrototype));
    TEST_ASSERT_NULL(ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, &openPrototype->super));
    destroy_reflection_test_state(state);
}

static void test_interpreter_generic_instance_resolves_generic_parameter_type_object(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_BOOL,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrString *openName;
    SZrObjectPrototype *openPrototype;
    SZrObject *object;
    SZrObject *argumentObject;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));
    openName = ZrCore_String_CreateFromNative(state, "Generic");
    TEST_ASSERT_NOT_NULL(openName);
    openPrototype = ZrCore_ObjectPrototype_New(state, openName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(openPrototype);
    object = ZrCore_Reflection_NewInterpreterGenericInstanceObject(
            state, runtime, &instance, openPrototype);
    TEST_ASSERT_NOT_NULL(object);

    argumentObject = ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
            state, runtime, object, TEST_TYPE_DEF_TOKEN, 0u);
    TEST_ASSERT_NOT_NULL(argumentObject);
    assert_object_string_field(state, argumentObject, "genericArgumentKind", "primitive");
    assert_object_int_field(state, argumentObject, "primitiveValueType", ZR_VALUE_TYPE_BOOL);

    argumentObject = ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
            state, runtime, object, TEST_TYPE_DEF_TOKEN, 1u);
    TEST_ASSERT_NOT_NULL(argumentObject);
    assert_object_string_field(state, argumentObject, "genericArgumentKind", "typeToken");
    assert_object_int_field(state, argumentObject, "typeToken", TEST_TYPE_DEF_TOKEN);

    TEST_ASSERT_NULL(ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
            state, runtime, object, TEST_TYPE_SPEC_TOKEN, 0u));
    TEST_ASSERT_NULL(ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
            state, runtime, object, TEST_TYPE_DEF_TOKEN, 2u));
    TEST_ASSERT_NULL(ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
            state, runtime, &openPrototype->super, TEST_TYPE_DEF_TOKEN, 0u));
    destroy_reflection_test_state(state);
}

static void test_interpreter_generic_call_info_context_survives_full_gc(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_BOOL,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrString *openName;
    SZrObjectPrototype *openPrototype;
    SZrObject *object;
    SZrObject *typeObject;
    SZrObject *argumentObject;
    SZrCallInfo callInfo = {0};
    SZrCallInfo *previousCallInfo;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));
    openName = ZrCore_String_CreateFromNative(state, "Generic");
    TEST_ASSERT_NOT_NULL(openName);
    openPrototype = ZrCore_ObjectPrototype_New(state, openName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(openPrototype);
    object = ZrCore_Reflection_NewInterpreterGenericInstanceObject(
            state, runtime, &instance, openPrototype);
    TEST_ASSERT_NOT_NULL(object);

    previousCallInfo = state->callInfoList;
    callInfo.previous = previousCallInfo;
    callInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->callInfoList = &callInfo;
    TEST_ASSERT_TRUE(ZrCore_Reflection_BindInterpreterGenericInstanceCallInfo(
            state, &callInfo, object));
    typeObject = ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(state, &callInfo);
    TEST_ASSERT_NOT_NULL(typeObject);
    assert_object_int_field(state, typeObject, "genericBaseToken", TEST_TYPE_DEF_TOKEN);

    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    typeObject = ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(state, &callInfo);
    TEST_ASSERT_NOT_NULL(typeObject);
    argumentObject = ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject(
            state, runtime, &callInfo, TEST_TYPE_DEF_TOKEN, 1u);
    TEST_ASSERT_NOT_NULL(argumentObject);
    assert_object_string_field(state, argumentObject, "genericArgumentKind", "typeToken");
    assert_object_int_field(state, argumentObject, "typeToken", TEST_TYPE_DEF_TOKEN);

    TEST_ASSERT_FALSE(ZrCore_Reflection_BindInterpreterGenericInstanceCallInfo(
            state, &callInfo, typeObject));
    TEST_ASSERT_NULL(ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(state, &callInfo));
    state->callInfoList = previousCallInfo;
    destroy_reflection_test_state(state);
}

typedef struct SInterpreterGenericMethodExecutionCapture {
    SZrMetadataRuntime *runtime;
    SZrFunction *expectedFunction;
    SZrObject *resolvedArgument;
    TZrUInt32 observedCount;
} SInterpreterGenericMethodExecutionCapture;

static TZrDebugSignal test_capture_interpreter_generic_method_context(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *programCounter,
        TZrUInt32 instructionOffset,
        TZrUInt32 sourceLine,
        TZrPtr userData) {
    SInterpreterGenericMethodExecutionCapture *capture =
            (SInterpreterGenericMethodExecutionCapture *)userData;

    ZR_UNUSED_PARAMETER(programCounter);
    ZR_UNUSED_PARAMETER(instructionOffset);
    ZR_UNUSED_PARAMETER(sourceLine);
    if (state == ZR_NULL || capture == ZR_NULL || function != capture->expectedFunction ||
        capture->observedCount != 0u) {
        return ZR_DEBUG_SIGNAL_NONE;
    }
    capture->observedCount = 1u;
    capture->resolvedArgument =
            ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject(
                    state,
                    capture->runtime,
                    state->callInfoList,
                    TEST_TYPE_DEF_TOKEN,
                    1u);
    state->debugHookSignal = 0u;
    return ZR_DEBUG_SIGNAL_NONE;
}

static SZrFunction *test_create_interpreter_generic_identity_method(SZrState *state) {
    SZrFunction *function = ZrCore_Function_New(state);
    TZrInstruction instruction;

    TEST_ASSERT_NOT_NULL(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode =
            (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = 1u;
    instruction.instruction.operand.operand1[0] = 1u;
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = instruction;
    function->instructionsLength = 1u;
    function->stackSize = 2u;
    function->parameterCount = 2u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static void test_interpreter_generic_instance_executes_resolved_vm_method_with_context(void) {
    SReflectionDynamicGenericFixture fixture;
    SReflectionDynamicGenericFixture wrongFixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrMetadataRuntime *wrongRuntime = fixture_init(&wrongFixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_BOOL,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrString *openName;
    SZrObjectPrototype *openPrototype;
    SZrObject *object;
    SZrFunction *function;
    SZrTypeValue methodArgument;
    SZrTypeValue result;
    SInterpreterGenericMethodExecutionCapture capture = {0};

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));
    openName = ZrCore_String_CreateFromNative(state, "Generic");
    TEST_ASSERT_NOT_NULL(openName);
    openPrototype = ZrCore_ObjectPrototype_New(state, openName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(openPrototype);
    object = ZrCore_Reflection_NewInterpreterGenericInstanceObject(
            state, runtime, &instance, openPrototype);
    TEST_ASSERT_NOT_NULL(object);
    function = test_create_interpreter_generic_identity_method(state);

    capture.runtime = runtime;
    capture.expectedFunction = function;
    ZrCore_Debug_SetTraceObserver(
            state, test_capture_interpreter_generic_method_context, &capture);
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
    ZrCore_Value_InitAsInt(state, &methodArgument, 73);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
            state,
            runtime,
            object,
            TEST_TYPE_DEF_TOKEN,
            function,
            &methodArgument,
            1u,
            &result));
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    state->debugHookSignal = 0u;

    TEST_ASSERT_EQUAL_UINT32(1u, capture.observedCount);
    TEST_ASSERT_NOT_NULL(capture.resolvedArgument);
    assert_object_string_field(state, capture.resolvedArgument, "genericArgumentKind", "typeToken");
    assert_object_int_field(state, capture.resolvedArgument, "typeToken", TEST_TYPE_DEF_TOKEN);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(73, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    ZrCore_Value_InitAsInt(state, &result, 91);
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
            state,
            wrongRuntime,
            object,
            TEST_TYPE_DEF_TOKEN,
            function,
            &methodArgument,
            1u,
            &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    ZrCore_Value_InitAsInt(state, &result, 92);
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
            state,
            runtime,
            object,
            TEST_TYPE_SPEC_TOKEN,
            function,
            &methodArgument,
            1u,
            &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    ZrCore_Value_InitAsInt(state, &result, 93);
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
            state,
            runtime,
            object,
            TEST_TYPE_DEF_TOKEN,
            function,
            ZR_NULL,
            0u,
            &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    destroy_reflection_test_state(state);
}

static void test_interpreter_generic_value_instance_preserves_copy_and_execution_semantics(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_BOOL,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrString *openName;
    SZrString *payloadName;
    SZrObjectPrototype *openStructPrototype;
    SZrObject *object;
    SZrObject *copiedObject;
    SZrObject *typeObject;
    SZrObject *argumentObject;
    SZrFunction *function;
    SZrTypeValue payloadKey;
    SZrTypeValue payloadValue;
    SZrTypeValue sourceValue;
    SZrTypeValue copiedValue;
    SZrTypeValue methodArgument;
    SZrTypeValue result;
    const SZrTypeValue *storedValue;
    SInterpreterGenericMethodExecutionCapture capture = {0};

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    openName = ZrCore_String_CreateFromNative(state, "GenericValue");
    payloadName = ZrCore_String_CreateFromNative(state, "payload");
    TEST_ASSERT_NOT_NULL(openName);
    TEST_ASSERT_NOT_NULL(payloadName);
    openStructPrototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, openName);
    TEST_ASSERT_NOT_NULL(openStructPrototype);

    object = ZrCore_Reflection_NewInterpreterGenericInstanceObject(
            state, runtime, &instance, openStructPrototype);
    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, object->internalType);
    TEST_ASSERT_EQUAL_PTR(openStructPrototype, object->prototype);
    typeObject = ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, object);
    TEST_ASSERT_NOT_NULL(typeObject);
    argumentObject = ZrCore_Reflection_ResolveInterpreterGenericParameterTypeObject(
            state, runtime, object, TEST_TYPE_DEF_TOKEN, 1u);
    TEST_ASSERT_NOT_NULL(argumentObject);
    assert_object_int_field(state, argumentObject, "typeToken", TEST_TYPE_DEF_TOKEN);

    ZrCore_Value_InitAsRawObject(
            state, &payloadKey, ZR_CAST_RAW_OBJECT_AS_SUPER(payloadName));
    payloadKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &payloadValue, 17);
    ZrCore_Object_SetValue(state, object, &payloadKey, &payloadValue);
    ZrCore_Value_InitAsRawObject(
            state, &sourceValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    sourceValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&copiedValue);
    ZrCore_Value_Copy(state, &copiedValue, &sourceValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, copiedValue.type);
    copiedObject = ZR_CAST_OBJECT(state, copiedValue.value.object);
    TEST_ASSERT_NOT_NULL(copiedObject);
    TEST_ASSERT_TRUE(object != copiedObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_STRUCT, copiedObject->internalType);
    TEST_ASSERT_NOT_NULL(
            ZrCore_Reflection_GetInterpreterGenericInstanceTypeObject(state, copiedObject));

    ZrCore_Value_InitAsInt(state, &payloadValue, 29);
    ZrCore_Object_SetValue(state, copiedObject, &payloadKey, &payloadValue);
    storedValue = ZrCore_Object_GetValue(state, object, &payloadKey);
    TEST_ASSERT_NOT_NULL(storedValue);
    TEST_ASSERT_EQUAL_INT64(17, storedValue->value.nativeObject.nativeInt64);
    storedValue = ZrCore_Object_GetValue(state, copiedObject, &payloadKey);
    TEST_ASSERT_NOT_NULL(storedValue);
    TEST_ASSERT_EQUAL_INT64(29, storedValue->value.nativeObject.nativeInt64);

    function = test_create_interpreter_generic_identity_method(state);
    capture.runtime = runtime;
    capture.expectedFunction = function;
    ZrCore_Debug_SetTraceObserver(
            state, test_capture_interpreter_generic_method_context, &capture);
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
    ZrCore_Value_InitAsInt(state, &methodArgument, 117);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeInterpreterGenericInstanceResolvedMethod(
            state,
            runtime,
            object,
            TEST_TYPE_DEF_TOKEN,
            function,
            &methodArgument,
            1u,
            &result));
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    state->debugHookSignal = 0u;
    TEST_ASSERT_EQUAL_UINT32(1u, capture.observedCount);
    TEST_ASSERT_NOT_NULL(capture.resolvedArgument);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(117, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    destroy_reflection_test_state(state);
}

#endif
