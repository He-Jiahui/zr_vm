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

static TZrInstruction create_iterator_instruction(EZrInstructionCode opcode,
                                                  TZrUInt16 destinationSlot,
                                                  TZrUInt16 operandA,
                                                  TZrUInt16 operandB) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static TZrInstruction create_jump_instruction(EZrInstructionCode opcode,
                                              TZrUInt16 destinationSlot,
                                              TZrUInt16 operandA,
                                              TZrUInt16 operandB) {
    return create_iterator_instruction(opcode, destinationSlot, operandA, operandB);
}

static TZrInstruction create_return_instruction(TZrUInt16 returnCount, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = returnCount;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static SZrFunction *create_iterator_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 6u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_INIT), 2u, 0u, 0u);
    function->instructionsList[1] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT), 3u, 2u, 0u);
    function->instructionsList[2] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_CURRENT), 4u, 2u, 0u);
    function->instructionsList[3] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(DYN_ITER_INIT), 2u, 0u, 0u);
    function->instructionsList[4] =
            create_jump_instruction(ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE), 3u, 2u, 0u);
    function->instructionsList[5] = create_return_instruction(1u, 4u);
    function->instructionsLength = 6u;
    function->stackSize = 5u;
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

static void test_aot_c_generated_shared_library_compiles_iterator_boundary_helper_lowering(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C iterator shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_iterator_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_iterator_smoke";
    options.sourceHash = "iterator-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "iterator-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       "aot_c_iterator_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "lib",
                                                       "libaot_c_iterator_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "#include \"zr_vm_library/aot_runtime.h\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_iter_init"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_iter_move_next"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_iter_current"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_iter_move_next_jump_if_false"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_IterInit(state, &frame, 2, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_IterMoveNext(state, &frame, 3, 2)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_IterCurrent(state, &frame, 4, 2)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(state, &frame, 3, 2, &zr_aot_branch_taken)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "goto zr_aot_fn_0_ins_5;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_IterInit(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_TryIterMoveNextCachedArrayFast(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_IterMoveNext(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_TryIterCurrentCachedMemberFastStackResult(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_IterCurrent(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_IsTruthy"));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT iterator core path"));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT iterator branch result"));
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

static void test_aot_c_full_aot_rejects_dynamic_iterator_deopt_bridge(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C iterator shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = create_iterator_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_full_aot_iterator_deopt_rejected";
    options.sourceHash = "full-aot-iterator-deopt-rejected";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "full-aot-iterator-deopt-rejected";
    options.requireExecutableLowering = ZR_TRUE;
    options.requireFullAot = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       "full_aot_iterator_deopt_rejected",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    remove(generatedCPath);

    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_compiles_iterator_boundary_helper_lowering);
    RUN_TEST(test_aot_c_full_aot_rejects_dynamic_iterator_deopt_bridge);
    return UNITY_END();
}
