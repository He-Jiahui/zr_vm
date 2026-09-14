#include "zr_vm_parser/exec_ir_aggregate_layout.h"

#include <assert.h>
#include <string.h>

static void make_private_pair(SZrExecIrAggregateFacts *facts) {
    ZrParser_ExecIr_AggregateFactsInit(facts);
    facts->logicalTypeToken = 17u;
    facts->logicalLayoutId = 23u;
    facts->logicalLayoutHash = UINT64_C(0x1111222233334444);
    facts->generation = 9u;
    facts->sourceId = 41u;
    facts->functionToken = 42u;
    facts->blockId = 3u;
    facts->instructionId = 8u;
    facts->privateClosedWorld = ZR_TRUE;
    facts->closedLifetime = ZR_TRUE;
    facts->reconstructibleIdentity = ZR_TRUE;
    facts->noUnknownEscape = ZR_TRUE;
    facts->noAddressObservation = ZR_TRUE;
    facts->fieldCount = 2u;

    facts->fields[0].fieldId = 100u;
    facts->fields[0].logicalIndex = 0u;
    facts->fields[0].typeToken = 31u;
    facts->fields[0].layoutId = 301u;
    facts->fields[0].logicalOffset = 0u;
    facts->fields[0].byteSize = 8u;
    facts->fields[0].byteAlign = 8u;
    facts->fields[0].useCount = 20u;
    facts->fields[0].loopUseCount = 20u;
    facts->fields[0].initialized = ZR_TRUE;
    facts->fields[0].ownership = ZR_EXEC_IR_OWNERSHIP_BORROWED;
    facts->fields[0].dropOrder = 0u;

    facts->fields[1] = facts->fields[0];
    facts->fields[1].fieldId = 101u;
    facts->fields[1].logicalIndex = 1u;
    facts->fields[1].typeToken = 32u;
    facts->fields[1].layoutId = 302u;
    facts->fields[1].logicalOffset = 8u;
    facts->fields[1].dropOrder = 1u;
    facts->fields[1].useCount = 10u;
    facts->fields[1].loopUseCount = 10u;
}

static void test_private_pair_scalarizes_with_initialization_bits(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrSroaCandidate candidate;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);

    assert(ZrParser_ExecIr_AggregateFactsValidate(&facts, &diagnostic));
    assert(ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(ZrParser_ExecIr_SroaBuildCandidate(&facts, &candidate, &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SROA);
    assert(candidate.scalarizedFieldCount == 2u);
    assert(candidate.unboxedFieldCount == 2u);
    assert((candidate.unboxedMask[0] & 3u) == 3u);
    assert((candidate.initializedMask[0] & 3u) == 3u);
    assert(ZrParser_ExecIr_SroaCandidateValidate(&facts, &candidate, &diagnostic));
    candidate.planHash ^= 1u;
    assert(!ZrParser_ExecIr_SroaCandidateValidate(&facts, &candidate,
                                                  &diagnostic));
    assert(diagnostic.execution.code == ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
}

static void test_sroa_rejects_union_and_uninitialized_read(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);

    facts.flags |= ZR_EXEC_IR_AGGREGATE_FLAG_UNION_OR_OVERLAY;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY);
    assert(diagnostic.fieldIndex == ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);

    make_private_pair(&facts);
    facts.fields[1].initialized = ZR_FALSE;
    assert(ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    {
        SZrExecIrSroaCandidate candidate;
        assert(ZrParser_ExecIr_SroaBuildCandidate(&facts, &candidate, &diagnostic));
        assert((candidate.initializedMask[0] & 1u) == 1u);
        assert((candidate.initializedMask[0] & 2u) == 0u);
    }

    make_private_pair(&facts);
    facts.fields[1].initialized = ZR_FALSE;
    facts.fields[1].readBeforeInit = ZR_TRUE;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ);
    assert(diagnostic.fieldIndex == 1u);
}

static void test_visibility_escape_and_ownership_block_transforms(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);

    facts.noAddressObservation = ZR_FALSE;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED);
    assert(diagnostic.execution.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    facts.noAddressObservation = ZR_TRUE;

    facts.noUnknownEscape = ZR_FALSE;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_UNKNOWN_ESCAPE);
    facts.noUnknownEscape = ZR_TRUE;

    facts.privateClosedWorld = ZR_FALSE;
    facts.publicLayout = ZR_TRUE;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_PUBLIC_LAYOUT);
    facts.privateClosedWorld = ZR_TRUE;
    facts.publicLayout = ZR_FALSE;

    facts.fields[1].flags |= ZR_EXEC_IR_AGGREGATE_FIELD_FFI_VISIBLE;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE);
    facts.fields[1].flags &=
            ~(TZrUInt32)ZR_EXEC_IR_AGGREGATE_FIELD_FFI_VISIBLE;

    facts.fields[1].flags |= ZR_EXEC_IR_AGGREGATE_FIELD_NATIVE_RETAINED;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED);
    facts.fields[1].flags &=
            ~(TZrUInt32)ZR_EXEC_IR_AGGREGATE_FIELD_NATIVE_RETAINED;

    facts.fields[0].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    facts.fields[0].ownershipTransferProven = ZR_FALSE;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE);

    facts.fields[0].ownership = ZR_EXEC_IR_OWNERSHIP_UNKNOWN;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE);
}

