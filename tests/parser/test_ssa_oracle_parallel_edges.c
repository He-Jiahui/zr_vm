#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static TZrBool copy_slot(void *userData, TZrUInt32 destinationSlot,
                         TZrUInt32 sourceSlot) {
    TZrUInt32 *slots = (TZrUInt32 *)userData;
    slots[destinationSlot] = slots[sourceSlot];
    return ZR_TRUE;
}

static TZrBool reject_slot(void *userData, TZrUInt32 destinationSlot,
                           TZrUInt32 sourceSlot) {
    (void)userData;
    (void)destinationSlot;
    (void)sourceSlot;
    return ZR_FALSE;
}

static TZrExecIrValueId add_value(SZrExecIrFunction *function) {
    TZrExecIrValueId id = ZrCore_ExecIr_FunctionAddValue(
        function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
        ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(id != ZR_EXEC_IR_VALUE_ID_INVALID, "could not create oracle value");
    return id;
}

static TZrExecIrInstructionId append_instruction(SZrExecIrFunction *function,
                                                   EZrExecIrOpcode opcode,
                                                   SZrExecIrRange operands,
                                                   SZrExecIrRange results,
                                                   SZrExecIrRange successors,
                                                   TZrUInt32 literal) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.successorRange = successors;
    instruction.layoutId = literal;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id),
          "could not append oracle instruction");
    return id;
}

static void build_parallel_phi(SZrExecIrFunction *function) {
    SZrExecIrRange empty = {.start = 0u, .count = 0u};
    SZrExecIrRange results, conditionOperand, returnOperand, incomingRange;
    SZrExecIrPhiIncoming incomings[2] = {{1u, 1u}, {1u, 2u}};
    SZrExecIrPhi phi;
    TZrExecIrBlockId target = 2u, predecessors[2] = {1u, 1u};
    TZrExecIrBlockId successors[2] = {2u, 2u};
    TZrExecIrValueId left, right, condition, merged;

    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 91u;
    left = add_value(function);
    right = add_value(function);
    condition = add_value(function);
    merged = add_value(function);
    check(left == 1u && right == 2u && condition == 3u && merged == 4u,
          "unexpected oracle value IDs");
    check(ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
              ZrCore_ExecIr_FunctionAddBlock(function, 0u) == target,
          "could not build parallel-edge blocks");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                                                 &function->blocks[0].successorRange) &&
              ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, 2u,
                                                        &function->blocks[1].predecessorRange),
          "could not append both CFG edges");
    check(ZrCore_ExecIr_FunctionAppendResults(function, &left, 1u, &results),
          "could not create first definition");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, empty, results,
                             empty, 11u) == 1u, "wrong first instruction ID");
    check(ZrCore_ExecIr_FunctionAppendResults(function, &right, 1u, &results),
          "could not create second definition");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, empty, results,
                             empty, 22u) == 2u, "wrong second instruction ID");
    check(ZrCore_ExecIr_FunctionAppendResults(function, &condition, 1u, &results),
          "could not create branch condition");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, empty, results,
                             empty, 0u) == 3u, "wrong condition instruction ID");
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u,
                                               &conditionOperand),
          "could not create condition operand");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                             conditionOperand, empty, function->blocks[0].successorRange,
                             0u) == 4u, "wrong branch instruction ID");
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &merged, 1u,
                                               &returnOperand),
          "could not create return operand");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperand,
                             empty, empty, 0u) == 5u, "wrong return instruction ID");
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = 4u;
    function->blocks[0].terminatorInstructionId = 4u;
    function->blocks[1].instructionRange.start = 4u;
    function->blocks[1].instructionRange.count = 1u;
    function->blocks[1].terminatorInstructionId = 5u;
    check(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incomings, 2u,
                                                   &incomingRange),
          "could not create parallel phi incoming slots");
    phi.result = merged;
    phi.incomings = incomingRange;
    check(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u,
                                          &function->blocks[1].phis),
          "could not create parallel phi");
}

