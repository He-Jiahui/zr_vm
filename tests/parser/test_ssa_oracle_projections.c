#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/exec_ir_oracle.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange result;
    result.start = start;
    result.count = count;
    return result;
}

static TZrExecIrInstructionId append_instruction(
        SZrExecIrFunction *function, EZrExecIrOpcode opcode,
        SZrExecIrRange operands, SZrExecIrRange results,
        SZrExecIrRange successors, TZrUInt32 layoutId,
        TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.successorRange = successors;
    instruction.layoutId = layoutId;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
    return id;
}

static void build_scalar_function(SZrExecIrFunction *function) {
    TZrExecIrValueId left, right, sum;
    SZrExecIrRange leftResult, rightResult, sumResult;
    SZrExecIrRange addOperands, returnOperands;
    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    function->functionToken = 7u;
    function->signatureHash = 99u;
    left = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    right = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    sum = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(left != 0u && right != 0u && sum != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &left, 1u, &leftResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &right, 1u, &rightResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &sum, 1u, &sumResult));
    {
        TZrExecIrValueId operands[2] = {left, right};
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, operands, 2u,
                                                     &addOperands));
    }
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &sum, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       leftResult, range(0u, 0u), 2u, 101u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       rightResult, range(0u, 0u), 3u, 102u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_ADD, addOperands, sumResult,
                       range(0u, 0u), 0u, 103u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands,
                       range(0u, 0u), range(0u, 0u), 0u, 104u);
}

/* A small caller-owned memory fixture keeps LOAD/STORE replay deterministic
 * without exposing host pointers through the reference oracle. */
static void build_load_function(SZrExecIrFunction *function) {
    TZrExecIrValueId address, stored, loaded;
    SZrExecIrRange addressResult, storedResult, storeOperands;
    SZrExecIrRange loadOperands, loadResult, returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    address = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    loaded = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    stored = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(address != 0u && stored != 0u && loaded != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &address, 1u, &addressResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &stored, 1u, &storedResult));
    {
        TZrExecIrValueId operands[2] = {address, stored};
        assert(ZrCore_ExecIr_FunctionAppendOperands(
                function, operands, 2u, &storeOperands));
    }
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &address, 1u, &loadOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &loaded, 1u, &loadResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &loaded, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), addressResult, range(0u, 0u),
                       7u, 501u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), storedResult, range(0u, 0u),
                       42u, 502u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_STORE,
                       storeOperands, range(0u, 0u), range(0u, 0u),
                       0u, 503u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_LOAD,
                       loadOperands, loadResult, range(0u, 0u), 0u, 504u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 505u);
}

typedef struct SZrOracleMemoryFixture {
    TZrInt64 address;
    SZrExecIrOracleValue value;
    TZrUInt32 loadCount;
    TZrUInt32 storeCount;
    TZrBool reject;
} SZrOracleMemoryFixture;

static TZrBool oracle_memory_callback(
        void *userData, const SZrExecIrInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrOracleMemoryFixture *memory = (SZrOracleMemoryFixture *)userData;
    assert(instruction != ZR_NULL && memory != ZR_NULL && operands != ZR_NULL);
    if (memory->reject != ZR_FALSE) {
        return ZR_FALSE;
    }
    if (operation == ZR_EXEC_IR_ORACLE_MEMORY_STORE) {
        if (operandCount != 2u || result != ZR_NULL ||
            operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
            operands[1].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED ||
            operands[0].as.signedInteger != memory->address) {
            return ZR_FALSE;
        }
        memory->value = operands[1];
        ++memory->storeCount;
        return ZR_TRUE;
    }
    if (operation == ZR_EXEC_IR_ORACLE_MEMORY_LOAD) {
        if (operandCount != 1u || result == ZR_NULL ||
            operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
            operands[0].as.signedInteger != memory->address) {
            return ZR_FALSE;
        }
        *result = memory->value;
        ++memory->loadCount;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static void test_load_requires_and_uses_memory_provider(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    SZrOracleMemoryFixture memory;

    build_load_function(&function);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    /* Without a provider, LOAD must fail closed with a stable unsupported
     * diagnostic instead of manufacturing a value. */
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 4u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    memset(&memory, 0, sizeof(memory));
    memory.address = 7;
    memory.value.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    memory.value.as.signedInteger = 0;
    input.memory = oracle_memory_callback;
    input.memoryUserData = &memory;
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == 42);
    assert(memory.storeCount == 1u && memory.loadCount == 1u &&
           execution.eventCount == 2u &&
           execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_STORE &&
           execution.events[1].kind == ZR_EXEC_IR_ORACLE_EVENT_LOAD &&
           execution.events[1].sourceId == 504u);
    ZrCore_ExecIr_OracleResultFree(&execution);

    memory.reject = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR &&
           diagnostic.instructionId == 3u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void build_allocate_function(SZrExecIrFunction *function) {
    TZrExecIrValueId size, allocated;
    SZrExecIrRange sizeResult, allocateOperands, allocateResult, returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    size = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    allocated = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(size != 0u && allocated != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &size, 1u, &sizeResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &size, 1u, &allocateOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &allocated, 1u, &allocateResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &allocated, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), sizeResult, range(0u, 0u),
                       16u, 601u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_ALLOC,
                       allocateOperands, allocateResult, range(0u, 0u),
                       0u, 602u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 603u);
}

typedef struct SZrOracleAllocationFixture {
    SZrExecIrOracleValue value;
    TZrUInt32 allocateCount;
    TZrUInt32 observedOperandCount;
    TZrBool reject;
    TZrBool returnUndefined;
} SZrOracleAllocationFixture;

static TZrBool oracle_allocate_callback(
        void *userData, const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result) {
    SZrOracleAllocationFixture *allocation =
            (SZrOracleAllocationFixture *)userData;
    assert(allocation != ZR_NULL && instruction != ZR_NULL &&
           instruction->opcode == ZR_EXEC_IR_OPCODE_ALLOC &&
           operands != ZR_NULL && result != ZR_NULL);
    if (allocation->reject != ZR_FALSE || operandCount != 1u ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
        operands[0].as.signedInteger != 16) {
        return ZR_FALSE;
    }
    ++allocation->allocateCount;
    allocation->observedOperandCount = operandCount;
    if (allocation->returnUndefined != ZR_FALSE) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
    } else {
        *result = allocation->value;
    }
    return ZR_TRUE;
}

