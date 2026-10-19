/* Exact nested context identity must not depend on scratch allocation or hash collisions. */
#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/syntax_context.h"
#include "syntax/syntax_context_internal.h"

typedef struct SQueryAllocatorProbe {
    SZrGlobalState *global;
    FZrAllocator allocator;
    TZrPtr userData;
    TZrSize attempts;
    TZrSize rejectAt;
    TZrBool rejectAll;
} SQueryAllocatorProbe;

static SZrState *g_state;
static SZrState *g_otherState;
static SZrSyntaxContextTable *g_tables[3];
static TZrSize g_tableCount;
static SQueryAllocatorProbe g_probe;
static SZrSyntaxContextBatch g_batch;

static TZrPtr query_allocator(
        TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
        TZrSize newSize, TZrInt64 flag) {
    SQueryAllocatorProbe *probe = (SQueryAllocatorProbe *)userData;

    if (newSize != 0u && flag == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        probe->attempts++;
        if (probe->rejectAll || probe->attempts == probe->rejectAt) {
            return ZR_NULL;
        }
    }
    return probe->allocator(
            probe->userData, pointer, originalSize, newSize, flag);
}

static void begin_probe(SZrGlobalState *global, TZrBool rejectAll,
                        TZrSize rejectAt) {
    memset(&g_probe, 0, sizeof(g_probe));
    memset(&g_batch, 0, sizeof(g_batch));
    g_probe.global = global;
    g_probe.allocator = global->allocator;
    g_probe.userData = global->userAllocationArguments;
    g_probe.rejectAll = rejectAll;
    g_probe.rejectAt = rejectAt;
    global->allocator = query_allocator;
    global->userAllocationArguments = &g_probe;
}

static void end_probe(void) {
    if (g_probe.global != ZR_NULL) {
        g_probe.global->allocator = g_probe.allocator;
        g_probe.global->userAllocationArguments = g_probe.userData;
        g_probe.global = ZR_NULL;
    }
}

void setUp(void) {
    memset(g_tables, 0, sizeof(g_tables));
    memset(&g_probe, 0, sizeof(g_probe));
    g_tableCount = 0u;
    g_otherState = ZR_NULL;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    end_probe();
    ZrParser_SyntaxContext_AbortBatch(&g_batch);
    while (g_tableCount != 0u) {
        ZrParser_SyntaxContextTable_Release(g_tables[--g_tableCount]);
    }
    if (g_otherState != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_otherState);
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
    }
}

static SZrSyntaxContextTable *new_table(SZrState *state) {
    SZrSyntaxContextTable *table;

    TEST_ASSERT_TRUE(g_tableCount < sizeof(g_tables) / sizeof(g_tables[0]));
    table = ZrParser_SyntaxContextTable_Create(state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(table);
    g_tables[g_tableCount++] = table;
    return table;
}

static SZrSyntaxContext context_with_sites(
        SZrSyntaxContextTable *table, TZrUInt64 stableIdentity,
        TZrSize callStart, SZrSyntaxContext callSite,
        SZrSyntaxContext definitionSite) {
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId id = 0u;
    SZrSyntaxContext result;

    memset(&info, 0, sizeof(info));
    info.stableIdentity = stableIdentity;
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    info.callSiteSpan.startOffset = callStart;
    info.callSiteContext = callSite;
    info.definitionSiteContext = definitionSite;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(table, &info, &id));
    result = ZrParser_SyntaxContext_ApplyMark(
            ZrParser_SyntaxContext_TableRoot(table), id, ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(result));
    return result;
}

static SZrSyntaxContext base_context(
        SZrSyntaxContextTable *table, TZrUInt64 stableIdentity,
        TZrSize callStart) {
    SZrSyntaxContext root = ZrParser_SyntaxContext_TableRoot(table);
    return context_with_sites(table, stableIdentity, callStart, root, root);
}

static SZrSyntaxContext wrap_context(
        SZrSyntaxContextTable *table, SZrSyntaxContext site,
        TZrBool definitionSite) {
    SZrSyntaxContext root = ZrParser_SyntaxContext_TableRoot(table);
    return context_with_sites(
            table, UINT64_C(0x2000), 0u,
            definitionSite ? root : site, definitionSite ? site : root);
}