static void test_branch_and_switch_parallel_edges_select_distinct_incomings(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrOracleInput input;
    SZrExecIrOracleValue condition;
    TZrUInt32 caseIndex, opcodeIndex;

    build_parallel_phi(&function);
    TZrBool verified = ZrCore_ExecIr_VerifyFunction(
        &function, ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA,
        &diagnostic);
    if (!verified)
        fprintf(stderr, "verifier diagnostic code=%u block=%u instruction=%u expected=%u actual=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.expectedVersion, (unsigned)diagnostic.actualVersion);
    check(verified,
          "core verifier rejected a valid parallel-edge phi");
    memset(&input, 0, sizeof(input));
    input.function = &function;
    input.constants = &condition;
    input.constantCount = 1u;
    for (opcodeIndex = 0u; opcodeIndex != 2u; ++opcodeIndex) {
        function.instructions[3].opcode = opcodeIndex == 0u
            ? ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH : ZR_EXEC_IR_OPCODE_SWITCH;
        for (caseIndex = 0u; caseIndex != 2u; ++caseIndex) {
            condition.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            condition.as.boolean = (TZrBool)(caseIndex == 0u);
            ZrCore_ExecIr_OracleResultInit(&execution);
            TZrBool interpreted = ZrCore_ExecIr_RunOracleEx(
                &input, &execution, &diagnostic);
            if (!interpreted)
                fprintf(stderr, "oracle diagnostic code=%u block=%u instruction=%u expected=%u actual=%u\n",
                        (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                        (unsigned)diagnostic.instructionId,
                        (unsigned)diagnostic.expectedVersion, (unsigned)diagnostic.actualVersion);
            check(interpreted,
                  "oracle rejected parallel CFG edges with ordered phi incoming slots");
            check(execution.returned &&
                      execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                      execution.returnValue.as.signedInteger ==
                          (opcodeIndex == 0u
                               ? (caseIndex == 0u ? 11 : 22)
                               : (caseIndex == 0u ? 22 : 11)),
                  "oracle merged the two parallel edges into one phi input");
            ZrCore_ExecIr_OracleResultFree(&execution);
        }
    }
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_selected_edge_missing_from_source_adjacency(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrOracleInput input;
    SZrExecIrOracleValue condition;

    build_parallel_phi(&function);
    /* The instruction still selects its second successor, but the source
     * block advertises only its first adjacency occurrence. */
    function.blocks[0].successorRange.count = 1u;
    memset(&input, 0, sizeof(input));
    input.function = &function;
    condition.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    condition.as.boolean = ZR_FALSE;
    input.constants = &condition;
    input.constantCount = 1u;
    ZrCore_ExecIr_OracleResultInit(&execution);
    check(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH &&
              diagnostic.blockId == 2u && diagnostic.instructionId == 4u &&
              execution.values == NULL,
          "oracle accepted a phi edge omitted by source adjacency");
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_projections_preserve_parallel_phi_edges(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcProjection bytecode = {0};
    SZrAotIrProjection aot = {0};
    SZrExecBcExecutionResult execution;
    SZrExecIrRange instructionSuccessors;
    TZrExecIrBlockId parallelSuccessors[2] = {2u, 2u};

    build_parallel_phi(&function);
    check(ZrCore_ExecIr_FunctionAppendSuccessors(&function,
                                                  parallelSuccessors, 2u,
                                                  &instructionSuccessors),
          "could not append separate terminator successor range");
    function.instructions[3].successorRange = instructionSuccessors;
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic),
          "ExecBC projection rejected a valid parallel-edge phi");
    check(bytecode.syntheticBlockCount == 2u && bytecode.blockCount == 4u &&
              bytecode.phiCopyCount == 2u &&
              bytecode.successors[bytecode.blocks[0].successors.start] == 3u &&
              bytecode.successors[bytecode.blocks[0].successors.start + 1u] == 4u &&
              bytecode.predecessors[bytecode.blocks[1].predecessors.start] == 3u &&
              bytecode.predecessors[bytecode.blocks[1].predecessors.start + 1u] == 4u &&
              bytecode.phiIncomings[0].predecessor == 3u &&
              bytecode.phiIncomings[1].predecessor == 4u &&
              bytecode.phiCopySources[0] == 1u &&
              bytecode.phiCopySources[1] == 2u &&
              bytecode.phiCopyDestinations[0] == 4u &&
              bytecode.phiCopyDestinations[1] == 4u &&
              bytecode.phiCopyEdges[0] == 3u &&
              bytecode.phiCopyEdges[1] == 4u &&
              bytecode.predecessors[bytecode.blocks[2].predecessors.start] == 1u &&
              bytecode.predecessors[bytecode.blocks[3].predecessors.start] == 1u &&
              bytecode.successors[bytecode.blocks[2].successors.start] == 2u &&
              bytecode.successors[bytecode.blocks[3].successors.start] == 2u &&
              bytecode.successors[bytecode.instructions[3].successorRange.start + 1u] == 4u,
          "ExecBC projection merged the two phi edge copies");
    ZrParser_ExecBcExecutionResult_Init(&execution);
    check(ZrParser_ExecBcProjection_Run(&bytecode, ZR_NULL, &execution,
                                         &diagnostic) && execution.returned &&
              execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
              execution.returnValue.as.signedInteger == 22,
          "ExecBC runner bypassed phi moves on a separate terminator range");
    ZrParser_ExecBcExecutionResult_Free(&execution);
    check(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic),
          "AOTIR projection rejected a valid parallel-edge phi");
    check(aot.syntheticBlockCount == 2u && aot.phiCopyCount == 2u &&
              aot.phiCopyEdges[0] == 3u && aot.phiCopyEdges[1] == 4u &&
              aot.phiCopySources[0] == 1u && aot.phiCopySources[1] == 2u &&
              aot.predecessors[aot.blocks[1].predecessors.start] == 3u &&
              aot.predecessors[aot.blocks[1].predecessors.start + 1u] == 4u &&
              aot.phiIncomings[0].predecessor == 3u &&
              aot.phiIncomings[1].predecessor == 4u &&
              !aot.runnable,
          "AOTIR projection discarded the parallel phi edge identities");
    check(function.blockCount == 2u && function.predecessors[0] == 1u &&
              function.predecessors[1] == 1u,
          "projection modified the source ExecIR function");
    check(bytecode.phiMoveCount == 2u && aot.phiMoveCount == 2u &&
              bytecode.phiMoves[0].edge == 3u &&
              bytecode.phiMoves[1].edge == 4u &&
              aot.phiMoves[0].edge == 3u && aot.phiMoves[1].edge == 4u,
          "parallel CFG edges lost their separate executable copy plans");
    for (TZrUInt32 branch = 0u; branch < 2u; ++branch) {
        TZrUInt32 slots[4] = {11u, 22u, 0u, 0u};
        TZrExecIrBlockId edge = branch == 0u ? 3u : 4u;
        for (TZrUInt32 move = 0u; move < bytecode.phiMoveCount; ++move) {
            if (bytecode.phiMoves[move].edge == edge) {
                slots[bytecode.phiMoves[move].destinationSlot] =
                    slots[bytecode.phiMoves[move].sourceSlot];
            }
        }
        check(slots[3] == (branch == 0u ? 11u : 22u),
              "copy plan executed the wrong parallel edge");
    }
    ZrParser_AotIrProjection_Free(&aot);
    ZrParser_ExecBcProjection_Free(&bytecode);
    ZrCore_ExecIr_FreeFunction(&function);
}

typedef struct SZrParallelDiffFixture {
    SZrSsaFixture fixture;
    const SZrExecIrFunction *function;
    SZrExecIrOracleValue condition;
    TZrBool corruptReturnSource;
} SZrParallelDiffFixture;

static TZrBool parallel_diff_runner(const SZrSsaFixture *fixture,
                                    TZrUInt32 backend,
                                    SZrSsaObservation *observation) {
    const SZrParallelDiffFixture *test = (const SZrParallelDiffFixture *)fixture;
    SZrExecIrOracleValue value;
    TZrExecIrSourceId returnSource;
    SZrExecIrDiagnostic diagnostic;
    if (backend == 0u) {
        SZrExecIrOracleExecutionResult result;
        SZrExecIrOracleInput input = {0};
        input.function = test->function;
        input.constants = &test->condition;
        input.constantCount = 1u;
        ZrCore_ExecIr_OracleResultInit(&result);
        if (!ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic) ||
            !result.returned || result.currentBlock == 0u ||
            result.currentBlock > test->function->blockCount) {
            ZrCore_ExecIr_OracleResultFree(&result);
            return ZR_FALSE;
        }
        value = result.returnValue;
        {
            TZrExecIrInstructionId returnId = test->function->blocks[
                result.currentBlock - 1u].terminatorInstructionId;
            returnSource = test->function->instructions[returnId - 1u].sourceId;
        }
        ZrCore_ExecIr_OracleResultFree(&result);
    } else if (backend == 1u) {
        SZrExecBcProjection projection = {0};
        SZrExecBcExecutionResult result;
        SZrExecBcExecutionInput input = {0};
        input.constants = &test->condition;
        input.constantCount = 1u;
        if (!ZrParser_ExecIr_LowerExecBc(test->function, &projection, &diagnostic))
            return ZR_FALSE;
        ZrParser_ExecBcExecutionResult_Init(&result);
        if (!ZrParser_ExecBcProjection_Run(&projection, &input, &result,
                                            &diagnostic) || !result.returned ||
            result.returnInstructionId == 0u ||
            result.currentBlock == 0u) {
            ZrParser_ExecBcExecutionResult_Free(&result);
            ZrParser_ExecBcProjection_Free(&projection);
            return ZR_FALSE;
        }
        value = result.returnValue;
        returnSource = result.returnSourceId;
        ZrParser_ExecBcExecutionResult_Free(&result);
        ZrParser_ExecBcProjection_Free(&projection);
    } else {
        return ZR_FALSE;
    }
    ZrTests_Ssa_ObservationInit(observation);
    observation->backend = backend;
    observation->completed = ZR_TRUE;
    if (value.kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED) return ZR_FALSE;
    observation->resultType = ZR_SSA_RESULT_INTEGER;
    observation->resultBits = (TZrUInt64)value.as.signedInteger;
    if (backend == 1u && test->corruptReturnSource) ++returnSource;
    return ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_RETURN,
                                               returnSource,
                                               observation->resultBits, 0u);
}

