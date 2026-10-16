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

/**
 * @fn static void function_with_two_gc_boundaries(SZrExecIrFunction *function)
 *
 * ## Summary
 * 复用单 GC value fixture，再追加一条 sourceId=84、带 MAY_GC 的 NOP，供两个不同 checkpoint 复用 resumeId 的负例构造。
 *
 * ## Parameters
 * @param[out] function 非空、可写的新鲜或已释放 function 记录；正常返回后由调用方拥有并释放构造所得的池。
 *
 * ## Returns
 * 正常返回时 function 含原 sourceId=42 的 MAY_GC NOP 与新增 sourceId=84 的 MAY_GC NOP。 本函数为 void，无 C 返回值；构造断言失败时由 Unity 记录失败并跳出调用此 helper 的当前 case。
 *
 * ## Constraints and lifecycle
 * function 必须非空、可写且是新鲜或已释放的记录：本 helper 会直接或经基础 helper 调用 FunctionInit，不能用于覆盖仍拥有池的 function。正常路径由调用方最终调用 FreeFunction 释放池；helper 不取得记录对象本身的所有权。构造操作或分配后的 Unity 断言失败会跳出所属 case，可能跳过该 case 的尾部清理；空 setUp/tearDown 不补做局部资源回收。
 * 内部先调用 function_with_one_gc_value，因此使用其固定 token、signature 和 generation；不建立 CFG、stateMap 或运行时对象。本 helper 只断言追加第二条 instruction 成功。
 *
 * ## References
 * 定义及首次声明：tests/parser/test_ssa_state_maps.c:108。
 * 全部直接调用：tests/parser/test_ssa_state_maps.c:420。
 * 构造依赖：function_with_one_gc_value，tests/parser/test_ssa_state_maps.c:111；ZrCore_ExecIr_FunctionAppendInstruction，tests/parser/test_ssa_state_maps.c:116。
 * 本体断言：tests/parser/test_ssa_state_maps.c:116。
 */

/**
 * @fn static void function_with_drop_boundary(SZrExecIrFunction *function)
 *
 * ## Summary
 * 在单 GC value fixture 后追加 ValueId=1 的 operand 范围和 sourceId=84 的 DROP，供 owner-state 与 operand 畸形负例修改。
 *
 * ## Parameters
 * @param[out] function 非空、可写的新鲜或已释放 function 记录；正常返回后由调用方拥有并释放构造所得的池。
 *
 * ## Returns
 * 正常返回时 function 拥有新增 operand 池内容及第二条 DROP instruction。 本函数为 void，无 C 返回值；构造断言失败时由 Unity 记录失败并跳出调用此 helper 的当前 case。
 *
 * ## Constraints and lifecycle
 * function 必须非空、可写且是新鲜或已释放的记录：本 helper 会直接或经基础 helper 调用 FunctionInit，不能用于覆盖仍拥有池的 function。正常路径由调用方最终调用 FreeFunction 释放池；helper 不取得记录对象本身的所有权。构造操作或分配后的 Unity 断言失败会跳出所属 case，可能跳过该 case 的尾部清理；空 setUp/tearDown 不补做局部资源回收。
 * 不把 GC fixture 解释为可执行 CFG；不执行 DROP，也不建立 stateMap。AppendOperands 与 AppendInstruction 的返回值分别由 Unity 断言检查。
 *
 * ## References
 * 定义及首次声明：tests/parser/test_ssa_state_maps.c:119。
 * 全部直接调用：tests/parser/test_ssa_state_maps.c:634；tests/parser/test_ssa_state_maps.c:772。
 * 构造依赖：function_with_one_gc_value，tests/parser/test_ssa_state_maps.c:124；ZrCore_ExecIr_FunctionAppendOperands，tests/parser/test_ssa_state_maps.c:125；ZrCore_ExecIr_FunctionAppendInstruction，tests/parser/test_ssa_state_maps.c:131。
 * 本体断言：tests/parser/test_ssa_state_maps.c:125；tests/parser/test_ssa_state_maps.c:131。
 */

