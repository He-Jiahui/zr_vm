#include <string.h>

#include "unity.h"

#include "backend_aot_function_table.h"
#include "backend_aot_reachability.h"
#include "backend_aot_reachability_function_graph.h"

void setUp(void) {}

void tearDown(void) {}

static TZrInstruction test_create_instruction_2(EZrInstructionCode opcode,
                                                TZrUInt16 operandExtra,
                                                TZrUInt16 operandA,
                                                TZrUInt16 operandB) {
    TZrInstruction instruction;

    instruction.value = 0u;
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static void test_reachability_marks_roots_and_direct_dependencies(void) {
    static const TZrUInt32 roots[] = {0u};
    static const EZrAotReachabilityReason rootReasons[] = {ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY};
    static const SZrAotReachabilityEdge edges[] = {
            {0u, 1u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
            {1u, 3u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
            {0u, 2u, ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS},
            {4u, 5u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
    };
    SZrAotReachabilityMark marks[6];
    TZrUInt32 queue[6];
    TZrUInt32 markedCount = 0u;

    TEST_ASSERT_TRUE(backend_aot_reachability_compute(marks,
                                                      6u,
                                                      roots,
                                                      rootReasons,
                                                      1u,
                                                      edges,
                                                      4u,
                                                      queue,
                                                      6u,
                                                      &markedCount));

    TEST_ASSERT_EQUAL_UINT32(4u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[0].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, marks[1].reason);
    TEST_ASSERT_EQUAL_UINT32(0u, marks[1].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[2].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(0u, marks[2].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[3].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, marks[3].reason);
    TEST_ASSERT_EQUAL_UINT32(1u, marks[3].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[4].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[5].state);
}

static void test_reachability_preserves_root_reason_and_rejects_invalid_graphs(void) {
    static const TZrUInt32 roots[] = {0u, 2u};
    static const EZrAotReachabilityReason rootReasons[] = {
            ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY,
            ZR_AOT_REACHABILITY_REASON_MANIFEST,
    };
    static const SZrAotReachabilityEdge edges[] = {
            {0u, 1u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
            {1u, 2u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
    };
    static const SZrAotReachabilityEdge invalidEdges[] = {
            {0u, 4u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
    };
    SZrAotReachabilityMark marks[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;

    TEST_ASSERT_TRUE(backend_aot_reachability_compute(marks,
                                                      3u,
                                                      roots,
                                                      rootReasons,
                                                      2u,
                                                      edges,
                                                      2u,
                                                      queue,
                                                      3u,
                                                      &markedCount));

    TEST_ASSERT_EQUAL_UINT32(3u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_MANIFEST, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[2].predecessor);
    TEST_ASSERT_FALSE(backend_aot_reachability_compute(marks,
                                                       3u,
                                                       roots,
                                                       rootReasons,
                                                       2u,
                                                       edges,
                                                       2u,
                                                       queue,
                                                       2u,
                                                       &markedCount));
    TEST_ASSERT_FALSE(backend_aot_reachability_compute(marks,
                                                       3u,
                                                       roots,
                                                       rootReasons,
                                                       1u,
                                                       invalidEdges,
                                                       1u,
                                                       queue,
                                                       3u,
                                                       &markedCount));
}

static void test_function_table_filter_keeps_reachable_entries_without_renumbering(void) {
    SZrFunction functions[4];
    SZrAotFunctionEntry entries[4] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
            {&functions[3], 3u},
    };
    SZrAotFunctionTable table = {
            entries,
            4u,
            4u,
            4u,
    };
    SZrAotReachabilityMark marks[4] = {
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_UNMARKED, ZR_AOT_REACHABILITY_REASON_NONE, ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, 0u},
            {ZR_AOT_REACHABILITY_STATE_UNMARKED, ZR_AOT_REACHABILITY_REASON_NONE, ZR_AOT_REACHABILITY_NO_NODE},
    };
    SZrAotFunctionEntry invalidEntries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 9u},
    };
    SZrAotFunctionTable invalidTable = {
            invalidEntries,
            3u,
            3u,
            3u,
    };
    SZrAotReachabilityMark invalidMarks[3] = {
            {ZR_AOT_REACHABILITY_STATE_UNMARKED, ZR_AOT_REACHABILITY_REASON_NONE, ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, 0u},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, 1u},
    };

    memset(functions, 0, sizeof(functions));
    TEST_ASSERT_TRUE(backend_aot_filter_function_table_by_reachability(&table, marks, 4u));
    TEST_ASSERT_EQUAL_UINT32(2u, table.count);
    TEST_ASSERT_EQUAL_PTR(&functions[0], table.entries[0].function);
    TEST_ASSERT_EQUAL_UINT32(0u, table.entries[0].flatIndex);
    TEST_ASSERT_EQUAL_PTR(&functions[2], table.entries[1].function);
    TEST_ASSERT_EQUAL_UINT32(2u, table.entries[1].flatIndex);
    TEST_ASSERT_EQUAL_UINT32(4u, backend_aot_function_table_index_space(&table));
    TEST_ASSERT_FALSE(backend_aot_filter_function_table_by_reachability(&table, marks, 1u));
    TEST_ASSERT_FALSE(backend_aot_filter_function_table_by_reachability(&invalidTable, invalidMarks, 3u));
    TEST_ASSERT_EQUAL_UINT32(3u, invalidTable.count);
    TEST_ASSERT_EQUAL_PTR(&functions[0], invalidTable.entries[0].function);
    TEST_ASSERT_EQUAL_UINT32(0u, invalidTable.entries[0].flatIndex);
}

static void test_static_callable_reachability_marks_get_sub_function_target(void) {
    TZrInstruction rootInstructions[1];
    SZrFunction functions[3];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    SZrAotReachabilityMark marks[3];
    SZrAotReachabilityEdge edges[3];
    TZrUInt32 roots[3];
    EZrAotReachabilityReason rootReasons[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;

    memset(functions, 0, sizeof(functions));
    rootInstructions[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 0u, 0u);
    functions[0].instructionsList = rootInstructions;
    functions[0].instructionsLength = 1u;
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 1u;

    TEST_ASSERT_TRUE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                      &table,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      roots,
                                                                      rootReasons,
                                                                      3u,
                                                                      marks,
                                                                      3u,
                                                                      edges,
                                                                      3u,
                                                                      queue,
                                                                      3u,
                                                                      &markedCount,
                                                                      &edgeCount));

    TEST_ASSERT_EQUAL_UINT32(1u, edgeCount);
    TEST_ASSERT_EQUAL_UINT32(0u, edges[0].source);
    TEST_ASSERT_EQUAL_UINT32(1u, edges[0].target);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, edges[0].reason);
    TEST_ASSERT_EQUAL_UINT32(2u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, marks[1].reason);
    TEST_ASSERT_EQUAL_UINT32(0u, marks[1].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[2].state);
    TEST_ASSERT_FALSE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                       &table,
                                                                       ZR_NULL,
                                                                       0u,
                                                                       roots,
                                                                       rootReasons,
                                                                       3u,
                                                                       marks,
                                                                       3u,
                                                                       edges,
                                                                       0u,
                                                                       queue,
                                                                       3u,
                                                                       &markedCount,
                                                                       &edgeCount));
}

