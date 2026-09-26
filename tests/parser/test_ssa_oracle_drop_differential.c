#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>

typedef enum EZrDropCase {
    ZR_DROP_PLAIN,
    ZR_DROP_CONDITIONAL_TWICE,
    ZR_DROP_AFTER_MOVE
} EZrDropCase;

void test_oracle_execbc_drop_differential(void);

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange result = {0};
    result.start = start;
    result.count = count;
    return result;
}

static TZrExecIrInstructionId append(SZrExecIrFunction *function,
                                     EZrExecIrOpcode opcode,
                                     SZrExecIrRange operands,
                                     SZrExecIrRange results,
                                     TZrUInt32 literal,
                                     TZrExecIrSourceId source) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id = 0u;
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.layoutId = literal;
    instruction.sourceId = source;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id),
          "could not append ownership instruction");
    return id;
}

static void add_drop(SZrExecIrFunction *function, EZrExecIrOpcode opcode,
                     TZrExecIrValueId owner, TZrUInt32 order,
                     TZrExecIrSourceId source) {
    TZrExecIrMemoryTokenId in = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_OWNERSHIP, order);
    TZrExecIrMemoryTokenId out = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_OWNERSHIP, order + 1u);
    SZrExecIrRange operands = {0}, memoryIn = {0}, memoryOut = {0};
    TZrExecIrInstructionId id;
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &owner, 1u, &operands) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &in, 1u, &memoryIn) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &out, 1u, &memoryOut),
          "could not add ownership effect and memory tokens");
    id = append(function, opcode, operands, range(0u, 0u), 0u, source);
    function->instructions[id - 1u].effectIn = order;
    function->instructions[id - 1u].effectOut = order + 1u;
    function->instructions[id - 1u].memoryIn = memoryIn;
    function->instructions[id - 1u].memoryOut = memoryOut;
}

static void build_function(SZrExecIrFunction *function, EZrDropCase which) {
    TZrExecIrValueId owner, moved, answer, returned;
    SZrExecIrRange operands = {0}, results = {0};
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 95u;
    owner = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    moved = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    answer = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(owner == 1u && moved == 2u && answer == 3u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not allocate ownership values and block");
    function->entryBlockId = 1u;
    if (which != ZR_DROP_PLAIN)
        function->blocks[0].flags |= ZR_EXEC_IR_BLOCK_FLAG_CLEANUP;
    check(ZrCore_ExecIr_FunctionAppendResults(function, &owner, 1u, &results),
          "could not append owner result");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 9u, 801u);
    if (which == ZR_DROP_AFTER_MOVE) {
        check(ZrCore_ExecIr_FunctionAppendOperands(function, &owner, 1u, &operands) &&
              ZrCore_ExecIr_FunctionAppendResults(function, &moved, 1u, &results),
              "could not append MOVE operand/result");
        append(function, ZR_EXEC_IR_OPCODE_MOVE, operands, results, 0u, 802u);
    }
    add_drop(function, which == ZR_DROP_PLAIN ? ZR_EXEC_IR_OPCODE_DROP
                           : ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED,
             owner, 1u, 803u);
    if (which == ZR_DROP_CONDITIONAL_TWICE)
        add_drop(function, ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED,
                 owner, 2u, 804u);
    check(ZrCore_ExecIr_FunctionAppendResults(function, &answer, 1u, &results),
          "could not append answer result");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 11u, 805u);
    returned = which == ZR_DROP_AFTER_MOVE ? moved : answer;
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &returned, 1u, &operands),
          "could not append RETURN operand");
    function->blocks[0].terminatorInstructionId = append(function,
            ZR_EXEC_IR_OPCODE_RETURN, operands, range(0u, 0u), 0u, 806u);
    function->blocks[0].instructionRange = range(0u, function->instructionCount);
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        fprintf(stderr, "drop verifier: case=%u code=%u block=%u instruction=%u source=%u\n",
                (unsigned)which, (unsigned)diagnostic.code,
                (unsigned)diagnostic.blockId, (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.sourceId);
        check(ZR_FALSE, "ownership function is not verifier-valid");
    }
}

