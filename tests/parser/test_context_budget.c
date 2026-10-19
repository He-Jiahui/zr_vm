/* Independent syntax-context budget, identity, and deep-import coverage. */
#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/syntax_context.h"
#include "syntax/syntax_context_internal.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrSyntaxContextTable *new_table(
        SZrParserComptimeBudget *budget) {
    SZrSyntaxContextTable *table =
            ZrParser_SyntaxContextTable_Create(g_state->global, budget);
    TEST_ASSERT_NOT_NULL(table);
    return table;
}

static TZrSyntaxExpansionId register_expansion(
        SZrSyntaxContextTable *table,
        TZrUInt64 stableIdentity,
        TZrSyntaxExpansionId parentExpansionId,
        TZrSize callStart) {
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId expansionId = 0u;

    memset(&info, 0, sizeof(info));
    info.parentExpansionId = parentExpansionId;
    info.stableIdentity = stableIdentity;
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();
    info.callSiteSpan.startOffset = callStart;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            table, &info, &expansionId));
    return expansionId;
}

static void test_table_create_heap_failure_is_atomic(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits = {0};
    SZrSyntaxContextTable *table;
    SZrParserComptimeBudgetUsage usageBefore;

    limits.heapBytes = 0u;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    usageBefore = budget.usage;
    table = ZrParser_SyntaxContextTable_Create(g_state->global, &budget);
    TEST_ASSERT_NULL(table);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.heapBytes, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.nodeCount, budget.usage.nodeCount);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.expansionCount,
                             budget.usage.expansionCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            budget.exceededResource);
}

static void test_apply_mark_heap_failure_is_atomic(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits = {0};
    SZrSyntaxContextTable *table;
    SZrSyntaxContext root;
    SZrSyntaxContext failed;
    SZrSyntaxContext invalidTransparency;
    TZrUInt64 usageBefore;
    TZrUInt64 nodeBefore;
    TZrSyntaxExpansionId expansionId;

    limits.heapBytes = UINT64_MAX;
    limits.nodeCount = 4u;
    limits.expansionCount = 4u;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    table = new_table(&budget);
    expansionId = register_expansion(table, UINT64_C(0xA001), 0u, 0u);
    usageBefore = budget.usage.heapBytes;
    nodeBefore = budget.usage.nodeCount;
    budget.limits.heapBytes = usageBefore;
    root = ZrParser_SyntaxContext_TableRoot(table);
    failed = ZrParser_SyntaxContext_ApplyMark(
            root, expansionId, ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_IsValid(failed));
    TEST_ASSERT_EQUAL_UINT64(usageBefore, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(nodeBefore, budget.usage.nodeCount);
    invalidTransparency = ZrParser_SyntaxContext_ApplyMark(
            root, expansionId, (EZrSyntaxTransparency)99);
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_IsValid(invalidTransparency));
    TEST_ASSERT_EQUAL_UINT64(usageBefore, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(nodeBefore, budget.usage.nodeCount);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_import_failure_rolls_back_budget_and_records(void) {
    SZrParserComptimeBudget sourceBudget;
    SZrParserComptimeBudget destinationBudget;
    SZrParserComptimeBudgetLimits sourceLimits = {0};
    SZrParserComptimeBudgetLimits destinationLimits = {0};
    SZrSyntaxContextTable *source;
    SZrSyntaxContextTable *destination;
    SZrSyntaxContext sourceContext;
    SZrSyntaxContext output = ZrParser_SyntaxContext_Root();
    TZrUInt64 heapBefore;
    TZrUInt64 expansionBefore;
    TZrSyntaxExpansionId expansionId;

    sourceLimits.heapBytes = UINT64_MAX;
    sourceLimits.expansionCount = 4u;
    sourceLimits.nodeCount = 4u;
    destinationLimits.heapBytes = UINT64_MAX;
    destinationLimits.expansionCount = 0u;
    destinationLimits.nodeCount = 4u;
    ZrParser_ComptimeBudget_Init(&sourceBudget, &sourceLimits);
    ZrParser_ComptimeBudget_Init(&destinationBudget, &destinationLimits);
    source = new_table(&sourceBudget);
    destination = new_table(&destinationBudget);
    destinationBudget.limits.heapBytes = destinationBudget.usage.heapBytes;
    expansionId = register_expansion(source, UINT64_C(0xA002), 0u, 0u);
    sourceContext = ZrParser_SyntaxContext_MixedSite(source, expansionId);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(sourceContext));
    heapBefore = destinationBudget.usage.heapBytes;
    expansionBefore = destinationBudget.usage.expansionCount;
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Import(
            destination, sourceContext, &output));
    TEST_ASSERT_EQUAL_UINT64(heapBefore, destinationBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(
            expansionBefore, destinationBudget.usage.expansionCount);
    TEST_ASSERT_NULL(ZrParser_SyntaxContextTable_Expansion(destination, 1u));
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(
            output, ZrParser_SyntaxContext_Root()));
    ZrParser_SyntaxContextTable_Release(source);
    ZrParser_SyntaxContextTable_Release(destination);
}