static void test_allocate_requires_and_uses_provider(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    SZrOracleAllocationFixture allocation;
    SZrExecBcProjection bc = {0};
    SZrAotIrProjection aot = {0};

    build_allocate_function(&function);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    /* ALLOC has no implicit host heap.  Without a provider it must remain
     * unsupported and leave no observable event. */
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 2u && diagnostic.sourceId == 602u);
    ZrCore_ExecIr_OracleResultFree(&execution);

    memset(&allocation, 0, sizeof(allocation));
    allocation.value.kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
    allocation.value.as.unsignedInteger = 0xa110cu;
    input.allocate = oracle_allocate_callback;
    input.allocateUserData = &allocation;
    memset(&execution, 0, sizeof(execution));
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED &&
           execution.returnValue.as.unsignedInteger == 0xa110cu &&
           allocation.allocateCount == 1u &&
           allocation.observedOperandCount == 1u && execution.eventCount == 1u &&
           execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_ALLOCATE &&
           execution.events[0].instructionId == 2u &&
           execution.events[0].sourceId == 602u &&
           execution.events[0].operandCount == 1u);
    ZrCore_ExecIr_OracleResultFree(&execution);

    allocation.reject = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ALLOCATION_ERROR &&
           diagnostic.instructionId == 2u && diagnostic.sourceId == 602u &&
           execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);

    allocation.reject = ZR_FALSE;
    allocation.returnUndefined = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
           diagnostic.instructionId == 2u && execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);

    /* Both no-optimization projections carry the allocation opcode and its
     * stable ranges even though neither projection is executable yet. */
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructionCount == 3u &&
           bc.instructions[1u].opcode == ZR_EXEC_IR_OPCODE_ALLOC &&
           bc.opcodes[1u] == ZR_EXEC_IR_OPCODE_ALLOC);
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructionCount == 3u &&
           aot.instructions[1u].opcode == ZR_EXEC_IR_OPCODE_ALLOC &&
           aot.opcodes[1u] == ZR_EXEC_IR_OPCODE_ALLOC && !aot.runnable);
    ZrParser_ExecBcProjection_Free(&bc);
    ZrParser_AotIrProjection_Free(&aot);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void build_drop_function(SZrExecIrFunction *function,
                                TZrBool useDroppedValue) {
    TZrExecIrValueId owned, replacement;
    SZrExecIrRange ownedResult, replacementResult, dropOperands;
    SZrExecIrRange returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    owned = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNIQUE,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    replacement = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(owned != 0u && replacement != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &owned, 1u, &ownedResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &replacement, 1u, &replacementResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &owned, 1u, &dropOperands));
    {
        TZrExecIrValueId returnValue = useDroppedValue ? owned : replacement;
        assert(ZrCore_ExecIr_FunctionAppendOperands(
                function, &returnValue, 1u, &returnOperands));
    }
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), ownedResult, range(0u, 0u),
                       9u, 701u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_DROP,
                       dropOperands, range(0u, 0u), range(0u, 0u),
                       0u, 702u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), replacementResult, range(0u, 0u),
                       11u, 703u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 704u);
}

static void test_drop_consumes_value_and_rejects_reuse(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    build_drop_function(&function, ZR_FALSE);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == 11 &&
           execution.values[0].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
           execution.eventCount == 1u &&
           execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_DROP &&
           execution.events[0].instructionId == 2u &&
           execution.events[0].sourceId == 702u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);

    build_drop_function(&function, ZR_TRUE);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
           diagnostic.instructionId == 4u && diagnostic.sourceId == 704u &&
           execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void build_move_function(SZrExecIrFunction *function,
                                TZrBool useMovedValue) {
    TZrExecIrValueId source, destination;
    SZrExecIrRange sourceResult, moveOperands, destinationResult;
    SZrExecIrRange returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    source = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNIQUE,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    destination = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNIQUE,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(source != 0u && destination != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &source, 1u, &sourceResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &source, 1u, &moveOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &destination, 1u, &destinationResult));
    {
        TZrExecIrValueId returnValue = useMovedValue ? source : destination;
        assert(ZrCore_ExecIr_FunctionAppendOperands(
                function, &returnValue, 1u, &returnOperands));
    }
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), sourceResult, range(0u, 0u),
                       17u, 711u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_MOVE,
                       moveOperands, destinationResult, range(0u, 0u),
                       0u, 712u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 713u);
}

