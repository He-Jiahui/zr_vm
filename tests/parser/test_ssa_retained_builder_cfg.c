#include "support/ssa_retained_builder_cfg_fixture.h"
#include "zr_vm_core/exec_ir_state_map.h"

#include <stdio.h>
#include <string.h>

typedef struct SCaseObservation {
    TZrUInt32 failedLine;
    TZrUInt64 expected;
    TZrUInt64 actual;
    SZrRetainedCfgProduction production;
    SZrExecIrDiagnostic diagnostic;
} SCaseObservation;

static TZrBool expect_number(SCaseObservation *observation, TZrUInt64 actual,
                            TZrUInt64 expected, TZrUInt32 line) {
    if (actual == expected) return ZR_TRUE;
    if (observation->failedLine == 0u) {
        observation->failedLine = line;
        observation->actual = actual;
        observation->expected = expected;
    }
    return ZR_FALSE;
}

/* Checking helpers own no resources. Case functions always reach cleanup. */
#define EXPECT_EQ(actual, expected) \
    do { if (!expect_number(o, (TZrUInt64)(actual), (TZrUInt64)(expected), \
                           (TZrUInt32)__LINE__)) return ZR_FALSE; } while (0)
#define REQUIRE_EQ(actual, expected) \
    do { if (!expect_number(o, (TZrUInt64)(actual), (TZrUInt64)(expected), \
                           (TZrUInt32)__LINE__)) goto cleanup; } while (0)

static TZrBool diagnostic_equal(SCaseObservation *o,
                               const SZrExecIrDiagnostic *actual,
                               const SZrExecIrDiagnostic *expected) {
    EXPECT_EQ(actual->code, expected->code);
    EXPECT_EQ(actual->functionToken, expected->functionToken);
    EXPECT_EQ(actual->blockId, expected->blockId);
    EXPECT_EQ(actual->instructionId, expected->instructionId);
    EXPECT_EQ(actual->sourceId, expected->sourceId);
    EXPECT_EQ(actual->expectedVersion, expected->expectedVersion);
    EXPECT_EQ(actual->actualVersion, expected->actualVersion);
    EXPECT_EQ(actual->expectedHash, expected->expectedHash);
    EXPECT_EQ(actual->actualHash, expected->actualHash);
    return ZR_TRUE;
}

static TZrBool valid_retained(SCaseObservation *o, const SZrExecIrModule *module) {
    SZrExecIrDiagnostic zero = {0};
    TZrUInt32 index;
    EXPECT_EQ(module != ZR_NULL, ZR_TRUE);
    memset(&o->diagnostic, 0xa5, sizeof(o->diagnostic));
    EXPECT_EQ(ZrCore_ExecIr_VerifyModule(module, &o->diagnostic), ZR_TRUE);
    EXPECT_EQ(diagnostic_equal(o, &o->diagnostic, &zero), ZR_TRUE);
    for (index = 0u; index < module->functionCount; ++index) {
        const SZrExecIrFunction *function = &module->functions[index];
        EXPECT_EQ(function->stateMap != ZR_NULL, ZR_TRUE);
        memset(&o->diagnostic, 0xa5, sizeof(o->diagnostic));
        EXPECT_EQ(ZrCore_ExecIr_ValidateStateMap(
                          function, function->stateMap, &o->diagnostic), ZR_TRUE);
        EXPECT_EQ(diagnostic_equal(o, &o->diagnostic, &zero), ZR_TRUE);
    }
    return ZR_TRUE;
}

static TZrBool incoming_pair(SCaseObservation *o, const SZrExecIrFunction *f,
                            SZrExecIrRange range, TZrUInt32 firstBlock,
                            TZrUInt32 firstValue, TZrUInt32 secondBlock,
                            TZrUInt32 secondValue) {
    EXPECT_EQ(range.count, 2u);
    EXPECT_EQ(range.start <= f->phiIncomingCount, ZR_TRUE);
    EXPECT_EQ(range.count <= f->phiIncomingCount - range.start, ZR_TRUE);
    EXPECT_EQ(f->phiIncoming[range.start].predecessor, firstBlock);
    EXPECT_EQ(f->phiIncoming[range.start].value, firstValue);
    EXPECT_EQ(f->phiIncoming[range.start + 1u].predecessor, secondBlock);
    EXPECT_EQ(f->phiIncoming[range.start + 1u].value, secondValue);
    return ZR_TRUE;
}

