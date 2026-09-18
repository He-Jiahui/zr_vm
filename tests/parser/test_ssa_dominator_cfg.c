#include "zr_vm_parser/exec_ir_builder.h"

#include <stdio.h>
#include <stdlib.h>

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static TZrExecIrBlockId append_block(SZrExecIrFunction *function, TZrUInt32 flags) {
    TZrExecIrBlockId id = ZrCore_ExecIr_FunctionAddBlock(function, flags);
    check(id != ZR_EXEC_IR_BLOCK_ID_INVALID, "append CFG block");
    return id;
}

static void append_successors(SZrExecIrFunction *function, TZrExecIrBlockId block,
                              const TZrExecIrBlockId *edges, TZrUInt32 count) {
    check(ZrCore_ExecIr_FunctionAppendSuccessors(function, edges, count,
                &function->blocks[block - 1u].successorRange), "append CFG successors");
}

static void append_predecessors(SZrExecIrFunction *function, TZrExecIrBlockId block,
                                const TZrExecIrBlockId *edges, TZrUInt32 count) {
    check(ZrCore_ExecIr_FunctionAppendPredecessors(function, edges, count,
                &function->blocks[block - 1u].predecessorRange), "append CFG predecessors");
}

static void test_rejects_invalid_successor_before_traversal(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId invalid = 2u;

    ZrCore_ExecIr_FunctionInit(&function);
    entry = append_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    append_successors(&function, entry, &invalid, 1u);
    check(!ZrParser_ExecIr_ComputeDominators(&function, &diagnostic),
          "out-of-range successor was silently skipped");
    check(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              diagnostic.blockId == entry && diagnostic.expectedVersion == 1u &&
              diagnostic.actualVersion == invalid,
          "invalid successor diagnostic lost edge identity");
    check(function.blocks[0].immediateDominator == ZR_EXEC_IR_BLOCK_ID_INVALID,
          "malformed graph changed cached dominators");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_diamond_and_repeated_analysis(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry, left, right, join;
    TZrExecIrBlockId branches[2];
    TZrExecIrBlockId incoming[2];

    ZrCore_ExecIr_FunctionInit(&function);
    entry = append_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = append_block(&function, 0u);
    right = append_block(&function, 0u);
    join = append_block(&function, 0u);
    branches[0] = left;
    branches[1] = right;
    incoming[0] = left;
    incoming[1] = right;
    append_successors(&function, entry, branches, 2u);
    append_successors(&function, left, &join, 1u);
    append_successors(&function, right, &join, 1u);
    append_predecessors(&function, left, &entry, 1u);
    append_predecessors(&function, right, &entry, 1u);
    append_predecessors(&function, join, incoming, 2u);

    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic),
          "valid diamond CFG rejected");
    check(function.blocks[left - 1u].immediateDominator == entry &&
              function.blocks[right - 1u].immediateDominator == entry &&
              function.blocks[join - 1u].immediateDominator == entry,
          "diamond join has the wrong immediate dominator");
    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic),
          "repeating dominator analysis failed");
    check(function.blocks[join - 1u].immediateDominator == entry,
          "repeated analysis changed diamond dominator");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_backedge_and_unreachable_block(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry, header, body, unreachable;
    TZrExecIrBlockId backedge[2];

    ZrCore_ExecIr_FunctionInit(&function);
    entry = append_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    header = append_block(&function, 0u);
    body = append_block(&function, 0u);
    unreachable = append_block(&function, 0u);
    backedge[0] = entry;
    backedge[1] = body;
    append_successors(&function, entry, &header, 1u);
    append_successors(&function, header, &body, 1u);
    append_successors(&function, body, &header, 1u);
    append_predecessors(&function, header, backedge, 2u);
    append_predecessors(&function, body, &header, 1u);
    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic),
          "valid loop CFG rejected");
    check(function.blocks[header - 1u].immediateDominator == entry &&
              function.blocks[body - 1u].immediateDominator == header &&
              function.blocks[unreachable - 1u].immediateDominator ==
                  ZR_EXEC_IR_BLOCK_ID_INVALID,
          "loop or unreachable dominator was incorrect");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_mismatched_predecessor_adjacency(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry, target, unrelated;

    ZrCore_ExecIr_FunctionInit(&function);
    entry = append_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    target = append_block(&function, 0u);
    unrelated = append_block(&function, 0u);
    append_successors(&function, entry, &target, 1u);
    append_predecessors(&function, target, &unrelated, 1u);
    check(!ZrParser_ExecIr_ComputeDominators(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH &&
              diagnostic.blockId == target &&
              diagnostic.expectedVersion == entry &&
              diagnostic.actualVersion == unrelated &&
              function.blocks[target - 1u].immediateDominator ==
                  ZR_EXEC_IR_BLOCK_ID_INVALID,
          "dominator computation accepted a fabricated in-range predecessor");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_missing_parallel_predecessor_occurrence(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry, target, repeated[2];

    ZrCore_ExecIr_FunctionInit(&function);
    entry = append_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    target = append_block(&function, 0u);
    repeated[0] = target;
    repeated[1] = target;
    append_successors(&function, entry, repeated, 2u);
    append_predecessors(&function, target, &entry, 1u);
    check(!ZrParser_ExecIr_ComputeDominators(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH &&
              diagnostic.expectedVersion == 2u && diagnostic.actualVersion == 1u &&
              function.blocks[target - 1u].immediateDominator ==
                  ZR_EXEC_IR_BLOCK_ID_INVALID,
          "dominator computation collapsed a parallel CFG edge");
    ZrCore_ExecIr_FreeFunction(&function);

    ZrCore_ExecIr_FunctionInit(&function);
    entry = append_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    target = append_block(&function, 0u);
    repeated[0] = entry;
    repeated[1] = entry;
    append_successors(&function, entry, &target, 1u);
    append_predecessors(&function, target, repeated, 2u);
    check(!ZrParser_ExecIr_ComputeDominators(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 2u,
          "dominator computation accepted an extra predecessor occurrence");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_accepts_parallel_edge_occurrences(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry, target, repeatedSuccessors[2], repeatedPredecessors[2];

    ZrCore_ExecIr_FunctionInit(&function);
    entry = append_block(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    target = append_block(&function, 0u);
    repeatedSuccessors[0] = target;
    repeatedSuccessors[1] = target;
    repeatedPredecessors[0] = entry;
    repeatedPredecessors[1] = entry;
    append_successors(&function, entry, repeatedSuccessors, 2u);
    append_predecessors(&function, target, repeatedPredecessors, 2u);
    check(ZrParser_ExecIr_ComputeDominators(&function, &diagnostic) &&
              function.blocks[target - 1u].immediateDominator == entry,
          "matching parallel CFG edges did not retain their common dominator");
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_rejects_invalid_successor_before_traversal();
    test_diamond_and_repeated_analysis();
    test_backedge_and_unreachable_block();
    test_rejects_mismatched_predecessor_adjacency();
    test_rejects_missing_parallel_predecessor_occurrence();
    test_accepts_parallel_edge_occurrences();
    puts("ssa dominator CFG PASS");
    return EXIT_SUCCESS;
}