static void test_move_consumes_source_and_rejects_reuse(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    build_move_function(&function, ZR_FALSE);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == 17 &&
           execution.values[0].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
           execution.values[1].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.values[1].as.signedInteger == 17 &&
           execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);

    build_move_function(&function, ZR_TRUE);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
           diagnostic.instructionId == 3u && diagnostic.sourceId == 713u &&
           execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void build_throw_function(SZrExecIrFunction *function) {
    TZrExecIrValueId payload, dead;
    SZrExecIrRange payloadResult, throwOperands, deadResult, returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    payload = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    dead = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(payload != 0u && dead != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &payload, 1u, &payloadResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &payload, 1u, &throwOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &dead, 1u, &deadResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &dead, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), payloadResult, range(0u, 0u),
                       37u, 721u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_THROW,
                       throwOperands, range(0u, 0u), range(0u, 0u),
                       0u, 722u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), deadResult, range(0u, 0u),
                       99u, 723u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 724u);
}

static void build_suspend_function(SZrExecIrFunction *function) {
    TZrExecIrValueId payload, suspended, dead;
    SZrExecIrRange payloadResult, suspendOperands, suspendResult;
    SZrExecIrRange deadResult, returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    payload = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    suspended = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    dead = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(payload != 0u && suspended != 0u && dead != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &payload, 1u, &payloadResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &payload, 1u, &suspendOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &suspended, 1u, &suspendResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &dead, 1u, &deadResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &dead, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), payloadResult, range(0u, 0u),
                       41u, 731u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_SUSPEND,
                       suspendOperands, suspendResult, range(0u, 0u),
                       0u, 732u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), deadResult, range(0u, 0u),
                       101u, 733u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 734u);
}

static void test_throw_and_suspend_publish_boundary(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;

    build_throw_function(&function);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(!execution.returned && execution.terminatedByThrow &&
           !execution.suspended && execution.executedInstructionCount == 2u &&
           execution.values[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.values[0].as.signedInteger == 37 &&
           execution.values[1].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
           execution.eventCount == 1u &&
           execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_THROW &&
           execution.events[0].instructionId == 2u &&
           execution.events[0].sourceId == 722u &&
           execution.events[0].operandCount == 1u &&
           execution.events[0].operands[0].as.signedInteger == 37);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);

    build_suspend_function(&function);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(!execution.returned && !execution.terminatedByThrow &&
           execution.suspended && execution.executedInstructionCount == 2u &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == 41 &&
           execution.values[0].as.signedInteger == 41 &&
           execution.values[1].as.signedInteger == 41 &&
           execution.values[2].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED &&
           execution.eventCount == 1u &&
           execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_SUSPEND &&
           execution.events[0].instructionId == 2u &&
           execution.events[0].sourceId == 732u &&
           execution.events[0].operandCount == 1u &&
           execution.events[0].operands[0].as.signedInteger == 41);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_memory_projection_preserves_load_and_token_pool(void) {
    SZrExecIrFunction function;
    SZrExecBcProjection bc;
    SZrAotIrProjection aot;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrMemoryTokenId tokens[2] = {1u, 2u};

    build_load_function(&function);
    assert(ZrCore_ExecIr_FunctionAppendMemoryTokens(
            &function, tokens, 2u, ZR_NULL));
    /* STORE publishes token 2; LOAD consumes token 1.  The projection must
     * carry the pool, not just these ranges into the source function. */
    function.instructions[2u].memoryOut = range(1u, 1u);
    function.instructions[3u].memoryIn = range(0u, 1u);
    memset(&bc, 0, sizeof(bc));
    memset(&aot, 0, sizeof(aot));
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructionCount == 5u &&
           bc.instructions[3u].opcode == ZR_EXEC_IR_OPCODE_LOAD &&
           bc.memoryTokenCount == 2u && bc.memoryTokens != ZR_NULL &&
           bc.memoryTokens[0u] == 1u && bc.memoryTokens[1u] == 2u);
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructionCount == 5u &&
           aot.instructions[3u].opcode == ZR_EXEC_IR_OPCODE_LOAD &&
           aot.memoryTokenCount == 2u && aot.memoryTokens != ZR_NULL &&
           aot.memoryTokens[0u] == 1u && aot.memoryTokens[1u] == 2u);
    ZrParser_ExecBcProjection_Free(&bc);
    ZrParser_AotIrProjection_Free(&aot);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void build_branch_phi_function(SZrExecIrFunction *function) {
    TZrExecIrValueId condition, left, right, merged;
    TZrExecIrBlockId entry, leftBlock, rightBlock, merge;
    SZrExecIrRange conditionResult, leftResult, rightResult;
    SZrExecIrRange conditionOperand, returnOperand;
    SZrExecIrRange phiIncomingRange, phiRange;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    TZrExecIrBlockId successors[2], predecessor;
    TZrExecIrInstructionId id;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    condition = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    left = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    right = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    merged = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(condition != 0u && left != 0u && right != 0u && merged != 0u);
    entry = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    leftBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    rightBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    merge = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    assert(entry == 1u && leftBlock == 2u && rightBlock == 3u && merge == 4u);
    function->entryBlockId = entry;
    successors[0] = leftBlock;
    successors[1] = rightBlock;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                                                   &function->blocks[0].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, &merge, 1u,
                                                   &function->blocks[1].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, &merge, 1u,
                                                   &function->blocks[2].successorRange));
    predecessor = entry;
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                                                    &function->blocks[1].predecessorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                                                    &function->blocks[2].predecessorRange));
    {
        TZrExecIrBlockId mergePredecessors[2] = {leftBlock, rightBlock};
        assert(ZrCore_ExecIr_FunctionAppendPredecessors(
                function, mergePredecessors, 2u,
                &function->blocks[3].predecessorRange));
    }

    assert(ZrCore_ExecIr_FunctionAppendResults(function, &condition, 1u, &conditionResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &left, 1u, &leftResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &right, 1u, &rightResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u, &conditionOperand));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &merged, 1u, &returnOperand));
    id = append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                            conditionResult, range(0u, 0u), 1u, 201u);
    assert(id == 1u);
    id = append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                            conditionOperand, range(0u, 0u),
                            function->blocks[0].successorRange, 0u, 202u);
    assert(id == 2u);
    id = append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                            leftResult, range(0u, 0u), 2u, 203u);
    assert(id == 3u);
    id = append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH, range(0u, 0u),
                            range(0u, 0u), function->blocks[1].successorRange,
                            0u, 204u);
    assert(id == 4u);
    id = append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                            rightResult, range(0u, 0u), 3u, 205u);
    assert(id == 5u);
    id = append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH, range(0u, 0u),
                            range(0u, 0u), function->blocks[2].successorRange,
                            0u, 206u);
    assert(id == 6u);
    id = append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperand,
                            range(0u, 0u), range(0u, 0u), 0u, 207u);
    assert(id == 7u);
    function->blocks[0].instructionRange = range(0u, 2u);
    function->blocks[1].instructionRange = range(2u, 2u);
    function->blocks[2].instructionRange = range(4u, 2u);
    function->blocks[3].instructionRange = range(6u, 1u);
    function->blocks[0].terminatorInstructionId = 2u;
    function->blocks[1].terminatorInstructionId = 4u;
    function->blocks[2].terminatorInstructionId = 6u;
    function->blocks[3].terminatorInstructionId = 7u;
    incoming[0].predecessor = leftBlock;
    incoming[0].value = left;
    incoming[1].predecessor = rightBlock;
    incoming[1].value = right;
    assert(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, 2u,
                                                   &phiIncomingRange));
    phi.result = merged;
    phi.incomings = phiIncomingRange;
    assert(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u, &phiRange));
    function->blocks[3].phis = phiRange;
    function->values[merged - 1u].definition = 7u;
}

