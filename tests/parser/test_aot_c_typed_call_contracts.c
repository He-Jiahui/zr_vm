#include "aot_c_typed_call_contract_cases.h"

#include "unity.h"

/* 四种返回类型的源码合同分别实现，此入口集中注册 Unity 用例。 */

/* Unity 每例初始化钩子；本套件不保留跨例状态。 */
void setUp(void) {}

/* Unity 每例收尾钩子；当前不执行自动资源回收。 */
void tearDown(void) {}

/* 集中注册 i64、bool、u64、f64 返回类型的调用合同。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_i64_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_bool_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_f64_calls_to_typed_thunks);
    return UNITY_END();
}
