#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_library/aot_runtime.h"

#define TEST_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u)
#define TEST_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_RESOLVED_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u)
#define TEST_RESOLVED_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 8u)
#define TEST_RESOLVED_SIGNATURE_HASH ((TZrUInt64)0x1112131415161718ULL)
#define TEST_MODULE_SIGNATURE_HASH ((TZrUInt64)0x2122232425262728ULL)

void setUp(void) {}
void tearDown(void) {}

static SZrMetadataTokenBinding make_matching_binding(void) {
    SZrMetadataTokenBinding binding = {0};

    binding.refToken = TEST_REF_TOKEN;
    binding.refSignatureToken = TEST_REF_SIGNATURE_TOKEN;
    binding.refSignatureHash = 0x0102030405060708ULL;
    binding.expectedMetadataToken = TEST_RESOLVED_TOKEN;
    binding.expectedSignatureToken = TEST_RESOLVED_SIGNATURE_TOKEN;
    binding.expectedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH;
    binding.expectedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    binding.resolvedMetadataToken = TEST_RESOLVED_TOKEN;
    binding.resolvedSignatureToken = TEST_RESOLVED_SIGNATURE_TOKEN;
    binding.resolvedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH;
    binding.resolvedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH;
    return binding;
}

static void attach_single_binding(SZrFunction *function,
                                  SZrMetadataTokenBinding *binding,
                                  SZrMetadataTokenRecord *record) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_NULL(record);

    record->token = binding->refToken;
    function->moduleMetadataBindings = binding;
    function->moduleMetadataBindingLength = 1u;
    function->metadataTokenRecords = record;
    function->metadataTokenRecordLength = 1u;
}

static ZrAotGeneratedFrame make_frame(SZrFunction *caller, SZrFunction **functionTable, TZrUInt32 functionCount) {
    ZrAotGeneratedFrame frame = {0};

    frame.function = caller;
    frame.functionTable = functionTable;
    frame.functionCount = functionCount;
    return frame;
}

static void test_typed_direct_call_guard_accepts_empty_caller_and_callee_bindings(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction caller = {0};
    SZrFunction callee = {0};
    SZrFunction *functionTable[2];
    ZrAotGeneratedFrame frame;

    TEST_ASSERT_NOT_NULL(state);
    functionTable[0] = &caller;
    functionTable[1] = &callee;
    frame = make_frame(&caller, functionTable, 2u);

    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, 1u));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_direct_call_guard_deopts_on_caller_binding_drift(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction caller = {0};
    SZrFunction callee = {0};
    SZrFunction *functionTable[2];
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataTokenRecord record = {0};
    ZrAotGeneratedFrame frame;

    TEST_ASSERT_NOT_NULL(state);
    binding.resolvedSignatureHash = TEST_RESOLVED_SIGNATURE_HASH + 1u;
    attach_single_binding(&caller, &binding, &record);
    functionTable[0] = &caller;
    functionTable[1] = &callee;
    frame = make_frame(&caller, functionTable, 2u);

    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, 1u));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_typed_direct_call_guard_deopts_on_callee_binding_drift(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction caller = {0};
    SZrFunction callee = {0};
    SZrFunction *functionTable[2];
    SZrMetadataTokenBinding binding = make_matching_binding();
    SZrMetadataTokenRecord record = {0};
    ZrAotGeneratedFrame frame;

    TEST_ASSERT_NOT_NULL(state);
    binding.resolvedModuleSignatureHash = TEST_MODULE_SIGNATURE_HASH + 1u;
    attach_single_binding(&callee, &binding, &record);
    functionTable[0] = &caller;
    functionTable[1] = &callee;
    frame = make_frame(&caller, functionTable, 2u);

    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, 1u));

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_direct_call_guard_accepts_empty_caller_and_callee_bindings);
    RUN_TEST(test_typed_direct_call_guard_deopts_on_caller_binding_drift);
    RUN_TEST(test_typed_direct_call_guard_deopts_on_callee_binding_drift);
    return UNITY_END();
}