static void observe(SZrSsaObservation *observation, TZrUInt32 backend,
                    const SZrExecIrOracleEvent *events, TZrUInt32 count,
                    const SZrExecIrOracleValue *returned) {
    ZrTests_Ssa_ObservationInit(observation);
    observation->backend = backend;
    observation->completed = ZR_TRUE;
    observation->resultType = ZR_SSA_RESULT_INTEGER;
    observation->resultBits = (TZrUInt64)returned->as.signedInteger;
    observation->droppedCount = count;
    for (TZrUInt32 i = 0u; i < count; ++i) {
        const SZrExecIrOracleEvent *event = &events[i];
        check(event->kind == ZR_EXEC_IR_ORACLE_EVENT_DROP &&
              event->sourceId == 803u && event->operandCount == 1u &&
              event->operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
              "drop event lost its owned operand or identity");
        check(ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_DROP,
                    event->sourceId, (TZrUInt64)event->operands[0].as.signedInteger,
                    event->instructionId), "could not record drop event");
    }
    check(ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_RETURN,
                806u, observation->resultBits, 0u), "could not record return event");
}

static void build_conditional_branch(SZrExecIrFunction *function,
                                     TZrBool initialize) {
    TZrExecIrValueId condition, answer, owner;
    TZrExecIrBlockId successors[2] = {2u, 3u}, predecessors[2] = {1u, 2u};
    SZrExecIrRange operands = {0}, results = {0};
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrInstructionId id;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 95u;
    condition = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    answer = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    owner = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNIQUE, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(condition == 1u && answer == 2u && owner == 3u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
          ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) == 3u,
          "could not build conditional cleanup blocks");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                  &function->blocks[0].successorRange) &&
          ZrCore_ExecIr_FunctionAppendSuccessors(function, &successors[1], 1u,
                  &function->blocks[1].successorRange) &&
          ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, 1u,
                  &function->blocks[1].predecessorRange) &&
          ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, 2u,
                  &function->blocks[2].predecessorRange),
          "could not build conditional cleanup edges");
    check(ZrCore_ExecIr_FunctionAppendResults(function, &condition, 1u, &results),
          "could not append branch condition");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results,
           initialize ? 1u : 0u, 800u);
    check(ZrCore_ExecIr_FunctionAppendResults(function, &answer, 1u, &results) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u, &operands),
          "could not append entry values");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 11u, 805u);
    id = append(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, operands,
                range(0u, 0u), 0u, 802u);
    function->instructions[id - 1u].successorRange = function->blocks[0].successorRange;
    function->blocks[0].instructionRange = range(0u, 3u);
    function->blocks[0].terminatorInstructionId = id;
    check(ZrCore_ExecIr_FunctionAppendResults(function, &owner, 1u, &results),
          "could not append conditional owner");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 9u, 801u);
    id = append(function, ZR_EXEC_IR_OPCODE_BRANCH, range(0u, 0u),
                range(0u, 0u), 0u, 804u);
    function->instructions[id - 1u].successorRange = function->blocks[1].successorRange;
    function->blocks[1].instructionRange = range(3u, 2u);
    function->blocks[1].terminatorInstructionId = id;
    add_drop(function, ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED, owner, 1u, 803u);
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &answer, 1u, &operands),
          "could not append conditional return");
    id = append(function, ZR_EXEC_IR_OPCODE_RETURN, operands,
                range(0u, 0u), 0u, 806u);
    function->blocks[2].instructionRange = range(5u, 2u);
    function->blocks[2].terminatorInstructionId = id;
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        fprintf(stderr, "conditional cleanup verifier: code=%u block=%u instruction=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId);
        check(ZR_FALSE, "conditional cleanup is not verifier-valid");
    }
}

static void test_conditional_branch_differential(SZrSsaCoverage *coverage) {
    for (TZrUInt32 initialize = 0u; initialize < 2u; ++initialize) {
        SZrExecIrFunction function;
        SZrExecBcProjection projection = {0};
        SZrAotIrProjection aot = {0};
        SZrExecIrOracleInput oracleInput = {0};
        SZrExecIrOracleExecutionResult direct;
        SZrExecBcExecutionResult projected;
        SZrExecIrDiagnostic diagnostic;
        SZrSsaObservation expected = {0}, actual = {0};
        SZrSsaDiffDiagnostic difference;
        build_conditional_branch(&function, (TZrBool)initialize);
        check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
              projection.runnable, "conditional cleanup did not lower");
        check(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic) &&
              !aot.runnable && aot.slotValues != ZR_NULL &&
              aot.slotValues[aot.valueSlots[2]].ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE,
              "AOT ownership metadata did not transfer");
        oracleInput.function = &function;
        ZrCore_ExecIr_OracleResultInit(&direct);
        ZrParser_ExecBcExecutionResult_Init(&projected);
        check(ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
              ZrParser_ExecBcProjection_Run(&projection, ZR_NULL,
                                            &projected, &diagnostic),
              "conditional cleanup did not execute");
        check(direct.returned && projected.returned &&
              direct.eventCount == initialize && projected.eventCount == initialize &&
              direct.ownerStates[2] == projected.ownerStates[projection.valueSlots[2]] &&
              direct.ownerStates[2] == (TZrUInt32)(initialize
                  ? ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED
                  : ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED),
              "conditional cleanup event or owner state differs");
        observe(&expected, 0u, direct.events, direct.eventCount, &direct.returnValue);
        observe(&actual, 1u, projected.events, projected.eventCount,
                &projected.returnValue);
        check(ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
              actual.resultBits == 11u, "conditional cleanup result or trace differs");
        ZrTests_Ssa_CoverageRecord(coverage, 0u, &expected, ZR_TRUE);
        ZrTests_Ssa_CoverageRecord(coverage, 1u, &actual, ZR_TRUE);
        ZrParser_ExecBcExecutionResult_Free(&projected);
        ZrCore_ExecIr_OracleResultFree(&direct);
        ZrParser_ExecBcProjection_Free(&projection);
        ZrParser_AotIrProjection_Free(&aot);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

