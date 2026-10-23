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
static TZrSize instructionsOffset, instructionCount, constantCount;
static TZrUInt32 stackSlots, frameLayoutCount, expectedFrameBytes, expectedFirstOffset;
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
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &encoded, &encodedLength));
    instructionCount = function->instructionsLength;
    constantCount = function->constantValueLength;
    stackSlots = function->stackSize;
    frameLayoutCount = function->frameSlotLayoutLength;
    expectedFrameBytes = function->frameByteSize;
    TEST_ASSERT_GREATER_THAN_UINT32(0u, frameLayoutCount);
    expectedFirstOffset = function->frameSlotLayouts[0].byteOffset;
    instructionsOffset = 0;
    for (TZrSize offset = 32u; offset + instructionCount * sizeof(TZrInstruction) <= encodedLength; ++offset) {
        if (memcmp(encoded + offset, function->instructionsList,
                   instructionCount * sizeof(TZrInstruction)) == 0) {
            TEST_ASSERT_EQUAL_size_t(0, instructionsOffset);
            instructionsOffset = offset;
        }
    }
    ZrCore_Function_Free(state, function);
    TEST_ASSERT_NOT_EQUAL(0, instructionsOffset);
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

static void test_old_raw_frame_offset_format_requires_recompilation(void) {
    TZrUInt32 savedPatch, oldPatch = ZR_IO_SOURCE_PATCH_MIN_SUPPORTED - 1u;
    memcpy(&savedPatch, encoded + 12u, sizeof(savedPatch));
    TEST_ASSERT_EQUAL_UINT32(ZR_IO_SOURCE_PATCH_CURRENT, savedPatch);
    memcpy(encoded + 12u, &oldPatch, sizeof(oldPatch));
    TEST_ASSERT_FALSE(read_binary(encoded, encodedLength, 7u));
    memcpy(encoded + 12u, &savedPatch, sizeof(savedPatch));
    TEST_ASSERT_TRUE(read_binary(encoded, encodedLength, 7u));
}

static void test_slot_relative_frame_coordinates_round_trip_and_reject_overflow(void) {
    Input input = {encoded, encodedLength, 0, 7u};
    SZrIo io;
    ZrCore_Io_Init(state, &io, read_chunk, ZR_NULL, &input);
    loaded = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(loaded);
    const SZrIoFunction *function = loaded->modules[0].entryFunction;
    TEST_ASSERT_EQUAL_UINT32(expectedFrameBytes, function->frameByteSize);
    TEST_ASSERT_EQUAL_UINT32(expectedFirstOffset, function->frameSlotLayouts[0].byteOffset);
    ZrCore_Io_ReadSourceFree(state->global, loaded);
    loaded = ZR_NULL;
    TEST_ASSERT_EQUAL_size_t(0u, ioBytes);

    /* Frame header: coordinate[2], alignment, count64; each layout is 28 bytes.
     * The instruction count follows immediately, so no byte-pattern search is
     * needed to locate the four coordinate fields under test. */
    TZrSize frameStart = instructionsOffset - sizeof(TZrUInt64) -
            (TZrSize)frameLayoutCount * 28u - 20u;
    const TZrSize positions[] = {frameStart, frameStart + 4u,
            frameStart + 24u, frameStart + 28u};
    for (unsigned n = 0; n < ZR_ARRAY_COUNT(positions); ++n) {
        TZrUInt32 saved, invalid = (n & 1u) ? (TZrUInt32)sizeof(SZrTypeValueOnStack) : UINT32_MAX;
        memcpy(&saved, encoded + positions[n], sizeof(saved));
        memcpy(encoded + positions[n], &invalid, sizeof(invalid));
        TEST_ASSERT_FALSE(read_binary(encoded, encodedLength, 7u));
        memcpy(encoded + positions[n], &saved, sizeof(saved));
    }
    TEST_ASSERT_TRUE(read_binary(encoded, encodedLength, 7u));
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

static void reject_instruction(TZrUInt16 opcode, TZrUInt16 extra,
                               TZrUInt16 a, TZrUInt16 b) {
    TZrInstruction saved, invalid = {0};
    memcpy(&saved, encoded + instructionsOffset, sizeof(saved));
    invalid.instruction.operationCode = opcode;
    invalid.instruction.operandExtra = extra;
    invalid.instruction.operand.operand1[0] = a;
    invalid.instruction.operand.operand1[1] = b;
    memcpy(encoded + instructionsOffset, &invalid, sizeof(invalid));
    TEST_ASSERT_FALSE(read_binary(encoded, encodedLength, 19u));
    memcpy(encoded + instructionsOffset, &saved, sizeof(saved));
}

static void test_invalid_instruction_boundaries_are_rejected(void) {
    TEST_ASSERT_TRUE(stackSlots > 0u && stackSlots < 128u);
    TEST_ASSERT_TRUE(constantCount > 0u && constantCount < UINT16_MAX);
    reject_instruction(ZR_INSTRUCTION_ENUM(ENUM_MAX), 0, 0, 0);
    reject_instruction(UINT16_MAX, 0, 0, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(GET_STACK), 0, UINT16_MAX, UINT16_MAX);
    reject_instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, (TZrUInt16)constantCount, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(ADD_SIGNED), (TZrUInt16)stackSlots, 0, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(ADD_SIGNED), 0, (TZrUInt16)stackSlots, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST), 0, 0, (TZrUInt16)constantCount);
    reject_instruction(ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST), ZR_INSTRUCTION_USE_RET_FLAG, 0, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(JUMP), 0, 0, 0x8000u);
    reject_instruction(ZR_INSTRUCTION_ENUM(JUMP), 0, UINT16_MAX, 0x7fffu);
    reject_instruction(ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED), 0, 0, (TZrUInt16)instructionCount);
    reject_instruction(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0, (TZrUInt16)(stackSlots - 1u), 1);
    reject_instruction(ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL), 0, UINT16_MAX, 1);
    reject_instruction(ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT), ZR_INSTRUCTION_USE_RET_FLAG,
                       (TZrUInt16)stackSlots, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 2, (TZrUInt16)(stackSlots - 1u), 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(GET_CLOSURE), 0, UINT16_MAX, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(GETUPVAL), 0, UINT16_MAX, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(CREATE_CLOSURE), 0, (TZrUInt16)constantCount, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(TRY), UINT16_MAX, 0, 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(CREATE_ARRAY), 0, UINT16_MAX, UINT16_MAX);
    reject_instruction(ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4), 0, (TZrUInt16)(stackSlots - 3u), 0);
    /* Packed operands include an otherwise invisible materialization slot. */
    reject_instruction(ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST), 0, (TZrUInt16)(stackSlots << 8u), 0);
    reject_instruction(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 0, 0, UINT16_MAX);
    reject_instruction(ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8), 0, UINT16_MAX, 0);
    TEST_ASSERT_TRUE(read_binary(encoded, encodedLength, 7u));
}