static TZrBool state_entries(SCaseObservation *o, const SZrExecIrFunction *f,
                            TZrBool loop, TZrBool parameter) {
    const SZrExecIrStateMap *map = f->stateMap;
    TZrUInt32 checkpoint;
    EXPECT_EQ(map != ZR_NULL, ZR_TRUE);
    EXPECT_EQ(map->functionToken, f->functionToken);
    EXPECT_EQ(map->signatureHash, f->signatureHash);
    EXPECT_EQ(map->generation, 1u);
    EXPECT_EQ(map->entryCount, parameter ? 6u : 0u);
    EXPECT_EQ(map->rootCount, 0u);
    EXPECT_EQ(map->valueCount, parameter ? (loop ? 14u : 8u) : 0u);
    EXPECT_EQ(map->ownerStateCount, map->valueCount);
    for (checkpoint = 0u; parameter && checkpoint < 2u; ++checkpoint) {
        TZrUInt32 phase;
        TZrUInt32 site = loop ? (checkpoint == 0u ? 3u : 9u)
                             : (checkpoint == 0u ? 4u : 7u);
        TZrUInt32 scalar = loop ? (checkpoint == 0u ? 1u : 4u)
                               : (checkpoint == 0u ? 2u : 3u);
        for (phase = 0u; phase < ZR_EXEC_IR_STATE_PHASE_COUNT; ++phase) {
            const SZrExecIrStateMapEntry *entry = ZrCore_ExecIr_StateMapFind(
                    map, site, checkpoint + 1u, (EZrExecIrStateMapPhase)phase);
            TZrUInt32 offset = 0u;
            EXPECT_EQ(entry != ZR_NULL, ZR_TRUE);
            EXPECT_EQ(entry == &map->entries[checkpoint * 3u + phase], ZR_TRUE);
            EXPECT_EQ(entry->sourceId, site);
            EXPECT_EQ(entry->instructionId, site);
            EXPECT_EQ(entry->resumeId, checkpoint + 1u);
            EXPECT_EQ(entry->phase, phase);
            EXPECT_EQ(entry->cleanupState, phase);
            EXPECT_EQ(entry->boundaryFlags, ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW);
            EXPECT_EQ(entry->exceptionState, ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW);
            EXPECT_EQ(entry->handlerBlockId, 0u);
            EXPECT_EQ(entry->deoptId, 0u);
            EXPECT_EQ(entry->rootValues.count, 0u);
            EXPECT_EQ(entry->liveValues.count, (loop ? 2u : 1u) + (phase == 0u));
            EXPECT_EQ(entry->ownerStates.count, entry->liveValues.count);
            EXPECT_EQ(entry->effectIn, f->instructions[site - 1u].effectIn);
            EXPECT_EQ(entry->effectOut, f->instructions[site - 1u].effectOut);
            if (phase == 0u) {
                EXPECT_EQ(map->valuePool[entry->liveValues.start + offset], scalar);
                ++offset;
            }
            if (loop) {
                EXPECT_EQ(map->valuePool[entry->liveValues.start + offset], 6u);
                ++offset;
            }
            EXPECT_EQ(map->valuePool[entry->liveValues.start + offset], loop ? 7u : 5u);
            for (offset = 0u; offset < entry->ownerStates.count; ++offset)
                EXPECT_EQ(map->ownerStatePool[entry->ownerStates.start + offset],
                          ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN);
        }
    }
    return ZR_TRUE;
}

