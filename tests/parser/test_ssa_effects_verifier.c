#include "zr_vm_core/exec_ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ok(TZrBool value, const char *message) {
    if (!value) { fprintf(stderr, "FAIL: %s\n", message); exit(EXIT_FAILURE); }
}

static SZrExecIrFunction *new_function(SZrExecIrModule *module, TZrExecIrFunctionId *id) {
    SZrExecIrFunction *function;
    ZrCore_ExecIr_ModuleInit(module);
    ok(ZrCore_ExecIr_ModuleAddFunction(module, 1u, 1u, id), "function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, *id);
    ok(function != ZR_NULL, "function query");
    ok(ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
           ZR_EXEC_IR_BLOCK_ID_ENTRY, "entry block");
    return function;
}

static void test_throw_requires_flag(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_DIV;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL), "append div");
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic), "missing throw flag accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE, "wrong throw diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_memory_tokens_must_be_monotonic(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrMemoryTokenId tokens[2] = {2u, 1u};
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, tokens, 2u, &instruction.memoryIn), "append tokens");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL), "append load");
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic), "descending memory tokens accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN, "wrong memory diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_phi_predecessor_set(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming;
    TZrExecIrBlockId predecessor = 99u;
    memset(&phi, 0, sizeof(phi));
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(function, &predecessor, 1u,
                                                &function->blocks[0].predecessors), "append predecessor");
    incoming.predecessor = 42u;
    incoming.value = ZR_EXEC_IR_VALUE_ID_INVALID;
    ok(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, &incoming, 1u,
                                               &phi.incomings), "append phi incoming");
    ok(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u, &function->blocks[0].phis), "append phi");
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic), "phi with foreign predecessor accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH, "wrong phi diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_malformed_memory_range_is_rejected(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    instruction.memoryIn.start = 7u;
    instruction.memoryIn.count = 1u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL), "append malformed load");
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = function->instructionCount;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic), "malformed range accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
           diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY, "range diagnostic lost block");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_memory_inputs_must_advance_across_instructions(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction first;
    SZrExecIrInstruction second;
    TZrExecIrMemoryTokenId firstToken = 2u;
    TZrExecIrMemoryTokenId secondToken = 1u;

    memset(&first, 0, sizeof(first));
    first.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &firstToken, 1u,
                                                 &first.memoryIn), "append first input");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &first, NULL), "append first load");
    memset(&second, 0, sizeof(second));
    second.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &secondToken, 1u,
                                                 &second.memoryIn), "append second input");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &second, NULL), "append second load");
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = function->instructionCount;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "descending memory inputs accepted across instructions");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
       "wrong cross-instruction memory diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_phi_incoming_order_matches_predecessors(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    TZrExecIrBlockId predecessors[2] = {2u, 3u};

    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u, "second block");
    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 3u, "third block");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, 2u,
                                                &function->blocks[0].predecessors),
       "append predecessors");
    memset(&phi, 0, sizeof(phi));
    incoming[0].predecessor = 3u;
    incoming[1].predecessor = 2u;
    incoming[0].value = incoming[1].value = ZR_EXEC_IR_VALUE_ID_INVALID;
    ok(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, 2u,
                                               &phi.incomings), "append phi incoming");
    ok(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u,
                                         &function->blocks[0].phis), "append phi");
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "phi incoming order mismatch accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
       "wrong phi ordering diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_call_binding_row_zero_does_not_require_all_dynamic_flags(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    TZrExecIrMemoryTokenId inputs[1] = {1u};

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.bindingRow = 0u;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, inputs, 1u,
                                                 &instruction.memoryIn), "append call inputs");
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &(TZrExecIrMemoryTokenId){2u}, 1u,
                                                 &instruction.memoryOut), "append call output");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL), "append call");
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = function->instructionCount;
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "call binding row zero rejected without unnecessary flags");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_malformed_block_instruction_range_is_rejected(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;

    function->blocks[0].instructions.start = 1u;
    function->blocks[0].instructions.count = 1u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "malformed block instruction range accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
           diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY,
       "block instruction range diagnostic lost block");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_empty_range_with_invalid_start_is_rejected(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;

    function->blocks[0].instructions.start = UINT32_MAX;
    function->blocks[0].instructions.count = 0u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "empty range with invalid start accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
           diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY,
       "empty invalid range diagnostic lost block");
    ZrCore_ExecIr_FreeModule(&module);
}

int main(void) {
    test_throw_requires_flag();
    test_memory_tokens_must_be_monotonic();
    test_phi_predecessor_set();
    test_malformed_memory_range_is_rejected();
    test_memory_inputs_must_advance_across_instructions();
    test_phi_incoming_order_matches_predecessors();
    test_call_binding_row_zero_does_not_require_all_dynamic_flags();
    test_malformed_block_instruction_range_is_rejected();
    test_empty_range_with_invalid_start_is_rejected();
    puts("ssa effects verifier PASS");
    return EXIT_SUCCESS;
}
