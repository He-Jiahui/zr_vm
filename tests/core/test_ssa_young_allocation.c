#include "zr_vm_core/gc_young_allocation.h"

#include <assert.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

static void test_tlab_fast_path_aligns_zeroes_and_rejects_exhaustion(void) {
    alignas(64) TZrByte storage[64];
    SZrGcTlab tlab;
    SZrGcYoungDiagnostic diagnostic;
    TZrByte *first;
    TZrByte *second;
    TZrByte *cursorBefore;

    memset(storage, 0xa5, sizeof(storage));
    ZrCore_GcTlab_Init(&tlab, storage, sizeof(storage), 7u, 3u);
    assert(ZrCore_GcTlab_Validate(&tlab, &diagnostic));

    first = (TZrByte *)ZrCore_GcTlab_AllocateFast(
            &tlab, 3u, 8u, &diagnostic);
    assert(first == storage);
    assert(((uintptr_t)first % 8u) == 0u);
    assert(first[0] == 0u && first[2] == 0u);

    second = (TZrByte *)ZrCore_GcTlab_AllocateFast(
            &tlab, 9u, 16u, &diagnostic);
    assert(second != NULL);
    assert(((uintptr_t)second % 16u) == 0u);
    assert(second >= storage && second + 9u <= storage + sizeof(storage));
    assert(second[0] == 0u && second[8] == 0u);

    cursorBefore = tlab.cursor;
    assert(ZrCore_GcTlab_AllocateFast(
                   &tlab, sizeof(storage), 8u, &diagnostic) == NULL);
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_TLAB_EXHAUSTED);
    assert(tlab.cursor == cursorBefore);
}

