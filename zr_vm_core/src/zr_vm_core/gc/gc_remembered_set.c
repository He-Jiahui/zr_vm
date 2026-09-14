#include "zr_vm_core/gc_young_allocation.h"

#include <stdint.h>
#include <string.h>

static void gc_young_set_diagnostic_local(
        SZrGcYoungDiagnostic *diagnostic,
        EZrGcYoungDiagnosticCode code,
        TZrUInt32 field,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
}

static TZrBool gc_young_generation_valid(
        EZrGarbageCollectHeapGenerationKind generation) {
    return generation == ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG ||
           generation == ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD ||
           generation == ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_PERMANENT;
}

static TZrBool gc_young_card_range_valid(
        const SZrGcCardTable *table,
        const TZrByte *address,
        TZrSize size,
        TZrSize *firstCard,
        TZrSize *lastCard) {
    uintptr_t heapStart;
    uintptr_t storeStart;
    uintptr_t storeEnd;
    TZrUInt64 offset;

    if (table == ZR_NULL || address == ZR_NULL || size == 0u ||
        table->heapBegin == ZR_NULL || table->heapBytes == 0u ||
        table->cardCount == 0u) {
        return ZR_FALSE;
    }
    heapStart = (uintptr_t)(const void *)table->heapBegin;
    storeStart = (uintptr_t)(const void *)address;
    if (storeStart < heapStart ||
        storeStart - heapStart >= (uintptr_t)table->heapBytes ||
        size > (TZrSize)(UINTPTR_MAX - storeStart)) {
        return ZR_FALSE;
    }
    storeEnd = storeStart + (uintptr_t)size - 1u;
    if (storeEnd < storeStart ||
        storeEnd - heapStart >= (uintptr_t)table->heapBytes) {
        return ZR_FALSE;
    }
    offset = (TZrUInt64)(storeStart - heapStart);
    *firstCard = (TZrSize)(offset >> ZR_GC_CARD_SHIFT);
    *lastCard = (TZrSize)(((TZrUInt64)(storeEnd - heapStart)) >> ZR_GC_CARD_SHIFT);
    return *firstCard <= *lastCard && *lastCard < table->cardCount;
}

