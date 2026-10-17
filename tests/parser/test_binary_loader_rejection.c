#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

static SZrState *state;
static SZrIoSource *loaded;
static TZrBytePtr encoded;
static TZrSize encodedLength;
static FZrAllocator originalAllocator;
static TZrSize ioBytes, allocationCalls, failAllocation;

static TZrPtr tracked_allocator(TZrPtr data, TZrPtr pointer, TZrSize oldSize,
                                TZrSize newSize, TZrInt64 kind) {
    TZrPtr result;
    if (kind == ZR_MEMORY_NATIVE_TYPE_IO && newSize > 0 &&
        ++allocationCalls == failAllocation) return ZR_NULL;
    result = originalAllocator(data, pointer, oldSize, newSize, kind);
    if (kind == ZR_MEMORY_NATIVE_TYPE_IO && (result != ZR_NULL || newSize == 0)) {
        ioBytes = ioBytes - oldSize + newSize;
    }
    return result;
}

typedef struct Input {
    TZrBytePtr bytes;
    TZrSize length, offset, chunk;
} Input;

static TZrBytePtr read_chunk(SZrState *unused, TZrPtr data, TZrSize *size) {
    Input *input = data;
    TZrBytePtr result = input->bytes + input->offset;
    (void)unused;
    *size = input->length - input->offset;
    if (*size > input->chunk) *size = input->chunk;
    input->offset += *size;
    return result;
}

static TZrBool read_binary(TZrBytePtr bytes, TZrSize length, TZrSize chunk) {
    Input input = {bytes, length, 0, chunk};
    SZrIo io;
    TZrBool success;
    ZrCore_Io_Init(state, &io, read_chunk, ZR_NULL, &input);
    loaded = ZrCore_Io_ReadSourceNew(&io);
    success = loaded != ZR_NULL;
    TEST_ASSERT_EQUAL_INT(!success, io.hasReadError);
    ZrCore_Io_ReadSourceFree(state->global, loaded);
    loaded = ZR_NULL;
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, ioBytes, "binary read leaked native IO storage");
    return success;
}

void setUp(void) {
    const char *source = "var values: int[] = [1, 2]; return values[0] + values[1];";
    char path[ZR_TESTS_PATH_MAX];
    SZrFunction *function;
    state = ZR_NULL;
    loaded = ZR_NULL;
    encoded = ZR_NULL;
    originalAllocator = ZR_NULL;
    ioBytes = allocationCalls = failAllocation = 0;
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "binary_bounds.zr"));
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("binary_loader_rejection", "", "valid", ".zro", path, sizeof(path)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, path));
    ZrCore_Function_Free(state, function);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &encoded, &encodedLength));
    originalAllocator = state->global->allocator;
    state->global->allocator = tracked_allocator;
}

void tearDown(void) {
    if (state != ZR_NULL) {
        ZrCore_Io_ReadSourceFree(state->global, loaded);
        if (originalAllocator != ZR_NULL) state->global->allocator = originalAllocator;
    }
    free(encoded);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_chunked_round_trip_and_all_truncations(void) {
    TEST_ASSERT_TRUE(read_binary(encoded, encodedLength, 7u));
    for (TZrSize cut = 0; cut < encodedLength; ++cut) {
        TEST_ASSERT_FALSE(read_binary(encoded, cut, 17u));
    }
}

static void test_header_and_length_bombs_are_rejected(void) {
    const TZrSize headerBytes[] = {0, 4, 8, 12, 16, 24, 25, 26, 27, 28, 29, 30, 31};
    const TZrUInt64 bombs[] = {UINT64_MAX, UINT64_MAX / 8u + 1u, 1ull << 30u};
    for (unsigned i = 0; i < ZR_ARRAY_COUNT(headerBytes); ++i) {
        encoded[headerBytes[i]] ^= 0x80u;
        TEST_ASSERT_FALSE(read_binary(encoded, encodedLength, encodedLength));
        encoded[headerBytes[i]] ^= 0x80u;
    }
    /* Header module count and the first string length are independent inputs. */
    for (TZrSize offset = 32u; offset <= 40u; offset += 8u) {
        TZrUInt64 saved;
        memcpy(&saved, encoded + offset, sizeof(saved));
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(bombs); ++i) {
            memcpy(encoded + offset, &bombs[i], sizeof(bombs[i]));
            TEST_ASSERT_FALSE(read_binary(encoded, encodedLength, encodedLength));
        }
        memcpy(encoded + offset, &saved, sizeof(saved));
    }
}

static void test_all_native_allocation_failures_release_partial_tree(void) {
    TZrSize count;
    TEST_ASSERT_TRUE(read_binary(encoded, encodedLength, encodedLength));
    count = allocationCalls;
    for (TZrSize fail = 1; fail <= count; ++fail) {
        allocationCalls = 0;
        failAllocation = fail;
        TEST_ASSERT_FALSE(read_binary(encoded, encodedLength, encodedLength));
    }
    failAllocation = 0;
    TEST_ASSERT_TRUE(read_binary(encoded, encodedLength, encodedLength));
}

static void test_sticky_short_reads_initialize_destinations(void) {
    Input input = {encoded, 0, 0, 1};
    SZrIo io;
    TZrUInt64 value = UINT64_MAX;
    ZrCore_Io_Init(state, &io, read_chunk, ZR_NULL, &input);
    TEST_ASSERT_EQUAL_size_t(0, ZrCore_Io_Read(&io, (TZrBytePtr)&value, sizeof(value)));
    TEST_ASSERT_EQUAL_UINT64(0, value);
    value = UINT64_MAX;
    TEST_ASSERT_EQUAL_size_t(0, ZrCore_Io_Read(&io, (TZrBytePtr)&value, sizeof(value)));
    TEST_ASSERT_EQUAL_UINT64(0, value);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_chunked_round_trip_and_all_truncations);
    RUN_TEST(test_header_and_length_bombs_are_rejected);
    RUN_TEST(test_all_native_allocation_failures_release_partial_tree);
    RUN_TEST(test_sticky_short_reads_initialize_destinations);
    return UNITY_END();
}
