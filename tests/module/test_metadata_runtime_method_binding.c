#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"

#define TEST_MEMBER_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define TEST_MEMBER_DEF_TOKEN_2 ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u)
#define TEST_UNKNOWN_MEMBER_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 99u)
#define TEST_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)

void setUp(void) {}

void tearDown(void) {}

static TZrInt64 test_aot_entry_one(struct SZrState *state) {
    (void)state;
    return 1;
}

static TZrInt64 test_aot_entry_two(struct SZrState *state) {
    (void)state;
    return 2;
}

static void test_aot_invoker(struct SZrState *state,
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
    (void)outReturn;
}

static void poison_method_binding_view(SZrMetadataRuntimeMethodBindingView *view) {
    view->methodToken = TEST_MEMBER_DEF_TOKEN;
    view->functionIndex = 77u;
    view->methodInfo = (const SZrAotMethodInfo *)view;
    view->functionPointer = test_aot_entry_one;
    view->invoker = test_aot_invoker;
}

static void assert_method_binding_view_cleared(const SZrMetadataRuntimeMethodBindingView *view) {
    TEST_ASSERT_EQUAL_UINT32(0u, view->methodToken);
    TEST_ASSERT_EQUAL_UINT32(0u, view->functionIndex);
    TEST_ASSERT_NULL(view->methodInfo);
    TEST_ASSERT_NULL(view->functionPointer);
    TEST_ASSERT_NULL(view->invoker);
}

static void test_method_binding_view_resolves_method_token_to_registered_aot_entry(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    FZrAotEntryThunk functionPointers[3] = {
            test_aot_entry_one,
            test_aot_entry_two,
            test_aot_entry_one,
    };
    SZrAotMethodInfo methodInfo0 = {0};
    SZrAotMethodInfo methodInfo1 = {0};
    SZrAotMethodInfo methodInfo2 = {0};
    const SZrAotMethodInfo *methodInfos[3] = {
            &methodInfo0,
            &methodInfo1,
            &methodInfo2,
    };
    TZrUInt32 methodTokens[3] = {
            0u,
            TEST_MEMBER_DEF_TOKEN,
            TEST_MEMBER_DEF_TOKEN_2,
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeMethodBindingView view = {0};

    methodInfo0.functionIndex = 0u;
    methodInfo0.invoker = test_aot_invoker;
    methodInfo1.functionIndex = 1u;
    methodInfo1.invoker = test_aot_invoker;
    methodInfo2.functionIndex = 2u;
    methodInfo2.invoker = test_aot_invoker;

    registration.functionCount = 3u;
    registration.functionPointers = functionPointers;
    registration.methodInfos = methodInfos;
    registration.methodInfoCount = 3u;
    registration.methodTokens = methodTokens;
    registration.methodTokenCount = 3u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadMethodBindingView(runtime,
                                                                  TEST_MEMBER_DEF_TOKEN,
                                                                  &view));
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, view.methodToken);
    TEST_ASSERT_EQUAL_UINT32(1u, view.functionIndex);
    TEST_ASSERT_EQUAL_PTR(&methodInfo1, view.methodInfo);
    TEST_ASSERT_TRUE(view.functionPointer == test_aot_entry_two);
    TEST_ASSERT_TRUE(view.invoker == test_aot_invoker);

    poison_method_binding_view(&view);
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(runtime, 0u, &view));
    assert_method_binding_view_cleared(&view);

    poison_method_binding_view(&view);
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(runtime, TEST_TYPE_DEF_TOKEN, &view));
    assert_method_binding_view_cleared(&view);

    poison_method_binding_view(&view);
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(runtime,
                                                                   TEST_UNKNOWN_MEMBER_DEF_TOKEN,
                                                                   &view));
    assert_method_binding_view_cleared(&view);

    poison_method_binding_view(&view);
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(ZR_NULL, TEST_MEMBER_DEF_TOKEN, &view));
    assert_method_binding_view_cleared(&view);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(runtime, TEST_MEMBER_DEF_TOKEN, ZR_NULL));
}

