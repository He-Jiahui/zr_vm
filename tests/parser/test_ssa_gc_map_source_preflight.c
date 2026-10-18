#include <stdio.h>
#include <stdlib.h>
#include "support/ssa_gc_map_source_preflight_fixture.h"
#include "unity.h"

void setUp(void) { Sp_SetUp(); }
void tearDown(void) { Sp_TearDown(); }

static void test_storage_frame_absent_null(void) { Sp_Matrix(SP_FRAME, SP_ABSENT, 0u); }
static void test_storage_frame_absent_writable(void) { Sp_Matrix(SP_FRAME, SP_ABSENT, 1u); }
static void test_storage_frame_empty_null(void) { Sp_Matrix(SP_FRAME, SP_EMPTY, 0u); }
static void test_storage_frame_empty_writable(void) { Sp_Matrix(SP_FRAME, SP_EMPTY, 1u); }
static void test_storage_frame_multi_null(void) { Sp_Matrix(SP_FRAME, SP_MULTI, 0u); }
static void test_storage_frame_multi_writable(void) { Sp_Matrix(SP_FRAME, SP_MULTI, 1u); }
static void test_storage_frame_real2_2_null(void) { Sp_Matrix(SP_FRAME, SP_REAL2_2, 0u); }
static void test_storage_frame_real2_2_writable(void) { Sp_Matrix(SP_FRAME, SP_REAL2_2, 1u); }
static void test_storage_frame_real1_2_null(void) { Sp_Matrix(SP_FRAME, SP_REAL1_2, 0u); }
static void test_storage_frame_real1_2_writable(void) { Sp_Matrix(SP_FRAME, SP_REAL1_2, 1u); }
static void test_storage_frame_nonnull0_0_null(void) { Sp_Matrix(SP_FRAME, SP_NONNULL0_0, 0u); }
static void test_storage_frame_nonnull0_0_writable(void) { Sp_Matrix(SP_FRAME, SP_NONNULL0_0, 1u); }
static void test_storage_frame_null1_1_null(void) { Sp_Matrix(SP_FRAME, SP_NULL1_1, 0u); }
static void test_storage_frame_null1_1_writable(void) { Sp_Matrix(SP_FRAME, SP_NULL1_1, 1u); }
static void test_storage_frame_null0_1_null(void) { Sp_Matrix(SP_FRAME, SP_NULL0_1, 0u); }
static void test_storage_frame_null0_1_writable(void) { Sp_Matrix(SP_FRAME, SP_NULL0_1, 1u); }
static void test_storage_frame_real2_1_null(void) { Sp_Matrix(SP_FRAME, SP_REAL2_1, 0u); }
static void test_storage_frame_real2_1_writable(void) { Sp_Matrix(SP_FRAME, SP_REAL2_1, 1u); }
static void test_storage_selected_absent_null(void) { Sp_Matrix(SP_SELECTED, SP_ABSENT, 0u); }
static void test_storage_selected_absent_writable(void) { Sp_Matrix(SP_SELECTED, SP_ABSENT, 1u); }
static void test_storage_selected_empty_null(void) { Sp_Matrix(SP_SELECTED, SP_EMPTY, 0u); }
static void test_storage_selected_empty_writable(void) { Sp_Matrix(SP_SELECTED, SP_EMPTY, 1u); }
static void test_storage_selected_multi_null(void) { Sp_Matrix(SP_SELECTED, SP_MULTI, 0u); }
static void test_storage_selected_multi_writable(void) { Sp_Matrix(SP_SELECTED, SP_MULTI, 1u); }
static void test_storage_selected_real2_2_null(void) { Sp_Matrix(SP_SELECTED, SP_REAL2_2, 0u); }
static void test_storage_selected_real2_2_writable(void) { Sp_Matrix(SP_SELECTED, SP_REAL2_2, 1u); }
static void test_storage_selected_real1_2_null(void) { Sp_Matrix(SP_SELECTED, SP_REAL1_2, 0u); }
static void test_storage_selected_real1_2_writable(void) { Sp_Matrix(SP_SELECTED, SP_REAL1_2, 1u); }
static void test_storage_selected_nonnull0_0_null(void) { Sp_Matrix(SP_SELECTED, SP_NONNULL0_0, 0u); }
static void test_storage_selected_nonnull0_0_writable(void) { Sp_Matrix(SP_SELECTED, SP_NONNULL0_0, 1u); }
static void test_storage_selected_null1_1_null(void) { Sp_Matrix(SP_SELECTED, SP_NULL1_1, 0u); }
static void test_storage_selected_null1_1_writable(void) { Sp_Matrix(SP_SELECTED, SP_NULL1_1, 1u); }
static void test_storage_selected_null0_1_null(void) { Sp_Matrix(SP_SELECTED, SP_NULL0_1, 0u); }
static void test_storage_selected_null0_1_writable(void) { Sp_Matrix(SP_SELECTED, SP_NULL0_1, 1u); }
static void test_storage_selected_real2_1_null(void) { Sp_Matrix(SP_SELECTED, SP_REAL2_1, 0u); }
static void test_storage_selected_real2_1_writable(void) { Sp_Matrix(SP_SELECTED, SP_REAL2_1, 1u); }
static void test_storage_finalized_absent_null(void) { Sp_Matrix(SP_FINALIZED, SP_ABSENT, 0u); }
static void test_storage_finalized_absent_writable(void) { Sp_Matrix(SP_FINALIZED, SP_ABSENT, 1u); }
static void test_storage_finalized_empty_null(void) { Sp_Matrix(SP_FINALIZED, SP_EMPTY, 0u); }
static void test_storage_finalized_empty_writable(void) { Sp_Matrix(SP_FINALIZED, SP_EMPTY, 1u); }
static void test_storage_finalized_multi_null(void) { Sp_Matrix(SP_FINALIZED, SP_MULTI, 0u); }
static void test_storage_finalized_multi_writable(void) { Sp_Matrix(SP_FINALIZED, SP_MULTI, 1u); }
static void test_storage_finalized_real2_2_null(void) { Sp_Matrix(SP_FINALIZED, SP_REAL2_2, 0u); }
static void test_storage_finalized_real2_2_writable(void) { Sp_Matrix(SP_FINALIZED, SP_REAL2_2, 1u); }
static void test_storage_finalized_real1_2_null(void) { Sp_Matrix(SP_FINALIZED, SP_REAL1_2, 0u); }
static void test_storage_finalized_real1_2_writable(void) { Sp_Matrix(SP_FINALIZED, SP_REAL1_2, 1u); }
static void test_storage_finalized_nonnull0_0_null(void) { Sp_Matrix(SP_FINALIZED, SP_NONNULL0_0, 0u); }
static void test_storage_finalized_nonnull0_0_writable(void) { Sp_Matrix(SP_FINALIZED, SP_NONNULL0_0, 1u); }
static void test_storage_finalized_null1_1_null(void) { Sp_Matrix(SP_FINALIZED, SP_NULL1_1, 0u); }
static void test_storage_finalized_null1_1_writable(void) { Sp_Matrix(SP_FINALIZED, SP_NULL1_1, 1u); }
static void test_storage_finalized_null0_1_null(void) { Sp_Matrix(SP_FINALIZED, SP_NULL0_1, 0u); }
static void test_storage_finalized_null0_1_writable(void) { Sp_Matrix(SP_FINALIZED, SP_NULL0_1, 1u); }
static void test_storage_finalized_real2_1_null(void) { Sp_Matrix(SP_FINALIZED, SP_REAL2_1, 0u); }
static void test_storage_finalized_real2_1_writable(void) { Sp_Matrix(SP_FINALIZED, SP_REAL2_1, 1u); }
static void test_storage_candidate_absent_null(void) { Sp_Matrix(SP_CANDIDATE, SP_ABSENT, 0u); }
static void test_storage_candidate_absent_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_ABSENT, 1u); }
static void test_storage_candidate_empty_null(void) { Sp_Matrix(SP_CANDIDATE, SP_EMPTY, 0u); }
static void test_storage_candidate_empty_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_EMPTY, 1u); }
static void test_storage_candidate_multi_null(void) { Sp_Matrix(SP_CANDIDATE, SP_MULTI, 0u); }
static void test_storage_candidate_multi_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_MULTI, 1u); }
static void test_storage_candidate_real2_2_null(void) { Sp_Matrix(SP_CANDIDATE, SP_REAL2_2, 0u); }
static void test_storage_candidate_real2_2_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_REAL2_2, 1u); }
static void test_storage_candidate_real1_2_null(void) { Sp_Matrix(SP_CANDIDATE, SP_REAL1_2, 0u); }
static void test_storage_candidate_real1_2_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_REAL1_2, 1u); }
static void test_storage_candidate_nonnull0_0_null(void) { Sp_Matrix(SP_CANDIDATE, SP_NONNULL0_0, 0u); }
static void test_storage_candidate_nonnull0_0_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_NONNULL0_0, 1u); }
static void test_storage_candidate_null1_1_null(void) { Sp_Matrix(SP_CANDIDATE, SP_NULL1_1, 0u); }
static void test_storage_candidate_null1_1_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_NULL1_1, 1u); }
static void test_storage_candidate_null0_1_null(void) { Sp_Matrix(SP_CANDIDATE, SP_NULL0_1, 0u); }
static void test_storage_candidate_null0_1_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_NULL0_1, 1u); }
static void test_storage_candidate_real2_1_null(void) { Sp_Matrix(SP_CANDIDATE, SP_REAL2_1, 0u); }
static void test_storage_candidate_real2_1_writable(void) { Sp_Matrix(SP_CANDIDATE, SP_REAL2_1, 1u); }
static void test_public_frame_real2_2_null(void) { Sp_PublicMalformed(SP_FRAME, 0u); }
static void test_public_frame_real2_2_writable(void) { Sp_PublicMalformed(SP_FRAME, 1u); }
static void test_public_selected_real2_2_null(void) { Sp_PublicMalformed(SP_SELECTED, 0u); }
static void test_public_selected_real2_2_writable(void) { Sp_PublicMalformed(SP_SELECTED, 1u); }
static void test_public_finalized_real2_2_null(void) { Sp_PublicMalformed(SP_FINALIZED, 0u); }
static void test_public_finalized_real2_2_writable(void) { Sp_PublicMalformed(SP_FINALIZED, 1u); }
static void test_profile_frame_empty_null(void) { Sp_Profile(SP_FRAME, SP_EMPTY, 0u); }
static void test_profile_frame_empty_writable(void) { Sp_Profile(SP_FRAME, SP_EMPTY, 1u); }
static void test_profile_frame_multi_null(void) { Sp_Profile(SP_FRAME, SP_MULTI, 0u); }
static void test_profile_frame_multi_writable(void) { Sp_Profile(SP_FRAME, SP_MULTI, 1u); }
static void test_profile_selected_empty_null(void) { Sp_Profile(SP_SELECTED, SP_EMPTY, 0u); }
static void test_profile_selected_empty_writable(void) { Sp_Profile(SP_SELECTED, SP_EMPTY, 1u); }
static void test_profile_selected_multi_null(void) { Sp_Profile(SP_SELECTED, SP_MULTI, 0u); }
static void test_profile_selected_multi_writable(void) { Sp_Profile(SP_SELECTED, SP_MULTI, 1u); }
static void test_profile_finalized_empty_null(void) { Sp_Profile(SP_FINALIZED, SP_EMPTY, 0u); }
static void test_profile_finalized_empty_writable(void) { Sp_Profile(SP_FINALIZED, SP_EMPTY, 1u); }
static void test_profile_finalized_multi_null(void) { Sp_Profile(SP_FINALIZED, SP_MULTI, 0u); }
static void test_profile_finalized_multi_writable(void) { Sp_Profile(SP_FINALIZED, SP_MULTI, 1u); }
static void test_priority_frame_0_null(void) { Sp_Priority(0u, 0u); }
static void test_priority_frame_0_writable(void) { Sp_Priority(0u, 1u); }
static void test_priority_frame_1_null(void) { Sp_Priority(1u, 0u); }
static void test_priority_frame_1_writable(void) { Sp_Priority(1u, 1u); }
static void test_priority_frame_2_null(void) { Sp_Priority(2u, 0u); }
static void test_priority_frame_2_writable(void) { Sp_Priority(2u, 1u); }
static void test_priority_frame_3_null(void) { Sp_Priority(3u, 0u); }
static void test_priority_frame_3_writable(void) { Sp_Priority(3u, 1u); }
static void test_priority_snapshot_0_null(void) { Sp_Priority(0u, 0u); }
static void test_priority_snapshot_0_writable(void) { Sp_Priority(0u, 1u); }
static void test_priority_snapshot_1_null(void) { Sp_Priority(1u, 0u); }
static void test_priority_snapshot_1_writable(void) { Sp_Priority(1u, 1u); }
static void test_priority_snapshot_2_null(void) { Sp_Priority(2u, 0u); }
static void test_priority_snapshot_2_writable(void) { Sp_Priority(2u, 1u); }
static void test_priority_snapshot_3_null(void) { Sp_Priority(3u, 0u); }
static void test_priority_snapshot_3_writable(void) { Sp_Priority(3u, 1u); }
static void test_healthy_frame(void) { Sp_Healthy(); }
static void test_healthy_snapshot(void) { Sp_Healthy(); }
static void test_grow_oom_retry_null(void) { Sp_Grow(0u); }
static void test_grow_oom_retry_writable(void) { Sp_Grow(1u); }

