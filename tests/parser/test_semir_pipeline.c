#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrBinaryFixtureReader {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrBinaryFixtureReader;

#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_INFO(summary, details)                                                                                    \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                      \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                    \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                      \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                      \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

static char *read_text_file_owned(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const TZrChar *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == ZR_NULL) {
        return ZR_NULL;
    }

    marker = strstr(sourceFile, "tests/parser/test_semir_pipeline.c");
    if (marker == ZR_NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_semir_pipeline.c");
    }
    if (marker == ZR_NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1 >= sizeof(path)) {
        return ZR_NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1);
    return read_text_file_owned(path);
}

static TZrByte *read_binary_file_owned(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (path == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    *outLength = 0;
    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    *outLength = (TZrSize)fileSize;
    return buffer;
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_semir_opcode(const SZrFunction *function, EZrSemIrOpcode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        if ((EZrSemIrOpcode)function->semIrInstructions[index].opcode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_native_helper_constant(const SZrFunction *function, TZrUInt64 helperId) {
    TZrUInt32 index;

    if (function == ZR_NULL || helperId == ZR_IO_NATIVE_HELPER_NONE || function->constantValueList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        if (constant->type == ZR_VALUE_TYPE_NATIVE_POINTER &&
            ZrParser_Writer_GetSerializableNativeHelperId(constant->value.nativeFunction) == helperId) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBytePtr binary_fixture_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrBinaryFixtureReader *reader = (SZrBinaryFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);
    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return (TZrBytePtr)reader->bytes;
}

static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static void test_intermediate_writer_emits_semir_sections(void) {
    SZrTestTimer timer;
    const char *testSummary = "Intermediate Writer Emits SemIR Sections";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Intermediate SemIR metadata",
              "Testing that .zri output includes TYPE_TABLE, OWNERSHIP_TABLE, EFFECT_TABLE, BLOCK_GRAPH, SEMIR, DEOPT_MAP and EH_TABLE sections");

    {
        SZrState *state = create_test_state();
        const char *source =
                "class Box {}\n"
                "var owner = %unique new Box();\n"
                "var alias = %shared(owner);\n"
                "var watcher = %weak(alias);";
        const char *intermediatePath = "semir_sections_test.zri";
        SZrString *sourceName;
        SZrFunction *func;
        char *intermediateText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_Create(state, "semir_sections_test.zr", 22);
        TEST_ASSERT_NOT_NULL(sourceName);
        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "TYPE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWNERSHIP_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "EFFECT_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "BLOCK_GRAPH"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DEOPT_MAP"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "EH_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_UNIQUE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_SHARE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_WEAK"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_ownership_builtins_lower_to_ownership_opcodes(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Builtins Lower To Ownership Opcodes";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Ownership opcode lowering",
              "Testing that ownership builtins no longer depend on serialized native helper constants and instead emit dedicated ownership opcodes");

    {
        SZrState *state = create_test_state();
        const char *source =
                "class Box {}\n"
                "var owner = %unique new Box();\n"
                "var alias = %shared(owner);\n"
                "var watcher = %weak(alias);";
        SZrString *sourceName;
        SZrFunction *func;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_Create(state, "ownership_opcode_lowering_test.zr", 33);
        TEST_ASSERT_NOT_NULL(sourceName);
        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_UNIQUE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_WEAK)));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_UNIQUE));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_WEAK));
        ZrCore_Function_Free(state, func);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_struct_value_type_places_emit_semir_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Struct Value-Type Places Emit SemIR Metadata";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Struct value-type SemIR",
              "Testing that known struct construction, field assignment, copy, and field load are visible as typed value-place SemIR rows");

    {
        SZrState *state = create_test_state();
        const char *source =
                "struct Point {\n"
                "    pub var x: int;\n"
                "    pub var y: int;\n"
                "    pub @constructor(x: int, y: int) {\n"
                "        this.x = x;\n"
                "        this.y = y;\n"
                "    }\n"
                "}\n"
                "var p: Point = $Point(1, 2);\n"
                "var q: Point = p;\n"
                "q.x = 3;\n"
                "var sum = q.x + q.y;";
        const char *intermediatePath = "semir_value_type_places_test.zri";
        SZrString *sourceName;
        SZrFunction *func;
        char *intermediateText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_Create(state, "semir_value_type_places_test.zr", 31);
        TEST_ASSERT_NOT_NULL(sourceName);
        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "FIELD_ADDR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "STORE_VALUE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "COPY_VALUE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "LOAD_VALUE"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_struct_value_type_call_and_return_emit_semir_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Struct Value-Type Call And Return Emit SemIR Metadata";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Struct call/return SemIR",
              "Testing that a known POD struct return is visible as typed CALL_TYPED and RETURN_TYPED SemIR rows");

    {
        SZrState *state = create_test_state();
        const char *source =
                "struct Point {\n"
                "    pub var x: int;\n"
                "    pub var y: int;\n"
                "    pub @constructor(x: int, y: int) {\n"
                "        this.x = x;\n"
                "        this.y = y;\n"
                "    }\n"
                "}\n"
                "pub makePoint(seed: int): Point {\n"
                "    var local: Point = $Point(seed, seed + 1);\n"
                "    return local;\n"
                "}\n"
                "var returned: Point = makePoint(3);\n"
                "return returned.x + returned.y;";
        const char *intermediatePath = "semir_value_type_call_return_test.zri";
        SZrString *sourceName;
        SZrFunction *func;
        char *intermediateText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_Create(state, "semir_value_type_call_return_test.zr", 36);
        TEST_ASSERT_NOT_NULL(sourceName);
        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALL_TYPED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "RETURN_TYPED"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_aot_execir_source_exposes_inline_frame_byte_layout(void) {
    SZrTestTimer timer;
    const char *testSummary = "AOT ExecIR Source Exposes Inline Frame Byte Layout";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("AOT ExecIR frame layout",
              "Testing that archived AotExecIR carries per-slot inline frame byte layout facts for value-type lowering");

    {
        char *execIrHeaderText = read_repo_text_file_owned(
                "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h");
        char *execIrSourceText = read_repo_text_file_owned(
                "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c");

        TEST_ASSERT_NOT_NULL(execIrHeaderText);
        TEST_ASSERT_NOT_NULL(execIrSourceText);

        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "typedef struct SZrAotExecIrFrameSlotLayout"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "TZrUInt32 frameByteSize;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "TZrUInt32 frameByteAlign;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "TZrUInt32 slotLayoutCount;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "SZrAotExecIrFrameSlotLayout *slotLayouts;"));

        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "outFrameLayout->frameByteSize = function->frameByteSize;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "outFrameLayout->frameByteAlign = function->frameByteAlign;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "function->frameSlotLayoutLength"));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "sizeof(SZrAotExecIrFrameSlotLayout)"));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "destinationLayout->byteOffset = sourceLayout->byteOffset;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "frameLayout.slotLayouts"));

        free(execIrHeaderText);
        free(execIrSourceText);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_semir_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves SemIR Metadata";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("SemIR binary roundtrip",
              "Testing that .zro roundtrip keeps SemIR instructions, ownership effects, and type table metadata available to runtime");

    {
        SZrState *state = create_test_state();
        const char *source =
                "class Box {}\n"
                "var owner = %unique new Box();\n"
                "var alias = %shared(owner);\n"
                "var watcher = %weak(alias);";
        const char *binaryPath = "semir_roundtrip_test.zro";
        SZrString *sourceName;
        SZrFunction *func;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_Create(state, "semir_roundtrip_test.zr", 23);
        TEST_ASSERT_NOT_NULL(sourceName);
        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, func, binaryPath));

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(runtimeFunction->semIrInstructionLength > 0);
        TEST_ASSERT_TRUE(runtimeFunction->semIrEffectTableLength > 0);
        TEST_ASSERT_TRUE(runtimeFunction->semIrTypeTableLength > 0);
        TEST_ASSERT_TRUE(runtimeFunction->semIrBlockTableLength > 0);
        TEST_ASSERT_TRUE(function_contains_semir_opcode(runtimeFunction, ZR_SEMIR_OPCODE_OWN_UNIQUE));
        TEST_ASSERT_EQUAL_UINT32(ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION,
                                 runtimeFunction->semIrEffectTable[0].kind);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Function_Free(state, func);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_intermediate_writer_emits_semir_sections);
    RUN_TEST(test_ownership_builtins_lower_to_ownership_opcodes);
    RUN_TEST(test_struct_value_type_places_emit_semir_metadata);
    RUN_TEST(test_struct_value_type_call_and_return_emit_semir_metadata);
    RUN_TEST(test_aot_execir_source_exposes_inline_frame_byte_layout);
    RUN_TEST(test_binary_roundtrip_preserves_semir_metadata);
    return UNITY_END();
}
