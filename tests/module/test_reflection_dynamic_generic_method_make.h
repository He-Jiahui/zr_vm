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
    return closure->nativeFunction(state);
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
    TEST_ASSERT_NOT_NULL(closure->nativeFunction);
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
    TEST_ASSERT_EQUAL_INT64(1, closure->nativeFunction(state));
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

static void test_reflection_runtime_module_exports_bound_make_generic_method(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrObjectModule *runtimeModule;
    SZrObjectModule *reflectionModule;
    SZrObject *definitionObject;
    SZrObject *contextObject;
    SZrObject *argumentsArray;
    SZrString *exportName;
    const SZrTypeValue *exportValue;
    SZrClosureNative *closure;
    SZrTypeValue *result;
    TZrStackValuePointer initialStackTop;
    TZrStackValuePointer rootBase;
    TZrStackValuePointer functionBase;

    TEST_ASSERT_NOT_NULL(state);
    initialStackTop = state->stackTop.valuePointer;
    TEST_ASSERT_NULL(ZrCore_Reflection_CreateModuleForRuntime(state, runtime));
    TEST_ASSERT_EQUAL_PTR(initialStackTop, state->stackTop.valuePointer);
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

    reflectionModule = ZrCore_Reflection_CreateModuleForRuntime(state, runtime);
    TEST_ASSERT_NOT_NULL(reflectionModule);
    TEST_ASSERT_EQUAL_PTR(initialStackTop, state->stackTop.valuePointer);
    rootBase = state->stackTop.valuePointer;
    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(rootBase),
            ZR_CAST_RAW_OBJECT_AS_SUPER(reflectionModule));
    state->stackTop.valuePointer = rootBase + 1;
    rootBase = ZrCore_Function_CheckStackAndGc(state, 4u, rootBase);
    reflectionModule = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    TEST_ASSERT_NOT_NULL(reflectionModule->moduleName);
    TEST_ASSERT_NOT_NULL(reflectionModule->fullPath);
    TEST_ASSERT_EQUAL_STRING("zr.reflection", ZrCore_String_GetNativeString(reflectionModule->moduleName));
    TEST_ASSERT_EQUAL_STRING("zr.reflection", ZrCore_String_GetNativeString(reflectionModule->fullPath));
    TEST_ASSERT_EQUAL_UINT64(
            ZrCore_Module_CalculatePathHash(state, reflectionModule->fullPath),
            reflectionModule->pathHash);
    TEST_ASSERT_EQUAL_UINT32(ZR_MODULE_INIT_STATE_READY, reflectionModule->initState);
    TEST_ASSERT_FALSE(reflectionModule->hasMetadataRuntime);
    TEST_ASSERT_EQUAL_UINT64(1u, reflectionModule->super.nodeMap.elementCount);

    exportName = ZrCore_String_CreateFromNative(state, "MakeGenericMethod");
    TEST_ASSERT_NOT_NULL(exportName);
    reflectionModule = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    exportValue = ZrCore_Module_GetPubExport(state, reflectionModule, exportName);
    TEST_ASSERT_NOT_NULL(exportValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, exportValue->type);
    TEST_ASSERT_TRUE(exportValue->isNative);
    TEST_ASSERT_NOT_NULL(exportValue->value.object);
    closure = ZR_CAST_NATIVE_CLOSURE(state, exportValue->value.object);
    TEST_ASSERT_NOT_NULL(closure->nativeFunction);

    functionBase = rootBase + 1;
    prepare_make_generic_method_native_entry(
            state, functionBase, closure, definitionObject, argumentsArray, 2u);
    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);

    reflectionModule = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    TEST_ASSERT_NOT_NULL(reflectionModule);
    exportName = ZrCore_String_CreateFromNative(state, "MakeGenericMethod");
    TEST_ASSERT_NOT_NULL(exportName);
    reflectionModule = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    exportValue = ZrCore_Module_GetPubExport(state, reflectionModule, exportName);
    TEST_ASSERT_NOT_NULL(exportValue);
    TEST_ASSERT_EQUAL_PTR(
            ZrCore_Stack_GetValue(functionBase)->value.object,
            exportValue->value.object);
    closure = ZR_CAST_NATIVE_CLOSURE(state, exportValue->value.object);
    TEST_ASSERT_EQUAL_INT64(1, closure->nativeFunction(state));
    result = ZrCore_Stack_GetValue(functionBase);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result->type);
    assert_object_string_field(
            state, ZR_CAST_OBJECT(state, result->value.object), "kind", "constructedGenericMethod");

    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(definitionObject)));
    destroy_reflection_test_state(state);
}