static void test_register_failure_preserves_budget_diagnostics(void) {
    SZrParserComptimeBudget budget;
    SZrParserComptimeBudgetLimits limits = {0};
    SZrSyntaxContextTable *table;
    SZrSyntaxExpansionInfo info;
    TZrSyntaxExpansionId expansionId = 777u;
    SZrSyntaxExpansionRecord *expansionsBefore;
    TZrSize expansionCapacityBefore;
    SZrParserComptimeBudgetUsage usageBefore;

    limits.heapBytes = UINT64_MAX;
    limits.expansionCount = 0u;
    limits.nodeCount = 4u;
    ZrParser_ComptimeBudget_Init(&budget, &limits);
    table = new_table(&budget);
    expansionsBefore = table->expansions;
    expansionCapacityBefore = table->expansionCapacity;
    usageBefore = budget.usage;
    memset(&info, 0, sizeof(info));
    info.stableIdentity = UINT64_C(0xA005);
    info.callSiteSpan = ZrParser_TokenSpan_None();
    info.definitionSiteSpan = ZrParser_TokenSpan_None();

    TEST_ASSERT_FALSE(ZrParser_SyntaxContextTable_RegisterExpansion(
            table, &info, &expansionId));
    TEST_ASSERT_EQUAL_UINT32(777u, expansionId);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.heapBytes, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.expansionCount,
                             budget.usage.expansionCount);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.nodeCount, budget.usage.nodeCount);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.aggregateCount,
                             budget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(0u, table->expansionCount);
    TEST_ASSERT_EQUAL_UINT64(0u, table->markCount);
    TEST_ASSERT_EQUAL_PTR(expansionsBefore, table->expansions);
    TEST_ASSERT_EQUAL_UINT64(expansionCapacityBefore,
                             table->expansionCapacity);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT,
            budget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(0u, budget.exceededLimit);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.expansionCount + 1u,
                             budget.requestedUsage);
    ZrParser_SyntaxContextTable_Release(table);
}

static void test_import_failure_preserves_budget_diagnostics(void) {
    SZrParserComptimeBudget sourceBudget;
    SZrParserComptimeBudget destinationBudget;
    SZrParserComptimeBudgetLimits sourceLimits = {0};
    SZrParserComptimeBudgetLimits destinationLimits = {0};
    SZrSyntaxContextTable *source;
    SZrSyntaxContextTable *destination;
    SZrSyntaxContext sourceContext;
    SZrSyntaxContext output;
    SZrSyntaxExpansionRecord *expansionsBefore;
    SZrSyntaxMarkRecord *marksBefore;
    TZrSize expansionCapacityBefore;
    TZrSize markCapacityBefore;
    SZrParserComptimeBudgetUsage usageBefore;
    TZrSyntaxExpansionId expansionId;

    sourceLimits.heapBytes = UINT64_MAX;
    sourceLimits.expansionCount = 4u;
    sourceLimits.nodeCount = 4u;
    destinationLimits.heapBytes = UINT64_MAX;
    destinationLimits.expansionCount = 0u;
    destinationLimits.nodeCount = 4u;
    ZrParser_ComptimeBudget_Init(&sourceBudget, &sourceLimits);
    ZrParser_ComptimeBudget_Init(&destinationBudget, &destinationLimits);
    source = new_table(&sourceBudget);
    destination = new_table(&destinationBudget);
    expansionId = register_expansion(source, UINT64_C(0xA006), 0u, 0u);
    sourceContext = ZrParser_SyntaxContext_ApplyMark(
            ZrParser_SyntaxContext_TableRoot(source), expansionId,
            ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(sourceContext));
    output = ZrParser_SyntaxContext_TableRoot(destination);
    expansionsBefore = destination->expansions;
    marksBefore = destination->marks;
    expansionCapacityBefore = destination->expansionCapacity;
    markCapacityBefore = destination->markCapacity;
    usageBefore = destinationBudget.usage;

    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Import(
            destination, sourceContext, &output));
    TEST_ASSERT_EQUAL_PTR(expansionsBefore, destination->expansions);
    TEST_ASSERT_EQUAL_PTR(marksBefore, destination->marks);
    TEST_ASSERT_EQUAL_UINT64(expansionCapacityBefore,
                             destination->expansionCapacity);
    TEST_ASSERT_EQUAL_UINT64(markCapacityBefore, destination->markCapacity);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.heapBytes,
                             destinationBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.expansionCount,
                             destinationBudget.usage.expansionCount);
    TEST_ASSERT_EQUAL_UINT64(0u, destination->expansionCount);
    TEST_ASSERT_EQUAL_UINT64(0u, destination->markCount);
    TEST_ASSERT_EQUAL_UINT32(0u, output.markId);
    TEST_ASSERT_EQUAL_PTR(destination, output.table);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT,
            destinationBudget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(0u, destinationBudget.exceededLimit);
    TEST_ASSERT_EQUAL_UINT64(usageBefore.expansionCount + 1u,
                             destinationBudget.requestedUsage);
    ZrParser_SyntaxContextTable_Release(source);
    ZrParser_SyntaxContextTable_Release(destination);
}

