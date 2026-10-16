#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "unity.h"
#include "support/ssa_literal_script_fixture.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "compiler/metadata/compiler_metadata_string_heap.h"

enum { RECORDS = 8192, DIAGNOSTICS = 64, RESERVE_ROWS = 8 };
typedef struct SRecord {
    void *pointer;
    size_t bytes;
    TZrInt64 flag;
    unsigned frees;
} SRecord;
typedef struct SError {
    unsigned reason, slot;
    size_t allocated, original, requested;
    TZrInt64 previousFlag, requestedFlag;
} SError;
typedef struct SLedger {
    SRecord rows[RECORDS];
    SError diagnostics[DIAGNOSTICS];
    unsigned count, attempts, allocations, frees, live, errors, diagnosticCount, dropped;
    unsigned injected;
    TZrBool armed;
    size_t expectedFaultBytes;
} SLedger;
static SLedger g_vm;
static SZrGlobalState *g_global;
static SZrSsaLiteralScriptFixture g_fixture;
static SZrMetadataStringHeapBuilder g_builder;
static TZrBool g_fixtureRegistered, g_savedStopGc, g_haveStopGc;

static void record_error(unsigned reason, unsigned slot, size_t original,
                          size_t requested, TZrInt64 flag) {
    SError *d;
    ++g_vm.errors;
    if (g_vm.diagnosticCount == DIAGNOSTICS) { ++g_vm.dropped; return; }
    d = &g_vm.diagnostics[g_vm.diagnosticCount++];
    d->reason = reason; d->slot = slot; d->original = original;
    d->requested = requested; d->requestedFlag = flag;
    if (slot < g_vm.count) {
        d->allocated = g_vm.rows[slot].bytes; d->previousFlag = g_vm.rows[slot].flag;
    }
}
/* Each fresh allocation gets a new bounded row, so post-free checks use only
 * numeric record indices/counters. Live pointer lookup never scans dead values. */