static TZrBool graph_facts(SCaseObservation *o, const SZrExecIrModule *module,
                          TZrUInt32 functionIndex, const SZrRetainedCfgSpec *spec) {
    const TZrBool loop = (TZrBool)(spec->graph == ZR_RETAINED_CFG_PROMOTED_LOOP ||
                                   spec->graph == ZR_RETAINED_CFG_PARAMETER_LOOP);
    const TZrBool parameter = (TZrBool)(spec->graph >= ZR_RETAINED_CFG_PARAMETER_DIAMOND);
    const SZrExecIrFunction *f = &module->functions[functionIndex];
    const TZrUInt32 startsDiamond[4] = {0u, 2u, 5u, 8u};
    const TZrUInt32 countsDiamond[4] = {2u, 3u, 3u, 2u};
    const TZrUInt32 startsLoop[4] = {0u, 4u, 6u, 10u};
    const TZrUInt32 countsLoop[4] = {4u, 2u, 4u, 2u};
    const TZrUInt32 predecessorsDiamond[4] = {1u, 1u, 2u, 3u};
    const TZrUInt32 successorsDiamond[4] = {2u, 3u, 4u, 4u};
    const TZrUInt32 predecessorsLoop[4] = {1u, 3u, 2u, 2u};
    const TZrUInt32 successorsLoop[4] = {2u, 3u, 4u, 2u};
    const TZrUInt32 *starts = loop ? startsLoop : startsDiamond;
    const TZrUInt32 *counts = loop ? countsLoop : countsDiamond;
    const SZrExecIrBlock *join = &f->blocks[loop ? 1u : 3u];
    const TZrUInt32 firstStore = loop ? 3u : 4u;
    const TZrUInt32 secondStore = loop ? 9u : 7u;
    const TZrUInt32 firstLoad = loop ? 5u : 9u;
    TZrUInt32 index;
    TZrUInt32 region;
    EXPECT_EQ(f->id, functionIndex + 1u);
    EXPECT_EQ(f->functionToken, spec->token);
    EXPECT_EQ(f->signatureHash, spec->signature);
    EXPECT_EQ(f->contract.targetToken, spec->token);
    EXPECT_EQ(f->contract.signatureHash, spec->signature);
    EXPECT_EQ(f->contract.generation, 1u);
    EXPECT_EQ(f->contract.schemaVersion, ZR_EXECUTION_CONTRACT_SCHEMA_VERSION);
    EXPECT_EQ(f->contract.abiVersion, ZR_EXECUTION_CONTRACT_ABI_VERSION);
    EXPECT_EQ(f->contract.logicalVersion, ZR_EXECUTION_CONTRACT_LOGICAL_VERSION);
    EXPECT_EQ(f->entryBlockId, 1u);
    EXPECT_EQ(f->blockCount, 4u);
    EXPECT_EQ(f->instructionCount, loop ? 12u : 10u);
    EXPECT_EQ(f->sourceMapCount, f->instructionCount);
    EXPECT_EQ(f->valueCount, loop ? (parameter ? 8u : 9u) : (parameter ? 6u : 7u));
    EXPECT_EQ(f->predecessorCount, 4u);
    EXPECT_EQ(f->successorCount, 4u);
    EXPECT_EQ(f->values[(loop ? 6u : 1u) - 1u].typeToken, ZR_RETAINED_CFG_BOOL_TYPE);
    EXPECT_EQ(f->values[(loop ? 6u : 1u) - 1u].flags, ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY);
    EXPECT_EQ(f->values[(loop ? 6u : 1u) - 1u].definitionInstructionId, 0u);
    EXPECT_EQ(f->values[0].id, 1u);
    EXPECT_EQ(f->values[loop ? 6u : 4u].flags,
              ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
              (parameter ? 0u : ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE));
    for (index = 0u; index < 4u; ++index) {
        const SZrExecIrBlock *block = &f->blocks[index];
        EXPECT_EQ(block->id, index + 1u);
        EXPECT_EQ(block->instructionRange.start, starts[index]);
        EXPECT_EQ(block->instructionRange.count, counts[index]);
        EXPECT_EQ(block->terminatorInstructionId, starts[index] + counts[index]);
        EXPECT_EQ(block->immediateDominator, loop && index >= 2u ? 2u : 1u);
        EXPECT_EQ(block->predecessorRange.count,
                  loop ? (index == 0u ? 0u : (index == 1u ? 2u : 1u))
                       : (index == 0u ? 0u : (index == 3u ? 2u : 1u)));
        EXPECT_EQ(block->successorRange.count,
                  index == 3u ? 0u : (index == (loop ? 1u : 0u) ? 2u : 1u));
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
            if (!parameter || block != join || region != ZR_EXEC_IR_MEMORY_MANAGED_HEAP) {
                EXPECT_EQ(block->memoryPhiResults[region], 0u);
                EXPECT_EQ(block->memoryPhiIncomings[region].count, 0u);
            }
        }
        if (!parameter || block != join) {
            EXPECT_EQ(block->effectPhiResult, 0u);
            EXPECT_EQ(block->effectPhiIncomings.count, 0u);
        }
        EXPECT_EQ(block->phis.count, !parameter && block == join ? 1u : 0u);
        EXPECT_EQ(f->predecessors[index], loop ? predecessorsLoop[index] : predecessorsDiamond[index]);
        EXPECT_EQ(f->successors[index], loop ? successorsLoop[index] : successorsDiamond[index]);
    }
    for (index = 0u; index < f->instructionCount; ++index) {
        EXPECT_EQ(f->instructions[index].sourceId, index + 1u);
        EXPECT_EQ(f->sourceMaps[index].sourceId, index + 1u);
        EXPECT_EQ(f->sourceMaps[index].instructionId, index + 1u);
        EXPECT_EQ(f->sourceMaps[index].startOffset, index * 4u);
        EXPECT_EQ(f->sourceMaps[index].endOffset, index * 4u + 3u);
        EXPECT_EQ(f->sourceMaps[index].startLine, index + 1u);
        EXPECT_EQ(f->sourceMaps[index].endLine, index + 1u);
        EXPECT_EQ(f->sourceMaps[index].startColumn, 1u);
        EXPECT_EQ(f->sourceMaps[index].endColumn, 4u);
    }
    EXPECT_EQ(f->instructions[loop ? 5u : 1u].opcode, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    EXPECT_EQ(f->instructions[loop ? 5u : 1u].operands.count, 1u);
    EXPECT_EQ(f->operandPool[f->instructions[loop ? 5u : 1u].operands.start], loop ? 6u : 1u);
    for (index = 0u; index < 2u; ++index) {
        const SZrExecIrInstruction *constant = &f->instructions[loop ? (index == 0u ? 1u : 6u)
                                                                 : (index == 0u ? 2u : 5u)];
        TZrUInt32 poolIndex = functionIndex * 2u + index;
        EXPECT_EQ(constant->opcode, ZR_EXEC_IR_OPCODE_CONSTANT);
        EXPECT_EQ(constant->results.count, 1u);
        EXPECT_EQ(f->resultPool[constant->results.start], loop ? (index == 0u ? 1u : 3u) : index + 2u);
        EXPECT_EQ(constant->layoutId, poolIndex);
        EXPECT_EQ(module->constants[poolIndex].typeToken, ZR_RETAINED_CFG_I64_TYPE);
        EXPECT_EQ(module->constants[poolIndex].flags, 0u);
        EXPECT_EQ(module->constants[poolIndex].bits, loop ? index : (index == 0u ? 10u : 20u));
    }
    EXPECT_EQ(f->phiCount, parameter ? 0u : 1u);
    EXPECT_EQ(f->phiIncomingCount, parameter ? 4u : 2u);
    EXPECT_EQ(f->memoryTokenCount, parameter ? (loop ? 4u : 3u) : 0u);
    EXPECT_EQ(f->instructions[firstStore - 1u].opcode, parameter ? ZR_EXEC_IR_OPCODE_STORE : ZR_EXEC_IR_OPCODE_NOP);
    EXPECT_EQ(f->instructions[secondStore - 1u].opcode, parameter ? ZR_EXEC_IR_OPCODE_STORE : ZR_EXEC_IR_OPCODE_NOP);
    EXPECT_EQ(f->instructions[firstLoad - 1u].opcode, parameter ? ZR_EXEC_IR_OPCODE_LOAD : ZR_EXEC_IR_OPCODE_COPY);
    EXPECT_EQ(f->instructions[f->instructionCount - 1u].opcode, ZR_EXEC_IR_OPCODE_RETURN);
    EXPECT_EQ(f->instructions[f->instructionCount - 1u].operands.count, 1u);
    EXPECT_EQ(f->operandPool[f->instructions[f->instructionCount - 1u].operands.start], loop ? 5u : 4u);
    if (loop) {
        EXPECT_EQ(f->instructions[7].opcode, ZR_EXEC_IR_OPCODE_ADD);
        EXPECT_EQ(f->instructions[7].operands.count, 2u);
        EXPECT_EQ(f->instructions[7].results.count, 1u);
        EXPECT_EQ(f->operandPool[f->instructions[7].operands.start], 2u);
        EXPECT_EQ(f->operandPool[f->instructions[7].operands.start + 1u], 3u);
        EXPECT_EQ(f->resultPool[f->instructions[7].results.start], 4u);
    }
    if (!parameter) {
        const SZrExecIrPhi *phi = &f->phiPool[join->phis.start];
        EXPECT_EQ(phi->result, loop ? 9u : 7u);
        EXPECT_EQ(incoming_pair(o, f, phi->incomings, loop ? 1u : 2u,
                               loop ? 1u : 2u, 3u, loop ? 4u : 3u), ZR_TRUE);
        EXPECT_EQ(f->operandPool[f->instructions[firstLoad - 1u].operands.start], phi->result);
        if (loop) {
            EXPECT_EQ(f->instructions[10].opcode, ZR_EXEC_IR_OPCODE_COPY);
            EXPECT_EQ(f->operandPool[f->instructions[10].operands.start], phi->result);
        }
        for (index = 0u; index < f->instructionCount; ++index) {
            EXPECT_EQ(f->instructions[index].memoryIn.count, 0u);
            EXPECT_EQ(f->instructions[index].memoryOut.count, 0u);
            EXPECT_EQ(f->instructions[index].effectIn, 0u);
            EXPECT_EQ(f->instructions[index].effectOut, 0u);
        }
    } else {
        const SZrExecIrInstruction *first = &f->instructions[firstStore - 1u];
        const SZrExecIrInstruction *second = &f->instructions[secondStore - 1u];
        TZrUInt32 firstMemory;
        TZrUInt32 secondMemory;
        TZrUInt32 joined = join->memoryPhiResults[ZR_EXEC_IR_MEMORY_MANAGED_HEAP];
        EXPECT_EQ(first->memoryOut.count, 1u);
        EXPECT_EQ(second->memoryOut.count, 1u);
        firstMemory = f->memoryTokenPool[first->memoryOut.start];
        secondMemory = f->memoryTokenPool[second->memoryOut.start];
        EXPECT_EQ(ZR_EXEC_IR_MEMORY_TOKEN_REGION(firstMemory), ZR_EXEC_IR_MEMORY_MANAGED_HEAP);
        EXPECT_EQ(ZR_EXEC_IR_MEMORY_TOKEN_REGION(secondMemory), ZR_EXEC_IR_MEMORY_MANAGED_HEAP);
        EXPECT_EQ(ZR_EXEC_IR_MEMORY_TOKEN_REGION(joined), ZR_EXEC_IR_MEMORY_MANAGED_HEAP);
        EXPECT_EQ(firstMemory != secondMemory && joined != firstMemory && joined != secondMemory, ZR_TRUE);
        EXPECT_EQ(first->effectOut != second->effectOut && first->effectOut != join->effectPhiResult &&
                  second->effectOut != join->effectPhiResult && join->effectPhiResult != 0u, ZR_TRUE);
        EXPECT_EQ(first->effectIn, 1u);
        EXPECT_EQ(second->effectIn, loop ? join->effectPhiResult : 1u);
        EXPECT_EQ(incoming_pair(o, f, join->effectPhiIncomings, loop ? 1u : 2u,
                               first->effectOut, 3u, second->effectOut), ZR_TRUE);
        EXPECT_EQ(incoming_pair(o, f, join->memoryPhiIncomings[ZR_EXEC_IR_MEMORY_MANAGED_HEAP],
                               loop ? 1u : 2u, firstMemory, 3u, secondMemory), ZR_TRUE);
        EXPECT_EQ(f->instructions[firstLoad - 1u].memoryIn.count, 1u);
        EXPECT_EQ(f->memoryTokenPool[f->instructions[firstLoad - 1u].memoryIn.start], joined);
        if (loop) {
            EXPECT_EQ(f->instructions[10].opcode, ZR_EXEC_IR_OPCODE_LOAD);
            EXPECT_EQ(f->instructions[10].memoryIn.count, 1u);
            EXPECT_EQ(f->memoryTokenPool[f->instructions[10].memoryIn.start], joined);
        }
    }
    return state_entries(o, f, loop, parameter);
}