static SZrExecIrDataLayoutCost make_profitable_cost(void) {
    SZrExecIrDataLayoutCost cost;
    ZrParser_ExecIr_DataLayoutCostInit(&cost);
    cost.elementCount = 1024u;
    cost.loopTripCount = 200u;
    cost.estimatedMoveBytes = 128u;
    cost.estimatedBridgeCost = 12u;
    cost.estimatedLocalityBenefit = 256u;
    cost.profileAvailable = ZR_TRUE;
    cost.evidence = ZR_EXEC_IR_AGGREGATE_EVIDENCE_ESTIMATED;
    return cost;
}

static void test_closed_private_collection_chooses_soa_only_when_profitable(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrDataLayoutCandidate candidate;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    cost = make_profitable_cost();

    assert(ZrParser_ExecIr_DataLayoutCanUseSoA(&facts, &cost, &diagnostic));
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA);
    assert(candidate.fieldOrder[0] == 0u);
    assert(candidate.fieldOrder[1] == 1u);
    assert(candidate.columnOffset[0] == 0u);
    assert(candidate.columnOffset[1] == 8u);
    assert(candidate.evidence == ZR_EXEC_IR_AGGREGATE_EVIDENCE_ESTIMATED);
    assert(candidate.estimatedBridgeCost == 12u);
    assert(candidate.measuredCacheMissesBefore ==
           ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT);

    cost.estimatedLocalityBenefit = 1u;
    assert(!ZrParser_ExecIr_DataLayoutCanUseSoA(&facts, &cost, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_BRIDGE_TOO_EXPENSIVE);
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS);
    assert(candidate.fieldOrder[0] == 0u);
    assert(candidate.fieldOrder[1] == 1u);
    assert(ZrParser_ExecIr_DataLayoutCandidateValidate(&facts, &candidate,
                                                        &diagnostic));

    cost.estimatedBridgeCost = 0u;
    assert(!ZrParser_ExecIr_DataLayoutCanUseSoA(&facts, &cost, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_COST_UNPROFITABLE);

    cost = make_profitable_cost();
    facts.closedLifetime = ZR_FALSE;
    assert(!ZrParser_ExecIr_DataLayoutCanUseSoA(&facts, &cost, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_LIFETIME_OPEN);
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS);
    assert(ZrParser_ExecIr_DataLayoutCandidateValidate(&facts, &candidate,
                                                        &diagnostic));
}

static void test_measured_evidence_is_kept_separate(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrDataLayoutCandidate candidate;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    cost = make_profitable_cost();
    cost.evidence = ZR_EXEC_IR_AGGREGATE_EVIDENCE_MEASURED;
    cost.measuredCacheMissesBefore = 1000u;
    cost.measuredCacheMissesAfter = 600u;
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.evidence == ZR_EXEC_IR_AGGREGATE_EVIDENCE_MEASURED);
    assert(candidate.measuredCacheMissesBefore == 1000u);
    assert(candidate.measuredCacheMissesAfter == 600u);
    assert(candidate.estimatedLocalityBenefit == 256u);
}