static void test_equivalent_origins_on_distinct_expansions_are_equal(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x1000), 0u);

    TEST_ASSERT_NOT_EQUAL(a.markId, b.markId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(a, b));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            wrap_context(table, a, ZR_FALSE), wrap_context(table, b, ZR_FALSE)));
}

static void assert_nested_collision_is_unequal(TZrBool definitionSite) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x1000), 1u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x1000), 2u);
    SZrSyntaxContext x;
    SZrSyntaxContext y;

    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(a, b));
    TEST_ASSERT_EQUAL_UINT64(ZrParser_SyntaxContext_StableHash(a),
                             ZrParser_SyntaxContext_StableHash(b));
    x = wrap_context(table, a, definitionSite);
    y = wrap_context(table, b, definitionSite);
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(x, y));
}

static void test_call_site_hash_collision_is_not_identity(void) {
    assert_nested_collision_is_unequal(ZR_FALSE);
}

static void test_definition_site_hash_collision_is_not_identity(void) {
    assert_nested_collision_is_unequal(ZR_TRUE);
}

static void test_equal_unequal_sites_survives_allocation_failure(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x1001), 0u);
    SZrSyntaxContext x = wrap_context(table, a, ZR_FALSE);
    SZrSyntaxContext y = wrap_context(table, b, ZR_FALSE);
    TZrBool equal;

    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(x, y));
    begin_probe(g_state->global, ZR_TRUE, 0u);
    equal = ZrParser_SyntaxContext_Equal(x, y);
    end_probe();
    TEST_ASSERT_FALSE(equal);
    TEST_ASSERT_EQUAL_UINT64(0u, g_probe.attempts);
}

static void test_equal_equivalent_sites_survives_one_allocation_failure(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxContext x = wrap_context(table, a, ZR_TRUE);
    SZrSyntaxContext y = wrap_context(table, b, ZR_TRUE);
    TZrBool equal;

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(x, y));
    begin_probe(g_state->global, ZR_FALSE, 1u);
    equal = ZrParser_SyntaxContext_Equal(x, y);
    end_probe();
    TEST_ASSERT_TRUE(equal);
    TEST_ASSERT_EQUAL_UINT64(0u, g_probe.attempts);
}

static void test_stable_hash_survives_allocation_failure(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext context = base_context(table, UINT64_C(0x1000), 0u);
    TZrUInt64 expected = ZrParser_SyntaxContext_StableHash(context);
    TZrUInt64 actual;

    TEST_ASSERT_NOT_EQUAL(0u, expected);
    begin_probe(g_state->global, ZR_TRUE, 0u);
    actual = ZrParser_SyntaxContext_StableHash(context);
    end_probe();
    TEST_ASSERT_EQUAL_UINT64(expected, actual);
    TEST_ASSERT_EQUAL_UINT64(0u, g_probe.attempts);
}

static void test_queries_allocate_nothing_with_exhausted_heap_budget(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxContext x = wrap_context(table, a, ZR_FALSE);
    SZrSyntaxContext y = wrap_context(table, b, ZR_FALSE);
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(table);
    SZrParserComptimeBudgetUsage before = budget->usage;
    TZrUInt64 hash;
    TZrBool equal;

    budget->limits.heapBytes = budget->usage.heapBytes;
    begin_probe(g_state->global, ZR_FALSE, 0u);
    hash = ZrParser_SyntaxContext_StableHash(x);
    equal = ZrParser_SyntaxContext_Equal(x, y);
    end_probe();
    TEST_ASSERT_NOT_EQUAL(0u, hash);
    TEST_ASSERT_TRUE(equal);
    TEST_ASSERT_EQUAL_UINT64(0u, g_probe.attempts);
    TEST_ASSERT_EQUAL_MEMORY(&before, &budget->usage, sizeof(before));
}

