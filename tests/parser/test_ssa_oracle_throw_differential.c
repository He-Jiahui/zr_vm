#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>

void test_oracle_execbc_throw_differential(void);

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

static void build_function(SZrExecIrFunction *function) {
    TZrExecIrValueId payload;
    SZrExecIrInstruction instruction = {0};
    SZrExecIrRange results = {0}, operands = {0};
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrInstructionId id;
    TZrExecIrMemoryTokenId scheduler = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_SCHEDULER_TASK, 1u);
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 97u;
    payload = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(payload == 1u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not allocate THROW value and block");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendResults(function, &payload, 1u, &results) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &payload, 1u, &operands),
          "could not append THROW result and operand");
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = results;
    instruction.layoutId = 37u;
    instruction.sourceId = 971u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id) &&
          id == 1u, "could not append THROW payload definition");
    instruction = (SZrExecIrInstruction){0};
    instruction.opcode = ZR_EXEC_IR_OPCODE_THROW;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.operands = operands;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 972u;
    check(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &scheduler, 1u,
                &instruction.memoryOut) &&
          ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id) &&
          id == 2u, "could not append THROW effect");
    function->blocks[0].instructionRange = range(0u, 2u);
    function->blocks[0].terminatorInstructionId = id;
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        fprintf(stderr, "THROW verifier: code=%u block=%u instruction=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId);
        check(ZR_FALSE, "THROW fixture must pass full verification");
    }
}

void test_oracle_execbc_throw_differential(void) {
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
          projection.runnable, "verified THROW projection is not runnable");
    input.function = &function;
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    check(ZrCore_ExecIr_RunOracleEx(&input, &direct, &diagnostic) &&
          ZrParser_ExecBcProjection_Run(&projection, ZR_NULL, &projected,
                                        &diagnostic), "THROW execution failed");
    check(direct.terminatedByThrow && projected.terminatedByThrow &&
          !direct.returned && !projected.returned &&
          direct.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
          projected.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
          direct.currentBlock == projected.currentBlock &&
          direct.eventCount == 1u && projected.eventCount == 1u &&
          direct.executedInstructionCount == 2u &&
          projected.executedInstructionCount == 2u,
          "THROW termination state or execution length differs");
    for (TZrUInt32 backend = 0u; backend < 2u; ++backend) {
        const SZrExecIrOracleEvent *event = backend == 0u
                ? &direct.events[0] : &projected.events[0];
        SZrSsaObservation *observation = backend == 0u ? &expected : &actual;
        check(event->kind == ZR_EXEC_IR_ORACLE_EVENT_THROW &&
              event->instructionId == 2u && event->sourceId == 972u &&
              event->operandCount == 1u &&
              event->operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
              event->operands[0].as.signedInteger == 37,
              "THROW event identity or payload differs");
        ZrTests_Ssa_ObservationInit(observation);
        observation->backend = backend;
        observation->completed = ZR_TRUE;
        observation->hasException = ZR_TRUE;
        observation->exceptionSourceId = event->sourceId;
        check(ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_THROW,
                    event->sourceId, (TZrUInt64)event->operands[0].as.signedInteger,
                    event->instructionId), "could not record THROW event");
    }
    check(ZrTests_Ssa_Compare(&expected, &actual, &difference),
          "THROW event or exception observation differs");
    actual.events[0].valueBits ^= 1u;
    check(!ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
          difference.eventIndex == 0u,
          "changed THROW payload escaped differential detection");
    actual.events[0].valueBits ^= 1u;
    ZrTests_Ssa_CoverageInit(&coverage, 3u);
    ZrTests_Ssa_CoverageRecord(&coverage, 0u, &expected, ZR_TRUE);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &actual, ZR_TRUE);
    check(ZrTests_Ssa_CoverageComplete(&coverage),
          "THROW differential did not execute both backends");
    {
        TZrUInt32 index = projection.instructions[1].operands.start;
        TZrExecIrValueId old = projection.operands[index];
        projection.operands[index] = projection.valueSlotCount + 1u;
        check(!ZrParser_ExecBcProjection_Run(&projection, ZR_NULL, &projected,
                                             &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 2u && diagnostic.sourceId == 972u &&
              projected.terminatedByThrow && projected.eventCount == 1u,
              "invalid THROW payload did not preserve published result");
        projection.operands[index] = old;
    }
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}
