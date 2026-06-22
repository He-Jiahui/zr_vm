#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
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

static TZrInstruction create_get_global_instruction(TZrUInt16 destinationSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_GLOBAL);
    instruction.instruction.operandExtra = destinationSlot;
    return instruction;
}

static TZrInstruction create_get_sub_function_instruction(TZrUInt16 destinationSlot, TZrUInt16 childFunctionIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = childFunctionIndex;
    return instruction;
}

static TZrInstruction create_typeof_instruction(TZrUInt16 destinationSlot, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(TYPEOF);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static TZrInstruction create_create_object_instruction(TZrUInt16 destinationSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(CREATE_OBJECT);
    instruction.instruction.operandExtra = destinationSlot;
    return instruction;
}

static TZrInstruction create_create_array_instruction(TZrUInt16 destinationSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(CREATE_ARRAY);
    instruction.instruction.operandExtra = destinationSlot;
    return instruction;
}

static TZrInstruction create_to_string_instruction(TZrUInt16 destinationSlot, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(TO_STRING);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static TZrInstruction create_to_object_instruction(TZrUInt16 destinationSlot,
                                                   TZrUInt16 sourceSlot,
                                                   TZrUInt16 typeNameConstantIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(TO_OBJECT);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    instruction.instruction.operand.operand1[1] = typeNameConstantIndex;
    return instruction;
}

static TZrInstruction create_to_struct_instruction(TZrUInt16 destinationSlot,
                                                   TZrUInt16 sourceSlot,
                                                   TZrUInt16 typeNameConstantIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(TO_STRUCT);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    instruction.instruction.operand.operand1[1] = typeNameConstantIndex;
    return instruction;
}

static TZrInstruction create_get_closure_instruction(TZrUInt16 destinationSlot, TZrInt32 closureIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_CLOSURE);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand2[0] = closureIndex;
    return instruction;
}

static TZrInstruction create_set_closure_instruction(TZrUInt16 sourceSlot, TZrInt32 closureIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SET_CLOSURE);
    instruction.instruction.operandExtra = sourceSlot;
    instruction.instruction.operand.operand2[0] = closureIndex;
    return instruction;
}

static TZrInstruction create_get_upval_instruction(TZrUInt16 destinationSlot, TZrUInt16 closureIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GETUPVAL);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = closureIndex;
    return instruction;
}

static TZrInstruction create_set_upval_instruction(TZrUInt16 sourceSlot, TZrUInt16 closureIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SETUPVAL);
    instruction.instruction.operandExtra = sourceSlot;
    instruction.instruction.operand.operand1[0] = closureIndex;
    return instruction;
}

static TZrInstruction create_meta_access_instruction(TZrUInt16 opcode,
                                                     TZrUInt16 destinationSlot,
                                                     TZrUInt16 operandSlot,
                                                     TZrUInt16 memberOrCacheIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = operandSlot;
    instruction.instruction.operand.operand1[1] = memberOrCacheIndex;
    return instruction;
}

static TZrInstruction create_dynamic_value_access_instruction(TZrUInt16 opcode,
                                                              TZrUInt16 destinationSlot,
                                                              TZrUInt16 operandSlot,
                                                              TZrUInt16 operandIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = operandSlot;
    instruction.instruction.operand.operand1[1] = operandIndex;
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

static SZrFunction *create_get_global_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_global_instruction(0u);
    function->instructionsList[1] = create_return_instruction(1u, 0u);
    function->instructionsLength = 2u;

    function->stackSize = 1u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_get_sub_function_native_closure_function(SZrState *state) {
    SZrFunction *function;
    SZrFunction *childFunction;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_sub_function_instruction(0u, 0u);
    function->instructionsList[1] = create_return_instruction(1u, 0u);
    function->instructionsLength = 2u;

    function->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->childFunctionList);
    memset(function->childFunctionList, 0, sizeof(SZrFunction));
    function->childFunctionLength = 1u;

    childFunction = &function->childFunctionList[0];
    childFunction->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(childFunction->instructionsList);
    childFunction->instructionsList[0] = create_return_instruction(0u, 0u);
    childFunction->instructionsLength = 1u;
    childFunction->stackSize = 0u;
    childFunction->parameterCount = 0u;
    childFunction->hasVariableArguments = ZR_FALSE;
    childFunction->closureValueLength = 0u;

    function->stackSize = 1u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_typeof_global_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_global_instruction(0u);
    function->instructionsList[1] = create_typeof_instruction(1u, 0u);
    function->instructionsList[2] = create_return_instruction(1u, 1u);
    function->instructionsLength = 3u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_object_array_creation_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_create_object_instruction(0u);
    function->instructionsList[1] = create_create_array_instruction(1u);
    function->instructionsList[2] = create_return_instruction(1u, 1u);
    function->instructionsLength = 3u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static void set_function_type_name_constant(SZrState *state, SZrFunction *function, TZrNativeString typeName) {
    SZrString *typeNameString;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(typeName);

    typeNameString = ZrCore_String_Create(state, typeName, strlen(typeName));
    TEST_ASSERT_NOT_NULL(typeNameString);

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 1u;

    ZrCore_Value_ResetAsNull(&function->constantValueList[0]);
    ZrCore_Value_InitAsRawObject(state,
                                 &function->constantValueList[0],
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(typeNameString));
    function->constantValueList[0].type = ZR_VALUE_TYPE_STRING;
}

static SZrFunction *create_object_struct_conversion_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_global_instruction(0u);
    function->instructionsList[1] = create_to_object_instruction(1u, 0u, 0u);
    function->instructionsList[2] = create_to_struct_instruction(2u, 1u, 0u);
    function->instructionsList[3] = create_return_instruction(1u, 2u);
    function->instructionsLength = 4u;

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    set_function_type_name_constant(state, function, "AotMissingType");
    return function;
}

