#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_projections.h"

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
    TZrExecIrValueId id = ZrCore_ExecIr_FunctionAddValue(
        function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
        ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(id != ZR_EXEC_IR_VALUE_ID_INVALID, "could not create oracle value");
    return id;
}

static TZrExecIrInstructionId append_instruction(SZrExecIrFunction *function,
                                                   EZrExecIrOpcode opcode,
                                                   SZrExecIrRange operands,
                                                   SZrExecIrRange results,
                                                   SZrExecIrRange successors,
                                                   TZrUInt32 literal) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.successorRange = successors;
    instruction.layoutId = literal;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id),
          "could not append oracle instruction");
    return id;
}

static void build_parallel_phi(SZrExecIrFunction *function) {
    SZrExecIrRange empty = {.start = 0u, .count = 0u};
    SZrExecIrRange results, conditionOperand, returnOperand, incomingRange;
    SZrExecIrPhiIncoming incomings[2] = {{1u, 1u}, {1u, 2u}};
    SZrExecIrPhi phi;
    TZrExecIrBlockId target = 2u, predecessors[2] = {1u, 1u};
    TZrExecIrBlockId successors[2] = {2u, 2u};
    TZrExecIrValueId left, right, condition, merged;

    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 91u;
    left = add_value(function);
    right = add_value(function);
    condition = add_value(function);
    merged = add_value(function);
    check(left == 1u && right == 2u && condition == 3u && merged == 4u,
          "unexpected oracle value IDs");
    check(ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u &&
              ZrCore_ExecIr_FunctionAddBlock(function, 0u) == target,
          "could not build parallel-edge blocks");
    function->entryBlockId = 1u;
    check(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                                                 &function->blocks[0].successorRange) &&
              ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, 2u,
                                                        &function->blocks[1].predecessorRange),
          "could not append both CFG edges");
    check(ZrCore_ExecIr_FunctionAppendResults(function, &left, 1u, &results),
          "could not create first definition");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, empty, results,
                             empty, 11u) == 1u, "wrong first instruction ID");
    check(ZrCore_ExecIr_FunctionAppendResults(function, &right, 1u, &results),
          "could not create second definition");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, empty, results,
                             empty, 22u) == 2u, "wrong second instruction ID");
    check(ZrCore_ExecIr_FunctionAppendResults(function, &condition, 1u, &results),
          "could not create branch condition");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, empty, results,
                             empty, 0u) == 3u, "wrong condition instruction ID");
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u,
                                               &conditionOperand),
          "could not create condition operand");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
                             conditionOperand, empty, function->blocks[0].successorRange,
                             0u) == 4u, "wrong branch instruction ID");
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &merged, 1u,
                                               &returnOperand),
          "could not create return operand");
    check(append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperand,
                             empty, empty, 0u) == 5u, "wrong return instruction ID");
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = 4u;
    function->blocks[0].terminatorInstructionId = 4u;
    function->blocks[1].instructionRange.start = 4u;
    function->blocks[1].instructionRange.count = 1u;
    function->blocks[1].terminatorInstructionId = 5u;
    check(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incomings, 2u,
                                                   &incomingRange),
          "could not create parallel phi incoming slots");
    phi.result = merged;
    phi.incomings = incomingRange;
    check(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u,
                                          &function->blocks[1].phis),
          "could not create parallel phi");
}

