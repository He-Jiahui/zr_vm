/* 迭代边界烟测：检查迭代指令的运行时 helper 降低，并确认要求 full AOT 时拒绝动态迭代 deopt 桥。 */
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include "harness/path_support.h"
#include "harness/aot_c_link_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
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

static void compile_shared_library_or_fail(const char *generatedCPath, const char *sharedLibraryPath) {
    char command[4096];

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
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

static TZrInstruction create_nop_instruction(void) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP);
    return instruction;
}

/* JUMP / JUMP_IF 的相对偏移在 operand2，融合指令才把偏移放进 operand1。 */
static TZrInstruction create_operand2_jump(EZrInstructionCode opcode, TZrUInt16 extra, TZrInt32 offset) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = extra;
    instruction.instruction.operand.operand2[0] = offset;
    return instruction;
}

static SZrFunction *create_function_from_instructions(SZrState *state,
                                                     const TZrInstruction *instructions,
                                                     TZrUInt32 count,
                                                     TZrUInt32 stackSize) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(instructions);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * count,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    memcpy(function->instructionsList, instructions, sizeof(TZrInstruction) * count);
    function->instructionsLength = count;
    function->stackSize = stackSize;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_iterator_function(SZrState *state) {
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
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_INIT), 2u, 0u, 0u);
    function->instructionsList[1] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT), 3u, 2u, 0u);
    function->instructionsList[2] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_CURRENT), 4u, 2u, 0u);
    function->instructionsList[3] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(DYN_ITER_INIT), 2u, 0u, 0u);
    function->instructionsList[4] =
            create_jump_instruction(ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE), 3u, 2u, 0u);
    /* 偏移 0 仍要跳过下一条：pc + 0 + 2 落到 return。 */
    function->instructionsList[5] = create_nop_instruction();
    function->instructionsList[6] = create_return_instruction(1u, 4u);
    function->instructionsLength = 7u;
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
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "        if (zr_aot_branch_taken) {\n"
                                "            goto zr_aot_fn_0_ins_6;\n"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "        if (zr_aot_branch_taken) {\n"
                            "            goto zr_aot_fn_0_ins_5;\n"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_IterInit(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_TryIterMoveNextCachedArrayFast(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_IterMoveNext(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_TryIterCurrentCachedMemberFastStackResult(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Object_IterCurrent(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_IsTruthy"));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT iterator core path"));
    TEST_ASSERT_NULL(strstr(generatedCText, "unsupported AOT iterator branch result"));
    free(generatedCText);

    compile_shared_library_or_fail(generatedCPath, sharedLibraryPath);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

/* 动态迭代需要运行时桥接；full AOT 模式必须拒绝该指令链而不是生成不可执行的纯 AOT 假象。 */
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

/* 融合 foreach 的假分支必须落到 pc + offset + 2。步长 1 会在非空循环体退出时重跑最后一条指令。 */
static void test_aot_fused_iterator_false_branch_uses_verifier_step(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C iterator shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrInstruction typedInstructions[4];
    TZrInstruction dynamicInstructions[6];
    TZrInstruction negativeInstructions[6];
    SZrFunction *typedFunction;
    SZrFunction *dynamicFunction;
    SZrFunction *negativeFunction;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedLlvmPath[ZR_TESTS_PATH_MAX];
    char *generatedText;

    TEST_ASSERT_NOT_NULL(state);

    /* SUPER_ITER 偏移 1，后面留着 JUMP_IF 和一条循环体。解释器与校验器目标是 0 + 1 + 2 = 3。步长 1 会落到循环体 2。 */
    typedInstructions[0] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE), 1u, 0u, 1u);
    typedInstructions[1] = create_operand2_jump(ZR_INSTRUCTION_ENUM(JUMP_IF), 1u, 1);
    typedInstructions[2] = create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_CURRENT), 2u, 0u, 0u);
    typedInstructions[3] = create_return_instruction(1u, 2u);
    typedFunction = create_function_from_instructions(state, typedInstructions, 4u, 3u);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_fused_typed_iterator_step";
    options.sourceHash = "fused-typed-iterator-step";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "fused-typed-iterator-step";
    options.requireExecutableLowering = ZR_TRUE;
    options.requireFullAot = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       "aot_fused_typed_iterator_step",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "lib",
                                                       "libaot_fused_typed_iterator_step",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       "aot_fused_typed_iterator_step",
                                                       ".ll",
                                                       generatedLlvmPath,
                                                       sizeof(generatedLlvmPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, typedFunction, generatedCPath, &options));
    generatedText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(state, &frame, 1, 0, &zr_aot_branch_taken)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "        if (zr_aot_branch_taken) {\n"
                                "            goto zr_aot_fn_0_ins_3;\n"));
    TEST_ASSERT_NULL(strstr(generatedText,
                            "        if (zr_aot_branch_taken) {\n"
                            "            goto zr_aot_fn_0_ins_2;\n"));
    TEST_ASSERT_NULL(strstr(generatedText, "unsupported AOT iterator"));
    free(generatedText);
    compile_shared_library_or_fail(generatedCPath, sharedLibraryPath);

    options.requireFullAot = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(state, typedFunction, generatedLlvmPath, &options));
    generatedText = read_text_file_owned_or_fail(generatedLlvmPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "declare i1 @ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(ptr, ptr, i32, i32, ptr)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "call i1 @ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(ptr %state, ptr %frame, i32 1, i32 0, ptr %truthy_value)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, "label %zr_aot_fn_0_ins_3, label %zr_aot_fn_0_ins_1"));
    TEST_ASSERT_NULL(strstr(generatedText, "label %zr_aot_fn_0_ins_2, label %zr_aot_fn_0_ins_1"));
    free(generatedText);
    ZrCore_Function_Free(state, typedFunction);

    /* SUPER_DYN 偏移 2，后面留着 JUMP_IF、循环体和回边。目标是 1 + 2 + 2 = 5。步长 1 会落到回边 4。 */
    dynamicInstructions[0] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(DYN_ITER_INIT), 2u, 0u, 0u);
    dynamicInstructions[1] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE), 3u, 2u, 2u);
    dynamicInstructions[2] = create_operand2_jump(ZR_INSTRUCTION_ENUM(JUMP_IF), 3u, 2);
    dynamicInstructions[3] =
            create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_CURRENT), 4u, 2u, 0u);
    dynamicInstructions[4] = create_operand2_jump(ZR_INSTRUCTION_ENUM(JUMP), 0u, -4);
    dynamicInstructions[5] = create_return_instruction(1u, 4u);
    dynamicFunction = create_function_from_instructions(state, dynamicInstructions, 6u, 5u);

    options.moduleName = "aot_fused_dynamic_iterator_step";
    options.sourceHash = "fused-dynamic-iterator-step";
    options.inputHash = "fused-dynamic-iterator-step";
    options.requireExecutableLowering = ZR_TRUE;
    options.requireFullAot = ZR_FALSE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       "aot_fused_dynamic_iterator_step",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "lib",
                                                       "libaot_fused_dynamic_iterator_step",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       "aot_fused_dynamic_iterator_step",
                                                       ".ll",
                                                       generatedLlvmPath,
                                                       sizeof(generatedLlvmPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, dynamicFunction, generatedCPath, &options));
    generatedText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(state, &frame, 3, 2, &zr_aot_branch_taken)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, "ZrLibrary_AotRuntime_IterCurrent(state, &frame, 4, 2)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "        if (zr_aot_branch_taken) {\n"
                                "            goto zr_aot_fn_0_ins_5;\n"));
    TEST_ASSERT_NULL(strstr(generatedText,
                            "        if (zr_aot_branch_taken) {\n"
                            "            goto zr_aot_fn_0_ins_4;\n"));
    free(generatedText);
    compile_shared_library_or_fail(generatedCPath, sharedLibraryPath);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(state, dynamicFunction, generatedLlvmPath, &options));
    generatedText = read_text_file_owned_or_fail(generatedLlvmPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "call i1 @ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(ptr %state, ptr %frame, i32 3, i32 2, ptr %truthy_value)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, "label %zr_aot_fn_0_ins_5, label %zr_aot_fn_0_ins_2"));
    TEST_ASSERT_NULL(strstr(generatedText, "label %zr_aot_fn_0_ins_4, label %zr_aot_fn_0_ins_2"));
    free(generatedText);
    ZrCore_Function_Free(state, dynamicFunction);

    /* 负的 int16 偏移必须符号扩展。3 + (-4) + 2 = 1。 */
    negativeInstructions[0] = create_nop_instruction();
    negativeInstructions[1] = create_nop_instruction();
    negativeInstructions[2] = create_nop_instruction();
    negativeInstructions[3] = create_iterator_instruction(
            ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE),
            1u,
            0u,
            (TZrUInt16)(TZrInt16)-4);
    negativeInstructions[4] = create_nop_instruction();
    negativeInstructions[5] = create_return_instruction(1u, 1u);
    negativeFunction = create_function_from_instructions(state, negativeInstructions, 6u, 2u);
    options.moduleName = "aot_fused_iterator_negative_step";
    options.sourceHash = "fused-iterator-negative-step";
    options.inputHash = "fused-iterator-negative-step";
    options.requireFullAot = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       "aot_fused_iterator_negative_step",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "lib",
                                                       "libaot_fused_iterator_negative_step",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, negativeFunction, generatedCPath, &options));
    generatedText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(state, &frame, 1, 0, &zr_aot_branch_taken)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, "            goto zr_aot_fn_0_ins_1;\n"));
    TEST_ASSERT_NULL(strstr(generatedText, "            goto zr_aot_fn_0_ins_0;\n"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText, "zr_aot_gc_safepoint_back_edge"));
    free(generatedText);
    compile_shared_library_or_fail(generatedCPath, sharedLibraryPath);
    ZrCore_Function_Free(state, negativeFunction);

    ZrTests_Runtime_State_Destroy(state);
