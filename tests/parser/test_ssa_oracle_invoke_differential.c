#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>

void test_oracle_execbc_invoke_differential(void);

static void check(TZrBool good, const char *message) {
    if (!good) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange value = {0};
    value.start = start;
    value.count = count;
    return value;
}

typedef struct SZrInvokeFixture {
    TZrBool threw;
    TZrBool reject;
    TZrBool undefined;
    TZrUInt32 calls;
} SZrInvokeFixture;

static TZrBool oracle_invoke(void *context, const SZrExecIrInstruction *instruction,
                            const SZrExecIrOracleValue *operands, TZrUInt32 count,
                            SZrExecIrOracleValue *result, TZrBool *threw) {
    SZrInvokeFixture *fixture = (SZrInvokeFixture *)context;
    (void)instruction;
    check(count == 1u && operands[0].as.signedInteger == 17,
          "oracle INVOKE operands changed");
    ++fixture->calls;
    if (fixture->reject) return ZR_FALSE;
    *threw = fixture->threw;
    result->kind = fixture->undefined ? ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED
                                      : ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = 31;
    return ZR_TRUE;
}

static TZrBool projected_invoke(void *context, const SZrExecBcInstruction *instruction,
                               const SZrExecIrOracleValue *operands, TZrUInt32 count,
                               SZrExecIrOracleValue *result, TZrBool *threw) {
    SZrInvokeFixture *fixture = (SZrInvokeFixture *)context;
    (void)instruction;
    check(count == 1u && operands[0].as.signedInteger == 17,
          "projected INVOKE operands changed");
    ++fixture->calls;
    if (fixture->reject) return ZR_FALSE;
    *threw = fixture->threw;
    result->kind = fixture->undefined ? ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED
                                      : ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = 31;
    return ZR_TRUE;
}

static TZrBool oracle_payload(void *context, const SZrExecIrInstruction *instruction,
                             SZrExecIrOracleValue *result) {
    TZrUInt32 *calls = (TZrUInt32 *)context;
    (void)instruction;
    ++*calls;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = -23;
    return ZR_TRUE;
}

static TZrBool projected_payload(void *context, const SZrExecBcInstruction *instruction,
                                SZrExecIrOracleValue *result) {
    TZrUInt32 *calls = (TZrUInt32 *)context;
    (void)instruction;
    ++*calls;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = -23;
    return ZR_TRUE;
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
    if (opcode == ZR_EXEC_IR_OPCODE_INVOKE) {
        TZrExecIrMemoryTokenId before[2] = {
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)};
        TZrExecIrMemoryTokenId after[2] = {
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
            ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)};
        instruction.flags = ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
        instruction.effectIn = 1u;
        instruction.effectOut = 2u;
        check(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, before, 2u,
                                                        &instruction.memoryIn) &&
              ZrCore_ExecIr_FunctionAppendMemoryTokens(function, after, 2u,
                                                        &instruction.memoryOut),
              "could not append INVOKE heap/native memory tokens");
    }
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, ZR_NULL),
          "could not append INVOKE fixture instruction");
}