static SZrFunction *create_to_string_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_global_instruction(0u);
    function->instructionsList[1] = create_to_string_instruction(1u, 0u);
    function->instructionsList[2] = create_return_instruction(1u, 1u);
    function->instructionsLength = 3u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_closure_value_access_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 5u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_closure_instruction(0u, 0);
    function->instructionsList[1] = create_set_closure_instruction(0u, 0);
    function->instructionsList[2] = create_get_upval_instruction(1u, 0u);
    function->instructionsList[3] = create_set_upval_instruction(1u, 0u);
    function->instructionsList[4] = create_return_instruction(1u, 1u);
    function->instructionsLength = 5u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 1u;
    return function;
}

static SZrFunction *create_meta_value_access_boundary_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_meta_access_instruction(ZR_INSTRUCTION_ENUM(META_GET), 0u, 1u, 7u);
    function->instructionsList[1] = create_meta_access_instruction(ZR_INSTRUCTION_ENUM(META_SET), 1u, 0u, 7u);
    function->instructionsList[2] =
            create_meta_access_instruction(ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED), 0u, 1u, 2u);
    function->instructionsList[3] =
            create_meta_access_instruction(ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED), 1u, 0u, 3u);
    function->instructionsList[4] =
            create_meta_access_instruction(ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED), 0u, 1u, 4u);
    function->instructionsList[5] =
            create_meta_access_instruction(ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED), 1u, 0u, 5u);
    function->instructionsList[6] = create_return_instruction(1u, 0u);
    function->instructionsLength = 7u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_dynamic_value_access_boundary_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] =
            create_dynamic_value_access_instruction(ZR_INSTRUCTION_ENUM(GET_MEMBER), 0u, 1u, 7u);
    function->instructionsList[1] =
            create_dynamic_value_access_instruction(ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT), 0u, 1u, 2u);
    function->instructionsList[2] =
            create_dynamic_value_access_instruction(ZR_INSTRUCTION_ENUM(GET_BY_INDEX), 0u, 1u, 2u);
    function->instructionsList[3] =
            create_dynamic_value_access_instruction(ZR_INSTRUCTION_ENUM(SET_MEMBER), 1u, 0u, 7u);
    function->instructionsList[4] =
            create_dynamic_value_access_instruction(ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT), 1u, 0u, 3u);
    function->instructionsList[5] =
            create_dynamic_value_access_instruction(ZR_INSTRUCTION_ENUM(SET_BY_INDEX), 1u, 0u, 2u);
    function->instructionsList[6] = create_return_instruction(1u, 0u);
    function->instructionsLength = 7u;

    function->stackSize = 2u;
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

static void test_aot_c_generated_shared_library_compiles_get_global_runtime_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C GET_GLOBAL shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_get_global_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_get_global_smoke";
    options.sourceHash = "get-global-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "get-global-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_get_global_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_get_global_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "#include \"zr_vm_library/aot_runtime.h\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_get_global"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetGlobal(state, &frame, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const SZrTypeValue *zr_aot_global_object"));
    TEST_ASSERT_NULL(strstr(generatedCText, "state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrCore_Value_Copy(state, zr_aot_destination, &state->global->zrObject);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_ResetAsNull(zr_aot_destination);"));
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

static void test_aot_c_generated_shared_library_compiles_get_sub_function_native_closure_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C GET_SUB_FUNCTION native-closure shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_get_sub_function_native_closure_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_get_sub_function_native_closure_smoke";
    options.sourceHash = "get-sub-function-native-closure-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "get-sub-function-native-closure-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_get_sub_function_native_closure_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_get_sub_function_native_closure_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_get_sub_function_native_closure_boundary"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_fn_1));"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_value_exec_get_sub_function_native_closure"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_ClosureNative_New(state, 0);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_closure->nativeFunction = zr_aot_fn_"));
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

