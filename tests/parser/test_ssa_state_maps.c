#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_builder.h"

#include "ssa_state_map_clone_fault_allocator.h"

void setUp(void) {}
void tearDown(void) {}

void test_state_map_find_supports_each_logical_phase(void);
void test_state_map_materialization_rejects_unknown_phase(void);
void test_state_map_materialization_rejects_inconsistent_exception_state(void);
void test_state_map_materialization_rejects_boundary_mismatch(void);
void test_state_map_materialization_rejects_forged_fallback_source(void);
void test_state_map_materialization_rejects_split_resume_identity(void);
void test_state_map_materialization_rejects_reused_resume_identity(void);
void test_state_map_materialization_rejects_unknown_value_enums(void);
void test_state_map_materialization_rejects_unreachable_handler(void);
void test_state_map_materialization_rejects_noncanonical_handler(void);
void test_state_map_materialization_requires_throw_boundary_for_handler(void);
void test_state_map_materialization_rejects_forged_owner_state(void);
void test_state_map_materialization_rejects_mismatched_deopt_identity(void);
void test_state_map_materialization_rejects_deopt_value_outside_live_state(void);
void test_state_map_materialization_rejects_missing_managed_root(void);
void test_state_map_materialization_rejects_malformed_owner_operand(void);
void test_state_map_materialization_rejects_duplicate_deopt_identity(void);
void test_state_map_clone_copies_pools_and_lifecycle(void);
void test_state_map_clone_rejects_aliased_destination(void);
void test_state_map_clone_rejects_interior_pool_alias(void);
void test_state_map_clone_rejects_overlapping_source_pools(void);
void test_state_map_clone_preserves_both_maps_on_each_pool_allocation_failure(void);
void test_state_map_clone_replaces_destination_after_last_pool_success(void);
void test_state_map_clone_skips_empty_pools_without_allocating(void);
void test_state_map_self_clone_does_not_allocate_or_change_pools(void);
void test_state_map_materialization_successfully_copies_logical_values(void);
void test_state_map_materialization_rejects_overlapping_map_pools(void);
void test_state_map_materialization_rejects_interior_target_alias(void);
void test_state_map_materialization_rejects_overlapping_target_arrays(void);
void test_state_map_materialization_is_transactional_on_failure(void);
void test_state_map_rejects_borrowed_value_across_suspend(void);
void test_state_map_builder_records_boundaries_and_is_liveness_conservative(void);
void test_state_map_builder_reuses_deopt_resume_identity(void);
void test_state_map_builder_rejects_duplicate_deopt_identity(void);
void test_state_map_builder_rejects_unknown_value_enums(void);
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
    TEST_ASSERT_EQUAL(1u, ZrCore_ExecIr_FunctionAddExternalValue(
                                  function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
                                  ZR_EXEC_IR_NULLABILITY_NULLABLE));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_GC;
    instruction.sourceId = 42u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL));
}

static void function_with_two_gc_boundaries(SZrExecIrFunction *function) {
    SZrExecIrInstruction instruction;

    function_with_one_gc_value(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_GC;
    instruction.sourceId = 84u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL));
}

static void function_with_drop_boundary(SZrExecIrFunction *function) {
    SZrExecIrInstruction instruction;
    SZrExecIrRange operandRange;
    TZrExecIrValueId value = 1u;

    function_with_one_gc_value(function);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
            function, &value, 1u, &operandRange));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_DROP;
    instruction.sourceId = 84u;
    instruction.operandRange = operandRange;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            function, &instruction, NULL));
}

