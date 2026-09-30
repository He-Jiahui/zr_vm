#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct SZrCallFixture {
    TZrUInt32 count;
    TZrUInt32 expectedArguments;
    TZrBool reject;
    TZrBool invalidResult;
} SZrCallFixture;

void test_oracle_execbc_call_differential(void);

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

static void append(SZrExecIrFunction *function, EZrExecIrOpcode opcode,
                   SZrExecIrRange operands, SZrExecIrRange results,
                   TZrUInt32 layout, TZrExecIrSourceId source) {
    SZrExecIrInstruction instruction = {0};
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.layoutId = layout;
    instruction.sourceId = source;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, ZR_NULL),
          "could not append call instruction");
}

static void build_function(SZrExecIrFunction *function, TZrUInt32 argumentCount) {
    TZrExecIrValueId arguments[9], result;
    SZrExecIrRange operands = {0}, results = {0};
    SZrExecIrRange memoryIn = {0}, memoryOut = {0};
    TZrExecIrMemoryTokenId readTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)
    };
    TZrExecIrMemoryTokenId writeTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)
    };
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 94u;
    check(ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not add call entry block");
    function->entryBlockId = 1u;
    check(argumentCount <= 9u, "too many fixture arguments");
    for (TZrUInt32 i = 0u; i < argumentCount; ++i) {
        arguments[i] = ZrCore_ExecIr_FunctionAddValue(function, 1u,
                ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        check(arguments[i] == i + 1u &&
              ZrCore_ExecIr_FunctionAppendResults(function, &arguments[i], 1u, &results),
              "could not add call argument");
        append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results,
               i + 1u, 700u + i);
    }
    result = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(result == argumentCount + 1u &&
          ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &results) &&
          ZrCore_ExecIr_FunctionAppendOperands(function, arguments,
                                               argumentCount, &operands),
          "could not add call result and operands");
    append(function, ZR_EXEC_IR_OPCODE_CALL, operands, results, 0u, 606u);
    check(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, readTokens, 2u,
                                                    &memoryIn) &&
          ZrCore_ExecIr_FunctionAppendMemoryTokens(function, writeTokens, 2u,
                                                    &memoryOut),
          "could not append call memory versions");
    function->instructions[argumentCount].memoryIn = memoryIn;
    function->instructions[argumentCount].memoryOut = memoryOut;
    function->instructions[argumentCount].flags =
            (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    function->instructions[argumentCount].effectIn = 1u;
    function->instructions[argumentCount].effectOut = 2u;
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &result, 1u, &operands),
          "could not add call return operand");
    append(function, ZR_EXEC_IR_OPCODE_RETURN, operands, range(0u, 0u), 0u, 607u);
    function->blocks[0].instructionRange = range(0u, argumentCount + 2u);
    function->blocks[0].terminatorInstructionId = argumentCount + 2u;
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic)) {
        fprintf(stderr, "call verifier: code=%u block=%u instruction=%u source=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId, (unsigned)diagnostic.sourceId);
        check(ZR_FALSE, "call fixture is not verifier-valid");
    }
}

static TZrBool call_operation(SZrCallFixture *fixture, TZrExecIrSourceId source,
                             const SZrExecIrOracleValue *operands, TZrUInt32 count,
                             SZrExecIrOracleValue *result) {
    TZrInt64 sum = 0;
    check(source == 606u && count == fixture->expectedArguments &&
          operands != ZR_NULL && result != ZR_NULL,
          "call provider lost its full argument list");
    for (TZrUInt32 i = 0u; i < count; ++i) {
        check(operands[i].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
              operands[i].as.signedInteger == (TZrInt64)i + 1,
              "call argument identity or order differs");
        sum += operands[i].as.signedInteger;
    }
    ++fixture->count;
    if (fixture->reject) return ZR_FALSE;
    result->kind = fixture->invalidResult ? ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED
                                         : ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = sum;
    return ZR_TRUE;
}

static TZrBool oracle_call(void *userData, const SZrExecIrInstruction *instruction,
                          const SZrExecIrOracleValue *operands, TZrUInt32 count,
                          SZrExecIrOracleValue *result) {
    return call_operation((SZrCallFixture *)userData, instruction->sourceId,
                          operands, count, result);
}

static TZrBool projected_call(void *userData, const SZrExecBcInstruction *instruction,
                             const SZrExecIrOracleValue *operands, TZrUInt32 count,
                             SZrExecIrOracleValue *result) {
    return call_operation((SZrCallFixture *)userData, instruction->sourceId,
                          operands, count, result);
}

