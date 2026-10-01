#include "ssa_eis6_fault_allocator.h"

#include <stdint.h>
#include <stdlib.h>

/* This fixed table is deliberately private to the small single-function
 * EIS6 test. It avoids allocator recursion while tracking the Core graph. */
#define ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY ((size_t)256u)

static void *g_zr_eis6_test_pointers[ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY];
static size_t g_zr_eis6_test_fail_ordinal = SIZE_MAX;
static size_t g_zr_eis6_test_attempts;
static size_t g_zr_eis6_test_outstanding;
static int g_zr_eis6_test_tracking_overflow;

static size_t zr_eis6_test_find_pointer(const void *pointer) {
    for (size_t index = 0u; index < ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY;
         ++index) {
        if (g_zr_eis6_test_pointers[index] == pointer) return index;
    }
    return ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY;
}

static size_t zr_eis6_test_find_empty(void) {
    for (size_t index = 0u; index < ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY;
         ++index) {
        if (g_zr_eis6_test_pointers[index] == NULL) return index;
    }
    return ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY;
}

static int zr_eis6_test_should_fail(void) {
    size_t ordinal = g_zr_eis6_test_attempts++;
    return ordinal == g_zr_eis6_test_fail_ordinal;
}

void *ZrEis6Test_Malloc(size_t size) {
    void *pointer;
    size_t slot;
    if (zr_eis6_test_should_fail()) return NULL;
    slot = zr_eis6_test_find_empty();
    if (slot == ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY) {
        g_zr_eis6_test_tracking_overflow = 1;
        return NULL;
    }
    pointer = malloc(size);
    if (pointer != NULL) {
        g_zr_eis6_test_pointers[slot] = pointer;
        ++g_zr_eis6_test_outstanding;
    }
    return pointer;
}

void *ZrEis6Test_Calloc(size_t count, size_t size) {
    void *pointer;
    size_t slot;
    if (zr_eis6_test_should_fail()) return NULL;
    slot = zr_eis6_test_find_empty();
    if (slot == ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY) {
        g_zr_eis6_test_tracking_overflow = 1;
        return NULL;
    }
    pointer = calloc(count, size);
    if (pointer != NULL) {
        g_zr_eis6_test_pointers[slot] = pointer;
        ++g_zr_eis6_test_outstanding;
    }
    return pointer;
}

void *ZrEis6Test_Realloc(void *pointer, size_t size) {
    void *resized;
    size_t oldSlot = pointer != NULL
            ? zr_eis6_test_find_pointer(pointer)
            : ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY;
    size_t newSlot = oldSlot;
    if (zr_eis6_test_should_fail()) return NULL;
    if (oldSlot == ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY) {
        newSlot = zr_eis6_test_find_empty();
        if (newSlot == ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY) {
            g_zr_eis6_test_tracking_overflow = 1;
            return NULL;
        }
    }
    resized = realloc(pointer, size);
    if (resized == NULL) return NULL;
    if (oldSlot != ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY) {
        g_zr_eis6_test_pointers[oldSlot] = resized;
    } else {
        g_zr_eis6_test_pointers[newSlot] = resized;
        ++g_zr_eis6_test_outstanding;
    }
    return resized;
}

void ZrEis6Test_Free(void *pointer) {
    if (pointer != NULL) {
        size_t slot = zr_eis6_test_find_pointer(pointer);
        if (slot != ZR_EIS6_TEST_TRACKED_POINTER_CAPACITY) {
            g_zr_eis6_test_pointers[slot] = NULL;
            --g_zr_eis6_test_outstanding;
        }
    }
    free(pointer);
}

void ZrEis6Test_ArmFailure(size_t allocationOrdinal) {
    g_zr_eis6_test_attempts = 0u;
    g_zr_eis6_test_fail_ordinal = allocationOrdinal;
    g_zr_eis6_test_tracking_overflow = 0;
}

void ZrEis6Test_DisableFailure(void) {
    g_zr_eis6_test_fail_ordinal = SIZE_MAX;
}

size_t ZrEis6Test_AllocationAttempts(void) {
    return g_zr_eis6_test_attempts;
}

size_t ZrEis6Test_OutstandingAllocations(void) {
    return g_zr_eis6_test_outstanding;
}

int ZrEis6Test_TrackingOverflowed(void) {
    return g_zr_eis6_test_tracking_overflow;
}