static TZrPtr vm_allocator(TZrPtr user, TZrPtr pointer, TZrSize original,
                           TZrSize requested, TZrInt64 flag) {
    TZrBool wasNew = pointer == ZR_NULL;
    unsigned slot = RECORDS;
    void *replacement;
    (void)user;
    ++g_vm.attempts;
    if (g_vm.armed) {
        g_vm.armed = ZR_FALSE;
        if (!wasNew || original != 0u || requested != g_vm.expectedFaultBytes ||
            flag != ZR_MEMORY_NATIVE_TYPE_FUNCTION) {
            record_error(1u, slot, original, requested, flag); return ZR_NULL;
        }
        ++g_vm.injected; return ZR_NULL;
    }
    if (wasNew) {
        if (original != 0u || requested == 0u || g_vm.count == RECORDS) {
            record_error(2u, slot, original, requested, flag); return ZR_NULL;
        }
        slot = g_vm.count;
    } else {
        for (unsigned i = 0u; i < g_vm.count; ++i)
            if (g_vm.rows[i].pointer != ZR_NULL && g_vm.rows[i].pointer == pointer) { slot = i; break; }
        if (slot == RECORDS) { record_error(3u, slot, original, requested, flag); return ZR_NULL; }
        if (g_vm.rows[slot].bytes != original || g_vm.rows[slot].flag != flag)
            record_error(4u, slot, original, requested, flag);
    }
    if (requested == 0u) {
        g_vm.rows[slot].pointer = ZR_NULL; ++g_vm.rows[slot].frees;
        --g_vm.live; ++g_vm.frees; free(pointer); return ZR_NULL;
    }
    replacement = realloc(pointer, requested);
    if (replacement == ZR_NULL) {
        record_error(5u, slot, original, requested, flag); return ZR_NULL;
    }
    if (wasNew) { ++g_vm.count; ++g_vm.live; }
    g_vm.rows[slot].pointer = replacement;
    g_vm.rows[slot].bytes = requested; g_vm.rows[slot].flag = flag;
    ++g_vm.allocations; return replacement;
}
static unsigned live_record(const void *pointer) {
    unsigned result = RECORDS;
    for (unsigned i = 0u; i < g_vm.count; ++i)
        if (g_vm.rows[i].pointer != ZR_NULL && g_vm.rows[i].pointer == pointer) { result = i; break; }
    TEST_ASSERT_LESS_THAN_UINT(RECORDS, result);
    return result;
}
static void close_owners(void) {
    g_vm.armed = ZR_FALSE;
    if (g_builder.compiler != ZR_NULL) metadata_token_string_heap_free(g_builder.compiler, &g_builder);
    /* Clear the borrowed compiler before destroying that owner. */
    memset(&g_builder, 0, sizeof(g_builder));
    if (g_fixtureRegistered) {
        ZrTests_SsaLiteralScriptFixture_Free(&g_fixture); g_fixtureRegistered = ZR_FALSE;
    }
    if (g_global != ZR_NULL) {
        SZrGlobalState *live = g_global;
        if (g_haveStopGc) live->garbageCollector->stopGcFlag = g_savedStopGc;
        g_haveStopGc = ZR_FALSE; g_global = ZR_NULL;
        ZrCore_GlobalState_Free(live);
    }
}
void setUp(void) {
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live);
    memset(&g_vm, 0, sizeof(g_vm)); memset(&g_fixture, 0, sizeof(g_fixture));
    memset(&g_builder, 0, sizeof(g_builder));
    g_global = ZR_NULL; g_fixtureRegistered = ZR_FALSE; g_haveStopGc = ZR_FALSE;
}
void tearDown(void) {
    close_owners();
    for (unsigned i = 0u; i < g_vm.diagnosticCount; ++i) {
        const SError *d = &g_vm.diagnostics[i];
        fprintf(stderr, "STRING_HEAP_VM reason=%u slot=%u allocated=%llu original=%llu requested=%llu priorFlag=%lld requestFlag=%lld\n",
                d->reason, d->slot, (unsigned long long)d->allocated,
                (unsigned long long)d->original, (unsigned long long)d->requested,
                (long long)d->previousFlag, (long long)d->requestedFlag);
    }
    if (g_vm.dropped != 0u) fprintf(stderr, "STRING_HEAP_VM dropped=%u\n", g_vm.dropped);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.live); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static void prepare(EZrSsaLiteralScriptSource source) {
    g_global = ZrCore_GlobalState_New(vm_allocator, &g_vm, 0xA019u, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_global); TEST_ASSERT_NOT_NULL(g_global->mainThreadState);
    TEST_ASSERT_NOT_NULL(g_global->garbageCollector);
    g_savedStopGc = g_global->garbageCollector->stopGcFlag; g_haveStopGc = ZR_TRUE;
    g_global->garbageCollector->stopGcFlag = ZR_TRUE;
    TEST_ASSERT_NULL(g_global->profileRuntime); TEST_ASSERT_NULL(g_global->compileSource);
    TEST_ASSERT_NULL(g_global->sourceLoader); TEST_ASSERT_NULL(g_global->aotModuleLoader);
    TEST_ASSERT_NULL(g_global->nativeModuleLoader); TEST_ASSERT_NULL(g_global->providerModuleNameResolver);
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture, g_global->mainThreadState);
    g_fixtureRegistered = ZR_TRUE;
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture, source);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static TZrUInt64 source_digest(void) {
    SZrFunction *f = g_fixture.compiler.currentFunction;
    TZrUInt64 h = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    h = ZrTests_SsaLiteralScriptFixture_DigestBytes(h, &g_fixture.compiler, sizeof(g_fixture.compiler));
    h = ZrTests_SsaLiteralScriptFixture_DigestBytes(h, f, sizeof(*f));
    h = ZrTests_SsaLiteralScriptFixture_DigestBytes(h, f->metadataStringHeap,
                         (size_t)f->metadataStringHeapLength * sizeof(*f->metadataStringHeap));
    return h;
}
static unsigned make_reserved_builder(SZrMetadataStringHeapEntry *witness) {
    SZrFunction *f = g_fixture.compiler.currentFunction;
    unsigned slot;
    TZrBool added;
    TEST_ASSERT_EQUAL_UINT32(1u, f->metadataStringHeapLength);
    TEST_ASSERT_NOT_NULL(f->metadataStringHeap); TEST_ASSERT_NOT_NULL(f->metadataStringHeap[0].value);
    memcpy(witness, &f->metadataStringHeap[0], sizeof(*witness));
    /* Global builder owns any partial allocation before Add can fail. */
    g_builder.compiler = &g_fixture.compiler;
    added = metadata_token_string_heap_add(&g_builder, witness->value);
    TEST_ASSERT_TRUE(added); TEST_ASSERT_NOT_NULL(g_builder.entries);
    metadata_token_sort_string_heap(&g_builder);
    TEST_ASSERT_EQUAL_UINT32(1u, g_builder.length);
    TEST_ASSERT_EQUAL_UINT32(RESERVE_ROWS, g_builder.capacity);
    TEST_ASSERT_EQUAL_UINT32(witness->stringIndex, g_builder.entries[0].stringIndex);
    TEST_ASSERT_EQUAL_PTR(witness->value, g_builder.entries[0].value);
    slot = live_record(g_builder.entries);
    TEST_ASSERT_EQUAL_UINT64((size_t)RESERVE_ROWS * sizeof(*g_builder.entries), g_vm.rows[slot].bytes);
    TEST_ASSERT_EQUAL_INT64(ZR_MEMORY_NATIVE_TYPE_FUNCTION, g_vm.rows[slot].flag);
    return slot;
}
static void assert_same_row(const SZrMetadataStringHeapEntry *witness) {
    TEST_ASSERT_EQUAL_UINT32(witness->stringIndex, g_builder.entries[0].stringIndex);
    TEST_ASSERT_EQUAL_PTR(witness->value, g_builder.entries[0].value);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(witness->value, g_builder.entries[0].value));
}
static void assert_freed(unsigned slot) {
    /* No pointer field is inspected after releasing this allocation. */
    TEST_ASSERT_EQUAL_UINT(1u, g_vm.rows[slot].frees);
}
static void release_builder(unsigned slot) {
    metadata_token_string_heap_free(&g_fixture.compiler, &g_builder);
    TEST_ASSERT_NULL(g_builder.entries); TEST_ASSERT_EQUAL_UINT32(0u, g_builder.length);
    TEST_ASSERT_EQUAL_UINT32(0u, g_builder.capacity); assert_freed(slot);
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static void finalized_heap(EZrSsaLiteralScriptSource source) {
    unsigned slot;
    SZrFunction *f;
    prepare(source); f = g_fixture.compiler.currentFunction;
    TEST_ASSERT_EQUAL_UINT32(1u, f->metadataStringHeapLength);
    TEST_ASSERT_NOT_NULL(f->metadataStringHeap);
    slot = live_record(f->metadataStringHeap);
    TEST_ASSERT_EQUAL_UINT64((size_t)f->metadataStringHeapLength * sizeof(*f->metadataStringHeap),
                            g_vm.rows[slot].bytes);
    TEST_ASSERT_EQUAL_INT64(ZR_MEMORY_NATIVE_TYPE_FUNCTION, g_vm.rows[slot].flag);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, f->metadataStringHeap[0].stringIndex);
    TEST_ASSERT_NOT_NULL(f->metadataStringHeap[0].value);
    close_owners();
    assert_freed(slot); TEST_ASSERT_EQUAL_UINT(0u, g_vm.live); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static void test_actual_nine_finalized_heap_owns_exact_live_bytes(void) {
    finalized_heap(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
}
static void test_actual_eight_finalized_heap_owns_exact_live_bytes(void) {
    finalized_heap(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
}
static void test_compact_failure_preserves_builder_then_retry(void) {
    SZrMetadataStringHeapEntry witness;
    SZrMetadataStringHeapBuilder saved;
    unsigned char savedRows[RESERVE_ROWS * sizeof(SZrMetadataStringHeapEntry)];
    unsigned oldSlot, newSlot, attempts, allocations, frees, live;
    TZrUInt64 digest;
    TZrBool result;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); oldSlot = make_reserved_builder(&witness);
    memcpy(&saved, &g_builder, sizeof(saved)); memcpy(savedRows, g_builder.entries, sizeof(savedRows));
    digest = source_digest(); attempts = g_vm.attempts; allocations = g_vm.allocations;
    frees = g_vm.frees; live = g_vm.live;
    g_vm.expectedFaultBytes = (size_t)g_builder.length * sizeof(*g_builder.entries);
    g_vm.armed = ZR_TRUE;
    result = metadata_token_string_heap_compact(&g_builder);
    g_vm.armed = ZR_FALSE;
    TEST_ASSERT_FALSE(result); TEST_ASSERT_EQUAL_UINT(attempts + 1u, g_vm.attempts);
    TEST_ASSERT_EQUAL_UINT(1u, g_vm.injected); TEST_ASSERT_EQUAL_UINT(allocations, g_vm.allocations);
    TEST_ASSERT_EQUAL_UINT(frees, g_vm.frees); TEST_ASSERT_EQUAL_UINT(live, g_vm.live);
    TEST_ASSERT_EQUAL_MEMORY(&saved, &g_builder, sizeof(saved));
    TEST_ASSERT_EQUAL_MEMORY(savedRows, g_builder.entries, sizeof(savedRows));
    TEST_ASSERT_EQUAL_UINT64(digest, source_digest()); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
    result = metadata_token_string_heap_compact(&g_builder);
    /* saved holds the now-freed reserve pointer and is never read again. */
    TEST_ASSERT_TRUE(result); TEST_ASSERT_EQUAL_UINT(attempts + 3u, g_vm.attempts);
    TEST_ASSERT_EQUAL_UINT(allocations + 1u, g_vm.allocations);
    TEST_ASSERT_EQUAL_UINT(frees + 1u, g_vm.frees); TEST_ASSERT_EQUAL_UINT(live, g_vm.live);
    TEST_ASSERT_EQUAL_UINT32(g_builder.length, g_builder.capacity); assert_freed(oldSlot);
    newSlot = live_record(g_builder.entries);
    TEST_ASSERT_EQUAL_UINT64(sizeof(*g_builder.entries), g_vm.rows[newSlot].bytes);
    assert_same_row(&witness); TEST_ASSERT_EQUAL_UINT64(digest, source_digest());
    release_builder(newSlot);
}
static void test_compacted_builder_second_compact_allocates_nothing(void) {
    SZrMetadataStringHeapEntry witness;
    SZrMetadataStringHeapBuilder saved;
    unsigned oldSlot, newSlot, attempts, frees;
    TZrUInt64 digest;
    TZrBool result;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); oldSlot = make_reserved_builder(&witness);
    result = metadata_token_string_heap_compact(&g_builder); TEST_ASSERT_TRUE(result);
    assert_freed(oldSlot); newSlot = live_record(g_builder.entries);
    memcpy(&saved, &g_builder, sizeof(saved)); attempts = g_vm.attempts; frees = g_vm.frees;
    digest = source_digest();
    result = metadata_token_string_heap_compact(&g_builder);
    TEST_ASSERT_TRUE(result); TEST_ASSERT_EQUAL_UINT(attempts, g_vm.attempts);
    TEST_ASSERT_EQUAL_UINT(frees, g_vm.frees); TEST_ASSERT_EQUAL_MEMORY(&saved, &g_builder, sizeof(saved));
    TEST_ASSERT_EQUAL_UINT64(digest, source_digest()); assert_same_row(&witness);
    release_builder(newSlot);
}
static void test_empty_live_compiler_builder_compact_allocates_nothing(void) {
    SZrMetadataStringHeapBuilder saved;
    unsigned attempts, frees;
    TZrUInt64 digest;
    TZrBool result;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    g_builder.compiler = &g_fixture.compiler;
    memcpy(&saved, &g_builder, sizeof(saved)); attempts = g_vm.attempts; frees = g_vm.frees;
    digest = source_digest(); result = metadata_token_string_heap_compact(&g_builder);
    TEST_ASSERT_TRUE(result); TEST_ASSERT_EQUAL_UINT(attempts, g_vm.attempts);
    TEST_ASSERT_EQUAL_UINT(frees, g_vm.frees); TEST_ASSERT_EQUAL_MEMORY(&saved, &g_builder, sizeof(saved));
    TEST_ASSERT_EQUAL_UINT64(digest, source_digest()); TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
static void test_zero_length_reserved_builder_releases_capacity(void) {
    SZrMetadataStringHeapEntry witness;
    unsigned slot, allocations, frees, live, attempts;
    TZrUInt32 savedLength;
    TZrUInt64 digest;
    TZrBool result;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); slot = make_reserved_builder(&witness);
    savedLength = g_builder.length; digest = source_digest();
    allocations = g_vm.allocations; frees = g_vm.frees; live = g_vm.live; attempts = g_vm.attempts;
    g_builder.length = 0u;
    result = metadata_token_string_heap_compact(&g_builder);
    /* A failed operation retains the reserve owner; restore before assertions.
     * Success completes the valid empty owner reset and must not restore length. */
    if (!result) g_builder.length = savedLength;
    TEST_ASSERT_TRUE(result); TEST_ASSERT_NULL(g_builder.entries);
    TEST_ASSERT_EQUAL_UINT32(0u, g_builder.capacity); TEST_ASSERT_EQUAL_UINT32(0u, g_builder.length);
    TEST_ASSERT_EQUAL_UINT(allocations, g_vm.allocations); TEST_ASSERT_EQUAL_UINT(frees + 1u, g_vm.frees);
    TEST_ASSERT_EQUAL_UINT(attempts + 1u, g_vm.attempts); TEST_ASSERT_EQUAL_UINT(live - 1u, g_vm.live);
    assert_freed(slot); TEST_ASSERT_EQUAL_UINT64(digest, source_digest());
    TEST_ASSERT_EQUAL_UINT(0u, g_vm.errors);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_actual_nine_finalized_heap_owns_exact_live_bytes);
    RUN_TEST(test_actual_eight_finalized_heap_owns_exact_live_bytes);
    RUN_TEST(test_compact_failure_preserves_builder_then_retry);
    RUN_TEST(test_compacted_builder_second_compact_allocates_nothing);
    RUN_TEST(test_empty_live_compiler_builder_compact_allocates_nothing);
    RUN_TEST(test_zero_length_reserved_builder_releases_capacity);
    return UNITY_END();
}
