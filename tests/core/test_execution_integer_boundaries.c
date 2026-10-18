#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

void setUp(void) {}
void tearDown(void) {}

typedef enum OperandForm { SLOTS, CONSTANT, LOAD_CONSTANT, LOAD_STACK_CONSTANT,
                           LOAD_STACK, LOAD_STACK_LOAD_CONSTANT, ADD_MOD } OperandForm;
typedef struct ArithmeticForm { EZrInstructionCode opcode; OperandForm form; } ArithmeticForm;
#define FORM(NAME, KIND) { ZR_INSTRUCTION_ENUM(NAME), KIND }
static const ArithmeticForm adds[] = {
    FORM(ADD, SLOTS), FORM(ADD_INT, SLOTS), FORM(ADD_INT_PLAIN_DEST, SLOTS),
    FORM(ADD_INT_CONST, CONSTANT), FORM(ADD_INT_CONST_PLAIN_DEST, CONSTANT),
    FORM(ADD_SIGNED, SLOTS), FORM(ADD_SIGNED_PLAIN_DEST, SLOTS),
    FORM(ADD_SIGNED_CONST, CONSTANT), FORM(ADD_SIGNED_CONST_PLAIN_DEST, CONSTANT),
    FORM(ADD_SIGNED_LOAD_CONST, LOAD_CONSTANT),
    FORM(ADD_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONSTANT),
    FORM(ADD_SIGNED_LOAD_STACK, LOAD_STACK),
    FORM(ADD_SIGNED_LOAD_STACK_LOAD_CONST, LOAD_STACK_LOAD_CONSTANT),
    FORM(ADD_SIGNED_MOD_CONST, ADD_MOD)
};
static const ArithmeticForm subtracts[] = {
    FORM(SUB, SLOTS), FORM(SUB_INT, SLOTS), FORM(SUB_INT_PLAIN_DEST, SLOTS),
    FORM(SUB_INT_CONST, CONSTANT), FORM(SUB_INT_CONST_PLAIN_DEST, CONSTANT),
    FORM(SUB_SIGNED, SLOTS), FORM(SUB_SIGNED_PLAIN_DEST, SLOTS),
    FORM(SUB_SIGNED_CONST, CONSTANT), FORM(SUB_SIGNED_CONST_PLAIN_DEST, CONSTANT),
    FORM(SUB_SIGNED_LOAD_CONST, LOAD_CONSTANT),
    FORM(SUB_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONSTANT)
};

static TZrInstruction instruction(EZrInstructionCode code, TZrUInt16 dest,
                                  TZrUInt16 left, TZrUInt16 right) {
    TZrInstruction inst = {0};
    inst.instruction.operationCode = code;
    inst.instruction.operandExtra = dest;
    inst.instruction.operand.operand1[0] = left;
    inst.instruction.operand.operand1[1] = right;
    return inst;
}

/* Execute real bytecode, including the packed operand layouts and aliased
 * destinations used by quickening. An error must occur before any host UB. */