#endif
}

#if defined(ZR_PLATFORM_UNIX)
/* 融合指令把 move-next 结果写进帧槽；残留 JUMP_IF 不得再读之前写入同一槽的标量局部。 */
static void assert_fused_iterator_write_clears_scalar_local(EZrInstructionCode fusedOpcode,
                                                            TZrBool boolConstant,
                                                            const char *moduleName) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrInstruction instructions[5];
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar libraryName[128];
    char *generatedText;
    char *jumpIfStart;
    char *jumpIfEnd;

    TEST_ASSERT_NOT_NULL(state);
    /* r1 先被常量写成标量局部，再被融合指令覆盖。目标是 1 + 1 + 2 = 4。 */
    instructions[0] = create_operand2_jump(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 0);
    instructions[1] = create_iterator_instruction(fusedOpcode, 1u, 0u, 1u);
    instructions[2] = create_operand2_jump(ZR_INSTRUCTION_ENUM(JUMP_IF), 1u, 1);
    instructions[3] = create_iterator_instruction(ZR_INSTRUCTION_ENUM(ITER_CURRENT), 2u, 0u, 0u);
    instructions[4] = create_return_instruction(1u, 2u);
    function = create_function_from_instructions(state, instructions, 5u, 3u);
    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 1u;
    if (boolConstant) {
        ZrCore_Value_InitAsBool(state, &function->constantValueList[0], ZR_FALSE);
    } else {
        ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 0);
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.sourceHash = moduleName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = moduleName;
    options.requireExecutableLowering = ZR_TRUE;
    snprintf(libraryName, sizeof(libraryName), "lib%s", moduleName);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "src",
                                                       moduleName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_iterator_shared_library",
                                                       "lib",
                                                       libraryName,
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));
    generatedText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "ZrLibrary_AotRuntime_IterMoveNextJumpIfFalse(state, &frame, 1, 0, &zr_aot_branch_taken)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedText,
                                "        if (zr_aot_branch_taken) {\n"
                                "            goto zr_aot_fn_0_ins_4;\n"));
    jumpIfStart = strstr(generatedText, "zr_aot_fn_0_ins_2:\n");
    TEST_ASSERT_NOT_NULL(jumpIfStart);
    jumpIfEnd = strstr(jumpIfStart, "zr_aot_fn_0_ins_3:\n");
    TEST_ASSERT_NOT_NULL(jumpIfEnd);
    *jumpIfEnd = '\0';
    TEST_ASSERT_NOT_NULL(strstr(jumpIfStart, "zr_aot_generic_jump_if"));
    TEST_ASSERT_NOT_NULL(strstr(jumpIfStart,
                                "ZrLibrary_AotRuntime_GenericPrimitiveIsTruthy(state, &frame, 1, &zr_aot_truthy)"));
    TEST_ASSERT_NULL(strstr(jumpIfStart, "_scalar_local"));
    TEST_ASSERT_NULL(strstr(jumpIfStart, "zr_aot_b1"));
    TEST_ASSERT_NULL(strstr(jumpIfStart, "zr_aot_s1"));
    free(generatedText);
    compile_shared_library_or_fail(generatedCPath, sharedLibraryPath);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

#endif

static void test_aot_fused_iterator_result_write_invalidates_scalar_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C iterator shared-library smoke currently validates the Unix toolchain path");
#else
    assert_fused_iterator_write_clears_scalar_local(ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE),
                                                    ZR_TRUE,
                                                    "aot_fused_typed_iterator_stale_bool");
    assert_fused_iterator_write_clears_scalar_local(ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE),
                                                    ZR_FALSE,
                                                    "aot_fused_typed_iterator_stale_int");
    assert_fused_iterator_write_clears_scalar_local(ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE),
                                                    ZR_TRUE,
                                                    "aot_fused_dynamic_iterator_stale_bool");
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_compiles_iterator_boundary_helper_lowering);
    RUN_TEST(test_aot_c_full_aot_rejects_dynamic_iterator_deopt_bridge);
    RUN_TEST(test_aot_fused_iterator_false_branch_uses_verifier_step);
    RUN_TEST(test_aot_fused_iterator_result_write_invalidates_scalar_local);
    return UNITY_END();
}