static void test_branch_and_switch_parallel_edges_select_distinct_incomings(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrOracleInput input;
    SZrExecIrOracleValue condition;
    TZrUInt32 caseIndex, opcodeIndex;

    build_parallel_phi(&function);
    TZrBool verified = ZrCore_ExecIr_VerifyFunction(
        &function, ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA,
        &diagnostic);
    if (!verified)
        fprintf(stderr, "verifier diagnostic code=%u block=%u instruction=%u expected=%u actual=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.expectedVersion, (unsigned)diagnostic.actualVersion);
    check(verified,
          "core verifier rejected a valid parallel-edge phi");
    memset(&input, 0, sizeof(input));
    input.function = &function;
    input.constants = &condition;
    input.constantCount = 1u;
    for (opcodeIndex = 0u; opcodeIndex != 2u; ++opcodeIndex) {
        function.instructions[3].opcode = opcodeIndex == 0u
            ? ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH : ZR_EXEC_IR_OPCODE_SWITCH;
        for (caseIndex = 0u; caseIndex != 2u; ++caseIndex) {
            condition.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            condition.as.boolean = (TZrBool)(caseIndex == 0u);
            ZrCore_ExecIr_OracleResultInit(&execution);
            TZrBool interpreted = ZrCore_ExecIr_RunOracleEx(
                &input, &execution, &diagnostic);
            if (!interpreted)
                fprintf(stderr, "oracle diagnostic code=%u block=%u instruction=%u expected=%u actual=%u\n",
                        (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                        (unsigned)diagnostic.instructionId,
                        (unsigned)diagnostic.expectedVersion, (unsigned)diagnostic.actualVersion);
            check(interpreted,
                  "oracle rejected parallel CFG edges with ordered phi incoming slots");
            check(execution.returned &&
                      execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                      execution.returnValue.as.signedInteger ==
                          (opcodeIndex == 0u
                               ? (caseIndex == 0u ? 11 : 22)
                               : (caseIndex == 0u ? 22 : 11)),
                  "oracle merged the two parallel edges into one phi input");
            ZrCore_ExecIr_OracleResultFree(&execution);
        }
    }
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_selected_edge_missing_from_source_adjacency(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrOracleInput input;
    SZrExecIrOracleValue condition;

    build_parallel_phi(&function);
    /* The instruction still selects its second successor, but the source
     * block advertises only its first adjacency occurrence. */
    function.blocks[0].successorRange.count = 1u;
    memset(&input, 0, sizeof(input));
    input.function = &function;
    condition.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
    condition.as.boolean = ZR_FALSE;
    input.constants = &condition;
    input.constantCount = 1u;
    ZrCore_ExecIr_OracleResultInit(&execution);
    check(!ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH &&
              diagnostic.blockId == 2u && diagnostic.instructionId == 4u &&
              execution.values == NULL,
          "oracle accepted a phi edge omitted by source adjacency");
    ZrCore_ExecIr_OracleResultFree(&execution);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_projections_preserve_parallel_phi_edges(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcProjection bytecode = {0};
    SZrAotIrProjection aot = {0};

    build_parallel_phi(&function);
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic),
          "ExecBC projection rejected a valid parallel-edge phi");
    check(bytecode.syntheticBlockCount == 2u && bytecode.blockCount == 4u &&
              bytecode.phiCopyCount == 2u &&
              bytecode.successors[bytecode.blocks[0].successors.start] == 3u &&
              bytecode.successors[bytecode.blocks[0].successors.start + 1u] == 4u &&
              bytecode.predecessors[bytecode.blocks[1].predecessors.start] == 3u &&
              bytecode.predecessors[bytecode.blocks[1].predecessors.start + 1u] == 4u &&
              bytecode.phiIncomings[0].predecessor == 3u &&
              bytecode.phiIncomings[1].predecessor == 4u &&
              bytecode.phiCopySources[0] == 1u &&
              bytecode.phiCopySources[1] == 2u &&
              bytecode.phiCopyDestinations[0] == 4u &&
              bytecode.phiCopyDestinations[1] == 4u &&
              bytecode.phiCopyEdges[0] == 3u &&
              bytecode.phiCopyEdges[1] == 4u &&
              bytecode.predecessors[bytecode.blocks[2].predecessors.start] == 1u &&
              bytecode.predecessors[bytecode.blocks[3].predecessors.start] == 1u &&
              bytecode.successors[bytecode.blocks[2].successors.start] == 2u &&
              bytecode.successors[bytecode.blocks[3].successors.start] == 2u,
          "ExecBC projection merged the two phi edge copies");
    check(ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic),
          "AOTIR projection rejected a valid parallel-edge phi");
    check(aot.syntheticBlockCount == 2u && aot.phiCopyCount == 2u &&
              aot.phiCopyEdges[0] == 3u && aot.phiCopyEdges[1] == 4u &&
              aot.phiCopySources[0] == 1u && aot.phiCopySources[1] == 2u &&
              aot.predecessors[aot.blocks[1].predecessors.start] == 3u &&
              aot.predecessors[aot.blocks[1].predecessors.start + 1u] == 4u &&
              aot.phiIncomings[0].predecessor == 3u &&
              aot.phiIncomings[1].predecessor == 4u &&
              !aot.runnable,
          "AOTIR projection discarded the parallel phi edge identities");
    check(function.blockCount == 2u && function.predecessors[0] == 1u &&
              function.predecessors[1] == 1u,
          "projection modified the source ExecIR function");
    ZrParser_AotIrProjection_Free(&aot);
    ZrParser_ExecBcProjection_Free(&bytecode);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_projections_reject_unpaired_edge_without_replacing_output(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecBcProjection bytecode = {0};
    SZrAotIrProjection aot = {0};
    TZrExecIrBlockId *originalEdges;

    build_parallel_phi(&function);
    check(ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic),
          "could not prepare valid projection for failure-atomicity test");
    originalEdges = bytecode.phiCopyEdges;
    function.blocks[0].successorRange.count = 1u;
    check(!ZrParser_ExecIr_LowerExecBc(&function, &bytecode, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              bytecode.phiCopyEdges == originalEdges &&
              bytecode.phiCopyCount == 2u,
          "projection accepted an unmatched incoming duplicate or replaced output");
    function.blocks[0].successorRange.count = 2u;
    function.blocks[1].predecessorRange.count = 1u;
    check(!ZrParser_ExecIr_LowerAot(&function, &aot, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              aot.phiCopyCount == 2u && aot.phiCopyEdges[1] == 4u,
          "AOT projection accepted an unmatched outgoing duplicate or replaced output");
    ZrParser_AotIrProjection_Free(&aot);
    ZrParser_ExecBcProjection_Free(&bytecode);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_branch_and_switch_parallel_edges_select_distinct_incomings();
    test_rejects_selected_edge_missing_from_source_adjacency();
    test_projections_preserve_parallel_phi_edges();
    test_projections_reject_unpaired_edge_without_replacing_output();
    puts("ssa oracle parallel edges PASS");
    return EXIT_SUCCESS;
}
