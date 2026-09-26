#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>

void test_oracle_execbc_barrier_differential(void);

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
    if (opcode == ZR_EXEC_IR_OPCODE_BARRIER)
        instruction.flags = ZR_EXEC_IR_FLAG_MAY_GC;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id),
          "could not append barrier fixture instruction");
    return id;
}

static void add_barrier(SZrExecIrFunction *function, TZrExecIrValueId payload,
                        TZrUInt32 order, TZrExecIrSourceId source) {
    TZrExecIrMemoryTokenId inputToken = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, order);
    TZrExecIrMemoryTokenId outputTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, order + 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_GC, order)
    };
    SZrExecIrRange operands = {0}, memoryIn = {0}, memoryOut = {0};
    TZrExecIrInstructionId id;
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &payload, 1u, &operands) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &inputToken, 1u,
                                                   &memoryIn) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, outputTokens, 2u,
                                                   &memoryOut),
          "could not append barrier effect versions");
    id = append(function, ZR_EXEC_IR_OPCODE_BARRIER, operands, range(0u, 0u),
                0u, source);
    function->instructions[id - 1u].effectIn = order;
    function->instructions[id - 1u].effectOut = order + 1u;
    function->instructions[id - 1u].memoryIn = memoryIn;
    function->instructions[id - 1u].memoryOut = memoryOut;
}

static void build_function(SZrExecIrFunction *function) {
    TZrExecIrValueId payload, answer;
    SZrExecIrRange results = {0}, operands = {0};
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 96u;
    payload = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    answer = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(payload == 1u && answer == 2u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not allocate barrier fixture values and block");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendResults(function, &payload, 1u, &results),
          "could not append barrier payload result");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 19u, 900u);
    add_barrier(function, payload, 1u, 901u);
    check(ZrCore_ExecIr_FunctionAppendResults(function, &answer, 1u, &results) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &answer, 1u, &operands),
          "could not append barrier answer");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 23u, 903u);
    add_barrier(function, answer, 2u, 902u);
    function->blocks[0].terminatorInstructionId = append(function,
            ZR_EXEC_IR_OPCODE_RETURN, operands, range(0u, 0u), 0u, 904u);
    function->blocks[0].instructionRange = range(0u, function->instructionCount);
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        fprintf(stderr, "barrier verifier: code=%u instruction=%u source=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.sourceId);
        check(ZR_FALSE, "barrier fixture must pass full verification");
    }
}

static void observe(SZrSsaObservation *observation, TZrUInt32 backend,
                    const SZrExecIrOracleEvent *events,
                    const SZrExecIrOracleValue *returned) {
    ZrTests_Ssa_ObservationInit(observation);
    observation->backend = backend;
    observation->completed = ZR_TRUE;
    observation->resultType = ZR_SSA_RESULT_INTEGER;
    observation->resultBits = (TZrUInt64)returned->as.signedInteger;
    for (TZrUInt32 i = 0u; i < 2u; ++i) {
        check(events[i].kind == ZR_EXEC_IR_ORACLE_EVENT_BARRIER &&
              events[i].instructionId == 2u * i + 2u &&
              events[i].sourceId == i + 901u &&
              events[i].operandCount == 1u,
              "barrier event identity or arity mismatch");
        check(events[i].operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
              events[i].operands[0].as.signedInteger == (i == 0u ? 19 : 23),
              "barrier operand snapshot mismatch");
        check(ZrTests_Ssa_ObservationAppendEvent(observation,
                    ZR_SSA_EVENT_BARRIER, events[i].sourceId,
                    (TZrUInt64)events[i].operands[0].as.signedInteger,
                    events[i].instructionId), "could not record barrier event");
    }
    check(ZrTests_Ssa_ObservationAppendEvent(observation,
                ZR_SSA_EVENT_RETURN, 904u, observation->resultBits, 0u),
          "could not record barrier return");
}

void test_oracle_execbc_barrier_differential(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult direct;
    SZrExecBcExecutionResult projected;
    SZrExecIrDiagnostic diagnostic;
    SZrSsaObservation expected = {0}, actual = {0};
    SZrSsaDiffDiagnostic difference;
    SZrSsaCoverage coverage;
    build_function(&function);
    check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
          projection.runnable, "verified barrier projection is not runnable");
    input.function = &function;
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    check(ZrCore_ExecIr_RunOracleEx(&input, &direct, &diagnostic) &&
          ZrParser_ExecBcProjection_Run(&projection, ZR_NULL, &projected,
                                        &diagnostic), "barrier execution failed");
    check(direct.returned && projected.returned &&
          direct.eventCount == 2u && projected.eventCount == 2u,
          "barrier event count or return differs");
    observe(&expected, 0u, direct.events, &direct.returnValue);
    observe(&actual, 1u, projected.events, &projected.returnValue);
    check(ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          actual.resultBits == 23u, "barrier trace or return differs");
    actual.events[1].valueBits ^= 1u;
    check(!ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
          difference.eventIndex == 1u,
          "changed second barrier payload escaped differential detection");
    actual.events[1].valueBits ^= 1u;
    ZrTests_Ssa_CoverageInit(&coverage, 3u);
    ZrTests_Ssa_CoverageRecord(&coverage, 0u, &expected, ZR_TRUE);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &actual, ZR_TRUE);
    check(ZrTests_Ssa_CoverageComplete(&coverage),
          "barrier differential did not execute both backends");
    {
        TZrUInt32 old = projection.operands[projection.instructions[1].operands.start];
        projection.operands[projection.instructions[1].operands.start] =
                projection.valueSlotCount + 1u;
        check(!ZrParser_ExecBcProjection_Run(&projection, ZR_NULL, &projected,
                                             &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 2u && diagnostic.sourceId == 901u &&
              projected.eventCount == 2u && projected.returned,
              "invalid barrier operand did not preserve published result");
        projection.operands[projection.instructions[1].operands.start] = old;
    }
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}
