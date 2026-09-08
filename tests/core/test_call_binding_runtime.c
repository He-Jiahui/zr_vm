#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

void setUp(void) {}
void tearDown(void) {}

static SZrCallBindingContract contract(void) {
    SZrCallBindingContract value = {0};
    value.bindingKind = ZR_CALL_BINDING_DIRECT;
    value.targetMetadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u);
    value.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u);
    value.signatureHash = 0x1020304050607080ULL;
    value.moduleSignatureHash = 0x9080706050403020ULL;
    value.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    return value;
}

static TZrInt64 native_entry(struct SZrState *state) { (void)state; return 13; }
static TZrInt64 aot_entry(struct SZrState *state) { (void)state; return 19; }
static void aot_invoke(struct SZrState *state, FZrAotEntryThunk target,
        const struct SZrAotMethodInfo *method, struct SZrTypeValue *self,
        struct SZrTypeValue *args, struct SZrTypeValue *result) {
    (void)state; (void)target; (void)method; (void)self; (void)args; (void)result;
}

static void test_resolve_vm_native_and_aot_targets(void) {
    SZrCallBindingContract expected = contract();
    SZrCallBindingCandidate candidate = {0};
    SZrCallBinding binding = {0};
    SZrFunction function = {0};
    SZrAotMethodInfo method = {0};

    candidate.contract = expected;
    candidate.generation = 1u;
    candidate.target.targetKind = ZR_CALL_BINDING_TARGET_VM;
    candidate.target.vm.function = &function;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_OK,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 1u, &binding, ZR_NULL));
    TEST_ASSERT_EQUAL_PTR(&function, binding.target.vm.function);
    candidate.target.targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    candidate.target.native.function = native_entry;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_OK,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 1u, &binding, ZR_NULL));
    TEST_ASSERT_EQUAL_INT64(13, binding.target.native.function(ZR_NULL));
    memset(&candidate.target, 0, sizeof(candidate.target));
    candidate.target.targetKind = ZR_CALL_BINDING_TARGET_AOT;
    candidate.target.aot.thunk = aot_entry;
    candidate.target.aot.methodInfo = &method;
    candidate.target.aot.invoker = aot_invoke;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_OK,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 1u, &binding, ZR_NULL));
    TEST_ASSERT_EQUAL_INT64(19, binding.target.aot.thunk(ZR_NULL));
}

static void test_contract_mismatch_and_reload_clear_resolved_target(void) {
    SZrCallBindingContract expected = contract();
    SZrCallBindingCandidate candidate = {0};
    SZrCallBinding binding = {0};
    SZrCallBindingDiagnostic diagnostic = {0};
    candidate.contract = expected;
    candidate.generation = 2u;
    candidate.target.targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    candidate.target.native.function = native_entry;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_OK,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 2u, &binding, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION,
            ZrCore_CallBinding_Validate(&binding, 3u, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_TARGET_NONE, binding.target.targetKind);
    TEST_ASSERT_EQUAL_UINT64(2u, diagnostic.expected);
    TEST_ASSERT_EQUAL_UINT64(3u, diagnostic.actual);
    candidate.contract.signatureHash++;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_SIGNATURE_MISMATCH,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 2u, &binding, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_TARGET_NONE, binding.target.targetKind);
    candidate.contract = expected;
    candidate.contract.moduleSignatureHash++;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_MODULE_MISMATCH,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 2u, &binding, &diagnostic));
}

static void test_missing_ambiguous_and_illegal_contracts_fail(void) {
    SZrCallBindingContract expected = contract();
    SZrCallBindingCandidate candidates[2] = {0};
    SZrCallBinding binding = {0};
    candidates[0].contract = expected;
    candidates[0].generation = 1u;
    candidates[0].target.targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    candidates[0].target.native.function = native_entry;
    candidates[1] = candidates[0];
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_TARGET_NOT_FOUND,
            ZrCore_CallBinding_Resolve(&expected, ZR_NULL, 0u, 1u, &binding, ZR_NULL));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_AMBIGUOUS_TARGET,
            ZrCore_CallBinding_Resolve(&expected, candidates, 2u, 1u, &binding, ZR_NULL));
    expected.signatureHash = 0u;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_MISSING_CONTRACT,
            ZrCore_CallBinding_Resolve(&expected, candidates, 1u, 1u, &binding, ZR_NULL));
    expected = contract();
    expected.signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_INVALID_TOKEN,
            ZrCore_CallBinding_CheckContract(&expected, ZR_NULL));
}

