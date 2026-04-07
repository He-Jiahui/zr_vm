#include <string.h>
#include <time.h>

#include "unity.h"

#include "container_test_common.h"
#include "runtime_support.h"
#include "test_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
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

static char *read_reference_file(const char *relativePath, size_t *outSize) {
    return ZrTests_Reference_ReadFixture(relativePath, outSize);
}

static TZrUInt32 count_instruction_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }

    return count;
}

static TZrUInt32 count_instruction_opcode_recursive(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 count;
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return 0;
    }

    count = count_instruction_opcode(function, opcode);
    if (function->childFunctionList == ZR_NULL) {
        return count;
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        count += count_instruction_opcode_recursive(&function->childFunctionList[index], opcode);
    }

    return count;
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

static void test_container_fixed_array_runtime_reiterates_managed_elements_without_corrupting_iterator_state(void) {
    SZrTestTimer timer = {0};
    const char *summary =
            "Container Runtime - Fixed Array Reiterates Managed Elements Without Corrupting Iterator State";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var xs: string[3] = [\"a\", \"bb\", \"ccc\"];\n"
            "var total = 0;\n"
            "for (var item in xs) {\n"
            "    if (item == \"a\") { total = total + 1; }\n"
            "    if (item == \"bb\") { total = total + 2; }\n"
            "    if (item == \"ccc\") { total = total + 3; }\n"
            "}\n"
            "for (var item in xs) {\n"
            "    if (item == \"a\") { total = total + 10; }\n"
            "    if (item == \"bb\") { total = total + 20; }\n"
            "    if (item == \"ccc\") { total = total + 30; }\n"
            "}\n"
            "return total;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_fixed_array_managed_foreach_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(66, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_fixed_array_runtime_supports_nested_independent_managed_iterators(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Fixed Array Supports Nested Independent Managed Iterators";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var xs: string[3] = [\"a\", \"bb\", \"ccc\"];\n"
            "var total = 0;\n"
            "for (var left in xs) {\n"
            "    for (var right in xs) {\n"
            "        if (left == \"a\") { total = total + 1; }\n"
            "        if (left == \"bb\") { total = total + 2; }\n"
            "        if (left == \"ccc\") { total = total + 3; }\n"
            "        if (right == \"a\") { total = total + 10; }\n"
            "        if (right == \"bb\") { total = total + 20; }\n"
            "        if (right == \"ccc\") { total = total + 30; }\n"
            "    }\n"
            "}\n"
            "return total;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_fixed_array_nested_managed_foreach_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(198, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_fixed_array_runtime_preserves_numeric_items_across_helper_calls(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Fixed Array Foreach Preserves Numeric Items Across Helper Calls";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "labelFor(value: int) {\n"
            "    if (value % 2 == 0) {\n"
            "        return \"even\";\n"
            "    }\n"
            "    return \"odd\";\n"
            "}\n"
            "var xs: int[3] = [1, 2, 3];\n"
            "var total = 0;\n"
            "for (var item in xs) {\n"
            "    var label = labelFor(item);\n"
            "    if (label == \"even\") {\n"
            "        total = total + 10;\n"
            "    }\n"
            "    total = total + item;\n"
            "}\n"
            "return total;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_fixed_array_helper_call_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(16, result);

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
            "var {Pair} = %import(\"zr.container\");\n"
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
            "var {Pair} = %import(\"zr.container\");\n"
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
            "var {Pair} = %import(\"zr.container\");\n"
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

static void test_container_linked_list_runtime_remove_first_preserves_pair_values_across_typed_function_boundary(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - LinkedList RemoveFirst Preserves Pair Values Across Typed Function Boundary";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Pair} = %import(\"zr.container\");\n"
            "labelThrough(value) {\n"
            "    return value;\n"
            "}\n"
            "numberThrough(value) {\n"
            "    return value;\n"
            "}\n"
            "var queue = new container.LinkedList<Pair<string, int>>();\n"
            "queue.addLast(new container.Pair<string, int>(labelThrough(\"odd_hi\"), numberThrough(3)));\n"
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

    entryFunction = compile_test_script(state, "container_linked_list_remove_first_pair_typed_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_set_to_map_runtime_preserves_bucket_values_in_fresh_state(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Set To Map Composite Preserves Bucket Values";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Pair} = %import(\"zr.container\");\n"
            "var seen = new container.Set<Pair<int, string>>();\n"
            "seen.add(new container.Pair<int, string>(3, \"odd_hi\"));\n"
            "seen.add(new container.Pair<int, string>(1, \"odd_lo\"));\n"
            "seen.add(new container.Pair<int, string>(2, \"even\"));\n"
            "seen.add(new container.Pair<int, string>(4, \"even\"));\n"
            "var buckets = new container.Map<string, Array<int>>();\n"
            "for (var pair in seen) {\n"
            "    var label = <string> pair.second;\n"
            "    var value = <int> pair.first;\n"
            "    var bucket = buckets[label];\n"
            "    if (bucket == null) {\n"
            "        bucket = new container.Array<int>();\n"
            "        buckets[label] = bucket;\n"
            "    }\n"
            "    bucket.add(value);\n"
            "}\n"
            "var oddLo: Array<int> = buckets[\"odd_lo\"];\n"
            "var oddHi: Array<int> = buckets[\"odd_hi\"];\n"
            "var even: Array<int> = buckets[\"even\"];\n"
            "var total = 0;\n"
            "if (oddLo != null) {\n"
            "    for (var item in oddLo) {\n"
            "        total = total + <int> item;\n"
            "    }\n"
            "}\n"
            "if (oddHi != null) {\n"
            "    for (var item in oddHi) {\n"
            "        total = total + <int> item;\n"
            "    }\n"
            "}\n"
            "if (even != null) {\n"
            "    for (var item in even) {\n"
            "        total = total + <int> item;\n"
            "    }\n"
            "}\n"
            "return total + (oddLo == null ? 0 : oddLo.length) * 100 + (oddHi == null ? 0 : oddHi.length) * 10 + (even == null ? 0 : even.length);\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_set_to_map_composite_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(122, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_container_linked_set_map_runtime_preserves_native_call_arguments_in_fresh_state(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Container Runtime - Linked Set Map Composite Preserves Native Call Arguments";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Pair} = %import(\"zr.container\");\n"
            "labelFor(value: int) {\n"
            "    if (value % 2 == 0) {\n"
            "        return \"even\";\n"
            "    }\n"
            "    if (value > 2) {\n"
            "        return \"odd_hi\";\n"
            "    }\n"
            "    return \"odd_lo\";\n"
            "}\n"
            "var fixedValues: int[6] = [3, 1, 3, 2, 4, 2];\n"
            "var queue = new container.LinkedList<Pair<string, int>>();\n"
            "var seen = new container.Set<Pair<int, string>>();\n"
            "var buckets = new container.Map<string, Array<int>>();\n"
            "for (var item in fixedValues) {\n"
            "    var numeric = <int> item;\n"
            "    queue.addLast(new container.Pair<string, int>(labelFor(numeric), numeric));\n"
            "}\n"
            "while (queue.count > 0) {\n"
            "    var head = queue.first;\n"
            "    var task = head.value;\n"
            "    var next = head.next;\n"
            "    if (next != null) {\n"
            "        next.previous = null;\n"
            "        queue.first = next;\n"
            "    } else {\n"
            "        queue.first = null;\n"
            "        queue.last = null;\n"
            "    }\n"
            "    head.next = null;\n"
            "    head.previous = null;\n"
            "    queue.count = queue.count - 1;\n"
            "    seen.add(new container.Pair<int, string>(<int> task.second, <string> task.first));\n"
            "}\n"
            "for (var pair in seen) {\n"
            "    var label = <string> pair.second;\n"
            "    var value = <int> pair.first;\n"
            "    var bucket = buckets[label];\n"
            "    if (bucket == null) {\n"
            "        bucket = new container.Array<int>();\n"
            "        buckets[label] = bucket;\n"
            "    }\n"
            "    bucket.add(value);\n"
            "}\n"
            "var oddLo: Array<int> = buckets[\"odd_lo\"];\n"
            "var oddHi: Array<int> = buckets[\"odd_hi\"];\n"
            "var even: Array<int> = buckets[\"even\"];\n"
            "var oddLoLen = oddLo == null ? 0 : oddLo.length;\n"
            "var oddHiLen = oddHi == null ? 0 : oddHi.length;\n"
            "var evenLen = even == null ? 0 : even.length;\n"
            "var oddLoSum = 0;\n"
            "var oddHiSum = 0;\n"
            "if (oddLo != null) {\n"
            "    for (var item in oddLo) {\n"
            "        oddLoSum = oddLoSum + <int> item;\n"
            "    }\n"
            "}\n"
            "if (oddHi != null) {\n"
            "    for (var item in oddHi) {\n"
            "        oddHiSum = oddHiSum + <int> item;\n"
            "    }\n"
            "}\n"
            "return seen.count * 100 + oddLoLen * 10 + oddHiLen + evenLen + oddLoSum + oddHiSum;\n";

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_test_script(state, "container_linked_set_map_composite_runtime_test.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(417, result);

    ZrCore_Function_Free(state, entryFunction);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_object_member_vs_string_index_fixture_separates_member_and_index_contracts(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Object Fixture - Member And String Index Stay Separate";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/object_member_index_construct_target/member_vs_string_index_split.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_member_vs_string_index_split.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)) >= 1);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)) >= 1);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)) >= 1);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(17, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_object_missing_member_vs_missing_key_fixture_preserves_member_error_boundary(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Object Fixture - Missing Member Stays Distinct From Missing Key";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t manifestSize = 0;
    size_t sourceSize = 0;
    char *manifestText;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    manifestText =
            read_reference_file("core_semantics/object_member_index_construct_target/manifest.json", &manifestSize);
    TEST_ASSERT_NOT_NULL(manifestText);
    TEST_ASSERT_TRUE(manifestSize > 0);
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "object-missing-member-vs-missing-key"));

    source = read_reference_file("core_semantics/object_member_index_construct_target/missing_member_vs_missing_key.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_missing_member_vs_missing_key.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode_recursive(entryFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)) >= 1);
    TEST_ASSERT_TRUE(count_instruction_opcode_recursive(entryFunction, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)) >= 1);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(7, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    free(manifestText);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_object_array_map_plain_index_fixture_keeps_contract_specific_miss_semantics(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Object Fixture - Array Map Plain Object Keep Index Semantics";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/object_member_index_construct_target/array_map_plain_index_split.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_array_map_plain_index_split.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)) >= 6);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)) >= 2);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(111111, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_object_property_fixture_prefers_getter_and_setter_contracts(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Object Fixture - Property Getter Setter Keep Contract Precedence";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/object_member_index_construct_target/property_getter_setter_precedence.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_property_getter_setter_precedence.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)) >= 2);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)) >= 1);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(4142, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_protocols_foreach_fixture_lowers_to_iter_contract_opcodes(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Protocol Fixture - Foreach Lowers To Iterator Contracts";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/protocols_iteration_comparable/foreach_contract_lowering.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_foreach_contract_lowering.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_INIT)) >= 1);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)) >= 1);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_CURRENT)) >= 1);
    TEST_ASSERT_EQUAL_UINT32(0, count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
    TEST_ASSERT_EQUAL_UINT32(0, count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(6, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_protocols_empty_iterator_fixture_keeps_zero_body_execution_stable(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Protocol Fixture - Empty Iterator Stays Stable";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/protocols_iteration_comparable/empty_iterator_boundary.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_empty_iterator_boundary.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_INIT)) >= 1);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)) >= 1);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_CURRENT)) >= 1);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(0, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_protocols_singleton_iterator_fixture_yields_once_per_pass(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Protocol Fixture - Singleton Iterator Yields Once Per Pass";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/protocols_iteration_comparable/singleton_iterator_boundary.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_singleton_iterator_boundary.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_INIT)) >= 2);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)) >= 2);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_CURRENT)) >= 2);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(99, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_protocols_nested_foreach_fixture_keeps_iterators_independent(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Protocol Fixture - Nested Foreach Keeps Iterators Independent";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/protocols_iteration_comparable/nested_foreach_regression.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_nested_foreach_regression.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_INIT)) >= 2);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)) >= 2);
    TEST_ASSERT_TRUE(count_instruction_opcode(entryFunction, ZR_INSTRUCTION_ENUM(ITER_CURRENT)) >= 2);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(198, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
    ZrContainerTests_DestroyState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, summary);
    TEST_DIVIDER();
}

static void test_reference_protocols_comparable_hashable_fixture_preserves_container_consistency(void) {
    SZrTestTimer timer = {0};
    const char *summary = "Reference Protocol Fixture - Comparable And Hashable Stay Consistent";
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;
    size_t sourceSize = 0;
    char *source;

    TEST_START(summary);
    timer.startTime = clock();

    state = ZrContainerTests_CreateState();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/protocols_iteration_comparable/comparable_hashable_consistency.zr",
                                 &sourceSize);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceSize > 0);

    entryFunction = compile_test_script(state, "reference_comparable_hashable_consistency.zr", source);
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(2118, result);

    ZrCore_Function_Free(state, entryFunction);
    free(source);
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
    RUN_TEST(test_container_fixed_array_runtime_reiterates_managed_elements_without_corrupting_iterator_state);
    RUN_TEST(test_container_fixed_array_runtime_supports_nested_independent_managed_iterators);
    RUN_TEST(test_container_fixed_array_runtime_preserves_numeric_items_across_helper_calls);
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
    RUN_TEST(test_container_linked_list_runtime_remove_first_preserves_pair_values_across_typed_function_boundary);
    RUN_TEST(test_container_set_to_map_runtime_preserves_bucket_values_in_fresh_state);
    RUN_TEST(test_container_linked_set_map_runtime_preserves_native_call_arguments_in_fresh_state);
    RUN_TEST(test_reference_object_member_vs_string_index_fixture_separates_member_and_index_contracts);
    RUN_TEST(test_reference_object_missing_member_vs_missing_key_fixture_preserves_member_error_boundary);
    RUN_TEST(test_reference_object_array_map_plain_index_fixture_keeps_contract_specific_miss_semantics);
    RUN_TEST(test_reference_object_property_fixture_prefers_getter_and_setter_contracts);
    RUN_TEST(test_reference_protocols_foreach_fixture_lowers_to_iter_contract_opcodes);
    RUN_TEST(test_reference_protocols_empty_iterator_fixture_keeps_zero_body_execution_stable);
    RUN_TEST(test_reference_protocols_singleton_iterator_fixture_yields_once_per_pass);
    RUN_TEST(test_reference_protocols_nested_foreach_fixture_keeps_iterators_independent);
    RUN_TEST(test_reference_protocols_comparable_hashable_fixture_preserves_container_consistency);
    return UNITY_END();
}