static void function_with_deopt_boundary(SZrExecIrFunction *function) {
    function_with_one_gc_value(function);
    function->instructions[0].deoptId = 91u;
    function->deoptStates = (SZrExecIrDeoptState *)calloc(
            1u, sizeof(*function->deoptStates));
    function->deoptValues = (TZrExecIrValueId *)calloc(
            1u, sizeof(*function->deoptValues));
    TEST_ASSERT_NOT_NULL(function->deoptStates);
    TEST_ASSERT_NOT_NULL(function->deoptValues);
    function->deoptStateCount = function->deoptStateCapacity = 1u;
    function->deoptValueCount = function->deoptValueCapacity = 1u;
    function->deoptValues[0] = 1u;
    function->deoptStates[0].id = 91u;
    function->deoptStates[0].sourceId = 42u;
    function->deoptStates[0].resumeId = 701u;
    function->deoptStates[0].valueRange.start = 0u;
    function->deoptStates[0].valueRange.count = 1u;
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
    TEST_ASSERT_NULL(ZrCore_ExecIr_StateMapFind(
        &map, 10u, 20u, (EZrExecIrStateMapPhase)-1));
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_unknown_phase(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, (EZrExecIrStateMapPhase)-1,
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
    request.phase = (EZrExecIrStateMapPhase)-1;
    request.target = &target;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_EQUAL(0u, target.valueCount);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_inconsistent_exception_state(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    map.entries[0].exceptionState = ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
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
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL(0u, target.valueCount);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_boundary_mismatch(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    function_with_one_gc_value(&function);
    function.instructions[0].flags = ZR_EXEC_IR_FLAG_MAY_SUSPEND;
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
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_forged_fallback_source(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    function_with_one_gc_value(&function);
    function.instructions[0].sourceId = 0u;
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

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL(1u, diagnostic.expectedVersion);
    TEST_ASSERT_EQUAL(42u, diagnostic.actualVersion);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_split_resume_identity(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    map.entries = (SZrExecIrStateMapEntry *)realloc(
        map.entries, 2u * sizeof(*map.entries));
    TEST_ASSERT_NOT_NULL(map.entries);
    map.entryCapacity = map.entryCount = 2u;
    map.entries[1] = map.entries[0];
    map.entries[1].phase = ZR_EXEC_IR_STATE_BEFORE_EFFECT;
    map.entries[1].resumeId = 8u;
    map.entries[1].cleanupState = 0u;
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

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_reused_resume_identity(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    map.entries = (SZrExecIrStateMapEntry *)realloc(
        map.entries, 2u * sizeof(*map.entries));
    TEST_ASSERT_NOT_NULL(map.entries);
    map.entryCapacity = map.entryCount = 2u;
    map.entries[1] = map.entries[0];
    map.entries[1].instructionId = 2u;
    map.entries[1].sourceId = 84u;
    function_with_two_gc_boundaries(&function);
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

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_unknown_value_enums(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    map.entries[0].rootValues.count = 0u;
    map.rootCount = 0u;
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

    function.values[0].ownership = (EZrExecIrOwnership)-1;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);

    function.values[0].ownership = ZR_EXEC_IR_OWNERSHIP_UNKNOWN;
    function.values[0].nullability = (EZrExecIrNullability)-1;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_unreachable_handler(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW);
    map.entries[0].exceptionState = ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
    map.entries[0].handlerBlockId = 2u;
    function_with_one_gc_value(&function);
    function.instructions[0].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    TEST_ASSERT_EQUAL(1u, ZrCore_ExecIr_FunctionAddBlock(
                                  &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY));
    TEST_ASSERT_EQUAL(2u, ZrCore_ExecIr_FunctionAddBlock(
                                  &function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION));
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = 1u;
    function.blocks[1].instructionRange.start = 1u;
    function.blocks[1].instructionRange.count = 0u;
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
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_noncanonical_handler(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId handlers[2];

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW);
    map.entries[0].exceptionState = ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
    function_with_one_gc_value(&function);
    function.instructions[0].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    TEST_ASSERT_EQUAL(1u, ZrCore_ExecIr_FunctionAddBlock(
                                  &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY));
    handlers[0] = ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION);
    handlers[1] = ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_CLEANUP);
    TEST_ASSERT_EQUAL(2u, handlers[0]);
    TEST_ASSERT_EQUAL(3u, handlers[1]);
    map.entries[0].handlerBlockId = handlers[1];
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = 1u;
    function.blocks[1].instructionRange.start = 1u;
    function.blocks[1].instructionRange.count = 0u;
    function.blocks[2].instructionRange.start = 1u;
    function.blocks[2].instructionRange.count = 0u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, handlers, 2u, &function.blocks[0].successorRange));
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

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_requires_throw_boundary_for_handler(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId handler = 2u;
    TZrExecIrValueId *committedValues;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW);
    map.entries[0].exceptionState = ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW;
    map.entries[0].handlerBlockId = handler;
    function_with_one_gc_value(&function);
    function.instructions[0].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    TEST_ASSERT_EQUAL(1u, ZrCore_ExecIr_FunctionAddBlock(
                                  &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY));
    TEST_ASSERT_EQUAL(handler, ZrCore_ExecIr_FunctionAddBlock(
                                   &function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION));
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = 1u;
    function.blocks[1].instructionRange.start = 1u;
    function.blocks[1].instructionRange.count = 0u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, &handler, 1u, &function.blocks[0].successorRange));
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
    TEST_ASSERT_EQUAL(handler, target.handlerBlockId);
    committedValues = target.values;

    map.entries[0].boundaryFlags = ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC;
    map.entries[0].exceptionState = 0u;
    function.instructions[0].flags = ZR_EXEC_IR_FLAG_MAY_GC;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_EQUAL_PTR(committedValues, target.values);
    TEST_ASSERT_EQUAL(handler, target.handlerBlockId);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_forged_owner_state(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_BEFORE_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP);
    map.entries[0].sourceId = 84u;
    map.entries[0].instructionId = 2u;
    map.ownerStatePool[0] = ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED;
    function_with_drop_boundary(&function);
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 84u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_BEFORE_EFFECT;
    request.target = &target;

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_mismatched_deopt_identity(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC |
                               ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT |
                               ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT);
    map.entries[0].deoptId = 91u;
    function_with_deopt_boundary(&function);
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

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_deopt_value_outside_live_state(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC |
                               ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT |
                               ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT);
    map.entries[0].deoptId = 91u;
    map.entries[0].resumeId = 701u;
    map.entries[0].liveValues.count = 0u;
    map.entries[0].rootValues.count = 0u;
    map.entries[0].ownerStates.count = 0u;
    map.rootCount = 0u;
    map.ownerStateCount = 0u;
    function_with_deopt_boundary(&function);
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 42u;
    request.resumeId = 701u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_missing_managed_root(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    map.entries[0].rootValues.count = 0u;
    map.rootCount = 0u;
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

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_malformed_owner_operand(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP);
    map.entries[0].sourceId = 84u;
    map.entries[0].instructionId = 2u;
    map.entries[0].ownerStates.offset = 0u;
    map.ownerStatePool[0] = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    function_with_drop_boundary(&function);
    function.operandPool[0] = 2u;
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 84u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_duplicate_deopt_identity(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC |
                               ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT |
                               ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT);
    map.entries[0].deoptId = 91u;
    map.entries[0].resumeId = 701u;
    function_with_deopt_boundary(&function);
    function.deoptStates = (SZrExecIrDeoptState *)realloc(
            function.deoptStates, 2u * sizeof(*function.deoptStates));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    function.deoptStateCapacity = function.deoptStateCount = 2u;
    function.deoptStates[1] = function.deoptStates[0];
    function.deoptStates[1].resumeId = 702u;
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 42u;
    request.resumeId = 701u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_builder_reuses_deopt_resume_identity(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrStateMapEntry *before;
    const SZrExecIrStateMapEntry *after;

    function_with_deopt_boundary(&function);

    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    before = ZrCore_ExecIr_StateMapFind(function.stateMap, 42u, 701u,
                                         ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    after = ZrCore_ExecIr_StateMapFind(function.stateMap, 42u, 701u,
                                        ZR_EXEC_IR_STATE_AFTER_EFFECT);
    TEST_ASSERT_NOT_NULL(before);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL(701u, before->resumeId);
    TEST_ASSERT_EQUAL(701u, after->resumeId);
    ZrCore_ExecIr_FreeFunction(&function);
}

void test_state_map_builder_rejects_duplicate_deopt_identity(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;

    function_with_deopt_boundary(&function);
    function.deoptStates = (SZrExecIrDeoptState *)realloc(
            function.deoptStates, 2u * sizeof(*function.deoptStates));
    TEST_ASSERT_NOT_NULL(function.deoptStates);
    function.deoptStateCapacity = function.deoptStateCount = 2u;
    function.deoptStates[1] = function.deoptStates[0];
    function.deoptStates[1].resumeId = 702u;

    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(function.stateMap);
    ZrCore_ExecIr_FreeFunction(&function);
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

#include "test_ssa_state_map_clone_fault_cases.h"

void test_state_map_clone_rejects_aliased_destination(void) {
    SZrExecIrStateMap source;
    SZrExecIrStateMap destination;

    map_with_one_entry(&source, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    destination = source;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_StateMapClone(&source, &destination));
    memset(&destination, 0, sizeof(destination));
    TEST_ASSERT_NOT_NULL(source.entries);
    TEST_ASSERT_EQUAL(42u, source.entries[0].sourceId);
    ZrCore_ExecIr_StateMapFree(&source);
}

void test_state_map_clone_rejects_interior_pool_alias(void) {
    SZrExecIrStateMap source;
    SZrExecIrStateMap destination;
    TZrExecIrValueId *expandedPool;

    map_with_one_entry(&source, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    expandedPool = (TZrExecIrValueId *)realloc(
            source.valuePool, 2u * sizeof(*source.valuePool));
    TEST_ASSERT_NOT_NULL(expandedPool);
    source.valuePool = expandedPool;
    source.valueCapacity = 2u;
    ZrCore_ExecIr_StateMapInit(&destination);
    destination.valuePool = &source.valuePool[1];
    destination.valueCapacity = 1u;

    TEST_ASSERT_FALSE(ZrCore_ExecIr_StateMapClone(&source, &destination));
    destination.valuePool = ZR_NULL;
    destination.valueCapacity = 0u;
    TEST_ASSERT_EQUAL(1u, source.valuePool[0]);
    ZrCore_ExecIr_StateMapFree(&source);
}

void test_state_map_clone_rejects_overlapping_source_pools(void) {
    SZrExecIrStateMap source;
    SZrExecIrStateMap destination;
    TZrExecIrValueId *expandedPool;

    map_with_one_entry(&source, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    expandedPool = (TZrExecIrValueId *)realloc(
            source.valuePool, 2u * sizeof(*source.valuePool));
    TEST_ASSERT_NOT_NULL(expandedPool);
    free(source.rootPool);
    source.valuePool = expandedPool;
    source.valueCapacity = 2u;
    source.valuePool[1] = 1u;
    source.rootPool = &source.valuePool[1];
    source.rootCapacity = 1u;
    ZrCore_ExecIr_StateMapInit(&destination);

    TEST_ASSERT_FALSE(ZrCore_ExecIr_StateMapClone(&source, &destination));
    TEST_ASSERT_NULL(destination.entries);
    source.rootPool = ZR_NULL;
    source.rootCount = source.rootCapacity = 0u;
    ZrCore_ExecIr_StateMapFree(&source);
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

void test_state_map_materialization_rejects_overlapping_map_pools(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId *expandedPool;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    expandedPool = (TZrExecIrValueId *)realloc(
            map.valuePool, 2u * sizeof(*map.valuePool));
    TEST_ASSERT_NOT_NULL(expandedPool);
    free(map.rootPool);
    map.valuePool = expandedPool;
    map.valueCapacity = 2u;
    map.valuePool[1] = 1u;
    map.rootPool = &map.valuePool[1];
    map.rootCapacity = 1u;
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

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, diagnostic.code);
    TEST_ASSERT_NULL(target.values);
    map.rootPool = ZR_NULL;
    map.rootCount = map.rootCapacity = 0u;
    ZrCore_ExecIr_MaterializedStateFree(&target);
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_interior_target_alias(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId *expandedPool;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    expandedPool = (TZrExecIrValueId *)realloc(
            map.valuePool, 2u * sizeof(*map.valuePool));
    TEST_ASSERT_NOT_NULL(expandedPool);
    map.valuePool = expandedPool;
    map.valueCapacity = 2u;
    function_with_one_gc_value(&function);
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    target.values = &map.valuePool[1];
    target.valueCapacity = 1u;
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 42u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED,
                      diagnostic.code);
    target.values = ZR_NULL;
    target.valueCapacity = 0u;
    ZrCore_ExecIr_FreeFunction(&function);
    ZrCore_ExecIr_StateMapFree(&map);
}

void test_state_map_materialization_rejects_overlapping_target_arrays(void) {
    SZrExecIrStateMap map;
    SZrExecIrFunction function;
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId *sharedStorage;

    map_with_one_entry(&map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    function_with_one_gc_value(&function);
    map.functionToken = function.functionToken;
    map.signatureHash = function.signatureHash;
    ZrCore_ExecIr_MaterializedStateInit(&target);
    sharedStorage = (TZrExecIrValueId *)calloc(2u, sizeof(*sharedStorage));
    TEST_ASSERT_NOT_NULL(sharedStorage);
    target.values = sharedStorage;
    target.valueCapacity = 2u;
    target.roots = &sharedStorage[1];
    target.rootCapacity = 1u;
    memset(&request, 0, sizeof(request));
    request.function = &function;
    request.map = &map;
    request.generation = map.generation;
    request.sourceId = 42u;
    request.resumeId = 7u;
    request.phase = ZR_EXEC_IR_STATE_AFTER_EFFECT;
    request.target = &target;

    TEST_ASSERT_FALSE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED,
                      diagnostic.code);
    TEST_ASSERT_EQUAL_PTR(sharedStorage, target.values);
    target.values = ZR_NULL;
    target.valueCapacity = 0u;
    target.roots = ZR_NULL;
    target.rootCapacity = 0u;
    free(sharedStorage);
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
        instruction.flags = ZR_EXEC_IR_FLAG_MAY_SUSPEND;
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

void test_state_map_builder_rejects_unknown_value_enums(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;

    function_with_alloc_debug_boundary(&function);
    function.values[0].ownership = (EZrExecIrOwnership)-1;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_NULL(function.stateMap);

    function.values[0].ownership = ZR_EXEC_IR_OWNERSHIP_GC;
    function.values[0].nullability = (EZrExecIrNullability)-1;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_NULL(function.stateMap);
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

#include "ssa_state_map_verifier_cases.inc"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_state_map_verifier_rejects_attached_identity_mismatch);
    RUN_TEST(test_state_map_verifier_rejects_attached_storage_and_content);
    RUN_TEST(test_state_map_builder_replaces_stale_attached_map);
    RUN_TEST(test_state_map_find_supports_each_logical_phase);
    RUN_TEST(test_state_map_materialization_rejects_unknown_phase);
    RUN_TEST(test_state_map_materialization_rejects_inconsistent_exception_state);
    RUN_TEST(test_state_map_materialization_rejects_boundary_mismatch);
    RUN_TEST(test_state_map_materialization_rejects_forged_fallback_source);
    RUN_TEST(test_state_map_materialization_rejects_split_resume_identity);
    RUN_TEST(test_state_map_materialization_rejects_reused_resume_identity);
    RUN_TEST(test_state_map_materialization_rejects_unknown_value_enums);
    RUN_TEST(test_state_map_materialization_rejects_unreachable_handler);
    RUN_TEST(test_state_map_materialization_rejects_noncanonical_handler);
    RUN_TEST(test_state_map_materialization_requires_throw_boundary_for_handler);
    RUN_TEST(test_state_map_materialization_rejects_forged_owner_state);
    RUN_TEST(test_state_map_materialization_rejects_mismatched_deopt_identity);
    RUN_TEST(test_state_map_materialization_rejects_deopt_value_outside_live_state);
    RUN_TEST(test_state_map_materialization_rejects_missing_managed_root);
    RUN_TEST(test_state_map_materialization_rejects_malformed_owner_operand);
    RUN_TEST(test_state_map_materialization_rejects_duplicate_deopt_identity);
    RUN_TEST(test_state_map_clone_copies_pools_and_lifecycle);
    RUN_TEST(test_state_map_clone_rejects_aliased_destination);
    RUN_TEST(test_state_map_clone_rejects_interior_pool_alias);
    RUN_TEST(test_state_map_clone_rejects_overlapping_source_pools);
    RUN_TEST(test_state_map_clone_preserves_both_maps_on_each_pool_allocation_failure);
    RUN_TEST(test_state_map_clone_replaces_destination_after_last_pool_success);
    RUN_TEST(test_state_map_clone_skips_empty_pools_without_allocating);
    RUN_TEST(test_state_map_self_clone_does_not_allocate_or_change_pools);
    RUN_TEST(test_state_map_materialization_successfully_copies_logical_values);
    RUN_TEST(test_state_map_materialization_rejects_overlapping_map_pools);
    RUN_TEST(test_state_map_materialization_rejects_interior_target_alias);
    RUN_TEST(test_state_map_materialization_rejects_overlapping_target_arrays);
    RUN_TEST(test_state_map_materialization_is_transactional_on_failure);
    RUN_TEST(test_state_map_rejects_borrowed_value_across_suspend);
    RUN_TEST(test_state_map_builder_records_boundaries_and_is_liveness_conservative);
    RUN_TEST(test_state_map_builder_reuses_deopt_resume_identity);
    RUN_TEST(test_state_map_builder_rejects_duplicate_deopt_identity);
    RUN_TEST(test_state_map_builder_rejects_unknown_value_enums);
    RUN_TEST(test_state_map_builder_preserves_existing_map_on_verification_failure);
    RUN_TEST(test_state_map_materialization_accepts_managed_owner_roots);
    RUN_TEST(test_state_map_materialization_rejects_malformed_function_without_deref);
    RUN_TEST(test_state_map_function_clone_keeps_side_table_independent);
    return UNITY_END();
}

/**
 * @fn void test_state_map_builder_reuses_deopt_resume_identity(void)
 *
 * ## Summary
 * 防止 deopt 边界被重新编号：为含一个 deopt 状态的测试函数构建 map，并确认同一 source/resume 的 BEFORE_EFFECT 与 AFTER_EFFECT entry 都可查到且保留 resumeId 701。
 *
 * ## Returns
 * 本测试无 C 返回值；所有 Unity 断言通过时正常结束。断言失败由 Unity 退出当前 case 并继续 teardown 与 main 的后续派发，不代表整个测试进程立即结束。
 *
 * ## Constraints and lifecycle
 * 结论只覆盖该单一 deopt identity 及两种阶段 entry，不证明一般 deopt 去重、payload 恢复或其他 boundary phase。function 由 `function_with_deopt_boundary` 初始化，并在正常路径由 Core FreeFunction 释放；若前置断言失败，函数末尾的局部释放语句不会执行。
 *
 * ## References
 * 构造 helper 为 `function_with_deopt_boundary`；Unity main 在本文件 1390 行注册本 case；被测接口为 `ZrParser_ExecIr_BuildStateMaps`，entry 通过 `ZrCore_ExecIr_StateMapFind` 查询。
 */

/**
 * @fn void test_state_map_builder_rejects_duplicate_deopt_identity(void)
 *
 * ## Summary
 * 构造两个共享 deopt id 与 source id、但 resumeId 分别为 701 和 702 的状态记录，确认 builder 拒绝不唯一的 deopt 身份，报告 STATE_MAP_INVALID 且不发布 map。
 *
 * ## Returns
 * 本测试无 C 返回值；断言通过时正常结束。Unity 断言失败只退出当前 case，之后测试 harness 仍运行 teardown 与其余 main 派发。
 *
 * ## Constraints and lifecycle
 * 断言针对重复 id/source 的这一组矛盾 resume 记录；不测试 map 已存在时的失败保留或其他重复形式。测试通过 helper 创建函数后扩展 deoptStates 数组，并在正常路径释放函数；断言提前失败时局部释放语句会被跳过。
 *
 * ## References
 * 输入来自 `function_with_deopt_boundary`，重复项在本函数中复制并修改；Unity main 在本文件 1391 行注册，接口调用及诊断断言位于本函数内。
 */

/**
 * @fn void test_state_map_builder_records_boundaries_and_is_liveness_conservative(void)
 *
 * ## Summary
 * 以 ALLOC 与 DEBUG_POLL 边界构造 map，检查指定 source/resume 的 before/after entry、token/hash/generation 元数据，并确认 alloc-after 的 liveValues 与 rootValues 数量各为 1、live value pool 中的 ID 为 1，且死借用 value 2 不进入 valuePool。
 *
 * ## Returns
 * 本测试无 C 返回值；所有 Unity 断言通过时正常结束。断言失败由 Unity 结束当前 case 后继续 harness 流程，而非立即终止整个进程。
 *
 * ## Constraints and lifecycle
 * 覆盖仅限 `function_with_alloc_debug_boundary` 生成的两个边界和列明的 liveness/root 断言；测试没有检查 rootPool 中具体的 value ID，也不据此推断所有 phase、异常/挂起路径或所有 owner state 的覆盖。函数与其 map 在正常路径由 Core FreeFunction 释放；提前断言失败会跳过本函数末尾的释放语句。
 *
 * ## References
 * 测试图由 `function_with_alloc_debug_boundary` 构造；main 在本文件 1389 行注册该 case。map 检查使用 `ZrCore_ExecIr_StateMapFind` 与 map pools，生产接口为 `ZrParser_ExecIr_BuildStateMaps`。
 */

/**
 * @fn void test_state_map_builder_rejects_unknown_value_enums(void)
 *
 * ## Summary
 * 在同一 ALLOC/DEBUG_POLL fixture 上分别把 value ownership 与 nullability 改为无效枚举值，确认两种输入都返回失败、报告 INVALID_VALUE，并且不关联 state map。
 *
 * ## Returns
 * 本测试无 C 返回值；断言通过时正常结束。Unity 断言失败仅跳出当前 case，测试进程随后继续 teardown 与其他用例。
 *
 * ## Constraints and lifecycle
 * 第二轮先恢复有效的 GC ownership，再单独破坏 nullability；测试只覆盖这两个 -1 枚举输入，不覆盖其他畸形 value 字段。函数在正常路径释放；如果断言提前失败，局部 Core FreeFunction 不会执行。
 *
 * ## References
 * 测试数据来自 `function_with_alloc_debug_boundary`；Unity main 在本文件 1392 行注册，builder 与 INVALID_VALUE 断言位于本函数。
 */

/**
 * @fn void test_state_map_builder_preserves_existing_map_on_verification_failure(void)
 *
 * ## Summary
 * 先成功构建一个 map，再把入口 block 的 instructionRange.start 改为 UINT32_MAX，确认结构验证失败时诊断为 INVALID_BLOCK，旧 map 指针和 entryCount 均保持原值。
 *
 * ## Returns
 * 本测试无 C 返回值；断言通过时正常结束。Unity 断言失败只结束当前 case，之后 harness 继续运行 teardown 与剩余用例。
 *
 * ## Constraints and lifecycle
 * 本用例只证明 Core verification 早期失败时 map 指针与 entryCount 的保留，不证明 builder 分析、pool 扩容或最终 map 分配失败路径，也不逐字段比较旧 map。局部 function 在正常路径释放；断言提前失败会跳过末尾释放语句。
 *
 * ## References
 * 图由 `function_with_alloc_debug_boundary` 构造；main 在本文件 1393 行注册本 case；被测调用及 INVALID_BLOCK、旧指针和计数断言位于本函数。
 */

/**
 * @fn void test_state_map_clone_copies_pools_and_lifecycle(void)
 *
 * ## Summary
 * 从单条 AFTER_EFFECT/GC fixture 克隆一张 map，检查 generation、entry sourceId 与首个 value ID 相等，并确认 entries/valuePool 地址独立后可分别释放。
 *
 * ## Returns
 * 本测试无 C 返回值；Unity 断言均通过时正常返回。断言失败只 longjmp 离开当前 case，runner 继续 teardown 与其余用例。
 *
 * ## Constraints and lifecycle
 * 本用例只比较 entries 和 valuePool 指针及首个逻辑元素，没有逐项断言 rootPool/ownerStatePool 内容或地址；分别 Free 仅检查正常路径可完成释放，不增加对未断言字段的覆盖信用。source 与 clone 的局部资源在正常路径释放，提前失败可能跳过末尾释放。
 *
 * ## References
 * fixture 和被测调用位于本文件 875–878 行；实际字段及地址断言位于 879–883 行；main 在 1375 行注册。
 */

/**
 * @fn void test_state_map_clone_rejects_aliased_destination(void)
 *
 * ## Summary
 * 把 source 记录浅拷贝到 destination 形成共享池别名，确认 Clone 拒绝该 pair，且调用方脱离别名后 source entry 仍可读。
 *
 * ## Returns
 * 本测试无 C 返回值；Unity 断言均通过时正常返回。断言失败只退出当前 case，后续 teardown 和 Unity cases 仍继续。
 *
 * ## Constraints and lifecycle
 * 用例覆盖 map 记录不同地址但池指针完全相同的情况。失败后通过清零 destination 防止两张记录重复释放同一池；只断言 source.entries 非空及首个 sourceId，不比较完整 destination 状态。source 在正常路径释放；断言提前失败可能跳过局部清理。
 *
 * ## References
 * 浅拷贝、Clone 调用和 alias 分离位于本文件 894–898 行；source 断言与释放位于 899–901 行；main 在 1376 行注册。
 */

/**
 * @fn void test_state_map_clone_rejects_interior_pool_alias(void)
 *
 * ## Summary
 * 令 destination.valuePool 指向 source.valuePool 的容量内部槽，确认 Clone 以容量区间重叠拒绝此 cross-map interior alias。
 *
 * ## Returns
 * 本测试无 C 返回值；Unity 断言均通过时正常返回。断言失败仅结束当前 case，runner 会继续其余流程。
 *
 * ## Constraints and lifecycle
 * source 的 valuePool 扩至两个槽，destination 的 valuePool 从第二槽开始且 capacity 为一。Clone 后测试清除 destination 的借用指针以免误释放 source 内部地址，再检查 source 首值并释放 source；本例未逐字段快照 destination，也未覆盖其他池类型的 interior alias。断言早退可能跳过 detach/Free。
 *
 * ## References
 * source 扩容和 destination interior 地址设置位于本文件 909–918 行，失败断言及脱离借用位于 920–924 行；main 在 1377 行注册。
 */

/**
 * @fn void test_state_map_clone_rejects_overlapping_source_pools(void)
 *
 * ## Summary
 * 人为令 source 的 rootPool 指向其 valuePool 的第二槽，确认同一 map 内部池区间重叠会令 Clone 拒绝输入。
 *
 * ## Returns
 * 本测试无 C 返回值；Unity 断言均通过时正常返回。断言失败只退出当前 case，harness 随后继续 teardown 与剩余用例。
 *
 * ## Constraints and lifecycle
 * destination 是初始化后的空 map；用例只断言 Clone 失败且 destination.entries 仍为空，不对目标其他字节做快照。由于故意制造的 source 不可直接安全释放，测试在 Free 前清除 rootPool alias 和相应计数/容量。正常路径释放 source；断言提前失败可能跳过 alias 修复和释放。
 *
 * ## References
 * source 扩容、旧 root pool 释放与内部 alias 构造位于本文件 932–943 行；失败及最小 destination 断言、释放前修复位于 945–949 行；main 在 1378 行注册。
 */

/**
 * @fn static void map_with_one_entry(SZrExecIrStateMap *map, EZrExecIrStateMapPhase phase, TZrUInt32 flags)
 *
 * ## Summary
 * 在测试中构造包含一个 entry、一个 live ValueId、一个 root ValueId 和一个 owner-state 项的独立 map，并提供少量固定身份字段供 checkpoint 用例复用。
 *
 * ## Parameters
 * @param[out] map 调用方提供的无资源 map 记录；helper 先调用 StateMapInit。
 * @param[in] phase 原样写入 entry 的逻辑阶段；本 helper 不验证枚举值。
 * @param[in] flags 原样写入 entry 的 boundary flags；零值也会被保留以供负例构造。
 *
 * ## Returns
 * 无 C 返回值；四个各含一个元素的池构造完成后正常返回，分配断言失败会由 Unity 离开当前 case。
 *
 * ## Constraints and lifecycle
 * map 记录须新鲜或已释放，否则 Init 会丢失旧池指针。helper 固定 generation=7、sourceId=42、instructionId=1、resumeId=7 和单元素池，但不设置 functionToken/signatureHash；要用于匹配函数的正向验证时，调用方须另行填入与 function 一致的身份字段。phase 与 flags 原样写入，零 flags 仅适合负例，本 helper 不保证构成任意合法 map。成功时调用方取得四池释放责任；若中途 TEST_ASSERT 失败，Unity longjmp 会跳过调用方后续清理，fixture 的 setUp/tearDown 不回收该局部 map。此 helper 只准备测试字段，不验证 map。
 *
 * ## References
 * 定义位于本文件 55 行；直接调用分布在本文件 249 行、875 行、958 行及本批其余 materialization cases；clone fault fixture 经本文件 888 行包含的 test_ssa_state_map_clone_fault_cases.h 15 行间接调用。
 */

/**
 * @fn static void function_with_one_gc_value(SZrExecIrFunction *function)
 *
 * ## Summary
 * 构造用于 state-map 测试的最小函数：一个 nullable GC external value 和一个 MAY_GC NOP 边界。
 *
 * ## Parameters
 * @param[out] function 调用方提供的无资源函数记录；helper 先调用 FunctionInit。
 *
 * ## Returns
 * 无 C 返回值；函数值与 NOP 指令加入成功后正常返回，Unity 断言失败会离开当前 case。
 *
 * ## Constraints and lifecycle
 * function 须新鲜或已释放；FunctionInit 不负责释放已有资源。正常返回后函数池由调用方通过 Core FreeFunction 释放；若内部断言提前失败，Unity longjmp 会跳过调用者的尾部释放，空 setUp/tearDown 不接管这些局部对象。该 fixture 固定设置 functionToken、signatureHash、generation 和 sourceId，但只添加值与一条 MAY_GC NOP，不构造可执行 CFG，也不代表一般函数构造。
 *
 * ## References
 * 定义位于本文件 91 行；除本文件直接调用外，function_with_two_gc_boundaries、function_with_drop_boundary 和 function_with_deopt_boundary 分别在本文件 111 行、124 行和 136 行复用。
 */

/**
 * @fn void test_state_map_materialization_successfully_copies_logical_values(void)
 *
 * ## Summary
 * 对一个 AFTER_EFFECT、GC boundary checkpoint 执行 MaterializeState，并检查少量逻辑状态字段及首个 ValueId。
 *
 * ## Returns
 * 本 case 无 C 返回值；断言通过时正常返回，失败由 Unity runner 记录，最终由 UNITY_END 返回失败计数。正常路径显式释放 target、function 和 map；提前断言失败可能跳过局部释放。
 *
 * ## Constraints and lifecycle
 * 仅断言 sourceId、valueCount/rootCount 各为 1，以及 values[0]/roots[0] 等于逻辑 ValueId 1；这些数组保存 ValueId，不是运行时对象或 GC 保活指针。用例未检查 ownerStates、所有池内容或一般失败清理。
 *
 * ## References
 * 定义位于本文件 952 行，materialization 调用与断言位于 972–977 行，Unity 注册位于 1383 行，CTest 目标注册位于 tests/cmake/ssa-tests.cmake 422 行。
 */

/**
 * @fn void test_state_map_materialization_rejects_overlapping_map_pools(void)
 *
 * ## Summary
 * 将 source map 的 rootPool 指向 valuePool 第二个元素，检查重叠源池导致 materialization 拒绝。
 *
 * ## Returns
 * 无 C 返回值；断言通过时 case 正常返回；Unity 断言失败只退出当前 case，runner 继续执行。正常路径先解除故意构造的 source alias，再释放 map/function。
 *
 * ## Constraints and lifecycle
 * 只断言失败、diagnostic.code 为 STATE_MAP_INVALID 及 target.values 仍为 NULL；不检查其他 target 字段或通用事务行为。rootPool 借用了 valuePool 内部地址，因此清理前必须将 rootPool/count/capacity 清零，避免重复释放。
 *
 * ## References
 * alias 构造位于本文件 993–1000 行，失败与字段断言位于 1015–1017 行，修复清理位于 1018–1022 行，Unity 注册位于 1384 行。
 */

/**
 * @fn void test_state_map_materialization_rejects_interior_target_alias(void)
 *
 * ## Summary
 * 将 target.values 指向 map.valuePool 内部的一个元素，验证目标池覆盖输入池时被拒绝。
 *
 * ## Returns
 * 无 C 返回值；断言通过时 case 正常返回；Unity 断言失败只退出当前 case，runner 继续执行。
 *
 * ## Constraints and lifecycle
 * 仅断言 MaterializeState 失败且诊断为 MATERIALIZATION_FAILED。target.values 是借自 map 的内部地址，不由 target 拥有；用例在释放 map 前将该字段与容量清零，不能对该 borrow 调用 MaterializedStateFree。
 *
 * ## References
 * 扩容和 alias 设置位于本文件 1035–1045 行，失败断言位于 1055–1057 行，解除借用和正常释放位于 1058–1061 行，Unity 注册位于 1385 行。
 */

/**
 * @fn void test_state_map_materialization_rejects_overlapping_target_arrays(void)
 *
 * ## Summary
 * 让 target.values 与 target.roots 指向同一分配的重叠区间，验证目标池之间重叠时被拒绝。
 *
 * ## Returns
 * 无 C 返回值；断言通过时 case 正常返回；Unity 断言失败只退出当前 case，runner 继续执行。
 *
 * ## Constraints and lifecycle
 * 仅断言失败、diagnostic.code 为 MATERIALIZATION_FAILED，以及 target.values 基址仍保留。两个字段共享一块分配，用例清空 target 两个借用字段后手动释放 sharedStorage；不构成对其他字段的完整保留或通用失败事务证明。
 *
 * ## References
 * 目标 alias 构造位于本文件 1078–1083 行，失败与断言位于 1093–1096 行，解除 alias 和释放位于 1097–1103 行，Unity 注册位于 1386 行。
 */

/**
 * @fn void test_state_map_materialization_is_transactional_on_failure(void)
 *
 * ## Summary
 * 通过一个无效 checkpoint 触发 MaterializeState 失败，并检查原 target 的 values 指针、计数和首个值仍保留。
 *
 * ## Returns
 * 无 C 返回值；断言通过时 case 正常返回；Unity 断言失败只退出当前 case，runner 继续执行。
 *
 * ## Constraints and lifecycle
 * fixture 在本用例以 AFTER_EFFECT 和 flags=0 构造 map；ValidateStateMap 会因 boundaryFlags 为零先拒绝，早于请求的 generation mismatch 检查。因此本测试只覆盖 map-validation rejection 后 values 指针、valueCount=1 和 values[0]=99 的保留，不断言 diagnostic.code，也不覆盖 stale-generation、分配失败或一般事务性。正常路径释放 target/function/map；Unity 提前失败可能跳过这些局部释放。
 *
 * ## References
 * flags=0 和请求初始化位于本文件 1113–1130 行，实际失败及 target 断言位于 1131–1134 行；验证先于 generation 检查的顺序见 exec_ir_materialize.c 685 行和 704 行，boundaryFlags 拒绝见 484 行；Unity 注册位于本文件 1387 行。
 */

/**
 * @fn void test_state_map_find_supports_each_logical_phase(void)
 *
 * ## Summary
 * 验证三个受支持逻辑 phase 均可查到相应 entry，并拒绝 COUNT 哨兵和负一枚举值。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无 C 返回值；所有 Unity 断言通过时正常结束此 case。断言失败由 runner 记录；当前 Unity setjmp 路径会跳过本函数后续语句，因此可能跳过末尾的 StateMapFree，随后 runner 调用空 tearDown，最终 UNITY_END 返回失败计数。
 *
 * ## Constraints and lifecycle
 * 初始化空 map 后分配三项 entry，均设 sourceId=10、resumeId=20，phase 按0、1、2设置；逐项查询只断言结果非空和返回 phase 相等，另断言 PHASE_COUNT 及 (EZrExecIrStateMapPhase)-1 返回 NULL。成功路径在 tests/parser/test_ssa_state_maps.c:239 释放 map。setUp/tearDown 为空；未覆盖 NULL map、resumeId=0、未命中、重复键首项选择、损坏shape或借用指针失效。Unity 配置入口当前为空，不屏蔽默认 setjmp/longjmp 断言中止路径。
 *
 * ## References
 * 定义：tests/parser/test_ssa_state_maps.c:217；Unity 注册：tests/parser/test_ssa_state_maps.c:1358；空 setUp/tearDown：tests/parser/test_ssa_state_maps.c:12；tests/parser/test_ssa_state_maps.c:13；目标定义和 CTest 注册：tests/cmake/ssa-tests.cmake:397；tests/cmake/ssa-tests.cmake:422；Unity 配置：tests/cmake/ssa-tests.cmake:420；tests/unity_config.h:4；RUN_TEST 到 UnityDefaultTestRun：tests/third_party/zr_unity/Unity/src/unity_internals.h:909；tests/third_party/zr_unity/Unity/src/unity_internals.h:910；断言失败及 longjmp：tests/third_party/zr_unity/Unity/src/unity_internals.h:983；tests/third_party/zr_unity/Unity/src/unity_internals.h:891；tests/third_party/zr_unity/Unity/src/unity.c:2239；tests/third_party/zr_unity/Unity/src/unity.c:2247；runner 调用 teardown：tests/third_party/zr_unity/Unity/src/unity.c:2283；tests/third_party/zr_unity/Unity/src/unity.c:2303；最终进程结果：tests/parser/test_ssa_state_maps.c:1397。查询实现：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:139。
 */