static void test_oracle_execbc_parallel_phi_differential(void) {
    SZrExecIrFunction function;
    SZrParallelDiffFixture fixture = {0};
    SZrExecIrDiagnostic executionDiagnostic;
    SZrSsaObservation oracle = {0}, projected = {0};
    SZrSsaDiffDiagnostic difference;
    SZrSsaCoverage coverage;
    SZrExecIrRange separateSuccessors;
    TZrExecIrBlockId edges[2] = {2u, 2u};

    build_parallel_phi(&function);
    function.instructions[4].sourceId = 401u;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(&function, edges, 2u,
                                                  &separateSuccessors),
          "could not add independent terminator successors for differential test");
    function.instructions[3].successorRange = separateSuccessors;
    check(ZrCore_ExecIr_VerifyFunction(&function,
              ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA,
              &executionDiagnostic),
          "parallel phi differential input is not verifier-valid");
    fixture.fixture.name = "oracle-execbc-parallel-phi";
    fixture.fixture.runner = parallel_diff_runner;
    fixture.fixture.requiredBackends = 3u;
    fixture.function = &function;
    fixture.condition.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;

    for (TZrUInt32 opcode = 0u; opcode < 2u; ++opcode) {
        function.instructions[3].opcode = opcode == 0u
            ? ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH : ZR_EXEC_IR_OPCODE_SWITCH;
        for (TZrUInt32 condition = 0u; condition < 2u; ++condition) {
            fixture.condition.as.boolean = (TZrBool)condition;
            check(ZrTests_Ssa_RunFixture(&fixture.fixture, 0u, &oracle,
                                          &difference) &&
                  ZrTests_Ssa_RunFixture(&fixture.fixture, 1u, &projected,
                                          &difference) &&
                  ZrTests_Ssa_Compare(&oracle, &projected, &difference),
                  "oracle and ExecBC disagree on a parallel CFG/phi edge");
            check(projected.eventCount == 1u &&
                  projected.events[0].sourceId == 401u &&
                  projected.resultBits == (opcode == 0u
                      ? (condition != 0u ? 11u : 22u)
                      : (condition != 0u ? 22u : 11u)),
                  "differential fixture chose the wrong phi incoming edge");
            ZrTests_Ssa_CoverageInit(&coverage, fixture.fixture.requiredBackends);
            ZrTests_Ssa_CoverageRecord(&coverage, 0u, &oracle, ZR_TRUE);
            ZrTests_Ssa_CoverageRecord(&coverage, 1u, &projected, ZR_TRUE);
            check(ZrTests_Ssa_CoverageComplete(&coverage),
                  "actual oracle and projected backends were not covered");
        }
    }
    fixture.corruptReturnSource = ZR_TRUE;
    check(ZrTests_Ssa_RunFixture(&fixture.fixture, 1u, &projected,
                                  &difference) &&
          !ZrTests_Ssa_Compare(&oracle, &projected, &difference) &&
          difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
          difference.eventIndex == 0u &&
          difference.expectedSourceId == 401u &&
          difference.actualSourceId == 402u,
          "differential harness missed a return event source mismatch");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_projection_schedules_cycles_and_dependencies(void) {
    static const TZrExecIrValueId sources[7] = {2u, 3u, 1u, 1u, 5u, 7u, 6u};
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcProjection bytecode = {0};
    SZrExecIrPhi phis[7];
    SZrExecIrPhiIncoming incomings[7];
    SZrExecBcPhiMove *publishedMoves;
    TZrExecIrBlockId source = 1u, destination = 2u;
    TZrUInt32 slots[8] = {11u, 22u, 33u, 44u, 55u, 66u, 77u, 0u};

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 7u;
    function.functionToken = 701u;
    for (TZrUInt32 i = 0u; i < 7u; ++i) add_value(&function);
    check(ZrCore_ExecIr_FunctionAddBlock(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == source &&
              ZrCore_ExecIr_FunctionAddBlock(&function, 0u) == destination,
          "could not construct phi cycle blocks");
    function.entryBlockId = source;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(&function, &destination, 1u,
                                                  &function.blocks[0].successorRange) &&
              ZrCore_ExecIr_FunctionAppendPredecessors(&function, &source, 1u,
                                                        &function.blocks[1].predecessorRange),
          "could not connect phi cycle blocks");
    for (TZrUInt32 i = 0u; i < 7u; ++i) {
        phis[i].result = i + 1u;
        phis[i].incomings.start = i;
        phis[i].incomings.count = 1u;
        incomings[i].predecessor = source;
        incomings[i].value = sources[i];
    }
    check(ZrCore_ExecIr_FunctionAppendPhiIncoming(&function, incomings, 7u,
                                                   &function.blocks[1].phis) &&
              ZrCore_ExecIr_FunctionAppendPhis(&function, phis, 7u,
                                                &function.blocks[1].phis),
          "could not append phi cycle inputs");
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic),
          "could not project phi cycles");
    check(bytecode.phiCopyCount == 6u && bytecode.temporarySlotCount == 1u &&
              bytecode.phiMoveCount == 8u && bytecode.phiTemporarySlot == 7u,
          "phi cycles did not reserve one reusable temporary slot per edge");
    for (TZrUInt32 i = 0u; i < bytecode.phiMoveCount; ++i) {
        const SZrExecBcPhiMove *move = &bytecode.phiMoves[i];
        check(move->edge == source && move->sourceSlot < 8u &&
                  move->destinationSlot < 8u,
              "phi schedule referenced an invalid edge or slot");
        slots[move->destinationSlot] = slots[move->sourceSlot];
    }
    check(slots[0] == 22u && slots[1] == 33u && slots[2] == 11u &&
              slots[3] == 11u && slots[4] == 55u && slots[5] == 77u &&
              slots[6] == 66u,
          "sequential execution of scheduled phi moves broke simultaneous copies");
    publishedMoves = bytecode.phiMoves;
    function.frameLayout = (SZrExecIrFrameLayout *)calloc(1u, sizeof(*function.frameLayout));
    check(function.frameLayout != NULL, "could not allocate sparse frame layout");
    function.frameLayout->slots = (SZrExecIrFrameSlot *)calloc(7u, sizeof(*function.frameLayout->slots));
    check(function.frameLayout->slots != NULL, "could not allocate frame slots");
    function.frameLayout->slotCount = function.frameLayout->slotCapacity = 7u;
    function.frameLayout->storageSlotCount = 20u;
    for (TZrUInt32 i = 0u; i < 7u; ++i)
        function.frameLayout->slots[i].slotId = i;
    function.frameLayout->slots[6].slotId = UINT32_MAX;
    check(!ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW &&
              diagnostic.blockId == source && bytecode.phiMoves == publishedMoves &&
              bytecode.phiMoveCount == 8u,
          "overflowing a phi temporary clobbered the previously published schedule");
    function.frameLayout->slots[6].slotId = UINT32_MAX - 1u;
    check(!ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW &&
              bytecode.phiMoves == publishedMoves,
          "phi temporary at the largest slot ID cannot fit a slot count");
    function.frameLayout->slots[6].slotId = 6u;
    function.frameLayout->slots[5].slotId = 0u;
    check(!ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION &&
              bytecode.phiMoves == publishedMoves,
          "aliased physical value slots cannot implement simultaneous phi copies");
    function.frameLayout->slots[5].slotId = 5u;
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              bytecode.phiTemporarySlot == 20u && bytecode.phiMoveCount == 8u,
          "phi temporary overlapped a reserved frame slot");
    function.frameLayout->logicalSlotCount = 7u;
    function.frameLayout->storageSlotCount = 7u;
    for (TZrUInt32 i = 0u; i < 7u; ++i)
        function.frameLayout->slots[i].slotId = i + 1u;
    function.frameLayout->slots[0].slotId = 2u;
    function.frameLayout->slots[1].slotId = 1u;
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              bytecode.valueSlots[0] == 1u && bytecode.valueSlots[1] == 0u &&
              bytecode.phiTemporarySlot == 7u,
          "non-reusing packed frame confused logical value IDs with physical slots");
    {
        TZrUInt32 physical[8] = {22u, 11u, 33u, 44u, 55u, 66u, 77u, 0u};
        for (TZrUInt32 i = 0u; i < bytecode.phiMoveCount; ++i) {
            const SZrExecBcPhiMove *move = &bytecode.phiMoves[i];
            physical[move->destinationSlot] = physical[move->sourceSlot];
        }
        check(physical[0] == 33u && physical[1] == 22u &&
                  physical[2] == 11u && physical[3] == 11u &&
                  physical[5] == 77u && physical[6] == 66u,
              "packed physical move order changed simultaneous phi values");
    }
    publishedMoves = bytecode.phiMoves;
    function.frameLayout->slotCount = function.frameLayout->storageSlotCount = 6u;
    check(!ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION &&
              bytecode.phiMoves == publishedMoves,
          "slot-reusing packed frame replaced an executable phi move plan");
    ZrParser_ExecBcProjection_Free(&bytecode);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_verified_loop_backedge_phi_swap(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection bytecode = {0};
    SZrExecBcExecutionResult execution;
    SZrExecBcExecutionInput input = {0};
    SZrExecIrOracleValue initial[2] = {0};
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrRange empty = {.start = 0u, .count = 0u};
    SZrExecIrRange incomingRange;
    SZrExecIrPhi phis[2];
    SZrExecIrPhiIncoming incomings[4] = {
        {1u, 1u}, {2u, 4u}, {1u, 2u}, {2u, 3u}
    };
    TZrExecIrBlockId entry = 1u, loop = 2u, predecessors[2] = {1u, 2u};
    TZrUInt32 physical[5] = {11u, 22u, 0u, 0u, 0u};

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 8u;
    function.functionToken = 702u;
    for (TZrUInt32 i = 0u; i < 4u; ++i) add_value(&function);
    function.values[0].flags |= ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    function.values[1].flags |= ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    check(ZrCore_ExecIr_FunctionAddBlock(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == entry &&
              ZrCore_ExecIr_FunctionAddBlock(&function, 0u) == loop,
          "could not construct verified phi loop blocks");
    function.entryBlockId = entry;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(&function, &loop, 1u,
                                                  &function.blocks[0].successorRange) &&
              ZrCore_ExecIr_FunctionAppendPredecessors(&function, predecessors, 2u,
                                                        &function.blocks[1].predecessorRange) &&
              ZrCore_ExecIr_FunctionAppendSuccessors(&function, &loop, 1u,
                                                      &function.blocks[1].successorRange),
          "could not connect verified phi loop backedge");
    check(append_instruction(&function, ZR_EXEC_IR_OPCODE_BRANCH, empty, empty,
                             function.blocks[0].successorRange, 0u) == 1u &&
              append_instruction(&function, ZR_EXEC_IR_OPCODE_BRANCH, empty, empty,
                                 function.blocks[1].successorRange, 0u) == 2u,
          "could not append verified loop terminators");
    function.blocks[0].instructionRange.count = 1u;
    function.blocks[0].terminatorInstructionId = 1u;
    function.blocks[1].instructionRange.start = 1u;
    function.blocks[1].instructionRange.count = 1u;
    function.blocks[1].terminatorInstructionId = 2u;
    check(ZrCore_ExecIr_FunctionAppendPhiIncoming(&function, incomings, 4u,
                                                   &incomingRange),
          "could not append verified loop phi incomings");
    phis[0].result = 3u;
    phis[0].incomings = (SZrExecIrRange){.start = incomingRange.start, .count = 2u};
    phis[1].result = 4u;
    phis[1].incomings = (SZrExecIrRange){.start = incomingRange.start + 2u, .count = 2u};
    check(ZrCore_ExecIr_FunctionAppendPhis(&function, phis, 2u,
                                            &function.blocks[1].phis),
          "could not append verified loop phis");
    if (!ZrCore_ExecIr_VerifyFunction(&function,
            ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA, &diagnostic)) {
        fprintf(stderr, "loop verifier: code=%u block=%u instruction=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId);
        check(ZR_FALSE, "loop phi swap did not satisfy structural/SSA verification");
    }
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              bytecode.phiCopyCount == 4u && bytecode.temporarySlotCount == 1u,
          "could not lower verifier-valid loop phi copies");
    check(ZrParser_ExecBcProjection_ExecutePhiMoves(
              &bytecode, entry, physical, 5u, copy_slot, physical, &diagnostic),
          "ExecBC phi consumer rejected the loop entry edge");
    check(physical[2] == 11u && physical[3] == 22u,
          "verified loop entry phi copies lost initial values");
    check(ZrParser_ExecBcProjection_ExecutePhiMoves(
              &bytecode, loop, physical, 5u, copy_slot, physical, &diagnostic),
          "ExecBC phi consumer rejected the loop backedge");
    check(physical[2] == 22u && physical[3] == 11u,
          "verified loop backedge phi copies did not swap values");
    {
        TZrUInt32 before[5];
        memcpy(before, physical, sizeof(before));
        check(!ZrParser_ExecBcProjection_ExecutePhiMoves(
                  &bytecode, loop, physical, 5u, ZR_NULL, physical,
                  &diagnostic) &&
                  diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                  memcmp(before, physical, sizeof(before)) == 0,
              "phi consumer accepted a missing slot-copy callback");
        check(!ZrParser_ExecBcProjection_ExecutePhiMoves(
                  &bytecode, loop, physical, 2u, copy_slot, physical,
                  &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION &&
                  memcmp(before, physical, sizeof(before)) == 0,
              "phi consumer partially wrote slots before range validation");
    check(!ZrParser_ExecBcProjection_ExecutePhiMoves(
                  &bytecode, loop, physical, 5u, reject_slot, physical,
                  &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION &&
                  diagnostic.blockId == loop && diagnostic.actualVersion < bytecode.phiMoveCount,
              "phi consumer swallowed a slot-copy callback rejection");
    }
    initial[0].kind = initial[1].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    initial[0].as.signedInteger = 11;
    initial[1].as.signedInteger = 22;
    input.initialValues = initial;
    input.initialValueCount = 2u;
    input.maxSteps = 3u;
    ZrParser_ExecBcExecutionResult_Init(&execution);
    check(!ZrParser_ExecBcProjection_Run(&bytecode, &input, &execution,
                                          &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_STEP_LIMIT &&
              execution.slots == ZR_NULL,
          "ExecBC loop runner failed to stop before a fourth instruction");
    ZrParser_ExecBcExecutionResult_Free(&execution);
    ZrParser_ExecBcProjection_Free(&bytecode);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_branch_phi_moves_have_distinct_edge_blocks(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection bytecode = {0};
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId destinations[2] = {2u, 3u}, source = 1u;
    SZrExecIrPhiIncoming incoming;
    SZrExecIrPhi phi;
    SZrExecIrRange incomingRange, operandRange = {0}, instructionSuccessors = {0};
    SZrExecIrRange empty = {.start = 0u, .count = 0u};
    TZrExecIrValueId condition = 3u;

    ZrCore_ExecIr_FunctionInit(&function);
    add_value(&function);
    add_value(&function);
    add_value(&function);
    for (TZrUInt32 i = 0u; i < 3u; ++i)
        check(ZrCore_ExecIr_FunctionAddBlock(&function,
                  i == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u) == i + 1u,
              "could not build branching phi fixture");
    function.entryBlockId = source;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(&function, destinations, 2u,
                                                  &function.blocks[0].successorRange),
          "could not append branching phi successors");
    check(ZrCore_ExecIr_FunctionAppendSuccessors(&function, destinations, 2u,
                                                  &instructionSuccessors) &&
              ZrCore_ExecIr_FunctionAppendOperands(&function, &condition, 1u,
                                                    &operandRange),
          "could not append distinct terminator successors");
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = 1u;
    function.blocks[0].terminatorInstructionId = append_instruction(
        &function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
        operandRange, empty, instructionSuccessors, 0u);
    for (TZrUInt32 i = 1u; i < 3u; ++i) {
        check(ZrCore_ExecIr_FunctionAppendPredecessors(&function, &source, 1u,
                                                        &function.blocks[i].predecessorRange),
              "could not append branching phi predecessor");
        incoming.predecessor = source;
        incoming.value = i == 1u ? 2u : 1u;
        check(ZrCore_ExecIr_FunctionAppendPhiIncoming(&function, &incoming, 1u,
                                                       &incomingRange),
              "could not append branching phi input");
        phi.result = i;
        phi.incomings = incomingRange;
        check(ZrCore_ExecIr_FunctionAppendPhis(&function, &phi, 1u,
                                                &function.blocks[i].phis),
              "could not append branching phi");
    }
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic),
          "could not project branching phi edges");
    check(bytecode.syntheticBlockCount == 2u && bytecode.phiMoveCount == 2u &&
              bytecode.phiMoves[0].edge == 4u && bytecode.phiMoves[1].edge == 5u &&
              bytecode.successors[bytecode.blocks[0].successors.start] == 4u &&
              bytecode.successors[bytecode.blocks[0].successors.start + 1u] == 5u &&
              bytecode.successors[bytecode.instructions[0].successorRange.start] == 4u &&
              bytecode.successors[bytecode.instructions[0].successorRange.start + 1u] == 5u,
          "branch-local phi moves share a predecessor without an edge discriminator");
    ZrParser_ExecBcProjection_Free(&bytecode);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_projections_reject_unpaired_edge_without_replacing_output(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcProjection bytecode = {0};
    SZrAotIrProjection aot = {0};
    TZrExecIrBlockId *originalEdges;

    build_parallel_phi(&function);
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic),
          "could not prepare valid projection for failure-atomicity test");
    originalEdges = bytecode.phiCopyEdges;
    function.blocks[0].successorRange.count = 1u;
    check(!ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              bytecode.phiCopyEdges == originalEdges &&
              bytecode.phiCopyCount == 2u,
          "projection accepted an unmatched incoming duplicate or replaced output");
    function.blocks[0].successorRange.count = 2u;
    function.blocks[1].predecessorRange.count = 1u;
    check(!ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              aot.phiCopyCount == 2u && aot.phiCopyEdges[1] == 4u,
          "AOT projection accepted an unmatched outgoing duplicate or replaced output");
    ZrParser_AotIrProjection_Free(&aot);
    ZrParser_ExecBcProjection_Free(&bytecode);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_branch_and_switch_parallel_edges_select_distinct_incomings();
    test_rejects_selected_edge_missing_from_source_adjacency();
    test_projections_preserve_parallel_phi_edges();
    test_oracle_execbc_parallel_phi_differential();
    test_projection_schedules_cycles_and_dependencies();
    test_verified_loop_backedge_phi_swap();
    test_branch_phi_moves_have_distinct_edge_blocks();
    test_projections_reject_unpaired_edge_without_replacing_output();
    puts("ssa oracle parallel edges PASS");
    return EXIT_SUCCESS;
}