static TZrBool production_released(SCaseObservation *o, TZrUInt32 count) {
    SZrExecIrDiagnostic zero = {0};
    EXPECT_EQ(o->production.stage, 0u);
    EXPECT_EQ(o->production.producerAllocated, count);
    EXPECT_EQ(o->production.producerReleased, count);
    EXPECT_EQ(o->production.functionsBuilt, count);
    EXPECT_EQ(o->production.transientReleased, 1u);
    EXPECT_EQ(o->production.ownerPublished, 1u);
    EXPECT_EQ(diagnostic_equal(o, &o->production.diagnostic, &zero), ZR_TRUE);
    return ZR_TRUE;
}

static void run_case(TZrUInt32 caseIndex, SCaseObservation *o) {
    SZrRetainedCfgSpec specs[2] = {{ZR_RETAINED_CFG_PROMOTED_DIAMOND, 7001u, 0x1234567887654321ull},
                                 {ZR_RETAINED_CFG_PARAMETER_LOOP, 7002u, 0x2234567887654321ull}};
    SZrExecIrOwnedModule *owner = ZR_NULL;
    const SZrExecIrModule *view = ZR_NULL;
    SZrExecIrModule clone;
    SZrExecIrDiagnostic expected = {0};
    TZrUInt32 count = caseIndex == 4u ? 2u : 1u;
    TZrUInt32 index;
    TZrUInt32 saved = 0u;
    TZrUInt32 *mutation = ZR_NULL;
    TZrBool produced;
    ZrCore_ExecIr_ModuleInit(&clone);
    if (caseIndex < 4u) specs[0].graph = (EZrRetainedCfgGraph)caseIndex;
    if (caseIndex == 6u) specs[0].graph = ZR_RETAINED_CFG_PARAMETER_LOOP;
    if (caseIndex == 7u) specs[0].graph = ZR_RETAINED_CFG_PARAMETER_DIAMOND;
    produced = ZrTest_RetainedCfgProduce(specs, count, &owner, &o->production);
    o->diagnostic = o->production.diagnostic;
    REQUIRE_EQ(produced, ZR_TRUE);
    REQUIRE_EQ(production_released(o, count), ZR_TRUE);
    /* Producer helper has returned. No validation borrows its storage. */
    view = ZrCore_ExecIr_OwnedModuleView(owner);
    REQUIRE_EQ(valid_retained(o, view), ZR_TRUE);
    REQUIRE_EQ(view->functionCount, count);
    REQUIRE_EQ(view->constantCount, count * 2u);
    for (index = 0u; index < count; ++index)
        REQUIRE_EQ(graph_facts(o, view, index, &specs[index]), ZR_TRUE);
    if (caseIndex < 5u) goto cleanup;
    REQUIRE_EQ(ZrCore_ExecIr_CloneModule(view, &clone, &o->diagnostic), ZR_TRUE);
    REQUIRE_EQ(valid_retained(o, &clone), ZR_TRUE);
    expected.functionToken = specs[0].token;
    if (caseIndex == 5u) {
        const SZrExecIrPhi *phi = &clone.functions[0].phiPool[clone.functions[0].blocks[3].phis.start];
        mutation = &clone.functions[0].phiIncoming[phi->incomings.start].predecessor;
        expected.code = ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH;
        expected.blockId = 4u; expected.instructionId = 10u; expected.sourceId = 10u;
        expected.expectedVersion = 2u; expected.actualVersion = 3u;
        saved = *mutation; *mutation = 3u;
    } else if (caseIndex == 6u) {
        SZrExecIrFunction *f = &clone.functions[0];
        mutation = &f->memoryTokenPool[f->instructions[4].memoryIn.start];
        expected.code = ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN;
        expected.blockId = 2u; expected.instructionId = 5u; expected.sourceId = 5u;
        expected.expectedVersion = f->blocks[1].memoryPhiResults[ZR_EXEC_IR_MEMORY_MANAGED_HEAP];
        expected.actualVersion = f->memoryTokenPool[f->instructions[2].memoryOut.start];
        saved = *mutation; *mutation = expected.actualVersion;
    } else {
        mutation = &clone.functions[0].stateMap->entries[0].sourceId;
        expected.code = ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
        expected.blockId = 2u; expected.instructionId = 4u; expected.sourceId = 999u;
        expected.expectedVersion = 4u; expected.actualVersion = 999u;
        expected.expectedHash = 4u; expected.actualHash = 999u;
        saved = *mutation; *mutation = 999u;
    }
    memset(&o->diagnostic, 0xa5, sizeof(o->diagnostic));
    REQUIRE_EQ(ZrCore_ExecIr_VerifyModule(&clone, &o->diagnostic), ZR_FALSE);
    REQUIRE_EQ(diagnostic_equal(o, &o->diagnostic, &expected), ZR_TRUE);
    *mutation = saved;
    mutation = ZR_NULL;
    REQUIRE_EQ(valid_retained(o, &clone), ZR_TRUE);
    REQUIRE_EQ(graph_facts(o, &clone, 0u, &specs[0]), ZR_TRUE);
    REQUIRE_EQ(valid_retained(o, view), ZR_TRUE);
    REQUIRE_EQ(graph_facts(o, view, 0u, &specs[0]), ZR_TRUE);
cleanup:
    /* Mutation points only into this registered clone; restore before release. */
    if (mutation != ZR_NULL) *mutation = saved;
    ZrCore_ExecIr_FreeModule(&clone);
    ZrCore_ExecIr_OwnedModuleFree(&owner);
    (void)expect_number(o, owner == ZR_NULL, ZR_TRUE, (TZrUInt32)__LINE__);
}