static void check(ArithmeticForm form, TZrInt64 left, TZrInt64 right,
                  TZrBool succeeds, TZrInt64 expected, TZrUInt16 dest) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrTypeValue constants[3], result;
    TZrInstruction code[4];
    TZrBool success;
    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_InitAsInt(state, &constants[0], left);
    ZrCore_Value_InitAsInt(state, &constants[1], right);
    ZrCore_Value_InitAsInt(state, &constants[2], 7);
    code[0] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0, 0);
    code[1] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1, 0);
    code[2] = instruction(form.opcode, dest, 0, 1);
    if (form.form == LOAD_CONSTANT || form.form == LOAD_STACK || form.form == ADD_MOD) {
        code[2].instruction.operand.operand0[0] = 0;
        code[2].instruction.operand.operand0[1] = 1;
        if (form.form != LOAD_STACK) code[2].instruction.operand.operand1[1] = form.form == ADD_MOD ? 2 : 1;
    } else if (form.form == LOAD_STACK_CONSTANT || form.form == LOAD_STACK_LOAD_CONSTANT) {
        code[2].instruction.operand.operand0[0] = 0;
        code[2].instruction.operand.operand0[1] = 3;
        code[2].instruction.operand.operand1[1] = 1;
    }
    code[3] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, dest, 0);
    function->instructionsList = ZrCore_Memory_RawMallocWithType(state->global, sizeof(code), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->constantValueList = ZrCore_Memory_RawMallocWithType(state->global, sizeof(constants), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    memcpy(function->instructionsList, code, sizeof(code));
    memcpy(function->constantValueList, constants, sizeof(constants));
    function->instructionsLength = 4;
    function->constantValueLength = 3;
    function->stackSize = 6;
    success = ZrTests_Runtime_Function_Execute(state, function, &result);
    /* Release the VM before assertions, which can longjmp out of a test. */
    if (!success) ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    TEST_ASSERT_EQUAL_INT_MESSAGE(succeeds, success, "Unexpected arithmetic completion");
    if (success) {
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(expected, result.value.nativeObject.nativeInt64);
    }
}

static void test_checked_add_and_subtract_variants(void) {
    for (unsigned dest = 0; dest < 3; ++dest) {
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(adds); ++i) {
            check(adds[i], INT64_MAX, 1, ZR_FALSE, 0, dest);
            check(adds[i], INT64_MIN, -1, ZR_FALSE, 0, dest);
            check(adds[i], 8, -3, ZR_TRUE, 5, dest);
        }
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(subtracts); ++i) {
            check(subtracts[i], INT64_MIN, 1, ZR_FALSE, 0, dest);
            check(subtracts[i], INT64_MAX, -1, ZR_FALSE, 0, dest);
            check(subtracts[i], -8, -3, ZR_TRUE, -5, dest);
        }
    }
}

static void test_checked_negation(void) {
    const ArithmeticForm forms[] = { FORM(NEG, SLOTS), FORM(NEG_SIGNED, SLOTS) };
    for (unsigned i = 0; i < ZR_ARRAY_COUNT(forms); ++i) {
        check(forms[i], INT64_MIN, 0, ZR_FALSE, 0, 0);
        check(forms[i], -INT64_MAX, 0, ZR_TRUE, INT64_MAX, 0);
    }
}

static void test_masked_shifts(void) {
    const ArithmeticForm lefts[] = { FORM(SHIFT_LEFT_INT, SLOTS), FORM(BITWISE_SHIFT_LEFT, SLOTS) };
    const ArithmeticForm rights[] = { FORM(SHIFT_RIGHT_INT, SLOTS), FORM(BITWISE_SHIFT_RIGHT, SLOTS) };
    for (unsigned i = 0; i < ZR_ARRAY_COUNT(lefts); ++i) {
        check(lefts[i], 1, -1, ZR_TRUE, INT64_MIN, 0);
        check(lefts[i], -1, 64, ZR_TRUE, -1, 0);
        check(lefts[i], INT64_MIN, 1, ZR_TRUE, 0, 0);
        check(rights[i], INT64_MIN, -1, ZR_TRUE, -1, 0);
        check(rights[i], -2, 65, ZR_TRUE, -1, 0);
        check(rights[i], INT64_MIN, 64, ZR_TRUE, INT64_MIN, 0);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_checked_add_and_subtract_variants);
    RUN_TEST(test_checked_negation);
    RUN_TEST(test_masked_shifts);
    return UNITY_END();
}

/**
 * @enum OperandForm
 *
 * ## Summary
 * 给本测试的算术形式选择操作数字段构造方式，使普通槽操作数、常量索引和融合加载指令使用各自真实的布局。
 *
 * ## Fields
 * SLOTS 保留两个 16 位槽索引；CONSTANT 保留左槽和右常量索引；LOAD_CONSTANT、LOAD_STACK_CONSTANT、LOAD_STACK、LOAD_STACK_LOAD_CONSTANT 分别选择下层融合加载约定；ADD_MOD 为加法后取模另选常量 2。
 *
 * ## Constraints and lifecycle
 * 这七个值是测试私有的构造选择器，不是 VM opcode，也不是持久化格式。必须与 ArithmeticForm.opcode 配对；check 不验证任意组合。底层 operand0 与 operand1 是重叠视图，布局由对应 handler 解释。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:11
 * - tests/core/test_execution_integer_boundaries.c:13
 * - tests/core/test_execution_integer_boundaries.c:63
 * - tests/core/test_execution_integer_boundaries.c:67
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5101
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5110
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5120
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5127
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5145
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5156
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5165
 * - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:388
 */

