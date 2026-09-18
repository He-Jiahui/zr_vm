#include "zr_vm_parser/exec_ir_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static TZrExecIrValueId add_value(SZrExecIrFunction *function) {
    TZrExecIrValueId value = ZrCore_ExecIr_FunctionAddValue(
            function, (TZrMetadataToken)11u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(value != ZR_EXEC_IR_VALUE_ID_INVALID, "append value");
    return value;
}

static TZrExecIrBlockId add_block(SZrExecIrFunction *function,
                                  TZrUInt32 flags) {
    TZrExecIrBlockId block = ZrCore_ExecIr_FunctionAddBlock(function, flags);
    check(block != ZR_EXEC_IR_BLOCK_ID_INVALID, "append block");
    return block;
}

static void append_successors(SZrExecIrFunction *function,
                              TZrExecIrBlockId block,
                              const TZrExecIrBlockId *successors,
                              TZrUInt32 count) {
    check(ZrCore_ExecIr_FunctionAppendSuccessors(
                  function, successors, count,
                  &function->blocks[block - 1u].successorRange),
          "append successors");
}

static void append_predecessors(SZrExecIrFunction *function,
                                TZrExecIrBlockId block,
                                const TZrExecIrBlockId *predecessors,
                                TZrUInt32 count) {
    check(ZrCore_ExecIr_FunctionAppendPredecessors(
                  function, predecessors, count,
                  &function->blocks[block - 1u].predecessorRange),
          "append predecessors");
}

static TZrExecIrInstructionId append_instruction(
        SZrExecIrFunction *function,
        EZrExecIrOpcode opcode,
        TZrExecIrValueId result,
        const TZrExecIrValueId *operands,
        TZrUInt32 operandCount,
        SZrExecIrRange successors) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId instructionId = ZR_EXEC_IR_INSTRUCTION_ID_INVALID;

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.successorRange = successors;
    if (result != ZR_EXEC_IR_VALUE_ID_INVALID) {
        check(ZrCore_ExecIr_FunctionAppendResults(
                      function, &result, 1u, &instruction.resultRange),
              "append instruction result");
    }
    if (operandCount != 0u) {
        check(ZrCore_ExecIr_FunctionAppendOperands(
                      function, operands, operandCount,
                      &instruction.operandRange),
              "append instruction operands");
    }
    check(ZrCore_ExecIr_FunctionAppendInstruction(
                  function, &instruction, &instructionId),
          "append instruction");
    return instructionId;
}

static void set_block_instructions(SZrExecIrFunction *function,
                                   TZrExecIrBlockId block,
                                   TZrUInt32 start,
                                   TZrUInt32 count) {
    function->blocks[block - 1u].instructionRange.start = start;
    function->blocks[block - 1u].instructionRange.count = count;
    function->blocks[block - 1u].terminatorInstructionId = start + count;
}