static void test_boundary_materialization_is_explicit(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrSroaCandidate candidate;
    SZrExecIrLayoutTransformPlan plan;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    facts.flags = ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY;
    assert(ZrParser_ExecIr_SroaBuildCandidate(&facts, &candidate, &diagnostic));
    assert(candidate.requiresMaterialization == ZR_TRUE);
    assert(ZrParser_ExecIr_SroaCandidateValidate(&facts, &candidate, &diagnostic));
    assert(ZrParser_ExecIr_PlanAggregateLayout(&facts, ZR_NULL, &plan,
                                                &diagnostic));
    assert(plan.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SROA);
    assert(plan.requiresMaterialization == ZR_TRUE);
    assert(ZrParser_ExecIr_ApplyAggregateLayout(&facts, &plan, &diagnostic));

    /* Debug inspection is also a logical-layout boundary; it must not be
     * silently treated as an ordinary scalar-only execution path. */
    make_private_pair(&facts);
    facts.flags = ZR_EXEC_IR_AGGREGATE_FLAG_DEBUG_BOUNDARY;
    assert(ZrParser_ExecIr_SroaBuildCandidate(&facts, &candidate, &diagnostic));
    assert(candidate.requiresMaterialization == ZR_TRUE);
    assert(ZrParser_ExecIr_SroaCandidateValidate(&facts, &candidate, &diagnostic));
    assert(ZrParser_ExecIr_PlanAggregateLayout(&facts, ZR_NULL, &plan,
                                                &diagnostic));
    assert(plan.requiresMaterialization == ZR_TRUE);
    assert(ZrParser_ExecIr_ApplyAggregateLayout(&facts, &plan, &diagnostic));
}

static void test_alias_identity_stays_logical_and_not_unboxed(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrSroaCandidate sroa;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrDataLayoutCandidate soa;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    facts.identityToken = UINT64_C(0xabc);
    facts.flags = ZR_EXEC_IR_AGGREGATE_FLAG_IDENTITY_OBSERVED |
                  ZR_EXEC_IR_AGGREGATE_FLAG_ALIAS_OBSERVED;
    facts.aliasProven = ZR_TRUE;
    facts.fields[0].flags = ZR_EXEC_IR_AGGREGATE_FIELD_IDENTITY_BEARING |
                            ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED;
    facts.fields[1].flags = ZR_EXEC_IR_AGGREGATE_FIELD_IDENTITY_BEARING |
                            ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED;
    facts.fields[0].aliasClass = UINT64_C(55);
    facts.fields[1].aliasClass = UINT64_C(55);
    facts.fields[0].aliasProven = ZR_TRUE;
    facts.fields[1].aliasProven = ZR_TRUE;
    facts.fields[0].aliasLocation.baseKind = ZR_EXEC_IR_ALIAS_BASE_ALLOCATION;
    facts.fields[1].aliasLocation.baseKind = ZR_EXEC_IR_ALIAS_BASE_ALLOCATION;
    facts.fields[0].aliasLocation.baseId = 100u;
    facts.fields[1].aliasLocation.baseId = 100u;
    facts.fields[0].aliasLocation.projectionId = 1u;
    facts.fields[1].aliasLocation.projectionId = 2u;
    facts.fields[0].aliasLocation.layoutId = facts.logicalLayoutId;
    facts.fields[1].aliasLocation.layoutId = facts.logicalLayoutId;
    facts.fields[0].aliasLocation.generation = facts.generation;
    facts.fields[1].aliasLocation.generation = facts.generation;
    facts.fields[0].aliasLocation.hasStableBase = ZR_TRUE;
    facts.fields[1].aliasLocation.hasStableBase = ZR_TRUE;
    facts.fields[0].aliasLocation.projectionDisjoint = ZR_TRUE;
    facts.fields[1].aliasLocation.projectionDisjoint = ZR_TRUE;
    assert(ZrParser_ExecIr_SroaBuildCandidate(&facts, &sroa, &diagnostic));
    assert(sroa.unboxedFieldCount == 0u);
    assert(ZrParser_ExecIr_SroaCandidateValidate(&facts, &sroa, &diagnostic));
    cost = make_profitable_cost();
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &soa,
                                                    &diagnostic));
    assert(soa.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA);
    assert(ZrParser_ExecIr_DataLayoutCandidateValidate(&facts, &soa,
                                                        &diagnostic));

    /* Distinct projection ids without a disjointness witness are only
     * MAY_ALIAS; neither scalarisation nor column splitting may guess that
     * they do not overlap. */
    facts.fields[1].aliasLocation.projectionDisjoint = ZR_FALSE;
    assert(!ZrParser_ExecIr_SroaCanScalarize(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN);
    assert(diagnostic.fieldIndex == 1u);
    assert(!ZrParser_ExecIr_DataLayoutCanUseSoA(&facts, &cost, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN);
}

static void test_logical_field_order_is_independent_of_offsets(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrDataLayoutCandidate candidate;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    facts.fields[0].logicalOffset = 16u;
    facts.fields[1].logicalOffset = 0u;
    cost = make_profitable_cost();
    facts.fields[0].loopUseCount = 1u;
    facts.fields[1].loopUseCount = 30u;
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA);
    assert(candidate.fieldOrder[0] == 1u);
    assert(candidate.fieldOrder[1] == 0u);
    assert(candidate.columnOffset[0] == 0u);
    assert(candidate.columnOffset[1] == 8u);
    assert(ZrParser_ExecIr_DataLayoutCandidateValidate(&facts, &candidate,
                                                        &diagnostic));

    cost.estimatedLocalityBenefit = 1u;
    cost.estimatedBridgeCost = 0u;
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS);
    assert(candidate.fieldOrder[0] == 0u);
    assert(candidate.fieldOrder[1] == 1u);
    assert(ZrParser_ExecIr_DataLayoutCandidateValidate(&facts, &candidate,
                                                        &diagnostic));
}