static void build(SZrExecIrFunction *function) {
    TZrExecIrValueId input, output, payload, successors[2] = {2u, 3u}, predecessor = 1u;
    SZrExecIrRange callOperands = {0}, callResult = {0}, payloadResult = {0};
    SZrExecIrRange normalReturn = {0}, errorReturn = {0};
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 99u;
    input = ZrCore_ExecIr_FunctionAddValue(function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    output = ZrCore_ExecIr_FunctionAddValue(function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                             ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    payload = ZrCore_ExecIr_FunctionAddValue(function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                              ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(input == 1u && output == 2u && payload == 3u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
          ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u &&
          ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 3u,
          "could not create INVOKE CFG");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                                                 &function->blocks[0].successorRange) &&
          ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                                                   &function->blocks[1].predecessorRange) &&
          ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                                                   &function->blocks[2].predecessorRange) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &input, 1u, &callOperands) &&
          ZrCore_ExecIr_FunctionAppendResults(function, &output, 1u, &callResult) &&
          ZrCore_ExecIr_FunctionAppendResults(function, &payload, 1u, &payloadResult) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &output, 1u, &normalReturn) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, &payload, 1u, &errorReturn),
          "could not create INVOKE operands");
    function->values[0].flags |= ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    append(function, ZR_EXEC_IR_OPCODE_INVOKE, callOperands, callResult,
           function->blocks[0].successorRange, 991u);
    append(function, ZR_EXEC_IR_OPCODE_RETURN, normalReturn, range(0u, 0u),
           range(0u, 0u), 992u);
    append(function, ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD, range(0u, 0u),
           payloadResult, range(0u, 0u), 993u);
    append(function, ZR_EXEC_IR_OPCODE_RETURN, errorReturn, range(0u, 0u),
           range(0u, 0u), 994u);
    function->blocks[0].instructionRange = range(0u, 1u);
    function->blocks[0].terminatorInstructionId = 1u;
    function->blocks[1].instructionRange = range(1u, 1u);
    function->blocks[1].terminatorInstructionId = 2u;
    function->blocks[2].instructionRange = range(2u, 2u);
    function->blocks[2].terminatorInstructionId = 4u;
    function->sourceMaps = (SZrExecIrSourceMap *)calloc(1u, sizeof(*function->sourceMaps));
    check(function->sourceMaps != NULL, "could not allocate INVOKE source map");
    function->sourceMapCount = function->sourceMapCapacity = 1u;
    function->sourceMaps[0].sourceId = 991u;
    function->sourceMaps[0].instructionId = 1u;
    function->sourceMaps[0].startOffset = 14u;
    function->sourceMaps[0].endOffset = 25u;
    function->sourceMaps[0].startLine = 3u;
    function->sourceMaps[0].startColumn = 5u;
    function->sourceMaps[0].endLine = 3u;
    function->sourceMaps[0].endColumn = 16u;
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        fprintf(stderr, "INVOKE verifier: code=%u instruction=%u source=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.sourceId);
        check(ZR_FALSE, "INVOKE fixture must pass full verification");
    }
}