/**
 * @struct ArithmeticForm
 *
 * ## Summary
 * 把一个待执行的 opcode 与测试构造其操作数的方式绑定，供表驱动用例按值传入 check。
 *
 * ## Fields
 * opcode 是实际 EZrInstructionCode；form 是 OperandForm。两字段共同决定常量池、栈槽和融合副本的位置。
 *
 * ## Constraints and lifecycle
 * 只包含值字段，不拥有 VM、指令池或常量池。全局 const 表和各用例局部表提供已选定的合法配对；该类型没有自动校验，不能把任意 opcode/form 组合当作受支持字节码。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:13
 * - tests/core/test_execution_integer_boundaries.c:14
 * - tests/core/test_execution_integer_boundaries.c:47
 * - tests/core/test_execution_integer_boundaries.c:62
 * - tests/core/test_execution_integer_boundaries.c:110
 * - tests/core/test_execution_integer_boundaries.c:118
 */

/**
 * @def FORM(NAME, KIND)
 *
 * ## Summary
 * 为有限算术表生成 ArithmeticForm 的聚合初始化器，将指令名交给统一的 opcode 拼接宏。
 *
 * ## Parameters
 * @param[in] NAME 已经注册的指令名 token，不是字符串或运行期 opcode 值。
 * @param[in] KIND 与该指令布局对应的 OperandForm 表达式；本文件所有调用均使用枚举常量。
 *
 * ## Returns
 * 展开为包含 opcode 与 form 的花括号初始化器；不执行 VM、不分配资源，也不提供运行期失败状态。
 *
 * ## Constraints and lifecycle
 * NAME 经 ZR_INSTRUCTION_ENUM 生成枚举符号，KIND 原样进入第二字段。应只用于 ArithmeticForm 初始化上下文；宏不会验证配对是否符合 handler 的位宽或索引约定。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:14
 * - tests/core/test_execution_integer_boundaries.c:16
 * - tests/core/test_execution_integer_boundaries.c:27
 * - tests/core/test_execution_integer_boundaries.c:110
 * - tests/core/test_execution_integer_boundaries.c:118
 * - tests/core/test_execution_integer_boundaries.c:119
 * - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:296
 */

/**
 * @var static const ArithmeticForm adds[]
 *
 * ## Summary
 * 列出本目标要执行的 14 种加法形式，将普通、INT、SIGNED、常量及五种融合布局纳入同一有限边界矩阵。
 *
 * ## Fields
 * 数组元素均为 ArithmeticForm；前 9 项是普通槽或常量形式，后 5 项是融合加载或加法取模形式。
 *
 * ## Constraints and lifecycle
 * 静态 const 数组只供本 TU 的算术用例读取，无需释放。表项不是完整 opcode 覆盖清单。ADD_SIGNED_MOD_CONST 的第三常量由 check 固定为 7，成功控制只验证 (8 + -3) % 7；没有覆盖零除数或所有模语义。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:15
 * - tests/core/test_execution_integer_boundaries.c:24
 * - tests/core/test_execution_integer_boundaries.c:59
 * - tests/core/test_execution_integer_boundaries.c:66
 * - tests/core/test_execution_integer_boundaries.c:96
 * - tests/core/test_execution_integer_boundaries.c:97
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5127
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5142
 */

/**
 * @var static const ArithmeticForm subtracts[]
 *
 * ## Summary
 * 列出本目标的 11 种减法形式，复用同一执行夹具检验上下界拒绝及可表示结果。
 *
 * ## Fields
 * 数组包含普通、INT、SIGNED、常量、LOAD_CONST 和 LOAD_STACK_CONST 配对。
 *
 * ## Constraints and lifecycle
 * 静态 const 值数组只由本 TU 的算术用例读取。它不包含所有减法派生 opcode，也不引入额外 VM 资源；每个配对须遵循其 handler 的槽与常量域。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:26
 * - tests/core/test_execution_integer_boundaries.c:31
 * - tests/core/test_execution_integer_boundaries.c:32
 * - tests/core/test_execution_integer_boundaries.c:101
 * - tests/core/test_execution_integer_boundaries.c:102
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5156
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5165
 */

