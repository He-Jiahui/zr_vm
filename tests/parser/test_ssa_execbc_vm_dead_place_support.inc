#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_parser/exec_ir_execbc_vm.h"
#include "zr_vm_parser/exec_ir_projections.h"

void setUp(void) {}
void tearDown(void) {}

enum {
    DEAD_PLACE_SOURCE_I64_A = 401u,
    DEAD_PLACE_SOURCE_I64_B = 402u,
    DEAD_PLACE_SOURCE_BOOL = 403u,
    DEAD_PLACE_SOURCE_COPY = 404u,
    DEAD_PLACE_SOURCE_RETURN = 405u,
    DEAD_PLACE_SOURCE_CONSTANT = 406u,
    DEAD_PLACE_SOURCE_PHI = 501u,
    DEAD_PLACE_SOURCE_CONDITION = 502u,
    DEAD_PLACE_SOURCE_BRANCH = 503u,
    DEAD_PLACE_SOURCE_FALSE_VALUE = 504u,
    DEAD_PLACE_SOURCE_FALSE_BRANCH = 505u,
    DEAD_PLACE_SOURCE_MERGE_RETURN = 506u,
    DEAD_PLACE_SOURCE_TRUE_BRANCH = 507u,
    DEAD_PLACE_TRACE_CAPACITY = 32u
};

typedef enum EDeadPlaceFixtureKind {
    DEAD_PLACE_FIXTURE_UNUSED_RESULTS = 0,
    DEAD_PLACE_FIXTURE_LIVE_RESULT,
    DEAD_PLACE_FIXTURE_MIXED_EXTERNAL_USE,
    DEAD_PLACE_FIXTURE_PHI_USE
} EDeadPlaceFixtureKind;

typedef enum EDeadPlaceMutation {
    DEAD_PLACE_MUTATION_NONE = 0,
    DEAD_PLACE_MUTATION_LAYOUT,
    DEAD_PLACE_MUTATION_MALFORMED_RESULT_SHAPE,
    DEAD_PLACE_MUTATION_EFFECT,
    DEAD_PLACE_MUTATION_PLACE_FLAG_ON_SCALAR,
    DEAD_PLACE_MUTATION_UNKNOWN_VALUE_FLAG,
    DEAD_PLACE_MUTATION_EXTERNAL_PLACE_FLAGS,
    DEAD_PLACE_MUTATION_EXTERNAL_VALUE_HAS_LOCAL_RESULT,
    DEAD_PLACE_MUTATION_MEMORY_TOKEN_POOL,
    DEAD_PLACE_MUTATION_EMBEDDED_GC_MAP_WITHOUT_PRESENT
} EDeadPlaceMutation;

typedef struct SDeadPlaceOracleTrace {
    TZrExecIrSourceId sources[DEAD_PLACE_TRACE_CAPACITY];
    TZrUInt32 count;
    TZrBool valid;
} SDeadPlaceOracleTrace;

typedef struct SDeadPlaceVmTrace {
    const SZrFunction *function;
    TZrUInt32 pcs[DEAD_PLACE_TRACE_CAPACITY];
    TZrUInt32 count;
    TZrBool invalidPc;
    TZrBool overflow;
} SDeadPlaceVmTrace;

typedef struct SDeadPlaceFixture {
    SZrExecIrFunction function;
    TZrExecIrValueId externalI64;
    TZrExecIrValueId externalBool;
    TZrExecIrValueId placeValues[3];
    TZrExecIrValueId scalarResult;
    TZrExecIrInstructionId placeInstructions[3];
    TZrExecIrInstructionId scalarInstruction;
    TZrBool initialized;
} SDeadPlaceFixture;

typedef struct SDeadPlaceReport {
    TZrBool fixtureBuilt;
    TZrBool oracleReturned;
    TZrBool projectionBuilt;
    TZrBool mutationApplied;
    TZrBool materialized;
    TZrBool outputEmptyOnFailure;
    TZrBool vmReturned;
    TZrBool pcMapValid;
    TZrBool nopTraceMatchesOracle;
    TZrInt64 oracleInteger;
    TZrInt64 vmInteger;
    TZrUInt32 oraclePlaceCount;
    TZrUInt32 vmPlaceCount;
    EZrExecutionDiagnosticCode materializationCode;
} SDeadPlaceReport;

