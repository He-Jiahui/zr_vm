#include "zr_vm_parser/exec_ir_container_specialize.h"

#include <assert.h>
#include <string.h>

static void make_function(SZrExecIrFunction *function) {
    memset(function, 0, sizeof(*function));
    function->id = 7u;
    function->functionToken = 42u;
    function->signatureHash = UINT64_C(0x1122334455667788);
}

static void make_stable_map(SZrCompactMapCandidate *candidate) {
    ZrCore_CompactMapCandidate_Init(candidate);
    candidate->hash.seed = 11u;
    candidate->hash.domain = 22u;
    candidate->hash.version = 3u;
    candidate->hash.flags = ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
                            ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE |
                            ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE |
                            ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL |
                            ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE;
    candidate->layout.entrySize = 32u;
    candidate->layout.entryAlignment = 8u;
    candidate->layout.bucketCount = 8u;
    candidate->layout.maxLoadNumerator = 3u;
    candidate->layout.maxLoadDenominator = 4u;
    candidate->layout.hashOffset = 0u;
    candidate->layout.keyOffset = 8u;
    candidate->layout.valueOffset = 16u;
    candidate->layout.nextOffset = 24u;
    candidate->layout.tombstonePolicy = ZR_COMPACT_MAP_TOMBSTONE_MARK;
    candidate->layout.iterationPolicy = ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER;
    candidate->layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_OWNED;
    candidate->flags = ZR_COMPACT_MAP_FLAG_CACHE_HASH |
                      ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION;
    assert(ZrCore_CompactMapCandidate_Finalize(candidate, ZR_NULL));
}

static void make_builder_string(SZrStringStorageFacts *facts) {
    ZrCore_StringStorageFacts_Init(facts);
    facts->flags = ZR_STRING_STORAGE_FLAG_IMMUTABLE |
                   ZR_STRING_STORAGE_FLAG_UTF8_VALID |
                   ZR_STRING_STORAGE_FLAG_BUILDER_EFFECTS_EQUIVALENT;
    facts->byteLength = 1024u;
    facts->intermediateCount = 4u;
    facts->maxBuilderBytes = 2048u;
    facts->declaredEffects = ZR_EXECUTION_EFFECT_ALLOCATE;
    facts->replacementEffects = ZR_EXECUTION_EFFECT_ALLOCATE;
}

static void test_stable_map_and_builder_are_admitted(void) {
    SZrExecIrFunction function;
    SZrContainerSpecializationFacts facts;
    SZrContainerSpecializationPlan plan;
    SZrContainerSpecializationDiagnostic diagnostic;
    make_function(&function);
    ZrParser_ExecIr_ContainerSpecializationFactsInit(&facts);
    facts.functionToken = function.functionToken;
    facts.mapKnown = ZR_TRUE;
    facts.stringKnown = ZR_TRUE;
    make_stable_map(&facts.mapCandidate);
    make_builder_string(&facts.stringFacts);

    ZrParser_ExecIr_ContainerSpecializationPlanInit(&plan);
    assert(ZrParser_ExecIr_BuildContainerSpecialization(
            &function, &facts, &plan, &diagnostic));
    assert(plan.mapStrategy == ZR_EXEC_IR_CONTAINER_MAP_COMPACT);
    assert(plan.cacheHash == ZR_TRUE);
    assert(plan.stringStrategy == ZR_STRING_STORAGE_STRATEGY_BUILDER);
    assert(plan.preservesGenericEquality == ZR_TRUE);
    assert(plan.planHash != 0u);
    assert(diagnostic.status == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK);
    assert(ZrParser_ExecIr_SpecializeContainers(
            &function, &facts, &diagnostic.execution));
}

static void test_unknown_layout_keeps_generic_path(void) {
    SZrExecIrFunction function;
    SZrContainerSpecializationFacts facts;
    SZrContainerSpecializationPlan plan;
    SZrContainerSpecializationDiagnostic diagnostic;
    make_function(&function);
    ZrParser_ExecIr_ContainerSpecializationFactsInit(&facts);
    facts.functionToken = function.functionToken;

    assert(ZrParser_ExecIr_BuildContainerSpecialization(
            &function, &facts, &plan, &diagnostic));
    assert(plan.mapStrategy == ZR_EXEC_IR_CONTAINER_MAP_GENERIC);
    assert(plan.stringStrategy == ZR_STRING_STORAGE_STRATEGY_GENERIC);
    assert(plan.preservesGenericEquality == ZR_TRUE);
    assert(plan.fallbackReason == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT);
    assert(diagnostic.status == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT);
}

