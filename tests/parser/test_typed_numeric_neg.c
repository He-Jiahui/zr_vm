#include <string.h>

#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"

/* 扫描顶层和子函数指令，分别核对专用取负与通用取负的数量。 */
static TZrUInt32 count_opcode_recursive(const SZrFunction *function, EZrInstructionCode opcode, TZrUInt32 depth) {
    TZrUInt32 count = 0;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(depth < 64);

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            count += count_opcode_recursive(&function->childFunctionList[index], opcode, depth + 1);
        }
    }

    return count;
}

/* 在给定运行时编译脚本；返回的函数图由用例在成功路径释放。 */
static SZrFunction *compile_source(SZrState *state, const char *source) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_numeric_neg_test.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

/* 静态有符号整数应使用 NEG_SIGNED，执行结果保持整数 -7。 */
static void test_typed_signed_neg_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var i: int = 7;\n"
            "return -i;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(NEG_SIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(NEG), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(-7, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 静态浮点数应使用 NEG_FLOAT，执行后检查浮点类型和值。 */
static void test_typed_float_neg_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var f: float = 2.0;\n"
            "return -f;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(NEG_FLOAT), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(NEG), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, -2.0, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 两个用例各自创建运行时，不使用共享 Unity fixture。 */
void setUp(void) {}

/* BUG: 任一断言失败都会经 Unity longjmp 跳过本用例末尾的函数和运行时释放；
 * 此空钩子无法清理失败用例持有的运行时及函数图。 */
void tearDown(void) {}

/* Unity 入口按顺序执行整数及浮点 typed 取负用例。 */
/* TODO: CMake 只定义可执行目标，未见 add_test；需确认是否要求纳入 CTest 自动回归。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_signed_neg_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_float_neg_emits_direct_opcode_and_executes);
    return UNITY_END();
}