static TZrExecIrValueId dead_place_add_value(
        SZrExecIrFunction *function,
        TZrExecIrTypeToken typeToken) {
    return ZrCore_ExecIr_FunctionAddValue(
            function, typeToken, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}

static TZrBool dead_place_append_instruction(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        EZrExecIrOpcode opcode,
        TZrExecIrTypeToken typeToken,
        TZrUInt32 layoutId,
        TZrExecIrSourceId sourceId,
        const TZrExecIrValueId *operands,
        TZrUInt32 operandCount,
        const TZrExecIrValueId *results,
        TZrUInt32 resultCount,
        const TZrExecIrBlockId *successors,
        TZrUInt32 successorCount,
        TZrExecIrInstructionId *outInstructionId) {
    SZrExecIrInstruction instruction;
    SZrExecIrRange operandRange = {0u, 0u};
    SZrExecIrRange resultRange = {0u, 0u};
    SZrExecIrRange successorRange = {0u, 0u};
    TZrExecIrInstructionId instructionId = 0u;
    SZrExecIrBlock *block;

    if (function == ZR_NULL || blockId == 0u || blockId > function->blockCount ||
        (operandCount != 0u && operands == ZR_NULL) ||
        (resultCount != 0u && results == ZR_NULL) ||
        (successorCount != 0u && successors == ZR_NULL)) {
        return ZR_FALSE;
    }
    if ((operandCount != 0u &&
         !ZrCore_ExecIr_FunctionAppendOperands(
                 function, operands, operandCount, &operandRange)) ||
        (resultCount != 0u &&
         !ZrCore_ExecIr_FunctionAppendResults(
                 function, results, resultCount, &resultRange)) ||
        (successorCount != 0u &&
         !ZrCore_ExecIr_FunctionAppendSuccessors(
                 function, successors, successorCount, &successorRange))) {
        return ZR_FALSE;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operandRange;
    instruction.results = resultRange;
    instruction.successorRange = successorRange;
    instruction.typeToken = typeToken;
    instruction.layoutId = layoutId;
    instruction.sourceId = sourceId;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(
                function, &instruction, &instructionId)) {
        return ZR_FALSE;
    }

    block = &function->blocks[blockId - 1u];
    if (block->instructionRange.count == 0u) {
        block->instructionRange.start = instructionId - 1u;
    }
    ++block->instructionRange.count;
    if (successorCount != 0u) {
        block->successorRange = successorRange;
    }
    if (opcode == ZR_EXEC_IR_OPCODE_BRANCH ||
        opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
        opcode == ZR_EXEC_IR_OPCODE_RETURN) {
        block->terminatorInstructionId = instructionId;
    }
    if (outInstructionId != ZR_NULL) {
        *outInstructionId = instructionId;
    }
    return ZR_TRUE;
}

static TZrBool dead_place_append_predecessors(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        const TZrExecIrBlockId *predecessors,
        TZrUInt32 predecessorCount) {
    if (function == ZR_NULL || blockId == 0u || blockId > function->blockCount ||
        (predecessorCount != 0u && predecessors == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (predecessorCount == 0u) {
        function->blocks[blockId - 1u].predecessorRange =
                (SZrExecIrRange){0u, 0u};
        return ZR_TRUE;
    }
    return ZrCore_ExecIr_FunctionAppendPredecessors(
            function, predecessors, predecessorCount,
            &function->blocks[blockId - 1u].predecessorRange);
}

static TZrBool dead_place_add_source_maps(SZrExecIrFunction *function) {
    TZrUInt32 index;
    if (function == ZR_NULL || function->instructionCount == 0u) {
        return ZR_FALSE;
    }
    function->sourceMaps = (SZrExecIrSourceMap *)calloc(
            function->instructionCount, sizeof(*function->sourceMaps));
    if (function->sourceMaps == ZR_NULL) {
        return ZR_FALSE;
    }
    function->sourceMapCount = function->instructionCount;
    function->sourceMapCapacity = function->instructionCount;
    for (index = 0u; index < function->instructionCount; ++index) {
        function->sourceMaps[index].sourceId =
                function->instructions[index].sourceId;
        function->sourceMaps[index].instructionId = index + 1u;
    }
    return ZR_TRUE;
}

static TZrBool dead_place_initialize_function(SZrExecIrFunction *function) {
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 0xd301u;
    function->signatureHash = 0xd3010001u;
    return ZR_TRUE;
}

static TZrBool dead_place_build_simple_fixture(
        SDeadPlaceFixture *fixture,
        EDeadPlaceFixtureKind kind) {
    TZrExecIrBlockId entry;
    TZrExecIrValueId copyResult = ZR_EXEC_IR_VALUE_ID_INVALID;
    TZrExecIrValueId returnOperand;
    TZrUInt32 index;
    const TZrUInt32 placeFlags =
            ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
            ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE;

    if (fixture == ZR_NULL || !dead_place_initialize_function(&fixture->function)) {
        return ZR_FALSE;
    }
    fixture->initialized = ZR_TRUE;
    entry = ZrCore_ExecIr_FunctionAddBlock(
            &fixture->function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    fixture->externalI64 = ZrCore_ExecIr_FunctionAddExternalValue(
            &fixture->function, ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    fixture->externalBool = ZrCore_ExecIr_FunctionAddExternalValue(
            &fixture->function, ZR_VALUE_TYPE_BOOL,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    fixture->placeValues[0] = dead_place_add_value(
            &fixture->function, ZR_VALUE_TYPE_INT64);
    fixture->placeValues[1] = dead_place_add_value(
            &fixture->function, ZR_VALUE_TYPE_INT64);
    fixture->placeValues[2] = dead_place_add_value(
            &fixture->function, ZR_VALUE_TYPE_BOOL);
    fixture->scalarResult = dead_place_add_value(
            &fixture->function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || fixture->externalI64 == 0u ||
        fixture->externalBool == 0u || fixture->placeValues[0] == 0u ||
        fixture->placeValues[1] == 0u || fixture->placeValues[2] == 0u ||
        fixture->scalarResult == 0u) {
        return ZR_FALSE;
    }
    fixture->function.entryBlockId = entry;
    for (index = 0u; index < 3u; ++index) {
        fixture->function.values[fixture->placeValues[index] - 1u].flags =
                index == 2u ? ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS : placeFlags;
    }

    if (!dead_place_append_instruction(
                &fixture->function, entry, ZR_EXEC_IR_OPCODE_PLACE_BASE,
                ZR_VALUE_TYPE_INT64, 0u, DEAD_PLACE_SOURCE_I64_A,
                &fixture->externalI64, 1u, &fixture->placeValues[0], 1u,
                ZR_NULL, 0u, &fixture->placeInstructions[0]) ||
        !dead_place_append_instruction(
                &fixture->function, entry, ZR_EXEC_IR_OPCODE_PLACE_BASE,
                ZR_VALUE_TYPE_INT64, 0u, DEAD_PLACE_SOURCE_I64_B,
                &fixture->externalI64, 1u, &fixture->placeValues[1], 1u,
                ZR_NULL, 0u, &fixture->placeInstructions[1]) ||
        !dead_place_append_instruction(
                &fixture->function, entry, ZR_EXEC_IR_OPCODE_PLACE_BASE,
                ZR_VALUE_TYPE_BOOL, 0u, DEAD_PLACE_SOURCE_BOOL,
                &fixture->externalBool, 1u, &fixture->placeValues[2], 1u,
                ZR_NULL, 0u, &fixture->placeInstructions[2])) {
        return ZR_FALSE;
    }

    if (kind == DEAD_PLACE_FIXTURE_MIXED_EXTERNAL_USE) {
        copyResult = dead_place_add_value(
                &fixture->function, ZR_VALUE_TYPE_INT64);
        if (copyResult == 0u ||
            !dead_place_append_instruction(
                    &fixture->function, entry, ZR_EXEC_IR_OPCODE_COPY,
                    ZR_VALUE_TYPE_INT64, 0u, DEAD_PLACE_SOURCE_COPY,
                    &fixture->externalI64, 1u, &copyResult, 1u,
                    ZR_NULL, 0u, ZR_NULL)) {
            return ZR_FALSE;
        }
    }

    returnOperand = kind == DEAD_PLACE_FIXTURE_LIVE_RESULT
            ? fixture->placeValues[0]
            : fixture->scalarResult;
    if (!dead_place_append_instruction(
                &fixture->function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                ZR_VALUE_TYPE_INT64, 42u, DEAD_PLACE_SOURCE_CONSTANT,
                ZR_NULL, 0u, &fixture->scalarResult, 1u,
                ZR_NULL, 0u, &fixture->scalarInstruction)) {
        return ZR_FALSE;
    }
    if (!dead_place_append_instruction(
                &fixture->function, entry, ZR_EXEC_IR_OPCODE_RETURN,
                0u, 0u, DEAD_PLACE_SOURCE_RETURN,
                &returnOperand, 1u, ZR_NULL, 0u,
                ZR_NULL, 0u, ZR_NULL) ||
        !dead_place_add_source_maps(&fixture->function)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool dead_place_build_phi_fixture(SDeadPlaceFixture *fixture) {
    SZrExecIrFunction *function;
    TZrExecIrBlockId entry, trueBlock, falseBlock, mergeBlock;
    TZrExecIrBlockId branchTargets[2], predecessor[2];
    TZrExecIrValueId condition, falseValue, merged, placeResult;
    TZrExecIrValueId conditionOperand, falseReturnOperand, mergedOperand;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    SZrExecIrRange incomingRange;

    if (fixture == ZR_NULL || !dead_place_initialize_function(&fixture->function)) {
        return ZR_FALSE;
    }
    fixture->initialized = ZR_TRUE;
    function = &fixture->function;
    entry = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    trueBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    falseBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    mergeBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    fixture->externalI64 = ZrCore_ExecIr_FunctionAddExternalValue(
            function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    placeResult = dead_place_add_value(function, ZR_VALUE_TYPE_INT64);
    condition = dead_place_add_value(function, ZR_VALUE_TYPE_BOOL);
    falseValue = dead_place_add_value(function, ZR_VALUE_TYPE_INT64);
    merged = dead_place_add_value(function, ZR_VALUE_TYPE_INT64);
    if (entry != 1u || trueBlock != 2u || falseBlock != 3u ||
        mergeBlock != 4u || fixture->externalI64 == 0u || placeResult == 0u ||
        condition == 0u || falseValue == 0u || merged == 0u) {
        return ZR_FALSE;
    }
    function->entryBlockId = entry;
    function->values[placeResult - 1u].flags =
            ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
            ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE;
    conditionOperand = condition;
    falseReturnOperand = falseValue;
    mergedOperand = merged;
    branchTargets[0] = trueBlock;
    branchTargets[1] = falseBlock;

    if (!dead_place_append_instruction(
                function, entry, ZR_EXEC_IR_OPCODE_PLACE_BASE,
                ZR_VALUE_TYPE_INT64, 0u, DEAD_PLACE_SOURCE_PHI,
                &fixture->externalI64, 1u, &placeResult, 1u,
                ZR_NULL, 0u, &fixture->placeInstructions[0]) ||
        !dead_place_append_instruction(
                function, entry, ZR_EXEC_IR_OPCODE_CONSTANT,
                ZR_VALUE_TYPE_BOOL, 1u, DEAD_PLACE_SOURCE_CONDITION,
                ZR_NULL, 0u, &condition, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !dead_place_append_instruction(
                function, entry, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                0u, 0u, DEAD_PLACE_SOURCE_BRANCH,
                &conditionOperand, 1u, ZR_NULL, 0u,
                branchTargets, 2u, ZR_NULL)) {
        return ZR_FALSE;
    }

    branchTargets[0] = mergeBlock;
    if (!dead_place_append_instruction(
                function, trueBlock, ZR_EXEC_IR_OPCODE_BRANCH,
                0u, 0u, DEAD_PLACE_SOURCE_TRUE_BRANCH,
                ZR_NULL, 0u, ZR_NULL, 0u, branchTargets, 1u, ZR_NULL) ||
        !dead_place_append_instruction(
                function, falseBlock, ZR_EXEC_IR_OPCODE_CONSTANT,
                ZR_VALUE_TYPE_INT64, 9u, DEAD_PLACE_SOURCE_FALSE_VALUE,
                ZR_NULL, 0u, &falseValue, 1u, ZR_NULL, 0u, ZR_NULL) ||
        !dead_place_append_instruction(
                function, falseBlock, ZR_EXEC_IR_OPCODE_BRANCH,
                0u, 0u, DEAD_PLACE_SOURCE_FALSE_BRANCH,
                ZR_NULL, 0u, ZR_NULL, 0u, branchTargets, 1u, ZR_NULL) ||
        !dead_place_append_instruction(
                function, mergeBlock, ZR_EXEC_IR_OPCODE_RETURN,
                0u, 0u, DEAD_PLACE_SOURCE_MERGE_RETURN,
                &mergedOperand, 1u, ZR_NULL, 0u, ZR_NULL, 0u, ZR_NULL)) {
        return ZR_FALSE;
    }

    predecessor[0] = entry;
    if (!dead_place_append_predecessors(function, trueBlock, predecessor, 1u) ||
        !dead_place_append_predecessors(function, falseBlock, predecessor, 1u)) {
        return ZR_FALSE;
    }
    predecessor[0] = trueBlock;
    predecessor[1] = falseBlock;
    if (!dead_place_append_predecessors(function, mergeBlock, predecessor, 2u)) {
        return ZR_FALSE;
    }

    incoming[0].predecessor = trueBlock;
    incoming[0].value = placeResult;
    incoming[1].predecessor = falseBlock;
    incoming[1].value = falseValue;
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    if (!ZrCore_ExecIr_FunctionAppendPhiIncoming(
                function, incoming, 2u, &incomingRange)) {
        return ZR_FALSE;
    }
    phi.incomings = incomingRange;
    if (!ZrCore_ExecIr_FunctionAppendPhis(
                function, &phi, 1u,
                &function->blocks[mergeBlock - 1u].phis) ||
        !dead_place_add_source_maps(function)) {
        return ZR_FALSE;
    }
    fixture->placeValues[0] = placeResult;
    return ZR_TRUE;
}

static TZrBool dead_place_oracle_place(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SDeadPlaceOracleTrace *trace = (SDeadPlaceOracleTrace *)userData;
    if (trace == ZR_NULL || instruction == ZR_NULL || result == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_PLACE_BASE ||
        operandCount != 1u || operands == ZR_NULL ||
        trace->count >= DEAD_PLACE_TRACE_CAPACITY) {
        if (trace != ZR_NULL) {
            trace->valid = ZR_FALSE;
        }
        return ZR_FALSE;
    }
    trace->sources[trace->count++] = instruction->sourceId;
    if (instruction->typeToken == (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
        result->as.boolean = ZR_TRUE;
    } else if (instruction->typeToken ==
               (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        result->as.signedInteger = (TZrInt64)instruction->sourceId;
    } else {
        trace->valid = ZR_FALSE;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrDebugSignal dead_place_record_vm_instruction(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *programCounter,
        TZrUInt32 instructionOffset,
        TZrUInt32 line,
        TZrPtr userData) {
    SDeadPlaceVmTrace *trace = (SDeadPlaceVmTrace *)userData;
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(line);
    if (trace == ZR_NULL || programCounter == ZR_NULL ||
        function != trace->function ||
        instructionOffset >= function->instructionsLength ||
        programCounter != &function->instructionsList[instructionOffset]) {
        if (trace != ZR_NULL) {
            trace->invalidPc = ZR_TRUE;
        }
        return ZR_DEBUG_SIGNAL_NONE;
    }
    if (trace->count >= DEAD_PLACE_TRACE_CAPACITY) {
        trace->overflow = ZR_TRUE;
        return ZR_DEBUG_SIGNAL_NONE;
    }
    trace->pcs[trace->count++] = instructionOffset;
    return ZR_DEBUG_SIGNAL_NONE;
}

static TZrBool dead_place_set_external_initial_values(
        const SDeadPlaceFixture *fixture,
        SZrExecIrOracleValue **outValues) {
    SZrExecIrOracleValue *values;
    if (fixture == ZR_NULL || outValues == ZR_NULL) {
        return ZR_FALSE;
    }
    *outValues = ZR_NULL;
    values = (SZrExecIrOracleValue *)calloc(
            fixture->function.valueCount, sizeof(*values));
    if (values == ZR_NULL) {
        return ZR_FALSE;
    }
    values[fixture->externalI64 - 1u].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    values[fixture->externalI64 - 1u].as.signedInteger = 17;
    if (fixture->externalBool != 0u) {
        values[fixture->externalBool - 1u].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
        values[fixture->externalBool - 1u].as.boolean = ZR_TRUE;
    }
    *outValues = values;
    return ZR_TRUE;
}

static const SZrExecBcVmPcMapEntry *dead_place_find_pc_map(
        const SZrExecBcVmEmission *emission,
        TZrUInt32 pc) {
    TZrUInt32 index;
    if (emission == ZR_NULL || emission->pcMap == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0u; index < emission->pcMapCount; ++index) {
        if (emission->pcMap[index].pc == pc) {
            return &emission->pcMap[index];
        }
    }
    return ZR_NULL;
}

static TZrBool dead_place_source_is_expected(
        TZrExecIrSourceId sourceId) {
    return (TZrBool)(sourceId == DEAD_PLACE_SOURCE_I64_A ||
                     sourceId == DEAD_PLACE_SOURCE_I64_B ||
                     sourceId == DEAD_PLACE_SOURCE_BOOL ||
                     sourceId == DEAD_PLACE_SOURCE_PHI);
}

static TZrBool dead_place_mutate_projection(
        SZrExecBcProjection *projection,
        const SDeadPlaceFixture *fixture,
        EDeadPlaceMutation mutation) {
    TZrUInt32 externalI64Slot;
    TZrUInt32 scalarSlot;
    if (projection == ZR_NULL || fixture == ZR_NULL) {
        return ZR_FALSE;
    }
    if (mutation == DEAD_PLACE_MUTATION_NONE) {
        return ZR_TRUE;
    }
    if (fixture->placeInstructions[0] != 0u) {
        SZrExecBcInstruction *place =
                &projection->instructions[fixture->placeInstructions[0] - 1u];
        if (mutation == DEAD_PLACE_MUTATION_LAYOUT) {
            place->layoutId = 1u;
        } else if (mutation == DEAD_PLACE_MUTATION_MALFORMED_RESULT_SHAPE) {
            place->results.count = 0u;
        } else if (mutation == DEAD_PLACE_MUTATION_EFFECT) {
            place->effectIn = 1u;
        }
    } else if (mutation == DEAD_PLACE_MUTATION_LAYOUT ||
               mutation == DEAD_PLACE_MUTATION_MALFORMED_RESULT_SHAPE ||
               mutation == DEAD_PLACE_MUTATION_EFFECT) {
        return ZR_FALSE;
    }
    if (fixture->externalI64 == 0u ||
        fixture->externalI64 > projection->valueSlotCount ||
        fixture->scalarResult == 0u ||
        fixture->scalarResult > projection->valueSlotCount) {
        return ZR_FALSE;
    }
    externalI64Slot = projection->valueSlots[fixture->externalI64 - 1u];
    scalarSlot = projection->valueSlots[fixture->scalarResult - 1u];
    if (mutation == DEAD_PLACE_MUTATION_PLACE_FLAG_ON_SCALAR) {
        projection->slotValues[scalarSlot].flags =
                ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS;
    } else if (mutation == DEAD_PLACE_MUTATION_UNKNOWN_VALUE_FLAG) {
        projection->slotValues[externalI64Slot].flags |= 0x80000000u;
    } else if (mutation == DEAD_PLACE_MUTATION_EXTERNAL_PLACE_FLAGS) {
        projection->slotValues[externalI64Slot].flags |=
                ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS;
    } else if (mutation ==
               DEAD_PLACE_MUTATION_EXTERNAL_VALUE_HAS_LOCAL_RESULT) {
        SZrExecBcInstruction *scalar = &projection->instructions[
                fixture->scalarInstruction - 1u];
        if (scalar->results.count != 1u) {
            return ZR_FALSE;
        }
        projection->results[scalar->results.start] = fixture->externalI64;
    } else if (mutation == DEAD_PLACE_MUTATION_MEMORY_TOKEN_POOL) {
        projection->memoryTokens = (TZrExecIrMemoryTokenId *)calloc(
                1u, sizeof(*projection->memoryTokens));
        if (projection->memoryTokens == ZR_NULL) {
            return ZR_FALSE;
        }
        projection->memoryTokens[0] = 1u;
        projection->memoryTokenCount = 1u;
    } else if (mutation ==
               DEAD_PLACE_MUTATION_EMBEDDED_GC_MAP_WITHOUT_PRESENT) {
        projection->gcMap.entries = (SZrExecIrGcMapEntry *)calloc(
                1u, sizeof(*projection->gcMap.entries));
        projection->gcMap.slotIndexPool = (TZrUInt32 *)calloc(
                1u, sizeof(*projection->gcMap.slotIndexPool));
        projection->gcMap.inlineRefOffsetPool = (TZrUInt32 *)calloc(
                1u, sizeof(*projection->gcMap.inlineRefOffsetPool));
        if (projection->gcMap.entries == ZR_NULL ||
            projection->gcMap.slotIndexPool == ZR_NULL ||
            projection->gcMap.inlineRefOffsetPool == ZR_NULL) {
            return ZR_FALSE;
        }
        projection->gcMap.entryCount = 1u;
        projection->gcMap.entryCapacity = 1u;
        projection->gcMap.entries[0].site = fixture->placeInstructions[0];
        projection->gcMap.entries[0].liveRefSlots = (SZrExecIrRange){0u, 1u};
        projection->gcMap.slotIndexCount = 1u;
        projection->gcMap.slotIndexCapacity = 1u;
        projection->gcMap.slotIndexPool[0] = projection->valueSlots[
                fixture->placeValues[0] - 1u];
        projection->gcMap.inlineRefOffsetCount = 1u;
        projection->gcMap.inlineRefOffsetCapacity = 1u;
        projection->gcMap.inlineRefOffsetPool[0] = 0u;
        projection->gcMap.safepointId = 1u;
        projection->gcMap.sourceId = DEAD_PLACE_SOURCE_I64_A;
        projection->gcMap.rootRange = (SZrExecIrRange){0u, 1u};
    }
    return ZR_TRUE;
}

static SDeadPlaceReport dead_place_run_case(
        EDeadPlaceFixtureKind fixtureKind,
        EDeadPlaceMutation mutation) {
    SDeadPlaceReport report;
    SDeadPlaceFixture fixture;
    SZrExecBcProjection projection;
    SZrExecBcVmEmission emission;
    SZrExecIrOracleInput oracleInput;
    SZrExecIrOracleExecutionResult oracleResult;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleValue *initialValues = ZR_NULL;
    SDeadPlaceOracleTrace oracleTrace;
    SDeadPlaceVmTrace vmTrace;
    SZrState *state = ZR_NULL;
    TZrBool functionRooted = ZR_FALSE;
    TZrUInt32 index;

    memset(&report, 0, sizeof(report));
    memset(&fixture, 0, sizeof(fixture));
    memset(&projection, 0, sizeof(projection));
    memset(&emission, 0, sizeof(emission));
    memset(&oracleInput, 0, sizeof(oracleInput));
    memset(&oracleResult, 0, sizeof(oracleResult));
    memset(&diagnostic, 0, sizeof(diagnostic));
    memset(&oracleTrace, 0, sizeof(oracleTrace));
    memset(&vmTrace, 0, sizeof(vmTrace));
    oracleTrace.valid = ZR_TRUE;
    ZrCore_ExecIr_OracleResultInit(&oracleResult);

    report.fixtureBuilt = fixtureKind == DEAD_PLACE_FIXTURE_PHI_USE
            ? dead_place_build_phi_fixture(&fixture)
            : dead_place_build_simple_fixture(&fixture, fixtureKind);
    if (report.fixtureBuilt == ZR_FALSE ||
        !dead_place_set_external_initial_values(&fixture, &initialValues)) {
        goto cleanup;
    }

    oracleInput.function = &fixture.function;
    oracleInput.initialValues = initialValues;
    oracleInput.initialValueCount = fixture.function.valueCount;
    oracleInput.maxSteps = 64u;
    oracleInput.place = dead_place_oracle_place;
    oracleInput.placeUserData = &oracleTrace;
    if (ZrCore_ExecIr_RunOracleEx(
                &oracleInput, &oracleResult, &diagnostic) &&
        oracleResult.returned &&
        oracleResult.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED) {
        report.oracleReturned = ZR_TRUE;
        report.oracleInteger = oracleResult.returnValue.as.signedInteger;
        report.oraclePlaceCount = oracleTrace.count;
    }
    if (report.oracleReturned == ZR_FALSE || oracleTrace.valid == ZR_FALSE ||
        !ZrParser_ExecIr_LowerExecBc(
                &fixture.function, &projection, &diagnostic)) {
        goto cleanup;
    }
    report.projectionBuilt = ZR_TRUE;
    report.mutationApplied = dead_place_mutate_projection(
            &projection, &fixture, mutation);
    if (report.mutationApplied == ZR_FALSE) {
        goto cleanup;
    }

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        goto cleanup;
    }
    if (!ZrParser_ExecBcProjection_MaterializeVmFunction(
                state, &projection, &emission, &diagnostic)) {
        report.materializationCode = diagnostic.code;
        report.outputEmptyOnFailure = (TZrBool)(
                emission.function == ZR_NULL && emission.pcMap == ZR_NULL &&
                emission.pcMapCount == 0u);
        goto cleanup;
    }
    report.materialized = ZR_TRUE;
    if (emission.function == ZR_NULL || emission.pcMap == ZR_NULL ||
        !ZrCore_GarbageCollector_IgnoreObject(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function))) {
        goto cleanup;
    }
    functionRooted = ZR_TRUE;
    vmTrace.function = emission.function;
    ZrCore_Debug_SetTraceObserver(
            state, dead_place_record_vm_instruction, &vmTrace);
    report.vmReturned = ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, emission.function, &report.vmInteger);
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    report.vmPlaceCount = 0u;
    report.pcMapValid = (TZrBool)(vmTrace.invalidPc == ZR_FALSE &&
                                  vmTrace.overflow == ZR_FALSE);
    for (index = 0u; index < vmTrace.count; ++index) {
        const SZrExecBcVmPcMapEntry *entry =
                dead_place_find_pc_map(&emission, vmTrace.pcs[index]);
        if (entry == ZR_NULL || entry->instructionId == 0u ||
            entry->pc >= emission.function->instructionsLength) {
            report.pcMapValid = ZR_FALSE;
            continue;
        }
        if (dead_place_source_is_expected(entry->sourceId)) {
            TZrUInt32 placeIndex;
            if (emission.function->instructionsList[entry->pc]
                        .instruction.operationCode !=
                (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP)) {
                report.pcMapValid = ZR_FALSE;
            }
            for (placeIndex = 0u; placeIndex < fixture.function.instructionCount;
                 ++placeIndex) {
                if (fixture.function.instructions[placeIndex].sourceId ==
                    entry->sourceId) {
                    if (entry->instructionId != placeIndex + 1u) {
                        report.pcMapValid = ZR_FALSE;
                    }
                    break;
                }
            }
            if (placeIndex == fixture.function.instructionCount) {
                report.pcMapValid = ZR_FALSE;
            }
        }
    }

    {
        TZrExecIrSourceId vmPlaceSources[DEAD_PLACE_TRACE_CAPACITY];
        TZrUInt32 traceIndex;
        for (traceIndex = 0u; traceIndex < vmTrace.count; ++traceIndex) {
            const SZrExecBcVmPcMapEntry *entry =
                    dead_place_find_pc_map(&emission, vmTrace.pcs[traceIndex]);
            if (entry != ZR_NULL &&
                dead_place_source_is_expected(entry->sourceId)) {
                if (report.vmPlaceCount < DEAD_PLACE_TRACE_CAPACITY) {
                    vmPlaceSources[report.vmPlaceCount++] = entry->sourceId;
                } else {
                    report.pcMapValid = ZR_FALSE;
                }
            }
        }
        report.nopTraceMatchesOracle = (TZrBool)(
                report.vmPlaceCount == report.oraclePlaceCount &&
                report.vmPlaceCount <= DEAD_PLACE_TRACE_CAPACITY);
        for (traceIndex = 0u;
             report.nopTraceMatchesOracle != ZR_FALSE &&
             traceIndex < report.vmPlaceCount;
             ++traceIndex) {
            if (vmPlaceSources[traceIndex] !=
                oracleTrace.sources[traceIndex]) {
                report.nopTraceMatchesOracle = ZR_FALSE;
            }
        }
    }

cleanup:
    if (state != ZR_NULL) {
        ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
        if (functionRooted != ZR_FALSE && emission.function != ZR_NULL) {
            ZrCore_GarbageCollector_UnignoreObject(
                    state->global,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(emission.function));
        }
        ZrParser_ExecBcVmEmission_Free(state, &emission);
    }
    ZrTests_Runtime_State_Destroy(state);
    ZrParser_ExecBcProjection_Free(&projection);
    ZrCore_ExecIr_OracleResultFree(&oracleResult);
    free(initialValues);
    if (fixture.initialized != ZR_FALSE) {
        ZrCore_ExecIr_FreeFunction(&fixture.function);
    }
    return report;
}
