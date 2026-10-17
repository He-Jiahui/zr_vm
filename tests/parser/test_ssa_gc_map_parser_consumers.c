#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "zr_vm_parser/exec_ir_fusion.h"
#include "zr_vm_parser/exec_ir_container_specialize.h"
#include "exec_ir/exec_ir_pass_internal.h"
#include "exec_ir/exec_ir_mutation_internal.h"
/* System declarations precede redirects; Unity and allocator keep their CRT. */
#include "ssa_exec_ir_owned_module_shim.h"
#include "support/ssa_gc_map_parser_consumers_fixture.h"

enum EGcEntrance {
    GC_FUSION, GC_CONTAINER_BUILD, GC_CONTAINER_VALIDATE, GC_CONTAINER_SPECIALIZE,
    GC_DCE, GC_SCCP_COMPUTE, GC_SCCP_PASS, GC_ENTRANCE_COUNT
};
static unsigned g_entrance, g_row, g_null_diagnostic, g_legal_kind;
static const char *const g_entrance_names[GC_ENTRANCE_COUNT] = {
    "fusion_input_hash", "container_build", "container_validate", "container_specialize",
    "dce", "sccp_compute", "sccp_pass"
};
static const char *const g_shape_names[6] = {
    "real_2_2", "real_1_2", "nonnull_0_0", "null_1_1", "null_0_1", "real_2_1"
};

void setUp(void) {
    ZrTests_ExecOwner_Reset();
    memset(&g_function, 0, sizeof(g_function));
    memset(&g_facts, 0, sizeof(g_facts));
    memset(&g_plan, 0, sizeof(g_plan));
    memset(&g_cache, 0, sizeof(g_cache));
    memset(&g_context, 0, sizeof(g_context));
    g_borrowed = g_saved_map = NULL;
    g_saved_count = g_saved_capacity = g_bound = 0u;
}

void tearDown(void) {
    SZrOwnerHeapStats end;
    ZrTests_ExecOwner_Disarm();
    gc_release_fixture();
    end = ZrTests_ExecOwner_Stats();
    /* Recover is only failed-test containment; record actual cleanup first. */
    ZrTests_ExecOwner_Recover();
    TEST_ASSERT_EQUAL_UINT(0u, end.live);
    TEST_ASSERT_EQUAL_UINT(0u, end.errors);
    ZrTests_ExecOwner_AssertReleased(0u, end.records);
}

static TZrBool gc_consume(SZrExecIrDiagnostic *execution,
                         SZrContainerSpecializationDiagnostic *detail,
                         TZrBool *changed, TZrUInt64 *hash) {
    switch (g_entrance) {
        case GC_FUSION:
            *hash = ZrParser_ExecBcFusion_InputHash(&g_function);
            return (TZrBool)(*hash != 0u);
        case GC_CONTAINER_BUILD:
            return ZrParser_ExecIr_BuildContainerSpecialization(
                    &g_function, &g_facts, &g_plan, detail);
        case GC_CONTAINER_VALIDATE:
            return ZrParser_ExecIr_ValidateContainerSpecializationPlan(
                    &g_function, &g_facts, &g_plan, detail);
        case GC_CONTAINER_SPECIALIZE:
            return ZrParser_ExecIr_SpecializeContainers(&g_function, &g_facts, execution);
        case GC_DCE:
            return ZrParser_ExecIr_RunDcePass(&g_function, &g_context, changed, execution);
        case GC_SCCP_COMPUTE:
            return ZrParser_ExecIr_ComputeSccp(
                    &g_function, &g_context, ZR_FALSE, changed, execution);
        default:
            return ZrParser_ExecIr_RunSccpPass(&g_function, &g_context, changed, execution);
    }
}

