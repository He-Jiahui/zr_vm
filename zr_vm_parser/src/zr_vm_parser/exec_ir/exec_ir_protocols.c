#include "zr_vm_parser/optimization_facts.h"

#include <stdint.h>
#include <string.h>

#define ZR_OPTIMIZATION_HASH_OFFSET UINT64_C(1469598103934665603)
#define ZR_OPTIMIZATION_HASH_PRIME UINT64_C(1099511628211)

static void zr_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 byte;

    if (hash == ZR_NULL) {
        return;
    }
    for (byte = 0u; byte < 4u; ++byte) {
        *hash ^= (TZrUInt64)((value >> (byte * 8u)) & 0xffu);
        *hash *= ZR_OPTIMIZATION_HASH_PRIME;
    }
}

static void zr_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    zr_hash_u32(hash, (TZrUInt32)value);
    zr_hash_u32(hash, (TZrUInt32)(value >> 32u));
}

static void zr_diagnostic_clear(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void zr_diagnostic_set(SZrExecIrDiagnostic *diagnostic,
                              EZrExecutionDiagnosticCode code,
                              TZrUInt64 expected,
                              TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    zr_diagnostic_clear(diagnostic);
    diagnostic->code = code;
    diagnostic->expectedVersion = (TZrUInt32)expected;
    diagnostic->actualVersion = (TZrUInt32)actual;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static void zr_diagnostic_apply_query_location(
        SZrExecIrDiagnostic *diagnostic,
        const SZrOptimizationFactQuery *query) {
    if (diagnostic == ZR_NULL || query == ZR_NULL) {
        return;
    }
    diagnostic->functionToken = query->functionToken;
    diagnostic->blockId = query->blockId;
    diagnostic->instructionId = query->instructionId;
    diagnostic->sourceId = query->sourceId;
}

static TZrBool zr_has_unknown_bits(TZrUInt32 value, TZrUInt32 knownMask) {
    return (TZrBool)((value & ~knownMask) != 0u);
}

static TZrBool zr_is_valid_receiver_effect(TZrUInt32 effect) {
    return (TZrBool)(effect < (TZrUInt32)ZR_OPTIMIZATION_RECEIVER_EFFECT_COUNT);
}

static TZrBool zr_is_valid_borrow_state(TZrUInt32 state) {
    return (TZrBool)(state < (TZrUInt32)ZR_OPTIMIZATION_BORROW_STATE_COUNT);
}

static void zr_facts_mark_unknown(SZrOptimizationFacts *facts,
                                  EZrOptimizationFactUnknownReason reason) {
    if (facts == ZR_NULL) {
        return;
    }
    facts->validity = ZR_OPTIMIZATION_FACTS_UNKNOWN;
    facts->unknownReason = (TZrUInt32)reason;
}

static EZrOptimizationFactUnknownReason zr_missing_fact_reason(
        TZrUInt32 missingFacts) {
    if ((missingFacts & ZR_OPTIMIZATION_FACT_RECEIVER_READONLY) != 0u) {
        return ZR_OPTIMIZATION_UNKNOWN_RECEIVER;
    }
    if ((missingFacts & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u) {
        return ZR_OPTIMIZATION_UNKNOWN_BORROW;
    }
    if ((missingFacts & ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC) != 0u) {
        return ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    }
    if ((missingFacts & (ZR_OPTIMIZATION_FACT_IMMUTABLE |
                         ZR_OPTIMIZATION_FACT_PERSISTENT)) != 0u) {
        return ZR_OPTIMIZATION_UNKNOWN_IMMUTABILITY;
    }
    return ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED;
}

static TZrUInt64 zr_facts_hash_payload(const SZrOptimizationFacts *facts) {
    TZrUInt64 hash = ZR_OPTIMIZATION_HASH_OFFSET;

    if (facts == ZR_NULL) {
        return 0u;
    }

    /* Do not include proofHash: it is the witness over this payload.  Including
     * it would make every valid witness self-invalidating. */
    zr_hash_u32(&hash, facts->schemaVersion);
    zr_hash_u32(&hash, facts->validity);
    zr_hash_u32(&hash, facts->unknownReason);
    zr_hash_u32(&hash, facts->factMask);
    zr_hash_u32(&hash, facts->typeToken);
    zr_hash_u32(&hash, facts->layoutId);
    zr_hash_u32(&hash, facts->ownershipMask);
    zr_hash_u32(&hash, facts->declaredEffects);
    zr_hash_u64(&hash, facts->moduleHash);
    zr_hash_u64(&hash, facts->generation);
    zr_hash_u64(&hash, facts->protocolHash);
    zr_hash_u32(&hash, facts->taskEffects);
    zr_hash_u32(&hash, facts->receiverEffect);
    zr_hash_u32(&hash, facts->borrowState);
    zr_hash_u32(&hash, facts->protocolKind);
    zr_hash_u32(&hash, facts->protocolOperations);
    zr_hash_u32(&hash, facts->proofFunctionToken);
    zr_hash_u32(&hash, facts->proofBlockId);
    zr_hash_u32(&hash, facts->proofInstructionId);
    zr_hash_u32(&hash, facts->proofSourceId);
    return hash;
}

void ZrParser_OptimizationFacts_Init(SZrOptimizationFacts *facts) {
    if (facts == ZR_NULL) {
        return;
    }
    memset(facts, 0, sizeof(*facts));
    facts->schemaVersion = ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION;
    facts->validity = ZR_OPTIMIZATION_FACTS_UNKNOWN;
    facts->unknownReason = ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED;
    facts->receiverEffect = ZR_OPTIMIZATION_RECEIVER_UNKNOWN;
    facts->borrowState = ZR_OPTIMIZATION_BORROW_UNKNOWN;
}

void ZrParser_OptimizationFactQuery_Init(SZrOptimizationFactQuery *query) {
    if (query == ZR_NULL) {
        return;
    }
    memset(query, 0, sizeof(*query));
    query->schemaVersion = ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION;
    /* Ordinary parser/body effects are complete by default.  A native or
     * plugin producer that has no effect contract sets this field back to
     * zero (or sets observedExternalEffectsUnknown) and therefore remains
     * conservative. */
    query->observedExternalEffectsKnown = ZR_TRUE;
    /* The same default applies to task effects produced by the parser's
     * checked async/task analysis.  Native/plugin callers must explicitly
     * clear this bit when they cannot publish a task-effect contract. */
    query->observedTaskEffectsKnown = ZR_TRUE;
    query->observedReceiverEffect = ZR_OPTIMIZATION_RECEIVER_UNKNOWN;
    query->observedBorrowState = ZR_OPTIMIZATION_BORROW_UNKNOWN;
}

void ZrParser_OptimizationProtocol_Init(SZrOptimizationProtocol *protocol) {
    if (protocol == ZR_NULL) {
        return;
    }
    memset(protocol, 0, sizeof(*protocol));
    protocol->schemaVersion = ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION;
}

TZrUInt64 ZrParser_OptimizationFacts_Hash(const SZrOptimizationFacts *facts) {
    return zr_facts_hash_payload(facts);
}

TZrUInt64 ZrParser_OptimizationProtocol_Hash(
        const SZrOptimizationProtocol *protocol) {
    TZrUInt64 hash = ZR_OPTIMIZATION_HASH_OFFSET;

    if (protocol == ZR_NULL) {
        return 0u;
    }
    zr_hash_u32(&hash, protocol->schemaVersion);
    zr_hash_u32(&hash, protocol->protocolToken);
    zr_hash_u32(&hash, protocol->kind);
    zr_hash_u32(&hash, protocol->operationMask);
    zr_hash_u32(&hash, protocol->requiredFacts);
    zr_hash_u32(&hash, protocol->declaredEffects);
    zr_hash_u32(&hash, protocol->elementTypeToken);
    zr_hash_u32(&hash, protocol->layoutId);
    zr_hash_u64(&hash, protocol->signatureHash);
    zr_hash_u64(&hash, protocol->layoutHash);
    zr_hash_u64(&hash, protocol->moduleHash);
    zr_hash_u64(&hash, protocol->generation);
    return hash;
}

TZrBool ZrParser_OptimizationFacts_Validate(
        const SZrOptimizationFacts *facts,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt64 expectedHash;

    zr_diagnostic_clear(diagnostic);
    if (facts == ZR_NULL ||
        facts->schemaVersion != ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION ||
        facts->validity > ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE ||
        facts->unknownReason >= ZR_OPTIMIZATION_UNKNOWN_REASON_COUNT ||
        zr_has_unknown_bits(facts->factMask,
                            ZR_OPTIMIZATION_FACT_KNOWN_MASK) ||
        zr_has_unknown_bits(facts->declaredEffects,
                            ZR_EXECUTION_EFFECT_KNOWN_MASK) ||
        zr_has_unknown_bits(facts->taskEffects,
                            ZR_OPTIMIZATION_TASK_EFFECT_KNOWN_MASK) ||
        !zr_is_valid_receiver_effect(facts->receiverEffect) ||
        !zr_is_valid_borrow_state(facts->borrowState) ||
        facts->protocolKind >= ZR_OPTIMIZATION_PROTOCOL_KIND_COUNT ||
        zr_has_unknown_bits(facts->protocolOperations,
                            ZR_OPTIMIZATION_PROTOCOL_OP_KNOWN_MASK) ||
        (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
         facts->unknownReason != ZR_OPTIMIZATION_UNKNOWN_NONE) ||
        (facts->validity == ZR_OPTIMIZATION_FACTS_UNKNOWN &&
         facts->unknownReason == ZR_OPTIMIZATION_UNKNOWN_NONE) ||
        (facts->validity == ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE &&
         facts->unknownReason != ZR_OPTIMIZATION_UNKNOWN_NONE)) {
        zr_diagnostic_set(diagnostic,
                          ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION,
                          facts == ZR_NULL ? 0u : facts->schemaVersion);
        return ZR_FALSE;
    }

    /* A proven negative effect cannot coexist with the corresponding
     * observed effect.  UNKNOWN records intentionally retain the producer's
     * raw observations (including an explicit UNKNOWN task bit) for remarks;
     * validity prevents those observations from being consumed as proofs.
     * Reject contradictory combinations only when the record claims PROVEN. */
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_NO_ALLOC) != 0u &&
            (facts->declaredEffects & ZR_EXECUTION_EFFECT_ALLOCATE) != 0u) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                          ZR_EXECUTION_EFFECT_ALLOCATE, facts->declaredEffects);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_PURITY) != 0u &&
            ((facts->declaredEffects &
              (ZR_EXECUTION_EFFECT_WRITE_MEMORY |
               ZR_EXECUTION_EFFECT_ALLOCATE |
               ZR_EXECUTION_EFFECT_THROW |
               ZR_EXECUTION_EFFECT_SUSPEND)) != 0u ||
             facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE)) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                          ZR_EXECUTION_EFFECT_READ_MEMORY,
                          facts->declaredEffects | facts->taskEffects);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_NO_THROW) != 0u &&
            (facts->declaredEffects & ZR_EXECUTION_EFFECT_THROW) != 0u) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                          ZR_EXECUTION_EFFECT_THROW, facts->declaredEffects);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_NO_SUSPEND) != 0u &&
            ((facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) != 0u ||
             facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE)) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                          ZR_EXECUTION_EFFECT_SUSPEND, facts->declaredEffects);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->ownershipMask &
             ~ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_KNOWN_MASK) != 0u) {
        /* A proven summary cannot silently carry an ownership capability that
         * this schema does not understand.  Unknown records may retain such
         * bits as producer evidence, but consumers must never treat them as
         * part of a proven contract. */
        zr_diagnostic_set(
                diagnostic, ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_KNOWN_MASK,
                facts->ownershipMask);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_RECEIVER_READONLY) != 0u &&
            facts->receiverEffect != ZR_OPTIMIZATION_RECEIVER_READONLY) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                          ZR_OPTIMIZATION_RECEIVER_READONLY,
                          facts->receiverEffect);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u &&
            facts->borrowState != ZR_OPTIMIZATION_BORROW_STABLE) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                          ZR_OPTIMIZATION_BORROW_STABLE, facts->borrowState);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u &&
            (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) != 0u) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                          ZR_EXECUTION_EFFECT_SUSPEND,
                          facts->declaredEffects);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u &&
            (facts->taskEffects &
             (ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND |
              ZR_OPTIMIZATION_TASK_EFFECT_BORROW_ESCAPE |
              ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN)) != 0u) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                          ZR_OPTIMIZATION_TASK_EFFECT_NONE,
                          facts->taskEffects);
        return ZR_FALSE;
    }
    if (facts->validity == ZR_OPTIMIZATION_FACTS_PROVEN &&
            (facts->factMask & ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC) != 0u &&
            (facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE ||
             (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) != 0u)) {
        zr_diagnostic_set(diagnostic, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                          ZR_OPTIMIZATION_TASK_EFFECT_NONE,
                          facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE
                                  ? facts->taskEffects
                                  : facts->declaredEffects);
        return ZR_FALSE;
    }

    if (facts->proofHash != 0u) {
        expectedHash = zr_facts_hash_payload(facts);
        if (facts->proofHash != expectedHash) {
            zr_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                              expectedHash,
                              facts->proofHash);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_OptimizationProtocol_Validate(
        const SZrOptimizationProtocol *protocol,
        SZrExecIrDiagnostic *diagnostic) {
    TZrBool operationShapeValid = ZR_TRUE;

    zr_diagnostic_clear(diagnostic);
    if (protocol == ZR_NULL ||
        protocol->schemaVersion != ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION ||
        protocol->kind == ZR_OPTIMIZATION_PROTOCOL_NONE ||
        protocol->kind >= ZR_OPTIMIZATION_PROTOCOL_KIND_COUNT ||
        protocol->protocolToken == 0u || protocol->operationMask == 0u ||
        zr_has_unknown_bits(protocol->operationMask,
                            ZR_OPTIMIZATION_PROTOCOL_OP_KNOWN_MASK) ||
        zr_has_unknown_bits(protocol->requiredFacts,
                            ZR_OPTIMIZATION_FACT_KNOWN_MASK) ||
        zr_has_unknown_bits(protocol->declaredEffects,
                            ZR_EXECUTION_EFFECT_KNOWN_MASK)) {
        zr_diagnostic_set(diagnostic,
                          ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION,
                          protocol == ZR_NULL ? 0u : protocol->schemaVersion);
        return ZR_FALSE;
    }
    switch ((EZrOptimizationProtocolKind)protocol->kind) {
        case ZR_OPTIMIZATION_PROTOCOL_CONTIGUOUS_VIEW:
            operationShapeValid = (TZrBool)(
                    (protocol->operationMask &
                     (ZR_OPTIMIZATION_PROTOCOL_OP_LOAD |
                      ZR_OPTIMIZATION_PROTOCOL_OP_STORE |
                      ZR_OPTIMIZATION_PROTOCOL_OP_SLICE |
                      ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE |
                      ZR_OPTIMIZATION_PROTOCOL_OP_BORROW |
                      ZR_OPTIMIZATION_PROTOCOL_OP_READONLY)) != 0u);
            break;
        case ZR_OPTIMIZATION_PROTOCOL_TYPED_ARRAY:
            operationShapeValid = (TZrBool)(
                    (protocol->operationMask &
                     (ZR_OPTIMIZATION_PROTOCOL_OP_LOAD |
                      ZR_OPTIMIZATION_PROTOCOL_OP_STORE |
                      ZR_OPTIMIZATION_PROTOCOL_OP_SLICE |
                      ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE |
                      ZR_OPTIMIZATION_PROTOCOL_OP_BATCH |
                      ZR_OPTIMIZATION_PROTOCOL_OP_BORROW |
                      ZR_OPTIMIZATION_PROTOCOL_OP_READONLY)) != 0u);
            break;
        case ZR_OPTIMIZATION_PROTOCOL_BATCH:
            operationShapeValid = (TZrBool)(
                    (protocol->operationMask & ZR_OPTIMIZATION_PROTOCOL_OP_BATCH) != 0u);
            break;
        case ZR_OPTIMIZATION_PROTOCOL_ITERATION:
            operationShapeValid = (TZrBool)(
                    (protocol->operationMask &
                     (ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE |
                      ZR_OPTIMIZATION_PROTOCOL_OP_NEXT)) != 0u);
            break;
        case ZR_OPTIMIZATION_PROTOCOL_IMMUTABLE_UPDATE:
        case ZR_OPTIMIZATION_PROTOCOL_PERSISTENT_UPDATE:
            operationShapeValid = (TZrBool)(
                    (protocol->operationMask & ZR_OPTIMIZATION_PROTOCOL_OP_UPDATE) != 0u);
            break;
        case ZR_OPTIMIZATION_PROTOCOL_NONE:
        case ZR_OPTIMIZATION_PROTOCOL_KIND_COUNT:
        default:
            operationShapeValid = ZR_FALSE;
            break;
    }
    if (!operationShapeValid) {
        zr_diagnostic_set(diagnostic,
                          ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          protocol->kind,
                          protocol->operationMask);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_query_is_malformed(const SZrOptimizationFactQuery *query) {
    if (query == ZR_NULL ||
        query->schemaVersion != ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION ||
        zr_has_unknown_bits(query->requiredFacts,
                            ZR_OPTIMIZATION_FACT_KNOWN_MASK) ||
        zr_has_unknown_bits(query->forbiddenEffects,
                            ZR_EXECUTION_EFFECT_KNOWN_MASK) ||
        zr_has_unknown_bits(query->expectedTaskEffects,
                            ZR_OPTIMIZATION_TASK_EFFECT_KNOWN_MASK) ||
        zr_has_unknown_bits(query->forbiddenTaskEffects,
                            ZR_OPTIMIZATION_TASK_EFFECT_KNOWN_MASK) ||
        !zr_is_valid_receiver_effect(query->observedReceiverEffect) ||
        !zr_is_valid_receiver_effect(query->expectedReceiverEffect) ||
        !zr_is_valid_borrow_state(query->observedBorrowState) ||
        !zr_is_valid_borrow_state(query->expectedBorrowState) ||
        query->observedProtocolKind >= ZR_OPTIMIZATION_PROTOCOL_KIND_COUNT ||
        query->expectedProtocolKind >= ZR_OPTIMIZATION_PROTOCOL_KIND_COUNT ||
        zr_has_unknown_bits(query->observedProtocolOperations,
                            ZR_OPTIMIZATION_PROTOCOL_OP_KNOWN_MASK) ||
        zr_has_unknown_bits(query->expectedProtocolOperations,
                            ZR_OPTIMIZATION_PROTOCOL_OP_KNOWN_MASK) ||
        query->observedTaskEffectsKnown > 1u ||
        query->observedTaskEffectsUnknown > 1u ||
        query->observedExternalEffectsUnknown > 1u ||
        query->observedExternalEffectsKnown > 1u ||
        query->observedReceiverMayMutate > 1u ||
        query->observedBorrowAcrossSuspend > 1u) {
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool zr_validate_facts_preserving_diagnostic(
        const SZrOptimizationFacts *facts,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrDiagnostic validationDiagnostic;

    memset(&validationDiagnostic, 0, sizeof(validationDiagnostic));
    if (ZrParser_OptimizationFacts_Validate(facts, &validationDiagnostic)) {
        return ZR_TRUE;
    }
    if (diagnostic != ZR_NULL) {
        *diagnostic = validationDiagnostic;
    }
    return ZR_FALSE;
}

static void zr_query_set_diagnostic_for_unknown(
        const SZrOptimizationFactQuery *query,
        const SZrOptimizationFacts *facts,
        EZrOptimizationFactUnknownReason reason,
        SZrExecIrDiagnostic *diagnostic) {
    if (query == ZR_NULL || facts == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    switch (reason) {
        case ZR_OPTIMIZATION_UNKNOWN_STALE_GENERATION:
            zr_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                              query->expectedGeneration,
                              facts->generation);
            break;
        case ZR_OPTIMIZATION_UNKNOWN_LAYOUT:
            zr_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH,
                              query->expectedLayoutId,
                              facts->layoutId);
            break;
        case ZR_OPTIMIZATION_UNKNOWN_PROTOCOL:
            if (query->expectedTypeToken != 0u &&
                    query->expectedTypeToken != facts->typeToken) {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                                  query->expectedTypeToken,
                                  facts->typeToken);
            } else if (query->expectedProtocolKind != 0u &&
                       query->expectedProtocolKind != facts->protocolKind) {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                                  query->expectedProtocolKind,
                                  facts->protocolKind);
            } else if (query->expectedProtocolOperations != 0u &&
                       (facts->protocolOperations &
                        query->expectedProtocolOperations) !=
                               query->expectedProtocolOperations) {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                                  query->expectedProtocolOperations,
                                  facts->protocolOperations);
            } else {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                                  query->expectedProtocolHash,
                                  facts->protocolHash);
            }
            break;
        case ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS:
            zr_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                              query->forbiddenEffects,
                              facts->declaredEffects);
            break;
        case ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT:
            zr_diagnostic_set(diagnostic,
                              ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                              query->forbiddenTaskEffects,
                              facts->taskEffects);
            break;
        case ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED:
            if (query->expectedModuleHash != 0u &&
                    query->expectedModuleHash != facts->moduleHash) {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                                  query->expectedModuleHash,
                                  facts->moduleHash);
            } else if (query->expectedTypeToken != 0u &&
                       query->expectedTypeToken != facts->typeToken) {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                                  query->expectedTypeToken,
                                  facts->typeToken);
            }
            break;
        case ZR_OPTIMIZATION_UNKNOWN_RECEIVER:
            if (query->expectedReceiverEffect != ZR_OPTIMIZATION_RECEIVER_UNKNOWN) {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                                  query->expectedReceiverEffect,
                                  facts->receiverEffect);
            }
            break;
        case ZR_OPTIMIZATION_UNKNOWN_BORROW:
            if (query->expectedBorrowState != ZR_OPTIMIZATION_BORROW_UNKNOWN) {
                zr_diagnostic_set(diagnostic,
                                  ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                                  query->expectedBorrowState,
                                  facts->borrowState);
            }
            break;
        case ZR_OPTIMIZATION_UNKNOWN_IMMUTABILITY:
        case ZR_OPTIMIZATION_UNKNOWN_ALIAS:
        case ZR_OPTIMIZATION_UNKNOWN_ESCAPE:
        case ZR_OPTIMIZATION_UNKNOWN_OWNERSHIP:
        case ZR_OPTIMIZATION_UNKNOWN_NONE:
        case ZR_OPTIMIZATION_UNKNOWN_REASON_COUNT:
        default:
            /* Unknown facts are not language errors.  Leave a clean
             * diagnostic unless a contract identity mismatch above can help
             * a remark pinpoint the blocker. */
            break;
    }
    if (diagnostic != ZR_NULL &&
        diagnostic->code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        zr_diagnostic_apply_query_location(diagnostic, query);
    }
}

