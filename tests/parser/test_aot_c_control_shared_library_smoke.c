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

static TZrInstruction create_handler_instruction(EZrInstructionCode opcode, TZrUInt16 handlerIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = handlerIndex;
    return instruction;
}

static TZrInstruction create_slot_instruction(EZrInstructionCode opcode, TZrUInt16 slotIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = slotIndex;
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

static SZrFunction *create_try_throw_end_try_catch_end_finally_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 6u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_handler_instruction(ZR_INSTRUCTION_ENUM(TRY), 0u);
    function->instructionsList[1] = create_slot_instruction(ZR_INSTRUCTION_ENUM(THROW), 0u);
    function->instructionsList[2] = create_handler_instruction(ZR_INSTRUCTION_ENUM(END_TRY), 0u);
    function->instructionsList[3] = create_slot_instruction(ZR_INSTRUCTION_ENUM(CATCH), 0u);
    function->instructionsList[4] = create_handler_instruction(ZR_INSTRUCTION_ENUM(END_FINALLY), 0u);
    function->instructionsList[5] = create_return_instruction(1u, 0u);
    function->instructionsLength = 6u;

    function->exceptionHandlerList = (SZrFunctionExceptionHandlerInfo *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionExceptionHandlerInfo),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->exceptionHandlerList);
    memset(function->exceptionHandlerList, 0, sizeof(SZrFunctionExceptionHandlerInfo));
    function->exceptionHandlerCount = 1u;

    function->stackSize = 1u;
    function->parameterCount = 1u;
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

static void test_aot_c_generated_shared_library_compiles_direct_try_end_try_lowering(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C control shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_try_throw_end_try_catch_end_finally_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_control_smoke";
    options.sourceHash = "control-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "control-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_control_shared_library",
                                                       "src",
                                                       "aot_c_control_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_control_shared_library",
                                                       "lib",
                                                       "libaot_c_control_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_try_direct"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "execution_push_exception_handler(state, zr_aot_call_info, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_throw_direct"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Exception_NormalizeThrownValue(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "execution_unwind_exception_to_handler(state, &zr_aot_call_info)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Exception_Throw(state, state->currentExceptionStatus);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_end_try_direct"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "execution_find_handler_state(state, zr_aot_call_info, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "execution_pop_exception_handler(state, handlerState);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_catch_direct"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_Copy(state, zr_aot_destination, &state->currentException);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Exception_ClearCurrent(state);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_ResetAsNull(zr_aot_destination);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "execution_clear_pending_control(state);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_end_finally_direct"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "switch (state->pendingControl.kind)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case ZR_VM_PENDING_CONTROL_EXCEPTION:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "case ZR_VM_PENDING_CONTROL_RETURN:"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "execution_resume_pending_via_outer_finally(state, &zr_aot_call_info)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Try"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_EndTry"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Throw"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_Catch"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_EndFinally"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
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
    RUN_TEST(test_aot_c_generated_shared_library_compiles_direct_try_end_try_lowering);
    return UNITY_END();
}
