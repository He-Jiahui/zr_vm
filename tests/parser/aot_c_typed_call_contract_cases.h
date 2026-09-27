#ifndef ZR_VM_TESTS_AOT_C_TYPED_CALL_CONTRACT_CASES_H
#define ZR_VM_TESTS_AOT_C_TYPED_CALL_CONTRACT_CASES_H

/** @brief 检验无参 i64 静态调用的 typed thunk 源码合同。 */
void test_aot_c_source_lowers_static_no_arg_i64_calls_to_typed_thunks(void);
/** @brief 检验无参 bool 静态调用的 typed thunk 源码合同。 */
void test_aot_c_source_lowers_static_no_arg_bool_calls_to_typed_thunks(void);
/** @brief 检验无参 u64 静态调用的 typed thunk 源码合同。 */
void test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks(void);
/** @brief 检验 f64 静态调用及返回边界的 typed thunk 源码合同。 */
void test_aot_c_source_lowers_static_f64_calls_to_typed_thunks(void);

#endif