static void test_custom_equality_and_escape_are_rejected(void) {
    SZrExecIrFunction function;
    SZrContainerSpecializationFacts facts;
    SZrContainerSpecializationPlan plan;
    SZrContainerSpecializationDiagnostic diagnostic;
    make_function(&function);
    ZrParser_ExecIr_ContainerSpecializationFactsInit(&facts);
    facts.functionToken = function.functionToken;
    facts.mapKnown = ZR_TRUE;
    facts.stringKnown = ZR_TRUE;
    make_stable_map(&facts.mapCandidate);
    facts.mapCandidate.hash.flags |= ZR_COMPACT_MAP_HASH_FLAG_CUSTOM_EQUALITY;
    make_builder_string(&facts.stringFacts);
    facts.stringFacts.flags |= ZR_STRING_STORAGE_FLAG_IDENTITY_OBSERVED;

    assert(ZrParser_ExecIr_BuildContainerSpecialization(
            &function, &facts, &plan, &diagnostic));
    assert(plan.mapStrategy == ZR_EXEC_IR_CONTAINER_MAP_GENERIC);
    assert(plan.stringStrategy == ZR_STRING_STORAGE_STRATEGY_GENERIC);
    assert(plan.preservesGenericEquality == ZR_TRUE);
    assert(diagnostic.status != ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK);
}

static void test_sealed_function_is_not_mutated(void) {
    SZrExecIrFunction function;
    SZrContainerSpecializationFacts facts;
    SZrContainerSpecializationPlan plan;
    SZrContainerSpecializationDiagnostic diagnostic;
    make_function(&function);
    function.sealed = ZR_TRUE;
    ZrParser_ExecIr_ContainerSpecializationFactsInit(&facts);
    assert(!ZrParser_ExecIr_BuildContainerSpecialization(
            &function, &facts, &plan, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SEALED);
}

static void test_plan_is_bound_to_current_scalar_facts(void) {
    SZrExecIrFunction function;
    SZrContainerSpecializationFacts facts;
    SZrContainerSpecializationPlan plan;
    SZrContainerSpecializationDiagnostic diagnostic;
    TZrUInt64 savedHash;
    make_function(&function);
    ZrParser_ExecIr_ContainerSpecializationFactsInit(&facts);
    facts.functionToken = function.functionToken;
    facts.mapKnown = ZR_TRUE;
    make_stable_map(&facts.mapCandidate);

    assert(ZrParser_ExecIr_BuildContainerSpecialization(
            &function, &facts, &plan, &diagnostic));
    assert(ZrParser_ExecIr_ValidateContainerSpecializationPlan(
            &function, &facts, &plan, &diagnostic));

    savedHash = plan.planHash;
    plan.planHash ^= UINT64_C(1);
    assert(!ZrParser_ExecIr_ValidateContainerSpecializationPlan(
            &function, &facts, &plan, &diagnostic));
    assert(diagnostic.status ==
           ZR_EXEC_IR_CONTAINER_SPECIALIZATION_PLAN_HASH_MISMATCH);
    plan.planHash = savedHash;

    facts.mapCandidate.layout.bucketCount = 16u;
    assert(!ZrParser_ExecIr_ValidateContainerSpecializationPlan(
            &function, &facts, &plan, &diagnostic));
    assert(diagnostic.status != ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK);
}

static void test_draft_entry_reports_generic_fallback_without_rewriting_ir(void) {
    SZrExecIrFunction function;
    SZrContainerSpecializationFacts facts;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 before;
    make_function(&function);
    ZrParser_ExecIr_ContainerSpecializationFactsInit(&facts);
    before = ZrParser_ExecIr_ContainerSpecializationFactsHash(&facts);

    assert(!ZrParser_ExecIr_SpecializeContainers(&function, &facts, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert(before == ZrParser_ExecIr_ContainerSpecializationFactsHash(&facts));
}

int main(void) {
    test_stable_map_and_builder_are_admitted();
    test_unknown_layout_keeps_generic_path();
    test_custom_equality_and_escape_are_rejected();
    test_sealed_function_is_not_mutated();
    test_plan_is_bound_to_current_scalar_facts();
    test_draft_entry_reports_generic_fallback_without_rewriting_ir();
    return 0;
}
