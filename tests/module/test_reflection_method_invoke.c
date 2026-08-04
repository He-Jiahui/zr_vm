#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"

#define TEST_INVOKE_MEMBER_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define TEST_INVOKE_MEMBER_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)

void setUp(void) {}

void tearDown(void) {}

static TZrInt64 test_invoke_aot_entry(struct SZrState *state) {
    (void)state;
    return 0;
}

static TZrUInt32 test_invoke_call_count;
static EZrValueType test_invoke_return_type = ZR_VALUE_TYPE_ENUM_MAX;

static void reset_invoke_capture(void) {
    test_invoke_call_count = 0u;
    test_invoke_return_type = ZR_VALUE_TYPE_ENUM_MAX;
}

static void test_invoke_aot_invoker(struct SZrState *state,
                                    FZrAotEntryThunk target,
                                    const SZrAotMethodInfo *method,
                                    SZrTypeValue *self,
                                    SZrTypeValue *args,
                                    SZrTypeValue *outReturn) {
    (void)state;
    (void)target;
    (void)method;
    (void)self;
    (void)args;
    if (outReturn != ZR_NULL && test_invoke_return_type != ZR_VALUE_TYPE_ENUM_MAX) {
        outReturn->type = test_invoke_return_type;
    }
    test_invoke_call_count++;
}

static void set_method_records(SZrFunction *metadataFunction, SZrMetadataTokenRecord records[2]) {
    records[0].token = TEST_INVOKE_MEMBER_DEF_TOKEN;
    records[0].relatedToken = TEST_INVOKE_MEMBER_DEF_SIGNATURE_TOKEN;
    records[0].signatureHash = 0x1020304050607080ULL;
    records[1].token = TEST_INVOKE_MEMBER_DEF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_INVOKE_MEMBER_DEF_TOKEN;
    records[1].ownerToken = TEST_INVOKE_MEMBER_DEF_TOKEN;
    records[1].signatureHash = records[0].signatureHash;

    metadataFunction->metadataTokenRecords = records;
    metadataFunction->metadataTokenRecordLength = 2u;
}

static SZrMetadataRuntime *attach_method_runtime(SZrObjectModule *module,
                                                 SZrFunction *metadataFunction,
                                                 SZrAotCodeRegistration *registration,
                                                 SZrAotMethodInfo *methodInfo,
                                                 FZrAotEntryThunk functionPointers[1],
                                                 const SZrAotMethodInfo *methodInfos[1],
                                                 TZrUInt32 methodTokens[1]) {
    functionPointers[0] = test_invoke_aot_entry;
    methodInfo->functionIndex = 0u;
    methodInfo->invoker = test_invoke_aot_invoker;
    methodInfos[0] = methodInfo;
    methodTokens[0] = TEST_INVOKE_MEMBER_DEF_TOKEN;

    registration->functionCount = 1u;
    registration->functionPointers = functionPointers;
    registration->methodInfos = methodInfos;
    registration->methodInfoCount = 1u;
    registration->methodTokens = methodTokens;
    registration->methodTokenCount = 1u;

    return ZrCore_Module_AttachMetadataRuntime(module, metadataFunction, registration);
}

static void test_reflection_invoke_method_token_rejects_incomplete_signature_shape(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[1] = {0};
    SZrAotMethodInfo methodInfo = {0};
    const SZrAotMethodInfo *methodInfos[1] = {0};
    TZrUInt32 methodTokens[1] = {0};
    SZrAotSignature signature = {0};
    SZrAotSignatureType parameterType = {0};
    SZrAotSignatureType returnType = {0};
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue argumentValue = {0};
    SZrTypeValue returnValue = {0};

    set_method_records(&metadataFunction, records);
    runtime = attach_method_runtime(&module,
                                    &metadataFunction,
                                    &registration,
                                    &methodInfo,
                                    functionPointers,
                                    methodInfos,
                                    methodTokens);
    TEST_ASSERT_NOT_NULL(runtime);

    methodInfo.signature = &signature;
    signature.parameterCount = 1u;
    signature.parameterTypes = ZR_NULL;
    signature.hasReturnValue = 0u;

    reset_invoke_capture();
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                      ZR_NULL,
                                                                      &argumentValue,
                                                                      1u,
                                                                      &returnValue));
    TEST_ASSERT_EQUAL_UINT32(0u, test_invoke_call_count);

    parameterType.baseType = (TZrUInt16)ZR_VALUE_TYPE_INT64;
    parameterType.passingMode = (TZrUInt8)ZR_AOT_PARAMETER_PASSING_VALUE;
    signature.parameterTypes = &parameterType;
    argumentValue.type = ZR_VALUE_TYPE_INT64;
    reset_invoke_capture();
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                     runtime,
                                                                     TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                     ZR_NULL,
                                                                     &argumentValue,
                                                                     1u,
                                                                     &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);

    signature.parameterCount = 0u;
    signature.parameterTypes = ZR_NULL;
    signature.hasReturnValue = 1u;
    signature.returnType = ZR_NULL;

    reset_invoke_capture();
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                      ZR_NULL,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      &returnValue));
    TEST_ASSERT_EQUAL_UINT32(0u, test_invoke_call_count);

    returnType.baseType = (TZrUInt16)ZR_VALUE_TYPE_INT64;
    signature.returnType = &returnType;
    reset_invoke_capture();
    test_invoke_return_type = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                     runtime,
                                                                     TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                     ZR_NULL,
                                                                     ZR_NULL,
                                                                     0u,
                                                                     &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);
}

