#ifndef ZR_VM_TEST_REFLECTION_RUNTIME_MODULE_IMPORT_H
#define ZR_VM_TEST_REFLECTION_RUNTIME_MODULE_IMPORT_H

#define TEST_REFLECTION_MODULE_PATH "zr.reflection"

static SZrObjectModule *reflection_import_create_loaded_runtime_module(
        SZrState *state,
        SMethodSpecGenericContextFixture *fixture,
        SZrFunction *metadataFunction,
        TZrNativeString pathText) {
    SZrObjectModule *module;
    SZrMetadataRuntime *runtime;
    SZrString *path;
    TZrBool ownsMetadataFunction = ZR_FALSE;

    if (metadataFunction == ZR_NULL) {
        metadataFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(metadataFunction);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataFunction)));
        ownsMetadataFunction = ZR_TRUE;
    }

    module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(module)));

    runtime = ZrCore_Module_AttachMetadataRuntime(
            module, metadataFunction, &fixture->registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(
            runtime, fixture->metadataBytes, sizeof(fixture->metadataBytes)));

    path = ZrCore_String_CreateFromNative(state, pathText);
    TEST_ASSERT_NOT_NULL(path);
    ZrCore_Module_SetInfo(
            state, module, path, ZrCore_Module_CalculatePathHash(state, path), path);
    ZrCore_Module_SetInitializationState(module, ZR_MODULE_INIT_STATE_READY);
    ZrCore_Module_AddToCache(state, path, module);
    if (ZrCore_GarbageCollector_IsObjectIgnored(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(module))) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(module)));
    }
    if (ownsMetadataFunction) {
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataFunction)));
    }
    return module;
}

static TZrStackValuePointer reflection_import_invoke_bytes(
        SZrState *state,
        SZrFunction *callerFunction,
        TZrNativeString pathText,
        TZrSize pathLength,
        TZrBool guarded,
        TZrBool forceGcDuringImport) {
    SZrCallInfo callerCallInfo;
    SZrString *path;
    SZrTypeValue *pathValue;
    TZrStackValuePointer functionBase;
    TZrInt64 resultCount;

    functionBase = state->stackTop.valuePointer;
    functionBase = ZrCore_Function_CheckStackAndGc(state, 2u, functionBase);
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    path = ZrCore_String_Create(state, pathText, pathLength);
    TEST_ASSERT_NOT_NULL(path);
    pathValue = ZrCore_Stack_GetValue(functionBase + 1);
    ZrCore_Value_InitAsRawObject(
            state, pathValue, ZR_CAST_RAW_OBJECT_AS_SUPER(path));
    pathValue->type = ZR_VALUE_TYPE_STRING;
    state->stackTop.valuePointer = functionBase + 2;

    memset(&callerCallInfo, 0, sizeof(callerCallInfo));
    callerCallInfo.functionBase.valuePointer = functionBase;
    callerCallInfo.functionTop.valuePointer = functionBase + 1;
    callerCallInfo.metadataFunction = callerFunction;
    callerCallInfo.next = &state->baseCallInfo;

    state->baseCallInfo.functionBase.valuePointer = functionBase;
    state->baseCallInfo.functionTop.valuePointer = functionBase + 2;
    state->baseCallInfo.callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    state->baseCallInfo.metadataFunction = ZR_NULL;
    state->baseCallInfo.previous = callerFunction != ZR_NULL ? &callerCallInfo : ZR_NULL;
    state->baseCallInfo.next = ZR_NULL;
    state->callInfoList = &state->baseCallInfo;

    if (forceGcDuringImport) {
        TEST_ASSERT_NOT_NULL(callerFunction);
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IsObjectIgnored(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction)));
        TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(callerFunction)));
        reflection_import_allocator_fail_next();
    }
    resultCount = guarded
                          ? ZrCore_Module_ImportGuardNativeEntry(state)
                          : ZrCore_Module_ImportNativeEntry(state);
    state->baseCallInfo.previous = ZR_NULL;
    if (forceGcDuringImport) {
        reflection_import_allocator_assert_consumed_and_clear();
    }
    TEST_ASSERT_EQUAL_INT64(1, resultCount);
    return functionBase;
}