static void test_method_binding_view_rejects_incomplete_or_ambiguous_registration(void) {
    SZrObjectModule module = {0};
    SZrObjectModule duplicateModule = {0};
    SZrObjectModule staleModule = {0};
    SZrFunction metadataFunction = {0};
    FZrAotEntryThunk functionPointers[2] = {
            test_aot_entry_one,
            test_aot_entry_two,
    };
    SZrAotMethodInfo methodInfo0 = {0};
    SZrAotMethodInfo methodInfo1 = {0};
    const SZrAotMethodInfo *methodInfos[2] = {
            &methodInfo0,
            &methodInfo1,
    };
    TZrUInt32 methodTokens[2] = {
            TEST_MEMBER_DEF_TOKEN,
            TEST_MEMBER_DEF_TOKEN_2,
    };
    TZrUInt32 duplicateMethodTokens[2] = {
            TEST_MEMBER_DEF_TOKEN,
            TEST_MEMBER_DEF_TOKEN,
    };
    SZrAotCodeRegistration missingTokenTableRegistration = {0};
    SZrAotCodeRegistration duplicateRegistration = {0};
    SZrAotCodeRegistration staleRegistration = {0};
    SZrMetadataRuntimeMethodBindingView view = {0};

    methodInfo0.functionIndex = 0u;
    methodInfo0.invoker = test_aot_invoker;
    methodInfo1.functionIndex = 1u;
    methodInfo1.invoker = test_aot_invoker;

    missingTokenTableRegistration.functionCount = 2u;
    missingTokenTableRegistration.functionPointers = functionPointers;
    missingTokenTableRegistration.methodInfos = methodInfos;
    missingTokenTableRegistration.methodInfoCount = 2u;
    missingTokenTableRegistration.methodTokenCount = 2u;

    TEST_ASSERT_NOT_NULL(
            ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &missingTokenTableRegistration));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(ZrCore_Module_GetMetadataRuntime(&module),
                                                                   TEST_MEMBER_DEF_TOKEN,
                                                                   &view));
    assert_method_binding_view_cleared(&view);

    duplicateRegistration.functionCount = 2u;
    duplicateRegistration.functionPointers = functionPointers;
    duplicateRegistration.methodInfos = methodInfos;
    duplicateRegistration.methodInfoCount = 2u;
    duplicateRegistration.methodTokens = duplicateMethodTokens;
    duplicateRegistration.methodTokenCount = 2u;

    TEST_ASSERT_NOT_NULL(
            ZrCore_Module_AttachMetadataRuntime(&duplicateModule, &metadataFunction, &duplicateRegistration));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(
            ZrCore_Module_GetMetadataRuntime(&duplicateModule),
            TEST_MEMBER_DEF_TOKEN,
            &view));
    assert_method_binding_view_cleared(&view);

    methodInfo1.functionIndex = 0u;
    staleRegistration.functionCount = 2u;
    staleRegistration.functionPointers = functionPointers;
    staleRegistration.methodInfos = methodInfos;
    staleRegistration.methodInfoCount = 2u;
    staleRegistration.methodTokens = methodTokens;
    staleRegistration.methodTokenCount = 2u;

    TEST_ASSERT_NOT_NULL(
            ZrCore_Module_AttachMetadataRuntime(&staleModule, &metadataFunction, &staleRegistration));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodBindingView(
            ZrCore_Module_GetMetadataRuntime(&staleModule),
            TEST_MEMBER_DEF_TOKEN_2,
            &view));
    assert_method_binding_view_cleared(&view);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_method_binding_view_resolves_method_token_to_registered_aot_entry);
    RUN_TEST(test_method_binding_view_rejects_incomplete_or_ambiguous_registration);
    return UNITY_END();
}
