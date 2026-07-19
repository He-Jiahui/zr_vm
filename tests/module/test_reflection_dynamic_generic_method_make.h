#ifndef ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_METHOD_MAKE_H
#define ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_METHOD_MAKE_H

static void test_make_generic_method_object_resolves_and_materializes(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_UINT64,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN,
            },
    };
    SZrObject *methodObject;

    TEST_ASSERT_NOT_NULL(state);
    methodObject = ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments));
    TEST_ASSERT_NOT_NULL(methodObject);
    assert_object_string_field(state, methodObject, "kind", "constructedGenericMethod");
    assert_object_string_field(state, methodObject, "name", "Map");
    assert_object_int_field(
            state, methodObject, "metadataToken", TEST_METHOD_CONTEXT_SPEC_TOKEN);
    assert_object_int_field(
            state, methodObject, "genericMethodToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);

    arguments[1].typeToken = TEST_TYPE_DEF_TOKEN;
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    arguments[1].typeToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN;
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_SPEC_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            ZR_NULL,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            ZR_NULL,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            ZR_NULL,
            ZR_ARRAY_COUNT(arguments)));

    destroy_reflection_test_state(state);
}

static void test_make_generic_method_from_objects_decodes_reflection_arguments(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrMetadataRuntime differentRuntime;
    SZrState *state = create_reflection_test_state();
    SZrObject *definitionObject;
    SZrObject *contextObject;
    SZrObject *argumentsArray;
    SZrObject *firstArgument;
    SZrObject *constructedObject;

    TEST_ASSERT_NOT_NULL(state);
    differentRuntime = *runtime;
    definitionObject = ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, TEST_METHOD_CONTEXT_SPEC_TOKEN);
    TEST_ASSERT_NOT_NULL(definitionObject);
    TEST_ASSERT_NOT_NULL(contextObject);
    argumentsArray = assert_object_object_field(
            state, contextObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    firstArgument = assert_array_object_entry(state, argumentsArray, 0u);

    constructedObject = ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, argumentsArray);
    TEST_ASSERT_NOT_NULL(constructedObject);
    assert_object_string_field(
            state, constructedObject, "kind", "constructedGenericMethod");
    assert_object_string_field(state, constructedObject, "name", "Map");
    assert_object_int_field(
            state, constructedObject, "metadataToken", TEST_METHOD_CONTEXT_SPEC_TOKEN);
    assert_object_int_field(
            state,
            assert_object_object_field(
                    state,
                    constructedObject,
                    "genericMethodDefinition",
                    ZR_VALUE_TYPE_OBJECT),
            "metadataToken",
            TEST_METHOD_CONTEXT_MEMBER_TOKEN);

    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, constructedObject, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, &differentRuntime, definitionObject, argumentsArray));
    set_object_int_field_value(
            state,
            firstArgument,
            "genericArgumentKindValue",
            ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION);
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, argumentsArray));
    set_object_int_field_value(
            state,
            firstArgument,
            "genericArgumentKindValue",
            ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE);
    set_object_int_field_value(
            state, firstArgument, "primitiveValueType", ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, argumentsArray));
    set_object_int_field_value(
            state, firstArgument, "primitiveValueType", ZR_VALUE_TYPE_UINT64);
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, contextObject));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            ZR_NULL, runtime, definitionObject, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, ZR_NULL, definitionObject, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, ZR_NULL, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, ZR_NULL));

    destroy_reflection_test_state(state);
}

static void prepare_make_generic_method_native_entry(
        SZrState *state,
        TZrStackValuePointer functionBase,
        SZrClosureNative *closure,
        SZrObject *definitionObject,
        SZrObject *argumentsObject,
        TZrUInt32 argumentCount) {
    SZrTypeValue *value;

    state->stackTop.valuePointer = functionBase;
    value = ZrCore_Stack_GetValue(functionBase);
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    if (argumentCount > 0u) {
        value = ZrCore_Stack_GetValue(functionBase + 1);
        ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject));
    }
    if (argumentCount > 1u) {
        value = ZrCore_Stack_GetValue(functionBase + 2);
        ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsObject));
    }

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + argumentCount + 1u;
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    state->baseCallInfo.previous = ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;
    state->stackTop.valuePointer = functionBase + argumentCount + 1u;
}

static TZrInt64 invoke_make_generic_method_native_entry(
        SZrState *state,
        TZrStackValuePointer functionBase,
        SZrClosureNative *closure,
        SZrObject *definitionObject,
        SZrObject *argumentsObject,
        TZrUInt32 argumentCount,
        TZrBool collectBeforeInvoke) {
    prepare_make_generic_method_native_entry(
            state, functionBase, closure, definitionObject, argumentsObject, argumentCount);
    if (collectBeforeInvoke) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    }
    return ZrCore_Reflection_MakeGenericMethodNativeEntry(state);
}