/**
 * @fn TZrInstruction instruction(EZrInstructionCode code, TZrUInt16 dest, TZrUInt16 left, TZrUInt16 right)
 *
 * ## Summary
 * 为 check 构造一条先清零的真实 VM 指令，统一写入 opcode、额外目标字段和两个 16 位操作数，再由调用方按融合布局改写重叠的字节视图。
 *
 * ## Parameters
 * @param[in] code 要写入 operationCode 的指令枚举；本 helper 不检查 opcode 或操作数 schema。
 * @param[in] dest 写入 operandExtra 的目标或返回控制字段，其具体含义由 code 决定。
 * @param[in] left 写入 operand1[0] 的值；调用方按 opcode 将其解释为槽或其他字段。
 * @param[in] right 写入 operand1[1] 的值；调用方按 opcode 将其解释为槽或常量索引。
 *
 * ## Returns
 * @return 按值返回已清零并写入上述字段的 TZrInstruction，不分配或借出存储；不代表字节码合法性验证成功。
 *
 * ## Constraints and lifecycle
 * 使用本机 TZrInstruction 联合体布局；operand0 与 operand1 不是独立字段。check 的 packed 改写必须与实际 handler 一致，不能将此构造器视为跨端序编码器。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:35
 * - tests/core/test_execution_integer_boundaries.c:60
 * - tests/core/test_execution_integer_boundaries.c:62
 * - tests/core/test_execution_integer_boundaries.c:63
 * - tests/core/test_execution_integer_boundaries.c:72
 * - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:388
 */

/**
 * @fn void check(ArithmeticForm form, TZrInt64 left, TZrInt64 right, TZrBool succeeds, TZrInt64 expected, TZrUInt16 dest)
 *
 * ## Summary
 * 在独立 VM 中执行由两个常量装载、一条待测运算和返回组成的四条真实字节码，将有限边界输入落实到真实 packed handlers，并在执行普通返回后先释放 VM 资源再判断完成状态和整数结果。
 *
 * ## Parameters
 * @param[in] form 按值传入的受测 opcode/布局配对；当前调用只使用全局或局部固定表。
 * @param[in] left 写入常量 0 并装载到槽 0 的有符号输入。
 * @param[in] right 写入常量 1 并装载到槽 1 的有符号输入；常量类指令直接引用常量 1。
 * @param[in] succeeds 该输入期望的完成布尔值；仅比较 Execute 返回结果，不断言具体异常代码或诊断文本。
 * @param[in] expected Execute 成功时比较的有符号整数值；预期失败场景传入的占位值不被用作结果断言。
 * @param[in] dest 算术目标槽；当前调用只用 0、1、2，其中 0/1 可与原输入槽同址。
 *
 * ## Returns
 * 无 C 返回值。普通执行路径先处理失败线程状态、释放函数附属存储和独立 global，然后经 Unity 断言报告结果；断言失败退出当前 case，runner 记录失败后可继续其余 case。
 *
 * ## Constraints and lifecycle
 * 函数有 4 条指令、3 个常量和 6 个栈槽。LOAD_STACK_CONSTANT 与 LOAD_STACK_LOAD_CONSTANT 先把槽 0 物化到槽 3，再由 handler 读取；后一形式还使用 operand0[2] 指定常量副本槽。联合体字节位置遵循本机实际布局。两个 RawMallocWithType 在发布长度前完成并经断言检查；它们不提供本测试的 OOM 回滚证明。执行的普通 FALSE 路径可打印未处理异常诊断并重置线程，不能要求 stderr 为空。Function_Free 释放附属池并将 GC 对象变成 tombstone，State_Destroy 再释放 global/collector；整数结果按值保留，不能由此推及对象结果在 VM 销毁后的可用性。较早的 state/function/池分配断言或其他非局部跳转可能跳过本地尾部清理，空 tearDown 不补做回收。每次调用拥有独立 VM，当前 runner 串行执行；未测试自然 OOM、强制 GC、任意损坏字节码或宿主 UB 检测器。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:49
 * - tests/core/test_execution_integer_boundaries.c:63
 * - tests/core/test_execution_integer_boundaries.c:67
 * - tests/core/test_execution_integer_boundaries.c:73
 * - tests/core/test_execution_integer_boundaries.c:75
 * - tests/core/test_execution_integer_boundaries.c:79
 * - tests/core/test_execution_integer_boundaries.c:82
 * - tests/core/test_execution_integer_boundaries.c:84
 * - tests/core/test_execution_integer_boundaries.c:85
 * - tests/core/test_execution_integer_boundaries.c:86
 * - tests/core/test_execution_integer_boundaries.c:87
 * - tests/core/test_execution_integer_boundaries.c:89
 * - tests/core/test_execution_integer_boundaries.c:90
 * - tests/harness/runtime_support.c:413
 * - tests/harness/runtime_support.c:272
 * - tests/harness/runtime_support.c:435
 * - tests/harness/runtime_support.c:447
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5114
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5149
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5151
 * - zr_vm_core/src/zr_vm_core/function/function.c:1390
 * - zr_vm_core/src/zr_vm_core/function/function.c:1396
 * - zr_vm_core/src/zr_vm_core/function/function.c:1668
 * - tests/harness/runtime_support.c:403
 * - zr_vm_core/src/zr_vm_core/runtime/global.c:758
 */

