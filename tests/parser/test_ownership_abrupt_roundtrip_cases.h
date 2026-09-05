#ifndef ZR_TEST_OWNERSHIP_ABRUPT_ROUNDTRIP_CASES_H
#define ZR_TEST_OWNERSHIP_ABRUPT_ROUNDTRIP_CASES_H

#include "harness/path_support.h"
#include "zr_vm_core/io.h"
#include "zr_vm_parser/writer.h"
#include "test_ownership_abrupt_parity_cases.h"

typedef struct SZrAbruptReader {
    TZrBytePtr bytes;
    TZrSize length;
    TZrBool consumed;
} SZrAbruptReader;

static TZrBytePtr abrupt_read(SZrState *state, TZrPtr argument, TZrSize *length) {
    SZrAbruptReader *reader = argument;
    ZR_UNUSED_PARAMETER(state);
    if (reader->consumed) { return ZR_NULL; }
    reader->consumed = ZR_TRUE;
    *length = reader->length;
    return reader->bytes;
}

static void abrupt_close(SZrState *state, TZrPtr argument) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(argument);
}

static void test_ownership_abrupt_vm_and_binary_roundtrip(void) {
    SZrFunction *function = compile_source(ownership_abrupt_parity_source());
    SZrFunction *loaded;
    SZrState *loadedState;
    SZrAbruptReader reader = {ZR_NULL, 0u, ZR_FALSE};
    SZrIo *io;
    SZrIoSource *source;
    TZrChar path[ZR_TESTS_PATH_MAX];
    TZrInt64 result = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("ownership_astra", "roundtrip",
            "abrupt", ".zro", path, sizeof(path)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(g_state, function, path));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result));
    TEST_ASSERT_EQUAL_INT64(ZR_TEST_OWNERSHIP_ABRUPT_PARITY_EXPECTED, result);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &reader.bytes, &reader.length));
    loadedState = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(loadedState);
    io = ZrCore_Io_New(loadedState->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(loadedState, io, abrupt_read, abrupt_close, &reader);
    io->isBinary = ZR_TRUE;
    source = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(source);
    loaded = ZrCore_Io_LoadEntryFunctionToRuntime(loadedState, source);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(loadedState, loaded, &result));
    TEST_ASSERT_EQUAL_INT64(ZR_TEST_OWNERSHIP_ABRUPT_PARITY_EXPECTED, result);
    ZrCore_Function_Free(loadedState, loaded);
    ZrCore_Io_ReadSourceFree(loadedState->global, source);
    ZrCore_Io_Free(loadedState->global, io);
    ZrTests_Runtime_State_Destroy(loadedState);
    free(reader.bytes);
    TEST_ASSERT_EQUAL_INT(0, remove(path));
    ZrCore_Function_Free(g_state, function);
}

#endif
