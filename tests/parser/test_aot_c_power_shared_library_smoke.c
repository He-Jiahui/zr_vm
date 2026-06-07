#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/writer.h"
#endif

#ifndef ZR_VM_TESTS_C_COMPILER
#define ZR_VM_TESTS_C_COMPILER "cc"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

void setUp(void) {}

void tearDown(void) {}

#if defined(ZR_PLATFORM_UNIX)
static int run_command_expect_success(const char *command) {
    int result;

    TEST_ASSERT_NOT_NULL(command);
    result = system(command);
    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
}

static TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 destinationSlot, TZrInt32 operand) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

static TZrInstruction create_instruction_2(EZrInstructionCode opcode,
                                           TZrUInt16 destinationSlot,
                                           TZrUInt16 leftSlot,
                                           TZrUInt16 rightSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = leftSlot;
    instruction.instruction.operand.operand1[1] = rightSlot;
    return instruction;
}

static TZrInstruction create_return_instruction(TZrUInt16 returnCount, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = returnCount;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static SZrFunction *create_typed_power_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 13u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(POW_SIGNED), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4u, 3);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(POW_UNSIGNED), 5u, 3u, 4u);
    function->instructionsList[6] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 6u, 4);
    function->instructionsList[7] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 7u, 5);
    function->instructionsList[8] = create_instruction_2(ZR_INSTRUCTION_ENUM(POW_FLOAT), 8u, 6u, 7u);
    function->instructionsList[9] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 9u, 0);
    function->instructionsList[10] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 10u, 1);
    function->instructionsList[11] = create_instruction_2(ZR_INSTRUCTION_ENUM(POW), 11u, 9u, 10u);
    function->instructionsList[12] = create_return_instruction(1u, 8u);
    function->instructionsLength = 13u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 6u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 3);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 4);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[2], 2u);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[3], 5u);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[4], 2.0);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[5], 3.0);
    function->constantValueLength = 6u;

    function->stackSize = 12u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    TEST_ASSERT_NOT_NULL(path);
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    fileSize = ftell(file);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(0, fileSize);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));

    buffer = (char *)malloc((size_t)fileSize + 1u);
    TEST_ASSERT_NOT_NULL(buffer);
    if (fileSize > 0) {
        TEST_ASSERT_EQUAL_size_t((size_t)fileSize, fread(buffer, 1, (size_t)fileSize, file));
    }
    buffer[fileSize] = '\0';
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    return buffer;
}
#endif

static void test_aot_c_generated_shared_library_compiles_typed_power(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C typed power shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_typed_power_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_typed_power_smoke";
    options.sourceHash = "typed-power-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "typed-power-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_power_shared_library",
                                                       "src",
                                                       "aot_c_typed_power_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_power_shared_library",
                                                       "lib",
                                                       "libaot_c_typed_power_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_arith_exec_signed_power"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_arith_exec_unsigned_power"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_arith_exec_float_power"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_power"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_GetMeta(state, zr_aot_left, ZR_META_POW)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_ResetAsNull(zr_aot_destination)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "unsupported AOT generic power meta dispatch"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_power_result *= zr_aot_power_base"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "pow(zr_aot_left_scalar, zr_aot_right_scalar)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Pow(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_PowSigned(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_PowUnsigned(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_PowFloat(state, &frame"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_compiles_typed_power);
    return UNITY_END();
}
