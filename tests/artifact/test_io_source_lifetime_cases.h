#ifndef ZR_TEST_IO_SOURCE_LIFETIME_CASES_H
#define ZR_TEST_IO_SOURCE_LIFETIME_CASES_H

#include "zr_vm_core/memory.h"

/* 只核算 IO 类原生分配；其他 VM 分配不属于 source 释放责任。 */
static FZrAllocator g_io_source_allocator;
static TZrSize g_io_source_allocated_count;
static TZrSize g_io_source_freed_count;
static TZrSize g_io_source_allocated_bytes;
static TZrSize g_io_source_freed_bytes;

/* 委托原分配器并核对 IO 类 realloc 的旧块与新块，供两种所有权场景共用。 */
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

/* 分别检验未装载的 source 全量释放，以及装载后函数脱离 source 的独立寿命。 */
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
    /* 仅在 source 读取和释放期间替换分配器；函数执行必须回到原分配器。 */
    state->global->allocator = io_source_counting_allocator;
    /* BUG: 分配器恢复前的 Unity 断言若失败，会 longjmp 越过恢复与资源释放，污染后续测试。 */
    /* TODO: 将失败报告移到恢复之后，或建立统一清理出口。 */
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

/* 不装载运行时函数时，source 内的原生对象应成对释放。 */
static void test_io_source_free_releases_unloaded_graph(void) {
    io_source_assert_read_free(ZR_FALSE);
}

/* 已装载函数应在 source 释放后仍可运行，证明装载没有借用待释放数组。 */
static void test_io_source_free_preserves_loaded_function(void) {
    io_source_assert_read_free(ZR_TRUE);
}

#endif
