#undef malloc
#undef calloc
#undef realloc
#undef free

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct SZrExecIrTestAllocation {
    void *address;
    struct SZrExecIrTestAllocation *next;
} SZrExecIrTestAllocation;

static SZrExecIrTestAllocation *g_zr_exec_ir_test_allocations;
static uint64_t g_zr_exec_ir_test_allocation_count;
static uint64_t g_zr_exec_ir_test_fail_at;
static uint64_t g_zr_exec_ir_test_live_count;

static void zr_exec_ir_test_track(void *address) {
    SZrExecIrTestAllocation *entry;
    if (address == NULL) return;
    entry = (SZrExecIrTestAllocation *)malloc(sizeof(*entry));
    if (entry == NULL) abort();
    entry->address = address;
    entry->next = g_zr_exec_ir_test_allocations;
    g_zr_exec_ir_test_allocations = entry;
    ++g_zr_exec_ir_test_live_count;
}

static void zr_exec_ir_test_untrack(void *address) {
    SZrExecIrTestAllocation **cursor = &g_zr_exec_ir_test_allocations;
    while (*cursor != NULL) {
        if ((*cursor)->address == address) {
            SZrExecIrTestAllocation *entry = *cursor;
            *cursor = entry->next;
            free(entry);
            --g_zr_exec_ir_test_live_count;
            return;
        }
        cursor = &(*cursor)->next;
    }
}

static int zr_exec_ir_test_should_fail(void) {
    ++g_zr_exec_ir_test_allocation_count;
    return g_zr_exec_ir_test_fail_at != 0u &&
           g_zr_exec_ir_test_allocation_count == g_zr_exec_ir_test_fail_at;
}

void *zr_exec_ir_test_malloc(size_t size) {
    void *address;
    if (zr_exec_ir_test_should_fail()) return NULL;
    address = malloc(size);
    zr_exec_ir_test_track(address);
    return address;
}

void *zr_exec_ir_test_calloc(size_t count, size_t size) {
    void *address;
    if (zr_exec_ir_test_should_fail()) return NULL;
    address = calloc(count, size);
    zr_exec_ir_test_track(address);
    return address;
}

void *zr_exec_ir_test_realloc(void *storage, size_t size) {
    void *address;
    SZrExecIrTestAllocation *entry = g_zr_exec_ir_test_allocations;
    while (entry != NULL && entry->address != storage) entry = entry->next;
    if (zr_exec_ir_test_should_fail()) return NULL;
    address = realloc(storage, size);
    if (address == NULL) return NULL;
    if (entry != NULL) entry->address = address;
    else zr_exec_ir_test_track(address);
    return address;
}

void zr_exec_ir_test_free(void *storage) {
    if (storage != NULL) zr_exec_ir_test_untrack(storage);
    free(storage);
}

void ZrCore_ExecIr_TestAllocatorReset(void) {
    g_zr_exec_ir_test_allocation_count = 0u;
    g_zr_exec_ir_test_fail_at = 0u;
}

void ZrCore_ExecIr_TestAllocatorFailAt(uint64_t allocationIndex) {
    g_zr_exec_ir_test_allocation_count = 0u;
    g_zr_exec_ir_test_fail_at = allocationIndex;
}

uint64_t ZrCore_ExecIr_TestAllocatorCount(void) {
    return g_zr_exec_ir_test_allocation_count;
}

uint64_t ZrCore_ExecIr_TestAllocatorLiveCount(void) {
    return g_zr_exec_ir_test_live_count;
}