/**
 * @fn static void function_with_deopt_boundary(SZrExecIrFunction *function)
 *
 * ## Summary
 * 在单 GC value fixture 上设置 deoptId=91，并分配一条 sourceId=42、resumeId=701、payload ValueId=1 的 deopt state。
 *
 * ## Parameters
 * @param[out] function 非空、可写的新鲜或已释放 function 记录；正常返回后由调用方拥有并释放构造所得的池。
 *
 * ## Returns
 * 正常返回时 deoptStates 与 deoptValues 两个池各有一个元素，count 与 capacity 均为 1，第一条 instruction 的 deoptId 为 91。 本函数为 void，无 C 返回值；构造断言失败时由 Unity 记录失败并跳出调用此 helper 的当前 case。
 *
 * ## Constraints and lifecycle
 * function 必须非空、可写且是新鲜或已释放的记录：本 helper 会直接或经基础 helper 调用 FunctionInit，不能用于覆盖仍拥有池的 function。正常路径由调用方最终调用 FreeFunction 释放池；helper 不取得记录对象本身的所有权。构造操作或分配后的 Unity 断言失败会跳出所属 case，可能跳过该 case 的尾部清理；空 setUp/tearDown 不补做局部资源回收。
 * 该 helper 的 state/payload 由 calloc 独立分配；若一项分配成功而后续断言失败，可留下部分 function 状态，不提供回滚。固定 identity 用于有意制造匹配或矛盾的测试，不是任意合法 deopt 图构造器，不构造可执行 CFG。
 *
 * ## References
 * 定义及首次声明：tests/parser/test_ssa_state_maps.c:135。
 * 全部直接调用：tests/parser/test_ssa_state_maps.c:667；tests/parser/test_ssa_state_maps.c:706；tests/parser/test_ssa_state_maps.c:807；tests/parser/test_ssa_state_maps.c:840；tests/parser/test_ssa_state_maps.c:858。
 * 构造依赖：function_with_one_gc_value，tests/parser/test_ssa_state_maps.c:136；calloc deoptStates，tests/parser/test_ssa_state_maps.c:138；calloc deoptValues，tests/parser/test_ssa_state_maps.c:140。
 * 本体断言：tests/parser/test_ssa_state_maps.c:142；tests/parser/test_ssa_state_maps.c:143。
 */

/**
 * @fn static void function_with_alloc_debug_boundary(SZrExecIrFunction *function)
 *
 * ## Summary
 * 构造供 builder、verifier 与 FunctionClone 测试复用的单 entry block：ALLOC、DEBUG_POLL NOP、RETURN 三条 instruction，并保留一个不被 instruction 使用的 BORROWED value。
 *
 * ## Parameters
 * @param[out] function 非空、可写的新鲜或已释放 function 记录；正常返回后由调用方拥有并释放构造所得的池。
 *
 * ## Returns
 * 正常返回时固定 token=77、signatureHash=0x1234、generation=11，function 拥有两个 value、一个 entry block 及相关 instruction/result/operand/memory-token 池。 本函数为 void，无 C 返回值；构造断言失败时由 Unity 记录失败并跳出调用此 helper 的当前 case。
 *
 * ## Constraints and lifecycle
 * function 必须非空、可写且是新鲜或已释放的记录：本 helper 会直接或经基础 helper 调用 FunctionInit，不能用于覆盖仍拥有池的 function。正常路径由调用方最终调用 FreeFunction 释放池；helper 不取得记录对象本身的所有权。构造操作或分配后的 Unity 断言失败会跳出所属 case，可能跳过该 case 的尾部清理；空 setUp/tearDown 不补做局部资源回收。
 * ValueId=1 为 nullable GC value，ALLOC 定义它且 RETURN 使用它；ValueId=2 为 nullable BORROWED value，未加入 instruction operand，因此该 fixture 不把它当作跨 SUSPEND 的活跃借用。ALLOC 带 MAY_ALLOCATE|MAY_GC、effect 1→2 和 memoryOut token=1；DEBUG_POLL 与 RETURN 的 sourceId 分别为 20 与 30。这里只生成 IR 供测试接口读取，没有运行解释器或 Oracle，也没有创建实际 GC 对象。
 *
 * ## References
 * 定义及首次声明：tests/parser/test_ssa_state_maps.c:154。
 * 全部直接调用：tests/parser/test_ssa_state_maps.c:1192；tests/parser/test_ssa_state_maps.c:1225；tests/parser/test_ssa_state_maps.c:1245；tests/parser/test_ssa_state_maps.c:1338；tests/parser/ssa_state_map_verifier_cases.inc:5；tests/parser/ssa_state_map_verifier_cases.inc:26；tests/parser/ssa_state_map_verifier_cases.inc:50。
 * 构造依赖：ZrCore_ExecIr_FunctionInit，tests/parser/test_ssa_state_maps.c:162；ZrCore_ExecIr_FunctionAddValue，tests/parser/test_ssa_state_maps.c:167；ZrCore_ExecIr_FunctionAddValue，tests/parser/test_ssa_state_maps.c:173；ZrCore_ExecIr_FunctionAddBlock，tests/parser/test_ssa_state_maps.c:177；ZrCore_ExecIr_FunctionAppendResults，tests/parser/test_ssa_state_maps.c:186；ZrCore_ExecIr_FunctionAppendMemoryTokens，tests/parser/test_ssa_state_maps.c:190；ZrCore_ExecIr_FunctionAppendInstruction，tests/parser/test_ssa_state_maps.c:192；ZrCore_ExecIr_FunctionAppendInstruction，tests/parser/test_ssa_state_maps.c:199；ZrCore_ExecIr_FunctionAppendOperands，tests/parser/test_ssa_state_maps.c:205；ZrCore_ExecIr_FunctionAppendInstruction，tests/parser/test_ssa_state_maps.c:208。
 * 本体断言：tests/parser/test_ssa_state_maps.c:170；tests/parser/test_ssa_state_maps.c:173；tests/parser/test_ssa_state_maps.c:176；tests/parser/test_ssa_state_maps.c:186；tests/parser/test_ssa_state_maps.c:190；tests/parser/test_ssa_state_maps.c:192；tests/parser/test_ssa_state_maps.c:199；tests/parser/test_ssa_state_maps.c:205；tests/parser/test_ssa_state_maps.c:208。
 * 文本 include 入口：tests/parser/test_ssa_state_maps.c:1351。
 */