static void test_reflection_runtime_module_cache_is_owned_by_target_module(void) {
    SMethodSpecGenericContextFixture firstFixture;
    SMethodSpecGenericContextFixture secondFixture;
    SZrMetadataRuntime *fixtureRuntime = method_spec_generic_context_fixture_init(&firstFixture);
    SZrState *state = create_reflection_test_state();
    SZrObjectModule *firstTarget;
    SZrObjectModule *secondTarget;
    SZrMetadataRuntime *firstRuntime;
    SZrMetadataRuntime *secondRuntime;
    SZrObjectModule *firstService;
    SZrObjectModule *secondService;
    SZrObjectModule *corruptResult;
    SZrString *cacheName;
    SZrString *exportName;
    const SZrTypeValue *cacheValue;
    const SZrTypeValue *exportValue;
    SZrTypeValue *mutableCacheValue;
    SZrTypeValue *mutableExportValue;
    SZrTypeValue invalidCacheValue;
    SZrClosureNative *cachedClosure;
    SZrClosureValue *captureOwner;
    TZrStackValuePointer rootBase;
    TZrStackValuePointer savedStackTop;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(ZrCore_Reflection_GetOrCreateModuleForRuntime(state, fixtureRuntime));
    firstTarget = ZrCore_Module_Create(state);
    secondTarget = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(firstTarget);
    TEST_ASSERT_NOT_NULL(secondTarget);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(firstTarget)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(secondTarget)));
    firstRuntime = ZrCore_Module_AttachMetadataRuntime(
            firstTarget, &firstFixture.metadataFunction, &firstFixture.registration);
    TEST_ASSERT_NOT_NULL(firstRuntime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(
            firstRuntime, firstFixture.metadataBytes, sizeof(firstFixture.metadataBytes)));
    TEST_ASSERT_NOT_NULL(method_spec_generic_context_fixture_init(&secondFixture));
    secondRuntime = ZrCore_Module_AttachMetadataRuntime(
            secondTarget, &secondFixture.metadataFunction, &secondFixture.registration);
    TEST_ASSERT_NOT_NULL(secondRuntime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(
            secondRuntime, secondFixture.metadataBytes, sizeof(secondFixture.metadataBytes)));

    rootBase = state->stackTop.valuePointer;
    rootBase = ZrCore_Function_CheckStackAndGc(state, 4u, rootBase);
    ZrCore_Value_InitAsRawObject(
            state, ZrCore_Stack_GetValue(rootBase), ZR_CAST_RAW_OBJECT_AS_SUPER(firstTarget));
    ZrCore_Value_InitAsRawObject(
            state, ZrCore_Stack_GetValue(rootBase + 1), ZR_CAST_RAW_OBJECT_AS_SUPER(secondTarget));
    state->stackTop.valuePointer = rootBase + 2;

    savedStackTop = state->stackTop.valuePointer;
    firstService = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime);
    TEST_ASSERT_NOT_NULL(firstService);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(firstTarget)));
    TEST_ASSERT_TRUE((firstTarget->super.super.garbageCollectMark.pinFlags &
                      ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0u);
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(
            state, ZrCore_Stack_GetValue(rootBase + 2), ZR_CAST_RAW_OBJECT_AS_SUPER(firstService));
    state->stackTop.valuePointer = rootBase + 3;
    savedStackTop = state->stackTop.valuePointer;
    TEST_ASSERT_EQUAL_PTR(
            firstService,
            ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime));
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);

    exportName = ZrCore_String_CreateFromNative(state, "MakeGenericMethod");
    TEST_ASSERT_NOT_NULL(exportName);
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    exportValue = ZrCore_Module_GetPubExport(state, firstService, exportName);
    TEST_ASSERT_NOT_NULL(exportValue);
    cachedClosure = ZR_CAST_NATIVE_CLOSURE(state, exportValue->value.object);
    captureOwner = (SZrClosureValue *)ZrCore_ClosureNative_GetCaptureOwner(cachedClosure, 0u);
    TEST_ASSERT_NOT_NULL(captureOwner);
    TEST_ASSERT_TRUE(ZrCore_ClosureValue_IsClosed(captureOwner));
    captureOwner->value.valuePointer = rootBase;
    savedStackTop = state->stackTop.valuePointer;
    corruptResult = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime);
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    exportName = ZrCore_String_CreateFromNative(state, "MakeGenericMethod");
    TEST_ASSERT_NOT_NULL(exportName);
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    exportValue = ZrCore_Module_GetPubExport(state, firstService, exportName);
    TEST_ASSERT_NOT_NULL(exportValue);
    cachedClosure = ZR_CAST_NATIVE_CLOSURE(state, exportValue->value.object);
    captureOwner = (SZrClosureValue *)ZrCore_ClosureNative_GetCaptureOwner(cachedClosure, 0u);
    TEST_ASSERT_NOT_NULL(captureOwner);
    captureOwner->value.valuePointer = ZR_CAST_STACK_VALUE(&captureOwner->link.closedValue);
    TEST_ASSERT_NULL(corruptResult);
    TEST_ASSERT_EQUAL_PTR(
            firstService,
            ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime));

    firstTarget = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    firstService->super.prototype = (SZrObjectPrototype *)&firstTarget->super;
    savedStackTop = state->stackTop.valuePointer;
    corruptResult = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime);
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    firstService->super.prototype = ZR_NULL;
    TEST_ASSERT_NULL(corruptResult);
    TEST_ASSERT_EQUAL_PTR(
            firstService,
            ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime));

    exportName = ZrCore_String_CreateFromNative(state, "MakeGenericMethod");
    TEST_ASSERT_NOT_NULL(exportName);
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    mutableExportValue = (SZrTypeValue *)ZrCore_Module_GetPubExport(
            state, firstService, exportName);
    TEST_ASSERT_NOT_NULL(mutableExportValue);
    TEST_ASSERT_TRUE(mutableExportValue->isGarbageCollectable);
    mutableExportValue->isGarbageCollectable = ZR_FALSE;
    savedStackTop = state->stackTop.valuePointer;
    corruptResult = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime);
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    exportName = ZrCore_String_CreateFromNative(state, "MakeGenericMethod");
    TEST_ASSERT_NOT_NULL(exportName);
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    mutableExportValue = (SZrTypeValue *)ZrCore_Module_GetPubExport(
            state, firstService, exportName);
    TEST_ASSERT_NOT_NULL(mutableExportValue);
    mutableExportValue->isGarbageCollectable = ZR_TRUE;
    TEST_ASSERT_NULL(corruptResult);
    TEST_ASSERT_EQUAL_PTR(
            firstService,
            ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime));

    firstTarget = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    firstService->moduleName = (SZrString *)firstTarget;
    savedStackTop = state->stackTop.valuePointer;
    corruptResult = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime);
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    firstService = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 2)->value.object;
    firstService->moduleName = firstService->fullPath;
    TEST_ASSERT_NULL(corruptResult);
    TEST_ASSERT_EQUAL_PTR(
            firstService,
            ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime));

    secondTarget->proNodeMap.isValid = ZR_FALSE;
    savedStackTop = state->stackTop.valuePointer;
    TEST_ASSERT_NULL(ZrCore_Reflection_GetOrCreateModuleForRuntime(state, secondRuntime));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(secondTarget)));
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    secondTarget->proNodeMap.isValid = ZR_TRUE;

    secondService = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, secondRuntime);
    TEST_ASSERT_NOT_NULL(secondService);
    TEST_ASSERT_FALSE(ZrCore_GarbageCollector_IsObjectIgnored(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(secondTarget)));
    TEST_ASSERT_TRUE((secondTarget->super.super.garbageCollectMark.pinFlags &
                      ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) != 0u);
    TEST_ASSERT_NOT_EQUAL(firstService, secondService);
    ZrCore_Value_InitAsRawObject(
            state, ZrCore_Stack_GetValue(rootBase + 3), ZR_CAST_RAW_OBJECT_AS_SUPER(secondService));
    state->stackTop.valuePointer = rootBase + 4;

    cacheName = ZrCore_String_CreateFromNative(state, "__zr_reflection_service_module");
    TEST_ASSERT_NOT_NULL(cacheName);
    firstTarget = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    cacheValue = ZrCore_Module_GetProExport(state, firstTarget, cacheName);
    TEST_ASSERT_NOT_NULL(cacheValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, cacheValue->type);
    TEST_ASSERT_EQUAL_PTR(
            ZrCore_Stack_GetValue(rootBase + 2)->value.object,
            cacheValue->value.object);
    TEST_ASSERT_FALSE(cacheValue->isNative);
    TEST_ASSERT_TRUE(cacheValue->isGarbageCollectable);
    TEST_ASSERT_NULL(ZrCore_Module_GetPubExport(state, firstTarget, cacheName));

    mutableCacheValue = (SZrTypeValue *)cacheValue;
    mutableCacheValue->isNative = ZR_TRUE;
    mutableCacheValue->isGarbageCollectable = ZR_FALSE;
    savedStackTop = state->stackTop.valuePointer;
    corruptResult = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime);
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);
    cacheName = ZrCore_String_CreateFromNative(state, "__zr_reflection_service_module");
    TEST_ASSERT_NOT_NULL(cacheName);
    firstTarget = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    mutableCacheValue = (SZrTypeValue *)ZrCore_Module_GetProExport(state, firstTarget, cacheName);
    TEST_ASSERT_NOT_NULL(mutableCacheValue);
    mutableCacheValue->isNative = ZR_FALSE;
    mutableCacheValue->isGarbageCollectable = ZR_TRUE;
    TEST_ASSERT_NULL(corruptResult);
    TEST_ASSERT_EQUAL_PTR(
            ZrCore_Stack_GetValue(rootBase + 2)->value.object,
            ZR_CAST_RAW_OBJECT_AS_SUPER(
                    ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime)));

    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    firstTarget = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    secondTarget = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase + 1)->value.object;
    firstRuntime = ZrCore_Module_GetMetadataRuntime(firstTarget);
    secondRuntime = ZrCore_Module_GetMetadataRuntime(secondTarget);
    TEST_ASSERT_NOT_NULL(firstRuntime);
    TEST_ASSERT_NOT_NULL(secondRuntime);
    savedStackTop = state->stackTop.valuePointer;
    firstService = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime);
    secondService = ZrCore_Reflection_GetOrCreateModuleForRuntime(state, secondRuntime);
    TEST_ASSERT_NOT_NULL(firstService);
    TEST_ASSERT_NOT_NULL(secondService);
    TEST_ASSERT_EQUAL_PTR(
            ZrCore_Stack_GetValue(rootBase + 2)->value.object,
            ZR_CAST_RAW_OBJECT_AS_SUPER(firstService));
    TEST_ASSERT_EQUAL_PTR(
            ZrCore_Stack_GetValue(rootBase + 3)->value.object,
            ZR_CAST_RAW_OBJECT_AS_SUPER(secondService));
    TEST_ASSERT_EQUAL_PTR(savedStackTop, state->stackTop.valuePointer);

    cacheName = ZrCore_String_CreateFromNative(state, "__zr_reflection_service_module");
    TEST_ASSERT_NOT_NULL(cacheName);
    firstTarget = (SZrObjectModule *)ZrCore_Stack_GetValue(rootBase)->value.object;
    ZrCore_Value_InitAsInt(state, &invalidCacheValue, 17);
    ZrCore_Module_AddProExport(state, firstTarget, cacheName, &invalidCacheValue);
    TEST_ASSERT_NULL(ZrCore_Reflection_GetOrCreateModuleForRuntime(state, firstRuntime));
    cacheValue = ZrCore_Module_GetProExport(state, firstTarget, cacheName);
    TEST_ASSERT_NOT_NULL(cacheValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, cacheValue->type);

    destroy_reflection_test_state(state);
}

#endif
