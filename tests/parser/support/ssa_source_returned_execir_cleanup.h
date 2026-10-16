#ifndef ZR_TEST_SSA_SOURCE_RETURNED_EXECIR_CLEANUP_H
#define ZR_TEST_SSA_SOURCE_RETURNED_EXECIR_CLEANUP_H

#include "ssa_source_returned_crt_ledger.h"
#include "tests/core/support/ssa_exec_ir_owned_module_allocator.h"

enum { ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS = ZR_RETURNED_OWNER + 1 };

typedef struct SZrReturnedExecCleanupRecord {
    int beforeResult, afterResult;
    unsigned beforeFrees, afterFrees;
} SZrReturnedExecCleanupRecord;

/* Genuine independent writable spans, serialized with both real ledgers.
 * Capacities describe caller storage, not an allocator inventory promise.
 */
typedef struct SZrReturnedExecCleanupStorage {
    SZrReturnedExecCleanupRecord *crt[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    unsigned crtCapacity[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    SZrReturnedExecCleanupRecord *owner;
    unsigned ownerCapacity;
} SZrReturnedExecCleanupStorage;

typedef struct SZrReturnedExecCleanupReport {
    unsigned begun, closed, completed, healthyBeforeRecovery;
    unsigned refusedQueries, observationErrors;
    int crtBeforeResult[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    int crtAfterResult[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    SZrReturnedHeapStats crtBefore[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    SZrReturnedHeapStats crtAfter[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    SZrOwnerHeapStats ownerBefore, ownerAfter;
    unsigned crtRecordCount[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    unsigned ownerRecordCount;
    SZrReturnedExecCleanupStorage storage;
} SZrReturnedExecCleanupReport;

/* Caller verifies all previous witnesses before granting exactly one, finishes
 * the prior close, and supplies fresh independent storage. Current live storage,
 * invalid storage, insufficient capacity or an unverified prior lifecycle
 * refuses without modifying the report or any witness. No Reset occurs here.
 */
int ZrTests_ReturnedExec_CleanupBegin(SZrReturnedExecCleanupReport *report,
        const SZrReturnedExecCleanupStorage *storage, int priorWitnessesVerified);

/* Ledger recovery only: no Function/Global/opaque-owner operation or Unity call.
 * closed is an internal once marker, independent of the completion feature.
 * Save actual stats/results and record frees before and after real recovery.
 * Counts identify the before-recovery record ranges. Genuine independent report
 * and storage remain writable until close; invalid geometry leaves storage alone
 * while local queries and recovery continue. Result!=1 makes its frees invalid.
 * completed means recovery and observations finished, even when saved health is
 * false or witnesses were refused. It is set last and never certifies health.
 * Missing report/begin still drains actual ledgers; a closed lifecycle retains
 * its first report unchanged. Output/witness lifetimes extend past cleanup.
 */
void ZrTests_ReturnedExec_CleanupLedgers(SZrReturnedExecCleanupReport *report);

#endif
