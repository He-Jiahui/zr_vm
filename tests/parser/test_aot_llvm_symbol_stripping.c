#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/writer.h"

void setUp(void) {}

void tearDown(void) {}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL(strstr(text, needle));
}

static void assert_text_does_not_contain(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL(strstr(text, needle));
}

static TZrInstruction test_create_instruction_2(EZrInstructionCode opcode,
                                                TZrUInt16 operandExtra,
                                                TZrUInt16 operandA,
                                                TZrUInt16 operandB) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static SZrFunction *create_llvm_direct_call_fixture(SZrState *state) {
    SZrFunction *root;
    SZrFunction *child;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);

    root->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->instructionsList);
    root->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 1u, 0u, 0u);
    root->instructionsList[1] =
            test_create_instruction_2(ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0u, 1u, 0u);
    root->instructionsList[2] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 0u, 0u);
    root->instructionsLength = 3u;
    root->stackSize = 2u;
    root->parameterCount = 0u;
    root->lineInSourceStart = 1u;
    root->lineInSourceEnd = 3u;

    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction));
    root->childFunctionLength = 1u;

    child = &root->childFunctionList[0];
    child->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(child->instructionsList);
    child->instructionsList[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 0u, 0u, 0u);
    child->instructionsLength = 1u;
    child->stackSize = 1u;
    child->parameterCount = 0u;
    child->ownerFunction = root;
    child->lineInSourceStart = 10u;
    child->lineInSourceEnd = 10u;
    return root;
}

static char *write_llvm_fixture(TZrBool stripGeneratedSymbols, TZrSize *outGeneratedLength) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedLlvmPath[ZR_TESTS_PATH_MAX];
    char *generatedLlvmText;

    TEST_ASSERT_NOT_NULL(state);
    function = create_llvm_direct_call_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = stripGeneratedSymbols ? "aot_llvm_symbol_stripping_private" : "aot_llvm_symbol_stripping_default";
    options.sourceHash = "aot-llvm-symbol-stripping";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-llvm-symbol-stripping";
    options.requireExecutableLowering = ZR_TRUE;
    options.stripGeneratedSymbols = stripGeneratedSymbols;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_symbol_stripping",
                                                       "generated",
                                                       stripGeneratedSymbols ? "stripped" : "default",
                                                       ".ll",
                                                       generatedLlvmPath,
                                                       sizeof(generatedLlvmPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, generatedLlvmPath, &options));

    generatedLlvmText = ZrTests_ReadTextFile(generatedLlvmPath, outGeneratedLength);
    TEST_ASSERT_NOT_NULL(generatedLlvmText);
    ZrTests_Runtime_State_Destroy(state);
    return generatedLlvmText;
}

static void test_aot_llvm_default_preserves_generated_function_symbols(void) {
    TZrSize generatedLength = 0u;
    char *generatedLlvmText = write_llvm_fixture(ZR_FALSE, &generatedLength);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedLlvmText, "; symbol_stripping.generatedSymbols = 0");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_aot_fn_0(ptr %state)");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_aot_fn_1(ptr %state)");
    assert_text_contains(generatedLlvmText, "ptr @zr_aot_fn_0");
    assert_text_contains(generatedLlvmText, "ptr @zr_aot_fn_1");
    assert_text_contains(generatedLlvmText, "call i64 @zr_aot_fn_0(ptr %state)");
    assert_text_contains(generatedLlvmText, "call i64 @zr_aot_fn_1(ptr %state)");
    assert_text_contains(generatedLlvmText, "define ptr @ZrVm_GetAotCompiledModule()");

    free(generatedLlvmText);
}

static void test_aot_llvm_strip_generated_symbols_renames_private_function_symbols(void) {
    TZrSize generatedLength = 0u;
    char *generatedLlvmText = write_llvm_fixture(ZR_TRUE, &generatedLength);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, generatedLength);
    assert_text_contains(generatedLlvmText, "; symbol_stripping.generatedSymbols = 1");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_fn_g0(ptr %state)");
    assert_text_contains(generatedLlvmText, "define internal i64 @zr_fn_g1(ptr %state)");
    assert_text_contains(generatedLlvmText, "ptr @zr_fn_g0");
    assert_text_contains(generatedLlvmText, "ptr @zr_fn_g1");
    assert_text_contains(generatedLlvmText, "call i64 @zr_fn_g0(ptr %state)");
    assert_text_contains(generatedLlvmText, "call i64 @zr_fn_g1(ptr %state)");
    assert_text_does_not_contain(generatedLlvmText, "@zr_aot_fn_");
    assert_text_contains(generatedLlvmText, "define ptr @ZrVm_GetAotCompiledModule()");

    free(generatedLlvmText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_llvm_default_preserves_generated_function_symbols);
    RUN_TEST(test_aot_llvm_strip_generated_symbols_renames_private_function_symbols);
    return UNITY_END();
}
