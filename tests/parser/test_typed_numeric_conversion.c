#include <string.h>

#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"

/* 检索覆盖整个函数树，避免只看入口函数时漏掉嵌套代码中的指令或局部绑定。 */
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

/* 返回首个匹配指令，供带类型局部变量的测试核对源槽位操作数。 */
static const TZrInstruction *find_first_opcode_recursive(const SZrFunction *function,
                                                         EZrInstructionCode opcode,
                                                         TZrUInt32 depth) {
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(depth < 64);

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return &function->instructionsList[index];
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            const TZrInstruction *match =
                    find_first_opcode_recursive(&function->childFunctionList[index], opcode, depth + 1);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }

    return ZR_NULL;
}

/* 短串和长串使用不同接口读取原生字符，统一后再比较局部名文本。 */
static TZrBool string_equals_native(const SZrString *name, const char *text) {
    TZrNativeString nativeName;

    if (name == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeName = name->shortStringLength < ZR_VM_LONG_STRING_FLAG
            ? ZrCore_String_GetNativeStringShort((SZrString *)name)
            : ZrCore_String_GetNativeString((SZrString *)name);
    return nativeName != ZR_NULL && strcmp(nativeName, text) == 0;
}

/* 沿函数树查找编译器记录的 typed 局部绑定，验证名称到栈槽的映射。 */
static const SZrFunctionTypedLocalBinding *find_typed_local_binding_recursive(const SZrFunction *function,
                                                                              const char *name,
                                                                              TZrUInt32 depth) {
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(depth < 64);

    for (index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (string_equals_native(binding->name, name)) {
            return binding;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            const SZrFunctionTypedLocalBinding *match =
                    find_typed_local_binding_recursive(&function->childFunctionList[index], name, depth + 1);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }

    return ZR_NULL;
}

/* 统一走 parser 的公开源码编译入口，让每个用例覆盖解析、类型推断和指令生成。 */
static SZrFunction *compile_source(SZrState *state, const char *source) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_numeric_conversion_test.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

/* 每个数值用例同时核对类型专用指令和 VM 执行值，避免结果相同掩盖通用指令回退。 */
static void test_typed_signed_to_float_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var i: int = 7;\n"
            "return <float> i;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 7.0, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* unsigned 整数转 float 还要核对 typed binding 的源槽位没有漂移。 */
static void test_typed_unsigned_to_float_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var u: uint = <uint>9;\n"
            "return <float> u;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT), 0));
    /* 确认 unsigned 局部变量的绑定槽位就是转换指令的源操作数。 */
    {
        const TZrInstruction *conversion =
                find_first_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED), 0);
        const SZrFunctionTypedLocalBinding *localBinding =
                find_typed_local_binding_recursive(function, "u", 0);
        TEST_ASSERT_NOT_NULL(conversion);
        TEST_ASSERT_NOT_NULL(localBinding);
        TEST_ASSERT_EQUAL_UINT32(localBinding->stackSlot, conversion->instruction.operand.operand1[0]);
    }
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FLOAT(result.type));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 9.0, result.value.nativeObject.nativeDouble);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* unsigned 加法必须保持无符号算术指令，并通过 int 返回值观察结果。 */
static void test_typed_unsigned_binary_arithmetic_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var u: uint = <uint>9;\n"
            "var v: uint = <uint>4;\n"
            "var sum: uint = u + v;\n"
            "return <int> sum;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(13, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* float 转 int 验证专用转换路径及语言层面的截断结果。 */
static void test_typed_float_to_signed_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var f: float = 2.75;\n"
            "return <int> f;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_FLOAT), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(2, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 普通 unsigned 到 signed 转换覆盖正值路径，排除通用 TO_INT 回退。 */
static void test_typed_unsigned_to_signed_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var u: uint = <uint>17;\n"
            "return <int> u;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(17, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 最高位边界固定 unsigned 到 signed 的 unchecked 式回绕公式。 */
static void test_typed_unsigned_to_signed_cast_wraps_high_bit_like_unchecked_csharp(void) {
    const char *source =
            "var u: uint = <uint>-1;\n"
            "return <int> u;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED), 0));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(-1, result);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* float 转 unsigned 验证截断和结果类型仍为 unsigned。 */
static void test_typed_float_to_unsigned_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var f: float = 12.75;\n"
            "return <uint> f;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_UINT64(12, result.value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 负 signed 转 unsigned 验证按 uint64 模数保留负值载荷。 */
static void test_typed_signed_to_unsigned_cast_emits_direct_opcode_and_executes(void) {
    const char *source =
            "var i: int = -3;\n"
            "return <uint> i;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    function = compile_source(state, source);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED), 0));
    TEST_ASSERT_EQUAL_UINT32(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT), 0));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_UNSIGNED_INT(result.type));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)-3, result.value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

/* 用例资源在各自函数中创建，不使用共享 Unity fixture。 */
void setUp(void) {}

/* BUG: state 创建成功后的断言失败会经 Unity longjmp 跳过函数与运行时释放；空 tearDown 无法回收资源。 */
void tearDown(void) {}

/* TODO: 确认此 Unity 目标是否应纳入 CTest；未见本目标对应的 EXECUTABLES 清单注册。 */
/* Unity 入口顺序运行各类数值转换与无符号运算回归。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_signed_to_float_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_unsigned_to_float_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_unsigned_binary_arithmetic_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_float_to_signed_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_unsigned_to_signed_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_unsigned_to_signed_cast_wraps_high_bit_like_unchecked_csharp);
    RUN_TEST(test_typed_float_to_unsigned_cast_emits_direct_opcode_and_executes);
    RUN_TEST(test_typed_signed_to_unsigned_cast_emits_direct_opcode_and_executes);
    return UNITY_END();
}
