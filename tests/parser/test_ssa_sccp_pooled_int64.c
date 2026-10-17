#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include "support/ssa_sccp_pooled_int64_fixture.h"
/* Preserve all 53 sealed main identities. GREEN emits 113 profiles: case30 adds
 * real UNKNOWN paths; case47 measures both private owners with both diagnostics. */
typedef struct SPiCase { unsigned id; const char *name, *kind; } SPiCase;
static const SPiCase g_cases[] = {
    {1u, "pool_add_40_2_materializes_42", "feature"},
    {2u, "pool_arithmetic_40_2", "feature"},
    {3u, "pool_sub_negative", "feature"},
    {4u, "pool_mul_negative", "feature"},
    {5u, "pool_neg_positive", "feature"},
    {6u, "pool_neg_negative", "feature"},
    {7u, "pool_div_effect_chain_add_consumer", "feature"},
    {8u, "pool_div_effect_chain_negative_consumer", "feature"},
    {9u, "pool_add_above_u32", "feature"},
    {10u, "pool_sub_int64_min", "feature"},
    {11u, "pool_add_int64_max", "feature"},
    {12u, "pool_mul_zero", "feature"},
    {13u, "pool_div_effect_chain_multi_consumers", "feature"},
    {14u, "dedup_existing_exact", "feature"},
    {15u, "dedup_repeated_same_function", "feature"},
    {16u, "dedup_cross_function", "feature"},
    {17u, "dedup_flags_are_part_of_key", "feature"},
    {18u, "chain_realloc_refreshes_view", "feature"},
    {19u, "repeat_optimize_fixed_point", "feature"},
    {20u, "same_bits_other_type_not_deduped", "feature"},
    {21u, "add_overflow", "control"},
    {22u, "sub_overflow", "control"},
    {23u, "mul_overflow", "control"},
    {24u, "neg_min", "control"},
    {25u, "div_zero", "control"},
    {26u, "div_min_minus_one", "control"},
    {27u, "no_int64_materialize_float_bits", "control"},
    {28u, "no_int64_materialize_unsigned_bits", "control"},
    {29u, "no_int64_materialize_retagged_copy", "control"},
    {30u, "no_int64_materialize_retagged_phi", "control"},
    {31u, "no_int64_materialize_int32_not_int64", "control"},
    {32u, "no_int64_materialize_compare_bool", "control"},
    {33u, "barrier_may_allocate", "control"},
    {34u, "barrier_may_gc", "control"},
    {35u, "barrier_may_suspend", "control"},
    {36u, "barrier_debug_poll", "control"},
    {37u, "barrier_guard_exit", "control"},
    {38u, "barrier_effect_tokens", "control"},
    {39u, "barrier_memory_ranges", "control"},
    {40u, "barrier_non_div_may_throw", "control"},
    {41u, "empty_pool_immediate_protocol", "control"},
    {42u, "public_function_context_has_no_owner", "control"},
    {43u, "disable_sccp", "control"},
    {44u, "cache_invalidates_changed_ir", "control"},
    {45u, "cache_rejects_changed_constant_view", "control"},
    {46u, "cache_rolls_back_on_pass_failure", "control"},
    {47u, "budget_no_orphan_append", "feature_budget"},
    {48u, "budget_max_instructions", "control"},
    {49u, "all_attempts_oom", "transaction_feature"},
    {50u, "late_second_function_oom", "transaction_feature"},
    {51u, "remarks_growth_oom", "transaction_feature"},
    {52u, "verify_failure_preserves_owner", "control"},
    {53u, "healthy_free_and_retry", "feature_lifetime"},
};
static unsigned g_case;
static SPiResult g_result;
void setUp(void) { Pi_SetUp(); g_result = (SPiResult){0u,0u}; }
void tearDown(void) {
    SZrOwnerHeapStats st;
    Pi_TearDown(); st = ZrTests_ExecOwner_Stats();
    TEST_ASSERT_EQUAL_UINT(0u, st.live);
    TEST_ASSERT_EQUAL_UINT(0u, st.errors);
}
static void pi_case(void) {
    g_result = Pi_Matrix(g_cases[g_case].id);
    /* All profiles and all cleanup execute before either aborting assertion. */
    TEST_ASSERT_TRUE_MESSAGE(g_result.safety, "fixture/Verify/diagnostic/owner-ledger safety failure");
    TEST_ASSERT_TRUE_MESSAGE(g_result.feature, "registered materialization or transaction guarantee not satisfied");
}
int main(void) {
    unsigned passed=0u,failed=0u;
    SPiTotals totals;
    UNITY_BEGIN();
    for(g_case=0u;g_case<sizeof(g_cases)/sizeof(g_cases[0]);++g_case) {
        unsigned before=Unity.TestFailures;
        UnityDefaultTestRun(pi_case,g_cases[g_case].name,(int)(g_case+1u));
        if(Unity.TestFailures==before)++passed;else ++failed;
        printf("POOLED_INT64_CASE id=%u name=%s kind=%s safety=%u feature=%u result=%s\n",
                g_cases[g_case].id,g_cases[g_case].name,g_cases[g_case].kind,
                g_result.safety,g_result.feature,Unity.TestFailures==before?"PASS":"FAIL");
    }
    totals=Pi_Totals();
    printf("POOLED_INT64_SUMMARY cases=%u passed=%u failed=%u attempts=%u fault_measured=%u fault_completed=%u live=%u errors=%u\n",
            (unsigned)(sizeof(g_cases)/sizeof(g_cases[0])),passed,failed,totals.attempts,
            totals.faultMeasured,totals.faultCompleted,totals.live,totals.errors);
    return UNITY_END();
}
