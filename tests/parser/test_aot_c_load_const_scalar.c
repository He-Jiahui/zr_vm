#include "unity.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/value.h"
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

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    TZrBytePtr bytes = ZR_NULL;
    TZrSize byteLength = 0u;
    char *text;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &byteLength));
    TEST_ASSERT_NOT_NULL(bytes);

    text = (char *)malloc(byteLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    memcpy(text, bytes, byteLength);
    text[byteLength] = '\0';
    free(bytes);
    return text;
}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), needle);
}

static void assert_text_does_not_contain(const char *text, const char *needle) {
    const char *found;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);

    found = strstr(text, needle);
    if (found != ZR_NULL) {
        printf("Generated C still contains forbidden token '%s' at byte offset %td\n",
               needle,
               (ptrdiff_t)(found - text));
        TEST_FAIL_MESSAGE("generated load-const scalar AOT C contains forbidden runtime token");
    }
}

static void init_i64_type_ref(SZrFunctionTypedTypeRef *typeRef) {
    TEST_ASSERT_NOT_NULL(typeRef);
    memset(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_INT64;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->staticCType = ZR_STATIC_C_TYPE_I64;
    typeRef->staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

static TZrInstruction make_get_constant_instruction(TZrUInt16 destinationSlot, TZrInt32 constantIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_CONSTANT);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand2[0] = constantIndex;
    return instruction;
}

static TZrInstruction make_get_stack_instruction(TZrUInt16 destinationSlot, TZrInt32 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_STACK);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand2[0] = sourceSlot;
    return instruction;
}

static TZrInstruction make_signed_load_const_instruction(EZrInstructionCode opcode,
                                                         TZrUInt16 destinationSlot,
                                                         TZrUInt16 leftSlot,
                                                         TZrUInt16 materializedSlot,
                                                         TZrUInt16 constantIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand0[0] = (TZrUInt8)leftSlot;
    instruction.instruction.operand.operand0[1] = (TZrUInt8)materializedSlot;
    instruction.instruction.operand.operand1[1] = constantIndex;
    return instruction;
}

static TZrInstruction make_signed_load_stack_load_const_instruction(TZrUInt16 destinationSlot,
                                                                    TZrUInt16 leftSlot,
                                                                    TZrUInt16 materializedStackSlot,
                                                                    TZrUInt16 materializedConstSlot,
                                                                    TZrUInt16 constantIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand0[0] = (TZrUInt8)leftSlot;
    instruction.instruction.operand.operand0[1] = (TZrUInt8)materializedStackSlot;
    instruction.instruction.operand.operand0[2] = (TZrUInt8)materializedConstSlot;
    instruction.instruction.operand.operand1[1] = constantIndex;
    return instruction;
}

