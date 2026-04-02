#include <string.h>
#include <time.h>

#include "unity.h"

#include "container_test_common.h"
#include "runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/parser.h"

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                      \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                   \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

static SZrFunction *compile_test_script(SZrState *state, const char *path, const char *source) {
    SZrString *sourceName;

    if (state == ZR_NULL || path == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)path, strlen(path));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_container_fixed_array_runtime_supports_mutation_and_iteration(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Fixed Array Supports Mutation And Iteration";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var xs: int[4] = [1, 2, 3, 4];\n"
            "var sum = 0;\n"
            "xs[1] = 5;\n"
            "for (var item in xs) {\n"
            "    sum = sum + item;\n"
            "}\n"
            "return sum;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_fixed_array_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(13, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_array_runtime_supports_capacity_growth_and_structural_equality(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Array Supports Capacity Growth And Structural Equality";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<Pair<int, string>>(1);\n"
            "xs.add(new container.Pair<int, string>(2, \"b\"));\n"
            "xs.insert(0, new container.Pair<int, string>(1, \"a\"));\n"
            "xs.insert(2, new container.Pair<int, string>(3, \"c\"));\n"
            "xs.removeAt(1);\n"
            "var total = xs.length + xs.capacity + xs.indexOf(new container.Pair<int, string>(3, \"c\"));\n"
            "if (xs.contains(new container.Pair<int, string>(3, \"c\"))) {\n"
            "    total = total + 10;\n"
            "}\n"
            "return total;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_array_runtime_semantics_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(17, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_array_runtime_clear_preserves_capacity_and_missing_item_returns_null(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Array Clear Preserves Capacity And Missing Item Returns Null";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<int>(2);\n"
            "var before = 0;\n"
            "xs.add(10);\n"
            "xs.add(20);\n"
            "xs.add(30);\n"
            "before = xs.capacity;\n"
            "xs.clear();\n"
            "if (xs[0] != null) {\n"
            "    return -1;\n"
            "}\n"
            "return before * 10 + xs.capacity;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_array_clear_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(44, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_array_runtime_accepts_unary_negation_in_constructor_arguments(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Array Accepts Unary Negation In Constructor Arguments";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<int>(2 + -1);\n"
            "return xs.capacity;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_array_unary_ctor_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_array_runtime_rejects_negative_capacity(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Array Rejects Negative Capacity";
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "new container.Array<int>(-1);\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_array_negative_capacity_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_array_runtime_rejects_invalid_indexes(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Array Rejects Invalid Indexes";
    SZrState *state;
    SZrFunction *insertFunction;
    SZrFunction *setFunction;
    SZrFunction *removeFunction;
    SZrTypeValue result;
    const char *insertSource =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(1);\n"
            "xs.insert(2, 5);\n";
    const char *setSource =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(1);\n"
            "xs[1] = 5;\n";
    const char *removeSource =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.removeAt(0);\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    insertFunction = compile_test_script(state, "container_array_insert_oob_runtime_test.zr", insertSource);
    setFunction = compile_test_script(state, "container_array_set_oob_runtime_test.zr", setSource);
    removeFunction = compile_test_script(state, "container_array_remove_oob_runtime_test.zr", removeSource);
    TEST_ASSERT_NOT_NULL(insertFunction);
    TEST_ASSERT_NOT_NULL(setFunction);
    TEST_ASSERT_NOT_NULL(removeFunction);

    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, insertFunction, &result));
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, setFunction, &result));
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_Execute(state, removeFunction, &result));

    ZrCore_Function_Free(state, insertFunction);
    ZrCore_Function_Free(state, setFunction);
    ZrCore_Function_Free(state, removeFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_map_runtime_supports_pair_keys_and_value_overwrite(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Map Supports Pair Keys And Value Overwrite";
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue rawResult;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var map = new container.Map<Pair<int, string>, int>();\n"
            "var first = new container.Pair<int, string>(3, \"red\");\n"
            "var same = new container.Pair<int, string>(3, \"red\");\n"
            "map[first] = 4;\n"
            "map[same] = 7;\n"
            "var resolved = map[first];\n"
            "if (resolved == null) { return -100; }\n"
            "if (map.count == null) { return -200; }\n"
            "return resolved + map.count;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_map_pair_key_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &rawResult));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(rawResult.type));
    result = rawResult.value.nativeObject.nativeInt64;
    TEST_ASSERT_EQUAL_INT64(8, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_map_runtime_computed_access_beats_prototype_method_names(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Map Computed Access Beats Prototype Method Names";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var map = new container.Map<string, int>();\n"
            "map[\"clear\"] = 7;\n"
            "return map[\"clear\"] + map.count;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_map_method_collision_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(8, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_map_runtime_iterator_aggregates_pairs_without_order_assumptions(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Map Iterator Aggregates Pairs Without Order Assumptions";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var map = new container.Map<string, int>();\n"
            "var sum = 0;\n"
            "var seen = 0;\n"
            "map[\"r\"] = 2;\n"
            "map[\"g\"] = 3;\n"
            "map[\"b\"] = 5;\n"
            "for (var entry in map) {\n"
            "    sum = sum + entry.second;\n"
            "    seen = seen + 1;\n"
            "}\n"
            "return sum * 10 + seen;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_map_iterator_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(103, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_set_runtime_enforces_pair_uniqueness(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Set Enforces Pair Uniqueness";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var values = new container.Set<Pair<int, string>>();\n"
            "var score = 0;\n"
            "if (values.add(new container.Pair<int, string>(1, \"a\"))) { score = score + 10; }\n"
            "if (!values.add(new container.Pair<int, string>(1, \"a\"))) { score = score + 20; }\n"
            "if (values.add(new container.Pair<int, string>(2, \"b\"))) { score = score + 30; }\n"
            "for (var item in values) {\n"
            "    score = score + item.first;\n"
            "}\n"
            "return values.count * 100 + score;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_set_pair_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(263, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_pair_runtime_exposes_value_semantics(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Pair Exposes Value Semantics";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var left = new container.Pair<int, string>(1, \"a\");\n"
            "var right = new container.Pair<int, string>(1, \"b\");\n"
            "var same = new container.Pair<int, string>(1, \"a\");\n"
            "var score = 0;\n"
            "if (left.equals(same)) { score = score + 1; }\n"
            "if (left.compareTo(right) < 0) { score = score + 10; }\n"
            "if (left.hashCode() == same.hashCode()) { score = score + 100; }\n"
            "return score;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_pair_runtime_semantics_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(111, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_linked_list_runtime_detaches_removed_and_cleared_nodes(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - LinkedList Detaches Removed And Cleared Nodes";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var list = new container.LinkedList<int>();\n"
            "var first = list.addLast(10);\n"
            "var middle = list.addLast(20);\n"
            "var last = list.addLast(30);\n"
            "var score = 0;\n"
            "if (list.remove(20)) { score = score + 1; }\n"
            "if (middle.next == null) { score = score + 10; }\n"
            "if (middle.previous == null) { score = score + 100; }\n"
            "if (list.first.value == 10) { score = score + 1000; }\n"
            "if (list.last.value == 30) { score = score + 10000; }\n"
            "list.clear();\n"
            "if (first.next == null && first.previous == null && last.next == null && last.previous == null) {\n"
            "    score = score + 100000;\n"
            "}\n"
            "return score + list.count;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_linked_list_detach_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(111111, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_linked_list_runtime_empty_removals_return_null(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - LinkedList Empty Removals Return Null";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var list = new container.LinkedList<int>();\n"
            "if (list.removeFirst() == null && list.removeLast() == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_linked_list_empty_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_linked_list_runtime_remove_first_preserves_pair_values_across_function_boundary(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - LinkedList RemoveFirst Preserves Pair Values Across Function Boundary";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "buildQueue() {\n"
            "    var queue = new container.LinkedList<Pair<string, int>>();\n"
            "    queue.addLast(new container.Pair<string, int>(\"odd_hi\", 3));\n"
            "    return queue;\n"
            "}\n"
            "var queue = buildQueue();\n"
            "if (queue.count != 1) { return -10; }\n"
            "if (queue.first == null) { return -20; }\n"
            "if (queue.first.value == null) { return -30; }\n"
            "var task = queue.removeFirst();\n"
            "if (task == null) { return -1; }\n"
            "if (task.first != \"odd_hi\") { return -2; }\n"
            "if (task.second != 3) { return -3; }\n"
            "return queue.count;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_linked_list_remove_first_pair_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_container_fixed_array_runtime_supports_mutation_and_iteration);
    RUN_TEST(test_container_array_runtime_supports_capacity_growth_and_structural_equality);
    RUN_TEST(test_container_array_runtime_clear_preserves_capacity_and_missing_item_returns_null);
    RUN_TEST(test_container_array_runtime_accepts_unary_negation_in_constructor_arguments);
    RUN_TEST(test_container_array_runtime_rejects_negative_capacity);
    RUN_TEST(test_container_array_runtime_rejects_invalid_indexes);
    RUN_TEST(test_container_map_runtime_supports_pair_keys_and_value_overwrite);
    RUN_TEST(test_container_map_runtime_computed_access_beats_prototype_method_names);
    RUN_TEST(test_container_map_runtime_iterator_aggregates_pairs_without_order_assumptions);
    RUN_TEST(test_container_set_runtime_enforces_pair_uniqueness);
    RUN_TEST(test_container_pair_runtime_exposes_value_semantics);
    RUN_TEST(test_container_linked_list_runtime_detaches_removed_and_cleared_nodes);
    RUN_TEST(test_container_linked_list_runtime_empty_removals_return_null);
    RUN_TEST(test_container_linked_list_runtime_remove_first_preserves_pair_values_across_function_boundary);
    return UNITY_END();
}
