#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct SZrIteratorFixture {
    EZrExecIrOpcode opcode;
    TZrBool threw;
    TZrBool reject;
    TZrBool undefined;
    TZrUInt32 calls;
    TZrUInt32 payloadCalls;
} SZrIteratorFixture;

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange result = {start, count};
    return result;
}

static void append(SZrExecIrFunction *function, EZrExecIrOpcode opcode,
                   SZrExecIrRange operands, SZrExecIrRange results,
                   SZrExecIrRange successors, TZrExecIrSourceId source) {
    SZrExecIrInstruction instruction = {0};
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.successorRange = successors;
    instruction.sourceId = source;
    if (opcode == ZR_EXEC_IR_OPCODE_ITER_INIT ||
        opcode == ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT ||
        opcode == ZR_EXEC_IR_OPCODE_ITER_CURRENT) {
        instruction.flags = ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
        instruction.effectIn = 1u;
        instruction.effectOut = 2u;
    }
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, ZR_NULL),
          "iterator fixture instruction allocation failed");
}

static void build(SZrExecIrFunction *function, EZrExecIrOpcode opcode) {
    TZrExecIrValueId input, output, payload, successors[2] = {2u, 3u};
    TZrExecIrBlockId predecessor = 1u;
    SZrExecIrRange iteratorOperands = {0}, iteratorResult = {0};
    SZrExecIrRange payloadResult = {0}, memoryIn = {0}, memoryOut = {0};
    SZrExecIrRange normalReturn = {0}, exceptionalReturn = {0};
    TZrExecIrMemoryTokenId before[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)};
    TZrExecIrMemoryTokenId after[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)};
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 721u;
    input = ZrCore_ExecIr_FunctionAddExternalValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    output = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    payload = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(input == 1u && output == 2u && payload == 3u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
          ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 3u,
          "iterator fixture CFG allocation failed");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                &function->blocks[0].successorRange) &&
          ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                &function->blocks[1].predecessorRange) &&
          ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                &function->blocks[2].predecessorRange) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &input, 1u,
                &iteratorOperands) &&
          ZrCore_ExecIr_FunctionAppendResults(function, &output, 1u,
                &iteratorResult) &&
          ZrCore_ExecIr_FunctionAppendResults(function, &payload, 1u,
                &payloadResult) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &output, 1u,
                &normalReturn) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &payload, 1u,
                &exceptionalReturn) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, before, 2u,
                &memoryIn) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, after, 2u,
                &memoryOut), "iterator fixture values allocation failed");
    append(function, opcode, iteratorOperands, iteratorResult,
           function->blocks[0].successorRange, 721u);
    function->instructions[0].memoryIn = memoryIn;
    function->instructions[0].memoryOut = memoryOut;
    append(function, ZR_EXEC_IR_OPCODE_RETURN, normalReturn,
           range(0u, 0u), range(0u, 0u), 722u);
    append(function, ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD,
           range(0u, 0u), payloadResult, range(0u, 0u), 723u);
    append(function, ZR_EXEC_IR_OPCODE_RETURN, exceptionalReturn,
           range(0u, 0u), range(0u, 0u), 724u);
    function->blocks[0].instructionRange = range(0u, 1u);
    function->blocks[0].terminatorInstructionId = 1u;
    function->blocks[1].instructionRange = range(1u, 1u);
    function->blocks[1].terminatorInstructionId = 2u;
    function->blocks[2].instructionRange = range(2u, 2u);
    function->blocks[2].terminatorInstructionId = 4u;
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                      &diagnostic)) {
        fprintf(stderr, "iterator verifier: code=%u instruction=%u source=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.sourceId);
        check(ZR_FALSE, "iterator fixture must pass full verification");
    }
}

static TZrBool iterator_value(SZrIteratorFixture *fixture,
                              EZrExecIrOpcode opcode,
                              const SZrExecIrOracleValue *operands,
                              TZrUInt32 count, SZrExecIrOracleValue *result,
                              TZrBool *threw) {
    check(opcode == fixture->opcode && count == 1u &&
          operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
          operands[0].as.signedInteger == 17,
          "iterator callback operands or opcode changed");
    ++fixture->calls;
    if (fixture->reject) return ZR_FALSE;
    *threw = fixture->threw;
    result->kind = fixture->undefined ? ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED
                                     : ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = 31;
    return ZR_TRUE;
}

static TZrBool oracle_iterator(void *context,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 count,
        SZrExecIrOracleValue *result, TZrBool *threw) {
    return iterator_value((SZrIteratorFixture *)context,
            (EZrExecIrOpcode)instruction->opcode, operands, count, result, threw);
}

static TZrBool projected_iterator(void *context,
        const SZrExecBcInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 count,
        SZrExecIrOracleValue *result, TZrBool *threw) {
    return iterator_value((SZrIteratorFixture *)context,
            (EZrExecIrOpcode)instruction->opcode, operands, count, result, threw);
}

static TZrBool oracle_payload(void *context,
        const SZrExecIrInstruction *instruction, SZrExecIrOracleValue *result) {
    SZrIteratorFixture *fixture = (SZrIteratorFixture *)context;
    check(instruction->sourceId == 723u, "oracle payload source changed");
    ++fixture->payloadCalls;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = -23;
    return ZR_TRUE;
}

