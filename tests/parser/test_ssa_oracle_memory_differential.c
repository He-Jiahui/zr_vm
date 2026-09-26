#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_execbc.h"
#include "ssa_differential_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SZrMemoryFixture {
    SZrExecIrOracleValue value;
    TZrUInt32 stores;
    TZrUInt32 loads;
    TZrBool rejectStore;
    TZrBool rejectLoad;
    TZrBool invalidLoad;
} SZrMemoryFixture;

void test_oracle_execbc_memory_differential(void);

static void check(TZrBool condition, const char *message) {
    if (!condition) {
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

static void append(SZrExecIrFunction *function, EZrExecIrOpcode opcode,
                   SZrExecIrRange operands, SZrExecIrRange results,
                   TZrUInt32 literal, TZrExecIrSourceId source) {
    SZrExecIrInstruction instruction = {0};
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.layoutId = literal;
    instruction.sourceId = source;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, ZR_NULL),
          "could not append memory instruction");
}

static void build_memory_function(SZrExecIrFunction *function, TZrBool withLoad) {
    TZrExecIrValueId address, stored, loaded, storeValues[2];
    SZrExecIrRange results = {0}, storeOperands = {0};
    SZrExecIrRange loadOperands = {0}, returnOperands = {0};
    SZrExecIrRange heapVersion = {0};
    TZrExecIrMemoryTokenId token = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 93u;
    address = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    stored = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    loaded = withLoad ? ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) : 0u;
    check(address == 1u && stored == 2u &&
          loaded == (withLoad ? 3u : 0u),
          "could not allocate memory values");
    check(ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not allocate entry block");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendResults(function, &address, 1u, &results),
          "could not append address result");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 7u, 501u);
    check(ZrCore_ExecIr_FunctionAppendResults(function, &stored, 1u, &results),
          "could not append stored result");
    append(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u), results, 42u, 502u);
    storeValues[0] = address;
    storeValues[1] = stored;
    check(ZrCore_ExecIr_FunctionAppendOperands(function, storeValues, 2u,
                                                &storeOperands) &&
          (!withLoad || ZrCore_ExecIr_FunctionAppendOperands(
                            function, &address, 1u, &loadOperands)) &&
          (!withLoad || ZrCore_ExecIr_FunctionAppendResults(
                            function, &loaded, 1u, &results)) &&
          ZrCore_ExecIr_FunctionAppendOperands(function,
                                                withLoad ? &loaded : &stored, 1u,
                                                &returnOperands),
          "could not append memory operands");
    append(function, ZR_EXEC_IR_OPCODE_STORE, storeOperands, range(0u, 0u),
           0u, 503u);
    if (withLoad)
        append(function, ZR_EXEC_IR_OPCODE_LOAD, loadOperands, results, 0u, 504u);
    append(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands, range(0u, 0u),
           0u, 505u);
    check(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &token, 1u,
                                                    &heapVersion),
          "could not append managed-heap memory version");
    function->instructions[2].flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    function->instructions[2].effectIn = 1u;
    function->instructions[2].effectOut = 2u;
    function->instructions[2].memoryOut = heapVersion;
    if (withLoad) function->instructions[3].memoryIn = heapVersion;
    function->blocks[0].instructionRange = range(0u, withLoad ? 5u : 4u);
    function->blocks[0].terminatorInstructionId = withLoad ? 5u : 4u;
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                       &diagnostic)) {
        fprintf(stderr, "memory verifier: code=%u block=%u instruction=%u source=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId, (unsigned)diagnostic.sourceId);
        check(ZR_FALSE, "memory differential input is not verifier-valid");
    }
}

static TZrBool memory_operation(SZrMemoryFixture *memory, TZrExecIrSourceId source,
                               EZrExecIrOracleMemoryOperation operation,
                               const SZrExecIrOracleValue *operands,
                               TZrUInt32 count, SZrExecIrOracleValue *result) {
    if (memory == ZR_NULL || operands == ZR_NULL ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
        operands[0].as.signedInteger != 7) return ZR_FALSE;
    if (operation == ZR_EXEC_IR_ORACLE_MEMORY_STORE) {
        if (source != 503u || memory->rejectStore || count != 2u ||
            result != ZR_NULL || operands[1].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED)
            return ZR_FALSE;
        memory->value = operands[1];
        ++memory->stores;
        return ZR_TRUE;
    }
    if (source != 504u || operation != ZR_EXEC_IR_ORACLE_MEMORY_LOAD ||
        memory->rejectLoad ||
        count != 1u || result == ZR_NULL) return ZR_FALSE;
    *result = memory->value;
    if (memory->invalidLoad) result->kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
    ++memory->loads;
    return ZR_TRUE;
}