static void assert_import_retains_colliding_site(TZrBool definitionSite) {
    SZrSyntaxContextTable *source = new_table(g_state);
    SZrSyntaxContextTable *destination = new_table(g_state);
    SZrSyntaxContext a = base_context(source, UINT64_C(0x1000), 1u);
    SZrSyntaxContext b = base_context(source, UINT64_C(0x1000), 2u);
    SZrSyntaxContext x = wrap_context(source, a, definitionSite);
    SZrSyntaxContext y = wrap_context(source, b, definitionSite);
    SZrSyntaxContext importedX;
    SZrSyntaxContext importedY;
    const SZrSyntaxExpansionInfo *origin;
    SZrSyntaxContext retainedSite;
    TZrSize countBefore;

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(destination, x, &importedX));
    countBefore = destination->expansionCount;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(destination, y, &importedY));
    TEST_ASSERT_EQUAL_UINT64(countBefore + 2u, destination->expansionCount);
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(importedX, importedY));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(importedY));
    TEST_ASSERT_NOT_EQUAL(0u, importedY.markId);
    TEST_ASSERT_EQUAL_PTR(destination, importedY.table);
    origin = ZrParser_SyntaxContextTable_Expansion(destination,
            destination->marks[importedY.markId - 1u].markerId);
    TEST_ASSERT_NOT_NULL(origin);
    retainedSite = definitionSite ? origin->definitionSiteContext : origin->callSiteContext;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(retainedSite));
    TEST_ASSERT_NOT_EQUAL(0u, retainedSite.markId);
    TEST_ASSERT_EQUAL_PTR(destination, retainedSite.table);
    origin = ZrParser_SyntaxContextTable_Expansion(destination,
            destination->marks[retainedSite.markId - 1u].markerId);
    TEST_ASSERT_NOT_NULL(origin);
    TEST_ASSERT_EQUAL_UINT64(2u, origin->callSiteSpan.startOffset);
}

static void test_import_retains_colliding_call_site(void) {
    assert_import_retains_colliding_site(ZR_FALSE);
}

static void test_import_retains_colliding_definition_site(void) {
    assert_import_retains_colliding_site(ZR_TRUE);
}

static void test_repeat_import_never_allocates_in_source_owner(void) {
    SZrSyntaxContextTable *source = new_table(g_state);
    SZrSyntaxContext site = base_context(source, UINT64_C(0x1000), 0u);
    SZrSyntaxContext context = wrap_context(source, site, ZR_FALSE);
    SZrSyntaxContextTable *destination;
    SZrSyntaxContext first;
    SZrSyntaxContext second;
    TZrSize countBefore;
    TZrBool imported;

    g_otherState = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_otherState);
    destination = new_table(g_otherState);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(destination, context, &first));
    countBefore = destination->expansionCount;
    begin_probe(g_state->global, ZR_TRUE, 0u);
    imported = ZrParser_SyntaxContext_Import(destination, context, &second);
    end_probe();
    TEST_ASSERT_TRUE(imported);
    TEST_ASSERT_EQUAL_UINT64(countBefore, destination->expansionCount);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(first, second));
    TEST_ASSERT_EQUAL_UINT64(0u, g_probe.attempts);
}

static void test_roots_and_invalid_contexts_preserve_query_contract(void) {
    SZrSyntaxContextTable *a = new_table(g_state);
    SZrSyntaxContextTable *b = new_table(g_state);
    SZrSyntaxContext root = ZrParser_SyntaxContext_Root();
    SZrSyntaxContext invalid = {a, ZR_SYNTAX_CONTEXT_INVALID_MARK_ID};

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(root, ZrParser_SyntaxContext_TableRoot(a)));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(root, ZrParser_SyntaxContext_TableRoot(b)));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(invalid, invalid));
    TEST_ASSERT_EQUAL_UINT64(0u, ZrParser_SyntaxContext_StableHash(invalid));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(
            base_context(a, UINT64_C(0x1000), 0u),
            base_context(b, UINT64_C(0x1000), 0u)));
}

static SZrSyntaxContext append_existing_marker(
        SZrSyntaxContext parent, SZrSyntaxContext marker,
        EZrSyntaxTransparency transparency) {
    TZrUInt32 id = marker.table->marks[marker.markId - 1u].markerId;
    SZrSyntaxContext result = ZrParser_SyntaxContext_ApplyMark(parent, id, transparency);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(result));
    return result;
}

