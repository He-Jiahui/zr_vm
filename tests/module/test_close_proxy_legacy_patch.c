#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "tests/harness/path_support.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrCloseProxyBinaryReader {
    TZrBytePtr bytes;
    TZrSize length;
    TZrBool consumed;
} SZrCloseProxyBinaryReader;

typedef struct SZrCloseProxyReadContext {
    SZrIo *io;
    SZrIoSource *source;
} SZrCloseProxyReadContext;

void setUp(void) {}
void tearDown(void) {}

static TZrBytePtr close_proxy_binary_read(SZrState *state, TZrPtr customData,
                                           TZrSize *outLength) {
    SZrCloseProxyBinaryReader *reader = (SZrCloseProxyBinaryReader *)customData;
    ZR_UNUSED_PARAMETER(state);
    if (reader == ZR_NULL || outLength == ZR_NULL || reader->consumed) {
        return ZR_NULL;
    }
    reader->consumed = ZR_TRUE;
    *outLength = reader->length;
    return reader->bytes;
}

static void close_proxy_binary_close(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static void close_proxy_read_in_try(SZrState *state, TZrPtr arguments) {
    SZrCloseProxyReadContext *context = (SZrCloseProxyReadContext *)arguments;
    ZR_UNUSED_PARAMETER(state);
    context->source = ZrCore_Io_ReadSourceNew(context->io);
}

static TZrUInt32 close_proxy_read_u32_le(const TZrByte *bytes, TZrSize offset) {
    return (TZrUInt32)bytes[offset] |
           ((TZrUInt32)bytes[offset + 1u] << 8u) |
           ((TZrUInt32)bytes[offset + 2u] << 16u) |
           ((TZrUInt32)bytes[offset + 3u] << 24u);
}

static void close_proxy_write_u32_le(TZrByte *bytes, TZrSize offset,
                                      TZrUInt32 value) {
    bytes[offset] = (TZrByte)(value & 0xffu);
    bytes[offset + 1u] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[offset + 2u] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[offset + 3u] = (TZrByte)((value >> 24u) & 0xffu);
}

static void test_appended_opcode_preserves_legacy_instruction_numbers(void) {
    TEST_ASSERT_EQUAL_UINT32(244u, ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL));
    TEST_ASSERT_EQUAL_UINT32(245u, ZR_INSTRUCTION_ENUM(MARK_CLOSE_PROXY));
    TEST_ASSERT_EQUAL_UINT32(246u, ZR_INSTRUCTION_ENUM(ENUM_MAX));
}

static void test_patch_44_writer_and_reader_compatibility_gate(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrChar path[ZR_TESTS_PATH_MAX];
    TZrBytePtr bytes = ZR_NULL;
    TZrSize length = 0u;
    SZrCloseProxyBinaryReader reader = {0};
    SZrCloseProxyReadContext context = {0};
    SZrIo io = {0};
    EZrThreadStatus status;
    const TZrSize patchOffset = 12u;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_UINT32(44u, ZR_IO_SOURCE_PATCH_HAS_MARK_CLOSE_PROXY);
    TEST_ASSERT_EQUAL_UINT32(44u, ZR_IO_SOURCE_PATCH_CURRENT);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(TZrInstruction), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    memset(function->instructionsList, 0, sizeof(TZrInstruction));
    function->instructionsList[0].instruction.operationCode = ZR_INSTRUCTION_ENUM(NOP);
    function->instructionsLength = 1u;
    function->stackSize = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "close_proxy_legacy_patch", "bin", "simple", ".zro", path, sizeof(path)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, path));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &length));
    TEST_ASSERT_TRUE(length > patchOffset + sizeof(TZrUInt32));
    TEST_ASSERT_EQUAL_UINT32(44u, close_proxy_read_u32_le(bytes, patchOffset));

    /* Patch 44 introduces no fields: its opcode-free payload is valid as patch 43. */
    close_proxy_write_u32_le(bytes, patchOffset, 43u);
    reader.bytes = bytes;
    reader.length = length;
    ZrCore_Io_Init(state, &io, close_proxy_binary_read,
                   close_proxy_binary_close, &reader);
    io.isBinary = ZR_TRUE;
    context.io = &io;
    context.source = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(context.source);
    TEST_ASSERT_EQUAL_UINT32(43u, context.source->versionPatch);
    ZrCore_Io_ReadSourceFree(state->global, context.source);

    close_proxy_write_u32_le(bytes, patchOffset, 45u);
    reader.consumed = ZR_FALSE;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, close_proxy_binary_read,
                   close_proxy_binary_close, &reader);
    io.isBinary = ZR_TRUE;
    context.io = &io;
    context.source = ZR_NULL;
    status = ZrCore_Exception_TryRun(state, close_proxy_read_in_try, &context);
    TEST_ASSERT_TRUE(ZrCore_Exception_IsStausError(status));
    TEST_ASSERT_NULL(context.source);

    TEST_ASSERT_EQUAL_INT(0, remove(path));
    free(bytes);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_appended_opcode_preserves_legacy_instruction_numbers);
    RUN_TEST(test_patch_44_writer_and_reader_compatibility_gate);
    return UNITY_END();
}