static void test_tlab_refill_requires_safepoint_and_retire_counts_waste(void) {
    TZrByte firstStorage[32];
    TZrByte secondStorage[32];
    SZrGcTlab tlab;
    SZrGcYoungDiagnostic diagnostic;

    ZrCore_GcTlab_Init(&tlab, firstStorage, sizeof(firstStorage), 1u, 1u);
    assert(ZrCore_GcTlab_AllocateFast(&tlab, 8u, 8u, &diagnostic) != NULL);
    assert(!ZrCore_GcTlab_Refill(
            &tlab, secondStorage, sizeof(secondStorage), 2u, 1u,
            ZR_FALSE, &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_SAFEPOINT_REQUIRED);
    assert(tlab.regionId == 1u);

    assert(ZrCore_GcTlab_Retire(&tlab, &diagnostic));
    assert(tlab.retiredBytes == sizeof(firstStorage));
    assert(tlab.wasteBytes == sizeof(firstStorage) - 8u);
    assert(ZrCore_GcTlab_Refill(
            &tlab, secondStorage, sizeof(secondStorage), 2u, 1u,
            ZR_TRUE, &diagnostic));
    assert(tlab.regionId == 2u);
    assert(tlab.cursor == secondStorage);
    assert(tlab.limit == secondStorage + sizeof(secondStorage));
}

static void test_card_table_marks_old_to_young_and_remembered_roots(void) {
    alignas(64) TZrByte heap[1024];
    TZrByte cards[2] = {ZR_GC_CARD_CLEAN, ZR_GC_CARD_CLEAN};
    SZrGcRememberedRoot roots[4];
    SZrGcCardTable table;
    SZrGcRememberedRootSet remembered;
    SZrGcYoungDiagnostic diagnostic;
    TZrSize cursor = 0u;
    SZrGcRememberedRoot root;

    ZrCore_GcCardTable_Init(&table, cards, 2u, heap, sizeof(heap));
    assert(ZrCore_GcCardTable_Validate(&table, &diagnostic));
    ZrCore_GcRememberedRootSet_Init(&remembered, roots, 4u, 9u);

    assert(ZrCore_GcCardTable_RecordStore(
            &table, heap + 32u, 8u,
            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD,
            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG,
            &diagnostic));
    assert(ZrCore_GcCardTable_IsDirty(&table, 0u));
    assert(ZrCore_GcCardTable_DirtyCardCount(&table) == 1u);
    assert(ZrCore_GcRememberedRootSet_RecordCard(
            &remembered, 0u, &diagnostic));
    assert(ZrCore_GcRememberedRootSet_RecordCard(
            &remembered, 0u, &diagnostic));
    assert(remembered.count == 1u);

    assert(ZrCore_GcCardTable_RecordStore(
            &table, heap + 40u, 8u,
            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG,
            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG,
            &diagnostic));
    assert(remembered.count == 1u);

    assert(ZrCore_GcRememberedRootSet_RecordObject(
            &remembered, UINT64_C(0x1234), &diagnostic));
    assert(remembered.count == 2u);
    assert(ZrCore_GcRememberedRootSet_ScanNext(
            &remembered, &cursor, &root, &diagnostic));
    assert(root.kind == ZR_GC_REMEMBERED_ROOT_CARD);
    assert(root.token == 0u);
    assert(ZrCore_GcRememberedRootSet_ScanNext(
            &remembered, &cursor, &root, &diagnostic));
    assert(root.kind == ZR_GC_REMEMBERED_ROOT_OBJECT);
    assert(root.token == UINT64_C(0x1234));
    assert(!ZrCore_GcRememberedRootSet_ScanNext(
            &remembered, &cursor, &root, &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_END_OF_SCAN);

    assert(ZrCore_GcCardTable_Clear(&table, &diagnostic));
    assert(ZrCore_GcCardTable_DirtyCardCount(&table) == 0u);
    assert(!ZrCore_GcCardTable_RecordStore(
            &table, heap + sizeof(heap), 1u,
            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD,
            ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG,
            &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS);
}

static void test_promotion_policy_preserves_pin_escape_and_age_reasons(void) {
    SZrGcPromotionRequest request;
    SZrGcPromotionDecision decision;
    SZrGcYoungDiagnostic diagnostic;
    SZrGcYoungAllocationRequest allocation;
    SZrGcYoungAllocationDecision allocationDecision;

    ZrCore_GcPromotionRequest_Init(&request);
    request.objectBytes = 32u;
    request.survivalAge = 0u;
    request.survivorAgeThreshold = 2u;
    assert(ZrCore_GcYoung_DecidePromotion(
            &request, &decision, &diagnostic));
    assert(decision.target == ZR_GC_YOUNG_PROMOTION_SURVIVOR);
    assert(decision.reason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL);

    request.survivalAge = 2u;
    assert(ZrCore_GcYoung_DecidePromotion(
            &request, &decision, &diagnostic));
    assert(decision.target == ZR_GC_YOUNG_PROMOTION_OLD);
    assert(decision.reason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL);

    request.survivalAge = 0u;
    request.pinFlags = ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE;
    assert(ZrCore_GcYoung_DecidePromotion(
            &request, &decision, &diagnostic));
    assert(decision.target == ZR_GC_YOUNG_PROMOTION_PINNED);
    assert(decision.reason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED);

    request.pinFlags = 0u;
    request.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE;
    assert(ZrCore_GcYoung_DecidePromotion(
            &request, &decision, &diagnostic));
    assert(decision.target == ZR_GC_YOUNG_PROMOTION_OLD);
    assert(decision.reason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE);

    request.escapeFlags = 0u;
    request.objectBytes = request.largeObjectThreshold + 1u;
    assert(ZrCore_GcYoung_DecidePromotion(
            &request, &decision, &diagnostic));
    assert(decision.target == ZR_GC_YOUNG_PROMOTION_LARGE);
    assert(decision.reason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_LARGE_OBJECT);

    ZrCore_GcYoungAllocationRequest_Init(&allocation);
    allocation.objectBytes = 16u;
    allocation.tlabAvailableBytes = 32u;
    assert(ZrCore_GcYoung_SelectAllocation(
            &allocation, &allocationDecision, &diagnostic));
    assert(allocationDecision.target == ZR_GC_YOUNG_ALLOCATION_TLAB);
    assert(!allocationDecision.requiresSafepoint);

    allocation.nativeVisible = ZR_TRUE;
    assert(ZrCore_GcYoung_SelectAllocation(
            &allocation, &allocationDecision, &diagnostic));
    assert(allocationDecision.target == ZR_GC_YOUNG_ALLOCATION_PINNED);
    assert(allocationDecision.storageKind ==
           ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED);

    allocation.nativeVisible = ZR_FALSE;
    allocation.objectBytes = allocation.largeObjectThreshold;
    assert(ZrCore_GcYoung_SelectAllocation(
            &allocation, &allocationDecision, &diagnostic));
    assert(allocationDecision.target == ZR_GC_YOUNG_ALLOCATION_LARGE);
    assert(allocationDecision.storageKind ==
           ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT);
}

static void test_minor_transaction_never_resumes_at_partial_region_boundary(void) {
    SZrGcMinorTransaction transaction;
    SZrGcYoungDiagnostic diagnostic;

    ZrCore_GcMinorTransaction_Init(&transaction);
    assert(ZrCore_GcMinorTransaction_Begin(
            &transaction, 2u, 64u, 3u, ZR_TRUE, ZR_TRUE, &diagnostic));
    assert(transaction.phase == ZR_GC_MINOR_PHASE_EVACUATE);
    assert(!ZrCore_GcMinorTransaction_CanResumeMutators(&transaction));

    assert(ZrCore_GcMinorTransaction_EvacuateRegion(
            &transaction, 32u, 1u, &diagnostic));
    assert(transaction.regionCursor == 1u);
    assert(transaction.phase == ZR_GC_MINOR_PHASE_EVACUATE);
    assert(!ZrCore_GcMinorTransaction_CanResumeMutators(&transaction));

    assert(!ZrCore_GcMinorTransaction_EvacuateRegion(
            &transaction, 40u, 1u, &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_TOSPACE_EXHAUSTED);
    assert(transaction.regionCursor == 1u);
    assert(transaction.phase == ZR_GC_MINOR_PHASE_EVACUATE);
    assert(!ZrCore_GcMinorTransaction_CanResumeMutators(&transaction));

    assert(ZrCore_GcMinorTransaction_EvacuateRegion(
            &transaction, 32u, 1u, &diagnostic));
    assert(transaction.regionCursor == 2u);
    assert(transaction.phase == ZR_GC_MINOR_PHASE_REWRITE_REFERENCES);
    assert(ZrCore_GcMinorTransaction_RewriteReferences(
            &transaction, 2u, &diagnostic));
    assert(transaction.phase == ZR_GC_MINOR_PHASE_VERIFY);
    assert(!ZrCore_GcMinorTransaction_VerifyForwarding(
            &transaction, 1u, &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_UNRESOLVED_FORWARDING);
    assert(transaction.phase == ZR_GC_MINOR_PHASE_VERIFY);
    assert(ZrCore_GcMinorTransaction_VerifyForwarding(
            &transaction, 0u, &diagnostic));
    assert(transaction.phase == ZR_GC_MINOR_PHASE_RESUME);
    assert(ZrCore_GcMinorTransaction_CanResumeMutators(&transaction));
    assert(ZrCore_GcMinorTransaction_ResumeMutators(&transaction, &diagnostic));
    assert(transaction.phase == ZR_GC_MINOR_PHASE_COMPLETE);
    assert(transaction.consistentBoundary);
}

static void test_minor_transaction_rejects_invalid_inputs(void) {
    SZrGcMinorTransaction transaction;
    SZrGcYoungDiagnostic diagnostic;

    ZrCore_GcMinorTransaction_Init(&transaction);
    assert(!ZrCore_GcMinorTransaction_Begin(
            &transaction, 0u, 64u, 1u, ZR_TRUE, ZR_TRUE, &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(!ZrCore_GcMinorTransaction_Begin(
            &transaction, 1u, 64u, 1u, ZR_FALSE, ZR_TRUE, &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_MUTATORS_NOT_STOPPED);
    assert(!ZrCore_GcMinorTransaction_Begin(
            &transaction, 1u, 64u, 1u, ZR_TRUE, ZR_FALSE, &diagnostic));
    assert(diagnostic.code == ZR_GC_YOUNG_DIAGNOSTIC_ROOTS_UNAVAILABLE);
}

int main(void) {
    test_tlab_fast_path_aligns_zeroes_and_rejects_exhaustion();
    test_tlab_refill_requires_safepoint_and_retire_counts_waste();
    test_card_table_marks_old_to_young_and_remembered_roots();
    test_promotion_policy_preserves_pin_escape_and_age_reasons();
    test_minor_transaction_never_resumes_at_partial_region_boundary();
    test_minor_transaction_rejects_invalid_inputs();
    return 0;
}
