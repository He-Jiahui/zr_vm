#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "support/ssa_sccp_pooled_bool_fixture.h"

static SBpResult g_current;
void setUp(void) { Bp_SetUp(); }
void tearDown(void) { Bp_TearDown(); }
static void check(unsigned id) {
    g_current=id==9u?Bp_Budget():id==10u?Bp_Faults():id==22u?Bp_PublicBudgets():id>10u?Bp_Advanced(id):Bp_Semantic(id);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u,g_current.safety,"pooled BOOL safety prerequisite failed");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u,g_current.feature,"canonical pooled BOOL materialization missing");
}
static void public_eq(void) { check(1u); }
static void compare_eq(void) { check(2u); }
static void compare_lt(void) { check(3u); }
static void compare_le(void) { check(4u); }
static void compare_gt(void) { check(5u); }
static void compare_ge(void) { check(6u); }
static void compare_ne(void) { check(7u); }
static void disabled_sccp(void) { check(8u); }
static void measured_private_budgets(void) { check(9u); }
static void public_all_actual_oom(void) { check(10u); }
static void case_11(void) { check(11u); }
static void case_12(void) { check(12u); }
static void case_13(void) { check(13u); }
static void case_14(void) { check(14u); }
static void case_15(void) { check(15u); }
static void case_16(void) { check(16u); }
static void case_17(void) { check(17u); }
static void case_18(void) { check(18u); }
static void case_19(void) { check(19u); }
static void case_20(void) { check(20u); }
static void case_21(void) { check(21u); }
static void case_22(void) { check(22u); }
typedef struct SBpCase { unsigned id; const char *name,*kind; void (*run)(void); } SBpCase;
static const SBpCase cases[]={
    {1u,"public_eq_returns_bool","feature",public_eq},
    {2u,"compare_eq_true_false","feature",compare_eq},
    {3u,"compare_lt_true_false","feature",compare_lt},
    {4u,"compare_le_true_false","feature",compare_le},
    {5u,"compare_gt_true_false","feature",compare_gt},
    {6u,"compare_ge_true_false","feature",compare_ge},
    {7u,"compare_ne_true_false","feature",compare_ne},
    {8u,"disabled_sccp_retains_compare","control",disabled_sccp},
    {9u,"measured_private_budget_boundaries","feature_budget",measured_private_budgets},
    {10u,"public_actual_oom_and_retry","feature_lifetime",public_all_actual_oom},
    {11u,"dedup_bool_zero_one_first_exact_tuple","feature",case_11},
    {12u,"typed_and_flags_tuple_collision","feature",case_12},
    {13u,"pool_growth_two_function_reuse","feature_lifetime",case_13},
    {14u,"direct_owner_cache_and_repeat","feature",case_14},
    {15u,"observable_compare_is_retained","control",case_15},
    {16u,"non_int64_operand_is_not_proven","control",case_16},
    {17u,"pipeline_changed_and_no_change_remarks","feature",case_17},
    {18u,"external_operand_is_not_proven","control",case_18},
    {19u,"disabled_dce_still_materializes_bool","feature",case_19},
    {20u,"all_disabled_retains_compare","control",case_20},
    {21u,"source_map_and_result_identity","feature",case_21},
    {22u,"public_partial_budget_no_orphan","feature_budget",case_22}
};
int main(int argc,char **argv) {
    unsigned i,ran=0u,failed=0u; int red=0,semantic=0,result; SBpTotals totals;
    if (argc==2 && !strcmp(argv[1],"--red")) red=1;
    else if (argc==2 && !strcmp(argv[1],"--semantic")) semantic=1;
    else if (argc!=1) return 2;
    UNITY_BEGIN();
    for (i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        UNITY_UINT before;
        if (red && cases[i].id!=1u && cases[i].id!=8u) continue;
        if (semantic && (cases[i].id==9u || cases[i].id==10u || cases[i].id==22u)) continue;
        g_current=(SBpResult){0u,0u}; before=Unity.TestFailures;
        UnityDefaultTestRun(cases[i].run,cases[i].name,(int)(i+1u));
        ++ran; if (Unity.TestFailures!=before) ++failed;
        printf("POOLED_BOOL_CASE id=%u name=%s kind=%s safety=%u feature=%u result=%s\n",
               cases[i].id,cases[i].name,cases[i].kind,g_current.safety,g_current.feature,
               Unity.TestFailures==before?"PASS":"FAIL");
    }
    result=UNITY_END(); totals=Bp_Totals();
    printf("POOLED_BOOL_SUMMARY cases=%u passed=%u failed=%u attempts=%u fault_measured=%u fault_completed=%u live=%u errors=%u\n",
           ran,ran-failed,failed,totals.attempts,totals.faultMeasured,totals.faultCompleted,totals.live,totals.errors);
    return result;
}