static void test_identity_span_and_parent_relocation_are_stable(void) {
    SZrSyntaxContextTable *source;
    SZrSyntaxContextTable *destination;
    SZrSyntaxContext sourceContext;
    SZrSyntaxContext imported;
    TZrSyntaxExpansionId parent;
    TZrSyntaxExpansionId child;
    TZrSyntaxExpansionId unrelated;
    TZrUInt64 sourceHash;

    source = new_table(ZR_NULL);
    destination = new_table(ZR_NULL);
    parent = register_expansion(source, UINT64_C(0xA003), 0u, 0u);
    child = register_expansion(source, UINT64_C(0xA004), parent, 0u);
    sourceContext = ZrParser_SyntaxContext_ApplyMark(
            ZrParser_SyntaxContext_TableRoot(source), child,
            ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(sourceContext));
    sourceHash = ZrParser_SyntaxContext_StableHash(sourceContext);
    unrelated = register_expansion(destination, UINT64_C(0xB003), 0u, 7u);
    (void)unrelated;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(
            destination, sourceContext, &imported));
    TEST_ASSERT_EQUAL_UINT64(sourceHash,
                             ZrParser_SyntaxContext_StableHash(imported));
    /* Different source origins with the same stable ID are not deduplicated. */
    register_expansion(destination, UINT64_C(0xA003), 0u, 99u);
    TEST_ASSERT_NOT_NULL(ZrParser_SyntaxContextTable_Expansion(destination, 4u));
    ZrParser_SyntaxContextTable_Release(source);
    ZrParser_SyntaxContextTable_Release(destination);
}

static void test_import_failure_restores_growth_checkpoint(void) {
    SZrParserComptimeBudget sourceBudget;
    SZrParserComptimeBudget destinationBudget;
    SZrParserComptimeBudgetLimits sourceLimits = {0};
    SZrParserComptimeBudgetLimits destinationLimits = {0};
    SZrSyntaxContextTable *source;
    SZrSyntaxContextTable *destination;
    SZrSyntaxContext sourceContext;
    SZrSyntaxContext output = ZrParser_SyntaxContext_Root();
    TZrSyntaxExpansionId parent = 0u;
    TZrSyntaxExpansionId unrelated;
    SZrSyntaxExpansionRecord *expansionsBefore;
    SZrSyntaxMarkRecord *marksBefore;
    TZrSize expansionCapacityBefore;
    TZrSize markCapacityBefore;
    TZrSize expansionCountBefore;
    TZrSize markCountBefore;

    sourceLimits.heapBytes = UINT64_MAX;
    sourceLimits.expansionCount = 64u;
    sourceLimits.nodeCount = 64u;
    destinationLimits.heapBytes = UINT64_MAX;
    destinationLimits.expansionCount = UINT64_MAX;
    destinationLimits.nodeCount = 64u;
    ZrParser_ComptimeBudget_Init(&sourceBudget, &sourceLimits);
    ZrParser_ComptimeBudget_Init(&destinationBudget, &destinationLimits);
    source = new_table(&sourceBudget);
    destination = new_table(&destinationBudget);
    unrelated = register_expansion(
            destination, UINT64_C(0xD000), 0u, 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, unrelated);
    for (TZrSize index = 0u; index < 17u; index++) {
        parent = register_expansion(
                source, UINT64_C(0xD100) + (TZrUInt64)index,
                parent, index);
    }
    sourceContext = ZrParser_SyntaxContext_ApplyMark(
            ZrParser_SyntaxContext_TableRoot(source), parent,
            ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(sourceContext));
    expansionsBefore = destination->expansions;
    expansionCapacityBefore = destination->expansionCapacity;
    expansionCountBefore = destination->expansionCount;
    marksBefore = destination->marks;
    markCapacityBefore = destination->markCapacity;
    markCountBefore = destination->markCount;
    destinationBudget.limits.expansionCount =
            destinationBudget.usage.expansionCount + 5u;
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_Import(
            destination, sourceContext, &output));
    TEST_ASSERT_EQUAL_UINT64(expansionCountBefore,
                             destination->expansionCount);
    TEST_ASSERT_EQUAL_UINT64(markCountBefore, destination->markCount);
    TEST_ASSERT_EQUAL_UINT64(expansionCapacityBefore,
                             destination->expansionCapacity);
    TEST_ASSERT_EQUAL_UINT64(markCapacityBefore, destination->markCapacity);
    TEST_ASSERT_EQUAL_PTR(expansionsBefore, destination->expansions);
    TEST_ASSERT_EQUAL_PTR(marksBefore, destination->marks);
    ZrParser_SyntaxContextTable_Release(source);
    ZrParser_SyntaxContextTable_Release(destination);
}