static void build_phi_swap_function(SZrExecIrFunction *function) {
    TZrExecIrValueId first, second;
    TZrExecIrBlockId entry, target;
    SZrExecIrRange incomingRange, phiRange;
    SZrExecIrPhi phis[2];
    SZrExecIrPhiIncoming incoming[2];
    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    first = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    second = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    entry = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    target = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    assert(entry == 1u && target == 2u);
    function->entryBlockId = entry;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, &target, 1u,
                                                   &function->blocks[0].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(function, &entry, 1u,
                                                    &function->blocks[1].predecessorRange));
    incoming[0].predecessor = entry;
    incoming[0].value = second;
    incoming[1].predecessor = entry;
    incoming[1].value = first;
    assert(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, 2u,
                                                   &incomingRange));
    phis[0].result = first;
    phis[0].incomings = range(incomingRange.start, 1u);
    phis[1].result = second;
    phis[1].incomings = range(incomingRange.start + 1u, 1u);
    assert(ZrCore_ExecIr_FunctionAppendPhis(function, phis, 2u, &phiRange));
    function->blocks[1].phis = phiRange;
}

static void build_critical_edge_function(SZrExecIrFunction *function) {
    TZrExecIrBlockId entry, left, merge, side;
    TZrExecIrBlockId successors[2], predecessor, predecessors[2];
    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    entry = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    merge = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    side = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    assert(entry == 1u && left == 2u && merge == 3u && side == 4u);
    function->entryBlockId = entry;
    successors[0] = left;
    successors[1] = merge;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                                                   &function->blocks[0].successorRange));
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, &merge, 1u,
                                                   &function->blocks[3].successorRange));
    predecessor = entry;
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                                                    &function->blocks[1].predecessorRange));
    predecessors[0] = entry;
    predecessors[1] = side;
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, 2u,
                                                    &function->blocks[2].predecessorRange));
}

static TZrBool call_callback(void *userData, const SZrExecIrInstruction *instruction,
                             const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
                             SZrExecIrOracleValue *result) {
    TZrUInt32 *calls = (TZrUInt32 *)userData;
    assert(instruction != ZR_NULL && operands != ZR_NULL && operandCount == 1u);
    assert(operands[0].kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED && result != ZR_NULL);
    ++*calls;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = operands[0].as.signedInteger + 10;
    return ZR_TRUE;
}