static void test_virtual_interface_and_typed_function_keep_signature_contract(void) {
    SZrCallBindingContract expected = contract();
    SZrCallBindingCandidate candidate = {0};
    SZrCallBinding binding = {0};
    expected.ownerTypeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    expected.layoutVersion = 1u;
    expected.layoutHash = 123u;
    expected.dispatchSlot = 3u;
    candidate.generation = 1u;
    candidate.target.targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    candidate.target.native.function = native_entry;
    candidate.target.dispatchSlotCount = 4u;
    for (TZrUInt32 kind = ZR_CALL_BINDING_VIRTUAL; kind <= ZR_CALL_BINDING_INTERFACE; ++kind) {
        expected.bindingKind = kind;
        candidate.contract = expected;
        TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_OK,
                ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 1u, &binding, ZR_NULL));
        candidate.contract.layoutHash++;
        TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_LAYOUT_MISMATCH,
                ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 1u, &binding, ZR_NULL));
    }
    candidate.contract = expected;
    candidate.target.dispatchSlotCount = 3u;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_INVALID_SLOT,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 1u, &binding, ZR_NULL));
    expected = contract();
    expected.bindingKind = ZR_CALL_BINDING_TYPED_FUNCTION;
    expected.targetMetadataToken = 0u;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_OK, ZrCore_CallBinding_CheckContract(&expected, ZR_NULL));
}

static void test_native_provider_generation_invalidates_resolved_closure(void) {
    SZrCallBindingContract expected = contract();
    SZrCallBindingCandidate candidate = {0};
    SZrCallBinding binding = {0};
    SZrClosureNative closure = {0};
    candidate.contract = expected;
    candidate.generation = 1u;
    candidate.target.targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    candidate.target.native.function = native_entry;
    candidate.target.targetGeneration = 1u;
    candidate.target.callableObject = (SZrRawObject *)&closure;
    closure.super.type = ZR_RAW_OBJECT_TYPE_CLOSURE;
    closure.super.isNative = ZR_TRUE;
    closure.callBindingGeneration = 1u;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_OK,
            ZrCore_CallBinding_Resolve(&expected, &candidate, 1u, 1u, &binding, ZR_NULL));
    ++closure.callBindingGeneration;
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION,
            ZrCore_CallBinding_Validate(&binding, 1u, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NONE, binding.target.targetKind);
}

static void test_generation_walk_visits_shared_constant_and_cycle_once(void) {
    SZrFunction root = {0}, child = {0}, method = {0};
    SZrTypeValue constants[2] = {0}, backEdge = {0};
    root.super.type = ZR_RAW_OBJECT_TYPE_FUNCTION;
    method.super.type = ZR_RAW_OBJECT_TYPE_FUNCTION;
    root.childFunctionLength = 1u;
    root.childFunctionList = &child;
    root.constantValueLength = 2u;
    root.constantValueList = constants;
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        constants[index].type = ZR_VALUE_TYPE_FUNCTION;
        constants[index].value.object = (SZrRawObject *)&method;
    }
    method.constantValueLength = 1u;
    method.constantValueList = &backEdge;
    backEdge.type = ZR_VALUE_TYPE_FUNCTION;
    backEdge.value.object = (SZrRawObject *)&root;
    root.callBindingGeneration = child.callBindingGeneration = method.callBindingGeneration = 1u;
    TEST_ASSERT_TRUE(ZrCore_CallBinding_AdvanceGeneration(&root));
    TEST_ASSERT_EQUAL_UINT64(2u, root.callBindingGeneration);
    TEST_ASSERT_EQUAL_UINT64(2u, child.callBindingGeneration);
    TEST_ASSERT_EQUAL_UINT64(2u, method.callBindingGeneration);
}

static TZrPtr site_allocator(TZrPtr context, TZrPtr pointer, TZrSize oldSize,
                            TZrSize newSize, TZrInt64 type) {
    (void)context; (void)oldSize; (void)type;
    if (newSize == 0u) {
        free(pointer);
        return ZR_NULL;
    }
    return realloc(pointer, newSize);
}