static TZrInstruction make_return_instruction(TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = 1u;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static SZrSemIrInstruction make_semir_instruction(TZrUInt32 opcode,
                                                  TZrUInt32 execInstructionIndex,
                                                  TZrUInt32 destinationSlot) {
    SZrSemIrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = opcode;
    instruction.execInstructionIndex = execInstructionIndex;
    instruction.typeTableIndex = 0u;
    instruction.destinationSlot = destinationSlot;
    return instruction;
}

static void init_i64_constant(SZrTypeValue *value, TZrInt64 intValue) {
    TEST_ASSERT_NOT_NULL(value);
    ZrCore_Value_ResetAsNull(value);
    ZR_VALUE_FAST_SET(value, nativeInt64, intValue, ZR_VALUE_TYPE_INT64);
}

static SZrFunction *create_signed_load_const_scalar_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsLength = 8u;
    function->stackSize = 8u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    function->hasCallableReturnType = ZR_TRUE;
    init_i64_type_ref(&function->callableReturnType);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * function->instructionsLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = make_get_constant_instruction(0u, 0);
    function->instructionsList[1] =
            make_signed_load_const_instruction(ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST), 1u, 0u, 2u, 1u);
    function->instructionsList[2] = make_get_stack_instruction(3u, 1);
    function->instructionsList[3] =
            make_signed_load_const_instruction(ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST), 4u, 1u, 3u, 2u);
    function->instructionsList[4] = make_get_stack_instruction(5u, 4);
    function->instructionsList[5] = make_get_constant_instruction(6u, 6);
    function->instructionsList[6] =
            make_signed_load_stack_load_const_instruction(7u, 4u, 5u, 6u, 6u);
    function->instructionsList[7] = make_return_instruction(7u);

    function->constantValueLength = 7u;
    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * function->constantValueLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    init_i64_constant(&function->constantValueList[0], 10);
    init_i64_constant(&function->constantValueList[1], 5);
    init_i64_constant(&function->constantValueList[2], 3);
    init_i64_constant(&function->constantValueList[3], 7);
    init_i64_constant(&function->constantValueList[4], 0);
    init_i64_constant(&function->constantValueList[5], 0);
    init_i64_constant(&function->constantValueList[6], 7);

    function->semIrTypeTableLength = 1u;
    function->semIrTypeTable = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedTypeRef),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->semIrTypeTable);
    init_i64_type_ref(&function->semIrTypeTable[0]);

    function->semIrInstructionLength = 3u;
    function->semIrInstructions = (SZrSemIrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrSemIrInstruction) * function->semIrInstructionLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->semIrInstructions);
    function->semIrInstructions[0] = make_semir_instruction(ZR_SEMIR_OPCODE_ADD, 1u, 1u);
    function->semIrInstructions[1] = make_semir_instruction(ZR_SEMIR_OPCODE_SUB, 3u, 4u);
    function->semIrInstructions[2] = make_semir_instruction(ZR_SEMIR_OPCODE_ADD, 6u, 7u);

    return function;
}

static void test_aot_c_signed_load_const_fusion_elides_frame_and_value_materialization(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C load-const scalar smoke currently validates the Unix shared-library toolchain path");
#else
    static const TZrByte embeddedBlob[] = {0x7a, 0x72, 0x6f};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_signed_load_const_scalar_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_load_const_scalar";
    options.sourceHash = "load-const-scalar-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "load-const-scalar-smoke";
    options.embeddedModuleBlob = embeddedBlob;
    options.embeddedModuleBlobLength = sizeof(embeddedBlob);
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_load_const_scalar",
                                                       "src",
                                                       "aot_c_load_const_scalar",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_load_const_scalar",
                                                       "lib",
                                                       "libaot_c_load_const_scalar",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_text_does_not_contain(generatedCText, "/* zr_aot_generated_frame_setup */");
    assert_text_does_not_contain(generatedCText, "ZrAotGeneratedFrame frame = {0};");
    assert_text_does_not_contain(generatedCText, "frame.slotBase");
    assert_text_does_not_contain(generatedCText, "ZrCore_Stack_GetValue(");
    assert_text_does_not_contain(generatedCText, "ZR_VALUE_FAST_SET(");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s0 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s1 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s3 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s4 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s5 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s6 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "TZrInt64 zr_aot_s7 = (TZrInt64)0;");
    assert_text_contains(generatedCText, "zr_aot_s0 = (TZrInt64)10;");
    assert_text_contains(generatedCText, "zr_aot_s1 = zr_aot_s0 + (TZrInt64)5;");
    assert_text_contains(generatedCText, "/* zr_aot_scalar_stack_copy_i64 dstSlot=3 srcSlot=1 */");
    assert_text_contains(generatedCText, "zr_aot_s4 = zr_aot_s1 - (TZrInt64)3;");
    assert_text_contains(generatedCText, "/* zr_aot_scalar_stack_copy_i64 dstSlot=5 srcSlot=4 */");
    assert_text_contains(generatedCText, "zr_aot_s6 = (TZrInt64)7;");
    assert_text_contains(generatedCText, "zr_aot_s7 = zr_aot_s4 + (TZrInt64)7;");
    assert_text_contains(generatedCText, "/* zr_aot_direct_return_i64_local */");
    assert_text_contains(generatedCText, "ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s7)");
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

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_signed_load_const_fusion_elides_frame_and_value_materialization);
    return UNITY_END();
}
