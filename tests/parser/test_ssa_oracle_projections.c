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
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_ALLOC;
    function.instructions[0].operands = range(0u, 1u);
    assert(!ZrCore_ExecIr_RunOracle(&function, ZR_NULL, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           diagnostic.instructionId == 1u && diagnostic.sourceId == 101u);
    assert(!ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
           bc.instructions == oldInstructions && bc.instructionCount == oldCount);
    assert(!ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic));
    assert(aot.instructionCount == oldAotCount);
    function.instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    function.instructions[0].operands = range(0u, 0u);
    function.instructions[2].operands = range(UINT32_MAX, 1u);
    assert(!ZrParser_ExecIr_LowerExecBc(&function, &bc, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
           bc.instructions == oldInstructions && bc.instructionCount == oldCount);
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
    test_memory_projection_preserves_load_and_token_pool();
    test_scalar_oracle_and_projection();
    test_branch_phi_oracle();
    test_call_event_oracle();
    test_phi_copy_and_critical_edge_split();
    test_unsupported_and_transactional_failures();
    test_malformed_input();
    return 0;
}
