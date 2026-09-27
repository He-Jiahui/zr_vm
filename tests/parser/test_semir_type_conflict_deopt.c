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

/* 仅改变基础类型，保持其他类型引用字段一致以隔离槽位冲突。 */
static void init_type_ref(SZrFunctionTypedTypeRef *typeRef, EZrValueType baseType) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = baseType;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->staticCType = ZR_STATIC_C_TYPE_DYNAMIC;
    typeRef->staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

/* 构造相斥类型绑定的单条加法 ExecBC 指令以审查 SemIR 的冲突降级。
 * 成功返回的函数归调用方，内部指令和类型绑定随 Function_Free 释放。 */
static SZrFunction *create_conflicting_typed_scalar_function(SZrState *state) {
    SZrFunction *function;
    TZrInstruction *instructions;
    SZrFunctionTypedLocalBinding *bindings;

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
    bindings = (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedLocalBinding) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    /* 两次原生申请任一失败都须在交出 function 前释放已取得的缓冲区。 */
    if (instructions == ZR_NULL || bindings == ZR_NULL) {
        if (instructions != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          instructions,
                                          sizeof(TZrInstruction),
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (bindings != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          bindings,
                                          sizeof(SZrFunctionTypedLocalBinding) * 2u,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(instructions, 0, sizeof(*instructions));
    instructions[0].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(ADD_SIGNED);
    instructions[0].instruction.operandExtra = 2u;
    instructions[0].instruction.operand.operand1[0] = 0u;
    instructions[0].instruction.operand.operand1[1] = 1u;

    /* 两个绑定对 slot 0 给出互斥类型，专门触发静态加法的冲突降级。 */
    ZrCore_Memory_RawSet(bindings, 0, sizeof(SZrFunctionTypedLocalBinding) * 2u);
    bindings[0].stackSlot = 0u;
    init_type_ref(&bindings[0].type, ZR_VALUE_TYPE_INT64);
    bindings[1].stackSlot = 0u;
    init_type_ref(&bindings[1].type, ZR_VALUE_TYPE_DOUBLE);

    /* 指令与类型绑定的原生缓冲区所有权一起转入 function。 */
    function->instructionsList = instructions;
    function->instructionsLength = 1u;
    function->stackSize = 3u;
    function->typedLocalBindings = bindings;
    function->typedLocalBindingLength = 2u;
    return function;
}

/* 借出函数持有的目标 SemIR 指令；函数释放后指针失效。 */
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

/* TODO: 当前只覆盖 ADD_SIGNED 的同槽冲突；需用非算术指令和相斥绑定
 * 核查 typed scalar 预映射是否会在识别 opcode 前吞掉原指令类别。 */
/* 同一槽位被标成 int64 与 double 时，带类型加法不能冒充静态 ADD，
 * 应转为动态算术边界并保留可回退信息。 */
static void test_typed_scalar_slot_type_conflict_becomes_dynamic_deopt_boundary(void) {
    SZrState *state = create_test_state();
    SZrFunction *function;
    const SZrSemIrInstruction *instruction;

    TEST_ASSERT_NOT_NULL(state);
    function = create_conflicting_typed_scalar_function(state);
    TEST_ASSERT_NOT_NULL(function);

    /* 用真实元数据构建入口验证类型冲突后的降级结果。 */
    TEST_ASSERT_TRUE(compiler_build_function_semir_metadata(state, function));
    instruction = find_semir_opcode(function, ZR_SEMIR_OPCODE_DYN_ARITHMETIC);
    TEST_ASSERT_NOT_NULL(instruction);
    TEST_ASSERT_NULL(find_semir_opcode(function, ZR_SEMIR_OPCODE_ADD));

    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrInstructionLength);
    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrDeoptTableLength);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME,
                             function->semIrEffectTable[instruction->effectTableIndex].kind);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_RUNTIME_SEMIR_DEOPT_ID_NONE, instruction->deoptId);
    TEST_ASSERT_EQUAL_UINT32(instruction->deoptId, function->semIrDeoptTable[0].deoptId);
    TEST_ASSERT_EQUAL_UINT32(0u, function->semIrDeoptTable[0].execInstructionIndex);
    TEST_ASSERT_EQUAL_UINT32(2u, instruction->destinationSlot);
    TEST_ASSERT_EQUAL_UINT32(0u, instruction->operand0);
    TEST_ASSERT_EQUAL_UINT32(1u, instruction->operand1);
    TEST_ASSERT_EQUAL_UINT32(ZR_STATIC_C_TYPE_DYNAMIC,
                             function->semIrTypeTable[instruction->typeTableIndex].staticCType);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

/* Unity 要求的用例前钩子；夹具在测试函数内创建。 */
void setUp(void) {}

/* BUG: state 创建成功后任一 Unity 断言失败会经 TEST_ABORT/longjmp 跳过
 * 测试函数末尾的 Function_Free/destroy_test_state；空钩子无法补救，
 * 当时已取得的函数或 VM 原生资源在本进程内失去正常清理路径。 */
void tearDown(void) {}

/* 独立 Unity 入口；CMake 以 semir_type_conflict_deopt 注册 CTest。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_scalar_slot_type_conflict_becomes_dynamic_deopt_boundary);
    return UNITY_END();
}
