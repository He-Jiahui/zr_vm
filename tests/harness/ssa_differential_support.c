#include "ssa_differential_support.h"

#include <string.h>

static void zr_ssa_diff_clear(SZrSsaDiffDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static TZrBool zr_ssa_event_kind_is_valid(EZrSsaEventKind kind) {
    return (TZrBool)(kind > ZR_SSA_EVENT_INVALID && kind < ZR_SSA_EVENT_COUNT);
}

static TZrBool zr_ssa_observation_is_valid(const SZrSsaObservation *observation) {
    TZrUInt32 index;

    if (observation == ZR_NULL || observation->backend >= 32u ||
        observation->eventCount > ZR_SSA_MAX_EVENTS ||
        observation->resultType < ZR_SSA_RESULT_NONE ||
        observation->resultType > ZR_SSA_RESULT_UNIT ||
        (observation->hasException == ZR_FALSE &&
         (observation->exceptionType != 0u || observation->exceptionSourceId != 0u))) {
        return ZR_FALSE;
    }
    if (observation->nativeCoverageAvailable != ZR_FALSE &&
        (observation->nativeCoverage < 0.0 || observation->nativeCoverage > 1.0)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < observation->eventCount; ++index) {
        if (!zr_ssa_event_kind_is_valid(observation->events[index].kind)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

void ZrTests_Ssa_ObservationInit(SZrSsaObservation *observation) {
    if (observation != ZR_NULL) {
        memset(observation, 0, sizeof(*observation));
        observation->resultType = ZR_SSA_RESULT_NONE;
    }
}

TZrBool ZrTests_Ssa_ObservationAppendEvent(SZrSsaObservation *observation,
                                            EZrSsaEventKind kind,
                                            TZrUInt32 sourceId,
                                            TZrUInt64 valueBits,
                                            TZrUInt64 auxiliary) {
    SZrSsaEvent *event;

    if (observation == ZR_NULL || !zr_ssa_event_kind_is_valid(kind) ||
        observation->eventCount >= ZR_SSA_MAX_EVENTS) {
        return ZR_FALSE;
    }
    event = &observation->events[observation->eventCount++];
    event->kind = kind;
    event->sourceId = sourceId;
    event->valueBits = valueBits;
    event->auxiliary = auxiliary;
    return ZR_TRUE;
}

TZrBool ZrTests_Ssa_ObservationValidate(const SZrSsaObservation *observation) {
    return zr_ssa_observation_is_valid(observation);
}

static TZrBool zr_ssa_diff_fail(SZrSsaDiffDiagnostic *diagnostic,
                                EZrSsaDiffReason reason,
                                TZrUInt32 eventIndex,
                                EZrSsaEventKind expectedEvent,
                                EZrSsaEventKind actualEvent,
                                TZrUInt32 expectedSourceId,
                                TZrUInt32 actualSourceId) {
    if (diagnostic != ZR_NULL) {
        diagnostic->reason = reason;
        diagnostic->eventIndex = eventIndex;
        diagnostic->expectedEvent = expectedEvent;
        diagnostic->actualEvent = actualEvent;
        diagnostic->expectedSourceId = expectedSourceId;
        diagnostic->actualSourceId = actualSourceId;
    }
    return ZR_FALSE;
}

TZrBool ZrTests_Ssa_Compare(const SZrSsaObservation *expected,
                            const SZrSsaObservation *actual,
                            SZrSsaDiffDiagnostic *diagnostic) {
    TZrUInt32 index;

    zr_ssa_diff_clear(diagnostic);
    if (!zr_ssa_observation_is_valid(expected) || !zr_ssa_observation_is_valid(actual)) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_MALFORMED_OBSERVATION,
                                0u,
                                ZR_SSA_EVENT_INVALID,
                                ZR_SSA_EVENT_INVALID,
                                0u,
                                0u);
    }
    if (expected->completed != actual->completed ||
        expected->resultType != actual->resultType ||
        expected->resultBits != actual->resultBits ||
        expected->resultAuxiliary != actual->resultAuxiliary) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_RESULT_MISMATCH,
                                0u,
                                ZR_SSA_EVENT_INVALID,
                                ZR_SSA_EVENT_INVALID,
                                0u,
                                0u);
    }
    if (expected->hasException != actual->hasException ||
        expected->exceptionType != actual->exceptionType ||
        expected->exceptionSourceId != actual->exceptionSourceId) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_EXCEPTION_MISMATCH,
                                0u,
                                ZR_SSA_EVENT_INVALID,
                                ZR_SSA_EVENT_INVALID,
                                expected->exceptionSourceId,
                                actual->exceptionSourceId);
    }
    if (expected->droppedCount != actual->droppedCount ||
        expected->writebackCount != actual->writebackCount) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_EFFECT_MISMATCH,
                                0u,
                                ZR_SSA_EVENT_INVALID,
                                ZR_SSA_EVENT_INVALID,
                                0u,
                                0u);
    }
    if (expected->eventCount != actual->eventCount) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_EVENT_MISMATCH,
                                expected->eventCount < actual->eventCount
                                        ? expected->eventCount
                                        : actual->eventCount,
                                expected->eventCount < actual->eventCount
                                        ? ZR_SSA_EVENT_INVALID
                                        : expected->events[actual->eventCount].kind,
                                actual->eventCount < expected->eventCount
                                        ? ZR_SSA_EVENT_INVALID
                                        : actual->events[expected->eventCount].kind,
                                0u,
                                0u);
    }
    for (index = 0u; index < expected->eventCount; ++index) {
        const SZrSsaEvent *expectedEvent = &expected->events[index];
        const SZrSsaEvent *actualEvent = &actual->events[index];
        if (expectedEvent->kind != actualEvent->kind ||
            expectedEvent->sourceId != actualEvent->sourceId ||
            expectedEvent->valueBits != actualEvent->valueBits ||
            expectedEvent->auxiliary != actualEvent->auxiliary) {
            return zr_ssa_diff_fail(diagnostic,
                                    ZR_SSA_DIFF_EVENT_MISMATCH,
                                    index,
                                    expectedEvent->kind,
                                    actualEvent->kind,
                                    expectedEvent->sourceId,
                                    actualEvent->sourceId);
        }
    }
    return ZR_TRUE;
}