static void test_reflection_invoke_method_token_checks_fixed_parameter_base_types(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[1] = {0};
    SZrAotMethodInfo methodInfo = {0};
    const SZrAotMethodInfo *methodInfos[1] = {0};
    TZrUInt32 methodTokens[1] = {0};
    SZrAotSignature signature = {0};
    SZrAotSignatureType parameterTypes[2] = {{0}};
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue arguments[2] = {{0}};
    SZrTypeValue returnValue = {0};

    set_method_records(&metadataFunction, records);
    runtime = attach_method_runtime(&module,
                                    &metadataFunction,
                                    &registration,
                                    &methodInfo,
                                    functionPointers,
                                    methodInfos,
                                    methodTokens);
    TEST_ASSERT_NOT_NULL(runtime);

    parameterTypes[0].baseType = (TZrUInt16)ZR_VALUE_TYPE_INT64;
    parameterTypes[0].passingMode = (TZrUInt8)ZR_AOT_PARAMETER_PASSING_VALUE;
    parameterTypes[1].baseType = (TZrUInt16)ZR_VALUE_TYPE_BOOL;
    parameterTypes[1].passingMode = (TZrUInt8)ZR_AOT_PARAMETER_PASSING_VALUE;
    signature.parameterCount = 2u;
    signature.parameterTypes = parameterTypes;
    methodInfo.signature = &signature;

    arguments[0].type = ZR_VALUE_TYPE_INT64;
    arguments[1].type = ZR_VALUE_TYPE_INT64;
    reset_invoke_capture();
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                      ZR_NULL,
                                                                      arguments,
                                                                      2u,
                                                                      &returnValue));
    TEST_ASSERT_EQUAL_UINT32(0u, test_invoke_call_count);

    arguments[1].type = ZR_VALUE_TYPE_BOOL;
    reset_invoke_capture();
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                     runtime,
                                                                     TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                     ZR_NULL,
                                                                     arguments,
                                                                     2u,
                                                                     &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);
}

static void test_reflection_invoke_method_token_rejects_non_value_parameter_modes(void) {
    const EZrAotParameterPassingMode rejectedModes[] = {
            ZR_AOT_PARAMETER_PASSING_UNKNOWN,
            ZR_AOT_PARAMETER_PASSING_IN,
            ZR_AOT_PARAMETER_PASSING_REF,
            ZR_AOT_PARAMETER_PASSING_REF_READONLY,
            ZR_AOT_PARAMETER_PASSING_SCOPED_REF,
            ZR_AOT_PARAMETER_PASSING_SCOPED_REF_READONLY,
            ZR_AOT_PARAMETER_PASSING_OUT,
            (EZrAotParameterPassingMode)255,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[1] = {0};
    SZrAotMethodInfo methodInfo = {0};
    const SZrAotMethodInfo *methodInfos[1] = {0};
    TZrUInt32 methodTokens[1] = {0};
    SZrAotSignature signature = {0};
    SZrAotSignatureType parameterType = {0};
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue argument = {0};
    SZrTypeValue returnValue = {0};

    set_method_records(&metadataFunction, records);
    runtime = attach_method_runtime(&module,
                                    &metadataFunction,
                                    &registration,
                                    &methodInfo,
                                    functionPointers,
                                    methodInfos,
                                    methodTokens);
    TEST_ASSERT_NOT_NULL(runtime);

    reset_invoke_capture();
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodToken(
            state,
            runtime,
            TEST_INVOKE_MEMBER_DEF_TOKEN,
            ZR_NULL,
            &argument,
            &returnValue));
    TEST_ASSERT_EQUAL_UINT32(0u, test_invoke_call_count);

    parameterType.baseType = (TZrUInt16)ZR_VALUE_TYPE_INT64;
    signature.parameterCount = 1u;
    signature.parameterTypes = &parameterType;
    methodInfo.signature = &signature;
    argument.type = ZR_VALUE_TYPE_INT64;

    for (TZrUInt32 index = 0u;
         index < (TZrUInt32)(sizeof(rejectedModes) / sizeof(rejectedModes[0]));
         index++) {
        parameterType.passingMode = (TZrUInt8)rejectedModes[index];
        reset_invoke_capture();
        TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(
                state,
                runtime,
                TEST_INVOKE_MEMBER_DEF_TOKEN,
                ZR_NULL,
                &argument,
                1u,
                &returnValue));
        TEST_ASSERT_EQUAL_UINT32(0u, test_invoke_call_count);
        TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodToken(
                state,
                runtime,
                TEST_INVOKE_MEMBER_DEF_TOKEN,
                ZR_NULL,
                &argument,
                &returnValue));
        TEST_ASSERT_EQUAL_UINT32(0u, test_invoke_call_count);
    }

    parameterType.passingMode = (TZrUInt8)ZR_AOT_PARAMETER_PASSING_VALUE;
    reset_invoke_capture();
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(
            state,
            runtime,
            TEST_INVOKE_MEMBER_DEF_TOKEN,
            ZR_NULL,
            &argument,
            1u,
            &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);
    reset_invoke_capture();
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodToken(
            state,
            runtime,
            TEST_INVOKE_MEMBER_DEF_TOKEN,
            ZR_NULL,
            &argument,
            &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);
}

