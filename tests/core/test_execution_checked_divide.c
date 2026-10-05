#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/string.h"
#include "execution/execution_checked_integer.h"

/* These values own fixture resources beyond Unity's assertion longjmp. Never
 * retain cleanup pointers to automatic owner/weak values or observer captures. */
static SZrState *fixture_state;
static SZrFunction *fixture_function;
static SZrTypeValue fixture_owner;
static SZrTypeValue fixture_weak;
/* has_owned_destination 为真时，offset 指向夹具需兜底释放的 VM 目标槽；保存偏移而非可被栈增长搬走的地址。 */
static TZrMemoryOffset fixture_owned_destination_offset;
static TZrBool fixture_has_owned_destination;

/* 在普通完成和 Unity 断言跳转后的 tearDown 中释放夹具；先撤掉指向局部 trace 的 observer，再关闭登记值。目标槽用保存的栈偏移重新定位，避免清理依赖旧栈指针。 */
static void cleanup_fixture(void) {
    SZrState *state = fixture_state;
    if (state == ZR_NULL) return;
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    ZrCore_Closure_CloseClosure(state, state->stackBase.valuePointer + 1u,
            ZR_THREAD_STATUS_FINE, ZR_FALSE);
    if (fixture_has_owned_destination) {
        TZrStackValuePointer slot = ZrCore_Stack_LoadOffsetToPointer(
                state, fixture_owned_destination_offset);
        ZrCore_Ownership_ReleaseValue(state, ZrCore_Stack_GetValue(slot));
        fixture_has_owned_destination = ZR_FALSE;
    }
    ZrCore_Ownership_ReleaseValue(state, &fixture_owner);
    ZrCore_Ownership_ReleaseValue(state, &fixture_weak);
    ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    if (fixture_function != ZR_NULL) {
        ZrCore_Function_Free(state, fixture_function);
        fixture_function = ZR_NULL;
    }
    ZrTests_Runtime_State_Destroy(state);
    fixture_state = ZR_NULL;
}

static SZrState *new_state(void) {
    fixture_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(fixture_state);
    return fixture_state;
}

/** @brief 为每个 Unity 用例建立空的静态清理记录；不在此处创建 VM。 */
void setUp(void) {
    fixture_state = ZR_NULL;
    fixture_function = ZR_NULL;
    fixture_has_owned_destination = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&fixture_owner);
    ZrCore_Value_ResetAsNull(&fixture_weak);
}

/** @brief 回收被 Unity 普通断言跳转中断的夹具，也容许测试已经主动清理。 */
void tearDown(void) { cleanup_fixture(); }

/* 本组全部 五种 signed 指令形式；各用例按场景选择目的槽、observer 和边界目录。 */
static const EZrInstructionCode divide_forms[] = {
    ZR_INSTRUCTION_ENUM(DIV_SIGNED),
    ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST),
    ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST),
    ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST),
    ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST)
};

/* 同步 observer 的借用记录：function 限定目标函数，算术 offset 与两个计数限定所观察事件；callInfo 仅记当时帧，不拥有该帧。 */
typedef struct DivideTrace {
    SZrFunction *function;
    SZrCallInfo *callInfo;
    TZrUInt32 divideOffset;
    TZrUInt32 observedDivide;
    TZrUInt32 observedFinally;
} DivideTrace;

/* 只计目标函数的算术偏移和 END_FINALLY；在算术处借用当前 callInfo 作观察记录，返回 NONE 不主动请求 trap。 */
static TZrDebugSignal observe_divide(SZrState *state, SZrFunction *function,
        const TZrInstruction *pc, TZrUInt32 offset, TZrUInt32 line, TZrPtr data) {
    DivideTrace *trace = (DivideTrace *)data;
    (void)pc;
    (void)line;
    if (function == trace->function) {
        if (offset == trace->divideOffset) {
            trace->callInfo = state->callInfoList;
            ++trace->observedDivide;
        }
        if (ZR_INSTRUCTION_OPCODE(function->instructionsList[offset]) ==
                ZR_INSTRUCTION_ENUM(END_FINALLY)) ++trace->observedFinally;
    }
    return ZR_DEBUG_SIGNAL_NONE;
}

