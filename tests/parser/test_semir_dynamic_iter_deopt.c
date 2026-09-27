#include <stdlib.h>

#include "unity.h"

#include "compiler_internal.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

/* 为单指令测试的 VM state 提供宿主 malloc/realloc/free 分配器。 */
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    /* BUG: 正常释放或扩容非空旧块会以 >= 比较不相关的指针；ISO C 不定义
     * 这种关系比较，分配器返回和资源释放结果不受可移植 C 契约保证。 */
    /* TODO: 此处把低于 0x1000 的非空地址当作无效旧块；需核查宿主分配
     * 是否能返回该范围内的地址，以及漏释放或扩容不复制的可达性。 */
    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

/* 为元数据构建创建独立主线程 VM state，成功结果交给 destroy_test_state。 */
static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

/* 通过 global 释放测试 state；调用前先释放挂在其上的测试函数。 */
static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

/* 构造单条迭代 ExecBC 指令，直接检验 ExecBC 到 SemIR 的元数据投影。
 * 成功返回的函数归调用方；其 instructionsList 随 Function_Free 一起释放。 */
static SZrFunction *create_single_iter_instruction_function(SZrState *state,
                                                           EZrInstructionCode opcode,
                                                           TZrUInt32 resultSlot,
                                                           TZrUInt32 iteratorSlot) {
    SZrFunction *function;
    TZrInstruction *instructions;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                    sizeof(TZrInstruction),
                                                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    /* 指令缓冲区申请失败时归还尚未拥有指令的 function，交给上层断言。 */
    if (instructions == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(instructions, 0, sizeof(*instructions));
    instructions[0].instruction.operationCode = (TZrUInt16)opcode;
    instructions[0].instruction.operandExtra = (TZrUInt16)resultSlot;
    instructions[0].instruction.operand.operand1[0] = (TZrUInt16)iteratorSlot;
    instructions[0].instruction.operand.operand1[1] = 0u;

    /* 迭代初始化或前进在 SemIR 中保留结果槽和迭代器槽；缓冲区所有权转入 function。 */
    function->instructionsList = instructions;
    function->instructionsLength = 1u;
    function->stackSize = 6u;
    return function;
}

/* 从函数持有的 SemIR 表借出目标 opcode；函数释放后该指针失效。 */
static const SZrSemIrInstruction *find_semir_opcode(const SZrFunction *function, EZrSemIrOpcode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < function->semIrInstructionLength; index++) {
        if ((EZrSemIrOpcode)function->semIrInstructions[index].opcode == opcode) {
            return &function->semIrInstructions[index];
        }
    }

    return ZR_NULL;
}

/* 核对迭代动态效应、deopt 回链和动态类型，不运行迭代器。 */
static void assert_dynamic_iter_boundary(EZrInstructionCode execOpcode,
                                         EZrSemIrOpcode expectedSemIrOpcode,
                                         TZrUInt32 resultSlot,
                                         TZrUInt32 iteratorSlot) {
    SZrState *state = create_test_state();
    SZrFunction *function;
    const SZrSemIrInstruction *instruction;

    TEST_ASSERT_NOT_NULL(state);
    function = create_single_iter_instruction_function(state, execOpcode, resultSlot, iteratorSlot);
    TEST_ASSERT_NOT_NULL(function);

    /* 调用真实元数据构建入口，检验转换后的元数据契约。 */
    TEST_ASSERT_TRUE(compiler_build_function_semir_metadata(state, function));
    instruction = find_semir_opcode(function, expectedSemIrOpcode);
    TEST_ASSERT_NOT_NULL(instruction);

    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrInstructionLength);
    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrDeoptTableLength);
    TEST_ASSERT_EQUAL_UINT32(0u, instruction->execInstructionIndex);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME,
                             function->semIrEffectTable[instruction->effectTableIndex].kind);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_RUNTIME_SEMIR_DEOPT_ID_NONE, instruction->deoptId);
    TEST_ASSERT_EQUAL_UINT32(instruction->deoptId, function->semIrDeoptTable[0].deoptId);
    TEST_ASSERT_EQUAL_UINT32(0u, function->semIrDeoptTable[0].execInstructionIndex);
    TEST_ASSERT_EQUAL_UINT32(resultSlot, instruction->destinationSlot);
    TEST_ASSERT_EQUAL_UINT32(iteratorSlot, instruction->operand0);
    TEST_ASSERT_EQUAL_UINT32(0u, instruction->operand1);
    TEST_ASSERT_EQUAL_UINT32(ZR_STATIC_C_TYPE_DYNAMIC,
                             function->semIrTypeTable[instruction->typeTableIndex].staticCType);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

/* 迭代初始化与前进须形成不同动态边界，并保留迭代器槽位。 */
static void test_generic_iter_exec_opcodes_become_dynamic_deopt_boundaries(void) {
    assert_dynamic_iter_boundary(ZR_INSTRUCTION_ENUM(ITER_INIT), ZR_SEMIR_OPCODE_DYN_ITER_INIT, 4u, 1u);
    assert_dynamic_iter_boundary(ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT), ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT, 5u, 2u);
}

/* Unity 要求的用例前钩子；夹具在测试函数内创建。 */
void setUp(void) {}

/* BUG: state 创建成功后任一 Unity 断言失败会经 TEST_ABORT/longjmp 跳过
 * 测试函数末尾的 Function_Free/destroy_test_state；空钩子无法补救，
 * 当时已取得的函数或 VM 原生资源在本进程内失去正常清理路径。 */
void tearDown(void) {}

/* 独立 Unity 入口；CMake 以 semir_dynamic_iter_deopt 注册 CTest。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generic_iter_exec_opcodes_become_dynamic_deopt_boundaries);
    return UNITY_END();
}
