#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_builder.h"

void setUp(void) {}
void tearDown(void) {}

void test_state_map_find_supports_each_logical_phase(void);
void test_state_map_clone_copies_pools_and_lifecycle(void);
void test_state_map_materialization_successfully_copies_logical_values(void);
void test_state_map_materialization_is_transactional_on_failure(void);
void test_state_map_rejects_borrowed_value_across_suspend(void);
void test_state_map_builder_records_boundaries_and_is_liveness_conservative(void);
void test_state_map_builder_preserves_existing_map_on_verification_failure(void);
void test_state_map_materialization_accepts_managed_owner_roots(void);
void test_state_map_materialization_rejects_malformed_function_without_deref(void);
void test_state_map_function_clone_keeps_side_table_independent(void);

static void map_with_one_entry(SZrExecIrStateMap *map,
                               EZrExecIrStateMapPhase phase,
                               TZrUInt32 flags) {
    ZrCore_ExecIr_StateMapInit(map);
    map->entries = (SZrExecIrStateMapEntry *)calloc(1u, sizeof(*map->entries));
    map->valuePool = (TZrExecIrValueId *)calloc(1u, sizeof(*map->valuePool));
    map->rootPool = (TZrExecIrValueId *)calloc(1u, sizeof(*map->rootPool));
    map->ownerStatePool = (TZrUInt32 *)calloc(1u, sizeof(*map->ownerStatePool));
    TEST_ASSERT_NOT_NULL(map->entries);
    TEST_ASSERT_NOT_NULL(map->valuePool);
    TEST_ASSERT_NOT_NULL(map->rootPool);
    TEST_ASSERT_NOT_NULL(map->ownerStatePool);
    map->entryCount = map->entryCapacity = 1u;
    map->valueCount = map->valueCapacity = 1u;
    map->rootCount = map->rootCapacity = 1u;
    map->ownerStateCount = map->ownerStateCapacity = 1u;
    map->valuePool[0] = 1u;
    map->rootPool[0] = 1u;
    map->ownerStatePool[0] = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    map->generation = 7u;
    map->entries[0].sourceId = 42u;
    map->entries[0].instructionId = 1u;
    map->entries[0].resumeId = 7u;
    map->entries[0].boundaryFlags = flags;
    map->entries[0].phase = phase;
    map->entries[0].liveValues.offset = 0u;
    map->entries[0].liveValues.count = 1u;
    map->entries[0].rootValues.offset = 0u;
    map->entries[0].rootValues.count = 1u;
    map->entries[0].ownerStates.offset = 0u;
    map->entries[0].ownerStates.count = 1u;
    map->entries[0].cleanupState = phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT
                                        ? 0u
                                        : (phase == ZR_EXEC_IR_STATE_AFTER_EFFECT ? 1u : 2u);
}

static void function_with_one_gc_value(SZrExecIrFunction *function) {
    SZrExecIrInstruction instruction;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 99u;
    function->signatureHash = 123u;
    function->contract.generation = 7u;
    TEST_ASSERT_EQUAL(1u, ZrCore_ExecIr_FunctionAddValue(
                                  function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
                                  ZR_EXEC_IR_NULLABILITY_NULLABLE));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction.sourceId = 42u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL));
}