/* 按普通 E/A1/B1 编码构造手写 VM 指令；融合加载的 operand0 布局由专用构造器补充。 */
static TZrInstruction instruction(EZrInstructionCode code, TZrUInt16 dest,
        TZrUInt16 left, TZrUInt16 right) {
    TZrInstruction result = {0};
    result.instruction.operationCode = (TZrUInt16)code;
    result.instruction.operandExtra = dest;
    result.instruction.operand.operand1[0] = left;
    result.instruction.operand.operand1[1] = right;
    return result;
}

/* 按本组融合 opcode 的 operand0 布局补充取数位置；LOAD_STACK_CONST 先把源槽 0 物化到独立槽 3，再用加载后的槽参与算术。 */
static TZrInstruction divide_instruction(EZrInstructionCode code, TZrUInt16 dest) {
    TZrInstruction result = instruction(code, dest, 0u, 1u);
    if (code == ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST)) {
        result.instruction.operand.operand0[0] = 0u;
        result.instruction.operand.operand0[1] = 1u;
    } else if (code == ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST)) {
        result.instruction.operand.operand0[0] = 0u;
        /* Distinct source and loaded slots prove the load is used by DIV. */
        result.instruction.operand.operand0[1] = 3u;
    }
    return result;
}

/* 用两条常量加载、单条算术和返回搭建最小真实 Core 函数；统一五个栈槽，供目的槽别名及融合加载槽 3 的场景复用。 */
static SZrFunction *new_function(SZrState *state, EZrInstructionCode code,
        TZrInt64 left, TZrInt64 right, TZrUInt16 dest) {
    SZrFunction *function = ZrCore_Function_New(state);
    SZrTypeValue constants[2];
    TZrInstruction instructions[4];
    fixture_function = function;
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_InitAsInt(state, &constants[0], left);
    ZrCore_Value_InitAsInt(state, &constants[1], right);
    instructions[0] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0u, 0u);
    instructions[1] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1u, 0u);
    instructions[2] = divide_instruction(code, dest);
    instructions[3] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, dest, 0u);
    function->instructionsLength = 4u;
    function->constantValueLength = 2u;
    function->instructionsList = ZrCore_Memory_RawMallocWithType(state->global,
            sizeof(instructions), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->constantValueList = ZrCore_Memory_RawMallocWithType(state->global,
            sizeof(constants), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    memcpy(function->instructionsList, instructions, sizeof(instructions));
    memcpy(function->constantValueList, constants, sizeof(constants));
    function->instructionsLength = 4u;
    function->constantValueLength = 2u;
    function->stackSize = 5u;
    return function;
}

/* 从当前规范化异常对象读取 message 字符串，供除零与溢出的消息断言区分失败原因。 */
static const TZrChar *current_error_message(SZrState *state) {
    SZrString *fieldName;
    SZrTypeValue key;
    const SZrTypeValue *field;
    if (!state->hasCurrentException ||
            state->currentException.type != ZR_VALUE_TYPE_OBJECT) return ZR_NULL;
    fieldName = ZrCore_String_CreateFromNative(state, "message");
    if (fieldName == ZR_NULL) return ZR_NULL;
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    field = ZrCore_Object_GetValue(state,
            ZR_CAST_OBJECT(state, state->currentException.value.object), &key);
    return field != ZR_NULL && field->type == ZR_VALUE_TYPE_STRING
            ? ZrCore_String_GetNativeString(ZR_CAST_STRING(state, field->value.object))
            : ZR_NULL;
}

/* 统一检查手写算术函数的成功值或运行时失败；observer 只证明目标算术指令被看见一次，不能证明宿主采用 computed-goto。 */
static void assert_vm_quotient(EZrInstructionCode code, TZrInt64 left,
        TZrInt64 right, TZrBool overflow, TZrInt64 expected,
        TZrBool observer, TZrUInt16 dest) {
    SZrState *state = new_state();
    SZrFunction *function;
    SZrTypeValue result;
    DivideTrace trace = {0};
    TZrBool succeeded;
    EZrThreadStatus status;
    TEST_ASSERT_NOT_NULL(state);
    function = new_function(state, code, left, right, dest);
    trace.function = function;
    trace.divideOffset = 2u;
    if (observer) ZrCore_Debug_SetTraceObserver(state, observe_divide, &trace);
    succeeded = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
    status = state->threadStatus;
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    if (overflow) {
        TEST_ASSERT_FALSE_MESSAGE(succeeded, "legacy signed divide must reject overflow");
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
        TEST_ASSERT_TRUE(state->hasCurrentException);
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, state->currentExceptionStatus);
        TEST_ASSERT_EQUAL_STRING(right == 0 ? "divide by zero"
                : "signed integer division overflow", current_error_message(state));
    } else {
        TEST_ASSERT_TRUE(succeeded);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
        TEST_ASSERT_EQUAL_INT64(expected, result.value.nativeObject.nativeInt64);
    }
    if (observer) TEST_ASSERT_EQUAL_UINT32(1u, trace.observedDivide);
    cleanup_fixture();
}