TZrBool ZrTests_Ssa_RunFixture(const SZrSsaFixture *fixture,
                               TZrUInt32 backend,
                               SZrSsaObservation *observation,
                               SZrSsaDiffDiagnostic *diagnostic) {
    zr_ssa_diff_clear(diagnostic);
    if (fixture == ZR_NULL || observation == ZR_NULL || backend >= 32u) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_INVALID_ARGUMENT,
                                0u,
                                ZR_SSA_EVENT_INVALID,
                                ZR_SSA_EVENT_INVALID,
                                0u,
                                0u);
    }
    if (fixture->runner == ZR_NULL) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_BACKEND_UNSUPPORTED,
                                0u,
                                ZR_SSA_EVENT_INVALID,
                                ZR_SSA_EVENT_INVALID,
                                0u,
                                0u);
    }
    ZrTests_Ssa_ObservationInit(observation);
    if (!fixture->runner(fixture, backend, observation) ||
        !zr_ssa_observation_is_valid(observation)) {
        return zr_ssa_diff_fail(diagnostic,
                                ZR_SSA_DIFF_MALFORMED_OBSERVATION,
                                0u,
                                ZR_SSA_EVENT_INVALID,
                                ZR_SSA_EVENT_INVALID,
                                0u,
                                0u);
    }
    return ZR_TRUE;
}

void ZrTests_Ssa_CoverageInit(SZrSsaCoverage *coverage, TZrUInt32 requiredBackends) {
    if (coverage != ZR_NULL) {
        coverage->requiredBackends = requiredBackends;
        coverage->executedBackends = 0u;
        coverage->failedBackends = 0u;
    }
}

void ZrTests_Ssa_CoverageRecord(SZrSsaCoverage *coverage,
                                TZrUInt32 backend,
                                const SZrSsaObservation *observation,
                                TZrBool semanticMatches) {
    TZrUInt32 bit;

    if (coverage == ZR_NULL || backend >= 32u) {
        return;
    }
    bit = (TZrUInt32)1u << backend;
    coverage->executedBackends |= bit;
    /* Semantic equality can succeed under fallback; only the requested
     * backend actually executing without fallback satisfies its gate. */
    if (semanticMatches == ZR_FALSE ||
        !zr_ssa_observation_is_valid(observation) ||
        observation->backend != backend || observation->fallbackVisible) {
        coverage->failedBackends |= bit;
    }
}

TZrBool ZrTests_Ssa_CoverageComplete(const SZrSsaCoverage *coverage) {
    if (coverage == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)((coverage->requiredBackends & ~coverage->executedBackends) == 0u &&
                     (coverage->failedBackends & coverage->requiredBackends) == 0u);
}

const TZrChar *ZrTests_Ssa_DiffReasonName(EZrSsaDiffReason reason) {
    switch (reason) {
        case ZR_SSA_DIFF_NONE:
            return "NONE";
        case ZR_SSA_DIFF_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case ZR_SSA_DIFF_MALFORMED_OBSERVATION:
            return "MALFORMED_OBSERVATION";
        case ZR_SSA_DIFF_RESULT_MISMATCH:
            return "RESULT_MISMATCH";
        case ZR_SSA_DIFF_EXCEPTION_MISMATCH:
            return "EXCEPTION_MISMATCH";
        case ZR_SSA_DIFF_EFFECT_MISMATCH:
            return "EFFECT_MISMATCH";
        case ZR_SSA_DIFF_EVENT_MISMATCH:
            return "EVENT_MISMATCH";
        case ZR_SSA_DIFF_BACKEND_UNSUPPORTED:
            return "BACKEND_UNSUPPORTED";
        case ZR_SSA_DIFF_COVERAGE_INCOMPLETE:
            return "COVERAGE_INCOMPLETE";
        default:
            return "UNKNOWN";
    }
}
