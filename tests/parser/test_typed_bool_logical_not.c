#include <string.h>

#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"

/* 连同子函数扫描指令图，避免只看顶层而漏判 typed 或通用逻辑非。 */
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

    sourceName = ZrCore_String_CreateFromNative(state, "typed_bool_logical_not_test.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

/* 静态 bool 取反应发出专用指令、不退回通用指令，随后执行三元分支验证结果。 */
static void test_typed_bool_logical_not_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var flag: bool = false;\n"
            "var inverted: bool = !flag;\n"
            "return inverted ? 1 : 0;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 每个用例在本地创建运行时，不使用共享 Unity fixture。 */
void setUp(void) {}

/* BUG: 断言失败会经 Unity longjmp 跳过用例末尾的函数和运行时释放；
 * 此空钩子无法清理失败用例持有的运行时及函数图。 */
void tearDown(void) {}

/* Unity 入口执行 typed bool 逻辑非用例。 */
/* TODO: CMake 只定义可执行目标，未见 add_test；需确认是否要求纳入 CTest 自动回归。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_bool_logical_not_emits_direct_opcode_and_executes);
    return UNITY_END();
}