TZrBool ZrCore_GcCardTable_Init(
        SZrGcCardTable *table,
        TZrByte *cards,
        TZrSize cardCount,
        TZrByte *heapBegin,
        TZrSize heapBytes) {
    TZrSize requiredCardCount;

    if (table == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(table, 0, sizeof(*table));
    if (cards == ZR_NULL || cardCount == 0u || heapBegin == ZR_NULL || heapBytes == 0u) {
        return ZR_FALSE;
    }
    if (heapBytes > SIZE_MAX - (ZR_GC_CARD_BYTES - 1u)) {
        return ZR_FALSE;
    }
    requiredCardCount = (heapBytes + (ZR_GC_CARD_BYTES - 1u)) / ZR_GC_CARD_BYTES;
    if (cardCount < requiredCardCount) {
        return ZR_FALSE;
    }
    table->cards = cards;
    table->cardCount = cardCount;
    table->heapBegin = heapBegin;
    table->heapBytes = heapBytes;
    table->dirtyCount = 0u;
    memset(cards, ZR_GC_CARD_CLEAN, cardCount);
    return ZR_TRUE;
}

TZrBool ZrCore_GcCardTable_Validate(
        const SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize requiredCardCount;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (table == ZR_NULL || table->cards == ZR_NULL || table->cardCount == 0u ||
        table->heapBegin == ZR_NULL || table->heapBytes == 0u) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (table->heapBytes > SIZE_MAX - (ZR_GC_CARD_BYTES - 1u)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                      3u, SIZE_MAX, table->heapBytes);
        return ZR_FALSE;
    }
    requiredCardCount = (table->heapBytes + (ZR_GC_CARD_BYTES - 1u)) / ZR_GC_CARD_BYTES;
    if (table->cardCount < requiredCardCount || table->dirtyCount > table->cardCount) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
                                      1u, (TZrUInt64)requiredCardCount,
                                      (TZrUInt64)table->cardCount);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_GcCardTable_RecordStore(
        SZrGcCardTable *table,
        TZrByte *address,
        TZrSize size,
        EZrGarbageCollectHeapGenerationKind ownerGeneration,
        EZrGarbageCollectHeapGenerationKind valueGeneration,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize firstCard;
    TZrSize lastCard;
    TZrSize card;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (!ZrCore_GcCardTable_Validate(table, diagnostic) ||
        !gc_young_generation_valid(ownerGeneration) ||
        !gc_young_generation_valid(valueGeneration)) {
        if (diagnostic != ZR_NULL && diagnostic->code == ZR_GC_YOUNG_DIAGNOSTIC_NONE) {
            gc_young_set_diagnostic_local(diagnostic,
                                          ZR_GC_YOUNG_DIAGNOSTIC_INVALID_GENERATION,
                                          3u, 1u, 0u);
        }
        return ZR_FALSE;
    }
    if (size == 0u) {
        return ZR_TRUE;
    }
    if (!gc_young_card_range_valid(table, address, size, &firstCard, &lastCard)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
                                      1u, table->heapBytes, (TZrUInt64)size);
        return ZR_FALSE;
    }
    if ((ownerGeneration != ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_OLD &&
         ownerGeneration != ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_PERMANENT) ||
        valueGeneration != ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG) {
        return ZR_TRUE;
    }
    for (card = firstCard; card <= lastCard; ++card) {
        if (table->cards[card] == ZR_GC_CARD_CLEAN) {
            table->cards[card] = ZR_GC_CARD_DIRTY;
            if (table->dirtyCount == SIZE_MAX) {
                table->cards[card] = ZR_GC_CARD_CLEAN;
                gc_young_set_diagnostic_local(diagnostic,
                                              ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                              4u, SIZE_MAX - 1u, SIZE_MAX);
                return ZR_FALSE;
            }
            table->dirtyCount++;
        }
        if (card == SIZE_MAX) {
            break;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_GcCardTable_IsDirty(
        const SZrGcCardTable *table,
        TZrSize cardIndex) {
    return (TZrBool)(table != ZR_NULL && table->cards != ZR_NULL &&
                     cardIndex < table->cardCount &&
                     table->cards[cardIndex] == ZR_GC_CARD_DIRTY);
}

TZrSize ZrCore_GcCardTable_DirtyCardCount(
        const SZrGcCardTable *table) {
    return table != ZR_NULL ? table->dirtyCount : 0u;
}

TZrBool ZrCore_GcCardTable_Clear(
        SZrGcCardTable *table,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (!ZrCore_GcCardTable_Validate(table, diagnostic)) {
        return ZR_FALSE;
    }
    memset(table->cards, ZR_GC_CARD_CLEAN, table->cardCount);
    table->dirtyCount = 0u;
    return ZR_TRUE;
}

static TZrBool gc_young_remembered_root_equal(
        const SZrGcRememberedRoot *left,
        EZrGcRememberedRootKind kind,
        TZrUInt64 token) {
    return left != ZR_NULL && left->kind == kind && left->token == token;
}

static TZrBool gc_young_remembered_root_valid_kind(
        EZrGcRememberedRootKind kind) {
    return kind == ZR_GC_REMEMBERED_ROOT_CARD ||
           kind == ZR_GC_REMEMBERED_ROOT_OBJECT;
}

TZrBool ZrCore_GcRememberedRootSet_Init(
        SZrGcRememberedRootSet *set,
        SZrGcRememberedRoot *entries,
        TZrSize capacity,
        TZrUInt32 epoch) {
    if (set == ZR_NULL || entries == ZR_NULL || capacity == 0u ||
        capacity > SIZE_MAX / sizeof(*entries)) {
        return ZR_FALSE;
    }
    memset(set, 0, sizeof(*set));
    memset(entries, 0, capacity * sizeof(*entries));
    set->entries = entries;
    set->capacity = capacity;
    set->epoch = epoch;
    return ZR_TRUE;
}

static TZrBool gc_young_remembered_root_record(
        SZrGcRememberedRootSet *set,
        EZrGcRememberedRootKind kind,
        TZrUInt64 token,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize index;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (set == ZR_NULL || set->entries == ZR_NULL || set->capacity == 0u ||
        set->count > set->capacity ||
        !gc_young_remembered_root_valid_kind(kind)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < set->count; ++index) {
        if (gc_young_remembered_root_equal(&set->entries[index], kind, token)) {
            return ZR_TRUE;
        }
    }
    if (set->count == set->capacity) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
                                      1u, (TZrUInt64)set->capacity,
                                      (TZrUInt64)set->count + 1u);
        return ZR_FALSE;
    }
    set->entries[set->count].kind = kind;
    set->entries[set->count].token = token;
    set->count++;
    return ZR_TRUE;
}

TZrBool ZrCore_GcRememberedRootSet_RecordCard(
        SZrGcRememberedRootSet *set,
        TZrSize cardIndex,
        SZrGcYoungDiagnostic *diagnostic) {
    return gc_young_remembered_root_record(
            set, ZR_GC_REMEMBERED_ROOT_CARD, (TZrUInt64)cardIndex, diagnostic);
}

TZrBool ZrCore_GcRememberedRootSet_RecordObject(
        SZrGcRememberedRootSet *set,
        TZrUInt64 objectToken,
        SZrGcYoungDiagnostic *diagnostic) {
    if (objectToken == 0u) {
        ZrCore_GcYoung_DiagnosticClear(diagnostic);
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      1u, 1u, 0u);
        return ZR_FALSE;
    }
    return gc_young_remembered_root_record(
            set, ZR_GC_REMEMBERED_ROOT_OBJECT, objectToken, diagnostic);
}

TZrBool ZrCore_GcRememberedRootSet_ScanNext(
        const SZrGcRememberedRootSet *set,
        TZrSize *cursor,
        SZrGcRememberedRoot *root,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (set == ZR_NULL || set->entries == ZR_NULL || cursor == ZR_NULL || root == ZR_NULL) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (*cursor >= set->count) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_END_OF_SCAN,
                                      1u, (TZrUInt64)set->count,
                                      (TZrUInt64)*cursor);
        return ZR_FALSE;
    }
    *root = set->entries[*cursor];
    (*cursor)++;
    return ZR_TRUE;
}

TZrBool ZrCore_GcRememberedRootSet_Clear(
        SZrGcRememberedRootSet *set,
        SZrGcYoungDiagnostic *diagnostic) {
    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (set == ZR_NULL || set->entries == ZR_NULL || set->capacity == 0u ||
        set->capacity > SIZE_MAX / sizeof(*set->entries)) {
        gc_young_set_diagnostic_local(diagnostic,
                                      ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                      0u, 1u, 0u);
        return ZR_FALSE;
    }
    memset(set->entries, 0, set->capacity * sizeof(*set->entries));
    set->count = 0u;
    return ZR_TRUE;
}
