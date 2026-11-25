#include "zr_vm_strict/ownership.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Fixture {
    ZrStrictRuntime runtime;
    size_t allocations;
    size_t frees;
    size_t drops;
    size_t survivors;
    int failAllocation;
} Fixture;

typedef struct Payload {
    int value;
    ZrStrictWeak self;
    ZrStrictShared next;
} Payload;

static const ZrStrictSite site = {"ownership.zr", 5, 7};

static void *allocate(void *context, size_t size) {
    Fixture *fixture = context;
    if (fixture->failAllocation) {
        return NULL;
    }
    ++fixture->allocations;
    return malloc(size);
}

static void release(void *context, void *address) {
    Fixture *fixture = context;
    ++fixture->frees;
    free(address);
}

static void start(Fixture *fixture) {
    ZrStrictAllocator allocator = {allocate, release, fixture};
    *fixture = (Fixture){0};
    assert(ZrStrictRuntime_Init(&fixture->runtime, &allocator) == ZR_STRICT_OK);
}

static void finish(Fixture *fixture, ZrStrictReportSurvivor report) {
    assert(ZrStrictRuntime_Close(&fixture->runtime, report, fixture) == ZR_STRICT_OK);
    assert(fixture->allocations == fixture->frees);
}

static ZrStrictStatus drop_payload(void *storage, void *context) {
    Payload *payload = storage;
    Fixture *fixture = context;
    ZrStrictManualBlock forbidden = {0};
    ++fixture->drops;
    assert(ZrStrict_ManualAlloc(&fixture->runtime, 4, site, &forbidden) == ZR_STRICT_BUSY);
    assert(!forbidden.owner.record);
    if (payload->self.record) {
        ZrStrictShared revived = {0};
        assert(ZrStrict_Wake(&payload->self, &revived) == ZR_STRICT_EXPIRED);
        assert(!revived.record);
        assert(ZrStrict_WeakDrop(&payload->self) == ZR_STRICT_OK);
    }
    assert(ZrStrictRuntime_Close(&fixture->runtime, NULL, NULL) == ZR_STRICT_BUSY);
    return payload->value < 0 ? ZR_STRICT_FAULT : ZR_STRICT_OK;
}

static ZrStrictLayout payload_layout(Fixture *fixture) {
    return (ZrStrictLayout){sizeof(Payload), 64, "Student", drop_payload, fixture};
}

static Payload *construct(Fixture *fixture, int value, ZrStrictUnique *owner) {
    ZrStrictLayout layout = payload_layout(fixture);
    ZrStrictBuilder builder = {0};
    void *storage = NULL;
    Payload *payload;
    assert(ZrStrict_OwnBegin(&fixture->runtime, &layout, site, &builder, &storage) == ZR_STRICT_OK);
    assert((uintptr_t)storage % layout.alignment == 0);
    payload = storage;
    *payload = (Payload){.value = value};
    assert(ZrStrict_OwnCommit(&builder, owner) == ZR_STRICT_OK);
    assert(!builder.record);
    return payload;
}

static void test_last_strong_releases_payload_and_weak_delays_storage(void) {
    Fixture fixture;
    ZrStrictUnique unique = {0};
    ZrStrictShared shared = {0};
    ZrStrictShared second = {0};
    ZrStrictShared upgraded = {0};
    ZrStrictWeak weak = {0};
    ZrStrictWeak copiedWeak = {0};
    Payload *payload;
    start(&fixture);
    payload = construct(&fixture, 42, &unique);
    assert(ZrStrict_UniqueData(&unique) == payload);
    fixture.failAllocation = 1; /* Ownership operations must not allocate. */
    assert(ZrStrict_Share(&unique, &shared) == ZR_STRICT_OK);
    assert(!ZrStrict_UniqueData(&unique));
    assert(ZrStrict_SharedData(&shared) == payload);
    assert(ZrStrict_SharedClone(&shared, &second) == ZR_STRICT_OK);
    assert(ZrStrict_Degrade(&shared, &weak) == ZR_STRICT_OK);
    assert(ZrStrict_WeakClone(&weak, &copiedWeak) == ZR_STRICT_OK);
    assert(ZrStrict_Wake(&weak, &upgraded) == ZR_STRICT_OK);
    assert(ZrStrict_SharedDrop(&shared) == ZR_STRICT_OK);
    assert(ZrStrict_SharedDrop(&second) == ZR_STRICT_OK);
    assert(fixture.drops == 0);
    assert(ZrStrict_SharedDrop(&upgraded) == ZR_STRICT_OK);
    assert(fixture.drops == 1 && fixture.frees == 0);
    assert(ZrStrict_Wake(&weak, &upgraded) == ZR_STRICT_EXPIRED && !upgraded.record);
    assert(ZrStrict_WeakDrop(&weak) == ZR_STRICT_OK);
    assert(fixture.frees == 0);
    assert(ZrStrict_WeakDrop(&copiedWeak) == ZR_STRICT_OK);
    assert(fixture.allocations == 1 && fixture.frees == 1);
    finish(&fixture, NULL);
}