static void build_call_function(SZrExecIrFunction *function) {
    TZrExecIrValueId argument, result;
    SZrExecIrRange argumentResult, callOperands, resultRange, returnOperands;
    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    argument = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    result = ZrCore_ExecIr_FunctionAddValue(function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &argument, 1u, &argumentResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &resultRange));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &argument, 1u, &callOperands));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &result, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       argumentResult, range(0u, 0u), 5u, 301u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL, callOperands, resultRange,
                       range(0u, 0u), 0u, 302u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands,
                       range(0u, 0u), range(0u, 0u), 0u, 303u);
}

static void test_scalar_oracle_and_projection(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleResult legacy;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrOracleInput input;
    SZrExecBcProjection bc = {0};
    SZrAotIrProjection aot = {0};
    SZrExecIrDiagnostic diagnostic;
    build_scalar_function(&function);
    assert(ZrCore_ExecIr_RunOracle(&function, &legacy, &diagnostic));
    assert(legacy.instructionCount == 4u && legacy.supportedInstructionCount == 4u);
    memset(&execution, 0, sizeof(execution));
    memset(&input, 0, sizeof(input));
    input.function = &function;
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    ZrCore_ExecIr_OracleResultFree(&execution);
    memset(&execution, 0, sizeof(execution));
    assert(ZrParser_ExecIr_Interpret(&input, &execution, &diagnostic));
    assert(execution.returned && execution.executedInstructionCount == 4u);
    assert(execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == 5);
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructionCount == 4u && bc.opcodes[2] == ZR_EXEC_IR_OPCODE_ADD);
    assert(bc.valueSlotCount == function.valueCount && bc.runnable);
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.signatureHash == 99u && aot.functionToken == 7u && !aot.runnable);
    ZrParser_ExecBcProjection_Free(&bc);
    ZrParser_AotIrProjection_Free(&aot);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_branch_phi_oracle(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrOracleInput input;
    SZrExecIrDiagnostic diagnostic;
    build_branch_phi_function(&function);
    memset(&execution, 0, sizeof(execution));
    memset(&input, 0, sizeof(input));
    input.function = &function;
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(execution.returned && execution.currentBlock == 4u);
    assert(execution.returnValue.as.signedInteger == 2 &&
           execution.executedInstructionCount == 5u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_call_event_oracle(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrOracleInput input;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt32 calls = 0u;
    build_call_function(&function);
    memset(&execution, 0, sizeof(execution));
    memset(&input, 0, sizeof(input));
    input.function = &function;
    input.call = call_callback;
    input.userData = &calls;
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(calls == 1u && execution.eventCount == 1u);
    assert(execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_CALL &&
           execution.events[0].instructionId == 2u);
    assert(execution.returnValue.as.signedInteger == 15);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

typedef struct SZrOracleInvokeFixture {
    SZrExecIrOracleValue result;
    TZrUInt32 callCount;
    TZrBool reject;
    TZrBool throwResult;
    TZrBool returnUndefined;
} SZrOracleInvokeFixture;

typedef struct SZrOracleInvokePayloadFixture {
    SZrExecIrOracleValue payload;
    TZrUInt32 callCount;
} SZrOracleInvokePayloadFixture;

static TZrBool oracle_invoke_payload_callback(
        void *userData, const SZrExecIrInstruction *instruction,
        SZrExecIrOracleValue *result) {
    SZrOracleInvokePayloadFixture *fixture =
            (SZrOracleInvokePayloadFixture *)userData;
    assert(fixture != ZR_NULL && instruction != ZR_NULL &&
           instruction->opcode == ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD &&
           result != ZR_NULL);
    ++fixture->callCount;
    *result = fixture->payload;
    return ZR_TRUE;
}

static TZrBool oracle_invoke_callback(
        void *userData, const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result, TZrBool *threw) {
    SZrOracleInvokeFixture *fixture =
            (SZrOracleInvokeFixture *)userData;
    assert(fixture != ZR_NULL && instruction != ZR_NULL &&
           instruction->opcode == ZR_EXEC_IR_OPCODE_INVOKE &&
           operands != ZR_NULL && operandCount == 0u && result != ZR_NULL &&
           threw != ZR_NULL);
    if (fixture->reject != ZR_FALSE) {
        return ZR_FALSE;
    }
    ++fixture->callCount;
    *threw = fixture->throwResult;
    if (fixture->returnUndefined != ZR_FALSE) {
        result->kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED;
    } else {
        *result = fixture->result;
    }
    return ZR_TRUE;
}

static void build_invoke_function(SZrExecIrFunction *function) {
    TZrExecIrValueId invokeResult, payload;
    TZrExecIrBlockId entry, normal, exception, successors[2], predecessor;
    SZrExecIrRange invokeResultRange, payloadResultRange;
    SZrExecIrRange normalReturnOperands, exceptionReturnOperands;
    SZrExecIrInstruction instruction;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    invokeResult = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    payload = ZrCore_ExecIr_FunctionAddValue(
            function, 41u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(invokeResult != 0u && payload != 0u);
    entry = ZrCore_ExecIr_FunctionAddBlock(function,
                                            ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    normal = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    exception = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION);
    assert(entry == 1u && normal == 2u && exception == 3u);
    function->entryBlockId = entry;
    successors[0] = normal;
    successors[1] = exception;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(
            function, successors, 2u,
            &function->blocks[entry - 1u].successorRange));
    predecessor = entry;
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
            function, &predecessor, 1u,
            &function->blocks[normal - 1u].predecessorRange));
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(
            function, &predecessor, 1u,
            &function->blocks[exception - 1u].predecessorRange));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &invokeResult, 1u, &invokeResultRange));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_INVOKE;
    instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    instruction.results = invokeResultRange;
    instruction.successorRange = function->blocks[entry - 1u].successorRange;
    instruction.sourceId = 321u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                   ZR_NULL));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &invokeResult, 1u, &normalReturnOperands));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = normalReturnOperands;
    instruction.sourceId = 322u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                   ZR_NULL));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &payload, 1u, &payloadResultRange));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD;
    instruction.results = payloadResultRange;
    instruction.sourceId = 323u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                   ZR_NULL));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &payload, 1u, &exceptionReturnOperands));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = exceptionReturnOperands;
    instruction.sourceId = 324u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                   ZR_NULL));
    function->blocks[entry - 1u].instructionRange = range(0u, 1u);
    function->blocks[entry - 1u].terminatorInstructionId = 1u;
    function->blocks[normal - 1u].instructionRange = range(1u, 1u);
    function->blocks[normal - 1u].terminatorInstructionId = 2u;
    function->blocks[exception - 1u].instructionRange = range(2u, 2u);
    function->blocks[exception - 1u].terminatorInstructionId = 4u;
}

