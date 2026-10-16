#ifndef ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_FIXTURE_H
#define ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_FIXTURE_H

#include "ssa_source_returned_retention_fixture.h"
#include "ssa_source_returned_execir_cleanup.h"
#include "tests/core/support/ssa_exec_ir_owner_capture.h"
#include "zr_vm_core/function_exec_ir.h"

/* These are independent caller storage bounds, not allocator capacity APIs.
 * Every actual range is checked; a refused observation fails the lifecycle. */
enum { ZR_INTEGER_EXPR_TRACE = 65536, ZR_INTEGER_EXPR_CALLS = 128,
       ZR_INTEGER_EXPR_RECORDS = 32768, ZR_INTEGER_EXPR_DEPTH = 16 };
typedef enum EZrIntegerExprPhase {
    ZR_INTEGER_EXPR_NONE, ZR_INTEGER_EXPR_PRODUCER_OTHER,
    ZR_INTEGER_EXPR_BRIDGE, ZR_INTEGER_EXPR_COMPACTOR, ZR_INTEGER_EXPR_FRAME,
    ZR_INTEGER_EXPR_FINALIZER, ZR_INTEGER_EXPR_CANDIDATE, ZR_INTEGER_EXPR_OWNER,
    ZR_INTEGER_EXPR_PHASE_COUNT
} EZrIntegerExprPhase;
typedef enum EZrIntegerExprCallKind {
    ZR_INTEGER_EXPR_SOURCE_CALL, ZR_INTEGER_EXPR_BRIDGE_CALL,
    ZR_INTEGER_EXPR_COMPACTOR_CALL, ZR_INTEGER_EXPR_APPEND_CALL,
    ZR_INTEGER_EXPR_ATTACH_CALL, ZR_INTEGER_EXPR_FINALIZER_CALL,
    ZR_INTEGER_EXPR_CANDIDATE_CALL, ZR_INTEGER_EXPR_OWNER_CALL,
    ZR_INTEGER_EXPR_FRAME_VALUES_CALL, ZR_INTEGER_EXPR_FRAME_HEADER_CALL,
    ZR_INTEGER_EXPR_CALL_KIND_COUNT
} EZrIntegerExprCallKind;
typedef struct SZrIntegerExprEvent {
    SZrOwnerHeapEvent actual;
    unsigned phase;
} SZrIntegerExprEvent;
typedef struct SZrIntegerExprCall {
    unsigned kind, phase, depth, hasDiagnostic;
    SZrOwnerHeapStats before, after;
    SZrExecIrDiagnostic diagnostic;
    TZrBool result;
} SZrIntegerExprCall;
typedef struct SZrIntegerExprObservation {
    SZrOwnerHeapStats before, after;
    SZrSourceCompileDiagnostic source;
    unsigned calls[ZR_INTEGER_EXPR_CALL_KIND_COUNT], callCount, errors;
    unsigned frameValuesMax, packedFrees;
    SZrIntegerExprCall call[ZR_INTEGER_EXPR_CALLS];
} SZrIntegerExprObservation;
typedef struct SZrIntegerExprClose {
    SZrReturnedExecCleanupReport ledgers;
    SZrReturnedVmStats vm;
    int beginResult, vmResult;
    unsigned observerErrors, closed;
} SZrIntegerExprClose;
typedef struct SZrIntegerExprFinal {
    unsigned lifecycles, closed, healthy, unhealthyLifecycles, observerErrors;
    SZrOwnerHeapStats owner;
    SZrReturnedHeapStats crt[2];
    SZrOwnerHeapStats ownerBeforeRecovery;
    SZrReturnedHeapStats crtBeforeRecovery[2];
    SZrReturnedVmStats vm;
    int vmResult, crtResult[2], crtBeforeResult[2];
} SZrIntegerExprFinal;

/* Serialized test control. Init refuses to erase an unverified previous close.
 * Compile uses the real public Source entry through the generic NULL-callback
 * fixture. All producer observers below are nonasserting, allocation-free
 * pass-through calls; no alternate graph is ever handed to a producer. */
int ZrTests_IntegerExpr_Init(void);
void ZrTests_IntegerExpr_Open(void);
SZrFunction *ZrTests_IntegerExpr_Compile(unsigned slot, const char *source,
        SZrSourceCompileDiagnostic *diagnostic);
const SZrIntegerExprObservation *ZrTests_IntegerExpr_Observation(void);
int ZrTests_IntegerExpr_TryTrace(unsigned relativeIndex, SZrIntegerExprEvent *output);
int ZrTests_IntegerExpr_CrtQuiet(void);
int ZrTests_IntegerExpr_Released(unsigned first, unsigned last);
int ZrTests_IntegerExpr_Published(const SZrFunction *function,
        const SZrIntegerExprObservation *observation);
int ZrTests_IntegerExpr_CaptureKeeper(void);
int ZrTests_IntegerExpr_KeeperUnchanged(void);
unsigned ZrTests_IntegerExpr_KeeperLive(void);
void ZrTests_IntegerExpr_Close(void);
const SZrIntegerExprClose *ZrTests_IntegerExpr_CloseReport(void);
int ZrTests_IntegerExpr_ClosedHealthy(void);
SZrIntegerExprFinal ZrTests_IntegerExpr_Final(void);
const char *ZrTests_IntegerExpr_PhaseName(unsigned phase);

#endif
