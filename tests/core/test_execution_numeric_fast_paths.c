#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/value.h"

/* 这些定义对应 execution_internal.h 的私有操作编号，测试经链接直接进入数值分派。
 * TODO: 若内部枚举调整，需核对本地镜像与实现的编号是否仍一致。 */
typedef enum EZrExecutionNumericCompareOp {
    ZR_EXEC_NUMERIC_COMPARE_GREATER = 0,
    ZR_EXEC_NUMERIC_COMPARE_LESS,
    ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
    ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL
} EZrExecutionNumericCompareOp;

typedef enum EZrExecutionNumericFallbackOp {
    ZR_EXEC_NUMERIC_FALLBACK_ADD = 0,
    ZR_EXEC_NUMERIC_FALLBACK_SUB,
    ZR_EXEC_NUMERIC_FALLBACK_MUL,
    ZR_EXEC_NUMERIC_FALLBACK_DIV,
    ZR_EXEC_NUMERIC_FALLBACK_MOD,
    ZR_EXEC_NUMERIC_FALLBACK_POW
} EZrExecutionNumericFallbackOp;

TZrInt64 value_to_int64(const SZrTypeValue *value);
TZrBool try_builtin_add(SZrState *state,
                        SZrTypeValue *outResult,
                        const SZrTypeValue *opA,
                        const SZrTypeValue *opB);
TZrBool execution_try_builtin_mul_mixed_numeric_fast(SZrTypeValue *outResult,
                                                     const SZrTypeValue *opA,
                                                     const SZrTypeValue *opB);
void execution_apply_binary_numeric_compare_or_raise(SZrState *state,
                                                     EZrExecutionNumericCompareOp operation,
                                                     SZrTypeValue *destination,
                                                     const SZrTypeValue *opA,
                                                     const SZrTypeValue *opB,
                                                     const TZrChar *instructionName);
void execution_try_binary_numeric_float_fallback_or_raise(SZrState *state,
                                                          EZrExecutionNumericFallbackOp operation,
                                                          SZrTypeValue *destination,
                                                          const SZrTypeValue *opA,
                                                          const SZrTypeValue *opB,
                                                          const TZrChar *instructionName);

void setUp(void) {}

void tearDown(void) {}

/* 混合运算的共同前提：bool 转 0/1，整数保留符号；结果用于 add/mul 的快路径。 */
/* TODO: 用例只有小整数；execution_numeric.c 将高位 uint64 转成 int64 并直接做有符号算术。
 * 需先核对语言的越界语义，再补 UINT64_MAX / INT64_MAX 附近的回归。 */
