#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler_internal.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/writer.h"

#ifndef ZR_VM_TESTS_C_COMPILER
#define ZR_VM_TESTS_C_COMPILER "cc"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

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
#endif

void setUp(void) {}

void tearDown(void) {}

static TZrInstruction create_return_instruction(TZrUInt16 returnCount, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = returnCount;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static TZrInstruction create_dynamic_call_instruction(TZrUInt16 destinationSlot,
                                                      TZrUInt16 functionSlot,
                                                      TZrUInt16 argumentCount) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_CALL);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = functionSlot;
    instruction.instruction.operand.operand1[1] = argumentCount;
    return instruction;
}

static TZrInstruction create_dynamic_member_get_instruction(TZrUInt16 destinationSlot,
                                                            TZrUInt16 receiverSlot,
                                                            TZrUInt16 memberEntryIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_MEMBER);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = receiverSlot;
    instruction.instruction.operand.operand1[1] = memberEntryIndex;
    return instruction;
}

static TZrInstruction create_dynamic_index_get_instruction(TZrUInt16 destinationSlot,
                                                           TZrUInt16 receiverSlot,
                                                           TZrUInt16 keySlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_BY_INDEX);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = receiverSlot;
    instruction.instruction.operand.operand1[1] = keySlot;
    return instruction;
}

static SZrFunction *create_dynamic_deopt_boundary_function(SZrState *state,
                                                           TZrInstruction boundaryInstruction,
                                                           TZrUInt16 returnSlot,
                                                           TZrUInt32 stackSize) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = boundaryInstruction;
    function->instructionsList[1] = create_return_instruction(1u, returnSlot);
    function->instructionsLength = 2u;

    function->stackSize = stackSize;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_dynamic_deopt_call_boundary_function(SZrState *state) {
    return create_dynamic_deopt_boundary_function(state, create_dynamic_call_instruction(6u, 2u, 3u), 6u, 8u);
}

static SZrFunction *create_dynamic_member_deopt_boundary_function(SZrState *state) {
    return create_dynamic_deopt_boundary_function(state, create_dynamic_member_get_instruction(5u, 1u, 2u), 5u, 8u);
}

static SZrFunction *create_dynamic_index_deopt_boundary_function(SZrState *state) {
    return create_dynamic_deopt_boundary_function(state, create_dynamic_index_get_instruction(5u, 1u, 2u), 5u, 8u);
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

#if defined(ZR_PLATFORM_UNIX)
static void assert_generated_source_compiles(const TZrChar *generatedCPath, const TZrChar *sharedLibraryPath) {
    char command[4096];

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -g -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
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
}
#endif

static void test_aot_c_generated_shared_library_compiles_semir_dynamic_deopt_bridge(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C dynamic deopt bridge smoke currently validates the Unix shared-library toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_dynamic_deopt_call_boundary_function(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(compiler_build_function_semir_metadata(state, function));
    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrDeoptTableLength);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_deopt_bridge_smoke";
    options.sourceHash = "dynamic-deopt-bridge-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "dynamic-deopt-bridge-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_dynamic_deopt_bridge",
                                                       "runtime_project/bin/aot_c/src",
                                                       "dynamic_deopt_bridge",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_dynamic_deopt_bridge",
                                                       "runtime_project/bin/aot_c/lib",
                                                       "zrvm_aot_dynamic_deopt_bridge",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "DYN_CALL"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_dynamic_deopt_bridge deopt="));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_direct_function_call */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CallStackValue(state,"));
    free(generatedCText);

    assert_generated_source_compiles(generatedCPath, sharedLibraryPath);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_dynamic_value_access_deopt_bridges(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C dynamic value-access deopt bridge smoke currently validates the Unix shared-library toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *memberFunction;
    SZrFunction *indexFunction;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char expectedDeoptMarker[64];

    TEST_ASSERT_NOT_NULL(state);

    memberFunction = create_dynamic_member_deopt_boundary_function(state);
    TEST_ASSERT_NOT_NULL(memberFunction);
    TEST_ASSERT_TRUE(compiler_build_function_semir_metadata(state, memberFunction));
    TEST_ASSERT_EQUAL_UINT32(1u, memberFunction->semIrDeoptTableLength);
    snprintf(expectedDeoptMarker,
             sizeof(expectedDeoptMarker),
             "zr_aot_value_dynamic_deopt_bridge deopt=%u",
             (unsigned)memberFunction->semIrDeoptTable[0].deoptId);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_member_deopt_bridge_smoke";
    options.sourceHash = "dynamic-member-deopt-bridge-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "dynamic-member-deopt-bridge-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_dynamic_deopt_bridge",
                                                       "runtime_project/bin/aot_c/src",
                                                       "dynamic_member_deopt_bridge",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_dynamic_deopt_bridge",
                                                       "runtime_project/bin/aot_c/lib",
                                                       "zrvm_aot_dynamic_member_deopt_bridge",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, memberFunction, generatedCPath, &options));
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_dynamic_get_member_boundary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, expectedDeoptMarker));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetMember(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(state,"));
    free(generatedCText);
    assert_generated_source_compiles(generatedCPath, sharedLibraryPath);

    indexFunction = create_dynamic_index_deopt_boundary_function(state);
    TEST_ASSERT_NOT_NULL(indexFunction);
    TEST_ASSERT_TRUE(compiler_build_function_semir_metadata(state, indexFunction));
    TEST_ASSERT_EQUAL_UINT32(1u, indexFunction->semIrDeoptTableLength);
    snprintf(expectedDeoptMarker,
             sizeof(expectedDeoptMarker),
             "zr_aot_value_dynamic_deopt_bridge deopt=%u",
             (unsigned)indexFunction->semIrDeoptTable[0].deoptId);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_index_deopt_bridge_smoke";
    options.sourceHash = "dynamic-index-deopt-bridge-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "dynamic-index-deopt-bridge-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_dynamic_deopt_bridge",
                                                       "runtime_project/bin/aot_c/src",
                                                       "dynamic_index_deopt_bridge",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_dynamic_deopt_bridge",
                                                       "runtime_project/bin/aot_c/lib",
                                                       "zrvm_aot_dynamic_index_deopt_bridge",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, indexFunction, generatedCPath, &options));
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_dynamic_get_by_index_boundary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, expectedDeoptMarker));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetByIndex(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(state,"));
    free(generatedCText);
    assert_generated_source_compiles(generatedCPath, sharedLibraryPath);

    ZrCore_Function_Free(state, memberFunction);
    ZrCore_Function_Free(state, indexFunction);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_compiles_semir_dynamic_deopt_bridge);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_dynamic_value_access_deopt_bridges);
    return UNITY_END();
}
