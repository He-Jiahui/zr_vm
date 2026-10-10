#include "ssa_source_returned_execir_cleanup.h"
#include <stdint.h>
#include <string.h>

static int storage_valid(const SZrReturnedExecCleanupStorage *storage,
        const SZrReturnedHeapStats crt[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS],
        SZrOwnerHeapStats owner) {
    if (storage == NULL || storage->owner == NULL || storage->ownerCapacity == 0u ||
        sizeof(*storage->owner) > SIZE_MAX / (size_t)storage->ownerCapacity ||
        owner.records > storage->ownerCapacity) return 0;
    for (unsigned k = 0u; k < ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS; ++k) {
        if (storage->crt[k] == NULL || storage->crtCapacity[k] == 0u ||
            sizeof(*storage->crt[k]) > SIZE_MAX / (size_t)storage->crtCapacity[k] ||
            crt[k].successes > storage->crtCapacity[k] ||
            storage->crt[k] == storage->owner) return 0;
    }
    return storage->crt[0] != storage->crt[1];
}

int ZrTests_ReturnedExec_CleanupBegin(SZrReturnedExecCleanupReport *report,
        const SZrReturnedExecCleanupStorage *storage, int priorWitnessesVerified) {
    SZrReturnedHeapStats crt[ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS];
    SZrOwnerHeapStats owner;
    SZrReturnedExecCleanupReport next = {0};
    if (report == NULL || priorWitnessesVerified != 1 ||
        (report->begun != 0u && report->closed != 1u)) return 0;
    for (unsigned k = 0u; k < ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS; ++k) {
        if (ZrTests_Returned_TryStats(k, &crt[k]) != 1 || crt[k].live != 0u) return 0;
    }
    owner = ZrTests_ExecOwner_Stats();
    if (owner.live != 0u || !storage_valid(storage, crt, owner)) return 0;
    next.begun = 1u;
    next.storage = *storage;
    *report = next;
    return 1;
}

static void capture_record(SZrReturnedExecCleanupReport *report,
        SZrReturnedExecCleanupRecord *record, int after, int result, unsigned frees) {
    if (result != 1) {
        ++report->refusedQueries;
        report->healthyBeforeRecovery = 0u;
    } else if (frees != 1u) {
        report->healthyBeforeRecovery = 0u;
        if (after) ++report->observationErrors;
    }
    if (record == NULL) return;
    if (after) {
        record->afterResult = result;
        record->afterFrees = frees;
    } else {
        *record = (SZrReturnedExecCleanupRecord){result, 0, frees, 0u};
    }
}

static void capture_records(SZrReturnedExecCleanupReport *report, int after, int writable) {
    for (unsigned k = 0u; k < ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS; ++k) {
        unsigned count = after ? report->crtAfter[k].successes : report->crtBefore[k].successes;
        for (unsigned i = 0u; i < count; ++i) {
            unsigned frees = 0u;
            int result = ZrTests_Returned_TryRecordFrees(k, i, &frees);
            SZrReturnedExecCleanupRecord *record = writable ? &report->storage.crt[k][i] : NULL;
            capture_record(report, record, after, result, frees);
        }
    }
    unsigned count = after ? report->ownerAfter.records : report->ownerBefore.records;
    for (unsigned i = 0u; i < count; ++i) {
        unsigned frees = 0u;
        int result = ZrTests_ExecOwner_TryRecordFrees(i, &frees);
        SZrReturnedExecCleanupRecord *record = writable ? &report->storage.owner[i] : NULL;
        capture_record(report, record, after, result, frees);
    }
}

void ZrTests_ReturnedExec_CleanupLedgers(SZrReturnedExecCleanupReport *report) {
    int beforeWritable = 0;
    if (report != NULL && report->begun == 1u && report->closed == 1u) return;
    ZrTests_ReturnedCrt_Disarm();
    ZrTests_ExecOwner_Disarm();
    if (report != NULL && report->begun == 1u) {
        report->healthyBeforeRecovery = 1u;
        report->refusedQueries = 0u;
        report->observationErrors = 0u;
        for (unsigned k = 0u; k < ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS; ++k) {
            /* Literal kinds admit the existing raw copy even if TryStats refuses.
             * The actual public result remains separate evidence, never invented.
             */
            report->crtBefore[k] = ZrTests_ReturnedCrt_Stats(k);
            report->crtBeforeResult[k] = ZrTests_Returned_TryStats(k, &report->crtBefore[k]);
            report->crtRecordCount[k] = report->crtBefore[k].successes;
            if (report->crtBeforeResult[k] != 1) {
                ++report->refusedQueries;
                report->healthyBeforeRecovery = 0u;
            }
            if (report->crtBefore[k].errors != 0u || report->crtBefore[k].live != 0u ||
                report->crtBefore[k].frees != report->crtRecordCount[k])
                report->healthyBeforeRecovery = 0u;
        }
        report->ownerBefore = ZrTests_ExecOwner_Stats();
        report->ownerRecordCount = report->ownerBefore.records;
        if (report->ownerBefore.errors != 0u || report->ownerBefore.live != 0u ||
            report->ownerBefore.frees != report->ownerRecordCount)
            report->healthyBeforeRecovery = 0u;
        beforeWritable = storage_valid(&report->storage, report->crtBefore, report->ownerBefore);
        if (!beforeWritable) {
            ++report->observationErrors;
            report->healthyBeforeRecovery = 0u;
        }
        capture_records(report, 0, beforeWritable);
    }
    ZrTests_ReturnedCrt_Recover();
    ZrTests_ExecOwner_Recover();
    if (report != NULL && report->begun == 1u) {
        int sameCounts = 1;
        for (unsigned k = 0u; k < ZR_RETURNED_EXEC_CLEANUP_CRT_KINDS; ++k) {
            report->crtAfter[k] = ZrTests_ReturnedCrt_Stats(k);
            report->crtAfterResult[k] = ZrTests_Returned_TryStats(k, &report->crtAfter[k]);
            if (report->crtAfterResult[k] != 1) {
                ++report->refusedQueries;
                report->healthyBeforeRecovery = 0u;
            }
            if (report->crtAfter[k].successes != report->crtRecordCount[k]) sameCounts = 0;
            if (report->crtAfter[k].live != 0u ||
                report->crtAfter[k].frees != report->crtAfter[k].successes) {
                ++report->observationErrors;
                report->healthyBeforeRecovery = 0u;
            }
        }
        report->ownerAfter = ZrTests_ExecOwner_Stats();
        if (report->ownerAfter.records != report->ownerRecordCount) sameCounts = 0;
        if (report->ownerAfter.live != 0u || report->ownerAfter.frees != report->ownerAfter.records) {
            ++report->observationErrors;
            report->healthyBeforeRecovery = 0u;
        }
        int afterWritable = beforeWritable && sameCounts &&
            storage_valid(&report->storage, report->crtAfter, report->ownerAfter);
        if (!afterWritable) {
            ++report->observationErrors;
            report->healthyBeforeRecovery = 0u;
        }
        capture_records(report, 1, afterWritable);
        report->closed = 1u;
        report->completed = 1u;
    }
}
