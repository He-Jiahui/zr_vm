#ifndef ZR_VM_TESTS_SSA_DIFFERENTIAL_SUPPORT_H
#define ZR_VM_TESTS_SSA_DIFFERENTIAL_SUPPORT_H

#include "zr_vm_common/zr_common_conf.h"

#define ZR_SSA_MAX_EVENTS 128u

typedef enum EZrSsaEventKind {
    ZR_SSA_EVENT_INVALID = 0,
    ZR_SSA_EVENT_GET,
    ZR_SSA_EVENT_WRITE,
    ZR_SSA_EVENT_WRITEBACK,
    ZR_SSA_EVENT_DROP,
    ZR_SSA_EVENT_ALLOCATE,
    ZR_SSA_EVENT_SUSPEND,
    ZR_SSA_EVENT_RESUME,
    ZR_SSA_EVENT_THROW,
    ZR_SSA_EVENT_RETURN,
    ZR_SSA_EVENT_COUNT
} EZrSsaEventKind;

typedef enum EZrSsaResultType {
    ZR_SSA_RESULT_NONE = 0,
    ZR_SSA_RESULT_INTEGER,
    ZR_SSA_RESULT_FLOAT,
    ZR_SSA_RESULT_BOOLEAN,
    ZR_SSA_RESULT_REFERENCE,
    ZR_SSA_RESULT_UNIT
} EZrSsaResultType;

typedef struct SZrSsaEvent {
    EZrSsaEventKind kind;
    TZrUInt32 sourceId;
    TZrUInt64 valueBits;
    TZrUInt64 auxiliary;
} SZrSsaEvent;

typedef struct SZrSsaObservation {
    TZrUInt32 backend;
    TZrBool completed;
    TZrBool hasException;
    TZrBool fallbackVisible;
    TZrBool nativeCoverageAvailable;
    EZrSsaResultType resultType;
    TZrUInt64 resultBits;
    TZrUInt64 resultAuxiliary;
    TZrUInt32 exceptionType;
    TZrUInt32 exceptionSourceId;
    TZrUInt64 droppedCount;
    TZrUInt64 writebackCount;
    double nativeCoverage;
    TZrUInt32 eventCount;
    SZrSsaEvent events[ZR_SSA_MAX_EVENTS];
} SZrSsaObservation;

typedef enum EZrSsaDiffReason {
    ZR_SSA_DIFF_NONE = 0,
    ZR_SSA_DIFF_INVALID_ARGUMENT,
    ZR_SSA_DIFF_MALFORMED_OBSERVATION,
    ZR_SSA_DIFF_RESULT_MISMATCH,
    ZR_SSA_DIFF_EXCEPTION_MISMATCH,
    ZR_SSA_DIFF_EFFECT_MISMATCH,
    ZR_SSA_DIFF_EVENT_MISMATCH,
    ZR_SSA_DIFF_BACKEND_UNSUPPORTED,
    ZR_SSA_DIFF_COVERAGE_INCOMPLETE
} EZrSsaDiffReason;

typedef struct SZrSsaDiffDiagnostic {
    EZrSsaDiffReason reason;
    TZrUInt32 eventIndex;
    EZrSsaEventKind expectedEvent;
    EZrSsaEventKind actualEvent;
    TZrUInt32 expectedSourceId;
    TZrUInt32 actualSourceId;
} SZrSsaDiffDiagnostic;

struct SZrSsaFixture;
typedef TZrBool (*FZrSsaFixtureRunner)(const struct SZrSsaFixture *fixture,
                                       TZrUInt32 backend,
                                       SZrSsaObservation *observation);

typedef struct SZrSsaFixture {
    const TZrChar *name;
    TZrUInt32 requiredBackends;
    FZrSsaFixtureRunner runner;
} SZrSsaFixture;

typedef struct SZrSsaCoverage {
    TZrUInt32 requiredBackends;
    TZrUInt32 executedBackends;
    TZrUInt32 failedBackends;
} SZrSsaCoverage;

void ZrTests_Ssa_ObservationInit(SZrSsaObservation *observation);
TZrBool ZrTests_Ssa_ObservationAppendEvent(SZrSsaObservation *observation,
                                            EZrSsaEventKind kind,
                                            TZrUInt32 sourceId,
                                            TZrUInt64 valueBits,
                                            TZrUInt64 auxiliary);
TZrBool ZrTests_Ssa_ObservationValidate(const SZrSsaObservation *observation);
TZrBool ZrTests_Ssa_Compare(const SZrSsaObservation *expected,
                            const SZrSsaObservation *actual,
                            SZrSsaDiffDiagnostic *diagnostic);
TZrBool ZrTests_Ssa_RunFixture(const SZrSsaFixture *fixture,
                               TZrUInt32 backend,
                               SZrSsaObservation *observation,
                               SZrSsaDiffDiagnostic *diagnostic);
void ZrTests_Ssa_CoverageInit(SZrSsaCoverage *coverage, TZrUInt32 requiredBackends);
void ZrTests_Ssa_CoverageRecord(SZrSsaCoverage *coverage,
                                TZrUInt32 backend,
                                TZrBool succeeded);
TZrBool ZrTests_Ssa_CoverageComplete(const SZrSsaCoverage *coverage);
const TZrChar *ZrTests_Ssa_DiffReasonName(EZrSsaDiffReason reason);

#endif
