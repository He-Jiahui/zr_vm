#include "unity.h"
/** @file @brief 先取负再按位取反计数的独立 Unity 回归。BUG: 共用 helper 的 malloc compiler state 在断言失败长跳转时绕过 free。 */
#include "bitwise_zero_minus_shift_supported_count_range_test_support.h"
/** @brief 用左移的加法计数取负后按位取反验证零左操作数经位移与零减右项组合后，推断区间与 numeric fact 同为 [2,3] 且不溢出。 */
static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_additive_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_additive_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-(unit + unit)))) - (zero - unit));",
            2,
            3);
}
/** @brief 用右移的精确零 OR 计数取负后按位取反验证零左操作数经位移与零减右项组合后，推断区间与 numeric fact 同为 [2,3] 且不溢出。 */
static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_exact_zero_or_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_exact_zero_or_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-((unit + unit) | (span - span))))) - (zero - unit));",
            2,
            3);
}
/** @brief 用左移的内层取负与精确零 OR 后按位取反验证零左操作数经位移与零减右项组合后，推断区间与 numeric fact 同为 [2,3] 且不溢出。 */
static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_exact_zero_or_inner_unary_minus_count_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_exact_zero_or_inner_unary_minus_count_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero << (~(zero | (-(unit + unit))))) - (zero - unit));",
            2,
            3);
}
/** @brief 用右移的内层取负与精确零 XOR 后按位取反验证零左操作数经位移与零减右项组合后，推断区间与 numeric fact 同为 [2,3] 且不溢出。 */
static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_inner_unary_minus_count_xor_exact_zero_zero_minus_rhs_records_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_inner_unary_minus_count_xor_exact_zero_zero_minus_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((-((unit + unit) | (span - span))) ^ (span - span)))) - (zero - unit));",
            2,
            3);
}
/** @brief 用左移的加法计数取负后按位取反验证零左操作数经位移与零减一元负号右项组合后，推断区间与 numeric fact 同为 [-3,-2] 且不溢出。 */
static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_additive_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_additive_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(-(unit + unit)))) - (-(zero - unit)));",
            -3,
            -2);
}
/** @brief 用右移的精确零 OR 计数取负后按位取反验证零左操作数经位移与零减一元负号右项组合后，推断区间与 numeric fact 同为 [-3,-2] 且不溢出。 */
static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_exact_zero_or_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_exact_zero_or_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~(-((unit + unit) | (span - span))))) - (-(zero - unit)));",
            -3,
            -2);
}
/** @brief 用左移的内层取负与精确零 OR 后按位取反验证零左操作数经位移与零减一元负号右项组合后，推断区间与 numeric fact 同为 [-3,-2] 且不溢出。 */
static void test_or_zero_left_with_shift_left_bitwise_not_unary_minus_exact_zero_or_inner_unary_minus_count_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_left_bitwise_not_unary_minus_exact_zero_or_inner_unary_minus_count_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero << (~(zero | (-(unit + unit))))) - (-(zero - unit)));",
            -3,
            -2);
}
/** @brief 用右移的内层取负与精确零 XOR 后按位取反验证零左操作数经位移与零减一元负号右项组合后，推断区间与 numeric fact 同为 [-3,-2] 且不溢出。 */
static void test_or_zero_left_with_shift_right_bitwise_not_unary_minus_inner_unary_minus_count_xor_exact_zero_zero_minus_unary_rhs_records_negative_unit_range(void) {
    assert_bitwise_zero_minus_shift_supported_count_range(
            "bitwise_or_zero_left_shift_right_bitwise_not_unary_minus_inner_unary_minus_count_xor_exact_zero_zero_minus_unary_rhs_range.zr",
            "(zero + zero) | ((zero >> (~((-((unit + unit) | (span - span))) ^ (span - span)))) - (-(zero - unit)));",
            -3,
            -2);
}
/** @brief 通过 RUN_TEST 注册四种计数结构与两种右项符号；共用 support 翻译单元提供 setUp/tearDown，目标进入 language_pipeline 完整/core/stress 清单。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_additive_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_exact_zero_or_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_exact_zero_or_inner_unary_minus_count_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_inner_unary_minus_count_xor_exact_zero_zero_minus_rhs_records_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_additive_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_exact_zero_or_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_left_bitwise_not_unary_minus_exact_zero_or_inner_unary_minus_count_zero_minus_unary_rhs_records_negative_unit_range);
    RUN_TEST(test_or_zero_left_with_shift_right_bitwise_not_unary_minus_inner_unary_minus_count_xor_exact_zero_zero_minus_unary_rhs_records_negative_unit_range);
    return UNITY_END();
}
