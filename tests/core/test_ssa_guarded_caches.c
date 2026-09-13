#include "unity.h"

#include <string.h>

#include "zr_vm_core/execution_binding_guard.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"

void setUp(void) {}
void tearDown(void) {}

static TZrInt64 guard_native(struct SZrState *state) {
    (void)state;
    return 7;
}

static SZrCallBindingContract guard_contract(void) {
    SZrCallBindingContract contract = {0};
    contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    contract.targetMetadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    contract.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u);
    contract.signatureHash = 0x11u;
    contract.moduleSignatureHash = 0x22u;
    contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    return contract;
}

static SZrCallBinding binding_with_native(void) {
    SZrCallBinding binding = {0};
    binding.contract = guard_contract();
    binding.generation = 4u;
    binding.target.targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    binding.target.native.function = guard_native;
    return binding;
}

static void test_guard_accepts_resolved_native_and_counts_hit(void) {
    SZrCallBinding binding = binding_with_native();
    SZrFunctionCallSiteCacheEntry entry = {0};
    SZrExecutionBindingGuardInput input = {0};
    SZrExecutionBindingGuardDiagnostic diagnostic = {0};
    input.binding = &binding;
    input.cacheEntry = &entry;
    input.activeGeneration = 4u;
    input.expectedSignatureHash = 0x11u;
    input.expectedModuleSignatureHash = 0x22u;
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BINDING_GUARD_OK,
            ZrCore_Execution_CheckBindingGuard(&input, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(1u, entry.runtimeHitCount);
    TEST_ASSERT_EQUAL_STRING("ok", ZrCore_Execution_BindingGuardResultName(diagnostic.result));
}

static void test_guard_separates_stale_and_signature_failures(void) {
    SZrCallBinding binding = binding_with_native();
    SZrExecutionBindingGuardInput input = {0};
    SZrExecutionBindingGuardDiagnostic diagnostic = {0};
    input.binding = &binding;
    input.activeGeneration = 5u;
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BINDING_GUARD_STALE_GENERATION,
            ZrCore_Execution_CheckBindingGuard(&input, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_CALL_BINDING_STALE_GENERATION, diagnostic.bindingStatus);
    input.activeGeneration = 4u;
    input.expectedSignatureHash = 0x33u;
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BINDING_GUARD_CONTRACT_MISMATCH,
            ZrCore_Execution_CheckBindingGuard(&input, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_CALL_BINDING_SIGNATURE_MISMATCH, diagnostic.bindingStatus);
}

static void test_shape_miss_can_fall_back_to_declared_slot(void) {
    SZrCallBinding binding = binding_with_native();
    SZrObjectPrototype receiver = {0};
    SZrExecutionBindingGuardInput input = {0};
    SZrExecutionBindingGuardDiagnostic diagnostic = {0};
    binding.contract.bindingKind = ZR_CALL_BINDING_VIRTUAL;
    binding.contract.ownerTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    binding.contract.layoutVersion = 1u;
    binding.contract.layoutHash = 0x44u;
    binding.contract.dispatchSlot = 2u;
    receiver.shapeId = 9u;
    receiver.shapeGeneration = 3u;
    receiver.nextVirtualSlotIndex = 4u;
    input.binding = &binding;
    input.activeGeneration = 4u;
    input.receiverPrototype = &receiver;
    input.receiverShapeId = 8u;
    input.receiverShapeGeneration = 3u;
    input.allowSlotFallback = ZR_TRUE;
    TEST_ASSERT_EQUAL(ZR_EXECUTION_BINDING_GUARD_SLOT_FALLBACK,
            ZrCore_Execution_CheckBindingGuard(&input, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(2u, diagnostic.dispatchSlot);
}

static void test_reset_preserves_contract_and_relocation(void) {
    SZrFunctionCallSiteCacheEntry entry = {0};
    entry.binding.contract = guard_contract();
    entry.kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL;
    entry.instructionIndex = 6u;
    entry.bindingLocation.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
    entry.bindingLocation.targetIndex = 3u;
    entry.runtimeHitCount = 9u;
    entry.runtimeMissCount = 2u;
    entry.binding.target.targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    entry.binding.target.native.function = guard_native;
    ZrCore_Execution_ResetBindingCache(&entry);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_DIRECT, entry.binding.contract.bindingKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL, entry.kind);
    TEST_ASSERT_EQUAL_UINT32(6u, entry.instructionIndex);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_RELOCATION_MODULE, entry.bindingLocation.kind);
    TEST_ASSERT_EQUAL_UINT32(3u, entry.bindingLocation.targetIndex);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NONE, entry.binding.target.targetKind);
    TEST_ASSERT_EQUAL_UINT32(0u, entry.runtimeHitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, entry.runtimeMissCount);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_guard_accepts_resolved_native_and_counts_hit);
    RUN_TEST(test_guard_separates_stale_and_signature_failures);
    RUN_TEST(test_shape_miss_can_fall_back_to_declared_slot);
    RUN_TEST(test_reset_preserves_contract_and_relocation);
    return UNITY_END();
}