static TZrStackValuePointer reflection_import_invoke(
        SZrState *state,
        SZrFunction *callerFunction,
        TZrNativeString pathText,
        TZrBool guarded) {
    return reflection_import_invoke_bytes(
            state,
            callerFunction,
            pathText,
            ZrCore_NativeString_Length(pathText),
            guarded,
            ZR_FALSE);
}

static SZrObjectModule *reflection_import_assert_module_result(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    SZrTypeValue *result = ZrCore_Stack_GetValue(functionBase);
    SZrObject *object;

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result->type);
    TEST_ASSERT_FALSE(result->isNative);
    TEST_ASSERT_TRUE(result->isGarbageCollectable);
    TEST_ASSERT_NOT_NULL(result->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_RAW_OBJECT_TYPE_OBJECT, result->value.object->type);
    object = ZR_CAST_OBJECT(state, result->value.object);
    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_MODULE, object->internalType);
    return (SZrObjectModule *)object;
}

static void test_reflection_module_import_uses_loaded_caller_runtime(void) {
    SMethodSpecGenericContextFixture firstFixture;
    SMethodSpecGenericContextFixture secondFixture;
    SZrState *state = create_reflection_test_state();
    SZrObjectModule *firstTarget;
    SZrObjectModule *secondTarget;
    SZrObjectModule *ambiguousTarget;
    SZrFunction *firstMetadataFunction;
    SZrFunction *secondMetadataFunction;
    SZrFunction *gcCaller;
    SZrFunction *postGcCaller;
    SZrFunctionModuleEffect *gcEffect;
    SZrFunction childCaller;
    SZrFunction ownerCycleCaller;
    SZrFunction ownerCyclePeer;
    SZrFunction unknownCaller;
    SZrString *reflectionPath;
    SZrString *firstAliasPath;
    SZrString *malformedPath;
    SZrObject *loadedRegistry;
    SZrHashKeyValuePair *aliasPair;
    SZrRawObject *aliasKeyObject;
    SZrHashKeyValuePair overflowPair;
    SZrHashKeyValuePair *savedAliasNext;
    SZrGarbageCollectorStatsSnapshot gcStatsBefore;
    SZrGarbageCollectorStatsSnapshot gcStatsAfter;
    SZrTypeValue aliasKey;
    SZrTypeValue malformedKey;
    SZrTypeValue malformedValue;
    TZrChar embeddedNulPath[] = "zr.reflection\0extra";
    TZrStackValuePointer postGcCallerRoot;
    TZrStackValuePointer firstResultBase;
    TZrStackValuePointer repeatedResultBase;
    TZrStackValuePointer secondResultBase;
    TZrStackValuePointer rejectedResultBase;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(method_spec_generic_context_fixture_init(&firstFixture));
    TEST_ASSERT_NOT_NULL(method_spec_generic_context_fixture_init(&secondFixture));
    firstTarget = reflection_import_create_loaded_runtime_module(
            state, &firstFixture, ZR_NULL, "test.reflection.first");
    secondTarget = reflection_import_create_loaded_runtime_module(
            state, &secondFixture, ZR_NULL, "test.reflection.second");
    TEST_ASSERT_NOT_EQUAL(firstTarget, secondTarget);
    firstMetadataFunction = ZrCore_Module_GetMetadataRuntime(firstTarget)->metadataFunction;
    secondMetadataFunction = ZrCore_Module_GetMetadataRuntime(secondTarget)->metadataFunction;
    TEST_ASSERT_NOT_NULL(firstMetadataFunction);
    TEST_ASSERT_NOT_NULL(secondMetadataFunction);

    firstAliasPath = ZrCore_String_CreateFromNative(
            state, "test.reflection.first.alias");
    TEST_ASSERT_NOT_NULL(firstAliasPath);
    ZrCore_Module_AddToCache(state, firstAliasPath, firstTarget);

    memset(&childCaller, 0, sizeof(childCaller));
    childCaller.ownerFunction = firstMetadataFunction;

    gcCaller = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(gcCaller);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(gcCaller)));
    gcCaller->ownerFunction = firstMetadataFunction;

    postGcCaller = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(postGcCaller);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(postGcCaller)));
    postGcCaller->ownerFunction = firstMetadataFunction;
    gcEffect = (SZrFunctionModuleEffect *)
            ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(SZrFunctionModuleEffect),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(gcEffect);
    postGcCaller->moduleEntryEffects = gcEffect;
    postGcCaller->moduleEntryEffectLength = 1u;
    memset(gcEffect, 0, sizeof(*gcEffect));
    gcEffect->kind = ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    gcEffect->moduleName = ZrCore_String_CreateFromNative(
            state, TEST_REFLECTION_MODULE_PATH);
    gcEffect->symbolName = ZrCore_String_CreateFromNative(
            state, "MakeGenericMethod");
    gcEffect->targetSignatureHash = 0x1234u;
    TEST_ASSERT_NOT_NULL(gcEffect->moduleName);
    TEST_ASSERT_NOT_NULL(gcEffect->symbolName);

    postGcCallerRoot = state->stackTop.valuePointer;
    postGcCallerRoot = ZrCore_Function_CheckStackAndGc(
            state, 1u, postGcCallerRoot);
    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(postGcCallerRoot),
            ZR_CAST_RAW_OBJECT_AS_SUPER(postGcCaller));
    ZrCore_Stack_GetValue(postGcCallerRoot)->type = ZR_VALUE_TYPE_FUNCTION;
    state->stackTop.valuePointer = postGcCallerRoot + 1;
    reflection_import_allocator_prepare_post_gc_caller(
            state, postGcCallerRoot);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(postGcCaller)));

    ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &gcStatsBefore);
    rejectedResultBase = reflection_import_invoke_bytes(
            state,
            gcCaller,
            TEST_REFLECTION_MODULE_PATH,
            sizeof(TEST_REFLECTION_MODULE_PATH) - 1u,
            ZR_TRUE,
            ZR_TRUE);
    ZrCore_GarbageCollector_GetStatsSnapshot(state->global, &gcStatsAfter);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));
    TEST_ASSERT_TRUE(
            gcStatsAfter.fullCollectionCount > gcStatsBefore.fullCollectionCount);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    firstResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    reflection_import_assert_module_result(state, firstResultBase);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    repeatedResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_FALSE);
    reflection_import_assert_module_result(state, repeatedResultBase);
    TEST_ASSERT_EQUAL_PTR(
            ZrCore_Stack_GetValue(firstResultBase)->value.object,
            ZrCore_Stack_GetValue(repeatedResultBase)->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    secondResultBase = reflection_import_invoke(
            state,
            secondMetadataFunction,
            TEST_REFLECTION_MODULE_PATH,
            ZR_FALSE);
    reflection_import_assert_module_result(state, secondResultBase);
    TEST_ASSERT_NOT_EQUAL(
            ZrCore_Stack_GetValue(firstResultBase)->value.object,
            ZrCore_Stack_GetValue(secondResultBase)->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    rejectedResultBase = reflection_import_invoke_bytes(
            state,
            &childCaller,
            embeddedNulPath,
            sizeof(embeddedNulPath) - 1u,
            ZR_TRUE,
            ZR_FALSE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));

    loadedRegistry = ZR_CAST_OBJECT(
            state, state->global->loadedModulesRegistry.value.object);
    TEST_ASSERT_NOT_NULL(loadedRegistry);
    malformedPath = ZrCore_String_CreateFromNative(
            state, "test.reflection.malformed.registry");
    TEST_ASSERT_NOT_NULL(malformedPath);
    ZrCore_Value_InitAsRawObject(
            state, &malformedKey, ZR_CAST_RAW_OBJECT_AS_SUPER(malformedPath));
    malformedKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &malformedValue, 17);
    ZrCore_Object_SetValue(
            state, loadedRegistry, &malformedKey, &malformedValue);
    rejectedResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));
    ZrCore_Module_RemoveFromCache(state, malformedPath);

    ZrCore_Value_InitAsRawObject(
            state, &aliasKey, ZR_CAST_RAW_OBJECT_AS_SUPER(firstAliasPath));
    aliasKey.type = ZR_VALUE_TYPE_STRING;
    aliasPair = ZrCore_HashSet_Find(
            state, &loadedRegistry->nodeMap, &aliasKey);
    TEST_ASSERT_NOT_NULL(aliasPair);
    TEST_ASSERT_TRUE(aliasPair->key.isNative);
    aliasKeyObject = aliasPair->key.value.object;
    TEST_ASSERT_NOT_NULL(aliasKeyObject);
    TEST_ASSERT_TRUE(aliasKeyObject->isNative);

    aliasPair->key.isNative = ZR_FALSE;
    rejectedResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    aliasPair->key.isNative = ZR_TRUE;
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));

    aliasKeyObject->isNative = ZR_FALSE;
    rejectedResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    aliasKeyObject->isNative = ZR_TRUE;
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));

    overflowPair = *aliasPair;
    savedAliasNext = aliasPair->next;
    overflowPair.next = savedAliasNext;
    aliasPair->next = &overflowPair;
    rejectedResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    aliasPair->next = savedAliasNext;
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));

    savedAliasNext = aliasPair->next;
    aliasPair->next = aliasPair;
    rejectedResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    aliasPair->next = savedAliasNext;
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));

    reflectionPath = ZrCore_String_CreateFromNative(
            state, TEST_REFLECTION_MODULE_PATH);
    TEST_ASSERT_NOT_NULL(reflectionPath);
    TEST_ASSERT_NULL(ZrCore_Module_GetFromCache(state, reflectionPath));

    memset(&ownerCycleCaller, 0, sizeof(ownerCycleCaller));
    ownerCycleCaller.ownerFunction = &ownerCycleCaller;
    rejectedResultBase = reflection_import_invoke(
            state, &ownerCycleCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));

    memset(&ownerCyclePeer, 0, sizeof(ownerCyclePeer));
    ownerCycleCaller.ownerFunction = &ownerCyclePeer;
    ownerCyclePeer.ownerFunction = &ownerCycleCaller;
    rejectedResultBase = reflection_import_invoke(
            state, &ownerCycleCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));

    memset(&unknownCaller, 0, sizeof(unknownCaller));
    rejectedResultBase = reflection_import_invoke(
            state, &unknownCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));
    ZrCore_GlobalState_SetModuleLoadDiagnostic(state->global, "stale diagnostic");
    rejectedResultBase = reflection_import_invoke(
            state, ZR_NULL, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));
    TEST_ASSERT_NULL(ZrCore_GlobalState_GetModuleLoadDiagnostic(state->global));
    rejectedResultBase = reflection_import_invoke(
            state, &childCaller, "zr.reflection.extra", ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    ambiguousTarget = reflection_import_create_loaded_runtime_module(
            state, &firstFixture, firstMetadataFunction, "test.reflection.ambiguous");
    TEST_ASSERT_NOT_EQUAL(firstTarget, ambiguousTarget);
    rejectedResultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(rejectedResultBase)->type));
    TEST_ASSERT_NULL(ZrCore_Module_GetFromCache(state, reflectionPath));
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    destroy_reflection_test_state(state);
}

static void test_reflection_module_import_requires_registered_provider_role(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrState *state = create_reflection_test_state();
    SZrObjectModule *target;
    SZrFunction childCaller;
    TZrStackValuePointer resultBase;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(method_spec_generic_context_fixture_init(&fixture));
    target = reflection_import_create_loaded_runtime_module(
            state, &fixture, ZR_NULL, "test.reflection.provider.required");
    TEST_ASSERT_NOT_NULL(target);
    memset(&childCaller, 0, sizeof(childCaller));
    childCaller.ownerFunction =
            ZrCore_Module_GetMetadataRuntime(target)->metadataFunction;

    ZrCore_GlobalState_SetProviderModuleNameResolver(
            state->global, ZR_NULL, ZR_NULL);
    resultBase = reflection_import_invoke(
            state, &childCaller, TEST_REFLECTION_MODULE_PATH, ZR_TRUE);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(
            ZrCore_Stack_GetValue(resultBase)->type));
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    destroy_reflection_test_state(state);
}

#undef TEST_REFLECTION_MODULE_PATH

#endif