static void test_aot_c_generated_shared_library_compiles_typeof_runtime_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C TYPEOF shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_typeof_global_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_typeof_smoke";
    options.sourceHash = "typeof-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "typeof-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_typeof_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_typeof_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "#include \"zr_vm_library/aot_runtime.h\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_typeof"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_TypeOf(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrCore_Reflection_TypeOfValue(state, zr_aot_source, zr_aot_destination)"));
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

static void test_aot_c_generated_shared_library_compiles_object_array_creation_runtime_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C CREATE_OBJECT/CREATE_ARRAY shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_object_array_creation_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_object_array_creation_smoke";
    options.sourceHash = "object-array-creation-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "object-array-creation-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_object_array_creation_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_object_array_creation_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "#include \"zr_vm_library/aot_runtime.h\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_create_object"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_create_array"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CreateObject(state, &frame, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CreateArray(state, &frame, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_New(state, ZR_NULL)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_NewCustomized(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_destination->type = ZR_VALUE_TYPE_ARRAY;"));
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

static void test_aot_c_generated_shared_library_compiles_object_struct_conversions_runtime_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C TO_OBJECT/TO_STRUCT shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_object_struct_conversion_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_object_struct_conversion_smoke";
    options.sourceHash = "object-struct-conversion-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "object-struct-conversion-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_object_struct_conversion_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_object_struct_conversion_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "#include \"zr_vm_library/aot_runtime.h\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_to_object"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_to_struct"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToObject(state, &frame, 1, 0, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToStruct(state, &frame, 2, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Execution_ToObject(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Execution_ToStruct(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const SZrTypeValue *zr_aot_type_name"));
    TEST_ASSERT_NULL(strstr(generatedCText, "&frame.function->constantValueList"));
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

static void test_aot_c_generated_shared_library_compiles_closure_value_runtime_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C closure-value shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_closure_value_access_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_closure_value_access_smoke";
    options.sourceHash = "closure-value-access-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "closure-value-access-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_closure_value_access_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_closure_value_access_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_get_closure_value"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_set_closure_value"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetClosureValue(state, &frame, 0, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SetClosureValue(state, &frame, 0, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetClosureValue(state, &frame, 1, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SetClosureValue(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_ClosureNative_GetCaptureValue"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_ClosureValue_GetValue"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_Barrier(state, zr_aot_barrier_object, zr_aot_source);"));
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

static void test_aot_c_generated_shared_library_compiles_dynamic_value_access_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C dynamic value-access boundary shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_dynamic_value_access_boundary_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_dynamic_value_access_boundary_smoke";
    options.sourceHash = "dynamic-value-access-boundary-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "dynamic-value-access-boundary-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_dynamic_value_access_boundary_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_dynamic_value_access_boundary_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_unsupported_dynamic_value_access"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "GET_MEMBER"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "SET_BY_INDEX"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const char *zr_aot_opcode_name"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_primary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_secondary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const TZrUInt32 zr_aot_operand_index"));
    TEST_ASSERT_NULL(
            strstr(generatedCText, "ZrCore_Debug_RunError(state, \"unsupported AOT dynamic value access: %s\""));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetMember(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetMemberSlot(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetByIndex(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SetMember(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SetMemberSlot(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SetByIndex(state, &frame"));
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

static void test_aot_c_generated_shared_library_compiles_meta_value_access_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C meta-value boundary shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_meta_value_access_boundary_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_meta_value_access_boundary_smoke";
    options.sourceHash = "meta-value-access-boundary-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "meta-value-access-boundary-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_meta_value_access_boundary_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_meta_value_access_boundary_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_unsupported_meta_value_access"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "META_GET"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "SUPER_META_SET_STATIC_CACHED"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const char *zr_aot_opcode_name"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_primary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrTypeValue *zr_aot_secondary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const TZrUInt32 zr_aot_member_or_cache_index"));
    TEST_ASSERT_NULL(
            strstr(generatedCText, "ZrCore_Debug_RunError(state, \"unsupported AOT meta value access: %s\""));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_MetaGet(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_MetaSet(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_MetaGetCached(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_MetaSetCached(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_MetaGetStaticCached(state, &frame"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_MetaSetStaticCached(state, &frame"));
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

static void test_aot_c_generated_shared_library_compiles_to_string_runtime_boundary(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C TO_STRING shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_to_string_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_to_string_smoke";
    options.sourceHash = "to-string-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "to-string-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "src",
                                                       "aot_c_to_string_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_global_shared_library",
                                                       "lib",
                                                       "libaot_c_to_string_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "#include \"zr_vm_library/aot_runtime.h\""));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_to_string"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToString(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_ConvertToString(state, zr_aot_source)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase = zr_aot_call_info->functionBase.valuePointer + 1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_InitAsRawObject(state, zr_aot_destination"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_ResetAsNull(zr_aot_destination);"));
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
    RUN_TEST(test_aot_c_generated_shared_library_compiles_get_global_runtime_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_get_sub_function_native_closure_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_typeof_runtime_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_object_array_creation_runtime_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_object_struct_conversions_runtime_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_closure_value_runtime_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_dynamic_value_access_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_meta_value_access_boundary);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_to_string_runtime_boundary);
    return UNITY_END();
}
