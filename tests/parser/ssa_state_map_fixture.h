#ifndef ZR_TEST_SSA_STATE_MAP_FIXTURE_H
#define ZR_TEST_SSA_STATE_MAP_FIXTURE_H

#include "unity.h"

#include <stdlib.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/exec_ir_state_maps.h"

static SZrExecIrFunction function;
static SZrExecIrDiagnostic diagnostic;

void setUp(void) {
    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 99u;
    function.signatureHash = 123u;
    function.contract.generation = 7u;
}

void tearDown(void) {
    ZrCore_ExecIr_FreeFunction(&function);
}

static TZrExecIrValueId value(EZrExecIrOwnership ownership, TZrBool external) {
    TZrExecIrValueId id = external
            ? ZrCore_ExecIr_FunctionAddExternalValue(
                    &function, 1u, ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE)
            : ZrCore_ExecIr_FunctionAddValue(
                    &function, 1u, ownership, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    TEST_ASSERT_NOT_EQUAL(0u, id);
    return id;
}

static void blocks(TZrUInt32 count) {
    TZrUInt32 index;
    for (index = 0u; index < count; ++index) {
        TEST_ASSERT_EQUAL_UINT32(index + 1u, ZrCore_ExecIr_FunctionAddBlock(
                &function, index == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u));
    }
}

static void edges(TZrExecIrBlockId block, TZrExecIrBlockId first,
                  TZrExecIrBlockId second) {
    TZrExecIrBlockId targets[2] = {first, second};
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &function, targets, second == 0u ? 1u : 2u,
            &function.blocks[block - 1u].successorRange));
}

static void predecessors(void) {
    TZrUInt32 target, source, edge;
    for (target = 0u; target < function.blockCount; ++target) {
        SZrExecIrRange range = {0};
        range.start = function.predecessorCount;
        for (source = 0u; source < function.blockCount; ++source) {
            const SZrExecIrBlock *block = &function.blocks[source];
            for (edge = block->successorRange.start;
                 edge < block->successorRange.start + block->successorRange.count;
                 ++edge) {
                if (function.successors[edge] == target + 1u) {
                    TZrExecIrBlockId id = source + 1u;
                    SZrExecIrRange appended;
                    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
                            &function, &id, 1u, &appended));
                    ++range.count;
                }
            }
        }
        function.blocks[target].predecessorRange = range;
    }
}

static TZrExecIrInstructionId emit(TZrExecIrBlockId blockId, EZrExecIrOpcode opcode,
                                   TZrUInt16 flags, TZrExecIrValueId operand,
                                   TZrExecIrValueId result) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id;
    instruction.opcode = opcode;
    instruction.flags = flags;
    instruction.sourceId = 100u + function.instructionCount + 1u;
    if (operand != 0u) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(
                &function, &operand, 1u, &instruction.operandRange));
    }
    if (result != 0u) {
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(
                &function, &result, 1u, &instruction.resultRange));
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(
            &function, &instruction, &id));
    if (blockId != 0u) {
        SZrExecIrBlock *block = &function.blocks[blockId - 1u];
        if (block->instructionRange.count == 0u) {
            block->instructionRange.start = id - 1u;
        }
        ++block->instructionRange.count;
    }
    return id;
}

static const SZrExecIrStateMapEntry *checkpoint(TZrExecIrInstructionId instruction,
                                                EZrExecIrStateMapPhase phase) {
    TZrUInt32 index;
    const SZrExecIrStateMapEntry *found = NULL;
    TEST_ASSERT_NOT_NULL(function.stateMap);
    for (index = 0u; index < function.stateMap->entryCount; ++index) {
        const SZrExecIrStateMapEntry *entry = &function.stateMap->entries[index];
        if (entry->instructionId == instruction && entry->phase == phase) {
            found = entry;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(found, "missing checkpoint");
    return found;
}

static void assert_only_root(TZrExecIrInstructionId instruction,
                             EZrExecIrStateMapPhase phase, TZrExecIrValueId root) {
    const SZrExecIrStateMapEntry *entry = checkpoint(instruction, phase);
    SZrExecIrMaterializedState target;
    SZrExecIrResumeRequest request = {0};
    TEST_ASSERT_EQUAL_UINT32(root == 0u ? 0u : 1u, entry->rootValues.count);
    if (root != 0u) {
        TEST_ASSERT_EQUAL_UINT32(root,
                function.stateMap->rootPool[entry->rootValues.start]);
    }
    ZrCore_ExecIr_MaterializedStateInit(&target);
    request.function = &function;
    request.map = function.stateMap;
    request.functionToken = function.functionToken;
    request.generation = function.contract.generation;
    request.signatureHash = function.signatureHash;
    request.sourceId = entry->sourceId;
    request.resumeId = entry->resumeId;
    request.phase = phase;
    request.target = &target;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_MaterializeState(&request, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(entry->rootValues.count, target.rootCount);
    if (root != 0u) {
        TEST_ASSERT_EQUAL_UINT32(root, target.roots[0]);
    }
    ZrCore_ExecIr_MaterializedStateFree(&target);
}

static void build(void) {
    predecessors();
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildStateMaps(&function, &diagnostic));
}

#endif
