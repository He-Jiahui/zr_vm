#include "zr_vm_parser/optimization_facts.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_facts_are_conservative(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.requiredFacts = ZR_OPTIMIZATION_FACT_PURITY |
                          ZR_OPTIMIZATION_FACT_NO_ALLOC;
    query.observedFactMask = ZR_OPTIMIZATION_FACT_PURITY;
    query.observedEffects = ZR_EXECUTION_EFFECT_READ_MEMORY;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED);

    query.observedFactMask |= ZR_OPTIMIZATION_FACT_NO_ALLOC;
    query.forbiddenEffects = ZR_EXECUTION_EFFECT_WRITE_MEMORY;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_PROVEN);
    assert(facts.proofHash != 0u);
    assert(ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
}

static void test_readonly_borrow_and_task_facts_are_inferred(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.requiredFacts = ZR_OPTIMIZATION_FACT_RECEIVER_READONLY |
                          ZR_OPTIMIZATION_FACT_BORROW_SAFE |
                          ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC;
    query.observedReceiverEffect = ZR_OPTIMIZATION_RECEIVER_READONLY;
    query.observedBorrowState = ZR_OPTIMIZATION_BORROW_STABLE;
    query.observedTaskEffectsKnown = ZR_TRUE;
    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_NONE;
    /* Send/Sync is a type/ownership proof, not a synonym for an empty task
     * effect set; publish it explicitly alongside the task proof. */
    query.observedFactMask = ZR_OPTIMIZATION_FACT_NO_SUSPEND |
                             ZR_OPTIMIZATION_FACT_SEND_SYNC;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_PROVEN);
    assert((facts.factMask & ZR_OPTIMIZATION_FACT_RECEIVER_READONLY) != 0u);
    assert((facts.factMask & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u);
    assert((facts.factMask & ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC) != 0u);
    assert((facts.factMask & ZR_OPTIMIZATION_FACT_SEND_SYNC) != 0u);

    /* Canonical type capability bits are sufficient evidence for the
     * type-level Send/Sync proof; no source type spelling is consulted. */
    query.requiredFacts = ZR_OPTIMIZATION_FACT_SEND_SYNC;
    query.observedFactMask = 0u;
    query.observedOwnershipMask =
            ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND_SYNC;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_PROVEN);
    assert((facts.factMask & ZR_OPTIMIZATION_FACT_SEND_SYNC) != 0u);

    query.observedOwnershipMask =
            ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND_SYNC |
            ((TZrUInt32)1u << 31u);
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_OWNERSHIP);

    query.observedOwnershipMask = 0u;
    query.requiredFacts = ZR_OPTIMIZATION_FACT_RECEIVER_READONLY |
                          ZR_OPTIMIZATION_FACT_BORROW_SAFE |
                          ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC;
    query.observedFactMask = ZR_OPTIMIZATION_FACT_NO_SUSPEND |
                             ZR_OPTIMIZATION_FACT_SEND_SYNC;
    query.observedEffects = 0u;
    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_NONE;
    query.observedReceiverEffect = ZR_OPTIMIZATION_RECEIVER_MUTABLE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_RECEIVER);

    query.observedReceiverEffect = ZR_OPTIMIZATION_RECEIVER_READONLY;
    query.observedBorrowState = ZR_OPTIMIZATION_BORROW_ESCAPES;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_BORROW);

    query.observedBorrowState = ZR_OPTIMIZATION_BORROW_STABLE;
    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);
}

static void test_expected_receiver_effect_is_exact(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.expectedReceiverEffect = ZR_OPTIMIZATION_RECEIVER_MUTABLE;
    query.observedReceiverEffect = ZR_OPTIMIZATION_RECEIVER_READONLY;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_RECEIVER);
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH);
    assert(diagnostic.expectedHash == ZR_OPTIMIZATION_RECEIVER_MUTABLE);
    assert(diagnostic.actualHash == ZR_OPTIMIZATION_RECEIVER_READONLY);

    query.observedReceiverEffect = ZR_OPTIMIZATION_RECEIVER_MUTABLE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_PROVEN);
}

static void test_invalid_borrow_across_suspend_is_language_error(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    memset(&diagnostic, 0, sizeof(diagnostic));
    ZrParser_OptimizationFactQuery_Init(&query);
    query.observedBorrowState = ZR_OPTIMIZATION_BORROW_ACROSS_SUSPEND;
    query.functionToken = 17u;
    query.blockId = 3u;
    query.instructionId = 9u;
    query.sourceId = 41u;
    assert(!ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND);
    assert(diagnostic.functionToken == query.functionToken);
    assert(diagnostic.blockId == query.blockId);
    assert(diagnostic.instructionId == query.instructionId);
    assert(diagnostic.sourceId == query.sourceId);
    assert(facts.proofFunctionToken == query.functionToken);
    assert(facts.proofBlockId == query.blockId);
    assert(facts.proofInstructionId == query.instructionId);
    assert(facts.proofSourceId == query.sourceId);
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE);
}

