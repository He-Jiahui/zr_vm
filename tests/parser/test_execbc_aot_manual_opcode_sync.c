int zr_vm_execbc_aot_pipeline_full_main(void);
#define main zr_vm_execbc_aot_pipeline_full_main
#include "test_execbc_aot_pipeline.c"
#undef main

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_backends_lower_manual_extended_numeric_opcode_fixture);
    RUN_TEST(test_aot_backends_lower_manual_state_and_scope_opcode_fixture);
    RUN_TEST(test_aot_source_sync_keeps_extended_opcode_surfaces_aligned);
    return UNITY_END();
}
