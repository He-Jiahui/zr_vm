#include "unity.h"

#include "harness/module_fixture_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/module.h"

static SZrState *state;
static ZrTestsFixtureSource provider;

static TZrBool source_loader(SZrState *vm, TZrNativeString path, TZrNativeString hash, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(vm, path, hash, io, &provider, 1u);
}

void setUp(void) {
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    state->global->sourceLoader = source_loader;
    state->global->compileSource = ZrParser_Source_Compile;
    memset(&provider, 0, sizeof(provider));
    provider.path = "call_binding_provider";
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(state);
    state = ZR_NULL;
}

static SZrFunction *compile_consumer(const char *source) {
    return ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "call_binding_consumer.zr"));
}

static SZrFunctionCallSiteCacheEntry *module_binding(SZrFunction *function) {
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE &&
            entry->binding.contract.bindingKind == ZR_CALL_BINDING_DIRECT &&
            entry->binding.contract.moduleSignatureHash != function->moduleSignatureHash) return entry;
    }
    return ZR_NULL;
}

static void assert_module_call(const char *providerSource, const char *consumer, TZrInt64 expected) {
    SZrFunction *function;
    SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;
    provider.source = providerSource;
    function = compile_consumer(consumer);
    TEST_ASSERT_NOT_NULL(function);
    entry = module_binding(function);
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "Imported static call must publish a provider binding");
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, entry->binding.contract.targetMetadataToken);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_VM, entry->binding.target.targetKind);
    TEST_ASSERT_NOT_NULL(entry->binding.target.vm.function);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
}

static void test_source_module_function_is_token_bound(void) {
    assert_module_call("pub fn answer(): int { return 31; }",
            "var provider = import(\"call_binding_provider\"); return provider.answer();", 31);
}

static void test_source_module_static_type_method_is_token_bound(void) {
    assert_module_call("pub class Math { pub static fn answer(): int { return 37; } }",
            "var provider = import(\"call_binding_provider\"); return provider.Math.answer();", 37);
}

static void test_module_function_keeps_captured_module_state(void) {
    assert_module_call("var offset: int = 41; pub fn answer(): int { return offset; }",
            "var provider = import(\"call_binding_provider\"); return provider.answer();", 41);
}

static void test_binary_provider_keeps_function_and_type_contracts(void) {
    TZrChar path[512];
    TZrByte *bytes;
    SZrFunction *consumer;
    TZrInt64 result = 0;
    snprintf(path, sizeof(path), "%s/call_binding_provider.zro", ZR_VM_TESTS_BINARY_DIR);
    bytes = ZrTests_Fixture_BuildBinaryFile(state,
            "pub fn answer(): int { return 7; } pub class Math { pub static fn answer(): int { return 13; } }",
            path, ZR_FALSE, &provider.length);
    TEST_ASSERT_NOT_NULL(bytes);
    provider.bytes = bytes;
    provider.isBinary = ZR_TRUE;
    consumer = compile_consumer("var provider = import(\"call_binding_provider\"); "
            "return provider.answer() + provider.Math.answer();");
    TEST_ASSERT_NOT_NULL(consumer);
    TEST_ASSERT_NOT_NULL(module_binding(consumer));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, consumer, &result));
    TEST_ASSERT_EQUAL_INT64(20, result);
    free(bytes);
    remove(path);
}

static void test_provider_reload_rebinds_existing_calls(void) {
    SZrFunction *consumer;
    SZrFunctionCallSiteCacheEntry *entry;
    SZrFunction *oldFunction;
    SZrCallBinding staleBinding;
    SZrCallBindingDiagnostic diagnostic = {0};
    TZrInt64 result = 0;
    provider.source = "pub fn answer(): int { return 43; }";
    consumer = compile_consumer("var provider = import(\"call_binding_provider\"); return provider.answer();");
    TEST_ASSERT_NOT_NULL(consumer);
    entry = module_binding(consumer);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, consumer, &result));
    TEST_ASSERT_EQUAL_INT64(43, result);
    oldFunction = entry->binding.target.vm.function;
    staleBinding = entry->binding;
    ZrCore_Module_RemoveFromCache(state, ZrCore_String_CreateFromNative(state, "call_binding_provider"));
    TEST_ASSERT_NULL(ZrCore_Module_GetFromCache(state,
            ZrCore_String_CreateFromNative(state, "call_binding_provider")));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION,
            ZrCore_CallBinding_Validate(&staleBinding, consumer->callBindingGeneration, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, diagnostic.status);
    provider.source = "pub fn answer(): int { return 47; }";
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, consumer, &result));
    TEST_ASSERT_EQUAL_INT64(47, result);
    TEST_ASSERT_NOT_NULL(entry->binding.target.vm.function);
    TEST_ASSERT_TRUE(entry->binding.target.vm.function != oldFunction);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_source_module_function_is_token_bound);
    RUN_TEST(test_source_module_static_type_method_is_token_bound);
    RUN_TEST(test_module_function_keeps_captured_module_state);
    RUN_TEST(test_binary_provider_keeps_function_and_type_contracts);
    RUN_TEST(test_provider_reload_rebinds_existing_calls);
    return UNITY_END();
}