static TZrBool link_deferred_site(TZrUInt32 kind, EZrInstructionCode opcode,
        TZrUInt32 operation, TZrUInt16 operandCacheIndex, TZrMetadataToken ownerToken,
        SZrCallBindingDiagnostic *diagnostic) {
    SZrGlobalState global = {0};
    SZrState state = {0};
    SZrFunction function = {0};
    SZrFunctionCallSiteCacheEntry entry = {0};
    TZrInstruction instructions[2] = {0};
    TZrBool linked;
    global.allocator = site_allocator;
    state.global = &global;
    function.instructionsList = instructions;
    function.instructionsLength = 2u;
    function.callSiteCaches = &entry;
    function.callSiteCacheLength = 1u;
    entry.kind = kind;
    entry.instructionIndex = 1u;
    entry.memberEntryIndex = ZR_CALL_BINDING_SLOT_NONE;
    entry.binding.contract = contract();
    entry.binding.contract.operation = operation;
    entry.binding.contract.ownerTypeToken = ownerToken;
    if (ownerToken != 0u) {
        entry.binding.contract.layoutHash = 1u;
        entry.binding.contract.layoutVersion = ZR_CALL_BINDING_SCHEMA_VERSION;
    }
    entry.bindingLocation.kind = ZR_CALL_BINDING_RELOCATION_VM_MODULE;
    instructions[1].instruction.operationCode = (TZrUInt16)opcode;
    instructions[1].instruction.operand.operand1[1] = operandCacheIndex;
    linked = ZrCore_CallBinding_LinkFunction(&state, &function, diagnostic);
    free(function.callBindingInstructionMap);
    if (!linked) TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NONE, entry.binding.target.targetKind);
    return linked;
}

static void test_deferred_binding_rejects_operation_cache_and_instruction_mismatches(void) {
    const struct {
        TZrUInt32 kind;
        EZrInstructionCode opcode;
        TZrUInt32 operation;
        TZrUInt16 cacheIndex;
    } cases[] = {
        {ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL, ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
         ZR_CALL_BINDING_OPERATION_GET, 0u},
        {ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED),
         ZR_CALL_BINDING_OPERATION_CALL, 0u},
        {ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED),
         ZR_CALL_BINDING_OPERATION_SET, 0u},
        {ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET, ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
         ZR_CALL_BINDING_OPERATION_GET, 0u},
        {ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED),
         ZR_CALL_BINDING_OPERATION_GET, 1u},
        {ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE, ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
         ZR_CALL_BINDING_OPERATION_CALL, 0u}
    };
    SZrCallBindingDiagnostic diagnostic = {0};
    TEST_ASSERT_TRUE(link_deferred_site(ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL,
            ZR_INSTRUCTION_ENUM(FUNCTION_CALL), ZR_CALL_BINDING_OPERATION_CALL, 0u, 0u, &diagnostic));
    for (TZrSize index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        TEST_ASSERT_FALSE(link_deferred_site(cases[index].kind, cases[index].opcode,
                cases[index].operation, cases[index].cacheIndex, 0u, &diagnostic));
        TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_INVALID_RELOCATION, diagnostic.status);
        TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionIndex);
        TEST_ASSERT_EQUAL_UINT32(contract().targetMetadataToken, diagnostic.targetMetadataToken);
    }
}

static void test_imported_binding_owner_must_be_a_definition_token(void) {
    SZrCallBindingDiagnostic diagnostic = {0};
    TEST_ASSERT_FALSE(link_deferred_site(ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL,
            ZR_INSTRUCTION_ENUM(FUNCTION_CALL), ZR_CALL_BINDING_OPERATION_CALL, 0u,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u), &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_INVALID_RELOCATION, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT32(1u, diagnostic.instructionIndex);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_resolve_vm_native_and_aot_targets);
    RUN_TEST(test_contract_mismatch_and_reload_clear_resolved_target);
    RUN_TEST(test_missing_ambiguous_and_illegal_contracts_fail);
    RUN_TEST(test_virtual_interface_and_typed_function_keep_signature_contract);
    RUN_TEST(test_native_provider_generation_invalidates_resolved_closure);
    RUN_TEST(test_generation_walk_visits_shared_constant_and_cycle_once);
    RUN_TEST(test_deferred_binding_rejects_operation_cache_and_instruction_mismatches);
    RUN_TEST(test_imported_binding_owner_must_be_a_definition_token);
    return UNITY_END();
}