static void test_failed_constructor_and_destructor(void) {
    Fixture fixture;
    ZrStrictLayout layout;
    ZrStrictBuilder builder = {0};
    ZrStrictUnique unique = {0};
    ZrStrictShared shared = {0};
    void *storage = NULL;
    Payload *payload;
    start(&fixture);
    layout = payload_layout(&fixture);
    fixture.failAllocation = 1;
    assert(ZrStrict_OwnBegin(&fixture.runtime, &layout, site, &builder, &storage) == ZR_STRICT_OUT_OF_MEMORY);
    assert(!builder.record && !storage);
    fixture.failAllocation = 0;
    assert(ZrStrict_OwnBegin(&fixture.runtime, &layout, site, &builder, &storage) == ZR_STRICT_OK);
    assert(ZrStrict_OwnAbort(&builder) == ZR_STRICT_OK);
    assert(fixture.drops == 0 && fixture.frees == 1);
    payload = construct(&fixture, -1, &unique);
    assert(ZrStrict_Share(&unique, &shared) == ZR_STRICT_OK);
    assert(ZrStrict_Degrade(&shared, &payload->self) == ZR_STRICT_OK);
    assert(ZrStrict_SharedDrop(&shared) == ZR_STRICT_FAULT);
    assert(fixture.drops == 1 && fixture.frees == 2);
    finish(&fixture, NULL);
}

static void report_survivor(void *context, const char *typeName, ZrStrictSite origin, size_t strongCount) {
    Fixture *fixture = context;
    assert(strcmp(typeName, "Student") == 0);
    assert(strcmp(origin.source, site.source) == 0 && origin.line == 5 && origin.column == 7);
    assert(strongCount == 1);
    assert(fixture->frees == 0);
    ++fixture->survivors;
}

static void test_cycle_survives_until_shutdown(void) {
    Fixture fixture;
    ZrStrictUnique a = {0};
    ZrStrictUnique b = {0};
    ZrStrictShared sharedA = {0};
    ZrStrictShared sharedB = {0};
    Payload *payloadA;
    Payload *payloadB;
    start(&fixture);
    payloadA = construct(&fixture, 1, &a);
    payloadB = construct(&fixture, 2, &b);
    assert(ZrStrict_Share(&a, &sharedA) == ZR_STRICT_OK);
    assert(ZrStrict_Share(&b, &sharedB) == ZR_STRICT_OK);
    /* Native fixture builds a cycle to exercise teardown independently of
     * the language's readonly Shared access rules. */
    assert(ZrStrict_SharedClone(&sharedA, &payloadB->next) == ZR_STRICT_OK);
    assert(ZrStrict_SharedClone(&sharedB, &payloadA->next) == ZR_STRICT_OK);
    assert(ZrStrict_SharedDrop(&sharedA) == ZR_STRICT_OK);
    assert(ZrStrict_SharedDrop(&sharedB) == ZR_STRICT_OK);
    assert(fixture.drops == 0 && fixture.frees == 0);
    finish(&fixture, report_survivor);
    assert(fixture.survivors == 2 && fixture.drops == 0);
}

static void test_manual_blocks_are_fallible_and_tracked(void) {
    Fixture fixture;
    ZrStrictManualBlock block = {0};
    size_t size = 0;
    unsigned char *bytes;
    start(&fixture);
    assert(ZrStrict_ManualAlloc(&fixture.runtime, SIZE_MAX, site, &block) == ZR_STRICT_OUT_OF_MEMORY);
    assert(fixture.allocations == 0 && !block.owner.record);
    assert(ZrStrict_ManualAlloc(&fixture.runtime, 16, site, &block) == ZR_STRICT_OK);
    bytes = ZrStrict_ManualData(&block, &size);
    assert(bytes && size == 16);
    memset(bytes, 7, size);
    assert(ZrStrict_ManualFree(&block) == ZR_STRICT_OK);
    assert(!ZrStrict_ManualData(&block, &size) && size == 0);
    assert(fixture.allocations == 1 && fixture.frees == 1);
    finish(&fixture, NULL);
}

int main(void) {
    test_last_strong_releases_payload_and_weak_delays_storage();
    test_failed_constructor_and_destructor();
    test_cycle_survives_until_shutdown();
    test_manual_blocks_are_fallible_and_tracked();
    return 0;
}