/**
 * @fn void test_state_map_materialization_rejects_inconsistent_exception_state(void)
 *
 * ## Summary
 * 在 AFTER_EFFECT/GC checkpoint 中故意保留 THROW exceptionState，通过物化入口检查该不一致被拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：AFTER_EFFECT/GC entry的exceptionState改为THROW。
 * 实际断言：false；STATE_MAP_INVALID；target valueCount=0/values=NULL。
 * 只构造 GC flags 与 THROW exceptionState 的这一种矛盾，不推断其他异常组合。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:17；定义：tests/parser/test_ssa_state_maps.c:272；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1360。
 * 断言：tests/parser/test_ssa_state_maps.c:294；tests/parser/test_ssa_state_maps.c:295；tests/parser/test_ssa_state_maps.c:296；tests/parser/test_ssa_state_maps.c:297。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:279；tests/parser/test_ssa_state_maps.c:282；正常释放：tests/parser/test_ssa_state_maps.c:298；tests/parser/test_ssa_state_maps.c:299；tests/parser/test_ssa_state_maps.c:300。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_entry_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:486。
 */

/**
 * @fn void test_state_map_materialization_rejects_boundary_mismatch(void)
 *
 * ## Summary
 * 让 map 声明 GC 边界而对应 instruction 仅带 MAY_SUSPEND，通过物化入口检查边界种类不一致。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：map声明GC，instruction改为MAY_SUSPEND。
 * 实际断言：false；STATE_MAP_INVALID；target.values=NULL。
 * 只断言 target.values 仍为空，不检查 valueCount 或其余目标字段。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:18；定义：tests/parser/test_ssa_state_maps.c:303；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1361。
 * 断言：tests/parser/test_ssa_state_maps.c:325；tests/parser/test_ssa_state_maps.c:326；tests/parser/test_ssa_state_maps.c:327。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:310；tests/parser/test_ssa_state_maps.c:312；正常释放：tests/parser/test_ssa_state_maps.c:328；tests/parser/test_ssa_state_maps.c:329；tests/parser/test_ssa_state_maps.c:330。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_entry_boundary_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:460；zr_state_map_entry_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:490。
 */

/**
 * @fn void test_state_map_materialization_rejects_forged_fallback_source(void)
 *
 * ## Summary
 * 将 instruction.sourceId 清零，但保留 map/request 的 sourceId=42，检查物化拒绝及诊断中的 expected/actual 字段。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：instruction sourceId=0，map/request sourceId=42。
 * 实际断言：false；STATE_MAP_INVALID；expectedVersion=1、actualVersion=42；values=NULL。
 * 实际断言 expectedVersion=1、actualVersion=42；只记录这组输入的字段结果，不把测试名推广为所有 source fallback 情况。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:19；定义：tests/parser/test_ssa_state_maps.c:333；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1362。
 * 断言：tests/parser/test_ssa_state_maps.c:356；tests/parser/test_ssa_state_maps.c:357；tests/parser/test_ssa_state_maps.c:358；tests/parser/test_ssa_state_maps.c:359；tests/parser/test_ssa_state_maps.c:360。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:340；tests/parser/test_ssa_state_maps.c:342；正常释放：tests/parser/test_ssa_state_maps.c:361；tests/parser/test_ssa_state_maps.c:362；tests/parser/test_ssa_state_maps.c:363。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_entry_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:532；zr_state_map_set_diagnostic，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:535。
 */

/**
 * @fn void test_state_map_materialization_rejects_split_resume_identity(void)
 *
 * ## Summary
 * 为同一 instruction/source 复制出 BEFORE_EFFECT 与 AFTER_EFFECT 两项，并给它们不同 resumeId，检查物化拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：复制entry形成同instruction/source但不同resume的BEFORE/AFTER项。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * realloc 直接写回 map.entries；若分配断言早退，不承诺局部资源回滚。该负例不证明同 identity 的不同 phase 正例已通过验证。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:20；定义：tests/parser/test_ssa_state_maps.c:366；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1363。
 * 断言：tests/parser/test_ssa_state_maps.c:396；tests/parser/test_ssa_state_maps.c:397；tests/parser/test_ssa_state_maps.c:398。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:373；tests/parser/test_ssa_state_maps.c:383；正常释放：tests/parser/test_ssa_state_maps.c:399；tests/parser/test_ssa_state_maps.c:400；tests/parser/test_ssa_state_maps.c:401。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：ZrCore_ExecIr_ValidateStateMap::checkpoint_identity_consistency，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:626。
 */