static void test_query_downgrades_explicit_borrow_contradictions(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.requiredFacts = ZR_OPTIMIZATION_FACT_BORROW_SAFE;
    query.observedFactMask = ZR_OPTIMIZATION_FACT_BORROW_SAFE;
    query.observedBorrowState = ZR_OPTIMIZATION_BORROW_STABLE;
    query.observedEffects = ZR_EXECUTION_EFFECT_SUSPEND;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_BORROW);

    query.observedEffects = 0u;
    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);

    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_BORROW_ESCAPE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);

    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);
}

static void test_unknown_external_and_task_effects_never_become_pure(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.requiredFacts = ZR_OPTIMIZATION_FACT_PURITY;
    query.observedFactMask = ZR_OPTIMIZATION_FACT_PURITY;
    query.observedExternalEffectsUnknown = ZR_TRUE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS);

    query.observedExternalEffectsUnknown = ZR_FALSE;
    query.observedExternalEffectsKnown = ZR_FALSE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS);

    query.observedExternalEffectsKnown = ZR_TRUE;
    query.observedTaskEffectsUnknown = ZR_TRUE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);

    query.observedTaskEffectsUnknown = ZR_FALSE;
    query.observedTaskEffectsKnown = ZR_FALSE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);

    query.observedTaskEffectsKnown = ZR_TRUE;
    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);

    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_SPAWN;
    query.observedTaskEffectsUnknown = ZR_FALSE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);

    query.observedTaskEffects = ZR_OPTIMIZATION_TASK_EFFECT_NONE;
    query.requiredFacts = ZR_OPTIMIZATION_FACT_NO_ALLOC;
    query.observedFactMask = ZR_OPTIMIZATION_FACT_NO_ALLOC;
    query.observedEffects = ZR_EXECUTION_EFFECT_ALLOCATE;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS);

    query.requiredFacts = ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC;
    query.observedFactMask = ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC;
    query.observedEffects = ZR_EXECUTION_EFFECT_SUSPEND;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT);
}