TZrBool ZrParser_Optimization_QueryFacts(
        const SZrOptimizationFactQuery *query,
        SZrOptimizationFacts *facts,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 unknownObservedFacts;
    TZrUInt32 unknownObservedEffects;
    TZrUInt32 unknownObservedOwnership;
    TZrUInt32 unknownObservedTaskEffects;
    TZrUInt32 missingFacts;
    EZrOptimizationFactUnknownReason reason = ZR_OPTIMIZATION_UNKNOWN_NONE;

    zr_diagnostic_clear(diagnostic);
    if (facts == ZR_NULL || query == ZR_NULL ||
        (const void *)facts == (const void *)query ||
        zr_query_is_malformed(query)) {
        zr_diagnostic_set(diagnostic,
                          ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                          ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION,
                          query == ZR_NULL ? 0u : query->schemaVersion);
        zr_diagnostic_apply_query_location(diagnostic, query);
        return ZR_FALSE;
    }
    /* Do not partially overwrite the caller's destination for malformed
     * input.  A valid query, including one that reports a language error,
     * receives a fully initialized inspectable facts record below. */
    ZrParser_OptimizationFacts_Init(facts);

    facts->factMask = query->observedFactMask & ZR_OPTIMIZATION_FACT_KNOWN_MASK;
    facts->declaredEffects = query->observedEffects & ZR_EXECUTION_EFFECT_KNOWN_MASK;
    facts->typeToken = query->observedTypeToken;
    facts->layoutId = query->observedLayoutId;
    facts->ownershipMask = query->observedOwnershipMask;
    facts->moduleHash = query->observedModuleHash;
    facts->generation = query->observedGeneration;
    facts->protocolHash = query->observedProtocolHash;
    facts->taskEffects = query->observedTaskEffects &
                         ZR_OPTIMIZATION_TASK_EFFECT_KNOWN_MASK;
    facts->receiverEffect = query->observedReceiverEffect;
    facts->borrowState = query->observedBorrowState;
    facts->protocolKind = query->observedProtocolKind;
    facts->protocolOperations = query->observedProtocolOperations;
    facts->proofFunctionToken = query->functionToken;
    facts->proofBlockId = query->blockId;
    facts->proofInstructionId = query->instructionId;
    facts->proofSourceId = query->sourceId;

    /* These scalar states are canonical parser evidence.  Derive only facts
     * that are directly justified by an explicit state; absence of evidence
     * never creates a capability. */
    if (query->observedReceiverEffect == ZR_OPTIMIZATION_RECEIVER_READONLY &&
            query->observedReceiverMayMutate == 0u) {
        facts->factMask |= ZR_OPTIMIZATION_FACT_RECEIVER_READONLY;
    }
    /* Canonical type definitions already publish Send/Sync capability bits
     * alongside ownership facts.  Derive the optimization proof only when
     * both bits are present and no out-of-contract ownership bit is set;
     * otherwise an opaque/extended mask remains conservative. */
    if ((query->observedOwnershipMask &
         ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND_SYNC) ==
                ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND_SYNC &&
            (query->observedOwnershipMask &
             ~ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_KNOWN_MASK) == 0u) {
        facts->factMask |= ZR_OPTIMIZATION_FACT_SEND_SYNC;
    }
    if (query->observedBorrowState == ZR_OPTIMIZATION_BORROW_STABLE &&
            query->observedBorrowAcrossSuspend == 0u &&
            (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) == 0u &&
            query->observedTaskEffectsKnown != 0u &&
            (facts->taskEffects & (ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND |
                                   ZR_OPTIMIZATION_TASK_EFFECT_BORROW_ESCAPE |
                                   ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN)) == 0u) {
        facts->factMask |= ZR_OPTIMIZATION_FACT_BORROW_SAFE;
    }
    if (query->observedTaskEffectsKnown != 0u &&
            query->observedTaskEffectsUnknown == 0u &&
            facts->taskEffects == ZR_OPTIMIZATION_TASK_EFFECT_NONE &&
            (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) == 0u) {
        facts->factMask |= ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC;
    }

    unknownObservedFacts = query->observedFactMask &
                           ~ZR_OPTIMIZATION_FACT_KNOWN_MASK;
    unknownObservedEffects = query->observedEffects &
                             ~ZR_EXECUTION_EFFECT_KNOWN_MASK;
    unknownObservedOwnership = query->observedOwnershipMask &
                               ~ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_KNOWN_MASK;
    unknownObservedTaskEffects = query->observedTaskEffects &
                                 ~ZR_OPTIMIZATION_TASK_EFFECT_KNOWN_MASK;

    if (query->invalidLanguageUse != 0u ||
        query->observedBorrowAcrossSuspend != 0u ||
        query->observedBorrowState == ZR_OPTIMIZATION_BORROW_ACROSS_SUSPEND) {
        facts->validity = ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE;
        facts->unknownReason = ZR_OPTIMIZATION_UNKNOWN_NONE;
        facts->proofHash = zr_facts_hash_payload(facts);
        zr_diagnostic_set(
                diagnostic,
                (query->observedBorrowAcrossSuspend != 0u ||
                 query->observedBorrowState == ZR_OPTIMIZATION_BORROW_ACROSS_SUSPEND)
                        ? ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND
                        : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                0u,
                query->invalidLanguageUse != 0u
                        ? query->invalidLanguageUse
                        : query->observedBorrowState);
        zr_diagnostic_apply_query_location(diagnostic, query);
        /* The facts record remains inspectable for diagnostics, but the
         * query itself must fail so the compiler treats this as a language
         * error rather than a missed optimization. */
        (void)ZrParser_OptimizationFacts_Validate(facts, ZR_NULL);
        return ZR_FALSE;
    }

    if (unknownObservedEffects != 0u ||
        query->observedExternalEffectsUnknown != 0u ||
        query->observedExternalEffectsKnown == 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS;
    } else if (unknownObservedTaskEffects != 0u ||
               query->observedTaskEffectsKnown == 0u ||
               query->observedTaskEffectsUnknown != 0u ||
               (facts->taskEffects & ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN) != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if (((query->requiredFacts & ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC) != 0u) &&
               (facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE ||
                (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) != 0u)) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if (facts->borrowState == ZR_OPTIMIZATION_BORROW_ESCAPES ||
               (((query->requiredFacts & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u) &&
                facts->borrowState == ZR_OPTIMIZATION_BORROW_UNKNOWN)) {
        reason = ZR_OPTIMIZATION_UNKNOWN_BORROW;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u &&
               (facts->taskEffects &
                (ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND |
                 ZR_OPTIMIZATION_TASK_EFFECT_BORROW_ESCAPE |
                 ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN)) != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u &&
               (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) != 0u) {
        /* A producer may publish a candidate borrow proof before the complete
         * effect summary arrives.  Keep this a conservative optimization miss
         * rather than surfacing the validator's malformed-PROVEN result. */
        reason = ZR_OPTIMIZATION_UNKNOWN_BORROW;
    } else if (query->observedReceiverMayMutate != 0u ||
               (query->expectedReceiverEffect !=
                        ZR_OPTIMIZATION_RECEIVER_UNKNOWN &&
                facts->receiverEffect != query->expectedReceiverEffect) ||
               ((facts->factMask & ZR_OPTIMIZATION_FACT_RECEIVER_READONLY) != 0u &&
                facts->receiverEffect != ZR_OPTIMIZATION_RECEIVER_READONLY)) {
        reason = ZR_OPTIMIZATION_UNKNOWN_RECEIVER;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_NO_ALLOC) != 0u &&
               (facts->declaredEffects & ZR_EXECUTION_EFFECT_ALLOCATE) != 0u) {
        /* A producer may have copied a candidate NO_ALLOC bit before the
         * complete effect summary arrived.  Treat the contradiction as a
         * conservative optimization miss, not as a malformed language use. */
        reason = ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_PURITY) != 0u &&
               facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_PURITY) != 0u &&
               (facts->declaredEffects &
                (ZR_EXECUTION_EFFECT_WRITE_MEMORY |
                 ZR_EXECUTION_EFFECT_ALLOCATE |
                 ZR_EXECUTION_EFFECT_THROW |
                 ZR_EXECUTION_EFFECT_SUSPEND)) != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_NO_THROW) != 0u &&
               (facts->declaredEffects & ZR_EXECUTION_EFFECT_THROW) != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_NO_SUSPEND) != 0u &&
               (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_NO_SUSPEND) != 0u &&
               facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_BORROW_SAFE) != 0u &&
               facts->borrowState != ZR_OPTIMIZATION_BORROW_STABLE) {
        reason = ZR_OPTIMIZATION_UNKNOWN_BORROW;
    } else if ((facts->factMask & ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC) != 0u &&
               (facts->taskEffects != ZR_OPTIMIZATION_TASK_EFFECT_NONE ||
                (facts->declaredEffects & ZR_EXECUTION_EFFECT_SUSPEND) != 0u)) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if (unknownObservedOwnership != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_OWNERSHIP;
    } else if (unknownObservedFacts != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED;
    } else if ((facts->declaredEffects & query->forbiddenEffects) != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS;
    } else if ((facts->taskEffects & query->forbiddenTaskEffects) != 0u) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if (query->expectedGeneration != 0u &&
               query->expectedGeneration != facts->generation) {
        reason = ZR_OPTIMIZATION_UNKNOWN_STALE_GENERATION;
    } else if (query->expectedModuleHash != 0u &&
               query->expectedModuleHash != facts->moduleHash) {
        reason = ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED;
    } else if (query->expectedTypeToken != 0u &&
               query->expectedTypeToken != facts->typeToken) {
        reason = ZR_OPTIMIZATION_UNKNOWN_PROTOCOL;
    } else if (query->expectedLayoutId != 0u &&
               query->expectedLayoutId != facts->layoutId) {
        reason = ZR_OPTIMIZATION_UNKNOWN_LAYOUT;
    } else if (query->expectedProtocolHash != 0u &&
               query->expectedProtocolHash != facts->protocolHash) {
        reason = ZR_OPTIMIZATION_UNKNOWN_PROTOCOL;
    } else if (query->expectedTaskEffects != 0u &&
               (facts->taskEffects & query->expectedTaskEffects) !=
                       query->expectedTaskEffects) {
        reason = ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT;
    } else if (query->expectedBorrowState != 0u &&
               facts->borrowState != query->expectedBorrowState) {
        reason = ZR_OPTIMIZATION_UNKNOWN_BORROW;
    } else if (query->expectedProtocolKind != 0u &&
               facts->protocolKind != query->expectedProtocolKind) {
        reason = ZR_OPTIMIZATION_UNKNOWN_PROTOCOL;
    } else if (query->expectedProtocolOperations != 0u &&
               (facts->protocolOperations & query->expectedProtocolOperations) !=
                       query->expectedProtocolOperations) {
        reason = ZR_OPTIMIZATION_UNKNOWN_PROTOCOL;
    } else {
        missingFacts = query->requiredFacts & ~facts->factMask;
        if (missingFacts != 0u) {
            reason = zr_missing_fact_reason(missingFacts);
        } else {
            facts->validity = ZR_OPTIMIZATION_FACTS_PROVEN;
            facts->unknownReason = ZR_OPTIMIZATION_UNKNOWN_NONE;
        }
    }

    if (reason != ZR_OPTIMIZATION_UNKNOWN_NONE) {
        zr_facts_mark_unknown(facts, reason);
        zr_query_set_diagnostic_for_unknown(query, facts, reason, diagnostic);
    }
    facts->proofHash = zr_facts_hash_payload(facts);
    if (!zr_validate_facts_preserving_diagnostic(facts, diagnostic)) {
        zr_diagnostic_apply_query_location(diagnostic, query);
        return ZR_FALSE;
    }
    if (diagnostic != ZR_NULL &&
        diagnostic->code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        zr_diagnostic_apply_query_location(diagnostic, query);
    }
    return ZR_TRUE;
}