static void test_value_to_int64_converts_integral_and_bool_inputs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue unsignedValue;
    SZrTypeValue boolValue;
    SZrTypeValue signedValue;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &unsignedValue, 7u);
    ZrCore_Value_InitAsBool(state, &boolValue, ZR_TRUE);
    ZrCore_Value_InitAsInt(state, &signedValue, -3);

    TEST_ASSERT_EQUAL_INT64(7, value_to_int64(&unsignedValue));
    TEST_ASSERT_EQUAL_INT64(1, value_to_int64(&boolValue));
    TEST_ASSERT_EQUAL_INT64(-3, value_to_int64(&signedValue));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_try_builtin_add_mixed_uint_and_bool_returns_int64_sum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &leftValue, 7u);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(8, result.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_try_builtin_add_mixed_float_and_bool_returns_double_sum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsFloat(state, &leftValue, 2.5);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(try_builtin_add(state, &result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.type);
    TEST_ASSERT_EQUAL_DOUBLE(3.5, result.value.nativeObject.nativeDouble);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_try_builtin_mul_mixed_uint_and_bool_returns_int64_product(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &leftValue, 7u);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_try_builtin_mul_mixed_numeric_fast(&result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(7, result.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_try_builtin_mul_mixed_signed_and_uint_returns_int64_product(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsInt(state, &leftValue, -3);
    ZrCore_Value_InitAsUInt(state, &rightValue, 2u);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_try_builtin_mul_mixed_numeric_fast(&result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(-6, result.value.nativeObject.nativeInt64);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_try_builtin_mul_mixed_float_and_bool_returns_double_product(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsFloat(state, &leftValue, 2.5);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_TRUE(execution_try_builtin_mul_mixed_numeric_fast(&result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.type);
    TEST_ASSERT_EQUAL_DOUBLE(2.5, result.value.nativeObject.nativeDouble);

    ZrTests_Runtime_State_Destroy(state);
}

/* 两个 bool 不在混合数值乘法快路径内，需保留给后续通用语义处理。 */
static void test_execution_try_builtin_mul_rejects_bool_bool_pair(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsBool(state, &leftValue, ZR_TRUE);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_FALSE);
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_FALSE(execution_try_builtin_mul_mixed_numeric_fast(&result, &leftValue, &rightValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.type);

    ZrTests_Runtime_State_Destroy(state);
}

/* 直接检查解释器比较分支的跨类型输入转换与 bool 输出契约。 */
static void test_execution_apply_binary_numeric_compare_mixed_uint_and_bool_returns_true(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &leftValue, 7u);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    execution_apply_binary_numeric_compare_or_raise(state,
                                                    ZR_EXEC_NUMERIC_COMPARE_GREATER,
                                                    &result,
                                                    &leftValue,
                                                    &rightValue,
                                                    "TEST_COMPARE");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.type);
    TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_apply_binary_numeric_compare_mixed_float_and_bool_returns_false(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsFloat(state, &leftValue, 0.5);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    execution_apply_binary_numeric_compare_or_raise(state,
                                                    ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
                                                    &result,
                                                    &leftValue,
                                                    &rightValue,
                                                    "TEST_COMPARE");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.type);
    TEST_ASSERT_FALSE(result.value.nativeObject.nativeBool);

    ZrTests_Runtime_State_Destroy(state);
}

/* 浮点回退应与内建加法的 bool 转换一致，且结果统一标为 double。 */
static void test_execution_try_binary_numeric_float_fallback_add_mixed_float_and_bool_returns_double_sum(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsFloat(state, &leftValue, 2.5);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    execution_try_binary_numeric_float_fallback_or_raise(state,
                                                         ZR_EXEC_NUMERIC_FALLBACK_ADD,
                                                         &result,
                                                         &leftValue,
                                                         &rightValue,
                                                         "TEST_FLOAT_FALLBACK");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.type);
    TEST_ASSERT_EQUAL_DOUBLE(3.5, result.value.nativeObject.nativeDouble);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_execution_try_binary_numeric_float_fallback_mod_mixed_uint_and_bool_returns_double_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &leftValue, 7u);
    ZrCore_Value_InitAsBool(state, &rightValue, ZR_TRUE);
    ZrCore_Value_ResetAsNull(&result);

    execution_try_binary_numeric_float_fallback_or_raise(state,
                                                         ZR_EXEC_NUMERIC_FALLBACK_MOD,
                                                         &result,
                                                         &leftValue,
                                                         &rightValue,
                                                         "TEST_FLOAT_FALLBACK");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.type);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, result.value.nativeObject.nativeDouble);

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    /* BUG: tests/CMakeLists.txt 创建此目标但没有把它加入常规 CTest 套件；
     * ctest -N 仅登记 core_runtime 等套件，混合数值回归不会自动执行。 */
    UNITY_BEGIN();

    RUN_TEST(test_value_to_int64_converts_integral_and_bool_inputs);
    RUN_TEST(test_try_builtin_add_mixed_uint_and_bool_returns_int64_sum);
    RUN_TEST(test_try_builtin_add_mixed_float_and_bool_returns_double_sum);
    RUN_TEST(test_execution_try_builtin_mul_mixed_uint_and_bool_returns_int64_product);
    RUN_TEST(test_execution_try_builtin_mul_mixed_signed_and_uint_returns_int64_product);
    RUN_TEST(test_execution_try_builtin_mul_mixed_float_and_bool_returns_double_product);
    RUN_TEST(test_execution_try_builtin_mul_rejects_bool_bool_pair);
    RUN_TEST(test_execution_apply_binary_numeric_compare_mixed_uint_and_bool_returns_true);
    RUN_TEST(test_execution_apply_binary_numeric_compare_mixed_float_and_bool_returns_false);
    RUN_TEST(test_execution_try_binary_numeric_float_fallback_add_mixed_float_and_bool_returns_double_sum);
    RUN_TEST(test_execution_try_binary_numeric_float_fallback_mod_mixed_uint_and_bool_returns_double_result);

    return UNITY_END();
}