static void test_reflection_invoke_method_token_checks_return_base_type(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[1] = {0};
    SZrAotMethodInfo methodInfo = {0};
    const SZrAotMethodInfo *methodInfos[1] = {0};
    TZrUInt32 methodTokens[1] = {0};
    SZrAotSignature signature = {0};
    SZrAotSignatureType returnType = {0};
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue returnValue = {0};

    set_method_records(&metadataFunction, records);
    runtime = attach_method_runtime(&module,
                                    &metadataFunction,
                                    &registration,
                                    &methodInfo,
                                    functionPointers,
                                    methodInfos,
                                    methodTokens);
    TEST_ASSERT_NOT_NULL(runtime);

    returnType.baseType = (TZrUInt16)ZR_VALUE_TYPE_BOOL;
    signature.returnType = &returnType;
    signature.hasReturnValue = 1u;
    methodInfo.signature = &signature;

    reset_invoke_capture();
    test_invoke_return_type = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                      ZR_NULL,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);

    reset_invoke_capture();
    test_invoke_return_type = ZR_VALUE_TYPE_BOOL;
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                     runtime,
                                                                     TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                     ZR_NULL,
                                                                     ZR_NULL,
                                                                     0u,
                                                                     &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);
}

static void test_reflection_invoke_method_token_rejects_stale_return_slot_when_invoker_does_not_write(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[1] = {0};
    SZrAotMethodInfo methodInfo = {0};
    const SZrAotMethodInfo *methodInfos[1] = {0};
    TZrUInt32 methodTokens[1] = {0};
    SZrAotSignature signature = {0};
    SZrAotSignatureType returnType = {0};
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue returnValue = {0};

    set_method_records(&metadataFunction, records);
    runtime = attach_method_runtime(&module,
                                    &metadataFunction,
                                    &registration,
                                    &methodInfo,
                                    functionPointers,
                                    methodInfos,
                                    methodTokens);
    TEST_ASSERT_NOT_NULL(runtime);

    returnType.baseType = (TZrUInt16)ZR_VALUE_TYPE_BOOL;
    signature.returnType = &returnType;
    signature.hasReturnValue = 1u;
    methodInfo.signature = &signature;
    returnValue.type = ZR_VALUE_TYPE_BOOL;

    reset_invoke_capture();
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                      ZR_NULL,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_NULL, (TZrUInt16)returnValue.type);
}

static void test_reflection_invoke_method_token_clears_void_return_slot_after_dispatch(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[1] = {0};
    SZrAotMethodInfo methodInfo = {0};
    const SZrAotMethodInfo *methodInfos[1] = {0};
    TZrUInt32 methodTokens[1] = {0};
    SZrAotSignature signature = {0};
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue returnValue = {0};

    set_method_records(&metadataFunction, records);
    runtime = attach_method_runtime(&module,
                                    &metadataFunction,
                                    &registration,
                                    &methodInfo,
                                    functionPointers,
                                    methodInfos,
                                    methodTokens);
    TEST_ASSERT_NOT_NULL(runtime);

    signature.hasReturnValue = 0u;
    methodInfo.signature = &signature;
    returnValue.type = ZR_VALUE_TYPE_BOOL;

    reset_invoke_capture();
    test_invoke_return_type = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                     runtime,
                                                                     TEST_INVOKE_MEMBER_DEF_TOKEN,
                                                                     ZR_NULL,
                                                                     ZR_NULL,
                                                                     0u,
                                                                     &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_invoke_call_count);
    TEST_ASSERT_EQUAL_UINT16((TZrUInt16)ZR_VALUE_TYPE_NULL, (TZrUInt16)returnValue.type);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reflection_invoke_method_token_rejects_incomplete_signature_shape);
    RUN_TEST(test_reflection_invoke_method_token_checks_fixed_parameter_base_types);
    RUN_TEST(test_reflection_invoke_method_token_rejects_non_value_parameter_modes);
    RUN_TEST(test_reflection_invoke_method_token_checks_return_base_type);
    RUN_TEST(test_reflection_invoke_method_token_rejects_stale_return_slot_when_invoker_does_not_write);
    RUN_TEST(test_reflection_invoke_method_token_clears_void_return_slot_after_dispatch);
    return UNITY_END();
}