static void test_materialization_preserves_identity_alias_and_metadata(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrDataLayoutCandidate candidate;
    SZrExecIrMaterializationMap map;
    SZrExecIrMaterializationEntry entry;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    facts.fields[0].flags |= ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT |
                             ZR_EXEC_IR_AGGREGATE_FIELD_IDENTITY_BEARING;
    facts.fields[0].ownership = ZR_EXEC_IR_OWNERSHIP_SHARED;
    facts.fields[0].ownershipTransferProven = ZR_TRUE;
    facts.dropOrderProven = ZR_TRUE;
    facts.fields[0].rootSlot = 7u;
    facts.identityToken = UINT64_C(0xabc);
    facts.flags |= ZR_EXEC_IR_AGGREGATE_FLAG_DEBUG_BOUNDARY;
    cost = make_profitable_cost();
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));

    ZrParser_ExecIr_MaterializationMapInit(&map);
    assert(ZrParser_ExecIr_MaterializationMapBegin(&map, &facts, &candidate,
                                                   UINT64_C(0xabc),
                                                   &diagnostic));
    assert(ZrParser_ExecIr_MaterializationMapAddField(&map, &facts.fields[0],
                                                      0u, 0u, &diagnostic));
    assert(ZrParser_ExecIr_MaterializationMapAddField(&map, &facts.fields[1],
                                                      1u, 8u, &diagnostic));
    assert(ZrParser_ExecIr_MaterializationMapFinalize(&map, &diagnostic));
    assert(map.identityToken == UINT64_C(0xabc));
    assert(map.flags == facts.flags);
    assert(map.entryCount == 2u);
    assert(map.entries[0].rootSlot == 7u);
    assert(map.entries[0].ownership == ZR_EXEC_IR_OWNERSHIP_SHARED);
    assert(ZrParser_ExecIr_MaterializationMapResolve(&map, 1u, &entry,
                                                     &diagnostic));
    assert(entry.physicalIndex == 1u);
    assert(entry.physicalOffset == candidate.columnOffset[1]);
    assert(ZrParser_ExecIr_MaterializationMapValidate(&map, &facts, &candidate,
                                                      &diagnostic));

    /* Entry construction order is not part of the logical contract.  Finalize
     * canonicalizes by logical index so equivalent maps have a stable hash. */
    {
        SZrExecIrMaterializationMap reverse;
        ZrParser_ExecIr_MaterializationMapInit(&reverse);
        assert(ZrParser_ExecIr_MaterializationMapBegin(
                &reverse, &facts, &candidate, UINT64_C(0xabc), &diagnostic));
        assert(ZrParser_ExecIr_MaterializationMapAddField(
                &reverse, &facts.fields[1], 1u, candidate.columnOffset[1],
                &diagnostic));
        assert(ZrParser_ExecIr_MaterializationMapAddField(
                &reverse, &facts.fields[0], 0u, candidate.columnOffset[0],
                &diagnostic));
        assert(ZrParser_ExecIr_MaterializationMapFinalize(&reverse,
                                                          &diagnostic));
        assert(reverse.entries[0].logicalIndex == 0u);
        assert(reverse.entries[1].logicalIndex == 1u);
        assert(reverse.mapHash == map.mapHash);
    }

    {
        SZrExecIrMaterializationMap rejected;
        assert(!ZrParser_ExecIr_MaterializationMapBegin(
                &rejected, &facts, &candidate, 0u, &diagnostic));
        assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH);
    }

    map.flags |= (TZrUInt32)1u << 31u;
    assert(!ZrParser_ExecIr_MaterializationMapValidate(&map, &facts, &candidate,
                                                       &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS);
    map.flags = facts.flags;
    map.mapHash = ZrParser_ExecIr_MaterializationMapHash(&map);

    map.identityToken = UINT64_C(0xdef);
    assert(!ZrParser_ExecIr_MaterializationMapValidate(&map, &facts, &candidate,
                                                      &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH);

    map.identityToken = UINT64_C(0xabc);
    map.mapHash = ZrParser_ExecIr_MaterializationMapHash(&map);
    map.entries[1].physicalOffset++;
    assert(!ZrParser_ExecIr_MaterializationMapValidate(&map, &facts, &candidate,
                                                       &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID);
}

static void test_transform_plan_falls_back_without_proof(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrLayoutTransformPlan plan;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    facts.noUnknownEscape = ZR_FALSE;
    cost = make_profitable_cost();
    assert(ZrParser_ExecIr_PlanAggregateLayout(&facts, &cost, &plan, &diagnostic));
    assert(plan.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_GENERIC);
    assert(plan.fallbackReason == ZR_EXEC_IR_AGGREGATE_UNKNOWN_ESCAPE);
    assert(ZrParser_ExecIr_ApplyAggregateLayout(&facts, &plan, &diagnostic));
}

static void test_incomplete_semantic_proofs_remain_describable_as_fallback(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrDataLayoutCandidate candidate;
    SZrExecIrLayoutTransformPlan plan;
    SZrExecIrAggregateDiagnostic diagnostic;

    /* An observed alias without a proof is not a malformed record: the
     * optimiser must leave it in ordinary AoS/generic form rather than make
     * callers erase useful diagnostics just to describe a safe fallback. */
    make_private_pair(&facts);
    cost = make_profitable_cost();
    facts.flags |= ZR_EXEC_IR_AGGREGATE_FLAG_ALIAS_OBSERVED;
    assert(ZrParser_ExecIr_AggregateFactsValidateStructural(&facts,
                                                             &diagnostic));
    assert(!ZrParser_ExecIr_AggregateFactsValidate(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN);
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS);
    assert(candidate.fallbackReason == ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN);
    assert(ZrParser_ExecIr_DataLayoutCandidateValidate(&facts, &candidate,
                                                        &diagnostic));
    assert(ZrParser_ExecIr_PlanAggregateLayout(&facts, &cost, &plan,
                                                &diagnostic));
    assert(plan.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_GENERIC);
    assert(plan.fallbackReason == ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN);
    assert(ZrParser_ExecIr_ApplyAggregateLayout(&facts, &plan, &diagnostic));

    /* Ownership-transfer absence is likewise a transform blocker, not an
     * excuse to fabricate a replacement layout. */
    make_private_pair(&facts);
    facts.fields[0].ownership = ZR_EXEC_IR_OWNERSHIP_UNIQUE;
    assert(ZrParser_ExecIr_AggregateFactsValidateStructural(&facts,
                                                             &diagnostic));
    assert(!ZrParser_ExecIr_AggregateFactsValidate(&facts, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE);
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS);
    assert(candidate.fallbackReason == ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE);
    assert(ZrParser_ExecIr_DataLayoutCandidateValidate(&facts, &candidate,
                                                        &diagnostic));
}

static void test_apply_rechecks_soa_field_semantics(void) {
    SZrExecIrAggregateFacts facts;
    SZrExecIrDataLayoutCost cost;
    SZrExecIrDataLayoutCandidate candidate;
    SZrExecIrLayoutTransformPlan plan;
    SZrExecIrAggregateDiagnostic diagnostic;
    make_private_pair(&facts);
    cost = make_profitable_cost();
    assert(ZrParser_ExecIr_DataLayoutBuildCandidate(&facts, &cost, &candidate,
                                                    &diagnostic));
    assert(candidate.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA);
    /* Obtain a valid plan through the public planner before mutating the
     * source facts; the apply boundary must re-check field semantics. */
    assert(ZrParser_ExecIr_PlanAggregateLayout(&facts, &cost, &plan,
                                                &diagnostic));
    assert(plan.strategy == ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA);
    facts.fields[1].ownership = ZR_EXEC_IR_OWNERSHIP_UNKNOWN;
    assert(!ZrParser_ExecIr_ApplyAggregateLayout(&facts, &plan, &diagnostic));
    assert(diagnostic.status == ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE);
    assert(diagnostic.fieldIndex == 1u);
}

int main(void) {
    test_private_pair_scalarizes_with_initialization_bits();
    test_sroa_rejects_union_and_uninitialized_read();
    test_visibility_escape_and_ownership_block_transforms();
    test_closed_private_collection_chooses_soa_only_when_profitable();
    test_measured_evidence_is_kept_separate();
    test_boundary_materialization_is_explicit();
    test_alias_identity_stays_logical_and_not_unboxed();
    test_logical_field_order_is_independent_of_offsets();
    test_materialization_preserves_identity_alias_and_metadata();
    test_transform_plan_falls_back_without_proof();
    test_incomplete_semantic_proofs_remain_describable_as_fallback();
    test_apply_rechecks_soa_field_semantics();
    return 0;
}