/**
 * @fn void test_state_map_materialization_rejects_reused_resume_identity(void)
 *
 * ## Summary
 * 在两个不同 instruction/source 的 MAY_GC checkpoint 中复用 resumeId=7，检查物化拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：不同instruction/source的两个MAY_GC boundary复用resume 7。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * realloc 直接写回 map.entries，分配断言早退时不保证局部清理。此例不要求 sourceId 全局唯一，也不覆盖所有 pairwise identity 规则。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:21；定义：tests/parser/test_ssa_state_maps.c:404；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1364。
 * 断言：tests/parser/test_ssa_state_maps.c:433；tests/parser/test_ssa_state_maps.c:434；tests/parser/test_ssa_state_maps.c:435。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:411；tests/parser/test_ssa_state_maps.c:420；正常释放：tests/parser/test_ssa_state_maps.c:436；tests/parser/test_ssa_state_maps.c:437；tests/parser/test_ssa_state_maps.c:438。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：ZrCore_ExecIr_ValidateStateMap::checkpoint_identity_consistency，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:636。
 */

/**
 * @fn void test_state_map_materialization_rejects_unknown_value_enums(void)
 *
 * ## Summary
 * 清空 root 需求后，分别注入无效 ownership 与无效 nullability 枚举，检查两轮物化均被拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：ownership设-1；第二轮设UNKNOWN并将nullability设-1，root range/count清零。
 * 实际断言：两轮均false、STATE_MAP_INVALID、values=NULL。
 * 第二轮把 ownership 设为 UNKNOWN 后再令 nullability=-1；两轮只覆盖显式的 -1 枚举，不代表穷尽所有 value 结构畸形。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:22；定义：tests/parser/test_ssa_state_maps.c:441；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1365。
 * 断言：tests/parser/test_ssa_state_maps.c:466；tests/parser/test_ssa_state_maps.c:467；tests/parser/test_ssa_state_maps.c:468；tests/parser/test_ssa_state_maps.c:472；tests/parser/test_ssa_state_maps.c:473；tests/parser/test_ssa_state_maps.c:474。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:448；tests/parser/test_ssa_state_maps.c:452；正常释放：tests/parser/test_ssa_state_maps.c:475；tests/parser/test_ssa_state_maps.c:476；tests/parser/test_ssa_state_maps.c:477。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_function_values_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:195；ZrCore_ExecIr_ValidateStateMap，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:572。
 */

/**
 * @fn void test_state_map_materialization_rejects_unreachable_handler(void)
 *
 * ## Summary
 * 构造 MAY_THROW instruction 与单独的 exception block，但不为 entry block 添加到该 handler 的 successor，检查物化拒绝该 handler 指定。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：THROW entry引用exception block，MAY_THROW entry block无到handler successor。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * 只覆盖当前 block 没有该 successor、却在 entry 中保存非 INVALID handler 的场景；不证明一般 CFG 可达性，也不把 THROW 的 INVALID/INVALID 比较写成必然失败。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:23；定义：tests/parser/test_ssa_state_maps.c:480；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1366。
 * 断言：tests/parser/test_ssa_state_maps.c:512；tests/parser/test_ssa_state_maps.c:513；tests/parser/test_ssa_state_maps.c:514。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:487；tests/parser/test_ssa_state_maps.c:491；tests/parser/test_ssa_state_maps.c:493；tests/parser/test_ssa_state_maps.c:495；正常释放：tests/parser/test_ssa_state_maps.c:515；tests/parser/test_ssa_state_maps.c:516；tests/parser/test_ssa_state_maps.c:517。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_expected_handler，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:431；zr_state_map_entry_handler_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:455。
 */

/**
 * @fn void test_state_map_materialization_rejects_noncanonical_handler(void)
 *
 * ## Summary
 * 按 exception block、cleanup block 的顺序添加两个合格 successor，却让 entry 指向第二个，检查物化拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：successor先exception再cleanup，map handler指向后者。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * 本例的规则来自 successor 数组顺序，不表示 EXCEPTION 在所有图中优先于 CLEANUP。仅检查该两项顺序与 handler 选择。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:24；定义：tests/parser/test_ssa_state_maps.c:520；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1367。
 * 断言：tests/parser/test_ssa_state_maps.c:562；tests/parser/test_ssa_state_maps.c:563；tests/parser/test_ssa_state_maps.c:564。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:528；tests/parser/test_ssa_state_maps.c:531；tests/parser/test_ssa_state_maps.c:533；tests/parser/test_ssa_state_maps.c:535；tests/parser/test_ssa_state_maps.c:537；tests/parser/test_ssa_state_maps.c:548；正常释放：tests/parser/test_ssa_state_maps.c:565；tests/parser/test_ssa_state_maps.c:566；tests/parser/test_ssa_state_maps.c:567。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_expected_handler，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:431；zr_state_map_entry_handler_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:457。
 */