static void test_legacy_hash_vectors_normalization_and_relocation(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContextTable *destination = new_table(g_state);
    SZrSyntaxContext base = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId id = 0u;
    SZrSyntaxContext chain;
    SZrSyntaxContext normalized;
    SZrSyntaxContext imported;

    memset(&info, 0, sizeof(info));
    info.parentExpansionId = table->marks[base.markId - 1u].markerId;
    info.definitionModule = "pkg/module";
    info.definitionModuleLength = 10u;
    info.callSiteSource = "call.zr";
    info.callSiteSourceLength = 7u;
    info.definitionSiteSource = "def.zr";
    info.definitionSiteSourceLength = 6u;
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(table, &info, &id));
    chain = ZrParser_SyntaxContext_ApplyMark(base, id, ZR_SYNTAX_SEMI_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(chain));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(1469598103934665603),
            ZrParser_SyntaxContext_StableHash(ZrParser_SyntaxContext_Root()));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(12200770851240493209),
            ZrParser_SyntaxContext_StableHash(base));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(11359362221099775362),
            ZrParser_SyntaxContext_StableHash(chain));
    normalized = ZrParser_SyntaxContext_Normalize(chain, ZR_SYNTAX_NAMESPACE_ITEM);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(base, normalized));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(12200770851240493209),
            ZrParser_SyntaxContext_StableHash(normalized));
    base_context(destination, UINT64_C(0x9999), 0u);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(destination, chain, &imported));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(11359362221099775362),
            ZrParser_SyntaxContext_StableHash(imported));
}

static void test_normalize_initializes_multiple_pending_equivalent_marks(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x3000), 0u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x3001), 0u);
    SZrSyntaxContext c = base_context(table, UINT64_C(0x3002), 0u);
    SZrSyntaxContext semi = base_context(table, UINT64_C(0x3003), 0u);
    SZrSyntaxContext transparent = base_context(table, UINT64_C(0x3004), 0u);
    SZrSyntaxContext a2 = base_context(table, UINT64_C(0x3000), 0u);
    SZrSyntaxContext expected = append_existing_marker(a2, b, ZR_SYNTAX_OPAQUE);
    SZrSyntaxContext expectedItem;
    SZrSyntaxContext actual;
    SZrSyntaxContext normalized;
    TZrSize countBefore;

    expectedItem = append_existing_marker(expected, c, ZR_SYNTAX_OPAQUE);
    expected = append_existing_marker(expected, semi, ZR_SYNTAX_SEMI_OPAQUE);
    expected = append_existing_marker(expected, c, ZR_SYNTAX_OPAQUE);
    actual = append_existing_marker(a, transparent, ZR_SYNTAX_TRANSPARENT);
    actual = append_existing_marker(actual, b, ZR_SYNTAX_OPAQUE);
    actual = append_existing_marker(actual, semi, ZR_SYNTAX_SEMI_OPAQUE);
    actual = append_existing_marker(actual, c, ZR_SYNTAX_OPAQUE);
    countBefore = table->markCount;
    normalized = ZrParser_SyntaxContext_Normalize(actual, ZR_SYNTAX_NAMESPACE_LOCAL);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(normalized));
    TEST_ASSERT_EQUAL_UINT64(countBefore + 3u, table->markCount);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(expected, normalized));
    TEST_ASSERT_EQUAL_UINT64(ZrParser_SyntaxContext_StableHash(expected),
            ZrParser_SyntaxContext_StableHash(normalized));
    normalized = ZrParser_SyntaxContext_Normalize(actual, ZR_SYNTAX_NAMESPACE_ITEM);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(expectedItem, normalized));
    TEST_ASSERT_EQUAL_UINT64(ZrParser_SyntaxContext_StableHash(expectedItem),
            ZrParser_SyntaxContext_StableHash(normalized));
}

static void test_deep_site_context_identity_is_exact_without_query_allocations(void) {
    enum { DEPTH = 1024 };
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x1000), 1u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x1000), 2u);
    SZrSyntaxContext equivalent = base_context(table, UINT64_C(0x1000), 1u);
    TZrBool unequalResult;
    TZrBool equalResult;
    TZrUInt64 firstHash;
    TZrUInt64 secondHash;

    for (TZrSize index = 0u; index < DEPTH; index++) {
        a = wrap_context(table, a, (index & 1u) != 0u);
        b = wrap_context(table, b, (index & 1u) != 0u);
        equivalent = wrap_context(table, equivalent, (index & 1u) != 0u);
    }
    begin_probe(g_state->global, ZR_TRUE, 0u);
    unequalResult = ZrParser_SyntaxContext_Equal(a, b);
    equalResult = ZrParser_SyntaxContext_Equal(a, equivalent);
    firstHash = ZrParser_SyntaxContext_StableHash(a);
    secondHash = ZrParser_SyntaxContext_StableHash(b);
    end_probe();
    TEST_ASSERT_FALSE(unequalResult);
    TEST_ASSERT_TRUE(equalResult);
    TEST_ASSERT_EQUAL_UINT64(firstHash, secondHash);
    TEST_ASSERT_EQUAL_UINT64(0u, g_probe.attempts);
}