/**
 * @fn void test_checked_add_and_subtract_variants(void)
 *
 * ## Summary
 * 对 14 种加法和 11 种减法形式分别使用目标槽 0、1、2，检验两个方向的有符号越界拒绝及一个可表示负数参与的正常结果，防止常量、packed 加载和目标别名路径绕开相同边界意图。
 *
 * ## Returns
 * 无 C 返回值。每次 check 的断言通过后进入下一个表项；Unity 断言失败结束当前 case，尚未遍历的表项不获得执行或覆盖信用，runner 可继续另外两个 case。
 *
 * ## Constraints and lifecycle
 * 若全部到达，静态循环安排 225 次 check，其中 150 次期望 FALSE、75 次期望 TRUE；这是源码矩阵计数，不是运行 trace。目标槽 0/1 与原输入槽同址，2 为独立结果槽。融合取模只使用非零常量 7，不能外推零除数、全部模边界、所有 opcode 或 UB/Sanitizer 证明。每次 check 自管独立 VM；早期断言的资源限制见 check。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:94
 * - tests/core/test_execution_integer_boundaries.c:95
 * - tests/core/test_execution_integer_boundaries.c:96
 * - tests/core/test_execution_integer_boundaries.c:97
 * - tests/core/test_execution_integer_boundaries.c:98
 * - tests/core/test_execution_integer_boundaries.c:99
 * - tests/core/test_execution_integer_boundaries.c:101
 * - tests/core/test_execution_integer_boundaries.c:102
 * - tests/core/test_execution_integer_boundaries.c:103
 * - tests/core/test_execution_integer_boundaries.c:104
 * - tests/core/test_execution_integer_boundaries.c:132
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5101
 * - zr_vm_core/include/zr_vm_core/integer.h:9
 * - zr_vm_core/include/zr_vm_core/integer.h:26
 */

/**
 * @fn void test_checked_negation(void)
 *
 * ## Summary
 * 在 NEG 与 NEG_SIGNED 的有符号输入路径中，分别要求最小 64 位整数取负被拒绝，并用 -INT64_MAX 的可表示结果作控制。
 *
 * ## Returns
 * 无 C 返回值；通过 check 的 Unity 断言报告场景结果。断言失败仅截断当前 case 的剩余循环，runner 可继续后续 case。
 *
 * ## Constraints and lifecycle
 * 静态全部到达时为 4 次 check：2 次期望失败、2 次期望成功；目标固定为槽 0，与左输入槽同址。右输入 0 只是统一夹具的填充值，不证明 NEG 使用二元右操作数。没有覆盖 unsigned/float/元方法分支、所有目标别名或自然 OOM。资源正常与非局部退出边界由 check 承担。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:109
 * - tests/core/test_execution_integer_boundaries.c:110
 * - tests/core/test_execution_integer_boundaries.c:112
 * - tests/core/test_execution_integer_boundaries.c:113
 * - tests/core/test_execution_integer_boundaries.c:133
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5024
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:7254
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:7269
 * - zr_vm_core/include/zr_vm_core/integer.h:43
 */