/**
 * @fn void test_state_map_materialization_requires_throw_boundary_for_handler(void)
 *
 * ## Summary
 * 先物化一个带可选定 handler 的 THROW checkpoint，再改成 GC 且保留 handler，检查后一次拒绝及有限目标字段保留。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：先成功物化THROW/MAY_THROW及有效handler；再把map改为GC/exception=0、instruction改MAY_GC。
 * 实际断言：先断言成功和handler id；后续false/STATE_MAP_INVALID，旧values指针和handler id保留。
 * 第一轮仅断言成功与 handlerBlockId；第二轮仅断言 false、STATE_MAP_INVALID、旧 values 指针和 handlerBlockId 保留，不比较其他池或完整 target 快照。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:25；定义：tests/parser/test_ssa_state_maps.c:570；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1368。
 * 断言：tests/parser/test_ssa_state_maps.c:606；tests/parser/test_ssa_state_maps.c:607；tests/parser/test_ssa_state_maps.c:613；tests/parser/test_ssa_state_maps.c:614；tests/parser/test_ssa_state_maps.c:615；tests/parser/test_ssa_state_maps.c:616。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:579；tests/parser/test_ssa_state_maps.c:583；tests/parser/test_ssa_state_maps.c:585；tests/parser/test_ssa_state_maps.c:587；tests/parser/test_ssa_state_maps.c:593；正常释放：tests/parser/test_ssa_state_maps.c:617；tests/parser/test_ssa_state_maps.c:618；tests/parser/test_ssa_state_maps.c:619。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_entry_handler_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:452；ZrCore_ExecIr_MaterializeState，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685。
 */

/**
 * @fn void test_state_map_materialization_rejects_forged_owner_state(void)
 *
 * ## Summary
 * 将 DROP 前的 CLEANUP checkpoint owner state 伪造为 DROPPED，检查物化拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：BEFORE/CLEANUP对应DROP；owner state伪造DROPPED。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * 只覆盖该一项伪造 owner state，不推断整个 OwnerAnalysis 或运行时 DROP 行为已审。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:26；定义：tests/parser/test_ssa_state_maps.c:622；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1369。
 * 断言：tests/parser/test_ssa_state_maps.c:647；tests/parser/test_ssa_state_maps.c:648；tests/parser/test_ssa_state_maps.c:649。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:629；tests/parser/test_ssa_state_maps.c:634；正常释放：tests/parser/test_ssa_state_maps.c:650；tests/parser/test_ssa_state_maps.c:651；tests/parser/test_ssa_state_maps.c:652。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_entry_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503；zr_state_map_owners_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:65；zr_state_map_owners_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:75。
 */

/**
 * @fn void test_state_map_materialization_rejects_mismatched_deopt_identity(void)
 *
 * ## Summary
 * 让 entry/request 使用 resumeId=7，而关联 deopt state 使用 resumeId=701，检查物化拒绝该 identity 不一致。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：map带GC/DEOPT/GUARD_EXIT和deoptId91；entry/request resume7但function deopt resume701。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * 只断言失败、STATE_MAP_INVALID 和 target.values 为空，不恢复 deopt payload，也不检查具体首个拒绝分支。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:27；定义：tests/parser/test_ssa_state_maps.c:655；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1370。
 * 断言：tests/parser/test_ssa_state_maps.c:680；tests/parser/test_ssa_state_maps.c:681；tests/parser/test_ssa_state_maps.c:682。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:662；tests/parser/test_ssa_state_maps.c:667；正常释放：tests/parser/test_ssa_state_maps.c:683；tests/parser/test_ssa_state_maps.c:684；tests/parser/test_ssa_state_maps.c:685。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_entry_deopt_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:362；ZrCore_ExecIr_ValidateStateMap，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573。
 */

/**
 * @fn void test_state_map_materialization_rejects_deopt_value_outside_live_state(void)
 *
 * ## Summary
 * 将 deopt checkpoint 的 live/root/owner 范围置空并检查物化拒绝；精确记录当前构造先在 owner/liveness 的缺 live occurrence 检查失败。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * entry/request 的 resumeId 已改为 701，与 fixture 的 deopt state 对应；deopt payload 仍包含 ValueId=1，而 entry 的 live/root/owner ranges 被置空。
 * 实际断言仅为 MaterializeState 返回 false、STATE_MAP_INVALID，以及 target.values 仍为空。
 * 该 fixture 的 deopt payload ValueId=1 被 liveness 计入 after 集合，而 entry.liveValues.count=0；在 entry_valid 调用 owners_valid 时先因必需 live value 没有 occurrence 被拒绝，未到后续 entry_deopt 检查。本例的 false 不为 deopt payload-membership 分支授信，也不表示执行了值恢复。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 上述首拒绝点由当前源码短路顺序与 liveness 构造静态闭合；本提案没有运行或追踪该 case，也不为 liveness/owner/deopt 依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:28；定义：tests/parser/test_ssa_state_maps.c:688；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1371。
 * 构造及置空：tests/parser/test_ssa_state_maps.c:695；tests/parser/test_ssa_state_maps.c:701；tests/parser/test_ssa_state_maps.c:702；tests/parser/test_ssa_state_maps.c:703；tests/parser/test_ssa_state_maps.c:706；断言：tests/parser/test_ssa_state_maps.c:719；tests/parser/test_ssa_state_maps.c:720；tests/parser/test_ssa_state_maps.c:721。
 * 验证路径：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:614；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:621；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:52；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:62；deopt liveness 加入点：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:122。
 * 未到达的后续 deopt 检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:504；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:377。
 * 正常释放：tests/parser/test_ssa_state_maps.c:722；tests/parser/test_ssa_state_maps.c:723；tests/parser/test_ssa_state_maps.c:724。
 */