static void test_raw_parent_expansion_identity_remains_distinct(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext a = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxContext b = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId firstId = 0u;
    TZrSyntaxExpansionId secondId = 0u;
    SZrSyntaxContext first;
    SZrSyntaxContext second;

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(a, b));
    memset(&info, 0, sizeof(info));
    info.stableIdentity = UINT64_C(0x2000);
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    info.parentExpansionId = table->marks[a.markId - 1u].markerId;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(table, &info, &firstId));
    info.parentExpansionId = table->marks[b.markId - 1u].markerId;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(table, &info, &secondId));
    first = ZrParser_SyntaxContext_ApplyMark(ZrParser_SyntaxContext_TableRoot(table), firstId, ZR_SYNTAX_OPAQUE);
    second = ZrParser_SyntaxContext_ApplyMark(ZrParser_SyntaxContext_TableRoot(table), secondId, ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(first));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(second));
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Equal(first, second));
    TEST_ASSERT_EQUAL_UINT64(ZrParser_SyntaxContext_StableHash(first),
            ZrParser_SyntaxContext_StableHash(second));
}

static void test_batch_growth_abort_preserves_identity_and_retry(void) {
    SZrSyntaxContextTable *table = new_table(g_state);
    SZrSyntaxContext original = base_context(table, UINT64_C(0x1000), 0u);
    SZrSyntaxMarkRecord *marksBefore = table->marks;
    SZrSyntaxExpansionRecord *expansionsBefore = table->expansions;
    TZrSize markCountBefore = table->markCount;
    TZrSize expansionCountBefore = table->expansionCount;
    TZrSize markCapacityBefore = table->markCapacity;
    TZrSize expansionCapacityBefore = table->expansionCapacity;
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(table);
    SZrParserComptimeBudgetUsage usageBefore = budget->usage;
    SZrSyntaxContext candidate = original;
    TZrBool equal;

    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_BeginBatch(table, &g_batch));
    for (TZrSize index = 0u; index < 20u; index++) {
        candidate = base_context(table, UINT64_C(0x1000), 0u);
    }
    equal = ZrParser_SyntaxContext_Equal(original, candidate);
    ZrParser_SyntaxContext_AbortBatch(&g_batch);
    TEST_ASSERT_TRUE(equal);
    TEST_ASSERT_EQUAL_PTR(marksBefore, table->marks);
    TEST_ASSERT_EQUAL_PTR(expansionsBefore, table->expansions);
    TEST_ASSERT_EQUAL_UINT64(markCountBefore, table->markCount);
    TEST_ASSERT_EQUAL_UINT64(expansionCountBefore, table->expansionCount);
    TEST_ASSERT_EQUAL_MEMORY(&usageBefore, &budget->usage, sizeof(usageBefore));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(12200770851240493209),
            ZrParser_SyntaxContext_StableHash(original));
    for (TZrSize index = 0u; index < 20u; index++) {
        candidate = base_context(table, UINT64_C(0x1000), 0u);
        TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(original, candidate));
        TEST_ASSERT_EQUAL_UINT64(ZrParser_SyntaxContext_StableHash(original),
                ZrParser_SyntaxContext_StableHash(candidate));
    }
    TEST_ASSERT_EQUAL_UINT64(markCountBefore + 20u, table->markCount);
    TEST_ASSERT_EQUAL_UINT64(expansionCountBefore + 20u, table->expansionCount);
    TEST_ASSERT_TRUE(table->markCapacity > markCapacityBefore);
    TEST_ASSERT_TRUE(table->expansionCapacity > expansionCapacityBefore);
}