static void test_invoke_oracle_provider(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    SZrOracleInvokeFixture invoke;
    SZrOracleInvokePayloadFixture payload;

    build_invoke_function(&function);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_INVOKE);
    ZrCore_ExecIr_OracleResultFree(&execution);

    memset(&invoke, 0, sizeof(invoke));
    invoke.result.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    invoke.result.as.signedInteger = 19;
    input.invoke = oracle_invoke_callback;
    input.invokeUserData = &invoke;
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(invoke.callCount == 1u && execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == 19 &&
           execution.currentBlock == 2u && execution.eventCount == 1u &&
           execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_CALL &&
           execution.events[0].instructionId == 1u);
    ZrCore_ExecIr_OracleResultFree(&execution);

    memset(&payload, 0, sizeof(payload));
    payload.payload.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    payload.payload.as.signedInteger = -23;
    input.exceptionPayload = oracle_invoke_payload_callback;
    input.exceptionPayloadUserData = &payload;
    invoke.throwResult = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(invoke.callCount == 2u && payload.callCount == 1u &&
           execution.returned && execution.currentBlock == 3u &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == -23 &&
           execution.eventCount == 1u &&
           execution.events[0].kind == ZR_EXEC_IR_ORACLE_EVENT_CALL);
    ZrCore_ExecIr_OracleResultFree(&execution);

    invoke.throwResult = ZR_FALSE;
    invoke.reject = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_INVOKE_ERROR &&
           diagnostic.instructionId == 1u && execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);

    invoke.reject = ZR_FALSE;
    invoke.returnUndefined = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
           diagnostic.instructionId == 1u && execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

typedef struct SZrOracleTypeTestFixture {
    TZrExecIrTypeToken expectedMatchType;
    TZrUInt32 callCount;
    TZrBool result;
    TZrBool reject;
} SZrOracleTypeTestFixture;

static TZrBool oracle_type_test_callback(
        void *userData, const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *value,
        TZrExecIrTypeToken matchTypeToken, TZrBool *result) {
    SZrOracleTypeTestFixture *fixture =
            (SZrOracleTypeTestFixture *)userData;
    assert(fixture != ZR_NULL && instruction != ZR_NULL &&
           instruction->opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST &&
           value != ZR_NULL && result != ZR_NULL);
    if (fixture->reject != ZR_FALSE ||
        matchTypeToken != fixture->expectedMatchType) {
        return ZR_FALSE;
    }
    ++fixture->callCount;
    *result = fixture->result;
    return ZR_TRUE;
}

static void build_type_test_function(SZrExecIrFunction *function) {
    TZrExecIrValueId payload, matched;
    SZrExecIrRange matchedResult, testOperands,
            returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    payload = ZrCore_ExecIr_FunctionAddValue(
            function, 41u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    matched = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(payload != 0u && matched != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &matched, 1u, &matchedResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &payload, 1u, &testOperands));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &matched, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_TYPE_TEST,
                       testOperands, matchedResult, range(0u, 0u),
                       0u, 801u);
    function->instructions[0].matchTypeToken = 41u;
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 802u);
}