static EZrExecutionDiagnosticCode zr_protocol_mismatch_code(
        const SZrOptimizationProtocol *provided,
        const SZrOptimizationProtocol *required,
        TZrUInt64 *expected,
        TZrUInt64 *actual) {
    if (provided->protocolToken != required->protocolToken ||
        provided->kind != required->kind) {
        *expected = required->protocolToken;
        *actual = provided->protocolToken;
        return ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH;
    }
    if (required->signatureHash != 0u &&
        provided->signatureHash != required->signatureHash) {
        *expected = required->signatureHash;
        *actual = provided->signatureHash;
        return ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH;
    }
    if ((provided->operationMask & required->operationMask) !=
            required->operationMask ||
        (provided->requiredFacts & required->requiredFacts) !=
            required->requiredFacts) {
        *expected = required->operationMask | required->requiredFacts;
        *actual = provided->operationMask | provided->requiredFacts;
        return ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH;
    }
    if (provided->declaredEffects != required->declaredEffects) {
        *expected = required->declaredEffects;
        *actual = provided->declaredEffects;
        return ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH;
    }
    if ((required->layoutId != 0u &&
         provided->layoutId != required->layoutId) ||
        (required->layoutHash != 0u &&
         provided->layoutHash != required->layoutHash)) {
        *expected = required->layoutHash != 0u
                           ? required->layoutHash
                           : required->layoutId;
        *actual = provided->layoutHash != 0u
                         ? provided->layoutHash
                         : provided->layoutId;
        return ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH;
    }
    if (required->moduleHash != 0u &&
        provided->moduleHash != required->moduleHash) {
        *expected = required->moduleHash;
        *actual = provided->moduleHash;
        return ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH;
    }
    if (required->generation != 0u &&
        provided->generation != required->generation) {
        *expected = required->generation;
        *actual = provided->generation;
        return ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION;
    }
    return ZR_EXECUTION_DIAGNOSTIC_NONE;
}

TZrBool ZrParser_Optimization_RecognizeProtocol(
        const SZrOptimizationProtocol *provided,
        const SZrOptimizationProtocol *required,
        SZrExecIrDiagnostic *diagnostic) {
    EZrExecutionDiagnosticCode code;
    TZrUInt64 expected = 0u;
    TZrUInt64 actual = 0u;

    zr_diagnostic_clear(diagnostic);
    if (!ZrParser_OptimizationProtocol_Validate(provided, diagnostic) ||
        !ZrParser_OptimizationProtocol_Validate(required, diagnostic)) {
        return ZR_FALSE;
    }

    code = zr_protocol_mismatch_code(provided, required, &expected, &actual);
    if (code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        zr_diagnostic_set(diagnostic, code, expected, actual);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