static void test_import_charges_capacity_growth(void) {
    SZrParserComptimeBudget sourceBudget;
    SZrParserComptimeBudget destinationBudget;
    SZrParserComptimeBudgetLimits sourceLimits = {0};
    SZrParserComptimeBudgetLimits destinationLimits = {0};
    SZrSyntaxContextTable *source;
    SZrSyntaxContextTable *destination;
    SZrSyntaxContext sourceContext;
    SZrSyntaxContext imported;
    TZrSyntaxExpansionId expansionId;
    TZrUInt64 heapBefore;
    TZrUInt64 capacityBytes;

    sourceLimits.heapBytes = UINT64_MAX;
    sourceLimits.expansionCount = 4u;
    sourceLimits.nodeCount = 4u;
    destinationLimits.heapBytes = UINT64_MAX;
    destinationLimits.expansionCount = 4u;
    destinationLimits.nodeCount = 4u;
    ZrParser_ComptimeBudget_Init(&sourceBudget, &sourceLimits);
    ZrParser_ComptimeBudget_Init(&destinationBudget, &destinationLimits);
    source = new_table(&sourceBudget);
    destination = new_table(&destinationBudget);
    expansionId = register_expansion(
            source, UINT64_C(0xD200), 0u, 0u);
    sourceContext = ZrParser_SyntaxContext_ApplyMark(
            ZrParser_SyntaxContext_TableRoot(source), expansionId,
            ZR_SYNTAX_OPAQUE);
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(sourceContext));
    heapBefore = destinationBudget.usage.heapBytes;
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(
            destination, sourceContext, &imported));
    capacityBytes = (TZrUInt64)destination->expansionCapacity *
            (TZrUInt64)sizeof(*destination->expansions);
    capacityBytes += (TZrUInt64)destination->markCapacity *
            (TZrUInt64)sizeof(*destination->marks);
    TEST_ASSERT_TRUE(destinationBudget.usage.heapBytes - heapBefore >=
                     capacityBytes);
    ZrParser_SyntaxContextTable_Release(source);
    ZrParser_SyntaxContextTable_Release(destination);
}

static void test_invalid_sites_and_deep_import_are_fail_closed(void) {
    SZrSyntaxContextTable *source;
    SZrSyntaxContextTable *destination;
    SZrSyntaxContext context;
    SZrSyntaxContext imported;
    TZrSyntaxExpansionId expansionId = 0u;

    source = new_table(ZR_NULL);
    destination = new_table(ZR_NULL);
    TEST_ASSERT_FALSE(ZrParser_SyntaxContext_IsValid(
            ZrParser_SyntaxContext_CallSite(source, 999u)));
    for (TZrSize index = 0u; index < 1024u; index++) {
        expansionId = register_expansion(
                source, UINT64_C(0xC000) + (TZrUInt64)index,
                index == 0u ? 0u : expansionId, index);
        context = ZrParser_SyntaxContext_ApplyMark(
                index == 0u
                        ? ZrParser_SyntaxContext_TableRoot(source)
                        : context,
                expansionId, ZR_SYNTAX_OPAQUE);
        TEST_ASSERT_TRUE(ZrParser_SyntaxContext_IsValid(context));
    }
    TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Import(
            destination, context, &imported));
    TEST_ASSERT_EQUAL_UINT64(
            ZrParser_SyntaxContext_StableHash(context),
            ZrParser_SyntaxContext_StableHash(imported));
    ZrParser_SyntaxContextTable_Release(source);
    ZrParser_SyntaxContextTable_Release(destination);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_table_create_heap_failure_is_atomic);
    RUN_TEST(test_apply_mark_heap_failure_is_atomic);
    RUN_TEST(test_import_failure_rolls_back_budget_and_records);
    RUN_TEST(test_register_failure_preserves_budget_diagnostics);
    RUN_TEST(test_import_failure_preserves_budget_diagnostics);
    RUN_TEST(test_identity_span_and_parent_relocation_are_stable);
    RUN_TEST(test_import_failure_restores_growth_checkpoint);
    RUN_TEST(test_import_charges_capacity_growth);
    RUN_TEST(test_invalid_sites_and_deep_import_are_fail_closed);
    return UNITY_END();
}
