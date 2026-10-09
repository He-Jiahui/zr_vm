#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ssa_source_returned_retention_fixture.h"
#include "unity.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"

enum { VM_ROWS = 8192, CRT_ROWS = 256, VM_ERRORS = 64, FUNCTION_SLOTS = 2, NAME_SLOTS = 32 };
typedef struct SVmRow { void *pointer; size_t bytes; TZrInt64 flag; } SVmRow;
typedef struct SVmError {
    unsigned reason, row;
    size_t allocated, original, requested;
    TZrInt64 priorFlag, requestedFlag;
} SVmError;
typedef struct SVmLedger {
    SVmRow rows[VM_ROWS]; SVmError diagnostics[VM_ERRORS];
    unsigned attempts, frees, live, errors, diagnosticCount, dropped;
} SVmLedger;
typedef struct SCrtRow { void *pointer; size_t bytes; unsigned frees; } SCrtRow;
typedef struct SCrtLedger {
    SCrtRow rows[CRT_ROWS]; SZrReturnedHeapEvent events[CRT_ROWS];
    SZrReturnedHeapStats stats;
    unsigned failAt, callBase;
} SCrtLedger;
static SVmLedger g_vm;
static SCrtLedger g_crt[2];
static SZrGlobalState *g_global;
static SZrFunction *g_functions[FUNCTION_SLOTS];
static TZrBool g_functionRooted[FUNCTION_SLOTS];
static SZrString *g_names[NAME_SLOTS];
static TZrBool g_nameRooted[NAME_SLOTS];
static unsigned g_nameCount;
static unsigned g_pendingDescriptor;
static TZrBool g_pendingOwner;
static TZrBool g_savedStopGc, g_haveStopGc;
static const size_t g_counts[6] = {3u, 1u, 1u, 3u, 1u, 1u};
static const size_t g_widths[6] = {sizeof(SZrAotIrInstruction), sizeof(SZrAotIrBlock),
    sizeof(SZrAotIrFrameSlot), sizeof(SZrAotIrSourceMap), sizeof(SZrExecIrConstant), sizeof(SZrExecIrLayout)};

static void vm_error(unsigned reason, unsigned row, size_t original, size_t requested, TZrInt64 flag) {
    SVmError *d;
    ++g_vm.errors;
    if (g_vm.diagnosticCount == VM_ERRORS) { ++g_vm.dropped; return; }
    d = &g_vm.diagnostics[g_vm.diagnosticCount++];
    d->reason = reason; d->row = row; d->original = original; d->requested = requested; d->requestedFlag = flag;
    if (row < VM_ROWS) { d->allocated = g_vm.rows[row].bytes; d->priorFlag = g_vm.rows[row].flag; }
}
static TZrPtr vm_allocator(TZrPtr user, TZrPtr pointer, TZrSize original, TZrSize bytes, TZrInt64 flag) {
    unsigned row = VM_ROWS;
    TZrBool wasNew = pointer == ZR_NULL;
    void *replacement;
    (void)user; ++g_vm.attempts;
    for (unsigned i = 0u; i < VM_ROWS; ++i)
        if (wasNew ? g_vm.rows[i].pointer == ZR_NULL : g_vm.rows[i].pointer == pointer) { row = i; break; }
    if (row == VM_ROWS || (wasNew && (original != 0u || bytes == 0u))) {
        vm_error(1u, row, original, bytes, flag); return ZR_NULL;
    }
    if (!wasNew && (g_vm.rows[row].bytes != original || g_vm.rows[row].flag != flag))
        vm_error(2u, row, original, bytes, flag);
    if (bytes == 0u) {
        g_vm.rows[row].pointer = ZR_NULL; --g_vm.live; ++g_vm.frees;
        free(pointer); return ZR_NULL;
    }
    replacement = realloc(pointer, bytes);
    if (replacement == ZR_NULL) { vm_error(3u, row, original, bytes, flag); return ZR_NULL; }
    if (wasNew) ++g_vm.live;
    g_vm.rows[row].pointer = replacement; g_vm.rows[row].bytes = bytes; g_vm.rows[row].flag = flag;
    return replacement;
}
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