static void test_protocol_identity_is_capability_based(void) {
    SZrOptimizationProtocol provided;
    SZrOptimizationProtocol required;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationProtocol_Init(&provided);
    ZrParser_OptimizationProtocol_Init(&required);
    provided.protocolToken = required.protocolToken = 42u;
    provided.kind = required.kind = ZR_OPTIMIZATION_PROTOCOL_CONTIGUOUS_VIEW;
    provided.operationMask = required.operationMask = ZR_OPTIMIZATION_PROTOCOL_OP_LOAD;
    provided.requiredFacts = required.requiredFacts = ZR_OPTIMIZATION_FACT_CONTIGUOUS;
    provided.declaredEffects = required.declaredEffects = ZR_EXECUTION_EFFECT_READ_MEMORY;
    provided.signatureHash = required.signatureHash = UINT64_C(0x1234);
    provided.layoutId = required.layoutId = 7u;
    provided.layoutHash = required.layoutHash = UINT64_C(0x77);
    /* Element tokens are descriptive; two unrelated source type names can
     * implement the same stable protocol contract. */
    provided.elementTypeToken = 10u;
    required.elementTypeToken = 99u;
    assert(ZrParser_Optimization_RecognizeProtocol(&provided, &required,
                                                   &diagnostic));

    provided.layoutHash = UINT64_C(0x88);
    assert(!ZrParser_Optimization_RecognizeProtocol(&provided, &required,
                                                    &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);

    provided.layoutHash = required.layoutHash;
    provided.operationMask = ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE;
    required.operationMask = ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE;
    assert(ZrParser_OptimizationProtocol_Validate(&provided, &diagnostic));
}

static void test_iteration_and_persistent_protocols(void) {
    SZrOptimizationProtocol iteration;
    SZrOptimizationProtocol iterationRequired;
    SZrOptimizationProtocol persistent;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationProtocol_Init(&iteration);
    iteration.protocolToken = 100u;
    iteration.kind = ZR_OPTIMIZATION_PROTOCOL_ITERATION;
    iteration.operationMask = ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE;
    iteration.requiredFacts = ZR_OPTIMIZATION_FACT_IMMUTABLE;
    iteration.signatureHash = UINT64_C(0x1000);
    iterationRequired = iteration;
    iterationRequired.operationMask |= ZR_OPTIMIZATION_PROTOCOL_OP_LOAD;
    /* A consumer may ask for a stronger operation set than the producer has;
     * this must remain a capability miss, never a type-name fallback. */
    assert(!ZrParser_Optimization_RecognizeProtocol(&iteration,
                                                    &iterationRequired,
                                                    &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH);

    iterationRequired.operationMask = ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE;
    assert(ZrParser_Optimization_RecognizeProtocol(&iteration,
                                                   &iterationRequired,
                                                   &diagnostic));

    ZrParser_OptimizationProtocol_Init(&persistent);
    persistent.protocolToken = 200u;
    persistent.kind = ZR_OPTIMIZATION_PROTOCOL_PERSISTENT_UPDATE;
    persistent.operationMask = ZR_OPTIMIZATION_PROTOCOL_OP_UPDATE;
    persistent.requiredFacts = ZR_OPTIMIZATION_FACT_PERSISTENT |
                               ZR_OPTIMIZATION_FACT_IMMUTABLE;
    persistent.signatureHash = UINT64_C(0x2000);
    assert(ZrParser_OptimizationProtocol_Validate(&persistent, &diagnostic));

    {
        SZrOptimizationFactQuery query;
        SZrOptimizationFacts facts;

        ZrParser_OptimizationFactQuery_Init(&query);
        query.requiredFacts = ZR_OPTIMIZATION_FACT_IMMUTABLE |
                              ZR_OPTIMIZATION_FACT_PERSISTENT;
        query.observedFactMask = ZR_OPTIMIZATION_FACT_IMMUTABLE;
        assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
        assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
        assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_IMMUTABILITY);

        query.observedFactMask |= ZR_OPTIMIZATION_FACT_PERSISTENT;
        query.observedProtocolKind = ZR_OPTIMIZATION_PROTOCOL_ITERATION;
        query.observedProtocolOperations = ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE;
        query.expectedProtocolKind = ZR_OPTIMIZATION_PROTOCOL_ITERATION;
        query.expectedProtocolOperations = ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE;
        assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
        assert(facts.validity == ZR_OPTIMIZATION_FACTS_PROVEN);
    }
}

static void test_hash_witness_and_generation_diagnostics(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 originalHash;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.observedFactMask = ZR_OPTIMIZATION_FACT_NO_ALLOC;
    query.observedGeneration = UINT64_C(4);
    query.expectedGeneration = UINT64_C(5);
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_STALE_GENERATION);
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION);
    originalHash = facts.proofHash;

    facts.proofHash ^= UINT64_C(1);
    assert(!ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    facts.proofHash = originalHash;
    assert(ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
}

static void test_type_identity_diagnostic_is_not_a_signature_guess(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.expectedTypeToken = 71u;
    query.observedTypeToken = 72u;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(facts.validity == ZR_OPTIMIZATION_FACTS_UNKNOWN);
    assert(facts.unknownReason == ZR_OPTIMIZATION_UNKNOWN_PROTOCOL);
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    assert(diagnostic.expectedHash == 71u);
    assert(diagnostic.actualHash == 72u);
}

static void test_malformed_masks_are_rejected_without_type_fallback(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;
    SZrOptimizationFacts before;
    SZrOptimizationProtocol protocol;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFactQuery_Init(&query);
    ZrParser_OptimizationFacts_Init(&facts);
    facts.typeToken = 77u;
    before = facts;
    query.requiredFacts = (TZrUInt32)1u << 31u;
    assert(!ZrParser_Optimization_QueryFacts(&query, &facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(memcmp(&facts, &before, sizeof(facts)) == 0);

    ZrParser_OptimizationProtocol_Init(&protocol);
    protocol.protocolToken = 1u;
    protocol.kind = ZR_OPTIMIZATION_PROTOCOL_ITERATION;
    protocol.operationMask = (TZrUInt32)1u << 31u;
    assert(!ZrParser_OptimizationProtocol_Validate(&protocol, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);

    protocol.operationMask = ZR_OPTIMIZATION_PROTOCOL_OP_STORE;
    assert(!ZrParser_OptimizationProtocol_Validate(&protocol, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
}

static void test_proven_contradictions_are_rejected_but_unknown_is_inspectable(void) {
    SZrOptimizationFacts facts;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_OptimizationFacts_Init(&facts);
    facts.validity = ZR_OPTIMIZATION_FACTS_PROVEN;
    facts.unknownReason = ZR_OPTIMIZATION_UNKNOWN_NONE;
    facts.factMask = ZR_OPTIMIZATION_FACT_PURITY |
                     ZR_OPTIMIZATION_FACT_NO_ALLOC;
    facts.declaredEffects = ZR_EXECUTION_EFFECT_WRITE_MEMORY |
                            ZR_EXECUTION_EFFECT_ALLOCATE;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(!ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH);

    facts.validity = ZR_OPTIMIZATION_FACTS_UNKNOWN;
    facts.unknownReason = ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));

    facts.validity = ZR_OPTIMIZATION_FACTS_PROVEN;
    facts.unknownReason = ZR_OPTIMIZATION_UNKNOWN_NONE;
    facts.factMask = ZR_OPTIMIZATION_FACT_SEND_SYNC;
    facts.declaredEffects = ZR_EXECUTION_EFFECT_READ_MEMORY;
    facts.taskEffects = ZR_OPTIMIZATION_TASK_EFFECT_SPAWN;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    /* SEND_SYNC is a type/ownership proof and is independent of task
     * scheduling effects; the record remains valid when both are published. */
    assert(ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));

    facts.ownershipMask = ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND_SYNC |
                          ((TZrUInt32)1u << 31u);
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(!ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH);
    assert(diagnostic.expectedHash ==
           ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_KNOWN_MASK);
    assert(diagnostic.actualHash == facts.ownershipMask);

    facts.validity = ZR_OPTIMIZATION_FACTS_UNKNOWN;
    facts.unknownReason = ZR_OPTIMIZATION_UNKNOWN_OWNERSHIP;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));

    facts.ownershipMask = 0u;
    facts.validity = ZR_OPTIMIZATION_FACTS_PROVEN;
    facts.unknownReason = ZR_OPTIMIZATION_UNKNOWN_NONE;
    facts.factMask = ZR_OPTIMIZATION_FACT_BORROW_SAFE;
    facts.borrowState = ZR_OPTIMIZATION_BORROW_STABLE;
    facts.declaredEffects = ZR_EXECUTION_EFFECT_SUSPEND;
    facts.taskEffects = ZR_OPTIMIZATION_TASK_EFFECT_NONE;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(!ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH);
    assert(diagnostic.expectedHash == ZR_EXECUTION_EFFECT_SUSPEND);
    assert(diagnostic.actualHash == facts.declaredEffects);

    facts.declaredEffects = 0u;
    facts.taskEffects = ZR_OPTIMIZATION_TASK_EFFECT_BORROW_ESCAPE;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(!ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH);
    assert(diagnostic.expectedHash == ZR_OPTIMIZATION_TASK_EFFECT_NONE);
    assert(diagnostic.actualHash == facts.taskEffects);

    facts.taskEffects = ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(!ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH);
    assert(diagnostic.expectedHash == ZR_OPTIMIZATION_TASK_EFFECT_NONE);
    assert(diagnostic.actualHash == facts.taskEffects);

    facts.factMask = ZR_OPTIMIZATION_FACT_RECEIVER_READONLY;
    facts.taskEffects = ZR_OPTIMIZATION_TASK_EFFECT_NONE;
    facts.receiverEffect = ZR_OPTIMIZATION_RECEIVER_UNKNOWN;
    facts.borrowState = ZR_OPTIMIZATION_BORROW_UNKNOWN;
    facts.proofHash = ZrParser_OptimizationFacts_Hash(&facts);
    assert(!ZrParser_OptimizationFacts_Validate(&facts, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH);
}

static void test_diagnostic_is_optional(void) {
    SZrOptimizationFactQuery query;
    SZrOptimizationFacts facts;

    ZrParser_OptimizationFactQuery_Init(&query);
    query.observedFactMask = ZR_OPTIMIZATION_FACT_NO_ALLOC;
    assert(ZrParser_Optimization_QueryFacts(&query, &facts, ZR_NULL));
    assert(ZrParser_OptimizationFacts_Validate(&facts, ZR_NULL));
}

int main(void) {
    test_facts_are_conservative();
    test_readonly_borrow_and_task_facts_are_inferred();
    test_expected_receiver_effect_is_exact();
    test_invalid_borrow_across_suspend_is_language_error();
    test_query_downgrades_explicit_borrow_contradictions();
    test_unknown_external_and_task_effects_never_become_pure();
    test_protocol_identity_is_capability_based();
    test_iteration_and_persistent_protocols();
    test_hash_witness_and_generation_diagnostics();
    test_type_identity_diagnostic_is_not_a_signature_guess();
    test_malformed_masks_are_rejected_without_type_fallback();
    test_proven_contradictions_are_rejected_but_unknown_is_inspectable();
    test_diagnostic_is_optional();
    return 0;
}
