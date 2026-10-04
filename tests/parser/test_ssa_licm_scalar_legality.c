#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/exec_ir_loops.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_builder.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static void fail_with_exec_ir_diagnostic(const char *site,
                                         const SZrExecIrDiagnostic *diagnostic) {
    fprintf(stderr,
            "DIAGNOSTIC %s code=%u function=%u block=%u instruction=%u source=%u expectedVersion=%u actualVersion=%u expectedHash=%llu actualHash=%llu\n",
            site, (unsigned)diagnostic->code,
            (unsigned)diagnostic->functionToken, (unsigned)diagnostic->blockId,
            (unsigned)diagnostic->instructionId, (unsigned)diagnostic->sourceId,
            (unsigned)diagnostic->expectedVersion,
            (unsigned)diagnostic->actualVersion,
            (unsigned long long)diagnostic->expectedHash,
            (unsigned long long)diagnostic->actualHash);
    fflush(stderr);
    exit(EXIT_FAILURE);
}

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange value;
    value.start = start;
    value.count = count;
    return value;
}

static TZrExecIrValueId add_value(SZrExecIrFunction *function) {
    return ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
}

static void append_instruction(SZrExecIrFunction *function,
                               EZrExecIrOpcode opcode,
                               SZrExecIrRange operands,
                               SZrExecIrRange results,
                               TZrUInt32 layoutId,
                               TZrUInt16 flags,
                               TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.layoutId = layoutId;
    instruction.flags = flags;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
    assert(id == function->instructionCount);
}

static void append_successors(SZrExecIrFunction *function,
                              TZrExecIrBlockId blockId,
                              const TZrExecIrBlockId *successors,
                              TZrUInt32 count) {
    SZrExecIrRange successorRange;
    assert(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, count,
                                                   &successorRange));
    function->blocks[blockId - 1u].successorRange = successorRange;
}

static void append_predecessors(SZrExecIrFunction *function,
                                TZrExecIrBlockId blockId,
                                const TZrExecIrBlockId *predecessors,
                                TZrUInt32 count) {
    SZrExecIrRange predecessorRange;
    assert(ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, count,
                                                     &predecessorRange));
    function->blocks[blockId - 1u].predecessorRange = predecessorRange;
}

static void bind_block_terminator_successors(SZrExecIrFunction *function,
                                              TZrExecIrBlockId blockId) {
    SZrExecIrBlock *block = ZrCore_ExecIr_FunctionBlockAt(function, blockId);
    assert(block != ZR_NULL && block->instructionRange.count != 0u);
    assert(block->terminatorInstructionId !=
           ZR_EXEC_IR_INSTRUCTION_ID_INVALID);
    function->instructions[block->terminatorInstructionId - 1u].successorRange =
            block->successorRange;
}

static void configure_loop_effect_tokens(SZrExecIrFunction *function) {
    SZrExecIrDiagnostic diagnostic;
    if (!ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic)) {
        fail_with_exec_ir_diagnostic("loop fixture effect-token synthesis",
                                     &diagnostic);
    }
}

