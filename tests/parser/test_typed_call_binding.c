#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/module_fixture_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/typed_call_binding.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/module.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

static SZrState *state;

void setUp(void) { state = ZrTests_Runtime_State_Create(ZR_NULL); TEST_ASSERT_NOT_NULL(state); }
void tearDown(void) { ZrTests_Runtime_State_Destroy(state); state = ZR_NULL; }

static SZrFunction *compile_source(const char *source) {
    return ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "typed_call_binding.zr"));
}

static SZrFunctionCallSiteCacheEntry *find_typed(SZrFunction *function, SZrFunction **owner) {
    for (TZrUInt32 index = 0u; index < function->callSiteCacheLength; ++index) {
        if (function->callSiteCaches[index].binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
            if (owner != ZR_NULL) *owner = function;
            return &function->callSiteCaches[index];
        }
    }
    for (TZrUInt32 index = 0u; index < function->childFunctionLength; ++index) {
        SZrFunctionCallSiteCacheEntry *entry = find_typed(&function->childFunctionList[index], owner);
        if (entry != ZR_NULL) return entry;
    }
    return ZR_NULL;
}

static const char *callback_source =
        "fn add(value: int): int { return value + 4; } "
        "fn apply(callback: fn(int)->int, value: int): int { return callback(value); } "
        "return apply(add, 3);";

static const char *native_import_callback_source =
        "var provider = import(\"typed_callback_native\"); "
        "fn apply(callback: fn(int)->int, value: int): int { return callback(value); } "
        "return apply(provider.add, 2);";

static TZrBool native_add(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, 0u);
    if (value == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) return ZR_FALSE;
    ZrLib_Value_SetInt(context->state, result, value->value.nativeObject.nativeInt64 + 5);
    return ZR_TRUE;
}

static const ZrLibParameterDescriptor native_parameters[] = {
    {.name = "value", .typeName = "int", .passingMode = ZR_LIB_PARAMETER_PASSING_MODE_VALUE}
};
static const ZrLibFunctionDescriptor native_functions[] = {
    {.name = "add", .minArgumentCount = 1u, .maxArgumentCount = 1u, .callback = native_add,
     .returnTypeName = "int", .parameters = native_parameters, .parameterCount = 1u}
};
static const ZrLibModuleDescriptor native_module = {
    .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION, .moduleName = "typed_callback_native",
    .functions = native_functions, .functionCount = 1u, .moduleVersion = "1.0.0",
    .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION
};

static TZrInt64 aot_thunk(SZrState *aotState) { ZR_UNUSED_PARAMETER(aotState); return 1; }
static TZrInt64 other_aot_thunk(SZrState *aotState) { ZR_UNUSED_PARAMETER(aotState); return 2; }
static void aot_invoker(SZrState *aotState, FZrAotEntryThunk target, const SZrAotMethodInfo *method,
                       SZrTypeValue *self, SZrTypeValue *args, SZrTypeValue *outReturn) {
    ZR_UNUSED_PARAMETER(aotState); ZR_UNUSED_PARAMETER(target); ZR_UNUSED_PARAMETER(method);
    ZR_UNUSED_PARAMETER(self); ZR_UNUSED_PARAMETER(args); ZR_UNUSED_PARAMETER(outReturn);
}

static void assert_typed_result(const char *source, TZrInt64 expected) {
    SZrFunction *function = compile_source(source);
    SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    entry = find_typed(function, ZR_NULL);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->binding.contract.targetMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_RELOCATION_NONE, entry->bindingLocation.kind);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, entry->binding.contract.signatureToken);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(expected, result);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, entry->runtimeHitCount);
}

static void test_typed_parameter_is_signature_bound(void) { assert_typed_result(callback_source, 7); }

static void test_typed_native_value_uses_structural_signature(void) {
    SZrFunction *function;
    SZrFunction *owner = ZR_NULL;
    SZrFunctionCallSiteCacheEntry *entry;
    SZrObjectModule *module;
    const SZrTypeValue *nativeValue;
    SZrTypeValue callable;
    SZrTypeValue arguments[2];
    SZrTypeValue result;
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &native_module));
    function = compile_source(callback_source);
    TEST_ASSERT_NOT_NULL(function);
    entry = find_typed(function, &owner);
    TEST_ASSERT_NOT_NULL(entry);
    module = ZrCore_Module_ImportByPath(state, ZrCore_String_CreateFromNative(state, "typed_callback_native"));
    TEST_ASSERT_NOT_NULL(module);
    nativeValue = ZrCore_Module_GetPubExport(state, module, ZrCore_String_CreateFromNative(state, "add"));
    TEST_ASSERT_NOT_NULL(nativeValue);
    ZrCore_Value_InitAsRawObject(state, &callable, ZR_CAST_RAW_OBJECT_AS_SUPER(owner));
    arguments[0] = *nativeValue;
    ZrLib_Value_SetInt(state, &arguments[1], 2);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrLib_CallValue(state, &callable, ZR_NULL, arguments, 2u, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(7, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NATIVE, entry->binding.target.targetKind);
    TEST_ASSERT_EQUAL_PTR(nativeValue->value.object, entry->binding.target.callableObject);
}

