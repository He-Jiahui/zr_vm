#include "zr_vm_strict/ownership.h"
#include "zr_vm_strict/pool.h"

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

typedef struct DeepNode {
    ZrStrictUnique child;
    size_t ordinal;
} DeepNode;

static ZrStrictStatus drop_deep_node(void *storage, void *context) {
    DeepNode *node = storage;
    Fixture *fixture = context;
    assert(node->ordinal == ++fixture->drops);
    return node->child.record ? ZrStrict_UniqueDrop(&node->child) : ZR_STRICT_FAULT;
}

static void test_deep_release_is_iterative_and_preserves_child_fault(void) {
    enum { count = 8192 };
    Fixture fixture;
    ZrStrictUnique root = {0};
    start(&fixture);
    const ZrStrictLayout layout = {sizeof(DeepNode), _Alignof(DeepNode), "DeepNode", drop_deep_node, &fixture};
    for (size_t n = count; n > 0; --n) {
        ZrStrictBuilder builder = {0};
        void *raw = NULL;
        assert(ZrStrict_OwnBegin(&fixture.runtime, &layout, site, &builder, &raw) == ZR_STRICT_OK);
        DeepNode *payload = raw;
        *payload = (DeepNode){root, n};
        root = (ZrStrictUnique){0};
        assert(ZrStrict_OwnCommit(&builder, &root) == ZR_STRICT_OK);
    }
    fixture.failAllocation = 1;
    assert(ZrStrict_UniqueDrop(&root) == ZR_STRICT_FAULT);
    assert(!root.record && fixture.drops == count && fixture.frees == count && fixture.allocations == count);
    finish(&fixture, NULL);
}

static void report_rental(void *context, const char *name, ZrStrictSite origin) {
    Fixture *fixture = context;
    assert(strcmp(name, "Student") == 0 && origin.source == site.source && origin.line == site.line);
    ++fixture->survivors;
}

