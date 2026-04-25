#include <string.h>
#include <time.h>

#include "matrix_add_2d_compile_fixture.h"
#include "path_support.h"
#include "unity.h"
#include "runtime_support.h"
#include "zr_test_log_macros.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"

typedef struct SZrRegressionTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrRegressionTestTimer;

void test_matrix_add_2d_compile_binds_super_array_items_for_hot_typed_int_paths(void);
void test_w2_load_typed_arithmetic_probe_reports_residual_candidates(void);
void test_w2_dispatch_loops_materialized_constant_signed_arithmetic_fuses(void);
void test_w2_signed_equality_branch_fuses_slot_operands(void);
void test_w2_signed_greater_equal_branch_reuses_greater_signed_jump(void);
void test_w2_left_constant_add_mul_fold_to_existing_const_opcodes(void);
void test_w2_right_constant_mod_fold_uses_cfg_liveness_across_branch(void);
void test_w2_late_forward_get_stack_after_member_call_specialization(void);

static TZrUInt32 count_opcode_recursive(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 count = 0;
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        count += count_opcode_recursive(&function->childFunctionList[index], opcode);
    }

    return count;
}

static TZrBool function_has_left_constant_add_mul_pair_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        const TZrInstruction *loadInstruction = &function->instructionsList[index - 1];
        const TZrInstruction *arithmeticInstruction = &function->instructionsList[index];
        EZrInstructionCode arithmeticOpcode = (EZrInstructionCode)arithmeticInstruction->instruction.operationCode;

        if ((EZrInstructionCode)loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            continue;
        }

        if (arithmeticOpcode != ZR_INSTRUCTION_ENUM(ADD_SIGNED) &&
            arithmeticOpcode != ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST) &&
            arithmeticOpcode != ZR_INSTRUCTION_ENUM(MUL_SIGNED) &&
            arithmeticOpcode != ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)) {
            continue;
        }

        if (arithmeticInstruction->instruction.operand.operand1[0] == loadInstruction->instruction.operandExtra) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_left_constant_add_mul_pair_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_adjacent_right_constant_mod_signed_pair_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        const TZrInstruction *loadInstruction = &function->instructionsList[index - 1];
        const TZrInstruction *modInstruction = &function->instructionsList[index];

        if ((EZrInstructionCode)loadInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) &&
            (EZrInstructionCode)modInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(MOD_SIGNED) &&
            modInstruction->instruction.operand.operand1[1] == loadInstruction->instruction.operandExtra) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_adjacent_right_constant_mod_signed_pair_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_adjacent_signed_equality_const_jump_if_pair_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        const TZrInstruction *compareInstruction = &function->instructionsList[index - 1];
        const TZrInstruction *jumpInstruction = &function->instructionsList[index];

        if ((EZrInstructionCode)compareInstruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST) &&
            (EZrInstructionCode)jumpInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(JUMP_IF) &&
            compareInstruction->instruction.operandExtra == jumpInstruction->instruction.operandExtra) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_adjacent_signed_equality_const_jump_if_pair_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_adjacent_get_stack_to_mod_signed_const_pair_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        const TZrInstruction *loadInstruction = &function->instructionsList[index - 1];
        const TZrInstruction *modInstruction = &function->instructionsList[index];
        EZrInstructionCode modOpcode = (EZrInstructionCode)modInstruction->instruction.operationCode;

        if ((EZrInstructionCode)loadInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) &&
            (modOpcode == ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST) ||
             modOpcode == ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST)) &&
            modInstruction->instruction.operand.operand1[0] == loadInstruction->instruction.operandExtra) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_adjacent_get_stack_to_mod_signed_const_pair_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_post_member_call_get_stack_typed_arithmetic_pair_recursive(const SZrFunction *function) {
    TZrUInt32 index;
    TZrBool afterMemberCall = ZR_FALSE;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) {
            afterMemberCall = ZR_TRUE;
            continue;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(JUMP) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST) ||
            opcode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
            afterMemberCall = ZR_FALSE;
            continue;
        }

        if (afterMemberCall && opcode == ZR_INSTRUCTION_ENUM(GET_STACK) && index + 1 < function->instructionsLength) {
            const TZrInstruction *consumer = &function->instructionsList[index + 1];
            EZrInstructionCode consumerOpcode = (EZrInstructionCode)consumer->instruction.operationCode;
            TZrUInt32 copiedSlot = instruction->instruction.operandExtra;

            if ((consumerOpcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)) &&
                (consumer->instruction.operand.operand1[0] == copiedSlot ||
                 consumer->instruction.operand.operand1[1] == copiedSlot)) {
                return ZR_TRUE;
            }
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_post_member_call_get_stack_typed_arithmetic_pair_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_post_member_call_get_constant_typed_arithmetic_pair_recursive(
        const SZrFunction *function) {
    TZrUInt32 index;
    TZrBool afterMemberCall = ZR_FALSE;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) {
            afterMemberCall = ZR_TRUE;
            continue;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(JUMP) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST) ||
            opcode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
            afterMemberCall = ZR_FALSE;
            continue;
        }

        if (afterMemberCall && opcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) &&
            index + 1 < function->instructionsLength) {
            const TZrInstruction *consumer = &function->instructionsList[index + 1];
            EZrInstructionCode consumerOpcode = (EZrInstructionCode)consumer->instruction.operationCode;
            TZrUInt32 copiedSlot = instruction->instruction.operandExtra;

            if ((consumerOpcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(SUB_SIGNED) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(DIV_SIGNED) ||
                 consumerOpcode == ZR_INSTRUCTION_ENUM(MOD_SIGNED)) &&
                (consumer->instruction.operand.operand1[0] == copiedSlot ||
                 consumer->instruction.operand.operand1[1] == copiedSlot)) {
                return ZR_TRUE;
            }
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_post_member_call_get_constant_typed_arithmetic_pair_recursive(
                    &function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void test_matrix_add_2d_compile_binds_super_array_items_for_hot_typed_int_paths(void) {
    SZrRegressionTestTimer timer;
    ZrMatrixAdd2dCompileFixture fixture;

    timer.startTime = clock();
    ZR_TEST_START("Matrix Add 2D Binds Super Array Items For Hot Typed Int Paths");
    ZR_TEST_INFO("Array<int> loop-local items cache",
                 "Testing that typed Array<int> hot paths bind hidden items once and then use cached-items get/set opcodes.");

    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "w2_items_cache"),
            "Failed to prepare fresh matrix_add_2d compile fixture");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS)),
            "matrix_add_2d should bind Array<int> hidden items for repeated hot-loop indexed access");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST)),
            "matrix_add_2d should use cached-items plain-destination Array<int> reads");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS)),
            "matrix_add_2d should use cached-items Array<int> writes");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "Matrix Add 2D Binds Super Array Items For Hot Typed Int Paths");
    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    ZR_TEST_DIVIDER();
}