void test_oracle_execbc_invoke_differential(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrAotIrProjection aot = {0};
    SZrExecIrOracleInput directInput = {0};
    SZrExecBcExecutionInput projectedInput = {0};
    SZrExecIrOracleExecutionResult direct;
    SZrExecBcExecutionResult projected;
    SZrExecIrOracleValue initial = {0};
    SZrInvokeFixture oracle = {0}, execbc = {0};
    SZrExecIrDiagnostic diagnostic;
    SZrSsaObservation expected = {0}, actual = {0};
    SZrSsaDiffDiagnostic difference;
    TZrUInt32 directPayloadCalls = 0u, projectedPayloadCalls = 0u;
    build(&function);
    check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
          projection.runnable, "verified INVOKE projection must be runnable");
    check(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic) && !aot.runnable,
          "verified INVOKE AOTIR projection must retain its non-runnable boundary");
    check(projection.sourceMapCount == 1u && aot.sourceMapCount == 1u &&
          projection.sourceMaps[0].pc == 0u && aot.sourceMaps[0].pc == 0u &&
          projection.sourceMaps[0].startOffset == 14u &&
          projection.sourceMaps[0].endOffset == 25u &&
          aot.sourceMaps[0].startLine == 3u && aot.sourceMaps[0].startColumn == 5u &&
          aot.sourceMaps[0].endLine == 3u && aot.sourceMaps[0].endColumn == 16u,
          "INVOKE projections lost the full source span");
    function.sourceMaps[0].startLine = 99u;
    check(projection.sourceMaps[0].startLine == 3u &&
          aot.sourceMaps[0].startLine == 3u,
          "projected source spans alias the input function");
    function.sourceMaps[0].startLine = 3u;
    function.sourceMaps[0].instructionId = function.instructionCount + 1u;
    check(!ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
          aot.sourceMapCount == 1u && aot.sourceMaps[0].endColumn == 16u,
          "invalid source-map PC replaced a published AOTIR span");
    function.sourceMaps[0].instructionId = 1u;
    for (TZrUInt32 block = 0u; block < function.blockCount; ++block) {
        check(projection.blocks[block].flags == function.blocks[block].flags &&
              aot.blocks[block].flags == function.blocks[block].flags,
              "INVOKE projection lost entry or exception block flags");
    }
    initial.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    initial.as.signedInteger = 17;
    directInput.function = &function;
    directInput.initialValues = &initial;
    directInput.initialValueCount = 1u;
    directInput.invoke = oracle_invoke;
    directInput.invokeUserData = &oracle;
    directInput.exceptionPayload = oracle_payload;
    directInput.exceptionPayloadUserData = &directPayloadCalls;
    projectedInput.initialValues = &initial;
    projectedInput.initialValueCount = 1u;
    projectedInput.invoke = projected_invoke;
    projectedInput.invokeUserData = &execbc;
    projectedInput.exceptionPayload = projected_payload;
    projectedInput.exceptionPayloadUserData = &projectedPayloadCalls;
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    for (TZrUInt32 path = 0u; path < 2u; ++path) {
        oracle.threw = execbc.threw = path != 0u;
        check(ZrCore_ExecIr_RunOracleEx(&directInput, &direct, &diagnostic) &&
              ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                            &projected, &diagnostic),
              "INVOKE oracle/projected path failed");
        check(direct.returned && projected.returned &&
              direct.currentBlock == path + 2u &&
              projected.currentBlock == direct.currentBlock &&
              direct.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
              projected.returnValue.kind == direct.returnValue.kind &&
              direct.returnValue.as.signedInteger == (path == 0u ? 31 : -23) &&
              projected.returnValue.as.signedInteger == direct.returnValue.as.signedInteger &&
              direct.eventCount == 1u && projected.eventCount == 1u &&
              direct.executedInstructionCount == projected.executedInstructionCount &&
              direct.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_CALL &&
              projected.events[0].kind == direct.events[0].kind &&
              projected.events[0].instructionId == direct.events[0].instructionId &&
              projected.events[0].sourceId == direct.events[0].sourceId &&
              projected.events[0].operandCount == direct.events[0].operandCount &&
              projected.events[0].operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
              projected.events[0].operands[0].as.signedInteger == 17,
              "INVOKE CFG or event diverged");
        ZrTests_Ssa_ObservationInit(&expected);
        ZrTests_Ssa_ObservationInit(&actual);
        expected.completed = actual.completed = ZR_TRUE;
        expected.resultType = actual.resultType = ZR_SSA_RESULT_INTEGER;
        expected.resultBits = (TZrUInt64)direct.returnValue.as.signedInteger;
        actual.resultBits = (TZrUInt64)projected.returnValue.as.signedInteger;
        actual.backend = 1u;
        check(ZrTests_Ssa_ObservationAppendEvent(&expected, ZR_SSA_EVENT_CALL, 991u, 17u, 1u) &&
              ZrTests_Ssa_ObservationAppendEvent(&actual, ZR_SSA_EVENT_CALL, 991u, 17u, 1u) &&
              ZrTests_Ssa_Compare(&expected, &actual, &difference),
              "INVOKE differential trace changed");
        actual.events[0].valueBits ^= 1u;
        check(!ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
              difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH,
              "INVOKE event mutation went undetected");
    }
    check(oracle.calls == 2u && execbc.calls == 2u &&
          directPayloadCalls == 1u && projectedPayloadCalls == 1u,
          "INVOKE callback counts diverged");
    projectedInput.invoke = ZR_NULL;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
          diagnostic.instructionId == 1u && diagnostic.sourceId == 991u &&
          projected.returnValue.as.signedInteger == -23,
          "missing INVOKE provider was accepted");
    projectedInput.invoke = projected_invoke;
    projectedInput.exceptionPayload = ZR_NULL;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
          diagnostic.instructionId == 3u && diagnostic.sourceId == 993u &&
          projected.returnValue.as.signedInteger == -23,
          "missing exception payload provider replaced published result");
    projectedInput.exceptionPayload = projected_payload;
    execbc.undefined = ZR_TRUE;
    check(ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                        &projected, &diagnostic) &&
          projected.currentBlock == 3u &&
          projected.returnValue.as.signedInteger == -23 &&
          projected.slots[projection.valueSlots[1]].kind ==
                  ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED,
          "exception edge must not publish the INVOKE normal result");
    execbc.undefined = ZR_FALSE;
    execbc.reject = ZR_TRUE;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_INVOKE_ERROR &&
          diagnostic.instructionId == 1u && diagnostic.sourceId == 991u &&
          projected.returnValue.as.signedInteger == -23 && projected.eventCount == 1u,
          "rejected INVOKE replaced a published result");
    execbc.reject = ZR_FALSE;
    execbc.threw = ZR_FALSE;
    execbc.undefined = ZR_TRUE;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
          diagnostic.instructionId == 1u && projected.eventCount == 1u,
          "undefined normal INVOKE result replaced a published result");
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_AotIrProjection_Free(&aot);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}
