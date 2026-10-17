/* Run the existing scalar metadata cases without the checkpoint materializer.
 * The included legacy main is compiled under a different name and never called.
 * This file includes the actual checkout; it contains no copied case bodies.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "core/support/ssa_exec_ir_owned_module_allocator.h"
#include "core/support/ssa_exec_ir_owned_module_shim.h"
#define main zr_ssa_scalar_legacy_main_not_called
#include "test_ssa_pass_manager_scalar.c"
#undef main

void setUp(void) { ZrTests_ExecOwner_Reset(); }
void tearDown(void) { ZrTests_ExecOwner_AssertBalanced(); }

int main(void) {
    int result;
    UNITY_BEGIN();
    RUN_TEST(test_pipeline_rolls_back_failed_map_rebuild);
    RUN_TEST(test_pipeline_verifies_storage_before_hashing);
    RUN_TEST(test_scalar_pipeline_and_fixed_point);
    RUN_TEST(test_throwing_instruction_is_observable);
    RUN_TEST(test_checked_overflow_is_not_folded);
    RUN_TEST(test_unused_call_is_preserved);
    RUN_TEST(test_unused_move_is_preserved);
    RUN_TEST(test_typed_call_row_survives_scalar_optimization);
    RUN_TEST(test_dead_source_mapping_is_removed);
    RUN_TEST(test_type_test_identity_survives_hash_and_dead_code_cleanup);
    RUN_TEST(test_token_phi_metadata_participates_in_hash);
    RUN_TEST(test_direct_pass_rejects_malformed_storage);
    RUN_TEST(test_unsupported_analysis_requirements_roll_back);
    RUN_TEST(test_failed_pass_rolls_back);
    RUN_TEST(test_budget_is_bounded);
    result = UNITY_END();
    puts("SSA_SCALAR_METADATA_CASES=15");
    return result;
}