void ZrTests_Returned_Init(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    TEST_ASSERT_EQUAL_UINT(0u, g_crt[0].stats.live); TEST_ASSERT_EQUAL_UINT(0u, g_crt[1].stats.live);
    memset(&g_vm, 0, sizeof(g_vm)); memset(g_crt, 0, sizeof(g_crt));
    memset(g_functions, 0, sizeof(g_functions)); memset(g_functionRooted, 0, sizeof(g_functionRooted));
    memset(g_names, 0, sizeof(g_names)); memset(g_nameRooted, 0, sizeof(g_nameRooted));
    g_global = ZR_NULL; g_nameCount = 0u; g_savedStopGc = ZR_FALSE; g_haveStopGc = ZR_FALSE;
    g_pendingDescriptor = 0u; g_pendingOwner = ZR_FALSE;
}
void ZrTests_Returned_Open(void) {
    if (g_global != ZR_NULL) return;
    g_global = ZrCore_GlobalState_New(vm_allocator, &g_vm, 0xA01Au, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_global); TEST_ASSERT_NOT_NULL(g_global->mainThreadState);
    TEST_ASSERT_NOT_NULL(g_global->garbageCollector);
    g_savedStopGc = g_global->garbageCollector->stopGcFlag; g_haveStopGc = ZR_TRUE;
    g_global->garbageCollector->stopGcFlag = ZR_TRUE;
    TEST_ASSERT_NULL(g_global->profileRuntime); TEST_ASSERT_NULL(g_global->compileSource);
    TEST_ASSERT_NULL(g_global->userData); TEST_ASSERT_NULL(g_global->sourceLoaderUserData);
    TEST_ASSERT_NULL(g_global->sourceLoader); TEST_ASSERT_NULL(g_global->nativeModuleLoader);
    TEST_ASSERT_NULL(g_global->aotModuleLoader); TEST_ASSERT_NULL(g_global->providerModuleNameResolver);
    TEST_ASSERT_NULL(g_global->callBindingModuleResolver); TEST_ASSERT_NULL(g_global->typedCallBindingResolver);
    TEST_ASSERT_NULL(g_global->callbacks.afterStateInitialized); TEST_ASSERT_NULL(g_global->callbacks.beforeStateReleased);
    TEST_ASSERT_NULL(g_global->callbacks.afterThreadInitialized); TEST_ASSERT_NULL(g_global->callbacks.beforeThreadReleased);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
SZrState *ZrTests_Returned_State(void) { ZrTests_Returned_Open(); return g_global->mainThreadState; }
SZrString *ZrTests_Returned_Name(void) {
    char text[64];
    unsigned slot;
    SZrState *state = ZrTests_Returned_State();
    TEST_ASSERT_LESS_THAN_UINT(NAME_SLOTS, g_nameCount);
    slot = g_nameCount++;
    (void)snprintf(text, sizeof(text), "ssa_source_returned_%u.zr", slot);
    g_names[slot] = ZrCore_String_CreateFromNative(state, text);
    TEST_ASSERT_NOT_NULL(g_names[slot]);
    g_nameRooted[slot] = ZrCore_GarbageCollector_IgnoreObject(state, &g_names[slot]->super);
    TEST_ASSERT_TRUE(g_nameRooted[slot]); return g_names[slot];
}
SZrFunction *ZrTests_Returned_Compile(unsigned slot, const char *source, TZrBool ordinary,
        const SZrSourceCompileOptions *options, SZrSourceCompileDiagnostic *diagnostic) {
    SZrString *name = ZrTests_Returned_Name();
    SZrState *state = g_global->mainThreadState;
    size_t bytes = strlen(source);
    TZrUInt64 sourceHash = ZrCore_Hash_CreateStable64((const TZrByte *)source, bytes);
    TEST_ASSERT_LESS_THAN_UINT(FUNCTION_SLOTS, slot); TEST_ASSERT_NULL(g_functions[slot]);
    for (unsigned i = 0u; i < 2u; ++i) g_crt[i].callBase = g_crt[i].stats.attempts;
    if (g_pendingDescriptor != 0u) g_crt[0].failAt = g_crt[0].stats.attempts + g_pendingDescriptor;
    if (g_pendingOwner) g_crt[1].failAt = g_crt[1].stats.attempts + 1u;
    /* Assigned to the registered slot before any post-call Unity assertion. */
    g_functions[slot] = ordinary ? ZrParser_Source_Compile(state, source, bytes, name) :
            ZrParser_Source_CompileWithOptions(state, source, bytes, name, options, diagnostic);
    if (g_functions[slot] != ZR_NULL) {
        g_functionRooted[slot] = ZrCore_GarbageCollector_IgnoreObject(state, &g_functions[slot]->super);
    }
    /* Disarm before observations/assertions; caller inspects actual counters. */
    ZrTests_Returned_Disarm();
    TEST_ASSERT_EQUAL_UINT64(sourceHash, ZrCore_Hash_CreateStable64((const TZrByte *)source, bytes));
    if (g_functions[slot] != ZR_NULL) TEST_ASSERT_TRUE(g_functionRooted[slot]);
    return g_functions[slot];
}
SZrFunction *ZrTests_Returned_Function(unsigned slot) {
    TEST_ASSERT_LESS_THAN_UINT(FUNCTION_SLOTS, slot); return g_functions[slot];
}
void ZrTests_Returned_FreeFunction(unsigned slot) {
    SZrFunction *live = g_functions[slot];
    if (live == ZR_NULL) return;
    if (g_functionRooted[slot]) ZrCore_GarbageCollector_UnignoreObject(g_global, &live->super);
    g_functionRooted[slot] = ZR_FALSE; g_functions[slot] = ZR_NULL;
    ZrCore_Function_Free(g_global->mainThreadState, live);
}
unsigned ZrTests_Returned_OwnerRecord(unsigned slot) {
    SZrFunction *f = ZrTests_Returned_Function(slot);
    unsigned result = CRT_ROWS;
    TEST_ASSERT_NOT_NULL(f); TEST_ASSERT_NOT_NULL(f->ownedAotIr);
    for (unsigned i = 0u; i < g_crt[1].stats.successes; ++i)
        if (g_crt[1].rows[i].pointer != ZR_NULL && g_crt[1].rows[i].pointer == f->ownedAotIr) { result = i; break; }
    TEST_ASSERT_LESS_THAN_UINT(CRT_ROWS, result); return result;
}
void ZrTests_Returned_Disarm(void) {
    g_crt[0].failAt = 0u; g_crt[1].failAt = 0u; g_pendingDescriptor = 0u; g_pendingOwner = ZR_FALSE;
}
void ZrTests_Returned_ArmDescriptor(unsigned site) {
    TEST_ASSERT_TRUE(site >= 1u && site <= 6u); g_pendingDescriptor = site;
}
void ZrTests_Returned_ArmOwner(void) { g_pendingOwner = ZR_TRUE; }
unsigned ZrTests_Returned_RecordFrees(unsigned kind, unsigned record) {
    TEST_ASSERT_LESS_THAN_UINT(2u, kind); TEST_ASSERT_LESS_THAN_UINT(g_crt[kind].stats.successes, record);
    return g_crt[kind].rows[record].frees;
}
unsigned ZrTests_Returned_VmAttempts(void) { return g_vm.attempts; }
SZrReturnedHeapStats ZrTests_Returned_Stats(unsigned kind) {
    TEST_ASSERT_LESS_THAN_UINT(2u, kind); return g_crt[kind].stats;
}
SZrReturnedHeapEvent ZrTests_Returned_Event(unsigned kind, unsigned index) {
    TEST_ASSERT_LESS_THAN_UINT(2u, kind); TEST_ASSERT_LESS_THAN_UINT(g_crt[kind].stats.attempts, index);
    return g_crt[kind].events[index];
}
void ZrTests_Returned_Shutdown(void) {
    SZrGlobalState *live = g_global;
    ZrTests_Returned_Disarm();
    if (live == ZR_NULL) return;
    for (unsigned i = 0u; i < FUNCTION_SLOTS; ++i) {
        if (g_functions[i] != ZR_NULL && g_functionRooted[i])
            ZrCore_GarbageCollector_UnignoreObject(live, &g_functions[i]->super);
        g_functions[i] = ZR_NULL; g_functionRooted[i] = ZR_FALSE;
    }
    for (unsigned i = 0u; i < g_nameCount; ++i) {
        if (g_names[i] != ZR_NULL && g_nameRooted[i]) ZrCore_GarbageCollector_UnignoreObject(live, &g_names[i]->super);
        g_names[i] = ZR_NULL; g_nameRooted[i] = ZR_FALSE;
    }
    if (g_haveStopGc && live->garbageCollector != ZR_NULL) live->garbageCollector->stopGcFlag = g_savedStopGc;
    g_haveStopGc = ZR_FALSE; g_global = ZR_NULL;
    ZrCore_GlobalState_Free(live);
    for (unsigned i = 0u; i < g_vm.diagnosticCount; ++i) {
        const SVmError *d = &g_vm.diagnostics[i];
        fprintf(stderr, "RETURNED_VM reason=%u row=%u allocated=%llu original=%llu requested=%llu priorFlag=%lld requestFlag=%lld\n",
                d->reason, d->row, (unsigned long long)d->allocated, (unsigned long long)d->original,
                (unsigned long long)d->requested, (long long)d->priorFlag, (long long)d->requestedFlag);
    }
    if (g_vm.dropped != 0u) fprintf(stderr, "RETURNED_VM dropped=%u\n", g_vm.dropped);
}
void ZrTests_Returned_Close(void) {
    ZrTests_Returned_Disarm();
    for (unsigned i = 0u; i < FUNCTION_SLOTS; ++i) ZrTests_Returned_FreeFunction(i);
    ZrTests_Returned_Shutdown();
    /* Recovery only: normal tests prove candidate cleanup before this interval. */
    for (unsigned k = 0u; k < 2u; ++k) {
        if (g_crt[k].stats.live != 0u) ++g_crt[k].stats.errors;
        for (unsigned i = 0u; i < g_crt[k].stats.successes; ++i)
            if (g_crt[k].rows[i].pointer != ZR_NULL) crt_free(k, g_crt[k].rows[i].pointer);
    }
}
void ZrTests_Returned_AssertBalanced(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
    for (unsigned k = 0u; k < 2u; ++k) {
        TEST_ASSERT_EQUAL_UINT(0u, g_crt[k].stats.live); TEST_ASSERT_EQUAL_UINT(0u, g_crt[k].stats.errors);
        TEST_ASSERT_EQUAL_UINT(g_crt[k].stats.successes, g_crt[k].stats.frees);
        for (unsigned i = 0u; i < g_crt[k].stats.successes; ++i) TEST_ASSERT_EQUAL_UINT(1u, g_crt[k].rows[i].frees);
    }
}