static void test_optional_pool_recycle_and_slab_lifetime(void) {
    Fixture fixture, otherRuntime;
    start(&fixture);
    start(&otherRuntime);
    ZrStrictPool pool = {0}, unrelated = {0};
    ZrStrictLayout layout = payload_layout(&fixture);
    ZrStrictLayout otherLayout = payload_layout(&otherRuntime);
    assert(ZrStrict_PoolInit(&fixture.runtime, &pool, &layout, 2, 3) == ZR_STRICT_OK);
    assert(ZrStrict_PoolInit(&otherRuntime.runtime, &unrelated, &otherLayout, 2, 3) == ZR_STRICT_OK);
    assert(fixture.allocations == 0);
    assert(ZrStrictRuntime_Close(&fixture.runtime, NULL, NULL) == ZR_STRICT_BUSY);

    ZrStrictPoolHandle first = {0}, second = {0}, third = {0}, next = {0}, unrelatedHandle = {0};
    const Payload value = {.value = 42}, fault = {.value = -1};
    assert(ZrStrict_PoolRent(&pool, &value, site, &first) == ZR_STRICT_OK);
    assert(ZrStrict_PoolRent(&pool, &value, site, &second) == ZR_STRICT_OK);
    assert(ZrStrict_PoolRent(&pool, &value, site, &third) == ZR_STRICT_OK);
    assert(fixture.allocations == 2 && first.slab == second.slab && first.slab != third.slab);
    assert(ZrStrict_PoolRent(&pool, &value, site, &next) == ZR_STRICT_OUT_OF_MEMORY && !next.rental);
    assert(ZrStrict_PoolRent(&unrelated, &value, site, &unrelatedHandle) == ZR_STRICT_OK);
    assert(!ZrStrict_PoolContains(&unrelated, first) && !ZrStrict_PoolContains(&pool, unrelatedHandle));
    assert(ZrStrict_PoolClose(&unrelated, NULL, NULL) == ZR_STRICT_OK);
    finish(&otherRuntime, NULL);

    const ZrStrictPoolHandle copy = first;
    ZrStrictPoolView viewA = {0}, viewB = {0};
    assert(ZrStrict_PoolOpenView(&pool, first, 0, &viewA) == ZR_STRICT_OK);
    assert((uintptr_t)ZrStrict_PoolViewData(&viewA) % layout.alignment == 0);
    assert(ZrStrict_PoolOpenView(&pool, first, 1, &viewB) == ZR_STRICT_BUSY);
    assert(ZrStrict_PoolOpenView(&pool, first, 0, &viewB) == ZR_STRICT_OK);
    assert(ZrStrict_PoolRecycle(&pool, copy) == ZR_STRICT_OK);
    assert(!ZrStrict_PoolContains(&pool, first) && ZrStrict_PoolRecycle(&pool, first) == ZR_STRICT_EXPIRED);
    ZrStrictPoolView expired = {0};
    assert(ZrStrict_PoolOpenView(&pool, first, 0, &expired) == ZR_STRICT_EXPIRED && !expired.pool);
    assert(((const Payload *)ZrStrict_PoolViewData(&viewA))->value == 42);
    assert(ZrStrict_PoolViewDataMut(&viewA) == NULL);
    assert(ZrStrict_PoolClose(&pool, NULL, NULL) == ZR_STRICT_BUSY);
    assert(ZrStrict_PoolCloseView(&viewA) == ZR_STRICT_OK && fixture.drops == 0);
    assert(ZrStrict_PoolCloseView(&viewB) == ZR_STRICT_OK && fixture.drops == 1);
    ZrStrictPoolHandle reused = {0};
    assert(ZrStrict_PoolRent(&pool, &value, site, &reused) == ZR_STRICT_OK);
    assert(reused.slab == first.slab && reused.slot == first.slot && reused.rental != first.rental);
    assert(!ZrStrict_PoolContains(&pool, first) && ZrStrict_PoolContains(&pool, reused));
    assert(ZrStrict_PoolRecycle(&pool, reused) == ZR_STRICT_OK && fixture.drops == 2);
    assert(ZrStrict_PoolRecycle(&pool, second) == ZR_STRICT_OK && fixture.drops == 3);
    assert(fixture.frees == 0 && ZrStrict_PoolShrink(&pool) == 1 && fixture.frees == 1);

    fixture.failAllocation = 1;
    assert(ZrStrict_PoolRent(&pool, &value, site, &next) == ZR_STRICT_OUT_OF_MEMORY && !next.rental);
    fixture.failAllocation = 0;
    assert(ZrStrict_PoolRent(&pool, &fault, site, &next) == ZR_STRICT_OK);
    assert(!ZrStrict_PoolContains(&pool, copy) && next.slab != first.slab);
    assert(ZrStrict_PoolOpenView(&pool, next, 1, &viewA) == ZR_STRICT_OK);
    assert(((Payload *)ZrStrict_PoolViewDataMut(&viewA))->value == -1);
    assert(ZrStrict_PoolCloseView(&viewA) == ZR_STRICT_OK);
    assert(ZrStrict_PoolClose(&pool, report_rental, &fixture) == ZR_STRICT_FAULT);
    assert(fixture.survivors == 2 && fixture.drops == 5);

    ZrStrictPoolHandle reopened = {0};
    assert(ZrStrict_PoolInit(&fixture.runtime, &pool, &layout, 2, 3) == ZR_STRICT_OK);
    assert(ZrStrict_PoolRent(&pool, &value, site, &reopened) == ZR_STRICT_OK);
    assert(!ZrStrict_PoolContains(&pool, copy) && ZrStrict_PoolContains(&pool, reopened));
    assert(ZrStrict_PoolClose(&pool, NULL, NULL) == ZR_STRICT_OK);
    finish(&fixture, NULL);
}

int main(void) {
    test_optional_pool_recycle_and_slab_lifetime();
    test_last_strong_releases_payload_and_weak_delays_storage();
    test_failed_constructor_and_destructor();
    test_cycle_survives_until_shutdown();
    test_manual_blocks_are_fallible_and_tracked();
    test_deep_release_is_iterative_and_preserves_child_fault();
    return 0;
}