int main(void) {
    const char *names[ZR_RETAINED_CFG_CASE_COUNT] = {
        "promoted_diamond", "promoted_loop", "parameter_diamond", "parameter_loop",
        "two_functions", "phi_predecessor_refusal", "stale_memory_refusal", "map_source_refusal"
    };
    TZrUInt32 index;
    TZrUInt32 failed = 0u;
    for (index = 0u; index < ZR_RETAINED_CFG_CASE_COUNT; ++index) {
        SCaseObservation observation = {0};
        run_case(index, &observation);
        if (observation.failedLine != 0u) {
            ++failed;
            printf("FAIL %u %s line=%u expected=%llu actual=%llu stage=%u allocated=%u released=%u built=%u transient=%u owner=%u diag=%u,%u,%u,%u,%u,%u,%u,%llu,%llu\n",
                   index + 1u, names[index], observation.failedLine,
                   (unsigned long long)observation.expected, (unsigned long long)observation.actual,
                   observation.production.stage, observation.production.producerAllocated,
                   observation.production.producerReleased, observation.production.functionsBuilt,
                   observation.production.transientReleased, observation.production.ownerPublished,
                   (unsigned)observation.diagnostic.code, observation.diagnostic.functionToken,
                   observation.diagnostic.blockId, observation.diagnostic.instructionId,
                   observation.diagnostic.sourceId, observation.diagnostic.expectedVersion,
                   observation.diagnostic.actualVersion, (unsigned long long)observation.diagnostic.expectedHash,
                   (unsigned long long)observation.diagnostic.actualHash);
        } else printf("PASS %u %s\n", index + 1u, names[index]);
    }
    printf("ssa_retained_builder_cfg cases=%u failed=%u\n", ZR_RETAINED_CFG_CASE_COUNT, failed);
    return failed == 0u ? 0 : 1;
}