static void test_typed_native_import_member_is_a_source_callable_value(void) {
    SZrFunction *function;
    SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;

    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &native_module));
    function = compile_source(native_import_callback_source);
    TEST_ASSERT_NOT_NULL(function);
    entry = find_typed(function, ZR_NULL);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->binding.contract.targetMetadataToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, entry->binding.contract.signatureToken);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
}

static void test_typed_zero_argument_value_is_guarded(void) {
    assert_typed_result("fn answer(): int { return 17; } "
            "fn apply(callback: fn()->int): int { return callback(); } return apply(answer);", 17);
}

static void test_typed_aot_value_keeps_live_closure_and_checks_registration(void) {
    SZrFunction *function = compile_source(callback_source);
    SZrFunction *owner = ZR_NULL;
    SZrFunctionCallSiteCacheEntry *entry;
    SZrClosureNative *closure;
    SZrTypeValue callable;
    SZrCallBindingDiagnostic diagnostic;
    SZrAotMethodInfo method = {.functionIndex = 0u, .invoker = aot_invoker};
    const SZrAotMethodInfo *methods[] = {&method};
    FZrAotEntryThunk thunks[] = {aot_thunk};
    SZrAotCodeRegistration registration = {.functionCount = 1u, .methodInfoCount = 1u,
            .functionPointers = thunks, .methodInfos = methods};
    TEST_ASSERT_NOT_NULL(function);
    entry = find_typed(function, &owner);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function->childFunctionLength);
    closure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closure);
    closure->aotShimFunction = &function->childFunctionList[0];
    closure->aotShimFunction->metadataCodeRegistration = &registration;
    closure->nativeFunction = aot_thunk;
    ZrCore_Value_InitAsRawObject(state, &callable, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    TEST_ASSERT_TRUE(ZrCore_CallBinding_PrepareTypedCall(state, owner, entry, &callable, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_AOT, entry->binding.target.targetKind);
    TEST_ASSERT_EQUAL_PTR(closure, callable.value.object);
    TEST_ASSERT_EQUAL_PTR(&method, entry->binding.target.aot.methodInfo);
    closure->nativeFunction = other_aot_thunk;
    TEST_ASSERT_FALSE(ZrCore_CallBinding_PrepareTypedCall(state, owner, entry, &callable, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_TARGET_NOT_FOUND, diagnostic.status);
    closure->aotShimFunction->metadataCodeRegistration = ZR_NULL;
}

static void test_repeated_captured_values_keep_their_context(void) {
    assert_typed_result(
            "fn make(offset: int): fn(int)->int { return fn(value: int): int => value + offset; } "
            "fn apply(callback: fn(int)->int, value: int): int { return callback(value); } "
            "var first = make(10); var second = make(20); return apply(first, 1) + apply(second, 2);", 33);
}

static void test_captured_callback_retains_its_typed_contract(void) {
    assert_typed_result("fn add(value: int): int { return value + 1; } "
            "fn invoke(callback: fn(int)->int): int { "
            "var thunk = fn(): int => callback(8); return thunk(); } return invoke(add);", 9);
}

static void test_typed_mismatched_callable_is_rejected(void) {
    SZrFunction *function = compile_source(callback_source);
    SZrFunctionCallSiteCacheEntry *entry;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    entry = find_typed(function, ZR_NULL);
    TEST_ASSERT_NOT_NULL(entry);
    entry->binding.contract.signatureHash ^= 1u;
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_SIGNATURE_MISMATCH, state->lastCallBindingError.status);
}

static void test_typed_stale_generation_is_rejected(void) {
    SZrFunction *function = compile_source(callback_source);
    SZrFunction *owner = ZR_NULL;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(find_typed(function, &owner));
    ++owner->callBindingGeneration;
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_STALE_GENERATION, state->lastCallBindingError.status);
}

