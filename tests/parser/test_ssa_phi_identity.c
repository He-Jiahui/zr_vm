#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "support/ssa_phi_identity_fixture.h"

static SPiResult current;
void setUp(void) { Pi_SetUp(); }
void tearDown(void) { Pi_TearDown(); }
static void check(unsigned id,const char *name) {
    current=Pi_Semantic(id,name);
    /* Every profile has already disposed all owners before Unity longjmp. */
    TEST_ASSERT_TRUE_MESSAGE(current.safety,"PHI identity safety/precondition/owner/ledger failure");
    TEST_ASSERT_TRUE_MESSAGE(current.feature,"PHI identity expected transformation missing");
}
static void same_source(void) { check(1u,"public_unknown_same_source_diamond"); }
static void distinct_source(void) { check(2u,"distinct_source_diamond_retained"); }
static void disabled_sccp(void) { check(3u,"disabled_sccp_retains_live_phi"); }
#define MATRIX_CASE(id,name) static void case_##id(void) { check(id,name); }
MATRIX_CASE(4,"duplicate_edge_occurrences")
MATRIX_CASE(5,"copy_phi_chains_reverse_layout")
MATRIX_CASE(6,"definition_kinds_real_dominance_throwing")
MATRIX_CASE(7,"two_function_alias_isolation")
MATRIX_CASE(8,"descriptor_edges_and_repeated_mismatch")
MATRIX_CASE(9,"groundless_and_mixed_cycle_controls")
MATRIX_CASE(10,"global_consumption_tombstones")
MATRIX_CASE(11,"identity_metadata_freeze")
MATRIX_CASE(12,"active_token_and_shared_incoming_domains")
MATRIX_CASE(13,"source_maps_and_shared_operand_owners")
MATRIX_CASE(14,"disabled_pass_controls")
MATRIX_CASE(15,"three_run_fixedpoint_and_changed_reporting")
MATRIX_CASE(16,"measured_private_work_boundaries")
MATRIX_CASE(17,"finite_public_budget_bound")
MATRIX_CASE(18,"direct_actual_allocation_faults")
MATRIX_CASE(19,"manager_cache_context_prefix_rollback")
MATRIX_CASE(20,"public_late_function_rollback")
MATRIX_CASE(21,"empty_and_distinct_no_candidate")
#undef MATRIX_CASE
typedef struct SPiCase { unsigned id;const char *name,*kind;UnityTestFunction run; } SPiCase;
static const SPiCase cases[]={
    {1u,"public_unknown_same_source_diamond","feature",same_source},
    {2u,"distinct_source_diamond_retained","control",distinct_source},
    {3u,"disabled_sccp_retains_live_phi","control",disabled_sccp},
    {4u,"duplicate_edge_occurrences","semantic",case_4},
    {5u,"copy_phi_chains_reverse_layout","semantic",case_5},
    {6u,"definition_kinds_real_dominance_throwing","semantic_and_rejection",case_6},
    {7u,"two_function_alias_isolation","semantic",case_7},
    {8u,"descriptor_edges_and_repeated_mismatch","control",case_8},
    {9u,"groundless_and_mixed_cycle_controls","semantic_and_rejection",case_9},
    {10u,"global_consumption_tombstones","control",case_10},
    {11u,"identity_metadata_freeze","control",case_11},
    {12u,"active_token_and_shared_incoming_domains","semantic",case_12},
    {13u,"source_maps_and_shared_operand_owners","semantic_and_rejection",case_13},
    {14u,"disabled_pass_controls","control",case_14},
    {15u,"three_run_fixedpoint_and_changed_reporting","semantic",case_15},
    {16u,"measured_private_work_boundaries","budget",case_16},
    {17u,"finite_public_budget_bound","budget",case_17},
    {18u,"direct_actual_allocation_faults","fault",case_18},
    {19u,"manager_cache_context_prefix_rollback","fault",case_19},
    {20u,"public_late_function_rollback","fault",case_20},
    {21u,"empty_and_distinct_no_candidate","control",case_21}
};
int main(int argc,char **argv) {
    const char *mode="full";
    unsigned i,passed=0u,failed=0u,ran=0u,selected=0u;SPiTotals totals;
    if(argc==2&&(strcmp(argv[1],"--red")==0||strcmp(argv[1],"--minimal")==0))mode="red";
    else if(argc==2&&strcmp(argv[1],"--semantic")==0)mode="semantic";
    else if(argc==3&&strcmp(argv[1],"--case")==0){
        char *end=NULL;unsigned long value=strtoul(argv[2],&end,10);
        if(!argv[2][0]||argv[2][0]<'0'||argv[2][0]>'9'||!end||*end||value<1u||value>21u){
            fprintf(stderr,"--case requires an integer in 1..21\n");return 2;}
        selected=(unsigned)value;mode="case";
    }else if(argc!=1){fprintf(stderr,"expected no arguments, --red, --minimal, --semantic, or --case N (1..21)\n");return 2;}
    UNITY_BEGIN();
    for(i=0u;i<sizeof(cases)/sizeof(cases[0]);++i){
        const SPiCase *c=cases+i;UNITY_UINT before;
        if(selected&&c->id!=selected)continue;
        if(strcmp(mode,"red")==0&&c->id>3u)continue;
        if(strcmp(mode,"semantic")==0&&c->id>=16u&&c->id<=20u)continue;
        before=Unity.TestFailures;++ran;
        current=(SPiResult){0u,0u};
        UnityDefaultTestRun(c->run,c->name,(int)c->id);
        if(Unity.TestFailures==before)++passed;else ++failed;
        printf("PHI_IDENTITY_CASE id=%u name=%s kind=%s safety=%u feature=%u result=%s\n",
            c->id,c->name,c->kind,current.safety,current.feature,Unity.TestFailures==before?"PASS":"FAIL");
    }
    totals=Pi_Totals();
    printf("PHI_IDENTITY_SUMMARY mode=%s scope=%s cases=%u passed=%u failed=%u attempts=%u fault_measured=%u fault_completed=%u live=%u errors=%u\n",
        mode,strcmp(mode,"red")==0?"minimal":"matrix",ran,passed,failed,totals.attempts,totals.faultMeasured,totals.faultCompleted,totals.live,totals.errors);
    return UNITY_END();
}
