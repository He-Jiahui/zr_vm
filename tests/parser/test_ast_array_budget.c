/* Transactional heap accounting and failure atomicity for AST node arrays. */
#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "runtime_support.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/comptime_contract.h"

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

static void init_heap_budget(
        SZrParserComptimeBudget *budget, TZrUInt64 heapBytes) {
    SZrParserComptimeBudgetLimits limits;

    memset(&limits, 0, sizeof(limits));
    limits.heapBytes = heapBytes;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

static TZrUInt64 array_heap_bytes(TZrSize capacity) {
    return (TZrUInt64)sizeof(SZrAstNodeArray) +
            (TZrUInt64)capacity * (TZrUInt64)sizeof(SZrAstNode *);
}

static void init_node(SZrAstNode *node, EZrAstNodeType type) {
    memset(node, 0xA5, sizeof(*node));
    node->type = type;
}

static void test_zero_budget_rejects_array_without_usage(void) {
    SZrParserComptimeBudget budget;
    SZrAstNodeArray *array;

    init_heap_budget(&budget, 0u);
    array = ZrParser_AstNodeArray_NewBudget(g_state, 1u, &budget);

    TEST_ASSERT_NULL(array);
    TEST_ASSERT_EQUAL_UINT64(0u, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            budget.exceededResource);
}

static void test_new_budget_commits_exact_container_and_backing_bytes(void) {
    const TZrSize capacity = 3u;
    const TZrUInt64 expected = array_heap_bytes(capacity);
    SZrParserComptimeBudget budget;
    SZrAstNodeArray *array;

    init_heap_budget(&budget, expected);
    array = ZrParser_AstNodeArray_NewBudget(g_state, capacity, &budget);

    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL_UINT64(expected, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(0u, (TZrUInt64)array->count);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)capacity,
                             (TZrUInt64)array->capacity);
    TEST_ASSERT_NOT_NULL(array->nodes);

    ZrParser_AstNodeArray_Free(g_state, array);
}

static void test_growth_failure_preserves_array_and_unadopted_node(void) {
    SZrParserComptimeBudget budget;
    SZrAstNodeArray *array;
    SZrAstNode first;
    SZrAstNode candidate;
    SZrAstNode candidateBefore;
    SZrAstNode **oldNodes;
    TZrSize oldCapacity;
    TZrSize oldCount;
    TZrUInt64 committedHeap;

    init_heap_budget(&budget, UINT64_MAX);
    array = ZrParser_AstNodeArray_NewBudget(g_state, 1u, &budget);
    TEST_ASSERT_NOT_NULL(array);
    committedHeap = budget.usage.heapBytes;
    budget.limits.heapBytes = committedHeap;

    init_node(&first, ZR_AST_IDENTIFIER_LITERAL);
    init_node(&candidate, ZR_AST_INTEGER_LITERAL);
    candidateBefore = candidate;
    TEST_ASSERT_TRUE(ZrParser_AstNodeArray_TryAddBudget(
            g_state, array, &first, &budget));

    oldNodes = array->nodes;
    oldCapacity = array->capacity;
    oldCount = array->count;
    TEST_ASSERT_FALSE(ZrParser_AstNodeArray_TryAddBudget(
            g_state, array, &candidate, &budget));

    TEST_ASSERT_EQUAL_PTR(oldNodes, array->nodes);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)oldCapacity,
                             (TZrUInt64)array->capacity);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)oldCount,
                             (TZrUInt64)array->count);
    TEST_ASSERT_EQUAL_PTR(&first, array->nodes[0]);
    TEST_ASSERT_EQUAL_UINT64(committedHeap, budget.usage.heapBytes);
    TEST_ASSERT_EQUAL_MEMORY(&candidateBefore, &candidate,
                             sizeof(candidate));

    ZrParser_AstNodeArray_Free(g_state, array);
    TEST_ASSERT_EQUAL_MEMORY(&candidateBefore, &candidate,
                             sizeof(candidate));
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, first.type);
}

static void test_capacity_overflow_leaves_array_and_node_untouched(void) {
    SZrParserComptimeBudget budget;
    SZrAstNodeArray array;
    SZrAstNode node;
    SZrAstNode nodeBefore;
    SZrAstNode *sentinel;
    SZrAstNode **oldNodes;
    TZrSize oldCapacity;
    TZrSize oldCount;

    /* This capacity fits the pointer-byte multiplication but cannot double. */
    memset(&array, 0, sizeof(array));
    sentinel = &node;
    array.nodes = &sentinel;
    array.capacity = ZR_MAX_SIZE / sizeof(SZrAstNode *);
    array.count = array.capacity;
    oldNodes = array.nodes;
    oldCapacity = array.capacity;
    oldCount = array.count;
    init_node(&node, ZR_AST_STRING_LITERAL);
    nodeBefore = node;
    init_heap_budget(&budget, UINT64_MAX);

    TEST_ASSERT_FALSE(ZrParser_AstNodeArray_TryAddBudget(
            g_state, &array, &node, &budget));
    TEST_ASSERT_EQUAL_PTR(oldNodes, array.nodes);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)oldCapacity,
                             (TZrUInt64)array.capacity);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)oldCount,
                             (TZrUInt64)array.count);
    TEST_ASSERT_EQUAL_MEMORY(&nodeBefore, &node, sizeof(node));
}

static void test_legacy_new_and_add_remain_compatible(void) {
    SZrAstNodeArray *array;
    SZrAstNode node;

    init_node(&node, ZR_AST_BOOLEAN_LITERAL);
    array = ZrParser_AstNodeArray_New(g_state, 1u);
    TEST_ASSERT_NOT_NULL(array);

    ZrParser_AstNodeArray_Add(g_state, array, &node);
    TEST_ASSERT_EQUAL_UINT64(1u, (TZrUInt64)array->count);
    TEST_ASSERT_EQUAL_PTR(&node, array->nodes[0]);

    ZrParser_AstNodeArray_Free(g_state, array);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BOOLEAN_LITERAL, node.type);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_budget_rejects_array_without_usage);
    RUN_TEST(test_new_budget_commits_exact_container_and_backing_bytes);
    RUN_TEST(test_growth_failure_preserves_array_and_unadopted_node);
    RUN_TEST(test_capacity_overflow_leaves_array_and_node_untouched);
    RUN_TEST(test_legacy_new_and_add_remain_compatible);
    return UNITY_END();
}
