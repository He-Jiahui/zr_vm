#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>

void test_oracle_execbc_suspend_differential(void);

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

static void build_function(SZrExecIrFunction *function, TZrUInt32 count) {
    TZrExecIrValueId payloads[5], suspended;
    SZrExecIrInstruction instruction = {0};
    SZrExecIrRange results = {0}, operands = {0};
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrInstructionId id = 0u;
    TZrExecIrMemoryTokenId in = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_SCHEDULER_TASK, 1u);
    TZrExecIrMemoryTokenId out = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_SCHEDULER_TASK, 2u);
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 98u;
    for (TZrUInt32 i = 0u; i < count; ++i) {
        payloads[i] = ZrCore_ExecIr_FunctionAddValue(function, 1u,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        check(payloads[i] == i + 1u, "could not allocate SUSPEND operand");
    }
    suspended = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(suspended == count + 1u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not allocate SUSPEND values and block");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendOperands(function, payloads, count,
                                               &operands),
          "could not append SUSPEND operands");
    for (TZrUInt32 i = 0u; i < count; ++i) {
        check(ZrCore_ExecIr_FunctionAppendResults(function, &payloads[i], 1u,
                                                  &results),
              "could not append SUSPEND payload result");
        instruction = (SZrExecIrInstruction){0};
        instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
        instruction.results = results;
        instruction.layoutId = 41u + i;
        instruction.sourceId = 981u;
        check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id) &&
              id == i + 1u, "could not append SUSPEND payload definition");
    }
    check(ZrCore_ExecIr_FunctionAppendResults(function, &suspended, 1u, &results),
          "could not append SUSPEND result");
    instruction = (SZrExecIrInstruction){0};
    instruction.opcode = ZR_EXEC_IR_OPCODE_SUSPEND;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    instruction.operands = operands;
    instruction.results = results;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 982u;
    check(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &in, 1u,
                &instruction.memoryIn) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &out, 1u,
                &instruction.memoryOut) &&
          ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id) &&
          id == count + 1u, "could not append SUSPEND effect and result");
    function->blocks[0].instructionRange = range(0u, count + 1u);
    function->blocks[0].terminatorInstructionId = id;
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        fprintf(stderr, "SUSPEND verifier: code=%u block=%u instruction=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId);
        check(ZR_FALSE, "SUSPEND fixture must pass full verification");
    }
}

static void test_case(TZrUInt32 count) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecIrOracleInput input = {0};
    SZrExecIrOracleExecutionResult direct;
    SZrExecBcExecutionResult projected;
    SZrExecIrDiagnostic diagnostic;
    SZrSsaObservation expected = {0}, actual = {0};
    SZrSsaDiffDiagnostic difference;
    SZrSsaCoverage coverage;
    build_function(&function, count);
    check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
          projection.runnable, "verified SUSPEND projection is not runnable");
    input.function = &function;
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    check(ZrCore_ExecIr_RunOracleEx(&input, &direct, &diagnostic) &&
          ZrParser_ExecBcProjection_Run(&projection, ZR_NULL, &projected,
                                        &diagnostic), "SUSPEND execution failed");
    check(direct.suspended && projected.suspended &&
          !direct.returned && !projected.returned &&
          direct.currentBlock == projected.currentBlock &&
          direct.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
          projected.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
          direct.returnValue.as.signedInteger == 41 &&
          projected.returnValue.as.signedInteger == 41 &&
          direct.values[count].as.signedInteger ==
              projected.slots[projection.valueSlots[count]].as.signedInteger &&
          direct.eventCount == 1u && projected.eventCount == 1u &&
          direct.executedInstructionCount == count + 1u &&
          projected.executedInstructionCount == count + 1u,
          "SUSPEND state or result differs");
    for (TZrUInt32 backend = 0u; backend < 2u; ++backend) {
        const SZrExecIrOracleEvent *event = backend == 0u
                ? &direct.events[0] : &projected.events[0];
        SZrSsaObservation *observation = backend == 0u ? &expected : &actual;
        check(event->kind == ZR_EXEC_IR_ORACLE_EVENT_SUSPEND &&
              event->instructionId == count + 1u && event->sourceId == 982u &&
              event->operandCount == (count < ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT
                      ? count : ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT),
              "SUSPEND event identity or payload differs");
        ZrTests_Ssa_ObservationInit(observation);
        observation->backend = backend;
        observation->completed = ZR_TRUE;
        observation->resultType = ZR_SSA_RESULT_INTEGER;
        observation->resultBits = 41u;
        for (TZrUInt32 i = 0u; i < event->operandCount; ++i) {
            check(event->operands[i].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                  event->operands[i].as.signedInteger == (TZrInt64)(41u + i) &&
                  ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_SUSPEND,
                      event->sourceId,
                      (TZrUInt64)event->operands[i].as.signedInteger,
                      event->instructionId), "SUSPEND operand snapshot differs");
        }
    }
    check(ZrTests_Ssa_Compare(&expected, &actual, &difference),
          "SUSPEND event or result observation differs");
    actual.events[0].valueBits ^= 1u;
    check(!ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
          difference.eventIndex == 0u,
          "changed SUSPEND payload escaped differential detection");
    actual.events[0].valueBits ^= 1u;
    ZrTests_Ssa_CoverageInit(&coverage, 3u);
    ZrTests_Ssa_CoverageRecord(&coverage, 0u, &expected, ZR_TRUE);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &actual, ZR_TRUE);
    check(ZrTests_Ssa_CoverageComplete(&coverage),
          "SUSPEND differential did not execute both backends");
    {
        TZrUInt32 index = projection.instructions[count].operands.start + count - 1u;
        TZrExecIrValueId old = projection.operands[index];
        projection.operands[index] = projection.valueSlotCount + 1u;
        check(!ZrParser_ExecBcProjection_Run(&projection, ZR_NULL, &projected,
                                             &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == count + 1u && diagnostic.sourceId == 982u &&
              projected.suspended && projected.eventCount == 1u &&
              projected.returnValue.as.signedInteger == 41,
              "invalid SUSPEND operand did not preserve published result");
        projection.operands[index] = old;
    }
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}

void test_oracle_execbc_suspend_differential(void) {
    test_case(1u);
    test_case(5u);
}