/**
 * @fn void test_state_map_materialization_rejects_missing_managed_root(void)
 *
 * ## Summary
 * 在带 GC ownership value 的 checkpoint 中移除 root 范围，检查物化拒绝遗漏 managed root。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：function含managed GC value；entry root range/count清零。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * 只覆盖此单 value、单 root 缺失配置；不证明实际 GC 对象保活或所有 owner kind 的负例。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:29；定义：tests/parser/test_ssa_state_maps.c:727；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1372。
 * 断言：tests/parser/test_ssa_state_maps.c:751；tests/parser/test_ssa_state_maps.c:752；tests/parser/test_ssa_state_maps.c:753。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:734；tests/parser/test_ssa_state_maps.c:738；正常释放：tests/parser/test_ssa_state_maps.c:754；tests/parser/test_ssa_state_maps.c:755；tests/parser/test_ssa_state_maps.c:756。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_owners_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:69；zr_state_map_owners_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:79；zr_state_map_entry_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503。
 */

/**
 * @fn void test_state_map_materialization_rejects_malformed_owner_operand(void)
 *
 * ## Summary
 * 把 DROP operand 改为不存在的 ValueId=2，检查物化经 OwnerAnalysisBuild 输入验证拒绝该畸形 operand。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * case 为 sourceId=84 的 CLEANUP entry 配 INITIALIZED owner state，并在 function_with_drop_boundary 生成的 DROP operand 池中将 ValueId=1 改为 2；function 仍只有一个 value。
 * 实际断言为 MaterializeState 返回 false、diagnostic.code 为 STATE_MAP_INVALID，以及 target.values 仍为空。
 * function 仅有 ValueId=1，却把 DROP operand 改成 2；OwnerAnalysisBuild 的输入验证先拒绝该越界 operand，ValidateStateMap 立即返回，未到达逐 entry 的 owner-state 比较。本例不为伪造 INITIALIZED owner-state 的后续比较授信。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 首拒绝阶段由当前输入验证及调用顺序静态确定；本提案没有新的运行结果，不将此单一越界输入推广成任意损坏地址的可读性保证。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:30；定义：tests/parser/test_ssa_state_maps.c:759；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1373。
 * fixture 与 operand 修改：tests/parser/test_ssa_state_maps.c:766；tests/parser/test_ssa_state_maps.c:772；tests/parser/test_ssa_state_maps.c:773；断言：tests/parser/test_ssa_state_maps.c:786；tests/parser/test_ssa_state_maps.c:787；tests/parser/test_ssa_state_maps.c:788。
 * 完整调用及首拒绝：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:612；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:267；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:286；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:92；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:143；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:144。
 * 未到达的后续 entry 循环及 owner 检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:621；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503。
 * 正常释放：tests/parser/test_ssa_state_maps.c:789；tests/parser/test_ssa_state_maps.c:790；tests/parser/test_ssa_state_maps.c:791。
 */

/**
 * @fn void test_state_map_materialization_rejects_duplicate_deopt_identity(void)
 *
 * ## Summary
 * 复制同一 deopt id/source 的 state 并把第二项 resumeId 改为 702，检查物化拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：复制deopt state，保留id/source、第二项resume改702；map绑定701。
 * 实际断言：false；STATE_MAP_INVALID；values=NULL。
 * realloc 直接覆盖 function.deoptStates，断言早退不承诺局部回滚；此例不比较完整 target，也不授予 aggregate helper 全实现审查信用。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:31；定义：tests/parser/test_ssa_state_maps.c:794；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1374。
 * 断言：tests/parser/test_ssa_state_maps.c:826；tests/parser/test_ssa_state_maps.c:827；tests/parser/test_ssa_state_maps.c:828。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:801；tests/parser/test_ssa_state_maps.c:807；正常释放：tests/parser/test_ssa_state_maps.c:829；tests/parser/test_ssa_state_maps.c:830；tests/parser/test_ssa_state_maps.c:831。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：ZrCore_ExecIr_ValidateStateMap，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573；zr_exec_ir_deopt_aggregates_validate，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:175；zr_state_map_entry_deopt_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:348。
 */