/* 把一个明确的不可表示算术输入送过全部指令形式，并分别启用/停用 observer，防止某个形式绕过 checked 路径。 */
static void test_all_signed_divide_forms_reject_overflow(void) {
    size_t form;
    TZrBool observer;
    for (form = 0u; form < sizeof(divide_forms) / sizeof(divide_forms[0]); ++form)
        for (observer = 0; observer <= 1; ++observer)
            assert_vm_quotient(divide_forms[form], INT64_MIN, -1,
                    ZR_TRUE, 0, observer, 2u);
}

/* 边界目录的一条输入与期望；overflow 在除法也包括零除失败，失败行的 result 只是占位，不应被作为成功结果读取。 */
typedef struct DivideCase {
    TZrInt64 left, right, result;
    TZrBool overflow;
} DivideCase;

/* 19 组离散边界：成功值与失败标志共同驱动 helper 和 VM 矩阵，不是全域穷举。 */
static const DivideCase boundary_cases[] = {
    {0, INT64_MIN, 0, 0}, {0, INT64_MAX, 0, 0},
    {INT64_MAX, 1, INT64_MAX, 0}, {INT64_MIN, 1, INT64_MIN, 0},
    {INT64_MAX, -1, -INT64_MAX, 0}, {INT64_MIN, -1, 0, 1},
    {INT64_MIN, 2, INT64_MIN / 2, 0},
    {INT64_MIN, -2, -(INT64_MIN / 2), 0},
    {INT64_MIN, INT64_MIN, 1, 0}, {INT64_MAX, INT64_MIN, 0, 0},
    {INT64_MIN, INT64_MAX, -1, 0}, {1, INT64_MIN, 0, 0},
    {13, 7, 1, 0}, {-13, 7, -1, 0}, {13, -7, -1, 0}, {-13, -7, 1, 0},
    {0, 0, 0, 1}, {1, 0, 0, 1}, {INT64_MIN, 0, 0, 1}
};

/* 直接核对纯 checked helper 的边界结果和失败时的输出哨兵，再检查空输出及输入变量作输出的别名。 */
static void test_checked_helper_boundaries_preserve_failure_output(void) {
    size_t index;
    TZrInt64 alias = INT64_MIN;
    for (index = 0; index < sizeof(boundary_cases) / sizeof(boundary_cases[0]); ++index) {
        const DivideCase *item = &boundary_cases[index];
        TZrInt64 output = 71;
        TZrBool success = execution_checked_i64_divide(item->left, item->right, &output);
        TEST_ASSERT_EQUAL_INT(!item->overflow, success);
        TEST_ASSERT_EQUAL_INT64(item->overflow ? 71 : item->result, output);
    }
    TEST_ASSERT_FALSE(execution_checked_i64_divide(2, 1, ZR_NULL));
    TEST_ASSERT_FALSE(execution_checked_i64_divide(alias, -1, &alias));
    TEST_ASSERT_EQUAL_INT64(INT64_MIN, alias);
    TEST_ASSERT_FALSE(execution_checked_i64_divide(alias, 0, &alias));
    TEST_ASSERT_EQUAL_INT64(INT64_MIN, alias);
    TEST_ASSERT_TRUE(execution_checked_i64_divide(alias, 1, &alias));
    TEST_ASSERT_EQUAL_INT64(INT64_MIN, alias);
}

/* 将边界目录逐项送入每种 VM 形式与 observer 开关，核对成功结果及运行时错误归一。 */
static void test_vm_boundaries_all_forms_and_dispatch_modes(void) {
    size_t form, index;
    TZrBool observer;
    for (form = 0; form < sizeof(divide_forms) / sizeof(divide_forms[0]); ++form)
        for (index = 0; index < sizeof(boundary_cases) / sizeof(boundary_cases[0]); ++index)
            for (observer = 0; observer <= 1; ++observer) {
                const DivideCase *item = &boundary_cases[index];
                assert_vm_quotient(divide_forms[form], item->left, item->right,
                        item->overflow, item->result, observer, 2u);
            }
}

