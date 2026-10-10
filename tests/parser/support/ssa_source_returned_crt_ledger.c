#include "ssa_source_returned_crt_ledger.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "zr_vm_core/aot_ir.h"

enum { CRT_ROWS = ZR_RETURNED_CRT_ROWS };
typedef struct SCrtRow { void *pointer; size_t bytes; unsigned frees; } SCrtRow;
typedef struct SCrtLedger {
    SCrtRow rows[CRT_ROWS]; SZrReturnedHeapEvent events[CRT_ROWS];
    SZrReturnedHeapStats stats;
    unsigned failAt, callBase;
} SCrtLedger;
static SCrtLedger g_crt[2];
static const size_t g_counts[6] = {3u, 1u, 1u, 3u, 1u, 1u};
static const size_t g_widths[6] = {sizeof(SZrAotIrInstruction), sizeof(SZrAotIrBlock),
    sizeof(SZrAotIrFrameSlot), sizeof(SZrAotIrSourceMap), sizeof(SZrExecIrConstant), sizeof(SZrExecIrLayout)};

static void *crt_allocate(unsigned kind, size_t count, size_t width) {
    SCrtLedger *l = &g_crt[kind];
    unsigned site = l->stats.attempts - l->callBase;
    SZrReturnedHeapEvent *e;
    SCrtRow *r;
    void *pointer;
    if (l->stats.attempts == CRT_ROWS || count == 0u || width == 0u || count > SIZE_MAX / width ||
        (kind == ZR_RETURNED_DESCRIPTOR && (site >= 6u || count != g_counts[site] || width != g_widths[site]))) {
        ++l->stats.errors; return ZR_NULL;
    }
    e = &l->events[l->stats.attempts++]; e->count = count; e->width = width;
    e->bytes = count * width; e->site = site + 1u; e->record = CRT_ROWS;
    if (l->failAt == l->stats.attempts) {
        e->injected = ZR_TRUE; ++l->stats.failures; return ZR_NULL;
    }
    if (l->stats.successes == CRT_ROWS) { ++l->stats.errors; return ZR_NULL; }
    pointer = kind == ZR_RETURNED_DESCRIPTOR ? calloc(count, width) : malloc(e->bytes);
    if (pointer == ZR_NULL) { ++l->stats.errors; return ZR_NULL; }
    e->record = l->stats.successes;
    r = &l->rows[l->stats.successes++]; r->pointer = pointer; r->bytes = e->bytes;
    ++l->stats.live; return pointer;
}
static void crt_free(unsigned kind, void *pointer) {
    SCrtLedger *l = &g_crt[kind];
    if (pointer == ZR_NULL) return;
    for (unsigned i = 0u; i < l->stats.successes; ++i) {
        SCrtRow *r = &l->rows[i];
        if (r->pointer != ZR_NULL && r->pointer == pointer) {
            r->pointer = ZR_NULL; ++r->frees; ++l->stats.frees; --l->stats.live;
            free(pointer); return;
        }
    }
    ++l->stats.errors;
}
void *ZrTests_ReturnedDescriptor_Calloc(size_t count, size_t width) { return crt_allocate(0u, count, width); }
void ZrTests_ReturnedDescriptor_Free(void *pointer) { crt_free(0u, pointer); }
void *ZrTests_ReturnedOwner_Malloc(size_t bytes) { return crt_allocate(1u, 1u, bytes); }
void ZrTests_ReturnedOwner_Free(void *pointer) { crt_free(1u, pointer); }

SZrReturnedHeapStats ZrTests_ReturnedCrt_Stats(unsigned kind) { return g_crt[kind].stats; }
int ZrTests_Returned_TryStats(unsigned kind, SZrReturnedHeapStats *output) {
    if (output == ZR_NULL || kind >= 2u) return 0;
    *output = g_crt[kind].stats;
    return 1;
}
int ZrTests_Returned_TryRecordFrees(unsigned kind, unsigned record, unsigned *output) {
    if (output == ZR_NULL || kind >= 2u ||
        record >= g_crt[kind].stats.successes || record >= CRT_ROWS) return 0;
    *output = g_crt[kind].rows[record].frees;
    return 1;
}
int ZrTests_ReturnedCrt_TryEvent(unsigned kind, unsigned index, SZrReturnedHeapEvent *output) {
    if (output == ZR_NULL || kind >= 2u ||
        index >= g_crt[kind].stats.attempts || index >= CRT_ROWS) return 0;
    *output = g_crt[kind].events[index];
    return 1;
}
int ZrTests_ReturnedCrt_TryFindLiveRecord(unsigned kind, const void *pointer, unsigned *output) {
    if (output == ZR_NULL || kind >= 2u || pointer == ZR_NULL ||
        g_crt[kind].stats.successes > CRT_ROWS) return 0;
    for (unsigned i = 0u; i < g_crt[kind].stats.successes; ++i)
        if (g_crt[kind].rows[i].pointer != ZR_NULL && g_crt[kind].rows[i].pointer == pointer) {
            *output = i;
            return 1;
        }
    return 0;
}
void ZrTests_ReturnedCrt_Reset(void) { memset(g_crt, 0, sizeof(g_crt)); }
void ZrTests_ReturnedCrt_BeginCall(unsigned descriptorSite, TZrBool ownerFault) {
    for (unsigned i = 0u; i < 2u; ++i) g_crt[i].callBase = g_crt[i].stats.attempts;
    if (descriptorSite != 0u) g_crt[0].failAt = g_crt[0].stats.attempts + descriptorSite;
    if (ownerFault) g_crt[1].failAt = g_crt[1].stats.attempts + 1u;
}
void ZrTests_ReturnedCrt_Disarm(void) { g_crt[0].failAt = 0u; g_crt[1].failAt = 0u; }
void ZrTests_ReturnedCrt_Recover(void) {
    for (unsigned k = 0u; k < 2u; ++k) {
        if (g_crt[k].stats.live != 0u) ++g_crt[k].stats.errors;
        for (unsigned i = 0u; i < g_crt[k].stats.successes; ++i)
            if (g_crt[k].rows[i].pointer != ZR_NULL) crt_free(k, g_crt[k].rows[i].pointer);
    }
}