static void test_type_test_oracle_provider(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleValue initial;
    SZrOracleTypeTestFixture fixture;

    build_type_test_function(&function);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    initial.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    initial.as.signedInteger = 7;
    input.initialValues = &initial;
    input.initialValueCount = 1u;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_TYPE_TEST);
    ZrCore_ExecIr_OracleResultFree(&execution);

    memset(&fixture, 0, sizeof(fixture));
    fixture.expectedMatchType = 41u;
    fixture.result = ZR_TRUE;
    input.typeTest = oracle_type_test_callback;
    input.typeTestUserData = &fixture;
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(fixture.callCount == 1u && execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL &&
           execution.returnValue.as.boolean == ZR_TRUE);
    ZrCore_ExecIr_OracleResultFree(&execution);

    fixture.result = ZR_FALSE;
    memset(&execution, 0, sizeof(execution));
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(fixture.callCount == 2u && execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL &&
           execution.returnValue.as.boolean == ZR_FALSE);
    ZrCore_ExecIr_OracleResultFree(&execution);

    fixture.reject = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_ORACLE_TYPE_TEST_ERROR &&
           diagnostic.instructionId == 1u && execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

typedef struct SZrOracleExceptionPayloadFixture {
    SZrExecIrOracleValue payload;
    TZrUInt32 callCount;
    TZrBool reject;
} SZrOracleExceptionPayloadFixture;

static TZrBool oracle_exception_payload_callback(
        void *userData, const SZrExecIrInstruction *instruction,
        SZrExecIrOracleValue *result) {
    SZrOracleExceptionPayloadFixture *fixture =
            (SZrOracleExceptionPayloadFixture *)userData;
    assert(fixture != ZR_NULL && instruction != ZR_NULL &&
           instruction->opcode == ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD &&
           result != ZR_NULL);
    if (fixture->reject != ZR_FALSE) {
        return ZR_FALSE;
    }
    ++fixture->callCount;
    *result = fixture->payload;
    return ZR_TRUE;
}

static void build_exception_payload_function(SZrExecIrFunction *function) {
    TZrExecIrValueId payload;
    SZrExecIrRange payloadResult, returnOperands;

    memset(function, 0, sizeof(*function));
    ZrCore_ExecIr_FunctionInit(function);
    payload = ZrCore_ExecIr_FunctionAddValue(
            function, 41u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(payload != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            function, &payload, 1u, &payloadResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            function, &payload, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD,
                       range(0u, 0u), payloadResult, range(0u, 0u),
                       0u, 811u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                       returnOperands, range(0u, 0u), range(0u, 0u),
                       0u, 812u);
}

static void test_exception_payload_oracle_provider(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    SZrOracleExceptionPayloadFixture fixture;

    build_exception_payload_function(&function);
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD);
    ZrCore_ExecIr_OracleResultFree(&execution);

    memset(&fixture, 0, sizeof(fixture));
    fixture.payload.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    fixture.payload.as.signedInteger = -17;
    input.exceptionPayload = oracle_exception_payload_callback;
    input.exceptionPayloadUserData = &fixture;
    assert(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(fixture.callCount == 1u && execution.returned &&
           execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
           execution.returnValue.as.signedInteger == -17);
    ZrCore_ExecIr_OracleResultFree(&execution);

    fixture.reject = ZR_TRUE;
    memset(&execution, 0, sizeof(execution));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic));
    assert(diagnostic.code ==
                   ZR_EXEC_IR_DIAGNOSTIC_ORACLE_EXCEPTION_PAYLOAD_ERROR &&
           diagnostic.instructionId == 1u && execution.eventCount == 0u);
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_phi_copy_and_critical_edge_split(void) {
    SZrExecIrFunction phiFunction, criticalFunction;
    SZrExecBcProjection bc = {0};
    SZrExecIrDiagnostic diagnostic;
    build_phi_swap_function(&phiFunction);
    assert(ZrParser_ExecIr_LowerExecBc(&phiFunction, &bc, &diagnostic));
    assert(bc.phiCopyCount == 2u && bc.temporarySlotCount == 1u);
    assert(bc.phiCopySources[0] == 2u && bc.phiCopySources[1] == 1u);
    assert(bc.phiCopyDestinations[0] == 1u && bc.phiCopyDestinations[1] == 2u);
    assert(bc.phiCopyEdges[0] == 1u);
    ZrParser_ExecBcProjection_Free(&bc);
    ZrCore_ExecIr_FreeFunction(&phiFunction);
    build_critical_edge_function(&criticalFunction);
    assert(ZrParser_ExecIr_LowerExecBc(&criticalFunction, &bc, &diagnostic));
    assert(bc.blockCount == 5u && bc.syntheticBlockCount == 1u);
    assert(bc.successors[bc.blocks[0].successors.start + 1u] == 5u);
    assert(bc.predecessors[bc.blocks[4].predecessors.start] == 1u);
    assert(bc.successors[bc.blocks[4].successors.start] == 3u);
    ZrParser_ExecBcProjection_Free(&bc);
    ZrCore_ExecIr_FreeFunction(&criticalFunction);
}