/**
 * @fn void test_state_map_rejects_borrowed_value_across_suspend(void)
 *
 * ## Summary
 * 构造 BEFORE_EFFECT/SUSPEND 与 BORROWED value 的负例，只断言 false；记录其 entry exceptionState 不一致先拒绝而未覆盖目标 borrowed 分支的 TODO。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * map fixture 用 calloc 清零 entry，再填 BEFORE_EFFECT/SUSPEND 等字段，但没有设置 exceptionState；case 也没有补齐该字段。EXCEPTION_MASK 包含 SUSPEND，因此 entry_valid 内 exceptionState 比较会在后续 value-range 调用前拒绝这一 entry。
 * 实际断言仅检查 MaterializeState 返回 false，不检查 diagnostic.code 或 target。该负例不覆盖 BORROWED_ACROSS_SUSPEND 分支；不能把测试名当作该分支已执行或具体诊断已观察的证据。
 * TODO: tests/parser/test_ssa_state_maps.c:1140：待核对该场景预期的入口状态及目标分支覆盖。完整静态路径为 fixture 清零/填 flags，经物化前置 ValidateStateMap 到 entry 的 exceptionState 检查，尚未到后续 validate_value_range；不改本 case 行为，不把此覆盖缺口称为生产 BUG。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:45；定义：tests/parser/test_ssa_state_maps.c:1140；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1388。
 * fixture 与调用：tests/parser/test_ssa_state_maps.c:55；tests/parser/test_ssa_state_maps.c:59；tests/parser/test_ssa_state_maps.c:78；tests/parser/test_ssa_state_maps.c:1146；唯一拒绝断言：tests/parser/test_ssa_state_maps.c:1176。
 * mask 定义：zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:27；zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:28。
 * 完整验证调用与先拒绝点：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:621；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:486。
 * 未到达的后续 value-range 路径：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:505；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:275；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:286。
 * 正常释放：tests/parser/test_ssa_state_maps.c:1177；tests/parser/test_ssa_state_maps.c:1178；tests/parser/test_ssa_state_maps.c:1179。
 */

/**
 * @fn void test_state_map_materialization_accepts_managed_owner_roots(void)
 *
 * ## Summary
 * 分别用 UNIQUE 和 SHARED ownership 的固定逻辑 value 构造 GC checkpoint，检查物化成功及一个 root 的计数。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：两轮将managed value ownership设UNIQUE、SHARED。
 * 实际断言：两轮均断言成功和rootCount=1。
 * 每轮仅断言 true 和 rootCount=1；未比较 roots[0]、values、ownerStates、数组独立性或全部池。roots 是逻辑 ID，不是实际运行时对象的保活证明。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:51；定义：tests/parser/test_ssa_state_maps.c:1257；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1394。
 * 断言：tests/parser/test_ssa_state_maps.c:1287；tests/parser/test_ssa_state_maps.c:1288。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:1272；tests/parser/test_ssa_state_maps.c:1274；正常释放：tests/parser/test_ssa_state_maps.c:1289；tests/parser/test_ssa_state_maps.c:1290；tests/parser/test_ssa_state_maps.c:1291。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_validate_value_range，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:276；zr_state_map_owners_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:69；ZrCore_ExecIr_MaterializeState，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:779。
 */

/**
 * @fn void test_state_map_materialization_rejects_malformed_function_without_deref(void)
 *
 * ## Summary
 * 释放 function.values 并保留非零 valueCount，再检查物化拒绝且现有 target.values 的指针和首个逻辑 ID 保留。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 本函数为 void case，无 C 返回值。所列断言均通过时正常返回；断言失败由 Unity 记录并跳出当前 case，runner 随后执行 tearDown 并继续后续注册项。最终退出值由 main 的 UNITY_END() 汇总结果决定。
 *
 * ## Constraints and lifecycle
 * 构造场景：free function.values并置NULL；target.values预置独立数组及123。
 * 实际断言：false；STATE_MAP_INVALID；target.values指针和首值123保留。
 * 旧 target.values 是独立 calloc 的单元素数组，手工释放 function.values 后立即置 NULL；只检查 target 指针和首值 123，不检查 count/capacity，不把该 NULL 用例推广成任意地址不会被解引用。
 * map、function 和 target 均为 case 局部记录；正常路径按 MaterializedStateFree、FreeFunction、StateMapFree 的顺序释放各自拥有的池。Unity 断言可 longjmp，跳过当前 case 的剩余局部释放；空 setUp/tearDown 不接管这些局部资源。
 * 本例通过 MaterializeState 间接进入 ValidateStateMap。以下下层路径来自源码静态阅读，不表示本提案运行或追踪过对应分支，也不为跨文件依赖新增完整审查信用。
 *
 * ## References
 * 声明：tests/parser/test_ssa_state_maps.c:52；定义：tests/parser/test_ssa_state_maps.c:1295；RUN_TEST 注册：tests/parser/test_ssa_state_maps.c:1395。
 * 断言：tests/parser/test_ssa_state_maps.c:1324；tests/parser/test_ssa_state_maps.c:1325；tests/parser/test_ssa_state_maps.c:1326；tests/parser/test_ssa_state_maps.c:1327。
 * 构造/依赖调用：tests/parser/test_ssa_state_maps.c:1303；tests/parser/test_ssa_state_maps.c:1305；正常释放：tests/parser/test_ssa_state_maps.c:1328；tests/parser/test_ssa_state_maps.c:1329；tests/parser/test_ssa_state_maps.c:1330。
 * 物化及验证入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556。
 * 相关检查：zr_state_map_function_values_valid，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:179；ZrCore_ExecIr_ValidateStateMap，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:572；ZrCore_ExecIr_MaterializeState，zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685。
 */

