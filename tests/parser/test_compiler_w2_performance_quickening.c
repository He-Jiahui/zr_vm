#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "matrix_add_2d_compile_fixture.h"
#include "path_support.h"
#include "unity.h"
#include "runtime_support.h"
#include "zr_test_log_macros.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrRegressionTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrRegressionTestTimer;

void test_matrix_add_2d_compile_binds_super_array_items_for_hot_typed_int_paths(void);
void test_w2_load_typed_arithmetic_probe_reports_residual_candidates(void);
void test_w2_dispatch_loops_materialized_constant_signed_arithmetic_fuses(void);
void test_w2_signed_equality_branch_fuses_slot_operands(void);
void test_w2_signed_greater_equal_branch_reuses_greater_signed_jump(void);
void test_w2_static_iterator_move_next_branch_fuses(void);
void test_w2_static_iterator_plain_dest_state_does_not_cross_loop_exit(void);
void test_w2_super_array_add_variable_value_elides_dead_receiver_setup(void);
void test_w2_get_member_slot_direct_result_store_elides_temp_copy(void);
void test_w2_known_native_member_call_skips_argument_loads(void);
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

static TZrBool function_has_dead_super_array_add_receiver_setup_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 3; index < function->instructionsLength; index++) {
        const TZrInstruction *addInstruction = &function->instructionsList[index];
        const TZrInstruction *receiverReloadInstruction;
        const TZrInstruction *receiverStageInstruction;
        const TZrInstruction *receiverLoadInstruction;
        TZrUInt32 destinationSlot;
        TZrUInt32 receiverSlot;
        TZrUInt32 reloadIndex;

        if ((EZrInstructionCode)addInstruction->instruction.operationCode !=
            ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)) {
            continue;
        }

        destinationSlot = addInstruction->instruction.operandExtra;
        receiverSlot = addInstruction->instruction.operand.operand1[0];
        reloadIndex = index - 1u;
        if ((EZrInstructionCode)function->instructionsList[reloadIndex].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            if (index < 4) {
                continue;
            }
            reloadIndex--;
        }

        receiverReloadInstruction = &function->instructionsList[reloadIndex];
        receiverStageInstruction = &function->instructionsList[reloadIndex - 1u];
        receiverLoadInstruction = &function->instructionsList[reloadIndex - 2u];
        if ((EZrInstructionCode)receiverReloadInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) &&
            receiverReloadInstruction->instruction.operandExtra == destinationSlot &&
            (TZrUInt32)receiverReloadInstruction->instruction.operand.operand2[0] == receiverSlot &&
            (EZrInstructionCode)receiverStageInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SET_STACK) &&
            receiverStageInstruction->instruction.operandExtra == receiverSlot &&
            (TZrUInt32)receiverStageInstruction->instruction.operand.operand2[0] == destinationSlot &&
            (EZrInstructionCode)receiverLoadInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) &&
            receiverLoadInstruction->instruction.operandExtra == destinationSlot) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_dead_super_array_add_receiver_setup_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_adjacent_get_member_slot_set_stack_temp_store_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index + 1u < function->instructionsLength; index++) {
        const TZrInstruction *memberInstruction = &function->instructionsList[index];
        const TZrInstruction *storeInstruction = &function->instructionsList[index + 1u];
        TZrUInt32 temporarySlot;

        if ((EZrInstructionCode)memberInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT) ||
            (EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
            continue;
        }

        temporarySlot = memberInstruction->instruction.operandExtra;
        if ((TZrUInt32)storeInstruction->instruction.operand.operand2[0] == temporarySlot &&
            storeInstruction->instruction.operandExtra != temporarySlot) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_adjacent_get_member_slot_set_stack_temp_store_recursive(&function->childFunctionList[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
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

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
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

static TZrBool function_has_adjacent_iter_move_next_jump_if_pair_recursive(const SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        const TZrInstruction *moveNextInstruction = &function->instructionsList[index - 1];
        const TZrInstruction *jumpInstruction = &function->instructionsList[index];

        if ((EZrInstructionCode)moveNextInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT) &&
            (EZrInstructionCode)jumpInstruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(JUMP_IF) &&
            moveNextInstruction->instruction.operandExtra == jumpInstruction->instruction.operandExtra) {
            return ZR_TRUE;
        }
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        if (function_has_adjacent_iter_move_next_jump_if_pair_recursive(&function->childFunctionList[index])) {
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
    static const char *intermediatePath = "w2_dispatch_loops_add_mod_const_writer_test.zri";
    SZrRegressionTestTimer timer;
    char projectPath[ZR_TESTS_PATH_MAX];
    char sourcePath[ZR_TESTS_PATH_MAX];
    char *source;
    char *intermediateText;
    SZrGlobalState *global;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrUInt32 fusedConstantCount;
    TZrUInt32 fusedStackCount;
    TZrUInt32 fusedDeadStackCount;
    TZrUInt32 fusedStackLoadConstantCount;
    TZrUInt32 fusedSignedEqualityBranchCount;
    TZrUInt32 fusedAddModConstCount;
    TZrUInt32 genericArithmeticCount;
    TZrUInt32 resetNullCount;
    TZrUInt32 resetNull2Count;
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
    fusedDeadStackCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)) +
                          count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK));
    fusedStackLoadConstantCount =
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST));
    fusedSignedEqualityBranchCount =
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST));
    fusedAddModConstCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST));
    genericArithmeticCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD)) +
                             count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL)) +
                             count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MOD));
    resetNullCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(RESET_STACK_NULL));
    resetNull2Count = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(RESET_STACK_NULL2));
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
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fusedAddModConstCount,
            "Expected adjacent signed add plus const modulo to fuse");
    remove(intermediatePath);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath),
                             "Expected intermediate writer to handle ADD_SIGNED_MOD_CONST");
    intermediateText = read_text_file_owned(intermediatePath);
    TEST_ASSERT_NOT_NULL_MESSAGE(intermediateText, "Failed to read dispatch_loops intermediate output");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(intermediateText, "ADD_SIGNED_MOD_CONST"),
                                 "Intermediate output should name ADD_SIGNED_MOD_CONST");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(intermediateText, "constant_index="),
                                 "Intermediate output should include ADD_SIGNED_MOD_CONST operands");
    free(intermediateText);
    remove(intermediatePath);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            resetNullCount + (resetNull2Count * 2u),
            "Expected null constant temp clears to lower to RESET_STACK_NULL forms");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            resetNull2Count,
            "Expected adjacent null temp clears to fuse into RESET_STACK_NULL2");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            genericArithmeticCount,
            "dispatch_loops typed member arithmetic should not fall back to generic ADD/MUL/MOD opcodes");
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

