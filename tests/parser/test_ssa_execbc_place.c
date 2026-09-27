#include "zr_vm_parser/exec_ir_execbc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SZrPlaceDifferentialFixture {
    TZrUInt32 baseCalls;
    TZrUInt32 projectCalls;
    TZrUInt32 loadCalls;
    TZrExecIrSourceId rejectSource;
    TZrExecIrSourceId undefinedSource;
    TZrInt64 expectedAddress;
} SZrPlaceDifferentialFixture;

static SZrExecIrRange place_range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange range = {start, count};
    return range;
}

static void place_append(SZrExecIrFunction *function, EZrExecIrOpcode opcode,
                         SZrExecIrRange operands, SZrExecIrRange results,
                         TZrExecIrSourceId source) {
    SZrExecIrInstruction instruction = {0};
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.sourceId = source;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                    ZR_NULL));
}

static void build_place_function(SZrExecIrFunction *function,
                                 TZrBool project) {
    TZrExecIrValueId base, offset, address, projected, loaded, pair[2];
    SZrExecIrRange operands, results;
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 91u;
    function->entryBlockId = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    assert(function->entryBlockId == 1u);
    base = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    offset = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    address = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    projected = project ? ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN) : 0u;
    loaded = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(base == 1u && offset == 2u && address == 3u &&
           (!project || projected == 4u) && loaded != 0u);
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &base, 1u,
                                                 &operands));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &address, 1u,
                                                &results));
    place_append(function, ZR_EXEC_IR_OPCODE_PLACE_BASE, operands, results,
                 801u);
    if (project) {
        pair[0] = address;
        pair[1] = offset;
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, pair, 2u,
                                                     &operands));
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &projected, 1u,
                                                    &results));
        place_append(function, ZR_EXEC_IR_OPCODE_PLACE_PROJECT, operands,
                     results, 802u);
    }
    pair[0] = project ? projected : address;
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, pair, 1u,
                                                 &operands));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &loaded, 1u,
                                                &results));
    place_append(function, ZR_EXEC_IR_OPCODE_LOAD, operands, results, 803u);
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &loaded, 1u,
                                                 &operands));
    place_append(function, ZR_EXEC_IR_OPCODE_RETURN, operands,
                 place_range(0u, 0u), 804u);
    function->blocks[0].instructionRange =
            place_range(0u, function->instructionCount);
    function->blocks[0].terminatorInstructionId = function->instructionCount;
}

static TZrBool place_value(SZrPlaceDifferentialFixture *fixture,
                           EZrExecIrOpcode opcode, TZrExecIrSourceId source,
                           const SZrExecIrOracleValue *operands,
                           TZrUInt32 count, SZrExecIrOracleValue *result) {
    assert(fixture != ZR_NULL && operands != ZR_NULL && result != ZR_NULL);
    if (opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE) {
        assert(count == 1u && source == 801u &&
               operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
               operands[0].as.signedInteger == 5);
        ++fixture->baseCalls;
        result->as.signedInteger = 7;
    } else {
        assert(opcode == ZR_EXEC_IR_OPCODE_PLACE_PROJECT && count == 2u &&
               source == 802u &&
               operands[0].as.signedInteger == 7 &&
               operands[1].as.signedInteger == 9);
        ++fixture->projectCalls;
        result->as.signedInteger = 11;
    }
    if (fixture->rejectSource == source) return ZR_FALSE;
    result->kind = fixture->undefinedSource == source
                           ? ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED
                           : ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    return ZR_TRUE;
}

static TZrBool oracle_place(void *userData,
                            const SZrExecIrInstruction *instruction,
                            const SZrExecIrOracleValue *operands,
                            TZrUInt32 count, SZrExecIrOracleValue *result) {
    return place_value((SZrPlaceDifferentialFixture *)userData,
                       (EZrExecIrOpcode)instruction->opcode,
                       instruction->sourceId, operands, count, result);
}

static TZrBool execbc_place(void *userData,
                            const SZrExecBcInstruction *instruction,
                            const SZrExecIrOracleValue *operands,
                            TZrUInt32 count, SZrExecIrOracleValue *result) {
    return place_value((SZrPlaceDifferentialFixture *)userData,
                       (EZrExecIrOpcode)instruction->opcode,
                       instruction->sourceId, operands, count, result);
}

static TZrBool memory_value(SZrPlaceDifferentialFixture *fixture,
                            EZrExecIrOracleMemoryOperation operation,
                            const SZrExecIrOracleValue *operands,
                            TZrUInt32 count, SZrExecIrOracleValue *result) {
    assert(operation == ZR_EXEC_IR_ORACLE_MEMORY_LOAD && count == 1u &&
           operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           operands[0].as.signedInteger == fixture->expectedAddress &&
           result != ZR_NULL);
    ++fixture->loadCalls;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = 99;
    return ZR_TRUE;
}

