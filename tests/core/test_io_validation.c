#include "unity.h"

#include "tests/harness/module_fixture_support.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/io.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void io_validation_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

void setUp(void) {}
void tearDown(void) {}

static SZrIoSource *io_validation_read(SZrState *state, TZrByte *bytes, TZrSize length, SZrIo *io) {
    ZrTestsFixtureReader reader;
    memset(&reader, 0, sizeof(reader));
    reader.bytes = bytes;
    reader.length = length;
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, io_validation_close_noop, &reader);
    io->isBinary = ZR_TRUE;
    return ZrCore_Io_ReadSourceNew(io);
}

static void test_io_reads_valid_writer_output_and_rejects_bad_headers(void) {
    static const TZrChar sourceText[] = "return 42;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrFunction *function;
    TZrChar path[512];
    TZrByte *bytes;
    TZrByte *damaged;
    TZrSize length = 0u;
    SZrIo io;
    SZrIoSource *source;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "io_validation.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_CompileTest(state, sourceText, sizeof(sourceText) - 1u, sourceName);
    TEST_ASSERT_NOT_NULL(function);
    snprintf(path, sizeof(path), "%s/io_validation.zro", ZR_VM_TESTS_BINARY_DIR);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, path));
    bytes = ZrTests_Fixture_ReadFileBytes(path, &length);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_UINT32(40u, length);

    source = io_validation_read(state, bytes, length, &io);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_FALSE(io.hasReadError);
    TEST_ASSERT_EQUAL_UINT32(ZR_IO_SOURCE_PATCH_CURRENT, source->versionPatch);
    TEST_ASSERT_EQUAL_UINT64(1u, source->modulesLength);
    ZrCore_Io_ReadSourceFree(state->global, source);

    damaged = (TZrByte *)malloc(length);
    TEST_ASSERT_NOT_NULL(damaged);
    memcpy(damaged, bytes, length);
    damaged[0] ^= 1u;
    source = io_validation_read(state, damaged, length, &io);
    TEST_ASSERT_NULL(source);
    TEST_ASSERT_TRUE(io.hasReadError);

    /* The top-level module count is a native-size nested allocation boundary. */
    memcpy(damaged, bytes, length);
    {
        TZrSize impossibleCount = (TZrSize)-1;
        memcpy(damaged + 32u, &impossibleCount, sizeof(impossibleCount));
    }
    source = io_validation_read(state, damaged, length, &io);
    TEST_ASSERT_NULL(source);
    TEST_ASSERT_TRUE(io.hasReadError);

    /* Imports follow the module name and digest; reject their count before allocating. */
    memcpy(damaged, bytes, length);
    {
        TZrSize moduleNameLength = 0u;
        TZrSize moduleMd5Length = 0u;
        TZrSize importsCountOffset;
        TZrSize impossibleCount = (TZrSize)-1;
        memcpy(&moduleNameLength, bytes + 40u, sizeof(moduleNameLength));
        memcpy(&moduleMd5Length,
               bytes + 40u + sizeof(TZrSize) + moduleNameLength,
               sizeof(moduleMd5Length));
        importsCountOffset = 40u + sizeof(TZrSize) + moduleNameLength +
                             sizeof(TZrSize) + moduleMd5Length;
        TEST_ASSERT_TRUE(importsCountOffset + sizeof(impossibleCount) <= length);
        memcpy(damaged + importsCountOffset, &impossibleCount, sizeof(impossibleCount));
    }
    source = io_validation_read(state, damaged, length, &io);
    TEST_ASSERT_NULL(source);
    TEST_ASSERT_TRUE(io.hasReadError);

    /* The packed major/minor field must agree with its explicit version fields. */
    memcpy(damaged, bytes, length);
    damaged[16] ^= 1u;
    source = io_validation_read(state, damaged, length, &io);
    TEST_ASSERT_NULL(source);
    TEST_ASSERT_TRUE(io.hasReadError);

    /* Module-name length begins at byte 40 in the fixed header; reject SIZE_MAX before allocation. */
    memcpy(damaged, bytes, length);
    {
        TZrSize impossibleLength = (TZrSize)-1;
        memcpy(damaged + 40u, &impossibleLength, sizeof(impossibleLength));
    }
    source = io_validation_read(state, damaged, length, &io);
    TEST_ASSERT_NULL(source);
    TEST_ASSERT_TRUE(io.hasReadError);

    /* Truncate inside the header, module count, and final payload. */
    {
        const TZrSize truncatedLengths[] = {0u, 12u, 39u, length - 1u};
        for (TZrSize index = 0u; index < sizeof(truncatedLengths) / sizeof(truncatedLengths[0]); index++) {
            source = io_validation_read(state, bytes, truncatedLengths[index], &io);
            TEST_ASSERT_NULL(source);
            TEST_ASSERT_TRUE(io.hasReadError);
        }
    }

    free(damaged);
    free(bytes);
    remove(path);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_io_reads_valid_writer_output_and_rejects_bad_headers);
    return UNITY_END();
}
