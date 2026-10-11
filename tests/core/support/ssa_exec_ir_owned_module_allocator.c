#define ZR_TEST_EXEC_IR_OWNER_NO_REDIRECT
#include "ssa_exec_ir_owned_module_shim.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "unity.h"
enum { OWNER_RECORDS = 32768, OWNER_EVENTS = 65536 };
typedef struct SOwnerHeapRecord { void *pointer; size_t bytes; unsigned kind, frees; } SOwnerHeapRecord;
static SOwnerHeapRecord g_rows[OWNER_RECORDS];
static SZrOwnerHeapEvent g_events[OWNER_EVENTS];
static SZrOwnerHeapStats g_stats;
static unsigned g_failAt;
static unsigned find_row(void *pointer) {
    for (unsigned i = 0u; i < g_stats.records; ++i)
        if (g_rows[i].pointer != NULL && g_rows[i].pointer == pointer) return i;
    return OWNER_RECORDS;
}
static SZrOwnerHeapEvent *request(unsigned kind, size_t count, size_t width, unsigned row) {
    SZrOwnerHeapEvent *event;
    if (g_stats.attempts == OWNER_EVENTS || count == 0u || width == 0u || count > SIZE_MAX / width) {
        ++g_stats.errors; return NULL;
    }
    event = &g_events[g_stats.attempts++];
    event->kind = kind; event->count = count; event->width = width;
    event->bytes = count * width; event->record = row;
    if (g_failAt == g_stats.attempts) { event->failed = 1u; ++g_stats.failures; }
    return event;
}
static void *allocate(unsigned kind, size_t count, size_t width) {
    SZrOwnerHeapEvent *event = request(kind, count, width, OWNER_RECORDS);
    void *pointer;
    if (event == NULL || event->failed) return NULL;
    if (g_stats.records == OWNER_RECORDS) { ++g_stats.errors; return NULL; }
    pointer = kind == 2u ? calloc(count, width) : malloc(event->bytes);
    if (pointer == NULL) { ++g_stats.errors; return NULL; }
    event->record = g_stats.records;
    g_rows[g_stats.records++] = (SOwnerHeapRecord){pointer, event->bytes, kind, 0u};
    ++g_stats.live; return pointer;
}
void *ZrTests_ExecOwner_Malloc(size_t bytes) { return allocate(1u, 1u, bytes); }
void *ZrTests_ExecOwner_Calloc(size_t count, size_t width) { return allocate(2u, count, width); }
void *ZrTests_ExecOwner_Realloc(void *pointer, size_t bytes) {
    const int wasNew = pointer == NULL;
    unsigned row = wasNew ? OWNER_RECORDS : find_row(pointer);
    SZrOwnerHeapEvent *event;
    void *replacement;
    if (!wasNew && row == OWNER_RECORDS) { ++g_stats.errors; return NULL; }
    event = request(3u, 1u, bytes, row);
    if (event == NULL || event->failed) return NULL;
    if (wasNew && g_stats.records == OWNER_RECORDS) { ++g_stats.errors; return NULL; }
    replacement = realloc(pointer, bytes);
    if (replacement == NULL) { ++g_stats.errors; return NULL; }
    if (wasNew) { row = g_stats.records++; ++g_stats.live; }
    g_rows[row].pointer = replacement; g_rows[row].bytes = bytes; g_rows[row].kind = 3u;
    event->record = row; return replacement;
}
void ZrTests_ExecOwner_Free(void *pointer) {
    unsigned row;
    if (pointer == NULL) return;
    row = find_row(pointer);
    if (row == OWNER_RECORDS) { ++g_stats.errors; return; }
    g_rows[row].pointer = NULL; ++g_rows[row].frees; ++g_stats.frees; --g_stats.live;
    free(pointer);
}
void ZrTests_ExecOwner_Reset(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_stats.live); TEST_ASSERT_EQUAL_UINT(0u, g_failAt);
    memset(g_rows, 0, sizeof(g_rows)); memset(g_events, 0, sizeof(g_events)); memset(&g_stats, 0, sizeof(g_stats));
}
void ZrTests_ExecOwner_Arm(unsigned relativeAttempt) {
    TEST_ASSERT_EQUAL_UINT(0u, g_failAt); TEST_ASSERT_TRUE(relativeAttempt > 0u);
    TEST_ASSERT_TRUE(relativeAttempt <= OWNER_EVENTS - g_stats.attempts);
    g_failAt = g_stats.attempts + relativeAttempt;
}
void ZrTests_ExecOwner_Disarm(void) { g_failAt = 0u; }
SZrOwnerHeapStats ZrTests_ExecOwner_Stats(void) { return g_stats; }
SZrOwnerHeapEvent ZrTests_ExecOwner_Event(unsigned index) {
    TEST_ASSERT_LESS_THAN_UINT(g_stats.attempts, index); return g_events[index];
}
int ZrTests_ExecOwner_TryArm(unsigned relativeAttempt) {
    if (g_failAt != 0u || relativeAttempt == 0u || g_stats.attempts > OWNER_EVENTS) return 0;
    if (relativeAttempt > OWNER_EVENTS - g_stats.attempts) return 0;
    g_failAt = g_stats.attempts + relativeAttempt;
    return 1;
}
int ZrTests_ExecOwner_TryEvent(unsigned index, SZrOwnerHeapEvent *output) {
    if (output == NULL || index >= g_stats.attempts || index >= OWNER_EVENTS) return 0;
    *output = g_events[index];
    return 1;
}
unsigned ZrTests_ExecOwner_RecordFrees(unsigned record) {
    TEST_ASSERT_LESS_THAN_UINT(g_stats.records, record); return g_rows[record].frees;
}
int ZrTests_ExecOwner_TryRecordFrees(unsigned record, unsigned *frees) {
    if (frees == NULL || record >= g_stats.records || record >= OWNER_RECORDS) return 0;
    *frees = g_rows[record].frees;
    return 1;
}
int ZrTests_ExecOwner_TryLiveSpan(unsigned first, unsigned last, const void *pointer,
                               size_t count, size_t width, SZrOwnerLiveSpan *output) {
    size_t bytes;
    uintptr_t start, end;
    unsigned found = OWNER_RECORDS;
    SZrOwnerLiveSpan candidate = {0};
    if (output == NULL || pointer == NULL || count == 0u || width == 0u ||
        g_stats.records > OWNER_RECORDS || first >= last || last > g_stats.records)
        return 0;
    if (count > SIZE_MAX / width) return 0;
    bytes = count * width;
    if ((size_t)(uintptr_t)bytes != bytes) return 0;
    start = (uintptr_t)pointer;
    if ((const void *)start != pointer || start > UINTPTR_MAX - (uintptr_t)bytes)
        return 0;
    end = start + (uintptr_t)bytes;
    for (unsigned i = 0u; i < g_stats.records; ++i) {
        const SOwnerHeapRecord *row = &g_rows[i];
        uintptr_t rowStart, rowEnd;
        if (row->pointer == NULL) continue;
        if (row->frees != 0u || row->bytes == 0u ||
            (size_t)(uintptr_t)row->bytes != row->bytes)
            return 0;
        rowStart = (uintptr_t)row->pointer;
        if ((void *)rowStart != row->pointer || rowStart > UINTPTR_MAX - (uintptr_t)row->bytes)
            return 0;
        rowEnd = rowStart + (uintptr_t)row->bytes;
        if (start >= rowStart && end <= rowEnd) {
            if (found != OWNER_RECORDS) return 0;
            found = i;
        }
    }
    if (found == OWNER_RECORDS || found < first || found >= last) return 0;
    candidate.record = found;
    candidate.allocation = g_rows[found].pointer;
    candidate.bytes = g_rows[found].bytes;
    *output = candidate;
    return 1;
}
void ZrTests_ExecOwner_AssertReleased(unsigned first, unsigned last) {
    TEST_ASSERT_TRUE(first <= last && last <= g_stats.records);
    for (unsigned i = first; i < last; ++i) {
        TEST_ASSERT_NULL(g_rows[i].pointer); TEST_ASSERT_EQUAL_UINT(1u, g_rows[i].frees);
    }
}
void ZrTests_ExecOwner_AssertLive(unsigned first, unsigned last) {
    TEST_ASSERT_TRUE(first <= last && last <= g_stats.records);
    for (unsigned i = first; i < last; ++i) {
        TEST_ASSERT_NOT_NULL(g_rows[i].pointer); TEST_ASSERT_EQUAL_UINT(0u, g_rows[i].frees);
    }
}
void ZrTests_ExecOwner_Recover(void) {
    ZrTests_ExecOwner_Disarm();
    if (g_stats.live != 0u) {
        ++g_stats.errors;
    }
    if (g_stats.errors != 0u) fprintf(stderr, "EXEC_OWNER_LEDGER errors=%u live=%u\n", g_stats.errors,g_stats.live);
    for (unsigned i = 0u; i < g_stats.records; ++i)
        if (g_rows[i].pointer != NULL) ZrTests_ExecOwner_Free(g_rows[i].pointer);
}
void ZrTests_ExecOwner_AssertBalanced(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_failAt); TEST_ASSERT_EQUAL_UINT(0u, g_stats.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_stats.errors); TEST_ASSERT_EQUAL_UINT(g_stats.records, g_stats.frees);
    ZrTests_ExecOwner_AssertReleased(0u, g_stats.records);
}