static TZrBool oracle_memory(void *userData,
                             const SZrExecIrInstruction *instruction,
                             EZrExecIrOracleMemoryOperation operation,
                             const SZrExecIrOracleValue *operands,
                             TZrUInt32 count, SZrExecIrOracleValue *result) {
    assert(instruction->sourceId == 803u);
    return memory_value((SZrPlaceDifferentialFixture *)userData, operation,
                        operands, count, result);
}

static TZrBool execbc_memory(void *userData,
                             const SZrExecBcInstruction *instruction,
                             EZrExecIrOracleMemoryOperation operation,
                             const SZrExecIrOracleValue *operands,
                             TZrUInt32 count, SZrExecIrOracleValue *result) {
    assert(instruction->sourceId == 803u);
    return memory_value((SZrPlaceDifferentialFixture *)userData, operation,
                        operands, count, result);
}

void test_ssa_execbc_place_differential(void) {
    for (TZrUInt32 projected = 0u; projected < 2u; ++projected) {
        SZrExecIrFunction function;
        SZrExecBcProjection bc = {0};
        SZrExecIrDiagnostic diagnostic;
        SZrExecIrOracleInput oracleInput = {0};
        SZrExecBcExecutionInput bcInput = {0};
        SZrExecIrOracleExecutionResult oracleResult = {0};
        SZrExecBcExecutionResult bcResult;
        SZrPlaceDifferentialFixture oracleFixture = {0}, bcFixture = {0};
        SZrExecIrOracleValue initial[2] = {
                {.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                 .as.signedInteger = 5},
                {.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
                 .as.signedInteger = 9}};
        TZrExecIrSourceId badSource = projected ? 802u : 801u;

        build_place_function(&function, projected != 0u);
        if (!ZrCore_ExecIr_VerifyFunction(
                    &function,
                    (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                            ZR_EXEC_IR_VERIFY_SSA),
                    &diagnostic)) {
            fprintf(stderr, "Place fixture verifier code=%u block=%u instruction=%u\n",
                    (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                    (unsigned)diagnostic.instructionId);
            exit(EXIT_FAILURE);
        }
        oracleFixture.expectedAddress = projected ? 11 : 7;
        bcFixture.expectedAddress = oracleFixture.expectedAddress;
        oracleInput.function = &function;
        oracleInput.initialValues = initial;
        oracleInput.initialValueCount = 2u;
        oracleInput.place = oracle_place;
        oracleInput.placeUserData = &oracleFixture;
        oracleInput.memory = oracle_memory;
        oracleInput.memoryUserData = &oracleFixture;
        assert(ZrCore_ExecIr_RunOracleEx(&oracleInput, &oracleResult,
                                          &diagnostic));
        assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic) &&
               bc.runnable);
        bcInput.initialValues = initial;
        bcInput.initialValueCount = 2u;
        bcInput.place = execbc_place;
        bcInput.placeUserData = &bcFixture;
        bcInput.memory = execbc_memory;
        bcInput.memoryUserData = &bcFixture;
        ZrParser_ExecBcExecutionResult_Init(&bcResult);
        assert(ZrParser_ExecBcProjection_Run(&bc, &bcInput, &bcResult,
                                              &diagnostic));
        assert(oracleResult.returned && bcResult.returned &&
               oracleResult.returnValue.kind == bcResult.returnValue.kind &&
               oracleResult.returnValue.as.signedInteger == 99 &&
               bcResult.returnValue.as.signedInteger == 99 &&
               oracleFixture.baseCalls == bcFixture.baseCalls &&
               oracleFixture.projectCalls == bcFixture.projectCalls &&
               oracleFixture.loadCalls == bcFixture.loadCalls &&
               oracleResult.eventCount == 1u && bcResult.eventCount == 1u &&
               oracleResult.events[0].kind == bcResult.events[0].kind &&
               oracleResult.events[0].sourceId == bcResult.events[0].sourceId &&
               bcResult.events[0].operands[0].as.signedInteger ==
                       bcFixture.expectedAddress);

        bcInput.place = ZR_NULL;
        assert(!ZrParser_ExecBcProjection_Run(&bc, &bcInput, &bcResult,
                                               &diagnostic) &&
               diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
               diagnostic.instructionId == 1u &&
               diagnostic.sourceId == 801u && bcResult.returned);
        bcInput.place = execbc_place;
        bcFixture.rejectSource = badSource;
        assert(!ZrParser_ExecBcProjection_Run(&bc, &bcInput, &bcResult,
                                               &diagnostic) &&
               diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_PLACE_ERROR &&
               diagnostic.sourceId == badSource && bcResult.returned);
        bcFixture.rejectSource = 0u;
        bcFixture.undefinedSource = badSource;
        assert(!ZrParser_ExecBcProjection_Run(&bc, &bcInput, &bcResult,
                                               &diagnostic) &&
               diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
               diagnostic.sourceId == badSource && bcResult.returned);
        ZrParser_ExecBcExecutionResult_Free(&bcResult);
        ZrCore_ExecIr_OracleResultFree(&oracleResult);
        ZrParser_ExecBcProjection_Free(&bc);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}