void test_w2_load_typed_arithmetic_probe_reports_residual_candidates(void) {
    SZrRegressionTestTimer timer;
    ZrMatrixAdd2dCompileFixture fixture;
    SZrQuickeningLoadTypedArithmeticProbeStats stats;

    timer.startTime = clock();
    ZR_TEST_START("W2 Load Typed Arithmetic Probe Reports Residual Candidates");
    ZR_TEST_INFO("Load/typed arithmetic probe",
                 "Testing that W2 exposes a post-quickening statistics gate for residual GET_STACK/GET_CONSTANT plus typed arithmetic patterns.");

    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "w2_load_typed_arithmetic_probe"),
            "Failed to prepare fresh matrix_add_2d compile fixture");

    memset(&stats, 0, sizeof(stats));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_Quickening_CollectLoadTypedArithmeticProbeStats(fixture.function, &stats),
            "Expected load/typed arithmetic probe stats collection to succeed");
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32_MESSAGE(
            stats.getStackTypedArithmeticPairs + stats.getConstantTypedArithmeticPairs,
            stats.safeFusionCandidates + stats.materializedLoadCandidates,
            "Total residual load/typed arithmetic pairs should cover safe and materialized-blocked candidates");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Load Typed Arithmetic Probe Reports Residual Candidates");
    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    ZR_TEST_DIVIDER();
}

