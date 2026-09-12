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

int main(void) {
    test_throw_requires_flag();
    test_memory_tokens_must_be_monotonic();
    test_phi_predecessor_set();
    puts("ssa effects verifier PASS");
    return EXIT_SUCCESS;
}