static void function_with_alloc_debug_boundary(SZrExecIrFunction *function) {
    SZrExecIrBlock *block;
    SZrExecIrInstruction instruction;
    TZrExecIrValueId value;
    TZrExecIrMemoryTokenId memoryToken;
    SZrExecIrRange resultRange;
    SZrExecIrRange operandRange;

    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 77u;
    function->signatureHash = 0x1234u;
    function->contract.generation = 11u;
    value = ZrCore_ExecIr_FunctionAddValue(function, 1u,
                                            ZR_EXEC_IR_OWNERSHIP_GC,
                                            ZR_EXEC_IR_NULLABILITY_NULLABLE);
    TEST_ASSERT_EQUAL(1u, value);
    /* This borrowed parameter is dead at every boundary and must not be
     * mistaken for a cross-suspend live alias. */
    TEST_ASSERT_EQUAL(2u, ZrCore_ExecIr_FunctionAddValue(
                                  function, 2u, ZR_EXEC_IR_OWNERSHIP_BORROWED,
                                  ZR_EXEC_IR_NULLABILITY_NULLABLE));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_BLOCK_ID_ENTRY,
                      ZrCore_ExecIr_FunctionAddBlock(
                              function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY));

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_ALLOC;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_GC;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 10u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(
            function, &value, 1u, &resultRange));
    instruction.resultRange = resultRange;
    memoryToken = 1u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            function, &memoryToken, 1u, &instruction.memoryOut));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            function, &instruction, NULL));

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction.flags = ZR_EXEC_IR_FLAG_DEBUG_POLL;
    instruction.sourceId = 20u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            function, &instruction, NULL));

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.sourceId = 30u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
            function, &value, 1u, &operandRange));
    instruction.operandRange = operandRange;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            function, &instruction, NULL));

    block = &function->blocks[0];
    block->instructionRange.start = 0u;
    block->instructionRange.count = function->instructionCount;
    block->terminatorInstructionId = function->instructionCount;
}