/* 让目的槽依次复用左、右源槽，分别检查普通负数结果与溢出拒绝，防止存储提前覆盖尚需读取的操作数。 */
static void test_destination_aliases_each_operand(void) {
    size_t form;
    TZrUInt16 destination;
    for (form = 0; form < sizeof(divide_forms) / sizeof(divide_forms[0]); ++form)
        for (destination = 0; destination <= 1; ++destination) {
            assert_vm_quotient(divide_forms[form], -13, 7, ZR_FALSE, -1, ZR_FALSE, destination);
            assert_vm_quotient(divide_forms[form], INT64_MIN, -1, ZR_TRUE, 0, ZR_TRUE, destination);
        }
}

/* 把预先保留的 VM callInfo 交给 TryRun 执行，以便异常后仍能检查该记录保存的故障 PC。 */
static void execute_call_info(SZrState *state, TZrPtr arguments) {
    ZrCore_Execute(state, (SZrCallInfo *)arguments);
}

/* 手工发布 function 值和空局部槽，再挂起 CREATE_FRAME 的 callInfo；供保留故障 PC 和预置 owned 目标槽的用例使用。 */
static SZrCallInfo *prepare_call_info(SZrState *state, SZrFunction *function) {
    SZrTypeValue callable;
    TZrStackValuePointer base;
    SZrCallInfo *callInfo;
    TZrUInt32 index;
    ZrCore_Value_InitAsRawObject(state, &callable, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    callable.type = ZR_VALUE_TYPE_FUNCTION;
    callable.isGarbageCollectable = ZR_TRUE;
    callable.isNative = ZR_FALSE;
    base = ZrCore_Function_CheckStackAndGc(state, function->stackSize + 1u,
            state->stackTop.valuePointer);
    TEST_ASSERT_NOT_NULL(base);
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), &callable);
    for (index = 0; index < function->stackSize; ++index)
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base + index + 1u));
    state->stackTop.valuePointer = base + function->stackSize + 1u;
    callInfo = ZrCore_CallInfo_Extend(state);
    TEST_ASSERT_NOT_NULL(callInfo);
    ZrCore_CallInfo_EntryNativeInit(state, callInfo, state->stackBase, state->stackTop,
            state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1u;
    state->callInfoList = callInfo;
    return callInfo;
}

/* 保留故障调用记录核对 PC 与异常栈清空，再重置同一线程、替换常量并成功重跑，检查失败不会阻止下一次调用。 */
static void test_overflow_saves_faulting_pc_and_thread_recovers(void) {
    TZrBool observer;
    for (observer = 0; observer <= 1; ++observer) {
        SZrState *state = new_state();
        SZrFunction *function = new_function(state, divide_forms[0], INT64_MIN, -1, 2u);
        SZrCallInfo *callInfo = prepare_call_info(state, function);
        DivideTrace trace = {0};
        EZrThreadStatus status;
        SZrTypeValue result;
        trace.function = function;
        trace.divideOffset = 2u;
        if (observer) ZrCore_Debug_SetTraceObserver(state, observe_divide, &trace);
        status = ZrCore_Exception_TryRun(state, execute_call_info, callInfo);
        ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
        TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
        TEST_ASSERT_EQUAL_PTR(function->instructionsList + 2u,
                callInfo->context.context.programCounter);
        TEST_ASSERT_TRUE(state->hasCurrentException);
        TEST_ASSERT_EQUAL_UINT32(0u, state->exceptionHandlerStackLength);
        ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
        function->constantValueList[0].value.nativeObject.nativeInt64 = -13;
        function->constantValueList[1].value.nativeObject.nativeInt64 = 7;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(-1, result.value.nativeObject.nativeInt64);
        TEST_ASSERT_FALSE(state->hasCurrentException);
        cleanup_fixture();
    }
}