static void gc_legal(unsigned kind) {
    SZrExecIrDiagnostic execution = {0}, expected = {0};
    SZrContainerSpecializationDiagnostic detail = {0}, buildDetail;
    TZrBool changed = ZR_TRUE, accepted, planReady = ZR_TRUE;
    TZrUInt64 hash = 0u;
    unsigned attempts;
    int innerPreserved = 1;
    SZrExecIrGcMapEntry entries[2];
    TZrUInt32 slots[2], offsets[2];
    gc_build_fixture(kind);
    if (g_entrance == GC_CONTAINER_SPECIALIZE) gc_admit_map();
    if (kind == 2u) {
        memcpy(entries, g_function.gcMap->entries, sizeof(entries));
        memcpy(slots, g_function.gcMap->slotIndexPool, sizeof(slots));
        memcpy(offsets, g_function.gcMap->inlineRefOffsetPool, sizeof(offsets));
    }
    if (g_entrance == GC_CONTAINER_VALIDATE)
        planReady = ZrParser_ExecIr_BuildContainerSpecialization(
                &g_function, &g_facts, &g_plan, &buildDetail);
    attempts = ZrTests_ExecOwner_Stats().attempts;
    accepted = gc_consume(&execution, &detail, &changed, &hash);
    attempts = ZrTests_ExecOwner_Stats().attempts - attempts;
    if (kind == 2u) {
        const SZrExecIrGcMap *map = g_function.gcMap;
        innerPreserved = map != NULL && map->entryCount == 2u &&
                map->slotIndexCount == 2u && map->inlineRefOffsetCount == 2u &&
                memcmp(entries, map->entries, sizeof(entries)) == 0 &&
                memcmp(slots, map->slotIndexPool, sizeof(slots)) == 0 &&
                memcmp(offsets, map->inlineRefOffsetPool, sizeof(offsets)) == 0;
    }
    printf("GC_LEGAL entrance=%s kind=%u accepted=%u attempts=%u\n",
            g_entrance_names[g_entrance], kind, (unsigned)accepted, attempts);
    gc_release_fixture();
    TEST_ASSERT_TRUE_MESSAGE(planReady, "healthy plan preparation failed");
    if (g_entrance == GC_CONTAINER_SPECIALIZE) {
        /* A real map admission makes NULL-diagnostic rejection observable. */
        TEST_ASSERT_TRUE(accepted);
        expected.code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        expected.functionToken = 73u;
        gc_diagnostic_equal(&expected, &execution);
    } else {
        TEST_ASSERT_TRUE_MESSAGE(accepted, "legal outer shape rejected");
    }
    if (g_entrance == GC_FUSION) TEST_ASSERT_TRUE(hash != 0u);
    if (g_entrance == GC_CONTAINER_BUILD) {
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT, detail.status);
        expected.code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        expected.functionToken = 73u;
        gc_diagnostic_equal(&expected, &detail.execution);
        TEST_ASSERT_TRUE(g_plan.planHash != 0u);
    }
    if (g_entrance == GC_CONTAINER_VALIDATE) gc_diagnostic_equal(&expected, &detail.execution);
    if (g_entrance >= GC_DCE) gc_diagnostic_equal(&expected, &execution);
    TEST_ASSERT_TRUE_MESSAGE(innerPreserved, "legal inner GC pools changed");
}

static void test_legal_shape(void) { gc_legal(g_legal_kind); }

