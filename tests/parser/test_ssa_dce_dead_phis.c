#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "support/ssa_dce_dead_phis_fixture.h"
/* Main, Unity, and allocator use ordinary CRT. No execution backend is called. */
static SDpResult current;
void setUp(void) { Dp_SetUp(); }
void tearDown(void) { Dp_TearDown(); }
static void check(unsigned id) {
    current=(id>=15u&&id<=20u)?Dp_Faults(id):Dp_Semantic(id);
    TEST_ASSERT_TRUE_MESSAGE(current.safety,"DCE PHI safety/precondition/owner/ledger failure");
    TEST_ASSERT_TRUE_MESSAGE(current.feature,"DCE PHI expected IR transformation missing");
}
static void test_dead_diamond_phi_and_incoming_producers(void) { check(1u); }
static void test_dead_phi_dependency_chain(void) { check(2u); }
static void test_dead_self_loop_phi(void) { check(3u); }
static void test_dead_mutually_recursive_phi_cycle(void) { check(4u); }
static void test_mixed_live_dead_phi_order_across_blocks(void) { check(5u); }
static void test_dead_phi_with_shared_effect_memory_incoming_backing(void) { check(6u); }
static void test_live_phi_return_chain(void) { check(7u); }
static void test_one_root_keeps_mutual_cycle(void) { check(8u); }
static void test_duplicate_predecessor_occurrences_preserved(void) { check(9u); }
static void test_metadata_rooted_phi_profiles(void) { check(10u); }
static void test_effectful_producer_feeding_dead_phi(void) { check(11u); }
static void test_observable_boundary_profiles(void) { check(12u); }
static void test_source_mapping_dead_and_live_chains(void) { check(13u); }
static void test_rich_metadata_does_not_disable_unrelated_dead_phi_elimination(void) { check(14u); }
static void test_direct_budget_atomic_publication(void) { check(15u); }
static void test_pipeline_budget_partial_success_contract(void) { check(16u); }
static void test_direct_all_actual_oom_and_retry(void) { check(17u); }
static void test_pipeline_cache_remarks_failure_restore(void) { check(18u); }
static void test_public_module_late_function_transaction(void) { check(19u); }
static void test_repeat_direct_and_pipeline_fixed_point(void) { check(20u); }
static void test_empty_and_all_live_nochange(void) { check(21u); }
typedef struct SDpCase { unsigned id;const char *name,*kind;UnityTestFunction run; } SDpCase;
static const SDpCase cases[]={
    {1u,"dead_diamond_phi_and_incoming_producers","feature",test_dead_diamond_phi_and_incoming_producers},
    {2u,"dead_phi_dependency_chain","feature",test_dead_phi_dependency_chain},
    {3u,"dead_self_loop_phi","feature",test_dead_self_loop_phi},
    {4u,"dead_mutually_recursive_phi_cycle","feature",test_dead_mutually_recursive_phi_cycle},
    {5u,"mixed_live_dead_phi_order_across_blocks","feature",test_mixed_live_dead_phi_order_across_blocks},
    {6u,"dead_phi_with_shared_effect_memory_incoming_backing","feature",test_dead_phi_with_shared_effect_memory_incoming_backing},
    {7u,"live_phi_return_chain","control",test_live_phi_return_chain},
    {8u,"one_root_keeps_mutual_cycle","control",test_one_root_keeps_mutual_cycle},
    {9u,"duplicate_predecessor_occurrences_preserved","control",test_duplicate_predecessor_occurrences_preserved},
    {10u,"metadata_rooted_phi_profiles","control",test_metadata_rooted_phi_profiles},
    {11u,"effectful_producer_feeding_dead_phi","feature",test_effectful_producer_feeding_dead_phi},
    {12u,"observable_boundary_profiles","full_feature",test_observable_boundary_profiles},
    {13u,"source_mapping_dead_and_live_chains","feature",test_source_mapping_dead_and_live_chains},
    {14u,"rich_metadata_does_not_disable_unrelated_dead_phi_elimination","feature",test_rich_metadata_does_not_disable_unrelated_dead_phi_elimination},
    {15u,"direct_budget_atomic_publication","full_feature",test_direct_budget_atomic_publication},
    {16u,"pipeline_budget_partial_success_contract","full_feature",test_pipeline_budget_partial_success_contract},
    {17u,"direct_all_actual_oom_and_retry","full_feature",test_direct_all_actual_oom_and_retry},
    {18u,"pipeline_cache_remarks_failure_restore","full_feature",test_pipeline_cache_remarks_failure_restore},
    {19u,"public_module_late_function_transaction","full_feature",test_public_module_late_function_transaction},
    {20u,"repeat_direct_and_pipeline_fixed_point","full_feature",test_repeat_direct_and_pipeline_fixed_point},
    {21u,"empty_and_all_live_nochange","control",test_empty_and_all_live_nochange},
};
int main(int argc,char **argv) {
    const char *mode="full";unsigned i,ran=0u,passed=0u,failed=0u;SDpTotals totals;
    if(argc==2&&strcmp(argv[1],"--red")==0)mode="red";
    else if(argc==2&&strcmp(argv[1],"--semantic")==0)mode="semantic";
    else if(argc!=1){fprintf(stderr,"expected no arguments, --red, or --semantic\n");return 2;}
    UNITY_BEGIN();
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i){const SDpCase *c=cases+i;UNITY_UINT before;
        if(strcmp(mode,"red")==0&&c->id!=1u&&c->id!=7u)continue;
        if(strcmp(mode,"semantic")==0&&c->id>=15u&&c->id<=20u)continue;
        before=Unity.TestFailures;current=(SDpResult){0u,0u};
        UnityDefaultTestRun(c->run,c->name,(int)c->id);++ran;
        if(Unity.TestFailures==before)++passed;else ++failed;
        printf("DCE_PHI_CASE id=%u name=%s kind=%s safety=%u feature=%u result=%s\n",c->id,c->name,c->kind,current.safety,current.feature,Unity.TestFailures==before?"PASS":"FAIL");
    }
    totals=Dp_Totals();
    printf("DCE_PHI_SUMMARY mode=%s cases=%u passed=%u failed=%u attempts=%u fault_measured=%u fault_completed=%u live=%u errors=%u\n",mode,ran,passed,failed,totals.attempts,totals.faultMeasured,totals.faultCompleted,totals.live,totals.errors);
    return UNITY_END();
}