static TZrBool projected_payload(void *context,
        const SZrExecBcInstruction *instruction, SZrExecIrOracleValue *result) {
    SZrIteratorFixture *fixture = (SZrIteratorFixture *)context;
    check(instruction->sourceId == 723u, "projected payload source changed");
    ++fixture->payloadCalls;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = -23;
    return ZR_TRUE;
}

void test_oracle_execbc_iterator_differential(void) {
    static const EZrExecIrOpcode opcodes[] = {
        ZR_EXEC_IR_OPCODE_ITER_INIT, ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT,
        ZR_EXEC_IR_OPCODE_ITER_CURRENT};
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        SZrExecIrFunction function;
        SZrExecBcProjection projection = {0};
        SZrAotIrProjection aot = {0};
        SZrExecIrDiagnostic diagnostic;
        SZrExecIrOracleInput oracleInput = {0};
        SZrExecBcExecutionInput projectedInput = {0};
        SZrExecIrOracleExecutionResult direct = {0};
        SZrExecBcExecutionResult projected;
        SZrIteratorFixture oracle = {0}, execbc = {0};
        SZrExecIrOracleValue initial = {0};
        build(&function, opcodes[index]);
        check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
              projection.runnable, "iterator ExecBC must be runnable");
        check(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic) &&
              !aot.runnable, "iterator AOTIR must remain non-runnable");
        oracle.opcode = execbc.opcode = opcodes[index];
        initial.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        initial.as.signedInteger = 17;
        oracleInput.function = &function;
        oracleInput.initialValues = &initial;
        oracleInput.initialValueCount = 1u;
        oracleInput.iterator = oracle_iterator;
        oracleInput.iteratorUserData = &oracle;
        oracleInput.exceptionPayload = oracle_payload;
        oracleInput.exceptionPayloadUserData = &oracle;
        projectedInput.initialValues = &initial;
        projectedInput.initialValueCount = 1u;
        projectedInput.iterator = projected_iterator;
        projectedInput.iteratorUserData = &execbc;
        projectedInput.exceptionPayload = projected_payload;
        projectedInput.exceptionPayloadUserData = &execbc;
        ZrParser_ExecBcExecutionResult_Init(&projected);
        for (TZrUInt32 path = 0u; path < 2u; ++path) {
            oracle.threw = execbc.threw = path != 0u;
            check(ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
                  ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                                &projected, &diagnostic),
                  "iterator oracle/ExecBC execution failed");
            check(direct.returned && projected.returned &&
                  direct.currentBlock == path + 2u &&
                  projected.currentBlock == direct.currentBlock &&
                  direct.returnValue.kind == projected.returnValue.kind &&
                  direct.returnValue.as.signedInteger == (path ? -23 : 31) &&
                  projected.returnValue.as.signedInteger ==
                          direct.returnValue.as.signedInteger &&
                  direct.executedInstructionCount ==
                          projected.executedInstructionCount &&
                  direct.eventCount == 1u && projected.eventCount == 1u &&
                  direct.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_ITERATOR &&
                  projected.events[0].kind == direct.events[0].kind &&
                  projected.events[0].sourceId == direct.events[0].sourceId &&
                  projected.events[0].instructionId ==
                          direct.events[0].instructionId &&
                  projected.events[0].operandCount == 1u &&
                  projected.events[0].operands[0].as.signedInteger == 17,
                  "iterator normal/exception CFG or event diverged");
        }
        check(oracle.calls == 2u && execbc.calls == 2u &&
              oracle.payloadCalls == 1u && execbc.payloadCalls == 1u,
              "iterator callback counts diverged");
        projectedInput.iterator = ZR_NULL;
        check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                             &projected, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
              diagnostic.instructionId == 1u && diagnostic.sourceId == 721u &&
              projected.returnValue.as.signedInteger == -23,
              "missing iterator provider replaced published result");
        projectedInput.iterator = projected_iterator;
        execbc.reject = ZR_TRUE;
        check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                             &projected, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ITERATOR_ERROR &&
              diagnostic.instructionId == 1u && diagnostic.sourceId == 721u &&
              projected.eventCount == 1u,
              "rejected iterator callback replaced published result");
        execbc.reject = ZR_FALSE;
        execbc.undefined = ZR_TRUE;
        execbc.threw = ZR_FALSE;
        check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                             &projected, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 1u && diagnostic.sourceId == 721u &&
              projected.eventCount == 1u,
              "undefined normal iterator result replaced published result");
        execbc.threw = ZR_TRUE;
        check(ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                            &projected, &diagnostic) &&
              projected.currentBlock == 3u &&
              projected.slots[projection.valueSlots[1]].kind ==
                      ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED,
              "exceptional iterator edge published normal result");
        ZrParser_ExecBcExecutionResult_Free(&projected);
        ZrCore_ExecIr_OracleResultFree(&direct);
        ZrParser_AotIrProjection_Free(&aot);
        ZrParser_ExecBcProjection_Free(&projection);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}
