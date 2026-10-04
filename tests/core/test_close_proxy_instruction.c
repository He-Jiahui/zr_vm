#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

/* 单例测试回调观测：保存相对栈偏移，不在解释器/原生调用切换期间保留 source 裸地址。 */
static TZrUInt32 gCloseCalls;
static TZrMemoryOffset gSourceOffset;
static TZrBool gSourceWasCleared;

void setUp(void) {
    gCloseCalls = 0u;
    gSourceOffset = 0u;
    gSourceWasCleared = ZR_FALSE;
}

void tearDown(void) {}

/* 解释器关闭回调记录原 source 已为 null，避免代理只清自身而保留 local。
 * 回调只观察源槽与次数，不验证 receiver/error 参数或主动 GC。 */
static TZrInt64 close_proxy_instruction_close(SZrState *state) {
    ++gCloseCalls;
    gSourceWasCleared = ZR_VALUE_IS_TYPE_NULL(ZrCore_Stack_GetValueNoProfile(
            ZrCore_Stack_LoadOffsetToPointer(state, gSourceOffset))->type);
    return 0;
}

/* 构造清零的单条指令，再由调用方按 opcode 语义填写 E 与 A1 等操作数。
 * 未填 operand 保持零；此 helper 不检查指令有效性。 */
static TZrInstruction close_proxy_instruction_one(EZrInstructionCode opcode,
                                                   TZrUInt16 operandExtra) {
    TZrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    return instruction;
}

/* 执行手写六条 ExecBC：常量、source登记、高proxy、两次scope close与return，检查一次回调和最终链 sentinel。
 * 真的运行 ZrCore_Execute，但不编译源码、不执行 AOT；没有中间断言单独观察 older marker。 */
static void test_interpreter_registers_proxy_from_e_and_a1_and_preserves_older_marker(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrClosureNative *closer;
    SZrString *name;
    SZrObjectPrototype *prototype;
    SZrObject *object;
    SZrFunction *function;
    SZrCallInfo *callInfo;
    TZrStackValuePointer functionBase;
    SZrTypeValue *functionValue;
    TZrInstruction proxyInstruction;

    TEST_ASSERT_NOT_NULL(state);
    closer = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closer);
    closer->nativeFunction = close_proxy_instruction_close;
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closer));
    name = ZrCore_String_CreateFromNative(state, "CloseProxyInstructionProbe");
    TEST_ASSERT_NOT_NULL(name);
    prototype = ZrCore_ObjectPrototype_New(state, name, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    TEST_ASSERT_NOT_NULL(prototype);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    ZrCore_ObjectPrototype_AddMeta(state, prototype, ZR_META_CLOSE,
                                   ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closer)));
    object = ZrCore_Object_New(state, prototype);
    TEST_ASSERT_NOT_NULL(object);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    ZrCore_Object_Init(state, object);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(SZrTypeValue), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsRawObject(state, function->constantValueList,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    function->constantValueList[0].type = ZR_VALUE_TYPE_OBJECT;
    function->constantValueLength = 1u;
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(TZrInstruction) * 6u, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    /* E=2 的 proxy 指向 A1=1 的 source；两个 CLOSE_SCOPE 各摘一个登记，第二次不能再次关闭原对象。 */
    function->instructionsList[0] = close_proxy_instruction_one(
            ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u);
    function->instructionsList[0].instruction.operand.operand2[0] = 0;
    function->instructionsList[1] = close_proxy_instruction_one(
            ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED), 1u);
    proxyInstruction = close_proxy_instruction_one(
            ZR_INSTRUCTION_ENUM(MARK_CLOSE_PROXY), 2u);
    proxyInstruction.instruction.operand.operand1[0] = 1u;
    function->instructionsList[2] = proxyInstruction;
    function->instructionsList[3] = close_proxy_instruction_one(
            ZR_INSTRUCTION_ENUM(CLOSE_SCOPE), 1u);
    function->instructionsList[4] = close_proxy_instruction_one(
            ZR_INSTRUCTION_ENUM(CLOSE_SCOPE), 1u);
    function->instructionsList[5] = close_proxy_instruction_one(
            ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u);
    function->instructionsList[5].instruction.operand.operand1[0] = 1u;
    function->instructionsLength = 6u;
    function->stackSize = 3u;

    functionBase = state->stackTop.valuePointer;
    functionBase = ZrCore_Function_CheckStackAndGc(state, 1u + function->stackSize,
                                                   functionBase);
    TEST_ASSERT_NOT_NULL(functionBase);
    functionValue = ZrCore_Stack_GetValue(functionBase);
    ZrCore_Value_InitAsRawObject(state, functionValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    functionValue->type = ZR_VALUE_TYPE_FUNCTION;
    functionValue->isGarbageCollectable = ZR_TRUE;
    functionValue->isNative = ZR_FALSE;
    ZrCore_Value_ResetAsNullNoProfile(ZrCore_Stack_GetValueNoProfile(functionBase + 2u));
    gSourceOffset = ZrCore_Stack_SavePointerAsOffset(state, functionBase + 1u);
    state->stackTop.valuePointer = functionBase + 1u + function->stackSize;

    /* 手工建立 CREATE_FRAME call-info 并从首指令进入解释器，避免把静态操作数检查当作实际派发。 */
    callInfo = ZrCore_CallInfo_Extend(state);
    TEST_ASSERT_NOT_NULL(callInfo);
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase,
                                    state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = functionBase;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1u;
    state->callInfoList = callInfo;
    state->threadStatus = ZR_THREAD_STATUS_FINE;

    ZrCore_Execute(state, callInfo);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, gCloseCalls);
    TEST_ASSERT_TRUE(gSourceWasCleared);
    TEST_ASSERT_EQUAL_PTR(state->stackBase.valuePointer,
                          state->toBeClosedValueList.valuePointer);
    ZrTests_Runtime_State_Destroy(state);
}

/* 单条 MARK_CLOSE_PROXY 的高槽9使 generated-frame slot count为10。
 * source槽3低于9；该断言不能独立区分 source operand 扫描是否遗漏。 */
static void test_frame_slot_scan_includes_source_and_high_proxy_operands(void) {
    SZrFunction function = {0};
    TZrInstruction instruction = close_proxy_instruction_one(
            ZR_INSTRUCTION_ENUM(MARK_CLOSE_PROXY), 9u);

    instruction.instruction.operand.operand1[0] = 3u;
    function.instructionsList = &instruction;
    function.instructionsLength = 1u;
    TEST_ASSERT_EQUAL_UINT32(10u,
                             ZrCore_Function_GetGeneratedFrameSlotCount(&function));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_interpreter_registers_proxy_from_e_and_a1_and_preserves_older_marker);
    RUN_TEST(test_frame_slot_scan_includes_source_and_high_proxy_operands);
    return UNITY_END();
}