typedef struct SSpCase {
    const char *name, *kind;
    unsigned family;
    UnityTestFunction function;
} SSpCase;
/* Exact ordered case protocol; each row owns a fresh real fixture lifecycle. */
static const SSpCase g_cases[] = {
    { "storage_frame_absent_null", "legal", SP_PF, test_storage_frame_absent_null },
    { "storage_frame_absent_writable", "legal", SP_PF, test_storage_frame_absent_writable },
    { "storage_frame_empty_null", "legal", SP_PF, test_storage_frame_empty_null },
    { "storage_frame_empty_writable", "legal", SP_PF, test_storage_frame_empty_writable },
    { "storage_frame_multi_null", "legal", SP_PF, test_storage_frame_multi_null },
    { "storage_frame_multi_writable", "legal", SP_PF, test_storage_frame_multi_writable },
    { "storage_frame_real2_2_null", "malformed", SP_PF, test_storage_frame_real2_2_null },
    { "storage_frame_real2_2_writable", "malformed", SP_PF, test_storage_frame_real2_2_writable },
    { "storage_frame_real1_2_null", "malformed", SP_PF, test_storage_frame_real1_2_null },
    { "storage_frame_real1_2_writable", "malformed", SP_PF, test_storage_frame_real1_2_writable },
    { "storage_frame_nonnull0_0_null", "malformed", SP_PF, test_storage_frame_nonnull0_0_null },
    { "storage_frame_nonnull0_0_writable", "malformed", SP_PF, test_storage_frame_nonnull0_0_writable },
    { "storage_frame_null1_1_null", "malformed", SP_PF, test_storage_frame_null1_1_null },
    { "storage_frame_null1_1_writable", "malformed", SP_PF, test_storage_frame_null1_1_writable },
    { "storage_frame_null0_1_null", "malformed", SP_PF, test_storage_frame_null0_1_null },
    { "storage_frame_null0_1_writable", "malformed", SP_PF, test_storage_frame_null0_1_writable },
    { "storage_frame_real2_1_null", "malformed", SP_PF, test_storage_frame_real2_1_null },
    { "storage_frame_real2_1_writable", "malformed", SP_PF, test_storage_frame_real2_1_writable },
    { "storage_selected_absent_null", "legal", SP_FM, test_storage_selected_absent_null },
    { "storage_selected_absent_writable", "legal", SP_FM, test_storage_selected_absent_writable },
    { "storage_selected_empty_null", "legal", SP_FM, test_storage_selected_empty_null },
    { "storage_selected_empty_writable", "legal", SP_FM, test_storage_selected_empty_writable },
    { "storage_selected_multi_null", "legal", SP_FM, test_storage_selected_multi_null },
    { "storage_selected_multi_writable", "legal", SP_FM, test_storage_selected_multi_writable },
    { "storage_selected_real2_2_null", "malformed", SP_FM, test_storage_selected_real2_2_null },
    { "storage_selected_real2_2_writable", "malformed", SP_FM, test_storage_selected_real2_2_writable },
    { "storage_selected_real1_2_null", "malformed", SP_FM, test_storage_selected_real1_2_null },
    { "storage_selected_real1_2_writable", "malformed", SP_FM, test_storage_selected_real1_2_writable },
    { "storage_selected_nonnull0_0_null", "malformed", SP_FM, test_storage_selected_nonnull0_0_null },
    { "storage_selected_nonnull0_0_writable", "malformed", SP_FM, test_storage_selected_nonnull0_0_writable },
    { "storage_selected_null1_1_null", "malformed", SP_FM, test_storage_selected_null1_1_null },
    { "storage_selected_null1_1_writable", "malformed", SP_FM, test_storage_selected_null1_1_writable },
    { "storage_selected_null0_1_null", "malformed", SP_FM, test_storage_selected_null0_1_null },
    { "storage_selected_null0_1_writable", "malformed", SP_FM, test_storage_selected_null0_1_writable },
    { "storage_selected_real2_1_null", "malformed", SP_FM, test_storage_selected_real2_1_null },
    { "storage_selected_real2_1_writable", "malformed", SP_FM, test_storage_selected_real2_1_writable },
    { "storage_finalized_absent_null", "legal", SP_FM, test_storage_finalized_absent_null },
    { "storage_finalized_absent_writable", "legal", SP_FM, test_storage_finalized_absent_writable },
    { "storage_finalized_empty_null", "legal", SP_FM, test_storage_finalized_empty_null },
    { "storage_finalized_empty_writable", "legal", SP_FM, test_storage_finalized_empty_writable },
    { "storage_finalized_multi_null", "legal", SP_FM, test_storage_finalized_multi_null },
    { "storage_finalized_multi_writable", "legal", SP_FM, test_storage_finalized_multi_writable },
    { "storage_finalized_real2_2_null", "malformed", SP_FM, test_storage_finalized_real2_2_null },
    { "storage_finalized_real2_2_writable", "malformed", SP_FM, test_storage_finalized_real2_2_writable },
    { "storage_finalized_real1_2_null", "malformed", SP_FM, test_storage_finalized_real1_2_null },
    { "storage_finalized_real1_2_writable", "malformed", SP_FM, test_storage_finalized_real1_2_writable },
    { "storage_finalized_nonnull0_0_null", "malformed", SP_FM, test_storage_finalized_nonnull0_0_null },
    { "storage_finalized_nonnull0_0_writable", "malformed", SP_FM, test_storage_finalized_nonnull0_0_writable },
    { "storage_finalized_null1_1_null", "malformed", SP_FM, test_storage_finalized_null1_1_null },
    { "storage_finalized_null1_1_writable", "malformed", SP_FM, test_storage_finalized_null1_1_writable },
    { "storage_finalized_null0_1_null", "malformed", SP_FM, test_storage_finalized_null0_1_null },
    { "storage_finalized_null0_1_writable", "malformed", SP_FM, test_storage_finalized_null0_1_writable },
    { "storage_finalized_real2_1_null", "malformed", SP_FM, test_storage_finalized_real2_1_null },
    { "storage_finalized_real2_1_writable", "malformed", SP_FM, test_storage_finalized_real2_1_writable },
    { "storage_candidate_absent_null", "legal", SP_FM, test_storage_candidate_absent_null },
    { "storage_candidate_absent_writable", "legal", SP_FM, test_storage_candidate_absent_writable },
    { "storage_candidate_empty_null", "legal", SP_FM, test_storage_candidate_empty_null },
    { "storage_candidate_empty_writable", "legal", SP_FM, test_storage_candidate_empty_writable },
    { "storage_candidate_multi_null", "legal", SP_FM, test_storage_candidate_multi_null },
    { "storage_candidate_multi_writable", "legal", SP_FM, test_storage_candidate_multi_writable },
    { "storage_candidate_real2_2_null", "malformed", SP_FM, test_storage_candidate_real2_2_null },
    { "storage_candidate_real2_2_writable", "malformed", SP_FM, test_storage_candidate_real2_2_writable },
    { "storage_candidate_real1_2_null", "malformed", SP_FM, test_storage_candidate_real1_2_null },
    { "storage_candidate_real1_2_writable", "malformed", SP_FM, test_storage_candidate_real1_2_writable },
    { "storage_candidate_nonnull0_0_null", "malformed", SP_FM, test_storage_candidate_nonnull0_0_null },
    { "storage_candidate_nonnull0_0_writable", "malformed", SP_FM, test_storage_candidate_nonnull0_0_writable },
    { "storage_candidate_null1_1_null", "malformed", SP_FM, test_storage_candidate_null1_1_null },
    { "storage_candidate_null1_1_writable", "malformed", SP_FM, test_storage_candidate_null1_1_writable },
    { "storage_candidate_null0_1_null", "malformed", SP_FM, test_storage_candidate_null0_1_null },
    { "storage_candidate_null0_1_writable", "malformed", SP_FM, test_storage_candidate_null0_1_writable },
    { "storage_candidate_real2_1_null", "malformed", SP_FM, test_storage_candidate_real2_1_null },
    { "storage_candidate_real2_1_writable", "malformed", SP_FM, test_storage_candidate_real2_1_writable },
    { "public_frame_real2_2_null", "malformed", SP_PF, test_public_frame_real2_2_null },
    { "public_frame_real2_2_writable", "malformed", SP_PF, test_public_frame_real2_2_writable },
    { "public_selected_real2_2_null", "malformed", SP_FM, test_public_selected_real2_2_null },
    { "public_selected_real2_2_writable", "malformed", SP_FM, test_public_selected_real2_2_writable },
    { "public_finalized_real2_2_null", "malformed", SP_FM, test_public_finalized_real2_2_null },
    { "public_finalized_real2_2_writable", "malformed", SP_FM, test_public_finalized_real2_2_writable },
    { "profile_frame_empty_null", "control", SP_PF, test_profile_frame_empty_null },
    { "profile_frame_empty_writable", "control", SP_PF, test_profile_frame_empty_writable },
    { "profile_frame_multi_null", "control", SP_PF, test_profile_frame_multi_null },
    { "profile_frame_multi_writable", "control", SP_PF, test_profile_frame_multi_writable },
    { "profile_selected_empty_null", "control", SP_FM, test_profile_selected_empty_null },
    { "profile_selected_empty_writable", "control", SP_FM, test_profile_selected_empty_writable },
    { "profile_selected_multi_null", "control", SP_FM, test_profile_selected_multi_null },
    { "profile_selected_multi_writable", "control", SP_FM, test_profile_selected_multi_writable },
    { "profile_finalized_empty_null", "control", SP_FM, test_profile_finalized_empty_null },
    { "profile_finalized_empty_writable", "control", SP_FM, test_profile_finalized_empty_writable },
    { "profile_finalized_multi_null", "control", SP_FM, test_profile_finalized_multi_null },
    { "profile_finalized_multi_writable", "control", SP_FM, test_profile_finalized_multi_writable },
    { "priority_frame_0_null", "control", SP_PF, test_priority_frame_0_null },
    { "priority_frame_0_writable", "control", SP_PF, test_priority_frame_0_writable },
    { "priority_frame_1_null", "control", SP_PF, test_priority_frame_1_null },
    { "priority_frame_1_writable", "control", SP_PF, test_priority_frame_1_writable },
    { "priority_frame_2_null", "control", SP_PF, test_priority_frame_2_null },
    { "priority_frame_2_writable", "control", SP_PF, test_priority_frame_2_writable },
    { "priority_frame_3_null", "control", SP_PF, test_priority_frame_3_null },
    { "priority_frame_3_writable", "control", SP_PF, test_priority_frame_3_writable },
    { "priority_snapshot_0_null", "control", SP_FM, test_priority_snapshot_0_null },
    { "priority_snapshot_0_writable", "control", SP_FM, test_priority_snapshot_0_writable },
    { "priority_snapshot_1_null", "control", SP_FM, test_priority_snapshot_1_null },
    { "priority_snapshot_1_writable", "control", SP_FM, test_priority_snapshot_1_writable },
    { "priority_snapshot_2_null", "control", SP_FM, test_priority_snapshot_2_null },
    { "priority_snapshot_2_writable", "control", SP_FM, test_priority_snapshot_2_writable },
    { "priority_snapshot_3_null", "control", SP_FM, test_priority_snapshot_3_null },
    { "priority_snapshot_3_writable", "control", SP_FM, test_priority_snapshot_3_writable },
    { "healthy_frame", "control", SP_PF, test_healthy_frame },
    { "healthy_snapshot", "control", SP_FM, test_healthy_snapshot },
    { "grow_oom_retry_null", "control", SP_FM, test_grow_oom_retry_null },
    { "grow_oom_retry_writable", "control", SP_FM, test_grow_oom_retry_writable },
};

