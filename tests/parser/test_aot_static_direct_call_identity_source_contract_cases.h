#ifndef ZR_VM_TEST_AOT_STATIC_DIRECT_CALL_IDENTITY_SOURCE_CONTRACT_CASES_H
#define ZR_VM_TEST_AOT_STATIC_DIRECT_CALL_IDENTITY_SOURCE_CONTRACT_CASES_H

static void test_aot_static_direct_call_checks_frame_identity_before_preparation(void) {
    static const char *const helperNeedles[] = {
            "aot_runtime_static_direct_call_identity_matches(",
            "metadataFunction == ZR_NULL || calleeThunk == ZR_NULL",
            "frame->functionTable == ZR_NULL",
            "frame->functionThunks == ZR_NULL",
            "calleeFunctionIndex >= frame->functionCount",
            "calleeFunctionIndex >= frame->functionThunkCount",
            "frame->functionTable[calleeFunctionIndex] == metadataFunction",
            "frame->functionThunks[calleeFunctionIndex] == calleeThunk",
    };
    char *helperText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_internal.h");
    char *runtimeText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime.c");
    const char *functionStart;
    const char *directCallReset;
    const char *identityCheck;
    const char *framePreparation;

    TEST_ASSERT_NOT_NULL(helperText);
    TEST_ASSERT_NOT_NULL(runtimeText);
    assert_text_contains_all(helperText, helperNeedles, ARRAY_COUNT(helperNeedles));

    functionStart = strstr(
            runtimeText,
            "TZrBool ZrLibrary_AotRuntime_PrepareStaticDirectCall(");
    TEST_ASSERT_NOT_NULL(functionStart);
    directCallReset = strstr(
            functionStart,
            "memset(directCall, 0, sizeof(*directCall));");
    identityCheck = strstr(
            functionStart,
            "if (!aot_runtime_static_direct_call_identity_matches(");
    framePreparation = strstr(
            functionStart,
            "if (!aot_runtime_prepare_vm_direct_call_frame(");
    TEST_ASSERT_NOT_NULL(directCallReset);
    TEST_ASSERT_NOT_NULL(identityCheck);
    TEST_ASSERT_NOT_NULL(framePreparation);
    TEST_ASSERT_TRUE(directCallReset < identityCheck);
    TEST_ASSERT_TRUE(identityCheck < framePreparation);

    free(helperText);
    free(runtimeText);
}

static void test_aot_direct_core_checks_frame_identity_before_stack_growth(void) {
    char *runtimeText = read_repo_text_file_owned(
            "zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c");
    const char *staticStart;
    const char *inlineStart;
    const char *dynamicBridgeStart;
    const char *staticMetadata;
    const char *staticIdentity;
    const char *staticStackGrowth;
    const char *inlineMetadata;
    const char *inlineIdentity;
    const char *inlineStackGrowth;

    TEST_ASSERT_NOT_NULL(runtimeText);
    staticStart = strstr(runtimeText, "TZrBool ZrLibrary_AotRuntime_CallStaticDirect(");
    inlineStart = strstr(runtimeText, "TZrBool ZrLibrary_AotRuntime_CallInlineStruct(");
    dynamicBridgeStart = strstr(
            runtimeText,
            "TZrBool ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(");
    TEST_ASSERT_NOT_NULL(staticStart);
    TEST_ASSERT_NOT_NULL(inlineStart);
    TEST_ASSERT_NOT_NULL(dynamicBridgeStart);

    staticMetadata = strstr(
            staticStart,
            "metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);");
    staticIdentity = strstr(
            staticStart,
            "if (!aot_runtime_static_direct_call_identity_matches(");
    staticStackGrowth = strstr(staticStart, "ZrCore_Function_CheckStackAndGc(");
    TEST_ASSERT_NOT_NULL(staticMetadata);
    TEST_ASSERT_NOT_NULL(staticIdentity);
    TEST_ASSERT_NOT_NULL(staticStackGrowth);
    TEST_ASSERT_TRUE(staticMetadata < staticIdentity);
    TEST_ASSERT_TRUE(staticIdentity < staticStackGrowth);
    TEST_ASSERT_TRUE(staticIdentity < inlineStart);

    inlineMetadata = strstr(
            inlineStart,
            "metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);");
    inlineIdentity = strstr(
            inlineStart,
            "if (!aot_runtime_static_direct_call_identity_matches(");
    inlineStackGrowth = strstr(inlineStart, "ZrCore_Function_CheckStackAndGc(");
    TEST_ASSERT_NOT_NULL(inlineMetadata);
    TEST_ASSERT_NOT_NULL(inlineIdentity);
    TEST_ASSERT_NOT_NULL(inlineStackGrowth);
    TEST_ASSERT_TRUE(inlineMetadata < inlineIdentity);
    TEST_ASSERT_TRUE(inlineIdentity < inlineStackGrowth);
    TEST_ASSERT_TRUE(inlineIdentity < dynamicBridgeStart);
    TEST_ASSERT_NOT_NULL(strstr(
            staticIdentity,
            "generated AOT direct-core call identity drift"));

    free(runtimeText);
}

#endif