/* 手写一个无类型筛选的 catch 和 finally 元数据，检查算术失败能被消费并执行 END_FINALLY；随后去掉 catch 检查 finally 后仍传播失败。 */
static void test_overflow_runs_catch_and_finally(void) {
    SZrState *state = new_state();
    SZrFunction *function = new_function(state, divide_forms[0], INT64_MIN, -1, 2u);
    SZrTypeValue result;
    TZrInstruction code[11];
    SZrFunctionExceptionHandlerInfo handler = {0};
    SZrFunctionCatchClauseInfo catchInfo = {0};
    DivideTrace trace = {0};
    code[0] = function->instructionsList[0];
    code[1] = function->instructionsList[1];
    code[2] = instruction(ZR_INSTRUCTION_ENUM(TRY), 0u, 0u, 0u);
    code[3] = divide_instruction(divide_forms[0], 2u);
    code[4] = instruction(ZR_INSTRUCTION_ENUM(END_TRY), 0u, 0u, 0u);
    code[5] = instruction(ZR_INSTRUCTION_ENUM(NOP), 0u, 0u, 0u);
    code[6] = instruction(ZR_INSTRUCTION_ENUM(CATCH), 3u, 0u, 0u);
    code[7] = instruction(ZR_INSTRUCTION_ENUM(END_TRY), 0u, 0u, 0u);
    code[8] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2u, 1u, 0u);
    code[9] = instruction(ZR_INSTRUCTION_ENUM(END_FINALLY), 0u, 0u, 0u);
    code[10] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 2u, 0u);
    ZrCore_Memory_RawFreeWithType(state->global, function->instructionsList,
            sizeof(TZrInstruction) * function->instructionsLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->instructionsList = ZrCore_Memory_RawMallocWithType(state->global,
            sizeof(code), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    memcpy(function->instructionsList, code, sizeof(code));
    function->instructionsLength = 11u;
    handler.protectedStartInstructionOffset = 3u;
    handler.finallyTargetInstructionOffset = 8u;
    handler.afterFinallyInstructionOffset = 10u;
    handler.catchClauseCount = 1u;
    handler.hasFinally = ZR_TRUE;
    catchInfo.targetInstructionOffset = 6u;
    function->exceptionHandlerList = ZrCore_Memory_RawMallocWithType(state->global,
            sizeof(handler), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->catchClauseList = ZrCore_Memory_RawMallocWithType(state->global,
            sizeof(catchInfo), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    memcpy(function->exceptionHandlerList, &handler, sizeof(handler));
    memcpy(function->catchClauseList, &catchInfo, sizeof(catchInfo));
    function->exceptionHandlerCount = 1u;
    function->catchClauseCount = 1u;
    trace.function = function;
    trace.divideOffset = 3u;
    ZrCore_Debug_SetTraceObserver(state, observe_divide, &trace);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(-1, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.observedDivide);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.observedFinally);
    TEST_ASSERT_EQUAL_UINT32(0u, state->exceptionHandlerStackLength);
    TEST_ASSERT_FALSE(state->hasCurrentException);
    /* Without a catch, finally still runs and the original failure propagates. */
    handler.catchClauseCount = 0u;
    function->exceptionHandlerList[0] = handler;
    trace.observedFinally = trace.observedDivide = 0u;
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, state->threadStatus);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.observedFinally);
    TEST_ASSERT_EQUAL_UINT32(0u, state->exceptionHandlerStackLength);
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    cleanup_fixture();
}

/* 把左常量改为 -2.5、右常量保留整数 2，检查各 signed 形式仍得到 DOUBLE 浮点结果。 */
static void test_float_fallback_remains_available(void) {
    size_t form;
    for (form = 0; form < sizeof(divide_forms) / sizeof(divide_forms[0]); ++form) {
        SZrState *state = new_state();
        SZrFunction *function = new_function(state, divide_forms[form], 0, 2, 2u);
        SZrTypeValue result;
        ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], -2.5);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.type);
        TEST_ASSERT_EQUAL_DOUBLE(-1.25, result.value.nativeObject.nativeDouble);
        cleanup_fixture();
    }
}