/**
 * @fn void test_masked_shifts(void)
 *
 * ## Summary
 * 用两种左移和两种右移入口检验计数低六位掩码及有符号位模式：负计数、64/65 和高位丢弃不应依赖宿主有符号移位的未定义或实现相关结果。
 *
 * ## Returns
 * 无 C 返回值。check 比较完成状态、signed 类型族和整数结果；Unity 断言失败退出当前 case，未到达的组合不获得执行信用。
 *
 * ## Constraints and lifecycle
 * 两个索引位置分别选取一个左移与一个右移 opcode，每轮六次 check；全部到达时为 12 次期望成功。左移输入分别为 (1,-1)、(-1,64)、(INT64_MIN,1)，右移输入为 (INT64_MIN,-1)、(-2,65)、(INT64_MIN,64)。目标固定为槽 0；只覆盖 signed 数值输入，没有验证 unsigned/混合类型、元方法、所有计数或所有平台/Sanitizer。移位语义依据实际 Integer_ShiftLeft/Right 的 unsigned 位模式与显式符号填充，测试本身不是宿主 UB 探测器。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:117
 * - tests/core/test_execution_integer_boundaries.c:118
 * - tests/core/test_execution_integer_boundaries.c:119
 * - tests/core/test_execution_integer_boundaries.c:121
 * - tests/core/test_execution_integer_boundaries.c:122
 * - tests/core/test_execution_integer_boundaries.c:123
 * - tests/core/test_execution_integer_boundaries.c:124
 * - tests/core/test_execution_integer_boundaries.c:125
 * - tests/core/test_execution_integer_boundaries.c:126
 * - tests/core/test_execution_integer_boundaries.c:134
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:7746
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:7790
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:8098
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:8113
 * - zr_vm_core/include/zr_vm_core/integer.h:96
 * - zr_vm_core/include/zr_vm_core/integer.h:100
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 初始化 Unity，按固定顺序注册加减、取负和移位三个原有测试方法，并把 Unity 汇总失败数作为进程结果交给现有 CTest 入口。
 *
 * ## Returns
 * @return UnityEnd 汇总的失败数；0 仅代表这次 runner 记录未失败。进程没有 selector 参数，三个 RUN_TEST 各计一个 Unity case，不能把内部 241 次条件 check 当成 241 个 runner 测试。
 *
 * ## Constraints and lifecycle
 * RUN_TEST 将函数指针交给 UnityDefaultTestRun，后者负责 setUp、case 和 tearDown；普通 Unity 断言早退后可继续注册的下一 case。没有额外文件夹具或线程。未被该 runner 捕获的崩溃/终止不保证走到 UnityEnd；本入口不证明 GC/OOM/所有 opcode 或 stderr 为空。
 *
 * ## References
 * - tests/core/test_execution_integer_boundaries.c:130
 * - tests/core/test_execution_integer_boundaries.c:131
 * - tests/core/test_execution_integer_boundaries.c:132
 * - tests/core/test_execution_integer_boundaries.c:133
 * - tests/core/test_execution_integer_boundaries.c:134
 * - tests/core/test_execution_integer_boundaries.c:135
 * - tests/third_party/zr_unity/Unity/src/unity.c:2283
 * - tests/third_party/zr_unity/Unity/src/unity.c:2298
 * - tests/third_party/zr_unity/Unity/src/unity.c:2299
 * - tests/third_party/zr_unity/Unity/src/unity.c:2303
 * - tests/third_party/zr_unity/Unity/src/unity.c:2359
 * - tests/cmake/runtime-compiler-boundary-tests.cmake:3
 * - tests/cmake/runtime-compiler-boundary-tests.cmake:7
 */
