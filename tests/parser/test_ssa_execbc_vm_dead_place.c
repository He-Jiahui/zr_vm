#include "test_ssa_execbc_vm_dead_place_support.inc"

static void test_shared_external_provenance_for_dead_bases_materializes_as_nops(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS, DEAD_PLACE_MUTATION_NONE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(42, report.oracleInteger);
    TEST_ASSERT_EQUAL_UINT32(3u, report.oraclePlaceCount);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_TRUE(report.materialized);
    TEST_ASSERT_TRUE(report.vmReturned);
    TEST_ASSERT_EQUAL_INT64(report.oracleInteger, report.vmInteger);
    TEST_ASSERT_EQUAL_UINT32(3u, report.vmPlaceCount);
    TEST_ASSERT_TRUE(report.pcMapValid);
    TEST_ASSERT_TRUE(report.nopTraceMatchesOracle);
}

static void test_live_place_result_is_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_LIVE_RESULT, DEAD_PLACE_MUTATION_NONE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_external_provenance_mixed_with_copy_is_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_MIXED_EXTERNAL_USE,
            DEAD_PLACE_MUTATION_NONE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_place_result_used_by_phi_is_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_PHI_USE, DEAD_PLACE_MUTATION_NONE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_EQUAL_INT64(501, report.oracleInteger);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_nonzero_place_layout_metadata_is_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS, DEAD_PLACE_MUTATION_LAYOUT);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_malformed_place_result_shape_is_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS,
            DEAD_PLACE_MUTATION_MALFORMED_RESULT_SHAPE);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_per_instruction_effect_metadata_remains_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS, DEAD_PLACE_MUTATION_EFFECT);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_place_flags_on_non_place_value_are_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS,
            DEAD_PLACE_MUTATION_PLACE_FLAG_ON_SCALAR);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_unknown_value_flags_are_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS,
            DEAD_PLACE_MUTATION_UNKNOWN_VALUE_FLAG);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_external_entry_cannot_also_be_a_place_address(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS,
            DEAD_PLACE_MUTATION_EXTERNAL_PLACE_FLAGS);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_external_entry_cannot_be_defined_by_an_instruction(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS,
            DEAD_PLACE_MUTATION_EXTERNAL_VALUE_HAS_LOCAL_RESULT);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_projection_memory_token_pool_remains_rejected(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS,
            DEAD_PLACE_MUTATION_MEMORY_TOKEN_POOL);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

static void test_embedded_gc_map_is_rejected_when_present_flag_is_clear(void) {
    SDeadPlaceReport report = dead_place_run_case(
            DEAD_PLACE_FIXTURE_UNUSED_RESULTS,
            DEAD_PLACE_MUTATION_EMBEDDED_GC_MAP_WITHOUT_PRESENT);

    TEST_ASSERT_TRUE(report.fixtureBuilt);
    TEST_ASSERT_TRUE(report.oracleReturned);
    TEST_ASSERT_TRUE(report.projectionBuilt);
    TEST_ASSERT_TRUE(report.mutationApplied);
    TEST_ASSERT_FALSE(report.materialized);
    TEST_ASSERT_TRUE(report.outputEmptyOnFailure);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shared_external_provenance_for_dead_bases_materializes_as_nops);
    RUN_TEST(test_live_place_result_is_rejected);
    RUN_TEST(test_external_provenance_mixed_with_copy_is_rejected);
    RUN_TEST(test_place_result_used_by_phi_is_rejected);
    RUN_TEST(test_nonzero_place_layout_metadata_is_rejected);
    RUN_TEST(test_malformed_place_result_shape_is_rejected);
    RUN_TEST(test_per_instruction_effect_metadata_remains_rejected);
    RUN_TEST(test_place_flags_on_non_place_value_are_rejected);
    RUN_TEST(test_unknown_value_flags_are_rejected);
    RUN_TEST(test_external_entry_cannot_also_be_a_place_address);
    RUN_TEST(test_external_entry_cannot_be_defined_by_an_instruction);
    RUN_TEST(test_projection_memory_token_pool_remains_rejected);
    RUN_TEST(test_embedded_gc_map_is_rejected_when_present_flag_is_clear);
    return UNITY_END();
}
