#include <stdio.h>

#include "unity.h"
#include "zr_test_log_macros.h"

extern void test_known_vm_call_results_keep_typed_arithmetic_specialization(void);
extern void test_direct_child_function_calls_quicken_to_known_vm_call_family(void);
extern void test_loop_child_function_calls_quicken_to_known_vm_call_family(void);
extern void test_matrix_add_2d_benchmark_project_compile_quickens_array_add_loop_calls(void);
extern void test_map_object_access_benchmark_project_compile_quickens_labelFor_loop_call(void);
extern void test_call_chain_polymorphic_benchmark_project_compile_quickens_loop_helper_calls(void);

int main(void) {
    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("ZR-VM Compiler Call Lowering Focus\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();

    RUN_TEST(test_known_vm_call_results_keep_typed_arithmetic_specialization);
    RUN_TEST(test_direct_child_function_calls_quicken_to_known_vm_call_family);
    RUN_TEST(test_loop_child_function_calls_quicken_to_known_vm_call_family);
    RUN_TEST(test_matrix_add_2d_benchmark_project_compile_quickens_array_add_loop_calls);
    RUN_TEST(test_map_object_access_benchmark_project_compile_quickens_labelFor_loop_call);
    RUN_TEST(test_call_chain_polymorphic_benchmark_project_compile_quickens_loop_helper_calls);

    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("Compiler Call Lowering Focus Completed\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    return UNITY_END();
}