void test_state_map_find_supports_each_logical_phase(void) {
    SZrExecIrStateMap map;
    ZrCore_ExecIr_StateMapInit(&map);
    map.entries = (SZrExecIrStateMapEntry *)calloc(3u, sizeof(*map.entries));
    TEST_ASSERT_NOT_NULL(map.entries);
    map.entryCount = map.entryCapacity = 3u;
    for (TZrUInt32 i = 0u; i < 3u; ++i) {
        map.entries[i].sourceId = 10u;
        map.entries[i].resumeId = 20u;
        map.entries[i].phase = (EZrExecIrStateMapPhase)i;
    }
    for (TZrUInt32 i = 0u; i < 3u; ++i) {
        const SZrExecIrStateMapEntry *entry =
            ZrCore_ExecIr_StateMapFind(&map, 10u, 20u,
                                       (EZrExecIrStateMapPhase)i);
        TEST_ASSERT_NOT_NULL(entry);
        TEST_ASSERT_EQUAL((int)i, (int)entry->phase);
    }
    TEST_ASSERT_NULL(ZrCore_ExecIr_StateMapFind(
        &map, 10u, 20u, ZR_EXEC_IR_STATE_PHASE_COUNT));
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_clone_copies_pools_and_lifecycle(void) {
    SZrExecIrStateMap source;
    SZrExecIrStateMap clone;
    map_with_one_entry(&source, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    ZrCore_ExecIr_StateMapInit(&clone);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_StateMapClone(&source, &clone));
    TEST_ASSERT_EQUAL(source.generation, clone.generation);
    TEST_ASSERT_EQUAL(source.entries[0].sourceId, clone.entries[0].sourceId);
    TEST_ASSERT_EQUAL(source.valuePool[0], clone.valuePool[0]);
    TEST_ASSERT_NOT_EQUAL(source.entries, clone.entries);
    TEST_ASSERT_NOT_EQUAL(source.valuePool, clone.valuePool);
    ZrCore_ExecIr_StateMapFree(&source);
    ZrCore_ExecIr_StateMapFree(&clone);
}

void test_state_map_materialization_successfully_copies_logical_values(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    function_with_one_gc_value(&function);
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 42u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(42u, target.sourceId);
    TEST_ASSERT_EQUAL(1u, target.valueCount);
    TEST_ASSERT_EQUAL(1u, target.rootCount);
    TEST_ASSERT_EQUAL(1u, target.values[0]);
    TEST_ASSERT_EQUAL(1u, target.roots[0]);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_is_transactional_on_failure(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId *oldValues;
    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT, 0u);
    function_with_one_gc_value(&function);
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    oldValues = (TZrExecIrValueId *)calloc(1u, sizeof(*oldValues));
    TEST_ASSERT_NOT_NULL(oldValues);
    oldValues[0] = 99u;
    target.values = oldValues;
    target.valueCount = target.valueCapacity = 1u;
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation + 1u;
    request.sourceId = 42u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL_PTR(oldValues, target.values);
    TEST_ASSERT_EQUAL(1u, target.valueCount);
    TEST_ASSERT_EQUAL(99u, target.values[0]);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_rejects_borrowed_value_across_suspend(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    map_with_one_entry(&map, ZR_EXEC_IR_STATE_BEFORE_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND);
    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
    TEST_ASSERT_EQUAL(1u, ZrCore_ExecIr_FunctionAddValue(
                                  &function, 1u, ZR_EXEC_IR_OWNERSHIP_BORROWED,
                                  ZR_EXEC_IR_NULLABILITY_NULLABLE));
    {
        SZrExecIrInstruction instruction;
        memset(&instruction, 0, sizeof(instruction));
        instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
        instruction.sourceId = 42u;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
                &function, &instruction, NULL));
    }
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 42u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_BEFORE_EFFECT;
    request.target = &target;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_builder_records_boundaries_and_is_liveness_conservative(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrStateMapEntry *allocBefore;
    const SZrExecIrStateMapEntry *allocAfter;
    const SZrExecIrStateMapEntry *debugBefore;
    const SZrExecIrStateMapEntry *debugAfter;
    TZrUInt32 index;
    TZrBool foundDeadBorrow = ZR_FALSE;

    function_with_alloc_debug_boundary(&function);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_NOT_NULL(function.stateMap);
    TEST_ASSERT_EQUAL(77u, function.stateMap->functionToken);
    TEST_ASSERT_EQUAL(0x1234u, function.stateMap->signatureHash);
    TEST_ASSERT_EQUAL(11u, function.stateMap->generation);
    allocBefore = ZrCore_ExecIr_StateMapFind(function.stateMap, 10u, 1u,
                                              ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    allocAfter = ZrCore_ExecIr_StateMapFind(function.stateMap, 10u, 1u,
                                             ZR_EXEC_IR_STATE_AFTER_EFFECT);
    debugBefore = ZrCore_ExecIr_StateMapFind(function.stateMap, 20u, 2u,
                                              ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    debugAfter = ZrCore_ExecIr_StateMapFind(function.stateMap, 20u, 2u,
                                             ZR_EXEC_IR_STATE_AFTER_EFFECT);
    TEST_ASSERT_NOT_NULL(allocBefore);
    TEST_ASSERT_NOT_NULL(allocAfter);
    TEST_ASSERT_NOT_NULL(debugBefore);
    TEST_ASSERT_NOT_NULL(debugAfter);
    TEST_ASSERT_EQUAL(allocBefore->resumeId, allocAfter->resumeId);
    TEST_ASSERT_EQUAL(1u, allocAfter->liveValues.count);
    TEST_ASSERT_EQUAL(1u, allocAfter->rootValues.count);
    TEST_ASSERT_EQUAL(1u, function.stateMap->valuePool[allocAfter->liveValues.start]);
    for (index = 0u; index < function.stateMap->valueCount; ++index) {
        if (function.stateMap->valuePool[index] == 2u) foundDeadBorrow = ZR_TRUE;
    }
    TEST_ASSERT_FALSE(foundDeadBorrow);
    ZrCore_ExecIr_FreeFunction(&function);
}

void test_state_map_builder_preserves_existing_map_on_verification_failure(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrStateMap *oldMap;
    TZrUInt32 oldEntryCount;

    function_with_alloc_debug_boundary(&function);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    oldMap = function.stateMap;
    oldEntryCount = oldMap->entryCount;
    function.blocks[0].instructionRange.start = UINT32_MAX;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, diagnostic.code);
    TEST_ASSERT_EQUAL_PTR(oldMap, function.stateMap);
    TEST_ASSERT_EQUAL(oldEntryCount, function.stateMap->entryCount);
    ZrCore_ExecIr_FreeFunction(&function);
}

void test_state_map_materialization_accepts_managed_owner_roots(void) {
    EZrExecIrOwnership ownerships[] = {
        ZR_EXEC_IR_OWNERSHIP_UNIQUE,
        ZR_EXEC_IR_OWNERSHIP_SHARED
    };
    TZrUInt32 index;

    for (index = 0u; index < (TZrUInt32)(sizeof(ownerships) / sizeof(ownerships[0]));
         ++index) {
        SZrExecIrStateMap map;
        SZrExecIrFunction function;
        SZrExecIrMaterializedState target;
        SZrExecIrResumeRequest request;
        SZrExecIrDiagnostic diagnostic;

        map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                           ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
        function_with_one_gc_value(&function);
        function.values[0].ownership = ownerships[index];
        map.functionToken = function.functionToken;
        map.signatureHash = function.signatureHash;
        ZrCore_ExecIr_MaterializedStateInit(&target);
        memset(&request, 0, sizeof(request));
        request.function = &function;
        request.map = &map;
        request.generation = map.generation;
        request.sourceId = 42u;
        request.resumeId = 7u;
        request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
        request.target = &target;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
        TEST_ASSERT_EQUAL(1u, target.rootCount);
        ZrCore_ExecIr_MaterializedStateFree(&target);
        ZrCore_ExecIr_FreeFunction(&function);
        ZrCore_ExecIr_StateMapFree(&map);
    }
}

void test_state_map_materialization_rejects_malformed_function_without_deref(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId *oldValues;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    function_with_one_gc_value(&function);
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    free(function.values);
    function.values = ZR_NULL;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    oldValues = (TZrExecIrValueId *)calloc(1u, sizeof(*oldValues));
    TEST_ASSERT_NOT_NULL(oldValues);
    oldValues[0] = 123u;
    target.values = oldValues;
    target.valueCount = target.valueCapacity = 1u;
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 42u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_PTR(oldValues, target.values);
    TEST_ASSERT_EQUAL(123u, target.values[0]);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_function_clone_keeps_side_table_independent(void) {
    SZrExecIrFunction source;
    SZrExecIrFunction clone;
    SZrExecIrDiagnostic diagnostic;

    function_with_alloc_debug_boundary(&source);
    ZrCore_ExecIr_FunctionInit(&clone);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&source, &diagnostic));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneFunction(&source, &clone, &diagnostic));
    TEST_ASSERT_NOT_NULL(clone.stateMap);
    TEST_ASSERT_NOT_EQUAL(source.stateMap, clone.stateMap);
    TEST_ASSERT_EQUAL(source.stateMap->entryCount, clone.stateMap->entryCount);
    TEST_ASSERT_NOT_EQUAL(source.stateMap->entries, clone.stateMap->entries);
    ZrCore_ExecIr_FreeFunction(&source);
    TEST_ASSERT_NOT_NULL(clone.stateMap);
    ZrCore_ExecIr_FreeFunction(&clone);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_state_map_find_supports_each_logical_phase);
    RUN_TEST(test_state_map_clone_copies_pools_and_lifecycle);
    RUN_TEST(test_state_map_materialization_successfully_copies_logical_values);
    RUN_TEST(test_state_map_materialization_is_transactional_on_failure);
    RUN_TEST(test_state_map_rejects_borrowed_value_across_suspend);
    RUN_TEST(test_state_map_builder_records_boundaries_and_is_liveness_conservative);
    RUN_TEST(test_state_map_builder_preserves_existing_map_on_verification_failure);
    RUN_TEST(test_state_map_materialization_accepts_managed_owner_roots);
    RUN_TEST(test_state_map_materialization_rejects_malformed_function_without_deref);
    RUN_TEST(test_state_map_function_clone_keeps_side_table_independent);
    return UNITY_END();
}
