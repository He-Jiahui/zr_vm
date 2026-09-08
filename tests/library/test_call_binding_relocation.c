#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/module_fixture_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

void setUp(void) {}
void tearDown(void) {}

static void close_reader(SZrState *state, TZrPtr data) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(data);
}

static TZrBool contains_bytes(const TZrByte *bytes, TZrSize length,
                              const void *value, TZrSize size) {
    for (TZrSize offset = 0u; offset + size <= length; ++offset) {
        if (memcmp(bytes + offset, value, size) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_call_binding_binary_roundtrip_relocates_targets(void) {
    const char *source =
            "class Box { pub fn read(): int { return 29; } }\n"
            "class Holder { pub var box: Box; pub @constructor() { this.box = new Box(); } "
            "pub fn read(): int { return this.box.read(); } }\n"
            "var holder = new Holder(); return holder.read();\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *compiled;
    SZrFunction *loaded;
    SZrIo *io;
    SZrIoSource *artifact;
    ZrTestsFixtureReader reader = {0};
    TZrChar path[512];
    TZrByte *bytes;
    TZrSize length = 0u;
    TZrUInt32 bindingCount = 0u;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    compiled = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "call_binding_relocation.zr"));
    TEST_ASSERT_NOT_NULL(compiled);
    snprintf(path, sizeof(path), "%s/call_binding_relocation.zro", ZR_VM_TESTS_BINARY_DIR);
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
    TEST_ASSERT_EQUAL_UINT32(compiled->callSiteCacheLength, loaded->callSiteCacheLength);
    for (TZrUInt32 index = 0u; index < compiled->callSiteCacheLength; ++index) {
        const SZrCallBinding *before = &compiled->callSiteCaches[index].binding;
        const SZrCallBinding *after = &loaded->callSiteCaches[index].binding;
        if (before->contract.bindingKind == 0u) {
            continue;
        }
        ++bindingCount;
        TEST_ASSERT_EQUAL_MEMORY(&before->contract, &after->contract, sizeof(before->contract));
        if (before->target.targetKind == ZR_CALL_BINDING_TARGET_VM) {
            SZrFunction *target = before->target.vm.function;
            TEST_ASSERT_NOT_NULL(target);
            TEST_ASSERT_FALSE(contains_bytes(bytes, length, &target, sizeof(target)));
            TEST_ASSERT_NOT_NULL(after->target.vm.function);
            TEST_ASSERT_NOT_EQUAL(target, after->target.vm.function);
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0u, bindingCount);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, loaded, &result));
    TEST_ASSERT_EQUAL_INT64(29, result);
    ZrCore_Io_ReadSourceFree(state->global, artifact);
    ZrCore_Io_Free(state->global, io);
    free(bytes);
    remove(path);
    ZrTests_Runtime_State_Destroy(state);
}

static void assert_corrupt_accessor_is_rejected(TZrUInt32 operation, TZrBool corruptStatic,
                                               TZrBool corruptInitializer) {
    const char *source =
            "class Box { pub property value: int { get { return 29; } } } "
            "var box = new Box(); return box.value;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *compiled;
    SZrFunction *loaded;
    SZrFunctionCallSiteCacheEntry *entry = ZR_NULL;
    SZrIo *io;
    SZrIoSource *artifact;
    SZrCallBindingDiagnostic diagnostic;
    ZrTestsFixtureReader reader = {0};
    TZrChar path[512];
    TZrByte *bytes;
    TZrSize length = 0u;
    TZrUInt32 instructionIndex;
    TZrMetadataToken targetToken;
    TEST_ASSERT_NOT_NULL(state);
    compiled = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "corrupt_accessor_binding.zr"));
    TEST_ASSERT_NOT_NULL(compiled);
    for (TZrUInt32 index = 0u; index < compiled->callSiteCacheLength; ++index) {
        if (compiled->callSiteCaches[index].binding.contract.operation == ZR_CALL_BINDING_OPERATION_GET &&
            compiled->callSiteCaches[index].binding.contract.bindingKind != ZR_CALL_BINDING_NONE) {
            entry = &compiled->callSiteCaches[index];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(entry);
    instructionIndex = entry->instructionIndex;
    targetToken = entry->binding.contract.targetMetadataToken;
    entry->binding.contract.operation = operation;
    TEST_ASSERT_LESS_THAN_UINT32(compiled->memberEntryLength, entry->memberEntryIndex);
    if (corruptStatic) {
        compiled->memberEntries[entry->memberEntryIndex].reserved0 ^=
                ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR;
    }
    if (corruptInitializer) {
        compiled->memberEntries[entry->memberEntryIndex].reserved0 |=
                ZR_FUNCTION_MEMBER_ENTRY_FLAG_PROPERTY_INITIALIZER;
    }
    snprintf(path, sizeof(path), "%s/corrupt_accessor_binding.zro", ZR_VM_TESTS_BINARY_DIR);
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
    diagnostic = state->lastCallBindingError;
    ZrCore_Io_ReadSourceFree(state->global, artifact);
    ZrCore_Io_Free(state->global, io);
    free(bytes);
    remove(path);
    ZrTests_Runtime_State_Destroy(state);
    TEST_ASSERT_NULL_MESSAGE(loaded, "A serialized accessor cannot change its operation or descriptor role");
    TEST_ASSERT_EQUAL_INT(ZR_CALL_BINDING_INVALID_RELOCATION, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT32(instructionIndex, diagnostic.instructionIndex);
    TEST_ASSERT_EQUAL_UINT32(targetToken, diagnostic.targetMetadataToken);
}

static void test_serialized_getter_cannot_be_relabelled_as_call(void) {
    assert_corrupt_accessor_is_rejected(ZR_CALL_BINDING_OPERATION_CALL, ZR_FALSE, ZR_FALSE);
}

static void test_serialized_getter_cannot_be_relabelled_as_setter(void) {
    assert_corrupt_accessor_is_rejected(ZR_CALL_BINDING_OPERATION_SET, ZR_FALSE, ZR_FALSE);
}

static void test_serialized_accessor_static_role_must_match_descriptor(void) {
    assert_corrupt_accessor_is_rejected(ZR_CALL_BINDING_OPERATION_GET, ZR_TRUE, ZR_FALSE);
}

static void test_serialized_getter_cannot_have_initializer_role(void) {
    assert_corrupt_accessor_is_rejected(ZR_CALL_BINDING_OPERATION_GET, ZR_FALSE, ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_call_binding_binary_roundtrip_relocates_targets);
    RUN_TEST(test_serialized_getter_cannot_be_relabelled_as_call);
    RUN_TEST(test_serialized_getter_cannot_be_relabelled_as_setter);
    RUN_TEST(test_serialized_accessor_static_role_must_match_descriptor);
    RUN_TEST(test_serialized_getter_cannot_have_initializer_role);
    return UNITY_END();
}