static void test_make_generic_method_native_entry_uses_trusted_closure_runtime(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrObjectModule *runtimeModule;
    SZrObject *definitionObject;
    SZrObject *contextObject;
    SZrObject *argumentsArray;
    SZrClosureNative *closure;
    SZrClosureNative *invalidClosure;
    SZrTypeValue *captureValue;
    SZrTypeValue *result;
    TZrStackValuePointer functionBase;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(state, runtime));
    runtimeModule = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(runtimeModule);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule)));
    runtime = ZrCore_Module_AttachMetadataRuntime(
            runtimeModule, &fixture.metadataFunction, &fixture.registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(
            runtime, fixture.metadataBytes, sizeof(fixture.metadataBytes)));
    definitionObject = ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, TEST_METHOD_CONTEXT_SPEC_TOKEN);
    TEST_ASSERT_NOT_NULL(definitionObject);
    TEST_ASSERT_NOT_NULL(contextObject);
    argumentsArray = assert_object_object_field(
            state, contextObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject)));

    functionBase = state->stackTop.valuePointer;
    functionBase = ZrCore_Function_CheckStackAndGc(state, 3u, functionBase);
    closure = ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(state, runtime);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_EQUAL_PTR(ZrCore_Reflection_MakeGenericMethodNativeEntry, closure->nativeFunction);
    TEST_ASSERT_EQUAL_UINT64(1u, closure->closureValueCount);
    TEST_ASSERT_TRUE((runtimeModule->super.super.garbageCollectMark.pinFlags &
                      ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0u);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED,
            runtimeModule->super.super.garbageCollectMark.storageKind);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_GARBAGE_COLLECT_REGION_KIND_PINNED,
            runtimeModule->super.super.garbageCollectMark.regionKind);
    TEST_ASSERT_NOT_NULL(ZrCore_ClosureNative_GetCaptureOwner(closure, 0u));
    captureValue = ZrCore_ClosureNative_GetCaptureValue(closure, 0u);
    TEST_ASSERT_NOT_NULL(captureValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, captureValue->type);
    TEST_ASSERT_EQUAL_PTR(runtimeModule, captureValue->value.object);
    TEST_ASSERT_EQUAL_PTR(runtime, ZrCore_Module_GetMetadataRuntime(runtimeModule));

    prepare_make_generic_method_native_entry(
            state, functionBase, closure, definitionObject, argumentsArray, 2u);
    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeModule)));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    closure = ZR_CAST_NATIVE_CLOSURE(
            state, ZrCore_Stack_GetValue(functionBase)->value.object);
    captureValue = ZrCore_ClosureNative_GetCaptureValue(closure, 0u);
    TEST_ASSERT_NOT_NULL(captureValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, captureValue->type);
    runtimeModule = (SZrObjectModule *)captureValue->value.object;
    runtime = ZrCore_Module_GetMetadataRuntime(runtimeModule);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_NULL(closure->closureValuesExtend[0]);
    TEST_ASSERT_EQUAL_INT64(1, ZrCore_Reflection_MakeGenericMethodNativeEntry(state));
    result = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result->type);
    assert_object_string_field(
            state, ZR_CAST_OBJECT(state, result->value.object), "kind", "constructedGenericMethod");
    argumentsArray = assert_object_object_field(
            state, contextObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);

    closure = ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(state, runtime);
    TEST_ASSERT_NOT_NULL(closure);
    captureValue = ZrCore_ClosureNative_GetCaptureValue(closure, 0u);
    TEST_ASSERT_NOT_NULL(captureValue);
    ZrCore_Value_InitAsInt(state, captureValue, 7);
    TEST_ASSERT_EQUAL_INT64(
            1,
            invoke_make_generic_method_native_entry(
                    state, functionBase, closure, definitionObject, argumentsArray, 2u, ZR_FALSE));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, ZrCore_Stack_GetValue(functionBase)->type);

    invalidClosure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(invalidClosure);
    invalidClosure->nativeFunction = ZrCore_Reflection_MakeGenericMethodNativeEntry;
    TEST_ASSERT_EQUAL_INT64(
            1,
            invoke_make_generic_method_native_entry(
                    state, functionBase, invalidClosure, definitionObject, argumentsArray, 2u, ZR_FALSE));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, ZrCore_Stack_GetValue(functionBase)->type);

    closure = ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(state, runtime);
    TEST_ASSERT_NOT_NULL(closure);
    TEST_ASSERT_EQUAL_INT64(
            1,
            invoke_make_generic_method_native_entry(
                    state, functionBase, closure, definitionObject, argumentsArray, 1u, ZR_FALSE));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, ZrCore_Stack_GetValue(functionBase)->type);
    TEST_ASSERT_EQUAL_INT64(
            1,
            invoke_make_generic_method_native_entry(
                    state, functionBase, closure, definitionObject, contextObject, 2u, ZR_FALSE));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, ZrCore_Stack_GetValue(functionBase)->type);

    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject)));
    destroy_reflection_test_state(state);
}

#endif