static void build_loop(SZrExecIrFunction *function, TZrBool zeroTrip,
                       TZrBool throwingBody, TZrBool strengthBody) {
    TZrExecIrValueId condition;
    TZrExecIrValueId continueCondition;
    TZrExecIrValueId invariant;
    TZrExecIrValueId divisor = 0u;
    TZrExecIrValueId quotient = 0u;
    TZrExecIrValueId factor = 0u;
    TZrExecIrValueId product = 0u;
    TZrExecIrValueId merged;
    TZrExecIrValueId returnValue;
    SZrExecIrRange resultRange;
    SZrExecIrRange operandRange;
    SZrExecIrRange incomingRange;
    SZrExecIrRange phiRange;
    SZrExecIrPhi phi;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhiIncoming incoming[2];
    TZrExecIrBlockId edge;
    TZrExecIrBlockId edges[2];
    TZrUInt32 blockIndex;

    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 0x1101u;
    function->signatureHash = UINT64_C(0x10101010);
    condition = add_value(function);
    continueCondition = ZrCore_ExecIr_FunctionAddExternalValue(
            function, ZR_VALUE_TYPE_BOOL, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(continueCondition != ZR_EXEC_IR_VALUE_ID_INVALID);
    invariant = add_value(function);
    if (throwingBody) {
        divisor = add_value(function);
        quotient = add_value(function);
    }
    if (strengthBody) {
        factor = add_value(function);
        product = add_value(function);
    }
    merged = add_value(function);

    for (blockIndex = 0u; blockIndex < 4u; ++blockIndex) {
        TZrExecIrBlockId id = ZrCore_ExecIr_FunctionAddBlock(
                function, blockIndex == 0u ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u);
        assert(id == blockIndex + 1u);
    }

    assert(ZrCore_ExecIr_FunctionAppendResults(function, &condition, 1u,
                                                &resultRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       resultRange, zeroTrip ? 0u : 1u, 0u, 11u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH, range(0u, 0u),
                       range(0u, 0u), 0u, 0u, 12u);
    function->blocks[0].instructionRange = range(0u, 2u);
    function->blocks[0].terminatorInstructionId = 2u;

    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u,
                                                &operandRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                       operandRange, range(0u, 0u), 0u, 0u, 21u);
    function->blocks[1].instructionRange = range(2u, 1u);
    function->blocks[1].terminatorInstructionId = 3u;

    assert(ZrCore_ExecIr_FunctionAppendResults(function, &invariant, 1u,
                                                &resultRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       resultRange, 9u, 0u, 31u);
    if (strengthBody) {
        TZrExecIrValueId operands[2] = {invariant, factor};
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &factor, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                           resultRange, 1u, 0u, 32u);
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, operands, 2u,
                                                    &operandRange));
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &product, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_MUL, operandRange,
                           resultRange, 0u, 0u, 33u);
    }
    if (throwingBody) {
        TZrExecIrValueId divisors[2] = {invariant, divisor};
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &divisor, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                           resultRange, 0u, 0u, strengthBody ? 34u : 32u);
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, divisors, 2u,
                                                    &operandRange));
        assert(ZrCore_ExecIr_FunctionAppendResults(function, &quotient, 1u,
                                                    &resultRange));
        append_instruction(function, ZR_EXEC_IR_OPCODE_DIV, operandRange,
                           resultRange, 0u, ZR_EXEC_IR_FLAG_MAY_THROW,
                           strengthBody ? 35u : 33u);
    }
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &continueCondition, 1u,
                                                &operandRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                       operandRange, range(0u, 0u), 0u, 0u,
                       (TZrExecIrSourceId)(strengthBody ? (throwingBody ? 36u : 34u)
                                                         : (throwingBody ? 34u : 32u)));
    function->blocks[2].instructionRange = range(3u,
                                                  (throwingBody ? 4u : 2u) +
                                                  (strengthBody ? 2u : 0u));
    function->blocks[2].terminatorInstructionId =
            function->blocks[2].instructionRange.start +
            function->blocks[2].instructionRange.count;

    returnValue = throwingBody ? quotient
                               : (strengthBody ? product : invariant);
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &merged, 1u,
                                                &operandRange));
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, operandRange,
                       range(0u, 0u), 0u, 0u, 41u);
    function->blocks[3].instructionRange = range(function->instructionCount - 1u, 1u);
    function->blocks[3].terminatorInstructionId = function->instructionCount;

    edge = 2u;
    append_successors(function, 1u, &edge, 1u);
    edges[0] = 3u;
    edges[1] = 4u;
    append_successors(function, 2u, edges, 2u);
    edges[0] = 2u;
    edges[1] = 4u;
    append_successors(function, 3u, edges, 2u);
    append_successors(function, 4u, ZR_NULL, 0u);

    edge = 1u;
    append_predecessors(function, 1u, ZR_NULL, 0u);
    edges[0] = 1u;
    edges[1] = 3u;
    append_predecessors(function, 2u, edges, 2u);
    edge = 2u;
    append_predecessors(function, 3u, &edge, 1u);
    edges[0] = 2u;
    edges[1] = 3u;
    append_predecessors(function, 4u, edges, 2u);

    bind_block_terminator_successors(function, 1u);
    bind_block_terminator_successors(function, 2u);
    bind_block_terminator_successors(function, 3u);
    bind_block_terminator_successors(function, 4u);

    incoming[0].predecessor = 2u;
    incoming[0].value = condition;
    incoming[1].predecessor = 3u;
    incoming[1].value = returnValue;
    assert(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, 2u,
                                                   &incomingRange));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &merged, 1u,
                                               ZR_NULL));
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    phi.incomings = incomingRange;
    assert(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u, &phiRange));
    function->blocks[3].phis = phiRange;
    configure_loop_effect_tokens(function);
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                      &diagnostic)) {
        fail_with_exec_ir_diagnostic("loop fixture before optimization",
                                     &diagnostic);
    }
}