static TZrBool oracle_memory(void *userData, const SZrExecIrInstruction *instruction,
                             EZrExecIrOracleMemoryOperation operation,
                             const SZrExecIrOracleValue *operands, TZrUInt32 count,
                             SZrExecIrOracleValue *result) {
    return memory_operation((SZrMemoryFixture *)userData, instruction->sourceId,
                            operation, operands, count, result);
}

static TZrBool projected_memory(void *userData, const SZrExecBcInstruction *instruction,
                                EZrExecIrOracleMemoryOperation operation,
                                const SZrExecIrOracleValue *operands, TZrUInt32 count,
                                SZrExecIrOracleValue *result) {
    return memory_operation((SZrMemoryFixture *)userData, instruction->sourceId,
                            operation, operands, count, result);
}

static void observe(SZrSsaObservation *observation, TZrUInt32 backend,
                    const SZrExecIrOracleEvent *events, TZrUInt32 count,
                    const SZrExecIrOracleValue *returned,
                    TZrExecIrSourceId returnSource) {
    ZrTests_Ssa_ObservationInit(observation);
    observation->backend = backend;
    observation->completed = ZR_TRUE;
    observation->resultType = ZR_SSA_RESULT_INTEGER;
    check(returned->kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
          "memory result is not signed");
    observation->resultBits = (TZrUInt64)returned->as.signedInteger;
    for (TZrUInt32 index = 0u; index < count; ++index) {
        const SZrExecIrOracleEvent *event = &events[index];
        EZrSsaEventKind kind;
        TZrUInt64 value, auxiliary;
        check(event->operandCount == (index == 0u ? 2u : 1u) &&
              event->operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
              "memory event lost its operands");
        if (event->kind == ZR_EXEC_IR_ORACLE_EVENT_STORE) {
            kind = ZR_SSA_EVENT_WRITE;
            check(event->operands[1].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                  "store event lost its value");
            value = (TZrUInt64)event->operands[1].as.signedInteger;
            auxiliary = (TZrUInt64)event->operands[0].as.signedInteger;
        } else {
            check(event->kind == ZR_EXEC_IR_ORACLE_EVENT_LOAD,
                  "unexpected memory event");
            kind = ZR_SSA_EVENT_GET;
            value = (TZrUInt64)event->operands[0].as.signedInteger;
            auxiliary = (TZrUInt64)event->instructionId;
        }
        check(ZrTests_Ssa_ObservationAppendEvent(observation, kind,
                    event->sourceId, value, auxiliary),
              "could not record memory event");
    }
    check(ZrTests_Ssa_ObservationAppendEvent(observation, ZR_SSA_EVENT_RETURN,
                returnSource, observation->resultBits, 0u),
          "could not record memory return");
}

static void test_store_event_only(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecIrOracleInput oracleInput = {0};
    SZrExecBcExecutionInput projectedInput = {0};
    SZrExecIrOracleExecutionResult direct;
    SZrExecBcExecutionResult projected;
    SZrExecIrDiagnostic diagnostic;
    SZrSsaObservation expected = {0}, actual = {0};
    SZrSsaDiffDiagnostic difference;

    build_memory_function(&function, ZR_FALSE);
    check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
          projection.runnable, "event-only STORE did not lower as runnable");
    oracleInput.function = &function;
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    check(ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic) &&
          ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                        &projected, &diagnostic),
          "provider-free STORE did not execute on both backends");
    check(direct.returned && projected.returned && direct.eventCount == 1u &&
          projected.eventCount == 1u && direct.currentBlock == 1u &&
          projected.returnInstructionId == 4u,
          "provider-free STORE did not publish a memory event and return");
    observe(&expected, 0u, direct.events, direct.eventCount,
            &direct.returnValue, function.instructions[3].sourceId);
    observe(&actual, 1u, projected.events, projected.eventCount,
            &projected.returnValue, projected.returnSourceId);
    check(ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          actual.eventCount == 2u && actual.events[0].kind == ZR_SSA_EVENT_WRITE &&
          actual.events[0].sourceId == 503u && actual.events[0].valueBits == 42u &&
          actual.events[0].auxiliary == 7u && actual.resultBits == 42u,
          "event-only STORE lost its address, value, or ordering");
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
}