static void observe(SZrSsaObservation *observation, TZrUInt32 backend,
                    const SZrExecIrOracleEvent *event,
                    const SZrExecIrOracleValue *returned) {
    ZrTests_Ssa_ObservationInit(observation);
    observation->backend = backend;
    observation->completed = ZR_TRUE;
    observation->resultType = ZR_SSA_RESULT_INTEGER;
    observation->resultBits = (TZrUInt64)returned->as.signedInteger;
    check(event->kind == ZR_EXEC_IR_ORACLE_EVENT_CALL &&
          event->instructionId == 6u && event->sourceId == 606u &&
          event->operandCount == ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT,
          "call event lost source, instruction or bounded count");
    for (TZrUInt32 i = 0u; i < event->operandCount; ++i)
        check(ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_CALL,
                    event->sourceId, (TZrUInt64)event->operands[i].as.signedInteger,
                    i), "could not record call argument event");
    check(ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_RETURN,
                607u, observation->resultBits, 0u), "could not record call return");
}

void test_oracle_execbc_call_differential(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecIrOracleInput oracleInput = {0};
    SZrExecBcExecutionInput projectedInput = {0};
    SZrExecIrOracleExecutionResult direct;
    SZrExecBcExecutionResult projected;
    SZrExecIrDiagnostic diagnostic;
    SZrCallFixture calls[2] = {{0}};
    SZrSsaObservation expected = {0}, actual = {0};
    SZrSsaDiffDiagnostic difference;
    SZrExecIrOracleEvent *publishedEvents;
    SZrExecIrOracleValue *publishedSlots;
    SZrExecIrOracleValue constants[5] = {{0}};
    SZrExecIrOracleValue initial = {0};

    build_function(&function, 5u);
    calls[0].expectedArguments = 5u;
    calls[1].expectedArguments = 5u;
    check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
          projection.runnable, "call projection is not runnable");
    oracleInput.function = &function;
    oracleInput.call = oracle_call;
    oracleInput.userData = &calls[0];
    projectedInput.call = projected_call;
    projectedInput.callUserData = &calls[1];
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    check(ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
          ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                        &projected, &diagnostic),
          "call did not run on oracle and projection");
    check(calls[0].count == 1u && calls[1].count == 1u &&
          direct.returned && projected.returned && direct.eventCount == 1u &&
          projected.eventCount == 1u && projected.returnSourceId == 607u,
          "call execution count, event, or return source differs");
    observe(&expected, 0u, direct.events, &direct.returnValue);
    observe(&actual, 1u, projected.events, &projected.returnValue);
    check(ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          actual.resultBits == 15u && actual.eventCount == 5u,
          "call arguments, events or result differ");
    actual.events[3].valueBits = 99u;
    check(!ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
          difference.eventIndex == 3u, "call snapshot corruption was not detected");

    publishedEvents = projected.events;
    publishedSlots = projected.slots;
    for (TZrUInt32 i = 0u; i < 5u; ++i) {
        constants[i].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        constants[i].as.signedInteger = (TZrInt64)i + 1;
    }
    oracleInput.constants = constants;
    oracleInput.constantCount = 5u;
    projectedInput.constants = constants;
    projectedInput.constantCount = 5u;
    constants[4].kind = ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT;
    check(!ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
          diagnostic.instructionId == 0u && calls[0].count == 1u,
          "oracle accepted an invalid constant kind");
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
          diagnostic.instructionId == 0u && calls[1].count == 1u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "projection passed invalid constant kind to the call provider");
    constants[4].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    initial.kind = ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT;
    oracleInput.initialValues = &initial;
    oracleInput.initialValueCount = 1u;
    projectedInput.initialValues = &initial;
    projectedInput.initialValueCount = 1u;
    check(!ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
          calls[0].count == 1u,
          "oracle accepted an invalid initial value kind");
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
          calls[1].count == 1u && projected.events == publishedEvents &&
          projected.slots == publishedSlots,
          "projection passed invalid initial value kind to the call provider");
    oracleInput.constants = ZR_NULL;
    oracleInput.constantCount = 0u;
    oracleInput.initialValues = ZR_NULL;
    oracleInput.initialValueCount = 0u;
    projectedInput.constants = ZR_NULL;
    projectedInput.constantCount = 0u;
    projectedInput.initialValues = ZR_NULL;
    projectedInput.initialValueCount = 0u;
    projectedInput.call = ZR_NULL;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
          diagnostic.instructionId == 6u && diagnostic.sourceId == 606u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "missing call provider did not preserve previous result");
    projectedInput.call = projected_call;
    calls[1].reject = ZR_TRUE;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_CALL_ERROR &&
          diagnostic.instructionId == 6u && diagnostic.sourceId == 606u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "rejected call did not preserve result and source diagnostic");
    calls[1].reject = ZR_FALSE;
    calls[1].invalidResult = ZR_TRUE;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
          diagnostic.instructionId == 6u && diagnostic.sourceId == 606u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "undefined call result did not preserve previous result");
    calls[1].invalidResult = ZR_FALSE;
    check(ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                        &projected, &diagnostic) &&
          projected.eventCount == 1u && projected.returned &&
          projected.returnValue.as.signedInteger == 15,
          "repeated call did not publish a fresh result");
    {
        const TZrUInt64 moduleHash = UINT64_C(0x99118822);
        SZrExecIrBindingRow row;
        SZrExecIrOracleEvent *priorEvents = direct.events;
        SZrExecIrOracleValue *priorValues = direct.values;
        TZrUInt32 priorEventCount = direct.eventCount;
        TZrUInt32 priorValueCount = direct.valueCount;
        TZrBool priorReturned = direct.returned;
        function.contract.moduleHash = moduleHash;
        memset(&row, 0, sizeof(row));
        row.rowIndex = 0u;
        row.instructionId = 6u;
        row.segmentIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
        row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
        row.contract.targetMetadataToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 818u);
        row.contract.signatureToken =
                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 818u);
        row.contract.signatureHash = UINT64_C(0x1818);
        row.contract.moduleSignatureHash = moduleHash;
        row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
        row.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
        row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE;
        row.location.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
        row.sourceId = 606u;
        calls[0].count = 0u;
        check(ZrCore_ExecIr_FunctionSetBindingRows(&function, &row, 1u,
                                                    &diagnostic),
              "could not install typed oracle call row");
        check(!ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
              diagnostic.instructionId == 6u && diagnostic.sourceId == 606u &&
              calls[0].count == 0u && direct.events == priorEvents &&
              direct.values == priorValues &&
              direct.eventCount == priorEventCount &&
              direct.valueCount == priorValueCount &&
              direct.returned == priorReturned,
              "typed call row fell through to the generic oracle callback");
        function.bindingRowsSchemaVersion = 2u;
        calls[0].count = 0u;
        check(!ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
              diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH &&
              diagnostic.instructionId == 0u && calls[0].count == 0u &&
              direct.events == priorEvents && direct.values == priorValues &&
              direct.eventCount == priorEventCount &&
              direct.valueCount == priorValueCount &&
              direct.returned == priorReturned,
              "unknown binding-row schema reached the generic oracle callback");

        function.bindingRowsSchemaVersion = ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED;
        function.instructions[5].bindingRow = ZR_EXEC_IR_BINDING_ROW_REF_NONE;
        calls[0].count = 0u;
        check(!ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
              diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
              diagnostic.instructionId == 6u && calls[0].count == 0u &&
              direct.events == priorEvents && direct.values == priorValues &&
              direct.eventCount == priorEventCount &&
              direct.valueCount == priorValueCount &&
              direct.returned == priorReturned,
              "broken typed row association reached the generic oracle callback");
        function.instructions[5].bindingRow = 1u;
    }
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);

    build_function(&function, 9u);
    check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
          projection.runnable, "wide call projection is not runnable");
    calls[0].expectedArguments = 9u;
    calls[1].expectedArguments = 9u;
    oracleInput.function = &function;
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    check(ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
          ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                        &projected, &diagnostic),
          "wide call did not run on oracle and projection");
    check(direct.returnValue.as.signedInteger == 45 &&
          projected.returnValue.as.signedInteger == 45 &&
          direct.eventCount == 1u && projected.eventCount == 1u &&
          direct.events[0].operandCount == 4u &&
          projected.events[0].operandCount == 4u &&
          direct.events[0].instructionId == 10u &&
          projected.events[0].instructionId == 10u &&
          direct.events[0].operands[3].as.signedInteger == 4 &&
          projected.events[0].operands[3].as.signedInteger == 4,
          "wide call full operands or bounded snapshots differ");
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}