static void test_array_add_accepts_a_discarded_destination(void) {
    TZrInstruction saved, instruction = {0};
    memcpy(&saved, encoded + instructionsOffset, sizeof(saved));
    instruction.instruction.operationCode = ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT);
    instruction.instruction.operandExtra = ZR_INSTRUCTION_USE_RET_FLAG;
    /* Structural validation accepts the discard flag but still bounds both
     * source slots. Runtime type checks remain responsible for their values. */
    memcpy(encoded + instructionsOffset, &instruction, sizeof(instruction));
    TEST_ASSERT_TRUE(read_binary(encoded, encodedLength, 19u));
    memcpy(encoded + instructionsOffset, &saved, sizeof(saved));
}

static void test_valid_nested_functions_loops_and_prototypes_are_accepted(void) {
    const char *sources[] = {
        "fn makeRunner() { var seed = 4; return fn() => { return seed + 1; }; }"
        "var runner = makeRunner(); return runner();",
        "fn step(x: int): int { if (x > 0) { return x + 1; } return 0; }"
        "var total: int = 0; var i: int = 0;"
        "while (i < 5) { total = total + step(i); i = i + 1; } return total;",
        "class Empty {} return 1;"
    };
    char path[ZR_TESTS_PATH_MAX];
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("binary_loader_rejection", "", "nested", ".zro", path, sizeof(path)));
    for (unsigned n = 0; n < ZR_ARRAY_COUNT(sources); ++n) {
        TZrBytePtr bytes = ZR_NULL;
        TZrSize length = 0;
        SZrFunction *function = ZrParser_Source_Compile(state, sources[n], strlen(sources[n]),
                ZrCore_String_CreateFromNative(state, "nested.zr"));
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, path));
        ZrCore_Function_Free(state, function);
        TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &length));
        TEST_ASSERT_TRUE(read_binary(bytes, length, 13u));
        free(bytes);
    }
}

static void test_empty_and_unterminated_bodies_are_rejected(void) {
    TZrSize last = instructionsOffset + (instructionCount - 1u) * sizeof(TZrInstruction);
    TZrInstruction saved, invalid = {0};
    memcpy(&saved, encoded + last, sizeof(saved));
    invalid.instruction.operationCode = ZR_INSTRUCTION_ENUM(NOP);
    memcpy(encoded + last, &invalid, sizeof(invalid));
    TEST_ASSERT_FALSE(read_binary(encoded, encodedLength, 7u));
    memcpy(encoded + last, &saved, sizeof(saved));

    /* The instruction count precedes the encoded array. Remove the array too,
     * so subsequent metadata remains well formed and zero length is the error. */
    TZrSize countOffset = instructionsOffset - sizeof(TZrUInt64);
    TZrUInt64 savedCount, zero = 0;
    memcpy(&savedCount, encoded + countOffset, sizeof(savedCount));
    TEST_ASSERT_EQUAL_UINT64(instructionCount, savedCount);
    TZrSize removed = instructionCount * sizeof(TZrInstruction);
    TZrByte *empty = malloc(encodedLength - removed);
    TEST_ASSERT_NOT_NULL(empty);
    memcpy(empty, encoded, instructionsOffset);
    memcpy(empty + countOffset, &zero, sizeof(zero));
    memcpy(empty + instructionsOffset, encoded + instructionsOffset + removed,
           encodedLength - instructionsOffset - removed);
    TZrBool accepted = read_binary(empty, encodedLength - removed, 7u);
    free(empty);
    TEST_ASSERT_FALSE(accepted);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_and_unterminated_bodies_are_rejected);
    RUN_TEST(test_chunked_round_trip_and_all_truncations);
    RUN_TEST(test_old_raw_frame_offset_format_requires_recompilation);
    RUN_TEST(test_slot_relative_frame_coordinates_round_trip_and_reject_overflow);
    RUN_TEST(test_header_and_length_bombs_are_rejected);
    RUN_TEST(test_all_native_allocation_failures_release_partial_tree);
    RUN_TEST(test_sticky_short_reads_initialize_destinations);
    RUN_TEST(test_invalid_instruction_boundaries_are_rejected);
    RUN_TEST(test_array_add_accepts_a_discarded_destination);
    RUN_TEST(test_valid_nested_functions_loops_and_prototypes_are_accepted);
    return UNITY_END();
}