/* Regression: CONSTANT.layoutId names a pool slot, not the scalar payload. */
static int test_legacy_pool_slot_is_not_one(void) {
    SZrExecIrFunction function;
    SZrExecIrLoopInfo loops;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult before, after;
    SZrExecIrOracleValue constants[10];
    SZrExecIrOracleValue initialValues[6];
    TZrUInt32 index;

    build_loop(&function, ZR_FALSE, ZR_FALSE, ZR_TRUE);
    memset(constants, 0, sizeof(constants));
    for (index = 0u; index < 10u; ++index)
        constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    constants[1].as.signedInteger = 2;
    constants[9].as.signedInteger = 9;
    memset(initialValues, 0, sizeof(initialValues));
    initialValues[1].kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    initialValues[1].as.boolean = ZR_FALSE;
    memset(&input, 0, sizeof(input));
    input.function = &function;
    input.constants = constants;
    input.constantCount = 10u;
    input.initialValues = initialValues;
    input.initialValueCount = function.valueCount;
    input.maxSteps = 100u;
    ZrCore_ExecIr_OracleResultInit(&before);
    ZrCore_ExecIr_OracleResultInit(&after);
    if (!ZrCore_ExecIr_RunOracleEx(&input, &before, &diagnostic))
        fail_with_exec_ir_diagnostic("pooled MUL baseline", &diagnostic);
    assert(before.returned);
    assert(before.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED);
    assert(before.returnValue.as.signedInteger == 18);
    ZrParser_ExecIr_LoopInfoInit(&loops);
    if (!ZrParser_ExecIr_StrengthReduce(&function, &loops, &diagnostic))
        fail_with_exec_ir_diagnostic("pooled MUL strength reduction", &diagnostic);
    if (!ZrCore_ExecIr_RunOracleEx(&input, &after, &diagnostic))
        fail_with_exec_ir_diagnostic("pooled MUL optimized", &diagnostic);
    fprintf(stderr, "pooled MUL baseline=%lld optimized=%lld reduced=%u opcode=%u\n",
            (long long)before.returnValue.as.signedInteger,
            (long long)after.returnValue.as.signedInteger,
            (unsigned)loops.strengthReducedCount,
            (unsigned)function.instructions[5].opcode);
    fflush(stderr);
    if (!after.returned || after.returnValue.kind != before.returnValue.kind ||
        after.returnValue.as.signedInteger != before.returnValue.as.signedInteger ||
        function.instructions[5].opcode != ZR_EXEC_IR_OPCODE_MUL ||
        loops.strengthReducedCount != 0u) {
        fprintf(stderr, "FAIL legacy pooled constant semantic equality\n");
        return EXIT_FAILURE;
    }
    ZrCore_ExecIr_OracleResultFree(&after);
    ZrCore_ExecIr_OracleResultFree(&before);
    ZrParser_ExecIr_LoopInfoFree(&loops);
    ZrCore_ExecIr_FreeFunction(&function);
    return EXIT_SUCCESS;
}

#include "test_ssa_licm_scalar_context.inc"

int main(void) {
    if (test_legacy_pool_slot_is_not_one() != EXIT_SUCCESS) return EXIT_FAILURE;
    return test_scalar_context_cases();
}