/* 让 shared 字符串目标成为最后一份 strong，并保留 weak 观察其失效；比较正常写回与 checked 失败 unwind 的释放结果。 */
static void test_owned_destination_releases_on_store_and_overflow_unwind(void) {
    size_t form;
    TZrBool overflow;
    /* PLAIN_DEST is admitted only for plain slots: exercising owned metadata
     * there would violate that opcode's existing frame-layout precondition. */
    for (form = 0; form < sizeof(divide_forms) / sizeof(divide_forms[0]); ++form) {
        if (form == 2u) continue;
        for (overflow = 0; overflow <= 1; ++overflow) {
            SZrState *state = new_state();
            SZrFunction *function = new_function(state, divide_forms[form],
                    overflow ? INT64_MIN : -13, overflow ? -1 : 7, 2u);
            SZrTypeValue plain;
            SZrString *string = ZrCore_String_CreateFromNative(state, "owned divide destination");
            SZrCallInfo *callInfo;
            SZrTypeValue *destination;
            TZrInstruction code[5];
            EZrThreadStatus status;
            TEST_ASSERT_NOT_NULL(string);
            code[0] = function->instructionsList[0];
            code[1] = function->instructionsList[1];
            /* proxy 槽 4 登记目标槽 2，使算术失败前的 owned 值进入既有 unwind 关闭链。 */
            code[2] = instruction(ZR_INSTRUCTION_ENUM(MARK_CLOSE_PROXY), 4u, 2u, 0u);
            code[3] = function->instructionsList[2];
            code[4] = function->instructionsList[3];
            ZrCore_Memory_RawFreeWithType(state->global, function->instructionsList,
                    sizeof(TZrInstruction) * function->instructionsLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            function->instructionsList = ZrCore_Memory_RawMallocWithType(state->global,
                    sizeof(code), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            memcpy(function->instructionsList, code, sizeof(code));
            function->instructionsLength = 5u;
            callInfo = prepare_call_info(state, function);
            destination = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer + 3u);
            fixture_owned_destination_offset = ZrCore_Stack_SavePointerAsOffset(
                    state, callInfo->functionBase.valuePointer + 3u);
            fixture_has_owned_destination = ZR_TRUE;
            ZrCore_Value_InitAsRawObject(state, &plain, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
            plain.type = ZR_VALUE_TYPE_STRING;
            ZrCore_Value_ResetAsNull(&fixture_owner);
            ZrCore_Value_ResetAsNull(&fixture_weak);
            TEST_ASSERT_TRUE(ZrCore_Ownership_SharePlainValue(state, &fixture_owner, &plain));
            TEST_ASSERT_TRUE(ZrCore_Ownership_DegradeValue(state, &fixture_weak, &fixture_owner));
            /* 把 strong 留在目标槽后撤掉夹具 owner；显式 weak 保证控制块仍可供释放断言读取。 */
            ZrCore_Value_Copy(state, destination, &fixture_owner);
            ZrCore_Ownership_ReleaseValue(state, &fixture_owner);
            TEST_ASSERT_EQUAL_UINT32(1u, fixture_weak.ownershipControl->strongRefCount);
            status = ZrCore_Exception_TryRun(state, execute_call_info, callInfo);
            TEST_ASSERT_EQUAL_INT(overflow ? ZR_THREAD_STATUS_RUNTIME_ERROR : ZR_THREAD_STATUS_FINE, status);
            TEST_ASSERT_EQUAL_UINT32(0u, fixture_weak.ownershipControl->strongRefCount);
            TEST_ASSERT_FALSE(fixture_weak.ownershipControl->objectIsAlive);
            if (overflow) {
                TEST_ASSERT_EQUAL_PTR(function->instructionsList + 3u,
                        callInfo->context.context.programCounter);
                TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destination->ownershipKind);
                TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(destination->type));
            }
            ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
            ZrCore_Ownership_ReleaseValue(state, &fixture_weak);
            cleanup_fixture();
        }
    }
}

/** @brief 注册本翻译单元的八个 Unity 场景，作为 checked integer 独立 CTest 可执行文件入口。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_all_signed_divide_forms_reject_overflow);
    RUN_TEST(test_checked_helper_boundaries_preserve_failure_output);
    RUN_TEST(test_vm_boundaries_all_forms_and_dispatch_modes);
    RUN_TEST(test_destination_aliases_each_operand);
    RUN_TEST(test_overflow_saves_faulting_pc_and_thread_recovers);
    RUN_TEST(test_overflow_runs_catch_and_finally);
    RUN_TEST(test_float_fallback_remains_available);
    RUN_TEST(test_owned_destination_releases_on_store_and_overflow_unwind);
    return UNITY_END();
}