static void test_static_callable_reachability_keeps_exported_child_roots(void) {
    SZrFunction functions[3];
    SZrFunctionTopLevelCallableBinding exportedCallable;
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    SZrAotReachabilityMark marks[3];
    SZrAotReachabilityEdge edges[1];
    TZrUInt32 roots[3];
    EZrAotReachabilityReason rootReasons[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;

    memset(functions, 0, sizeof(functions));
    memset(&exportedCallable, 0, sizeof(exportedCallable));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    functions[1].lineInSourceStart = 10u;
    functions[1].lineInSourceEnd = 10u;
    functions[2].lineInSourceStart = 20u;
    functions[2].lineInSourceEnd = 20u;
    exportedCallable.callableChildIndex = 1u;
    exportedCallable.exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    functions[0].topLevelCallableBindings = &exportedCallable;
    functions[0].topLevelCallableBindingLength = 1u;

    TEST_ASSERT_TRUE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                      &table,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      roots,
                                                                      rootReasons,
                                                                      3u,
                                                                      marks,
                                                                      3u,
                                                                      edges,
                                                                      1u,
                                                                      queue,
                                                                      3u,
                                                                      &markedCount,
                                                                      &edgeCount));

    TEST_ASSERT_EQUAL_UINT32(0u, edgeCount);
    TEST_ASSERT_EQUAL_UINT32(2u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[2].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[2].predecessor);
}

static void test_static_callable_reachability_keeps_manifest_function_roots(void) {
    SZrFunction functions[3];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    static const TZrUInt32 manifestRoots[] = {2u};
    static const TZrUInt32 invalidManifestRoots[] = {7u};
    SZrAotReachabilityMark marks[3];
    SZrAotReachabilityEdge edges[1];
    TZrUInt32 roots[3];
    EZrAotReachabilityReason rootReasons[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;

    memset(functions, 0, sizeof(functions));

    TEST_ASSERT_TRUE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                      &table,
                                                                      manifestRoots,
                                                                      1u,
                                                                      roots,
                                                                      rootReasons,
                                                                      3u,
                                                                      marks,
                                                                      3u,
                                                                      edges,
                                                                      1u,
                                                                      queue,
                                                                      3u,
                                                                      &markedCount,
                                                                      &edgeCount));

    TEST_ASSERT_EQUAL_UINT32(0u, edgeCount);
    TEST_ASSERT_EQUAL_UINT32(2u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[2].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_MANIFEST, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[2].predecessor);
    TEST_ASSERT_FALSE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                       &table,
                                                                       invalidManifestRoots,
                                                                       1u,
                                                                       roots,
                                                                       rootReasons,
                                                                       3u,
                                                                       marks,
                                                                       3u,
                                                                       edges,
                                                                       1u,
                                                                       queue,
                                                                       3u,
                                                                       &markedCount,
                                                                       &edgeCount));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reachability_marks_roots_and_direct_dependencies);
    RUN_TEST(test_reachability_preserves_root_reason_and_rejects_invalid_graphs);
    RUN_TEST(test_function_table_filter_keeps_reachable_entries_without_renumbering);
    RUN_TEST(test_static_callable_reachability_marks_get_sub_function_target);
    RUN_TEST(test_static_callable_reachability_keeps_exported_child_roots);
    RUN_TEST(test_static_callable_reachability_keeps_manifest_function_roots);
    return UNITY_END();
}
