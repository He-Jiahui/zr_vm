#include "aot_c_typed_call_contract_cases.h"

#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_i64_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_bool_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks);
    RUN_TEST(test_aot_c_source_lowers_static_f64_calls_to_typed_thunks);
    return UNITY_END();
}