static void test_unsupported_and_transactional_failures(void) {
    static const EZrExecIrOpcode iteratorOpcodes[] = {
        ZR_EXEC_IR_OPCODE_ITER_INIT,
        ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT,
        ZR_EXEC_IR_OPCODE_ITER_CURRENT,
    };
    SZrExecIrFunction function;
    SZrExecBcProjection bc = {0};
    SZrAotIrProjection aot = {0};
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcInstruction *oldInstructions;
    TZrUInt32 oldCount, oldAotCount;
    build_scalar_function(&function);
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    oldInstructions = bc.instructions;
    oldCount = bc.instructionCount;
    oldAotCount = aot.instructionCount;
    function.instructions[0].operands = range(0u, 1u);
    for (TZrUInt32 index = 0u;
         index < sizeof(iteratorOpcodes) / sizeof(iteratorOpcodes[0]);
         ++index) {
        function.instructions[0].opcode = iteratorOpcodes[index];
        assert(!ZrCore_ExecIr_RunOracle(&function, ZR_NULL, &diagnostic));
        assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
               diagnostic.instructionId == 1u &&
               diagnostic.actualVersion == (TZrUInt32)iteratorOpcodes[index]);
        assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
        assert(bc.instructions[0u].opcode == iteratorOpcodes[index] &&
               bc.instructions[0u].operands.count == 1u && !bc.runnable);
        oldInstructions = bc.instructions;
        oldCount = bc.instructionCount;
        assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
        assert(aot.instructions[0u].opcode == iteratorOpcodes[index] &&
               aot.instructions[0u].operands.count == 1u && !aot.runnable);
        oldAotCount = aot.instructionCount;
    }
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD;
    function.instructions[0].operands = range(0u, 0u);
    assert(!ZrCore_ExecIr_RunOracle(&function, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD);
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD &&
           !bc.runnable);
    oldInstructions = bc.instructions;
    oldCount = bc.instructionCount;
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructions[0u].opcode ==
                   ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD &&
           !aot.runnable);
    oldAotCount = aot.instructionCount;
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_INVOKE;
    function.instructions[0].operands = range(0u, 0u);
    function.instructions[0].successorRange = range(0u, 0u);
    assert(!ZrCore_ExecIr_RunOracle(&function, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_INVOKE);
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_INVOKE &&
           !bc.runnable);
    oldInstructions = bc.instructions;
    oldCount = bc.instructionCount;
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_INVOKE &&
           !aot.runnable);
    oldAotCount = aot.instructionCount;
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_PLACE_BASE;
    function.instructions[0].operands = range(0u, 1u);
    assert(!ZrCore_ExecIr_RunOracle(&function, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_PLACE_BASE);
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE &&
           bc.instructions[0u].operands.count == 1u && !bc.runnable);
    oldInstructions = bc.instructions;
    oldCount = bc.instructionCount;
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE &&
           aot.instructions[0u].operands.count == 1u && !aot.runnable);
    oldAotCount = aot.instructionCount;
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_PLACE_PROJECT;
    function.instructions[0].operands = range(0u, 2u);
    assert(!ZrCore_ExecIr_RunOracle(&function, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_PLACE_PROJECT);
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_PLACE_PROJECT &&
           bc.instructions[0u].operands.count == 2u && !bc.runnable);
    oldInstructions = bc.instructions;
    oldCount = bc.instructionCount;
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_PLACE_PROJECT &&
           aot.instructions[0u].operands.count == 2u && !aot.runnable);
    oldAotCount = aot.instructionCount;
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_TYPE_TEST;
    function.instructions[0].operands = range(0u, 1u);
    function.instructions[0].matchTypeToken = 7u;
    assert(!ZrCore_ExecIr_RunOracle(&function, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u &&
           diagnostic.actualVersion == ZR_EXEC_IR_OPCODE_TYPE_TEST);
    assert(ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(bc.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST &&
           bc.instructions[0u].matchTypeToken == 7u && !bc.runnable);
    oldInstructions = bc.instructions;
    oldCount = bc.instructionCount;
    assert(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructions[0u].opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST &&
           aot.instructions[0u].matchTypeToken == 7u && !aot.runnable);
    oldAotCount = aot.instructionCount;
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    function.instructions[0].operands = range(0u, 0u);
    function.instructions[0].matchTypeToken = 0u;
    function.instructions[2].operands = range(UINT32_MAX, 1u);
    assert(!ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
           bc.instructions == oldInstructions && bc.instructionCount == oldCount);
    assert(!ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
           aot.instructionCount == oldAotCount);
    ZrParser_ExecBcProjection_Free(&bc);
    ZrParser_AotIrProjection_Free(&aot);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_malformed_input(void) {
    SZrExecIrFunction function;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult result;
    SZrExecIrDiagnostic diagnostic;
    memset(&function, 0, sizeof(function));
    ZrCore_ExecIr_FunctionInit(&function);
    function.instructionCount = 1u;
    function.instructionCapacity = 1u;
    function.instructions = ZR_NULL;
    memset(&input, 0, sizeof(input));
    input.function = &function;
    memset(&result, 0, sizeof(result));
    assert(!ZrCore_ExecIr_RunOracleEx(&input, &result, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    ZrCore_ExecIr_OracleResultFree(&result);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_load_requires_and_uses_memory_provider();
    test_allocate_requires_and_uses_provider();
    test_drop_consumes_value_and_rejects_reuse();
    test_move_consumes_source_and_rejects_reuse();
    test_throw_and_suspend_publish_boundary();
    test_memory_projection_preserves_load_and_token_pool();
    test_scalar_oracle_and_projection();
    test_branch_phi_oracle();
    test_call_event_oracle();
    test_invoke_oracle_provider();
    test_type_test_oracle_provider();
    test_exception_payload_oracle_provider();
    test_phi_copy_and_critical_edge_split();
    test_unsupported_and_transactional_failures();
    test_malformed_input();
    return 0;
}
