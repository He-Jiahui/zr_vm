#ifndef ZR_TEST_RETURNED_EXEC_FIXTURE_H
#define ZR_TEST_RETURNED_EXEC_FIXTURE_H
#include <stdint.h>
#include "zr_vm_parser/ast.h"
#include "ssa_source_returned_retention_fixture.h"
#include "ssa_source_returned_execir_cleanup.h"
#include "tests/core/support/ssa_exec_ir_owned_module_fixture.h"
#include "zr_vm_core/function_exec_ir.h"
enum { ZR_RETURNED_EXEC_ADDRESSES=128 };
typedef struct SZrReturnedExecObservation {
    SZrOwnerContents contents;
    uintptr_t moduleAddress,functionAddress,addresses[ZR_RETURNED_EXEC_ADDRESSES];
    unsigned addressCount,appendCalls,snapshotCalls,errors;
    TZrUInt32 constantCount,layoutCount,functionCount,typeToken,layoutTypeToken,moduleToken,entryToken;
    TZrUInt64 literalBits,moduleHash,signatureHash;
    TZrBool identityValid;
    unsigned producerFirst,ownerFirst,ownerLast,ownerAttempts,witnessFirst,witnessLast,directAttempts;
    SZrExecIrDiagnostic direct,actual;
    SZrOwnerHeapEvent directEvent,actualEvent;
    TZrBool directResult,snapshotResult,appendResult;
    int appendCaptureResult,beforeCaptureResult,afterCaptureResult,viewCaptureResult;
    int directArmResult,actualArmResult,directEventResult,actualEventResult;
    unsigned recordQueries,recordRefusals,eventQueries,eventRefusals;
} SZrReturnedExecObservation;
enum EZrReturnedExecChildSite {
    ZR_RETURNED_EXEC_CHILD_STATEMENTS_NULL=1,
    ZR_RETURNED_EXEC_CHILD_STATEMENTS_EMPTY,
    ZR_RETURNED_EXEC_CHILD_STATEMENT_NULL,
    ZR_RETURNED_EXEC_CHILD_EXPRESSION_NULL
};
typedef struct SZrReturnedExecChildObservation {
    unsigned site,entered,completed,unchanged,restored,ownerEmpty,poisoned,errors;
    uintptr_t scriptAddress,arrayAddress,nodesAddress,returnAddress,integerAddress,destinationAddress;
    TZrUInt32 functionToken;
    TZrBool result;
    SZrExecIrDiagnostic diagnostic;
} SZrReturnedExecChildObservation;
/* Only a genuine live producer AST is accepted. Begin/End have no assertions,
 * never decide the callee result, and restore all borrowed child links before
 * the producer resumes cleanup. Observation retains no dereferenceable borrow.
 */
int ZrTests_ReturnedExec_ChildArm(unsigned site);
void ZrTests_ReturnedExec_ChildBegin(const SZrAstNode *script,TZrUInt32 token,
        SZrExecIrOwnedModule **destination,SZrExecIrDiagnostic *diagnostic);
void ZrTests_ReturnedExec_ChildEnd(TZrBool result,SZrExecIrOwnedModule **destination,
        const SZrExecIrDiagnostic *diagnostic);
SZrReturnedExecChildObservation ZrTests_ReturnedExec_ChildObservation(void);
typedef struct SZrReturnedExecTeardown {
    int beginResult,vmResult;
    SZrReturnedVmStats vm;
    unsigned observerErrors;
    SZrReturnedExecCleanupReport ledgers;
} SZrReturnedExecTeardown;
/* Close always performs real shutdown and ledger recovery before returning;
 * it makes no Unity assertion and retains the first report on repeated calls.
 * Completion is distinct from pre-recovery health and observer/query errors.
 * The borrowed witness spans remain readable until a verified next Init.
 * Init checks that first report and every witness before any ledger reset.
 * Compile/Open/Name remain asserting interfaces; prior-failed Source entry is
 * not supported. Callback graphs must already be genuine live readable types;
 * TryCapture geometry does not certify pointer authenticity or Source identity.
 */
SZrReturnedExecTeardown ZrTests_ReturnedExec_Teardown(void);
void ZrTests_ReturnedExec_Init(void);
void ZrTests_ReturnedExec_Close(void);
void ZrTests_ReturnedExec_AssertObservation(void);
void ZrTests_ReturnedExec_AssertProducerReleased(void);
void ZrTests_ReturnedExec_AssertView(const SZrReturnedExecObservation *observed,const SZrExecIrModule *view);
void ZrTests_ReturnedExec_Fault(unsigned index);
SZrFunction *ZrTests_ReturnedExec_Compile(unsigned slot,const char *source,TZrBool ordinary,const SZrSourceCompileOptions *options,SZrSourceCompileDiagnostic *diagnostic);
SZrReturnedExecObservation ZrTests_ReturnedExec_Observation(void);
TZrBool ZrTests_ReturnedExec_AppendLayout(SZrExecIrModule *module,const SZrExecIrLayout *layout,TZrSize count,SZrExecIrRange *range);
TZrBool ZrTests_ReturnedExec_Snapshot(SZrFunction *function,const SZrExecIrModule *source,SZrExecIrDiagnostic *diagnostic);
#endif
