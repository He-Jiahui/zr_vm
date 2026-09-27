// 复用完整流水线的静态 fixture 与测试函数，同时为手工 opcode 子集提供独立 Unity 入口。
int zr_vm_execbc_aot_pipeline_full_main(void);
// 仅在包含主测试文件时改名其 main，避免两个入口冲突；展开后立即恢复宏。
#define main zr_vm_execbc_aot_pipeline_full_main
#include "test_execbc_aot_pipeline.c"
#undef main

// 专门运行手工扩展指令与源码同步用例；不触发主文件的全量矩阵。
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_backends_lower_manual_extended_numeric_opcode_fixture);
    RUN_TEST(test_aot_backends_lower_manual_state_and_scope_opcode_fixture);
    RUN_TEST(test_aot_source_sync_keeps_extended_opcode_surfaces_aligned);
    return UNITY_END();
}