void test_oracle_execbc_memory_differential(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection projection = {0};
    SZrExecIrOracleInput oracleInput = {0};
    SZrExecBcExecutionInput projectedInput = {0};
    SZrExecIrOracleExecutionResult direct;
    SZrExecBcExecutionResult projected;
    SZrExecIrDiagnostic diagnostic;
    SZrMemoryFixture memories[2];
    SZrSsaObservation expected = {0}, actual = {0};
    SZrSsaDiffDiagnostic difference;
    SZrSsaCoverage coverage;
    SZrExecIrOracleEvent *publishedEvents;
    SZrExecIrOracleValue *publishedSlots;

    memset(memories, 0, sizeof(memories));
    build_memory_function(&function, ZR_TRUE);
    check(ZrParser_ExecIr_LowerExecBc(&function, &projection, &diagnostic) &&
          projection.runnable && projection.memoryTokenCount == 1u &&
          projection.memoryTokens[0] == function.memoryTokenPool[0],
          "memory projection lost its runnable token chain");
    oracleInput.function = &function;
    oracleInput.memory = oracle_memory;
    oracleInput.memoryUserData = &memories[0];
    projectedInput.memory = projected_memory;
    projectedInput.memoryUserData = &memories[1];
    ZrCore_ExecIr_OracleResultInit(&direct);
    ZrParser_ExecBcExecutionResult_Init(&projected);
    check(ZrCore_ExecIr_RunOracleEx(&oracleInput, &direct, &diagnostic),
          "oracle memory run failed");
    check(ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                        &projected, &diagnostic),
          "ExecBC memory run failed");
    check(direct.returned && projected.returned && direct.eventCount == 2u &&
          projected.eventCount == 2u && memories[0].stores == 1u &&
          memories[1].stores == 1u && memories[0].loads == 1u &&
          memories[1].loads == 1u && memories[0].value.as.signedInteger == 42 &&
          memories[1].value.as.signedInteger == 42,
          "memory effect counts or caller-owned values differ");
    observe(&expected, 0u, direct.events, direct.eventCount,
            &direct.returnValue,
            function.instructions[function.blocks[direct.currentBlock - 1u]
                .terminatorInstructionId - 1u].sourceId);
    observe(&actual, 1u, projected.events, projected.eventCount,
            &projected.returnValue, projected.returnSourceId);
    check(ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          actual.eventCount == 3u &&
          actual.events[0].kind == ZR_SSA_EVENT_WRITE &&
          actual.events[0].sourceId == 503u && actual.events[0].valueBits == 42u &&
          actual.events[0].auxiliary == 7u &&
          actual.events[1].kind == ZR_SSA_EVENT_GET &&
          actual.events[1].sourceId == 504u && actual.events[1].valueBits == 7u &&
          actual.events[1].auxiliary == 4u &&
          direct.events[0].instructionId == 3u &&
          projected.events[0].instructionId == 3u &&
          projected.events[1].instructionId == 4u &&
          actual.events[2].sourceId == 505u && actual.resultBits == 42u,
          "real memory events or return value differ");
    ZrTests_Ssa_CoverageInit(&coverage, 3u);
    ZrTests_Ssa_CoverageRecord(&coverage, 0u, &expected, ZR_TRUE);
    ZrTests_Ssa_CoverageRecord(&coverage, 1u, &actual, ZR_TRUE);
    check(ZrTests_Ssa_CoverageComplete(&coverage), "memory backends not covered");
    actual.events[0].auxiliary = 8u;
    check(!ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
          difference.eventIndex == 0u,
          "store address snapshot corruption was not detected");
    actual.events[0].auxiliary = 7u;
    actual.events[0].kind = ZR_SSA_EVENT_GET;
    check(!ZrTests_Ssa_Compare(&expected, &actual, &difference) &&
          difference.reason == ZR_SSA_DIFF_EVENT_MISMATCH &&
          difference.eventIndex == 0u,
          "event order corruption was not detected");

    publishedEvents = projected.events;
    publishedSlots = projected.slots;
    projectedInput.memory = ZR_NULL;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
          diagnostic.instructionId == 4u && diagnostic.sourceId == 504u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "missing load provider did not fail without replacing result");
    projectedInput.memory = projected_memory;
    memories[1].rejectStore = ZR_TRUE;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR &&
          diagnostic.instructionId == 3u && diagnostic.sourceId == 503u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "rejected store did not preserve result and source diagnostic");
    memories[1].rejectStore = ZR_FALSE;
    memories[1].rejectLoad = ZR_TRUE;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR &&
          diagnostic.instructionId == 4u && diagnostic.sourceId == 504u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "rejected load did not preserve result and source diagnostic");
    memories[1].rejectLoad = ZR_FALSE;
    memories[1].invalidLoad = ZR_TRUE;
    check(!ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                         &projected, &diagnostic) &&
          diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
          diagnostic.instructionId == 4u && diagnostic.sourceId == 504u &&
          projected.events == publishedEvents && projected.slots == publishedSlots,
          "undefined load did not preserve result and source diagnostic");
    memories[1].invalidLoad = ZR_FALSE;
    check(ZrParser_ExecBcProjection_Run(&projection, &projectedInput,
                                        &projected, &diagnostic) &&
          projected.returned && projected.eventCount == 2u &&
          projected.returnValue.as.signedInteger == 42,
          "repeated memory execution did not publish a fresh result");
    ZrParser_ExecBcExecutionResult_Free(&projected);
    ZrCore_ExecIr_OracleResultFree(&direct);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_FreeFunction(&function);
    test_store_event_only();
}