static TZrExecIrValueId append_place_base(SZrExecIrFunction *function,
                                          TZrBool promotable) {
    TZrExecIrValueId provenance = ZrCore_ExecIr_FunctionAddExternalValue(
            function, (TZrMetadataToken)11u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    TZrExecIrValueId place = add_value(function);
    check(provenance != ZR_EXEC_IR_VALUE_ID_INVALID,
          "append place provenance");
    function->values[place - 1u].flags =
            ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS |
            (promotable ? ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE : 0u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_PLACE_BASE, place,
                       &provenance, 1u, (SZrExecIrRange){0u, 0u});
    return place;
}

static void verify_promoted_function(SZrExecIrFunction *function,
                                     const char *message) {
    SZrExecIrDiagnostic diagnostic;
    check(ZrCore_ExecIr_VerifyFunction(
                  function,
                  (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                         ZR_EXEC_IR_VERIFY_SSA),
                  &diagnostic),
          message);
}

static void build_ssa(SZrExecIrFunction *function,
                      SZrExecIrDiagnostic *diagnostic,
                      const char *message) {
    if (!ZrParser_ExecIr_BuildSsa(function, diagnostic)) {
        fprintf(stderr,
                "SSA diagnostic: code=%u instruction=%u block=%u expected=%u actual=%u\n",
                (unsigned)diagnostic->code,
                (unsigned)diagnostic->instructionId,
                (unsigned)diagnostic->blockId,
                (unsigned)diagnostic->expectedVersion,
                (unsigned)diagnostic->actualVersion);
        check(ZR_FALSE, message);
    }
}

static void test_promotes_straight_line_and_is_repeatable(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry;
    TZrExecIrValueId place, stored, loaded;
    TZrExecIrValueId storeOperands[2];

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 101u;
    entry = add_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    place = append_place_base(&function, ZR_TRUE);
    stored = add_value(&function);
    loaded = add_value(&function);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, stored,
                       NULL, 0u, (SZrExecIrRange){0u, 0u});
    storeOperands[0] = place;
    storeOperands[1] = stored;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE,
                       ZR_EXEC_IR_VALUE_ID_INVALID, storeOperands, 2u,
                       (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_LOAD, loaded,
                       &place, 1u, (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN,
                       ZR_EXEC_IR_VALUE_ID_INVALID, &loaded, 1u,
                       (SZrExecIrRange){0u, 0u});
    set_block_instructions(&function, entry, 0u, 5u);

    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic),
          "compute straight-line dominators");
    build_ssa(&function, &diagnostic, "promote straight-line place");
    check(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_NOP &&
              function.instructions[2].operandRange.count == 0u,
          "straight-line store was not removed");
    check(function.instructions[3].opcode == ZR_EXEC_IR_OPCODE_COPY &&
              function.operands[function.instructions[3].operandRange.start] ==
                      stored &&
              function.phiCount == 0u,
          "straight-line load was not rewritten to its stored value");
    verify_promoted_function(&function,
                             "straight-line promotion produced invalid SSA");
    check(ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              function.phiCount == 0u,
          "repeated straight-line promotion changed the function");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_ineligible_place_stays_in_memory(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry;
    TZrExecIrValueId place, stored, loaded;
    TZrExecIrValueId storeOperands[2];

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 102u;
    entry = add_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    place = append_place_base(&function, ZR_FALSE);
    stored = add_value(&function);
    loaded = add_value(&function);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, stored,
                       NULL, 0u, (SZrExecIrRange){0u, 0u});
    storeOperands[0] = place;
    storeOperands[1] = stored;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE,
                       ZR_EXEC_IR_VALUE_ID_INVALID, storeOperands, 2u,
                       (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_LOAD, loaded,
                       &place, 1u, (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN,
                       ZR_EXEC_IR_VALUE_ID_INVALID, &loaded, 1u,
                       (SZrExecIrRange){0u, 0u});
    set_block_instructions(&function, entry, 0u, 5u);

    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic) &&
              ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "process ineligible place");
    check(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_STORE &&
              function.instructions[3].opcode == ZR_EXEC_IR_OPCODE_LOAD &&
              function.phiCount == 0u,
          "ineligible place was promoted");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_read_before_definition_is_transactional(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry;
    TZrExecIrValueId place, loaded;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 105u;
    entry = add_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    place = append_place_base(&function, ZR_TRUE);
    loaded = add_value(&function);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_LOAD, loaded,
                       &place, 1u, (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN,
                       ZR_EXEC_IR_VALUE_ID_INVALID, &loaded, 1u,
                       (SZrExecIrRange){0u, 0u});
    set_block_instructions(&function, entry, 0u, 3u);

    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic),
          "compute read-before-definition dominators");
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 2u &&
              function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_LOAD &&
              function.phiCount == 0u,
          "read-before-definition did not fail transactionally");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_inserts_diamond_phi(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry, left, right, join;
    TZrExecIrBlockId branches[2], joinPredecessors[2];
    TZrExecIrValueId place, condition, leftValue, rightValue, loaded;
    TZrExecIrValueId storeOperands[2];
    const SZrExecIrPhi *phi;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 103u;
    entry = add_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = add_block(&function, 0u);
    right = add_block(&function, 0u);
    join = add_block(&function, 0u);
    branches[0] = left;
    branches[1] = right;
    joinPredecessors[0] = left;
    joinPredecessors[1] = right;
    append_successors(&function, entry, branches, 2u);
    append_successors(&function, left, &join, 1u);
    append_successors(&function, right, &join, 1u);
    append_predecessors(&function, left, &entry, 1u);
    append_predecessors(&function, right, &entry, 1u);
    append_predecessors(&function, join, joinPredecessors, 2u);

    place = append_place_base(&function, ZR_TRUE);
    condition = add_value(&function);
    leftValue = add_value(&function);
    rightValue = add_value(&function);
    loaded = add_value(&function);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, condition,
                       NULL, 0u, (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                       ZR_EXEC_IR_VALUE_ID_INVALID, &condition, 1u,
                       function.blocks[entry - 1u].successorRange);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, leftValue,
                       NULL, 0u, (SZrExecIrRange){0u, 0u});
    storeOperands[0] = place;
    storeOperands[1] = leftValue;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE,
                       ZR_EXEC_IR_VALUE_ID_INVALID, storeOperands, 2u,
                       (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_BRANCH,
                       ZR_EXEC_IR_VALUE_ID_INVALID, NULL, 0u,
                       function.blocks[left - 1u].successorRange);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, rightValue,
                       NULL, 0u, (SZrExecIrRange){0u, 0u});
    storeOperands[1] = rightValue;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE,
                       ZR_EXEC_IR_VALUE_ID_INVALID, storeOperands, 2u,
                       (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_BRANCH,
                       ZR_EXEC_IR_VALUE_ID_INVALID, NULL, 0u,
                       function.blocks[right - 1u].successorRange);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_LOAD, loaded,
                       &place, 1u, (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN,
                       ZR_EXEC_IR_VALUE_ID_INVALID, &loaded, 1u,
                       (SZrExecIrRange){0u, 0u});
    set_block_instructions(&function, entry, 0u, 3u);
    set_block_instructions(&function, left, 3u, 3u);
    set_block_instructions(&function, right, 6u, 3u);
    set_block_instructions(&function, join, 9u, 2u);

    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic) &&
              ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "promote diamond place");
    check(function.blocks[join - 1u].phis.count == 1u,
          "diamond join did not receive one phi");
    phi = &function.phiPool[function.blocks[join - 1u].phis.start];
    check(phi->incomings.count == 2u &&
              function.phiIncoming[phi->incomings.start].predecessor == left &&
              function.phiIncoming[phi->incomings.start].value == leftValue &&
              function.phiIncoming[phi->incomings.start + 1u].predecessor == right &&
              function.phiIncoming[phi->incomings.start + 1u].value == rightValue,
          "diamond phi incomings do not match branch definitions");
    check(function.instructions[4].opcode == ZR_EXEC_IR_OPCODE_NOP &&
              function.instructions[7].opcode == ZR_EXEC_IR_OPCODE_NOP &&
              function.instructions[9].opcode == ZR_EXEC_IR_OPCODE_COPY &&
              function.operands[function.instructions[9].operandRange.start] ==
                      phi->result,
          "diamond memory operations were not rewritten around the phi");
    verify_promoted_function(&function,
                             "diamond promotion produced invalid SSA");
    check(ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              function.blocks[join - 1u].phis.count == 1u,
          "repeated diamond promotion duplicated its phi");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_inserts_loop_carried_phi(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry, header, body, exit;
    TZrExecIrBlockId headerSuccessors[2], headerPredecessors[2];
    TZrExecIrValueId place, initial, headerLoad, next, exitLoad;
    TZrExecIrValueId storeOperands[2];
    const SZrExecIrPhi *phi;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 104u;
    entry = add_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    header = add_block(&function, 0u);
    body = add_block(&function, 0u);
    exit = add_block(&function, 0u);
    headerSuccessors[0] = body;
    headerSuccessors[1] = exit;
    headerPredecessors[0] = entry;
    headerPredecessors[1] = body;
    append_successors(&function, entry, &header, 1u);
    append_successors(&function, header, headerSuccessors, 2u);
    append_successors(&function, body, &header, 1u);
    append_predecessors(&function, header, headerPredecessors, 2u);
    append_predecessors(&function, body, &header, 1u);
    append_predecessors(&function, exit, &header, 1u);

    place = append_place_base(&function, ZR_TRUE);
    initial = add_value(&function);
    headerLoad = add_value(&function);
    next = add_value(&function);
    exitLoad = add_value(&function);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, initial,
                       NULL, 0u, (SZrExecIrRange){0u, 0u});
    storeOperands[0] = place;
    storeOperands[1] = initial;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE,
                       ZR_EXEC_IR_VALUE_ID_INVALID, storeOperands, 2u,
                       (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_BRANCH,
                       ZR_EXEC_IR_VALUE_ID_INVALID, NULL, 0u,
                       function.blocks[entry - 1u].successorRange);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_LOAD, headerLoad,
                       &place, 1u, (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                       ZR_EXEC_IR_VALUE_ID_INVALID, &headerLoad, 1u,
                       function.blocks[header - 1u].successorRange);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT, next,
                       NULL, 0u, (SZrExecIrRange){0u, 0u});
    storeOperands[1] = next;
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE,
                       ZR_EXEC_IR_VALUE_ID_INVALID, storeOperands, 2u,
                       (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_BRANCH,
                       ZR_EXEC_IR_VALUE_ID_INVALID, NULL, 0u,
                       function.blocks[body - 1u].successorRange);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_LOAD, exitLoad,
                       &place, 1u, (SZrExecIrRange){0u, 0u});
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN,
                       ZR_EXEC_IR_VALUE_ID_INVALID, &exitLoad, 1u,
                       (SZrExecIrRange){0u, 0u});
    set_block_instructions(&function, entry, 0u, 4u);
    set_block_instructions(&function, header, 4u, 2u);
    set_block_instructions(&function, body, 6u, 3u);
    set_block_instructions(&function, exit, 9u, 2u);

    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic) &&
              ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "promote loop-carried place");
    check(function.blocks[header - 1u].phis.count == 1u,
          "loop header did not receive one phi");
    phi = &function.phiPool[function.blocks[header - 1u].phis.start];
    check(phi->incomings.count == 2u &&
              function.phiIncoming[phi->incomings.start].predecessor == entry &&
              function.phiIncoming[phi->incomings.start].value == initial &&
              function.phiIncoming[phi->incomings.start + 1u].predecessor == body &&
              function.phiIncoming[phi->incomings.start + 1u].value == next,
          "loop phi incomings do not match entry and backedge definitions");
    check(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_NOP &&
              function.instructions[4].opcode == ZR_EXEC_IR_OPCODE_COPY &&
              function.instructions[7].opcode == ZR_EXEC_IR_OPCODE_NOP &&
              function.instructions[9].opcode == ZR_EXEC_IR_OPCODE_COPY &&
              function.operands[function.instructions[4].operandRange.start] ==
                      phi->result &&
              function.operands[function.instructions[9].operandRange.start] ==
                      phi->result,
          "loop loads and stores were not rewritten through the header phi");
    verify_promoted_function(&function,
                             "loop promotion produced invalid SSA");
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_promotes_straight_line_and_is_repeatable();
    test_ineligible_place_stays_in_memory();
    test_read_before_definition_is_transactional();
    test_inserts_diamond_phi();
    test_inserts_loop_carried_phi();
    puts("ssa place promotion PASS");
    return EXIT_SUCCESS;
}