void test_w2_dispatch_loops_materialized_constant_signed_arithmetic_fuses(void) {
    SZrRegressionTestTimer timer;
    char projectPath[ZR_TESTS_PATH_MAX];
    char sourcePath[ZR_TESTS_PATH_MAX];
    char *source;
    SZrGlobalState *global;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrUInt32 fusedConstantCount;
    TZrUInt32 fusedStackCount;
    TZrUInt32 fusedDeadStackCount;
    TZrUInt32 fusedStackLoadConstantCount;
    TZrUInt32 fusedSignedEqualityBranchCount;
    SZrQuickeningLoadTypedArithmeticProbeStats stats;
    int written;

    timer.startTime = clock();
    ZR_TEST_START("W2 Dispatch Loops Materialized Constant Signed Arithmetic Fuses");
    ZR_TEST_INFO("Load/typed arithmetic fusion",
                 "Testing that the real dispatch_loops hot pattern emits materialized GET_CONSTANT plus signed arithmetic fusion opcodes.");

    written = snprintf(projectPath,
                       sizeof(projectPath),
                       "%s/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp",
                       ZR_VM_TESTS_SOURCE_DIR);
    TEST_ASSERT_TRUE_MESSAGE(written > 0 && (TZrSize)written < sizeof(projectPath),
                             "Failed to build dispatch_loops project path");
    written = snprintf(sourcePath,
                       sizeof(sourcePath),
                       "%s/benchmarks/cases/dispatch_loops/zr/src/main.zr",
                       ZR_VM_TESTS_SOURCE_DIR);
    TEST_ASSERT_TRUE_MESSAGE(written > 0 && (TZrSize)written < sizeof(sourcePath),
                             "Failed to build dispatch_loops source path");

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL_MESSAGE(global, "Failed to create dispatch_loops project global state");
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to get dispatch_loops main thread state");
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibSystem_Register(global), "Failed to register zr.system for dispatch_loops compile");

    source = ZrTests_ReadTextFile(sourcePath, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(source, "Failed to read dispatch_loops source");

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to allocate source name");

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile dispatch_loops source");

    fusedConstantCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST)) +
                         count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST)) +
                         count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST)) +
                         count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST)) +
                         count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST));
    fusedStackCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST)) +
                      count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST)) +
                      count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST)) +
                      count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST)) +
                      count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST));
    fusedDeadStackCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK));
    fusedStackLoadConstantCount =
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST));
    fusedSignedEqualityBranchCount =
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST));
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fusedConstantCount,
            "Expected at least one materialized GET_CONSTANT plus signed arithmetic fused opcode");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fusedStackCount,
            "Expected at least one materialized GET_STACK plus signed const arithmetic fused opcode");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fusedDeadStackCount,
            "Expected at least one dead GET_STACK plus signed arithmetic fused opcode");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            fusedStackLoadConstantCount,
            "Dead constant materialization in GET_STACK plus GET_CONSTANT signed arithmetic should dematerialize");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fusedSignedEqualityBranchCount,
            "Expected at least one signed equality const plus branch fused opcode");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_adjacent_signed_equality_const_jump_if_pair_recursive(function),
            "No adjacent LOGICAL_EQUAL_SIGNED_CONST -> JUMP_IF pair should remain in dispatch_loops");
    memset(&stats, 0, sizeof(stats));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_Quickening_CollectLoadTypedArithmeticProbeStats(function, &stats),
            "Expected dispatch_loops residual load/typed arithmetic probe collection to succeed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            stats.getStackTypedArithmeticPairs + stats.getConstantTypedArithmeticPairs,
            "dispatch_loops should not leave residual GET_STACK/GET_CONSTANT plus typed arithmetic pairs after W2 fusion");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Dispatch Loops Materialized Constant Signed Arithmetic Fuses");
    ZrCore_Function_Free(state, function);
    free(source);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_w2_signed_equality_branch_fuses_slot_operands(void) {
    static const char *source =
            "var left: int = 17;\n"
            "var right: int = 19;\n"
            "if (left == right) {\n"
            "    return 1;\n"
            "}\n"
            "return 2;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START("W2 Signed Equality Branch Fuses Slot Operands");
    ZR_TEST_INFO("Load/branch fusion",
                 "Testing that signed equality followed by JUMP_IF fuses into a slot-slot branch opcode.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, "w2_signed_equality_branch_fusion.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile signed equality branch source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED)),
            "Expected signed equality plus branch to fuse into JUMP_IF_NOT_EQUAL_SIGNED");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Signed Equality Branch Fuses Slot Operands");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_signed_greater_equal_branch_reuses_greater_signed_jump(void) {
    static const char *source =
            "var cursor: int = 3;\n"
            "var floor: int = 0;\n"
            "if (cursor >= floor) {\n"
            "    return 1;\n"
            "}\n"
            "return 2;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START("W2 Signed Greater Equal Branch Reuses Greater Signed Jump");
    ZR_TEST_INFO("Load/branch fusion",
                 "Testing that signed >= followed by JUMP_IF fuses through the existing greater-signed branch opcode.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, "w2_signed_greater_equal_branch_fusion.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile signed greater-equal branch source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED)),
            "Expected signed >= plus branch to reuse JUMP_IF_GREATER_SIGNED with swapped operands");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED)),
            "No LOGICAL_GREATER_EQUAL_SIGNED should remain for the fused branch pattern");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Signed Greater Equal Branch Reuses Greater Signed Jump");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_left_constant_add_mul_fold_to_existing_const_opcodes(void) {
    static const char *source =
            "work(input: int): int {\n"
            "    var scaled = 24 * input;\n"
            "    var biased = 7 + scaled;\n"
            "    return biased;\n"
            "}\n"
            "return work(3);\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START("W2 Left Constant Add Mul Fold To Existing Const Opcodes");
    ZR_TEST_INFO("Load/typed arithmetic fusion",
                 "Testing that left constant ADD/MUL materialized temporaries fold to existing *_CONST opcodes.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, "w2_left_const_add_mul_fold_test.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to allocate source name");

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile left-constant arithmetic test source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST)),
            "24 * input should reuse the existing right-constant signed multiply opcode after operand swap");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST)),
            "7 + scaled should reuse the existing right-constant signed add opcode after operand swap");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_left_constant_add_mul_pair_recursive(function),
            "No adjacent GET_CONSTANT -> left ADD/MUL_SIGNED pair should remain for the folded pattern");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Left Constant Add Mul Fold To Existing Const Opcodes");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_right_constant_mod_fold_uses_cfg_liveness_across_branch(void) {
    static const char *source =
            "work(value: int): int {\n"
            "    var remainder = value % 7;\n"
            "    if (remainder == 0) {\n"
            "        return value + 1;\n"
            "    }\n"
            "    return remainder + value;\n"
            "}\n"
            "return work(31);\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START("W2 Right Constant Mod Fold Uses CFG Liveness Across Branch");
    ZR_TEST_INFO("Load/typed arithmetic fusion",
                 "Testing that right constant signed modulo folds even when the temp dies across a following branch.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, "w2_right_const_mod_cfg_liveness_test.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to allocate source name");

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile right-constant modulo test source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST)),
            "value % 7 should fold to MOD_SIGNED_CONST when CFG liveness proves the constant temp dead");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_adjacent_right_constant_mod_signed_pair_recursive(function),
            "No adjacent GET_CONSTANT -> MOD_SIGNED right-constant pair should remain for the folded pattern");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_adjacent_get_stack_to_mod_signed_const_pair_recursive(function),
            "No adjacent GET_STACK -> MOD_SIGNED_CONST pair should remain when CFG liveness proves the copied temp dead");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Right Constant Mod Fold Uses CFG Liveness Across Branch");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_late_forward_get_stack_after_member_call_specialization(void) {
    static const char *source =
            "class Worker {\n"
            "    pri var state: int;\n"
            "    pub @constructor(seed: int) { this.state = seed; }\n"
            "    pub step(delta: int): int {\n"
            "        this.state = (this.state + delta) % 10007;\n"
            "        return this.state;\n"
            "    }\n"
            "}\n"
            "work(): int {\n"
            "    var checksum = 0;\n"
            "    var delta = 5;\n"
            "    var worker = new Worker(1);\n"
            "    var value = worker.step(delta);\n"
            "    checksum = (checksum + value * 2 + delta % 29) % 1000000007;\n"
            "    return checksum;\n"
            "}\n"
            "return work();\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START("W2 Late Forward Get Stack After Member Call Specialization");
    ZR_TEST_INFO("Load/typed arithmetic fusion",
                 "Testing that late known member call specialization is followed by GET_STACK copy forwarding.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, "w2_late_forward_after_member_call_test.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to allocate source name");

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile late member-call forwarding test source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)),
            "The fixture should lower worker.step(delta) to a known VM member call");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_post_member_call_get_stack_typed_arithmetic_pair_recursive(function),
            "Late member-call specialization should not leave GET_STACK copies feeding typed arithmetic");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_post_member_call_get_constant_typed_arithmetic_pair_recursive(function),
            "Late member-call specialization should not leave GET_CONSTANT copies feeding typed arithmetic");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Late Forward Get Stack After Member Call Specialization");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}
