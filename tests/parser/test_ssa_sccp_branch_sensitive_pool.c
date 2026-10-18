#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include "support/ssa_sccp_branch_sensitive_pool_fixture.h"
typedef struct SBeCase { unsigned id;const char *name,*kind; } SBeCase;
static const SBeCase g_cases[] = {
    {1u,"bool_true_selects42","feature"},
    {2u,"bool_false_selects52","feature"},
    {3u,"strict_int64_compare_eq","feature"},
    {4u,"strict_int64_compare_lt","feature"},
    {5u,"strict_int64_compare_le","feature"},
    {6u,"strict_int64_compare_gt","feature"},
    {7u,"strict_int64_compare_ge","feature"},
    {8u,"strict_int64_compare_ne","feature"},
    {9u,"bool_copy","feature"},
    {10u,"identical_bool_phi_and_duplicate_slots","feature"},
    {11u,"nested_dependent_bool_phi","feature"},
    {12u,"external_bool_no_prune","control"},
    {13u,"true_plus_external_unknown_phi_no_prune","control"},
    {14u,"noncanonical_compare_origins_no_prune","control"},
    {15u,"differing_bool_phi_no_prune","control"},
    {16u,"loop_carried_condition","feature"},
    {17u,"legacy_no_owner_and_empty_pool","control"},
    {18u,"dead_div_and_observable_consumer_preserved","feature"},
    {19u,"repeat_optimize_fixed_point","feature"},
    {20u,"measured_budget_no_orphan","feature_budget"},
    {21u,"all_actual_oom_and_retry","feature_lifetime"},
    {22u,"late_function_and_remark_oom","feature_lifetime"}
};
static unsigned g_case;
static SBeResult g_result;
void setUp(void) { Be_SetUp();g_result=(SBeResult){0u,0u}; }
void tearDown(void) {
    SZrOwnerHeapStats st;Be_TearDown();st=ZrTests_ExecOwner_Stats();
    TEST_ASSERT_EQUAL_UINT(0u,st.live);TEST_ASSERT_EQUAL_UINT(0u,st.errors);
}
static void be_case(void) {
    g_result=Be_Matrix(g_cases[g_case].id);
    /* Every profile, armed allocation failure, retry and cleanup has finished
     * before either Unity assertion can abort this case. */
    TEST_ASSERT_TRUE_MESSAGE(g_result.safety,"fixture/Verify/ownership/ledger safety failure");
    TEST_ASSERT_TRUE_MESSAGE(g_result.feature,"registered branch-sensitive consumer fold missing");
}
int main(void) {
    unsigned passed=0u,failed=0u;SBeTotals st;
    UNITY_BEGIN();
    for(g_case=0u;g_case<sizeof(g_cases)/sizeof(g_cases[0]);++g_case){UNITY_UINT before=Unity.TestFailures;
        UnityDefaultTestRun(be_case,g_cases[g_case].name,(int)(g_case+1u));
        if(before==Unity.TestFailures)++passed;else ++failed;
        printf("BOOL_EDGE_CASE id=%u name=%s kind=%s safety=%u feature=%u result=%s\n",
            g_cases[g_case].id,g_cases[g_case].name,g_cases[g_case].kind,g_result.safety,g_result.feature,
            before==Unity.TestFailures?"PASS":"FAIL");}
    st=Be_Totals();
    printf("BOOL_EDGE_SUMMARY cases=%u passed=%u failed=%u attempts=%u fault_measured=%u fault_completed=%u live=%u errors=%u\n",
        (unsigned)(sizeof(g_cases)/sizeof(g_cases[0])),passed,failed,st.attempts,st.faultMeasured,st.faultCompleted,st.live,st.errors);
    return UNITY_END();
}