void test_w2_static_iterator_move_next_branch_fuses(void) {
    static const char *source =
            "var container = %import(\"zr.container\");\n"
            "var values = new container.Array<int>();\n"
            "values.add(3);\n"
            "values.add(5);\n"
            "var total = 0;\n"
            "for (var value in values) {\n"
            "    total = total + value;\n"
            "}\n"
            "return total;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START("W2 Static Iterator Move Next Branch Fuses");
    ZR_TEST_INFO("iterator loop guard fusion",
                 "Testing that typed foreach lowers ITER_MOVE_NEXT plus JUMP_IF to a static iterator superinstruction.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibContainer_Register(state->global),
                             "Failed to register zr.container for static iterator fusion test");

    sourceName = ZrCore_String_CreateFromNative(state, "w2_static_iterator_loop_guard_fusion.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile static iterator fusion source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE)),
            "Expected typed foreach loop guard to fuse into SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_adjacent_iter_move_next_jump_if_pair_recursive(function),
            "No adjacent ITER_MOVE_NEXT -> JUMP_IF pair should remain for typed foreach loop guards");
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
            "Fused static iterator loop guard should execute successfully");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(8, result, "Fused static iterator loop should preserve foreach semantics");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Static Iterator Move Next Branch Fuses");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_static_iterator_plain_dest_state_does_not_cross_loop_exit(void) {
    static const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Pair} = %import(\"zr.container\");\n"
            "var values = new container.Set<Pair<int, string>>();\n"
            "var score = 0;\n"
            "if (values.add(new container.Pair<int, string>(1, \"a\"))) { score = score + 10; }\n"
            "if (!values.add(new container.Pair<int, string>(1, \"a\"))) { score = score + 20; }\n"
            "if (values.add(new container.Pair<int, string>(2, \"b\"))) { score = score + 30; }\n"
            "for (var item in values) {\n"
            "    score = score + item.first;\n"
            "}\n"
            "return values.count * 100 + score;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START("W2 Static Iterator Plain Dest State Does Not Cross Loop Exit");
    ZR_TEST_INFO("iterator CFG",
                 "Testing that static iterator fused branch exits reset plain-destination state before post-loop arithmetic.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibContainer_Register(state->global),
                             "Failed to register zr.container for static iterator CFG test");

    sourceName = ZrCore_String_CreateFromNative(state, "w2_static_iterator_plain_dest_cfg_test.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile static iterator CFG test source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE)),
            "Expected typed Set foreach loop guard to fuse into SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE");
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
            "Static iterator loop followed by typed arithmetic should execute successfully");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(263, result, "Set<Pair<int,string>> uniqueness and post-loop arithmetic changed");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Static Iterator Plain Dest State Does Not Cross Loop Exit");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_super_array_add_variable_value_elides_dead_receiver_setup(void) {
    static const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array} = %import(\"zr.container\");\n"
            "var values = new container.Array<int>();\n"
            "var buckets = new container.Map<string, Array<int>>();\n"
            "var i = 0;\n"
            "var total = 0;\n"
            "while (i < 6) {\n"
            "    var value = (i * 3 + 1) % 17;\n"
            "    values.add((i * 3 + 1) % 17);\n"
            "    buckets[\"last\"] = values;\n"
            "    total = total + value;\n"
            "    i = i + 1;\n"
            "}\n"
            "return total;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START("W2 Super Array Add Variable Value Elides Dead Receiver Setup");
    ZR_TEST_INFO("Array<int>.add receiver setup",
                 "Testing that variable-value Array<int>.add folds the dead receiver materialization copies.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibContainer_Register(state->global),
                             "Failed to register zr.container for Array<int>.add receiver setup test");

    sourceName = ZrCore_String_CreateFromNative(state, "w2_super_array_add_variable_value_receiver_setup.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile Array<int>.add receiver setup source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)),
            "Expected typed Array<int>.add to lower to SUPER_ARRAY_ADD_INT");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_dead_super_array_add_receiver_setup_recursive(function),
            "No dead GET_STACK/SET_STACK/GET_STACK receiver setup should remain before SUPER_ARRAY_ADD_INT");
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
            "Array<int>.add receiver setup fold should execute successfully");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(51, result, "Array<int>.add loop result changed");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Super Array Add Variable Value Elides Dead Receiver Setup");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_get_member_slot_direct_result_store_elides_temp_copy(void) {
    static const char *source =
            "var container = %import(\"zr.container\");\n"
            "var queue = new container.LinkedList<int>();\n"
            "queue.addLast(7);\n"
            "queue.addLast(11);\n"
            "var head = queue.first;\n"
            "var firstValue = head.value;\n"
            "var tailValue = queue.last.value;\n"
            "return <int> firstValue * 10 + <int> tailValue;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START("W2 Get Member Slot Direct Result Store Elides Temp Copy");
    ZR_TEST_INFO("member slot direct result",
                 "Testing that GET_MEMBER_SLOT assignments store directly into the target stack slot.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibContainer_Register(state->global),
                             "Failed to register zr.container for member slot direct result test");

    sourceName = ZrCore_String_CreateFromNative(state, "w2_get_member_slot_direct_result_store.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile member slot direct result source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)),
            "Expected member access to lower to GET_MEMBER_SLOT");
    TEST_ASSERT_FALSE_MESSAGE(
            function_has_adjacent_get_member_slot_set_stack_temp_store_recursive(function),
            "No adjacent GET_MEMBER_SLOT -> SET_STACK temp store should remain");
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
            "GET_MEMBER_SLOT direct result store should execute successfully");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(81, result, "LinkedList member read result changed");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Get Member Slot Direct Result Store Elides Temp Copy");
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_w2_known_native_member_call_skips_argument_loads(void) {
    static const char *source =
            "var container = %import(\"zr.container\");\n"
            "var queue = new container.LinkedList<int>();\n"
            "var base = 10;\n"
            "queue.addLast(base);\n"
            "queue.addLast(base + 5);\n"
            "return <int> queue.first.value + <int> queue.last.value;\n";
    SZrRegressionTestTimer timer;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START("W2 Known Native Member Call Skips Argument Loads");
    ZR_TEST_INFO("native member call fusion",
                 "Testing that argument GET_STACK/GET_CONSTANT setup does not block KNOWN_NATIVE_MEMBER_CALL.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(state, "Failed to create test runtime state");
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibContainer_Register(state->global),
                             "Failed to register zr.container for native member call fusion test");

    sourceName = ZrCore_String_CreateFromNative(state, "w2_known_native_member_call_argument_loads.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName, "Failed to create source name");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "Failed to compile native member call fusion source");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            1u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL_RECV_U8)),
            "Expected LinkedList.addLast calls to fold loaded/computed arguments and receiver setup into RECV_U8 form");
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result),
            "KNOWN_NATIVE_MEMBER_CALL with loaded arguments should execute successfully");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(25, result, "LinkedList native member call result changed");

    timer.endTime = clock();
    ZR_TEST_PASS(timer, "W2 Known Native Member Call Skips Argument Loads");
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
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)),
            "The fixture should lower worker.step(delta) to a known or fused VM member call");
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