/**
 * @fn int main(void)
 *
 * ## Summary
 * 初始化 Unity，按本 TU 中实际的 42 个 RUN_TEST 注册项派发测试，并把汇总失败数返回给进程调用者。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 UNITY_END() 所调用 UnityEnd 的失败 case 数；0 表示该次 runner 未记录失败。函数的静态注册表和本注释不是测试执行结果。
 *
 * ## Constraints and lifecycle
 * 42 个注册项由本 C 的 35 个 case、文本 include 的 verifier .inc 中 3 个 case 和 clone fault header 中 4 个 case 组成。每次 RUN_TEST 把 case 作为 UnityTestFunction 交给 UnityDefaultTestRun；后者在保护段调用 setUp、case 与 tearDown。断言 longjmp 只结束当前 case，runner 记录结果并继续后续注册项。
 * 本 main 不拥有各 case 的局部 map/function/target 池。setUp 和 tearDown 为空，不补偿 case 早退跳过的局部尾部清理。全部依赖、fixture 和未纳入本批的 case 不因同一 TU/include 或本 runner 文档获得新增审查信用。
 *
 * ## References
 * 入口及 Unity 起止：tests/parser/test_ssa_state_maps.c:1353；tests/parser/test_ssa_state_maps.c:1354；tests/parser/test_ssa_state_maps.c:1397。
 * 全部注册：tests/parser/test_ssa_state_maps.c:1355；tests/parser/test_ssa_state_maps.c:1356；tests/parser/test_ssa_state_maps.c:1357；tests/parser/test_ssa_state_maps.c:1358；tests/parser/test_ssa_state_maps.c:1359；tests/parser/test_ssa_state_maps.c:1360；tests/parser/test_ssa_state_maps.c:1361；tests/parser/test_ssa_state_maps.c:1362；tests/parser/test_ssa_state_maps.c:1363；tests/parser/test_ssa_state_maps.c:1364；tests/parser/test_ssa_state_maps.c:1365；tests/parser/test_ssa_state_maps.c:1366；tests/parser/test_ssa_state_maps.c:1367；tests/parser/test_ssa_state_maps.c:1368；tests/parser/test_ssa_state_maps.c:1369；tests/parser/test_ssa_state_maps.c:1370；tests/parser/test_ssa_state_maps.c:1371；tests/parser/test_ssa_state_maps.c:1372；tests/parser/test_ssa_state_maps.c:1373；tests/parser/test_ssa_state_maps.c:1374；tests/parser/test_ssa_state_maps.c:1375；tests/parser/test_ssa_state_maps.c:1376；tests/parser/test_ssa_state_maps.c:1377；tests/parser/test_ssa_state_maps.c:1378；tests/parser/test_ssa_state_maps.c:1379；tests/parser/test_ssa_state_maps.c:1380；tests/parser/test_ssa_state_maps.c:1381；tests/parser/test_ssa_state_maps.c:1382；tests/parser/test_ssa_state_maps.c:1383；tests/parser/test_ssa_state_maps.c:1384；tests/parser/test_ssa_state_maps.c:1385；tests/parser/test_ssa_state_maps.c:1386；tests/parser/test_ssa_state_maps.c:1387；tests/parser/test_ssa_state_maps.c:1388；tests/parser/test_ssa_state_maps.c:1389；tests/parser/test_ssa_state_maps.c:1390；tests/parser/test_ssa_state_maps.c:1391；tests/parser/test_ssa_state_maps.c:1392；tests/parser/test_ssa_state_maps.c:1393；tests/parser/test_ssa_state_maps.c:1394；tests/parser/test_ssa_state_maps.c:1395；tests/parser/test_ssa_state_maps.c:1396。
 * allocator 声明头 include：tests/parser/test_ssa_state_maps.c:10；四个 clone fault case 的定义文本 include：tests/parser/test_ssa_state_maps.c:888；三个 verifier/builder case 的定义文本 include：tests/parser/test_ssa_state_maps.c:1351。
 * 构建/CTest 入口：tests/cmake/ssa-tests.cmake:397；tests/cmake/ssa-tests.cmake:422。
 * 宏与回调：tests/third_party/zr_unity/Unity/src/unity_internals.h:910；tests/third_party/zr_unity/Unity/src/unity.c:2283；tests/third_party/zr_unity/Unity/src/unity.c:2298；tests/third_party/zr_unity/Unity/src/unity.c:2299；tests/third_party/zr_unity/Unity/src/unity.c:2303；失败计数返回：tests/third_party/zr_unity/Unity/src/unity.c:2359。
 */