static void test_typed_signature_record_tampering_is_rejected(void) {
    SZrFunction *function = compile_source(callback_source);
    SZrFunction *owner = ZR_NULL;
    SZrFunctionCallSiteCacheEntry *entry;
    SZrCallBindingDiagnostic diagnostic;
    TEST_ASSERT_NOT_NULL(function);
    entry = find_typed(function, &owner);
    TEST_ASSERT_NOT_NULL(entry);
    for (TZrUInt32 index = 0u; index < owner->metadataTokenRecordLength; ++index) {
        if (owner->metadataTokenRecords[index].token == entry->binding.contract.signatureToken)
            owner->metadataTokenRecords[index].signatureHash ^= 1u;
    }
    TEST_ASSERT_FALSE(ZrCore_CallBinding_LinkFunction(state, function, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_SIGNATURE_MISMATCH, diagnostic.status);
}

static void close_reader(SZrState *readerState, TZrPtr data) {
    ZR_UNUSED_PARAMETER(readerState); ZR_UNUSED_PARAMETER(data);
}

static void test_typed_binary_roundtrip_retains_signature_without_target(void) {
    SZrFunction *compiled = compile_source(callback_source);
    SZrFunction *loaded;
    SZrFunctionCallSiteCacheEntry *before;
    SZrFunctionCallSiteCacheEntry *after;
    SZrIo *io;
    SZrIoSource *artifact;
    ZrTestsFixtureReader reader = {0};
    TZrChar path[512];
    TZrByte *bytes;
    TZrSize length = 0u;
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(compiled);
    before = find_typed(compiled, ZR_NULL);
    TEST_ASSERT_NOT_NULL(before);
    snprintf(path, sizeof(path), "%s/typed_call_binding.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, compiled, path));
    bytes = ZrTests_Fixture_ReadFileBytes(path, &length);
    TEST_ASSERT_NOT_NULL(bytes);
    reader.bytes = bytes; reader.length = length;
    io = ZrCore_Io_New(state->global);
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, close_reader, &reader);
    io->isBinary = ZR_TRUE;
    artifact = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(artifact);
    loaded = ZrCore_Io_LoadEntryFunctionToRuntime(state, artifact);
    TEST_ASSERT_NOT_NULL(loaded);
    after = find_typed(loaded, ZR_NULL);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL_MEMORY(&before->binding.contract, &after->binding.contract, sizeof(before->binding.contract));
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NONE, after->binding.target.targetKind);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, loaded, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
    ZrCore_Io_ReadSourceFree(state->global, artifact);
    ZrCore_Io_Free(state->global, io);
    free(bytes);
    remove(path);
}

static void test_typed_native_import_member_binary_roundtrip(void) {
    SZrFunction *compiled;
    SZrFunction *loaded;
    SZrFunctionCallSiteCacheEntry *before;
    SZrFunctionCallSiteCacheEntry *after;
    SZrIo *io;
    SZrIoSource *artifact;
    ZrTestsFixtureReader reader = {0};
    TZrChar path[512];
    TZrByte *bytes;
    TZrSize length = 0u;
    TZrInt64 result = 0;

    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &native_module));
    compiled = compile_source(native_import_callback_source);
    TEST_ASSERT_NOT_NULL(compiled);
    before = find_typed(compiled, ZR_NULL);
    TEST_ASSERT_NOT_NULL(before);
    snprintf(path, sizeof(path), "%s/typed_call_binding_native.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, compiled, path));
    bytes = ZrTests_Fixture_ReadFileBytes(path, &length);
    TEST_ASSERT_NOT_NULL(bytes);
    reader.bytes = bytes;
    reader.length = length;
    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, close_reader, &reader);
    io->isBinary = ZR_TRUE;
    artifact = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(artifact);
    loaded = ZrCore_Io_LoadEntryFunctionToRuntime(state, artifact);
    TEST_ASSERT_NOT_NULL(loaded);
    after = find_typed(loaded, ZR_NULL);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL_MEMORY(&before->binding.contract,
                             &after->binding.contract,
                             sizeof(before->binding.contract));
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_TARGET_NONE, after->binding.target.targetKind);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, loaded, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);
    ZrCore_Io_ReadSourceFree(state->global, artifact);
    ZrCore_Io_Free(state->global, io);
    free(bytes);
    remove(path);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_parameter_is_signature_bound);
    RUN_TEST(test_typed_native_value_uses_structural_signature);
    RUN_TEST(test_typed_native_import_member_is_a_source_callable_value);
    RUN_TEST(test_typed_zero_argument_value_is_guarded);
    RUN_TEST(test_typed_aot_value_keeps_live_closure_and_checks_registration);
    RUN_TEST(test_repeated_captured_values_keep_their_context);
    RUN_TEST(test_captured_callback_retains_its_typed_contract);
    RUN_TEST(test_typed_mismatched_callable_is_rejected);
    RUN_TEST(test_typed_stale_generation_is_rejected);
    RUN_TEST(test_typed_signature_record_tampering_is_rejected);
    RUN_TEST(test_typed_binary_roundtrip_retains_signature_without_target);
    RUN_TEST(test_typed_native_import_member_binary_roundtrip);
    return UNITY_END();
}
