#ifndef ZR_TEST_IO_SOURCE_LIFETIME_CASES_H
#define ZR_TEST_IO_SOURCE_LIFETIME_CASES_H

#include "zr_vm_core/memory.h"

static FZrAllocator g_io_source_allocator;
static TZrSize g_io_source_allocated_count;
static TZrSize g_io_source_freed_count;
static TZrSize g_io_source_allocated_bytes;
static TZrSize g_io_source_freed_bytes;

static TZrPtr io_source_counting_allocator(TZrPtr arguments,
                                          TZrPtr pointer,
                                          TZrSize originalSize,
                                          TZrSize newSize,
                                          TZrInt64 type) {
    TZrBool hadAllocation = pointer != ZR_NULL;
    TZrPtr result = g_io_source_allocator(arguments, pointer, originalSize, newSize, type);
    if (type == ZR_MEMORY_NATIVE_TYPE_IO) {
        if (hadAllocation && (newSize == 0u || result != ZR_NULL)) {
            ++g_io_source_freed_count;
            g_io_source_freed_bytes += originalSize;
        }
        if (result != ZR_NULL && newSize != 0u) {
            ++g_io_source_allocated_count;
            g_io_source_allocated_bytes += newSize;
        }
    }
    return result;
}

static void io_source_assert_read_free(TZrBool loadRuntime) {
    static const TZrChar sourceText[] =
            "fn adjust(value: int, offset: int = 2): int {\n"
            " try { return value + offset; } finally { var finished = true; }\n"
            "}\n"
            "return adjust(40);\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrFunction *function;
    SZrFunction *loaded = ZR_NULL;
    SZrIoSource *source;
    SZrIo io;
    ZrTestsFixtureReader reader;
    TZrChar binaryPath[512];
    TZrByte *binaryBytes;
    TZrSize binaryLength = 0u;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "io_source_lifetime.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_CompileTest(
            state, sourceText, sizeof(sourceText) - 1u, sourceName);
    TEST_ASSERT_NOT_NULL(function);
    snprintf(binaryPath, sizeof(binaryPath), "%s/io_source_lifetime.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    ZrCore_Io_Init(state, &io, ZrTests_Fixture_ReaderRead, manifest_reader_close_noop, &reader);
    io.isBinary = ZR_TRUE;
    g_io_source_allocated_count = 0u;
    g_io_source_freed_count = 0u;
    g_io_source_allocated_bytes = 0u;
    g_io_source_freed_bytes = 0u;
    g_io_source_allocator = state->global->allocator;
    state->global->allocator = io_source_counting_allocator;
    source = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_FALSE(io.hasReadError);
    TEST_ASSERT_GREATER_THAN_UINT64(1u, g_io_source_allocated_count);
    if (loadRuntime) {
        loaded = ZrCore_Io_LoadEntryFunctionToRuntime(state, source);
        TEST_ASSERT_NOT_NULL(loaded);
        TEST_ASSERT_TRUE(source->modules[0].entryFunction->instructions != loaded->instructionsList);
    }
    ZrCore_Io_ReadSourceFree(state->global, source);
    ZrCore_Io_ReadSourceFree(state->global, ZR_NULL);
    state->global->allocator = g_io_source_allocator;
    TEST_ASSERT_EQUAL_UINT64(g_io_source_allocated_count, g_io_source_freed_count);
    TEST_ASSERT_EQUAL_UINT64(g_io_source_allocated_bytes, g_io_source_freed_bytes);
    free(binaryBytes);
    remove(binaryPath);

    if (loaded != ZR_NULL) {
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, loaded, &result));
        TEST_ASSERT_EQUAL_INT64(42, result);
        ZrCore_Function_Free(state, loaded);
    }
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_io_source_free_releases_unloaded_graph(void) {
    io_source_assert_read_free(ZR_FALSE);
}

static void test_io_source_free_preserves_loaded_function(void) {
    io_source_assert_read_free(ZR_TRUE);
}

#endif