void test_oracle_execbc_drop_differential(void) {
    SZrSsaCoverage coverage;
    ZrTests_Ssa_CoverageInit(&coverage, 3u);
    for (TZrUInt32 which = ZR_DROP_PLAIN; which <= ZR_DROP_AFTER_MOVE; ++which) {
        SZrExecIrFunction function;
        SZrExecBcProjection projection = {0};
        SZrExecIrOracleInput oracleInput = {0};
        SZrExecBcExecutionInput projectedInput = {0};
        SZrExecIrOracleExecutionResult direct;
        SZrExecBcExecutionResult projected;
        SZrExecIrDiagnostic diagnostic;
        SZrSsaObservation expected = {0}, actual = {0};
        SZrSsaDiffDiagnostic difference;
        TZrUInt32 expectedDrops = which == ZR_DROP_AFTER_MOVE ? 0u : 1u;

        build_function(&function, (EZrDropCase)which);
        check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
              projection.runnable, "ownership function did not lower as runnable");
        oracleInput.function = &function;
        ZrCore_ExecIr_OracleResultInit(&direct);
        ZrParser_ExecBcExecutionResult_Init(&projected);
        {
            check(ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
                  ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                                &projected, &diagnostic),
                  "ownership case did not execute in both backends");
            check(direct.returned && projected.returned &&
                  direct.eventCount == expectedDrops &&
                  projected.eventCount == expectedDrops &&
                  direct.ownerStates[0] == projected.ownerStates[
                        projection.valueSlots[0]] &&
                  direct.ownerStates[1] == projected.ownerStates[
                        projection.valueSlots[1]],
                  "ownership state or drop count differs");
            check(direct.ownerStates[0] == (TZrUInt32)(which == ZR_DROP_AFTER_MOVE
                        ? ZR_EXEC_IR_STATE_MAP_OWNER_MOVED
                        : ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED),
                  "owner was not consumed exactly once");
            observe(&expected, 0u, direct.events, direct.eventCount,
                    &direct.returnValue);
            observe(&actual, 1u, projected.events, projected.eventCount,
                    &projected.returnValue);
            check(ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
                  actual.resultBits == (which == ZR_DROP_AFTER_MOVE ? 9u : 11u),
                  "drop event/order/result mismatch");
            ZrTests_Ssa_CoverageRecord(&coverage, 0u, &expected, ZR_TRUE);
            ZrTests_Ssa_CoverageRecord(&coverage, 1u, &actual, ZR_TRUE);
            if (which == ZR_DROP_PLAIN) {
                TZrExecIrValueId old = projection.operands[
                        projection.instructions[projection.instructionCount - 1u].operands.start];
                projection.operands[projection.instructions[
                        projection.instructionCount - 1u].operands.start] = 1u;
                check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                                     &projected, &diagnostic) &&
                      diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
                      diagnostic.sourceId == 806u,
                      "read after DROP did not report source and invalid value");
                projection.operands[projection.instructions[
                        projection.instructionCount - 1u].operands.start] = old;
            }
        }
        ZrParser_ExecBcExecutionResult_Free(&projected);
        ZrCore_ExecIr_OracleResultFree(&direct);
        ZrParser_ExecBcProjection_Free(&projection);
        ZrCore_ExecIr_FreeFunction(&function);
    }
    test_conditional_branch_differential(&coverage);
    check(ZrTests_Ssa_CoverageComplete(&coverage),
          "ownership differential did not execute both backends");
}