int main(void) {
    unsigned passed = 0u, failed = 0u;
    SSpSummary summary;
    UNITY_BEGIN();
    for (unsigned i = 0u; i < sizeof(g_cases) / sizeof(g_cases[0]); ++i) {
        unsigned failures = (unsigned)Unity.TestFailures;
        unsigned ignored = (unsigned)Unity.TestIgnores;
        unsigned ok;
        Sp_SetFamily(g_cases[i].family);
        UnityDefaultTestRun(g_cases[i].function, g_cases[i].name, __LINE__);
        ok = failures == (unsigned)Unity.TestFailures && ignored == (unsigned)Unity.TestIgnores;
        if (ok) ++passed; else ++failed;
        printf("SOURCE_PREFLIGHT_CASE name=%s kind=%s result=%s\n",
               g_cases[i].name, g_cases[i].kind, ok ? "PASS" : "FAIL");
    }
    summary = Sp_Summary();
    printf("SOURCE_PREFLIGHT_SUMMARY cases=%u passed=%u failed=%u attempts=%u live=%u errors=%u fault_measured=%u fault_completed=%u\n",
           (unsigned)(sizeof(g_cases) / sizeof(g_cases[0])), passed, failed, summary.attempts,
           summary.live, summary.errors, summary.faultMeasured, summary.faultCompleted);
    return UNITY_END();
}