static void test_import_growth_failure_preserves_identity_and_retry(void) {
    SZrSyntaxContextTable *source = new_table(g_state);
    SZrSyntaxContextTable *destination = new_table(g_state);
    SZrSyntaxContext context = base_context(source, UINT64_C(0x1000), 0u);
    SZrSyntaxContext original = base_context(destination, UINT64_C(0x1000), 0u);
    SZrSyntaxContext output = ZrParser_SyntaxContext_TableRoot(destination);
    SZrSyntaxMarkRecord *marksBefore = destination->marks;
    SZrSyntaxExpansionRecord *expansionsBefore = destination->expansions;
    SZrParserComptimeBudget *budget = ZrParser_SyntaxContext_Budget(destination);
    SZrParserComptimeBudgetUsage usageBefore = budget->usage;
    TZrUInt64 expansionLimit = budget->limits.expansionCount;

    for (TZrSize index = 0u; index < 20u; index++) {
        context = wrap_context(source, context, ZR_FALSE);
    }
    budget->limits.expansionCount = usageBefore.expansionCount + 5u;
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Import(destination, context, &output));
    TEST_ASSERT_EQUAL_PTR(marksBefore, destination->marks);
    TEST_ASSERT_EQUAL_PTR(expansionsBefore, destination->expansions);
    TEST_ASSERT_EQUAL_UINT64(1u, destination->markCount);
    TEST_ASSERT_EQUAL_UINT64(1u, destination->expansionCount);
    TEST_ASSERT_EQUAL_MEMORY(&usageBefore, &budget->usage, sizeof(usageBefore));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(12200770851240493209),
            ZrParser_SyntaxContext_StableHash(original));
    budget->limits.expansionCount = expansionLimit;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(destination, context, &output));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(output));
    TEST_ASSERT_EQUAL_UINT64(21u, destination->expansionCount);
    TEST_ASSERT_EQUAL_UINT64(21u, destination->markCount);
    TEST_ASSERT_EQUAL_UINT64(ZrParser_SyntaxContext_StableHash(context),
            ZrParser_SyntaxContext_StableHash(output));
    for (TZrSize index = 0u; index < 20u; index++) {
        const SZrSyntaxExpansionInfo *origin;

        TEST_ASSERT_EQUAL_PTR(destination, output.table);
        TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(output));
        TEST_ASSERT_NOT_EQUAL(0u, output.markId);
        origin = ZrParser_SyntaxContextTable_Expansion(destination,
                destination->marks[output.markId - 1u].markerId);
        TEST_ASSERT_NOT_NULL(origin);
        TEST_ASSERT_EQUAL_UINT64(UINT64_C(0x2000), origin->stableIdentity);
        TEST_ASSERT_EQUAL_UINT32(0u, origin->definitionSiteContext.markId);
        output = origin->callSiteContext;
    }
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(original, output));
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(12200770851240493209),
            ZrParser_SyntaxContext_StableHash(original));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_equivalent_origins_on_distinct_expansions_are_equal);
    RUN_TEST(test_call_site_hash_collision_is_not_identity);
    RUN_TEST(test_definition_site_hash_collision_is_not_identity);
    RUN_TEST(test_equal_unequal_sites_survives_allocation_failure);
    RUN_TEST(test_equal_equivalent_sites_survives_one_allocation_failure);
    RUN_TEST(test_stable_hash_survives_allocation_failure);
    RUN_TEST(test_queries_allocate_nothing_with_exhausted_heap_budget);
    RUN_TEST(test_import_retains_colliding_call_site);
    RUN_TEST(test_import_retains_colliding_definition_site);
    RUN_TEST(test_repeat_import_never_allocates_in_source_owner);
    RUN_TEST(test_roots_and_invalid_contexts_preserve_query_contract);
    RUN_TEST(test_legacy_hash_vectors_normalization_and_relocation);
    RUN_TEST(test_normalize_initializes_multiple_pending_equivalent_marks);
    RUN_TEST(test_deep_site_context_identity_is_exact_without_query_allocations);
    RUN_TEST(test_raw_parent_expansion_identity_remains_distinct);
    RUN_TEST(test_batch_growth_abort_preserves_identity_and_retry);
    RUN_TEST(test_import_growth_failure_preserves_identity_and_retry);
    return UNITY_END();
}