static void test_malformed_shape(void) {
    SZrExecIrDiagnostic execution, expected = {0};
    SZrContainerSpecializationDiagnostic detail, buildDetail, expectedDetail;
    SZrContainerSpecializationPlan initializedPlan, resultPlan;
    SZrOwnerHeapStats before, after, released;
    TZrBool changed = ZR_TRUE, accepted, planReady = ZR_TRUE;
    TZrUInt64 hash = 0u;
    int preserved;
    /* Prove this exact entrance can consume a healthy real fixture first. */
    gc_legal(0u);
    gc_build_fixture(2u);
    if (g_entrance == GC_CONTAINER_SPECIALIZE) gc_admit_map();
    if (g_entrance == GC_CONTAINER_VALIDATE)
        planReady = ZrParser_ExecIr_BuildContainerSpecialization(
                &g_function, &g_facts, &g_plan, &buildDetail);
    TEST_ASSERT_TRUE_MESSAGE(planReady, "healthy keeper plan preparation failed");
    if (g_entrance == GC_CONTAINER_BUILD) memset(&g_plan, 0xa5, sizeof(g_plan));
    if (g_entrance == GC_SCCP_COMPUTE || g_entrance == GC_SCCP_PASS) {
        /* A real independent prior cache allocation must survive rejection. */
        g_cache.sccpValues = calloc(1u, sizeof(*g_cache.sccpValues));
        TEST_ASSERT_NOT_NULL(g_cache.sccpValues);
        g_cache.sccpValueCount = 1u;
        g_cache.revision = 17u;
        g_cache.irHash = 19u;
        g_cache.validMask = ZR_EXEC_IR_ANALYSIS_DOMINATORS;
    }
    ZrParser_ExecIr_ContainerSpecializationPlanInit(&initializedPlan);
    ZrParser_ExecIr_ContainerSpecializationDiagnosticInit(&expectedDetail);
    expectedDetail.status = ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_INVALID;
    expectedDetail.kind = ZR_EXEC_IR_CONTAINER_KIND_NONE;
    expectedDetail.execution.code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    expectedDetail.execution.functionToken = g_function.functionToken;
    expected.code = g_entrance == GC_CONTAINER_SPECIALIZE
            ? ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
    expected.functionToken = g_function.functionToken;
    memset(&execution, 0xa5, sizeof(execution));
    memset(&detail, 0xa5, sizeof(detail));
    gc_bind_malformed(g_row);
    if (!gc_snapshot()) {
        gc_release_fixture();
        TEST_FAIL_MESSAGE("real ledger snapshot unavailable");
        return;
    }
    before = ZrTests_ExecOwner_Stats();
    /* Deliberately UNARMED: acceptance cannot be hidden by synthetic OOM. */
    accepted = gc_consume(g_null_diagnostic ? NULL : &execution,
            g_null_diagnostic ? NULL : &detail, &changed, &hash);
    after = ZrTests_ExecOwner_Stats();
    preserved = gc_unchanged(g_entrance == GC_CONTAINER_BUILD);
    resultPlan = g_plan;
    gc_release_fixture();
    released = ZrTests_ExecOwner_Stats();
    printf("GC_MALFORMED entrance=%s shape=%s null_diag=%u accepted=%u "
           "hash=%llu attempts=%u changed=%u preserved=%d\n",
            g_entrance_names[g_entrance], g_shape_names[g_row], g_null_diagnostic,
            (unsigned)accepted, (unsigned long long)hash, after.attempts - before.attempts,
            (unsigned)changed, preserved);
    /* Both restoration and all owner cleanup already happened. */
    TEST_ASSERT_FALSE_MESSAGE(accepted, "malformed outer shape accepted");
    if (g_entrance == GC_FUSION) TEST_ASSERT_EQUAL_UINT64(0u, hash);
    TEST_ASSERT_EQUAL_UINT(before.attempts, after.attempts);
    TEST_ASSERT_EQUAL_UINT(before.records, after.records);
    TEST_ASSERT_EQUAL_UINT(before.live, after.live);
    TEST_ASSERT_EQUAL_UINT(before.frees, after.frees);
    TEST_ASSERT_EQUAL_UINT(before.errors, after.errors);
    TEST_ASSERT_EQUAL_UINT(before.failures, after.failures);
    TEST_ASSERT_TRUE_MESSAGE(preserved, "malformed call changed input or prior result");
    TEST_ASSERT_EQUAL_UINT(0u, released.live);
    TEST_ASSERT_EQUAL_UINT(0u, released.errors);
    if (g_entrance >= GC_DCE) TEST_ASSERT_FALSE(changed);
    if (g_entrance == GC_CONTAINER_BUILD)
        TEST_ASSERT_EQUAL_MEMORY(&initializedPlan, &resultPlan, sizeof(resultPlan));
    if (!g_null_diagnostic) {
        if (g_entrance == GC_CONTAINER_BUILD || g_entrance == GC_CONTAINER_VALIDATE) {
            TEST_ASSERT_EQUAL_INT(expectedDetail.status, detail.status);
            TEST_ASSERT_EQUAL_INT(expectedDetail.kind, detail.kind);
            gc_diagnostic_equal(&expectedDetail.execution, &detail.execution);
            TEST_ASSERT_EQUAL_MEMORY(&expectedDetail.storage, &detail.storage, sizeof(detail.storage));
        } else if (g_entrance != GC_FUSION) {
            gc_diagnostic_equal(&expected, &execution);
        }
    }
}

int main(void) {
    char name[160];
    UNITY_BEGIN();
    for (g_entrance = 0u; g_entrance < GC_ENTRANCE_COUNT; ++g_entrance) {
        for (g_legal_kind = 0u; g_legal_kind < 3u; ++g_legal_kind) {
            snprintf(name, sizeof(name), "legal_%s_kind_%u",
                    g_entrance_names[g_entrance], g_legal_kind);
            UnityDefaultTestRun(test_legal_shape, name, __LINE__);
        }
        for (g_row = 0u; g_row < 6u; ++g_row) {
            unsigned modes = g_entrance == GC_FUSION ? 1u : 2u;
            for (g_null_diagnostic = 0u; g_null_diagnostic < modes; ++g_null_diagnostic) {
                snprintf(name, sizeof(name), "reject_%s_%s_%s",
                        g_entrance_names[g_entrance], g_shape_names[g_row],
                        g_null_diagnostic ? "null_diagnostic" : "poisoned_diagnostic");
                UnityDefaultTestRun(test_malformed_shape, name, __LINE__);
            }
        }
    }
    return UNITY_END();
}
