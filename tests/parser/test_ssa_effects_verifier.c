#include "zr_vm_core/exec_ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exec_ir/exec_ir_mutation_internal.h"

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

static void append_effect_call(SZrExecIrFunction *function,
                               TZrExecIrMemoryTokenId memoryIn,
                               TZrExecIrMemoryTokenId memoryOut,
                               TZrExecIrEffectTokenId effectIn,
                               TZrExecIrEffectTokenId effectOut) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.effectIn = effectIn;
    instruction.effectOut = effectOut;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &memoryIn, 1u,
                                                 &instruction.memoryIn),
       "append effect call input");
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &memoryOut, 1u,
                                                 &instruction.memoryOut),
       "append effect call output");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append effect call");
}

static void build_effect_diamond(SZrExecIrFunction *function) {
    TZrExecIrBlockId predecessor;
    TZrExecIrBlockId mergePredecessors[2] = {2u, 3u};
    TZrUInt32 blockIndex;

    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u,
       "effect diamond left block");
    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 3u,
       "effect diamond right block");
    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 4u,
       "effect diamond merge block");
    append_effect_call(function, 1u, 2u, 1u, 2u);
    append_effect_call(function, 2u, 3u, 2u, 3u);
    append_effect_call(function, 3u, 4u, 2u, 4u);
    append_effect_call(function, 4u, 5u, 5u, 6u);
    for (blockIndex = 0u; blockIndex < 4u; ++blockIndex) {
        function->blocks[blockIndex].instructions.start = blockIndex;
        function->blocks[blockIndex].instructions.count = 1u;
    }
    predecessor = 1u;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[1].predecessorRange),
       "effect diamond left predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[2].predecessorRange),
       "effect diamond right predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, mergePredecessors, 2u,
               &function->blocks[3].predecessorRange),
       "effect diamond merge predecessors");
}

static void test_effect_phi_joins_distinct_cfg_chains(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhiIncoming incoming[2] = {{2u, 3u}, {3u, 4u}};

    build_effect_diamond(function);
    ok(ZrCore_ExecIr_FunctionSetEffectPhi(function, 4u, 5u, incoming, 2u),
       "set effect phi");
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "distinct CFG effect chains rejected at explicit phi");

    function->phiIncoming[function->blocks[3].effectPhiIncomings.start + 1u].value = 3u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "stale effect phi incoming accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN &&
           diagnostic.expectedVersion == 4u && diagnostic.actualVersion == 3u,
       "stale effect phi diagnostic lost edge token versions");
    function->phiIncoming[function->blocks[3].effectPhiIncomings.start + 1u].value = 4u;
    function->instructions[3].effectIn = 4u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "effect instruction bypassed merge token accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN &&
           diagnostic.expectedVersion == 5u && diagnostic.actualVersion == 4u,
       "merge token diagnostic lost first consumer");
    function->instructions[3].effectIn = 5u;
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "valid effect phi chain rejected after repair");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_effect_join_requires_phi_for_distinct_predecessors(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;

    build_effect_diamond(function);
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "distinct CFG effect chains joined without phi");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN &&
           diagnostic.blockId == 4u && diagnostic.instructionId == 4u,
       "missing effect phi diagnostic lost merge location");
    ZrCore_ExecIr_FreeModule(&module);
}

static void append_tagged_store(SZrExecIrFunction *function,
                                TZrExecIrMemoryTokenId output,
                                TZrExecIrEffectTokenId effectIn,
                                TZrExecIrEffectTokenId effectOut) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_STORE;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.effectIn = effectIn;
    instruction.effectOut = effectOut;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &output, 1u,
                                                 &instruction.memoryOut),
       "append tagged store output");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append tagged store instruction");
}

static void append_tagged_load(SZrExecIrFunction *function,
                               TZrExecIrMemoryTokenId input) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &input, 1u,
                                                 &instruction.memoryIn),
       "append tagged load input");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append tagged load instruction");
}

static void build_memory_diamond(SZrExecIrFunction *function) {
    TZrExecIrBlockId predecessor;
    TZrExecIrBlockId mergePredecessors[2] = {2u, 3u};
    TZrUInt32 blockIndex;
    TZrExecIrMemoryTokenId heap1 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    TZrExecIrMemoryTokenId heap2 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u);
    TZrExecIrMemoryTokenId heap3 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 3u);
    TZrExecIrMemoryTokenId heap4 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 4u);

    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u,
       "memory diamond left block");
    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 3u,
       "memory diamond right block");
    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 4u,
       "memory diamond merge block");
    append_tagged_store(function, heap1, 1u, 2u);
    append_tagged_store(function, heap2, 2u, 3u);
    append_tagged_store(function, heap3, 2u, 4u);
    append_tagged_load(function, heap4);
    for (blockIndex = 0u; blockIndex < 4u; ++blockIndex) {
        function->blocks[blockIndex].instructions.start = blockIndex;
        function->blocks[blockIndex].instructions.count = 1u;
    }
    predecessor = 1u;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[1].predecessorRange),
       "memory diamond left predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[2].predecessorRange),
       "memory diamond right predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, mergePredecessors, 2u,
               &function->blocks[3].predecessorRange),
       "memory diamond merge predecessors");
}

static void test_memory_phi_joins_region_versions(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhiIncoming incoming[2] = {
        {2u, ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u)},
        {3u, ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 3u)}};
    TZrExecIrMemoryTokenId merged = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 4u);
    TZrExecIrMemoryTokenId stale = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u);

    build_memory_diamond(function);
    ok(ZrCore_ExecIr_FunctionSetMemoryPhi(
               function, 4u, ZR_EXEC_IR_MEMORY_MANAGED_HEAP,
               merged, incoming, 2u),
       "set managed-heap memory phi");
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "region memory phi rejected");

    function->phiIncoming[function->blocks[3].memoryPhiIncomings[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start + 1u].value = stale;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "stale region memory phi incoming accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
       "stale region memory phi lost memory diagnostic");
    function->phiIncoming[function->blocks[3].memoryPhiIncomings[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start + 1u].value = incoming[1].value;
    function->memoryTokenPool[function->instructions[3].memoryIn.start] = stale;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "region memory consumer bypassed phi accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
       "region memory consumer diagnostic lost code");
    function->memoryTokenPool[function->instructions[3].memoryIn.start] = merged;
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "repaired region memory phi rejected");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_memory_join_requires_region_phi(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;

    build_memory_diamond(function);
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "distinct region versions joined without memory phi");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic.blockId == 4u && diagnostic.instructionId == 4u,
       "missing region memory phi lost merge location");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_loop_carried_effect_and_memory_phis(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId header;
    TZrExecIrBlockId body;
    TZrExecIrBlockId exitBlock;
    TZrExecIrBlockId entry = 1u;
    TZrExecIrBlockId headerPreds[2] = {entry, 3u};
    SZrExecIrPhiIncoming effectIncoming[2] = {{entry, 2u}, {3u, 4u}};
    TZrExecIrMemoryTokenId heap1 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    TZrExecIrMemoryTokenId heap2 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u);
    TZrExecIrMemoryTokenId heap3 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 3u);
    TZrExecIrMemoryTokenId heap4 = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 4u);
    SZrExecIrPhiIncoming memoryIncoming[2] = {{entry, heap1}, {3u, heap3}};
    TZrExecIrBlockId predecessor;
    SZrExecIrRange ranges[4] = {0};
    SZrExecIrInstruction instruction;

    header = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    body = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    exitBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    ok(header == 2u && body == 3u && exitBlock == 4u, "loop phi blocks");
    append_tagged_store(function, heap1, 1u, 2u);
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "loop header branch");
    append_tagged_store(function, heap3, 3u, 4u);
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "loop exit return");
    ranges[0].start = 0u; ranges[0].count = 1u;
    ranges[1].start = 1u; ranges[1].count = 1u;
    ranges[2].start = 2u; ranges[2].count = 1u;
    ranges[3].start = 3u; ranges[3].count = 1u;
    function->blocks[0].instructionRange = ranges[0];
    function->blocks[1].instructionRange = ranges[1];
    function->blocks[2].instructionRange = ranges[2];
    function->blocks[3].instructionRange = ranges[3];
    predecessor = header;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
                         function, &predecessor, 1u,
                         &function->blocks[2].predecessorRange),
                 "loop body predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
                         function, headerPreds, 2u,
                         &function->blocks[1].predecessorRange),
                 "loop header predecessors");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
                         function, &header, 1u,
                         &function->blocks[3].predecessorRange),
                 "loop exit predecessor");
    ok(ZrCore_ExecIr_FunctionSetEffectPhi(
                         function, header, 3u, effectIncoming, 2u),
                 "set loop effect phi");
    ok(ZrCore_ExecIr_FunctionSetMemoryPhi(
                         function, header, ZR_EXEC_IR_MEMORY_MANAGED_HEAP,
                         heap2, memoryIncoming, 2u),
                 "set loop memory phi");
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "loop-carried effect and memory phis rejected");
    function->phiIncoming[function->blocks[header - 1u].memoryPhiIncomings[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start + 1u].value = heap2;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "stale loop backedge memory token accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic.blockId == header && diagnostic.expectedVersion == heap3 &&
           diagnostic.actualVersion == heap2,
       "stale loop backedge diagnostic lost token identity");
    function->phiIncoming[function->blocks[header - 1u].memoryPhiIncomings[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start + 1u].value = heap3;
    function->phiIncoming[function->blocks[header - 1u].effectPhiIncomings.start + 1u]
            .value = 3u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "stale loop backedge effect token accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN &&
           diagnostic.blockId == header && diagnostic.expectedVersion == 4u &&
           diagnostic.actualVersion == 3u,
       "stale loop effect diagnostic lost token identity");
    function->phiIncoming[function->blocks[header - 1u].effectPhiIncomings.start + 1u]
            .value = 4u;
    function->predecessors[function->blocks[body - 1u].predecessors.start] = entry;
    function->instructions[2].effectIn = 2u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "reverse-declared acyclic edge accepted as loop backedge");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic.blockId == header,
       "non-dominating reverse predecessor lost memory phi diagnostic");
    function->blocks[header - 1u].memoryPhiResults[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP] = heap4;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "reverse-declared acyclic effect edge accepted as loop backedge");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN &&
           diagnostic.blockId == header,
       "non-dominating reverse predecessor lost effect phi diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
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

static void test_tagged_memory_tokens_are_region_local(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    TZrExecIrMemoryTokenId heapOut = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    TZrExecIrMemoryTokenId ownershipIn = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_OWNERSHIP, 1u);
    TZrExecIrMemoryTokenId ownershipOut = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_OWNERSHIP, 2u);

    append_tagged_store(function, heapOut, 1u, 2u);
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_DROP;
    instruction.effectIn = 2u;
    instruction.effectOut = 3u;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &ownershipIn, 1u,
                                                 &instruction.memoryIn),
       "append tagged ownership input");
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &ownershipOut, 1u,
                                                 &instruction.memoryOut),
       "append tagged ownership output");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append tagged drop");
    function->blocks[0].instructions.count = function->instructionCount;

    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "independent tagged memory regions imposed a false order");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_tagged_call_requires_all_declared_memory_regions(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    TZrExecIrMemoryTokenId memoryIn[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)};
    TZrExecIrMemoryTokenId memoryOut[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)};

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 481u;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryIn, 2u,
                                                 &instruction.memoryIn),
       "append tagged call inputs");
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryOut, 2u,
                                                 &instruction.memoryOut),
       "append tagged call outputs");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append complete tagged call");
    function->blocks[0].instructions.count = function->instructionCount;

    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "complete tagged call rejected");

    function->instructions[0].memoryIn.count = 1u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "tagged call missing a declared memory input region accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic.functionToken == 1u && diagnostic.blockId == 1u &&
           diagnostic.instructionId == 1u && diagnostic.sourceId == 481u,
       "missing call input region diagnostic lost location");

    function->instructions[0].memoryIn.count = 2u;
    function->instructions[0].memoryOut.count = 1u;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "tagged call missing a declared memory output region accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic.functionToken == 1u && diagnostic.blockId == 1u &&
           diagnostic.instructionId == 1u && diagnostic.sourceId == 481u,
       "missing call output region diagnostic lost location");

    ZrCore_ExecIr_FreeModule(&module);
}

static void test_tagged_memory_token_region_must_match_schema(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    TZrExecIrMemoryTokenId nativeToken = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u);

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &nativeToken, 1u,
                                                 &instruction.memoryIn),
       "append wrong-region load token");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append wrong-region load");
    function->blocks[0].instructions.count = function->instructionCount;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "wrong-region memory token accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
       "wrong-region memory diagnostic lost code");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_tagged_store_and_load_share_region_version(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction store;
    SZrExecIrInstruction load;
    TZrExecIrMemoryTokenId heapToken = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    TZrExecIrMemoryTokenId futureToken = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u);

    memset(&store, 0, sizeof(store));
    store.opcode = ZR_EXEC_IR_OPCODE_STORE;
    store.flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    store.effectIn = 1u;
    store.effectOut = 2u;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &heapToken, 1u,
                                                 &store.memoryOut),
       "append tagged store output");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &store, NULL),
       "append tagged store");

    memset(&load, 0, sizeof(load));
    load.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &heapToken, 1u,
                                                 &load.memoryIn),
       "append tagged load input");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &load, NULL),
       "append tagged load");
    function->blocks[0].instructions.count = function->instructionCount;

    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "tagged load did not consume the store's region version");

    memset(&load, 0, sizeof(load));
    load.opcode = ZR_EXEC_IR_OPCODE_LOAD;
    load.sourceId = 903u;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &futureToken, 1u,
                                                 &load.memoryIn),
       "append future tagged load input");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &load, NULL),
       "append future tagged load");
    function->blocks[0].instructions.count = function->instructionCount;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "read skipped a tagged region version without a write");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
           diagnostic.instructionId == 3u &&
           diagnostic.sourceId == 903u &&
           diagnostic.expectedVersion == 1u &&
           diagnostic.actualVersion == 2u,
       "future tagged read lost its exact memory diagnostic");
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

static void test_phi_accepts_parallel_edge_occurrences(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhi phi = {0};
    SZrExecIrPhiIncoming incoming[2] = {{1u, 0u}, {1u, 0u}};
    TZrExecIrBlockId successors[2] = {2u, 2u};
    TZrExecIrBlockId predecessors[2] = {1u, 1u};

    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u,
       "parallel target block");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(function, successors, 2u,
                                              &function->blocks[0].successors),
       "parallel successors");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(function, predecessors, 2u,
                                                &function->blocks[1].predecessors),
       "parallel predecessors");
    ok(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, 2u,
                                               &phi.incomings), "parallel phi inputs");
    ok(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u,
                                       &function->blocks[1].phis), "parallel phi");
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "effect verifier rejected two distinct incoming CFG edge occurrences");
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

static void test_observable_effect_chain_rejects_skipped_version(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(&module, &id);
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    TZrExecIrMemoryTokenId tokens[] = {1u, 2u, 2u, 3u};
    TZrUInt32 index;

    for (index = 0u; index < 2u; ++index) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
        instruction.flags = ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
        instruction.sourceId = 551u + index;
        instruction.effectIn = index == 0u ? 1u : 3u;
        instruction.effectOut = index == 0u ? 2u : 4u;
        ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &tokens[index * 2u], 1u,
                                                     &instruction.memoryIn), "append effect input");
        ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &tokens[index * 2u + 1u], 1u,
                                                     &instruction.memoryOut), "append effect output");
        ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
           "append observable call");
        if (index == 0u) {
            memset(&instruction, 0, sizeof(instruction));
            instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
            ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
               "append intervening pure instruction");
        }
    }
    function->blocks[0].instructions.count = function->instructionCount;
    ok(!ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "an observable effect skipped its preceding token");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN &&
           diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
           diagnostic.instructionId == 3u && diagnostic.sourceId == 552u &&
           diagnostic.expectedVersion == 2u && diagnostic.actualVersion == 3u,
       "skipped effect diagnostic lost the producing token or location");
    function->instructions[2].effectIn = 2u;
    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "contiguous observable effect chain rejected");
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

static void test_ssa_rejects_use_before_definition_in_linear_ir(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrRange operands;
    SZrExecIrRange results;
    TZrExecIrValueId source;
    TZrExecIrValueId copy;

    ZrCore_ExecIr_ModuleInit(&module);
    ok(ZrCore_ExecIr_ModuleAddFunction(&module, 900u, 1u, &id),
       "linear dominance function");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    source = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    copy = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    ok(source != ZR_EXEC_IR_VALUE_ID_INVALID &&
           copy != ZR_EXEC_IR_VALUE_ID_INVALID,
       "linear dominance values");
    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &source, 1u,
                                             &operands),
       "linear dominance operands");
    ok(ZrCore_ExecIr_FunctionAppendResults(function, &copy, 1u, &results),
       "linear dominance result");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_COPY;
    instruction.operands = operands;
    instruction.results = results;
    instruction.sourceId = 901u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "linear use-before-definition instruction");

    ok(ZrCore_ExecIr_FunctionAppendResults(function, &source, 1u, &results),
       "linear late definition result");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = results;
    instruction.layoutId = 7u;
    instruction.sourceId = 902u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "linear late definition instruction");

    ok(!ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "linear use-before-definition accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE &&
           diagnostic.instructionId == 1u && diagnostic.sourceId == 901u &&
           diagnostic.expectedVersion == 2u && diagnostic.actualVersion == source,
       "linear dominance diagnostic lost definition/use identity");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_ssa_rejects_cross_branch_use_not_dominated(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrRange conditionOperands;
    SZrExecIrRange sourceResult;
    SZrExecIrRange returnOperands;
    TZrExecIrValueId condition;
    TZrExecIrValueId source;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId left;
    TZrExecIrBlockId right;
    TZrExecIrBlockId successors[2];
    TZrExecIrBlockId predecessor;

    ZrCore_ExecIr_ModuleInit(&module);
    ok(ZrCore_ExecIr_ModuleAddFunction(&module, 910u, 1u, &id),
       "branch dominance function");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    condition = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    source = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    ok(condition != ZR_EXEC_IR_VALUE_ID_INVALID &&
           source != ZR_EXEC_IR_VALUE_ID_INVALID,
       "branch dominance values");
    entry = ZrCore_ExecIr_FunctionAddBlock(function,
                                             ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    right = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    ok(entry == 1u && left == 2u && right == 3u,
       "branch dominance blocks");
    function->entryBlockId = entry;
    successors[0] = left;
    successors[1] = right;
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, successors, 2u,
               &function->blocks[entry - 1u].successorRange),
       "branch dominance successors");
    predecessor = entry;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[left - 1u].predecessorRange),
       "left dominance predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[right - 1u].predecessorRange),
       "right dominance predecessor");
    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u,
                                             &conditionOperands),
       "branch condition operands");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH;
    instruction.operands = conditionOperands;
    instruction.successorRange = function->blocks[entry - 1u].successorRange;
    instruction.sourceId = 911u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "branch terminator");

    ok(ZrCore_ExecIr_FunctionAppendResults(function, &source, 1u,
                                           &sourceResult),
       "branch late definition result");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = sourceResult;
    instruction.layoutId = 3u;
    instruction.sourceId = 912u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "branch left definition");
    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &source, 1u,
                                             &returnOperands),
       "branch return operands");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperands;
    instruction.sourceId = 913u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "branch left return");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperands;
    instruction.sourceId = 914u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "branch right return");

    function->blocks[entry - 1u].instructionRange.start = 0u;
    function->blocks[entry - 1u].instructionRange.count = 1u;
    function->blocks[entry - 1u].terminatorInstructionId = 1u;
    function->blocks[left - 1u].instructionRange.start = 1u;
    function->blocks[left - 1u].instructionRange.count = 2u;
    function->blocks[left - 1u].terminatorInstructionId = 3u;
    function->blocks[right - 1u].instructionRange.start = 3u;
    function->blocks[right - 1u].instructionRange.count = 1u;
    function->blocks[right - 1u].terminatorInstructionId = 4u;

    ok(!ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "cross-branch non-dominated use accepted");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE &&
           diagnostic.blockId == right && diagnostic.instructionId == 4u &&
           diagnostic.sourceId == 914u && diagnostic.expectedVersion == 2u &&
           diagnostic.actualVersion == source,
       "cross-branch dominance diagnostic lost edge identity");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_ssa_accepts_phi_edge_definitions_and_rejects_wrong_edge(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    SZrExecIrRange conditionOperands;
    SZrExecIrRange leftResult;
    SZrExecIrRange rightResult;
    SZrExecIrRange returnOperands;
    SZrExecIrRange incomingRange;
    SZrExecIrRange phiRange;
    TZrExecIrValueId condition;
    TZrExecIrValueId leftValue;
    TZrExecIrValueId rightValue;
    TZrExecIrValueId merged;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId left;
    TZrExecIrBlockId right;
    TZrExecIrBlockId merge;
    TZrExecIrBlockId successors[2];
    TZrExecIrBlockId mergePredecessors[2];
    TZrExecIrBlockId predecessor;

    ZrCore_ExecIr_ModuleInit(&module);
    ok(ZrCore_ExecIr_ModuleAddFunction(&module, 920u, 1u, &id),
       "phi dominance function");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    condition = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    leftValue = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    rightValue = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    merged = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    ok(condition != ZR_EXEC_IR_VALUE_ID_INVALID &&
           leftValue != ZR_EXEC_IR_VALUE_ID_INVALID &&
           rightValue != ZR_EXEC_IR_VALUE_ID_INVALID &&
           merged != ZR_EXEC_IR_VALUE_ID_INVALID,
       "phi dominance values");
    entry = ZrCore_ExecIr_FunctionAddBlock(function,
                                             ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    right = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    merge = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    ok(entry == 1u && left == 2u && right == 3u && merge == 4u,
       "phi dominance blocks");
    function->entryBlockId = entry;
    successors[0] = left;
    successors[1] = right;
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, successors, 2u,
               &function->blocks[entry - 1u].successorRange),
       "phi entry successors");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, &merge, 1u,
               &function->blocks[left - 1u].successorRange),
       "phi left successor");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, &merge, 1u,
               &function->blocks[right - 1u].successorRange),
       "phi right successor");
    predecessor = entry;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[left - 1u].predecessorRange),
       "phi left predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[right - 1u].predecessorRange),
       "phi right predecessor");
    mergePredecessors[0] = left;
    mergePredecessors[1] = right;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, mergePredecessors, 2u,
               &function->blocks[merge - 1u].predecessorRange),
       "phi merge predecessors");

    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &condition, 1u,
                                             &conditionOperands),
       "phi condition operands");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH;
    instruction.operands = conditionOperands;
    instruction.successorRange = function->blocks[entry - 1u].successorRange;
    instruction.sourceId = 921u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "phi entry branch");

    ok(ZrCore_ExecIr_FunctionAppendResults(function, &leftValue, 1u,
                                           &leftResult),
       "phi left result");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = leftResult;
    instruction.layoutId = 5u;
    instruction.sourceId = 922u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "phi left definition");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = function->blocks[left - 1u].successorRange;
    instruction.sourceId = 923u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "phi left branch");

    ok(ZrCore_ExecIr_FunctionAppendResults(function, &rightValue, 1u,
                                           &rightResult),
       "phi right result");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = rightResult;
    instruction.layoutId = 6u;
    instruction.sourceId = 924u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "phi right definition");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = function->blocks[right - 1u].successorRange;
    instruction.sourceId = 925u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "phi right branch");

    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &merged, 1u,
                                             &returnOperands),
       "phi return operands");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperands;
    instruction.sourceId = 926u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "phi merge return");

    function->blocks[entry - 1u].instructionRange.start = 0u;
    function->blocks[entry - 1u].instructionRange.count = 1u;
    function->blocks[entry - 1u].terminatorInstructionId = 1u;
    function->blocks[left - 1u].instructionRange.start = 1u;
    function->blocks[left - 1u].instructionRange.count = 2u;
    function->blocks[left - 1u].terminatorInstructionId = 3u;
    function->blocks[right - 1u].instructionRange.start = 3u;
    function->blocks[right - 1u].instructionRange.count = 2u;
    function->blocks[right - 1u].terminatorInstructionId = 5u;
    function->blocks[merge - 1u].instructionRange.start = 5u;
    function->blocks[merge - 1u].instructionRange.count = 1u;
    function->blocks[merge - 1u].terminatorInstructionId = 6u;

    incoming[0].predecessor = left;
    incoming[0].value = leftValue;
    incoming[1].predecessor = right;
    incoming[1].value = rightValue;
    ok(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, 2u,
                                                &incomingRange),
       "phi incoming range");
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    phi.incomings = incomingRange;
    ok(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u, &phiRange),
       "phi append");
    function->blocks[merge - 1u].phis = phiRange;

    ok(ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "valid phi edge definitions rejected");

    function->phiIncoming[incomingRange.start + 1u].value = leftValue;
    ok(!ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "phi accepted value unavailable on predecessor edge");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE &&
           diagnostic.blockId == merge && diagnostic.instructionId == 6u &&
           diagnostic.sourceId == 926u && diagnostic.expectedVersion == 2u &&
           diagnostic.actualVersion == leftValue,
       "phi edge dominance diagnostic lost identity");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_ssa_rejects_invoke_result_on_exception_edge(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrRange resultRange;
    SZrExecIrRange normalOperands;
    SZrExecIrRange exceptionOperands;
    SZrExecIrRange cleanupOperands;
    TZrExecIrValueId argument;
    TZrExecIrValueId result;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId normal;
    TZrExecIrBlockId exception;
    TZrExecIrBlockId cleanup;
    TZrExecIrBlockId successors[2];
    TZrExecIrBlockId predecessor;

    ZrCore_ExecIr_ModuleInit(&module);
    ok(ZrCore_ExecIr_ModuleAddFunction(&module, 930u, 1u, &id),
       "invoke exception function");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    argument = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    result = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    ok(argument != ZR_EXEC_IR_VALUE_ID_INVALID &&
           result != ZR_EXEC_IR_VALUE_ID_INVALID,
       "invoke exception values");
    entry = ZrCore_ExecIr_FunctionAddBlock(function,
                                             ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    normal = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    exception = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION);
    cleanup = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_CLEANUP);
    ok(entry == 1u && normal == 2u && exception == 3u && cleanup == 4u,
       "invoke exception blocks");
    function->entryBlockId = entry;
    successors[0] = normal;
    successors[1] = exception;
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, successors, 2u,
               &function->blocks[entry - 1u].successorRange),
       "invoke exception successors");
    predecessor = entry;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[normal - 1u].predecessorRange),
       "invoke normal predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[exception - 1u].predecessorRange),
       "invoke exception predecessor");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, &cleanup, 1u,
               &function->blocks[exception - 1u].successorRange),
       "invoke cleanup successor");
    predecessor = exception;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[cleanup - 1u].predecessorRange),
       "invoke cleanup predecessor");

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_INVOKE;
    instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    instruction.successorRange = function->blocks[entry - 1u].successorRange;
    instruction.sourceId = 931u;
    ok(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                           &resultRange),
       "invoke result range");
    instruction.results = resultRange;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke exception instruction");

    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &result, 1u,
                                             &normalOperands),
       "invoke normal use operands");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = normalOperands;
    instruction.sourceId = 932u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke normal return");

    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &argument, 1u,
                                             &exceptionOperands),
       "invoke exception operands");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = function->blocks[exception - 1u].successorRange;
    instruction.sourceId = 933u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke exception branch");

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_THROW;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.operands = exceptionOperands;
    instruction.sourceId = 934u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke cleanup throw");

    function->blocks[entry - 1u].instructionRange.start = 0u;
    function->blocks[entry - 1u].instructionRange.count = 1u;
    function->blocks[entry - 1u].terminatorInstructionId = 1u;
    function->blocks[normal - 1u].instructionRange.start = 1u;
    function->blocks[normal - 1u].instructionRange.count = 1u;
    function->blocks[normal - 1u].terminatorInstructionId = 2u;
    function->blocks[exception - 1u].instructionRange.start = 2u;
    function->blocks[exception - 1u].instructionRange.count = 1u;
    function->blocks[exception - 1u].terminatorInstructionId = 3u;
    function->blocks[cleanup - 1u].instructionRange.start = 3u;
    function->blocks[cleanup - 1u].instructionRange.count = 1u;
    function->blocks[cleanup - 1u].terminatorInstructionId = 4u;

    /* The normal edge is valid; the same result must not be usable on the
     * exceptional edge.  Put the use in the exception block after proving
     * the normal fixture itself is accepted. */
    ok(ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "valid invoke normal result rejected");
    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &result, 1u,
                                             &exceptionOperands),
       "invoke exceptional result operands");
    function->instructions[2].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    function->instructions[2].flags = 0u;
    function->instructions[2].operands = exceptionOperands;
    function->instructions[2].successorRange.start = 0u;
    function->instructions[2].successorRange.count = 0u;
    function->instructions[2].sourceId = 935u;
    function->blocks[exception - 1u].successorRange.count = 0u;
    function->blocks[cleanup - 1u].predecessorRange.count = 0u;
    ok(!ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "invoke result flowed through exceptional edge");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
           diagnostic.blockId == exception && diagnostic.instructionId == 3u &&
           diagnostic.sourceId == 935u && diagnostic.expectedVersion == 1u &&
           diagnostic.actualVersion == result,
       "invoke exceptional result diagnostic lost identity");

    function->blocks[exception - 1u].successorRange.count = 1u;
    function->blocks[cleanup - 1u].predecessorRange.count = 1u;
    function->instructions[2].opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    function->instructions[2].operands.start = 0u;
    function->instructions[2].operands.count = 0u;
    function->instructions[2].successorRange =
            function->blocks[exception - 1u].successorRange;
    function->instructions[2].sourceId = 933u;
    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &result, 1u,
                                             &cleanupOperands),
       "invoke cleanup result operands");
    function->instructions[3].opcode = ZR_EXEC_IR_OPCODE_RETURN;
    function->instructions[3].flags = 0u;
    function->instructions[3].operands = cleanupOperands;
    function->instructions[3].sourceId = 936u;
    ok(!ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "invoke result flowed through exceptional cleanup path");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
           diagnostic.blockId == cleanup && diagnostic.instructionId == 4u &&
           diagnostic.sourceId == 936u && diagnostic.expectedVersion == 1u &&
           diagnostic.actualVersion == result,
       "invoke cleanup result diagnostic lost identity");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_ssa_rejects_invoke_result_in_exception_phi(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[2];
    SZrExecIrRange resultRange;
    SZrExecIrRange returnOperands;
    SZrExecIrRange incomingRange;
    SZrExecIrRange phiRange;
    TZrExecIrValueId argument;
    TZrExecIrValueId result;
    TZrExecIrValueId merged;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId normal;
    TZrExecIrBlockId exception;
    TZrExecIrBlockId merge;
    TZrExecIrBlockId entrySuccessors[2];
    TZrExecIrBlockId predecessor;
    TZrExecIrBlockId mergePredecessors[2];

    ZrCore_ExecIr_ModuleInit(&module);
    ok(ZrCore_ExecIr_ModuleAddFunction(&module, 940u, 1u, &id),
       "invoke phi function");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    argument = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    result = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    merged = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    ok(argument != ZR_EXEC_IR_VALUE_ID_INVALID &&
           result != ZR_EXEC_IR_VALUE_ID_INVALID &&
           merged != ZR_EXEC_IR_VALUE_ID_INVALID,
       "invoke phi values");
    entry = ZrCore_ExecIr_FunctionAddBlock(function,
                                             ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    normal = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    exception = ZrCore_ExecIr_FunctionAddBlock(
            function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION);
    merge = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    ok(entry == 1u && normal == 2u && exception == 3u && merge == 4u,
       "invoke phi blocks");
    function->entryBlockId = entry;

    entrySuccessors[0] = normal;
    entrySuccessors[1] = exception;
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, entrySuccessors, 2u,
               &function->blocks[entry - 1u].successorRange),
       "invoke phi entry successors");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, &merge, 1u,
               &function->blocks[normal - 1u].successorRange),
       "invoke phi normal successor");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, &merge, 1u,
               &function->blocks[exception - 1u].successorRange),
       "invoke phi exception successor");

    predecessor = entry;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[normal - 1u].predecessorRange),
       "invoke phi normal predecessor");
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, &predecessor, 1u,
               &function->blocks[exception - 1u].predecessorRange),
       "invoke phi exception predecessor");
    mergePredecessors[0] = normal;
    mergePredecessors[1] = exception;
    ok(ZrCore_ExecIr_FunctionAppendPredecessors(
               function, mergePredecessors, 2u,
               &function->blocks[merge - 1u].predecessorRange),
       "invoke phi merge predecessors");

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_INVOKE;
    instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    instruction.successorRange = function->blocks[entry - 1u].successorRange;
    instruction.sourceId = 941u;
    ok(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                           &resultRange),
       "invoke phi result range");
    instruction.results = resultRange;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke phi instruction");

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange = function->blocks[normal - 1u].successorRange;
    instruction.sourceId = 942u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke phi normal branch");

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    instruction.successorRange =
            function->blocks[exception - 1u].successorRange;
    instruction.sourceId = 943u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke phi exception branch");

    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &merged, 1u,
                                             &returnOperands),
       "invoke phi return operands");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperands;
    instruction.sourceId = 944u;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke phi merge return");

    function->blocks[entry - 1u].instructionRange.start = 0u;
    function->blocks[entry - 1u].instructionRange.count = 1u;
    function->blocks[entry - 1u].terminatorInstructionId = 1u;
    function->blocks[normal - 1u].instructionRange.start = 1u;
    function->blocks[normal - 1u].instructionRange.count = 1u;
    function->blocks[normal - 1u].terminatorInstructionId = 2u;
    function->blocks[exception - 1u].instructionRange.start = 2u;
    function->blocks[exception - 1u].instructionRange.count = 1u;
    function->blocks[exception - 1u].terminatorInstructionId = 3u;
    function->blocks[merge - 1u].instructionRange.start = 3u;
    function->blocks[merge - 1u].instructionRange.count = 1u;
    function->blocks[merge - 1u].terminatorInstructionId = 4u;

    incoming[0].predecessor = normal;
    incoming[0].value = result;
    incoming[1].predecessor = exception;
    incoming[1].value = argument;
    ok(ZrCore_ExecIr_FunctionAppendPhiIncoming(function, incoming, 2u,
                                                &incomingRange),
       "invoke phi incoming range");
    memset(&phi, 0, sizeof(phi));
    phi.result = merged;
    phi.incomings = incomingRange;
    ok(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u, &phiRange),
       "invoke phi append");
    function->blocks[merge - 1u].phis = phiRange;

    ok(ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "valid invoke phi rejected");
    function->phiIncoming[incomingRange.start + 1u].value = result;
    ok(!ZrCore_ExecIr_VerifyFunction(
               function,
               (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                      ZR_EXEC_IR_VERIFY_SSA),
               &diagnostic),
       "invoke result flowed through exceptional phi edge");
    ok(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
           diagnostic.blockId == merge && diagnostic.instructionId == 4u &&
           diagnostic.sourceId == 944u && diagnostic.expectedVersion == 1u &&
           diagnostic.actualVersion == result,
       "invoke phi exceptional diagnostic lost identity");
    ZrCore_ExecIr_FreeModule(&module);
}

#include "test_ssa_effects_verifier_invoke.inc"

int main(void) {
    test_invoke_memory_versions_on_both_successors();
    test_effect_phi_joins_distinct_cfg_chains();
    test_effect_join_requires_phi_for_distinct_predecessors();
    test_memory_phi_joins_region_versions();
    test_memory_join_requires_region_phi();
    test_loop_carried_effect_and_memory_phis();
    test_throw_requires_flag();
    test_memory_tokens_must_be_monotonic();
    test_phi_predecessor_set();
    test_malformed_memory_range_is_rejected();
    test_memory_inputs_must_advance_across_instructions();
    test_tagged_memory_tokens_are_region_local();
    test_tagged_call_requires_all_declared_memory_regions();
    test_tagged_memory_token_region_must_match_schema();
    test_tagged_store_and_load_share_region_version();
    test_phi_incoming_order_matches_predecessors();
    test_phi_accepts_parallel_edge_occurrences();
    test_call_binding_row_zero_does_not_require_all_dynamic_flags();
    test_observable_effect_chain_rejects_skipped_version();
    test_malformed_block_instruction_range_is_rejected();
    test_empty_range_with_invalid_start_is_rejected();
    test_ssa_rejects_use_before_definition_in_linear_ir();
    test_ssa_rejects_cross_branch_use_not_dominated();
    test_ssa_accepts_phi_edge_definitions_and_rejects_wrong_edge();
    test_ssa_rejects_invoke_result_on_exception_edge();
    test_ssa_rejects_invoke_result_in_exception_phi();
    puts("ssa effects verifier PASS");
    return EXIT_SUCCESS;
}

/**
 * @fn static void ok(TZrBool value, const char *message)
 *
 * ### Summary
 * 为普通C测试提供不可恢复的布尔断言，失败时报告消息并结束整个进程。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in] value 已求值的测试条件；FALSE进入失败终止分支。
 * @param[in] message 失败分支读取的非空、以NUL结尾的消息借用；只在调用期间供fprintf使用。
 *
 * ### Returns
 * value非零时正常返回；FALSE时调用exit(EXIT_FAILURE)，不返回调用者。
 *
 * ### Constraints and lifecycle
 * 不保存message，不拥有测试module；失败不展开局部资源清理、无Unity或longjmp恢复，后续case不执行。fprintf/exit为当前C运行库操作。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:7（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:14（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:16（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:17（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:33（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:36（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:39（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:48（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:50（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:52（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:63（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:67（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:71（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:85（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:87（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:91（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:93（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:98（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:100（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:104（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:116（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:118（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:134（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:137（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:146（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:149（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:166（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:168（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:170（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:181（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:185（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:189（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:209（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:213（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:218（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:220（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:225（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:227（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:230（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:242（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:244（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:277（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:281（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:286（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:297（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:301（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:305（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:309（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:312（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:316（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:320（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:322（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:330（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:332（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:340（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:342（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:347（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:349（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:363（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:364（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:365（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:378（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:379（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:380（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:381（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:394（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:398（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:400（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:401（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:402（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:416（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:419（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:420（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:437（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:439（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:442（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:444（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:447（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:449（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:472（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:475（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:478（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:482（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:506（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:509（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:512（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:516（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:520（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:522（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:529（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:531（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:550（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:553（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:556（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:558（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:580（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:583（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:588（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:591（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:595（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:601（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:604（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:607（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:609（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:628（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:629（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:630（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:637（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:639（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:641（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:643（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:658（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:660（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:663（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:666（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:668（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:670（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:689（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:691（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:693（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:696（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:717（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:719（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:721（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:726（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:731（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:733（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:739（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:752（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:754（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:768（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:770（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:788（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:797（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:800（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:803（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:810（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:813（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:820（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:823（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:829（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:854（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:863（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:870（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:875（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:880（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:884（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:888（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:896（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:899（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:907（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:909（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:916（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:922（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:935（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:941（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:976（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:991（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1001（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1006（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1010（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1014（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1019（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1023（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1029（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1034（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1042（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1045（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1053（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1059（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1062（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1070（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1076（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1079（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1086（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1106（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1112（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1116（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1124（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1130（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1158（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1167（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1177（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1182（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1187（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1191（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1195（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1200（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1211（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1215（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1218（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1225（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1228（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1235（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1243（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1262（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1268（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1279（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1285（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1299（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1306（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1312（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1344（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1356（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1366（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1372（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1376（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1380（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1386（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1390（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1396（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1407（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1411（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1418（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1426（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1429（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1436（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1456（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1462（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1466（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1473（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1479（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:12（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:16（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:22（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:37（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:42（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:44（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:46（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:49（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:53（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:83（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:104（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:109（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:111（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:113（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:126（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:133（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:151（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:157（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:161（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:8（call or function-like macro）。
 */

/**
 * @fn static SZrExecIrFunction *new_function(SZrExecIrModule *module, TZrExecIrFunctionId *id)
 *
 * ### Summary
 * 初始化空测试module，创建固定实参的单函数及entry块，返回该module拥有的函数借用。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[out] module 非空独立可写module；必须未持有资源或已Free，ModuleInit直接重置字段。
 * @param[out] id 非空独立可写函数ID输出，正常返回后为新增函数ID。
 *
 * ### Returns
 * @return 正常返回非空函数指针；任何初始化/查询/entry创建断言失败直接退出进程。
 *
 * ### Constraints and lifecycle
 * ModuleInit不释放旧资源；函数归module而非caller单独释放。返回指针在module函数数组重分配或FreeModule时失效；正常使用结束由caller FreeModule。失败可能已创建部分module资源，不提供回滚或清理。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:11（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:80（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:112（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:198（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:238（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:253（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:358（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:372（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:388（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:409（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:428（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:457（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:490（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:542（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:566（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:622（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:651（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:678（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:704（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:747（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:763（direct_call）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:29（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:13（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:14（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:15（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:16（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:17（call or function-like macro）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:229（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:233（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:243（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:246（module allocation ownership and teardown）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:250（module allocation ownership and teardown）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void append_effect_call(SZrExecIrFunction *function, TZrExecIrMemoryTokenId memoryIn, TZrExecIrMemoryTokenId memoryOut, TZrExecIrEffectTokenId effectIn, TZrExecIrEffectTokenId effectOut)
 *
 * ### Summary
 * 追加用于effect链测试的CALL，同时复制一项memory输入/输出并写入指定effect版本。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in,out] function 非空当前测试函数，接收追加的memory池元素和指令。
 * @param[in] memoryIn 复制到CALL输入范围的token值。
 * @param[in] memoryOut 复制到CALL输出范围的token值。
 * @param[in] effectIn 写入CALL effectIn的token值。
 * @param[in] effectOut 写入CALL effectOut的token值。
 *
 * ### Returns
 * 正常返回表示三次追加均已通过ok；任一失败结束进程。
 *
 * ### Constraints and lifecycle
 * function为当前测试独占、非空可写函数，池保持有效且不并发修改。传入标量按值复制，临时instruction、token和ID数组通过Append接口复制进function池，不把其栈地址保存到最终函数。追加会重分配池，旧池元素指针/切片不得跨追加保留。任何ok失败直接exit(EXIT_FAILURE)，不回滚此前追加；正常返回后资源仍归函数所属module，最终由FreeModule释放。 CALL显式设MAY_ALLOCATE|MAY_THROW；helper不验证token合法性或自动构造block范围/phi。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:22（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:54（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:55（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:56（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:57（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:28（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:33（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:36（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:39（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void build_effect_diamond(SZrExecIrFunction *function)
 *
 * ### Summary
 * 在已有entry的空函数上构造四块各一CALL的effect菱形，让caller按测试需求补合流phi。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in,out] function 须由new_function形成，仅有id1 entry且没有原指令/前驱的独占可写函数。
 *
 * ### Returns
 * 正常返回后有id1..4四块、各一条CALL和1→2/3→4前驱关系；没有自动添加effect phi。
 *
 * ### Constraints and lifecycle
 * function为当前测试独占、非空可写函数，池保持有效且不并发修改。传入标量按值复制，临时instruction、token和ID数组通过Append接口复制进function池，不把其栈地址保存到最终函数。追加会重分配池，旧池元素指针/切片不得跨追加保留。任何ok失败直接exit(EXIT_FAILURE)，不回滚此前追加；正常返回后资源仍归函数所属module，最终由FreeModule释放。 新块硬断言为2/3/4，范围硬编码为0..3；不能在任意已有CFG上复用。effect分别1→2、2→3、2→4、5→6，合流phi由caller单独设置。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:43（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:84（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:115（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:48（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:50（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:52（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:54（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:55（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:56（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:57（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:63（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:67（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:71（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void test_effect_phi_joins_distinct_cfg_chains(void)
 *
 * ### Summary
 * 在菱形CFG显式effect phi后，断言分支效应链可合并，且过期incoming和首消费者绕过phi都被拒绝。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：先要求合法effect phi成功；将右incoming4改3要求EFFECT_TOKEN且expected4/actual3，再修复并把merge effectIn5改4要求expected5/actual4，最后修复要求成功。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:77（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1491（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:85（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:87（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:91（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:93（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:98（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:100（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:104（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:106（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_effect_join_requires_phi_for_distinct_predecessors(void)
 *
 * ### Summary
 * 检查不同前驱效应版本在没有effect phi时的拒绝及合流位置。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：构造effect diamond但不添加phi；要求VerifyEffects为FALSE、EFFECT_TOKEN、block4/instruction4，不检查其余诊断字段。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:109（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1492（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:116（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:118（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:121（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void append_tagged_store(SZrExecIrFunction *function, TZrExecIrMemoryTokenId output, TZrExecIrEffectTokenId effectIn, TZrExecIrEffectTokenId effectOut)
 *
 * ### Summary
 * 追加STORE并复制一项memory输出，用于构造指定region版本及effect链。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in,out] function 非空接收追加的测试函数。
 * @param[in] output 复制到memoryOut的一项token；helper不校验tag/region。
 * @param[in] effectIn STORE effect输入版本。
 * @param[in] effectOut STORE effect输出版本。
 *
 * ### Returns
 * 正常返回表示token和指令追加断言均通过；失败结束进程。
 *
 * ### Constraints and lifecycle
 * function为当前测试独占、非空可写函数，池保持有效且不并发修改。传入标量按值复制，临时instruction、token和ID数组通过Append接口复制进function池，不把其栈地址保存到最终函数。追加会重分配池，旧池元素指针/切片不得跨追加保留。任何ok失败直接exit(EXIT_FAILURE)，不回滚此前追加；正常返回后资源仍归函数所属module，最终由FreeModule释放。 STORE只显式设置MAY_THROW和memoryOut，不合成memoryIn或block范围。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:124（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:172（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:173（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:174（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:278（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:283（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:467（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:129（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:134（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:137（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void append_tagged_load(SZrExecIrFunction *function, TZrExecIrMemoryTokenId input)
 *
 * ### Summary
 * 追加只带一项指定memory输入的LOAD。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in,out] function 非空接收追加的测试函数。
 * @param[in] input 复制到LOAD memoryIn的token值。
 *
 * ### Returns
 * 正常返回表示token及指令均追加；失败结束进程。
 *
 * ### Constraints and lifecycle
 * function为当前测试独占、非空可写函数，池保持有效且不并发修改。传入标量按值复制，临时instruction、token和ID数组通过Append接口复制进function池，不把其栈地址保存到最终函数。追加会重分配池，旧池元素指针/切片不得跨追加保留。任何ok失败直接exit(EXIT_FAILURE)，不回滚此前追加；正常返回后资源仍归函数所属module，最终由FreeModule释放。 不自动生成memory输出、effect链、block范围或验证token。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:141（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:175（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:144（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:146（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:149（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void build_memory_diamond(SZrExecIrFunction *function)
 *
 * ### Summary
 * 构造四块managed-heap版本1/2/3及合流读取4的菱形，为caller添加region phi留下缺口。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in,out] function 须是new_function得到的仅entry、无既有指令/前驱的函数。
 *
 * ### Returns
 * 正常返回后四块分别有STORE/STORE/STORE/LOAD及菱形前驱，没有自动添加memory phi。
 *
 * ### Constraints and lifecycle
 * function为当前测试独占、非空可写函数，池保持有效且不并发修改。传入标量按值复制，临时instruction、token和ID数组通过Append接口复制进function池，不把其栈地址保存到最终函数。追加会重分配池，旧池元素指针/切片不得跨追加保留。任何ok失败直接exit(EXIT_FAILURE)，不回滚此前追加；正常返回后资源仍归函数所属module，最终由FreeModule释放。 block id硬断言2/3/4，instruction范围硬编码为0..3；三次STORE输出heap版本1/2/3，merge LOAD输入heap4，caller负责按意图设置phi。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:153（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:208（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:241（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:157（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:159（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:161（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:163（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:166（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:168（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:170（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:172（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:173（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:174（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:175（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:181（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:185（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:189（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:8（assertion failure exits process）。
 */

/**
 * @fn static void test_memory_phi_joins_region_versions(void)
 *
 * ### Summary
 * 检查managed-heap phi合并和消费同一版本，拒绝过期边输入与绕过phi的读取。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：先要求region phi成功；右incoming由heap3改heap2、merge load由heap4改heap2分别要求MEMORY_TOKEN；各自修复后要求成功，未对这两个失败断言全部位置/版本字段。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:195（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1493（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:209（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:213（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:218（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:220（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:225（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:227（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:230（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:232（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_memory_join_requires_region_phi(void)
 *
 * ### Summary
 * 检查不同前驱managed-heap版本缺少memory phi时的拒绝及合流位置。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：构造memory diamond但不添加memory phi；要求FALSE、MEMORY_TOKEN、block4/instruction4。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:235（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1494（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:242（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:244（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:247（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_loop_carried_effect_and_memory_phis(void)
 *
 * ### Summary
 * 检查循环携带的effect/memory phi，并用前驱改写区分真实回边和按声明顺序反向的无支配边。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：先要求合法循环成功；分别改回边memory/effect incoming要求对应code、header及expected/actual；修复后把body前驱改entry，先要求MEMORY_TOKEN，再改变header memory phi结果要求EFFECT_TOKEN。后两次仅检查code/header，不断言完整回边数组。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:250（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1495（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:277（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:281（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:286（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:297（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:301（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:305（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:309（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:312（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:316（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:320（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:322（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:330（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:332（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:340（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:342（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:347（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:349（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:352（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_throw_requires_flag(void)
 *
 * ### Summary
 * 检查DIV缺少MAY_THROW标记的拒绝分类。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：添加零flags的DIV；要求VerifyEffects为FALSE且EXCEPTION_EDGE，仅检查错误码。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:355（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1496（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:363（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:364（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:365（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:366（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_memory_tokens_must_be_monotonic(void)
 *
 * ### Summary
 * 检查同一LOAD输入范围内下降的未标记memory token顺序。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：输入token为2、1；要求FALSE和MEMORY_TOKEN，不断言诊断位置或版本。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:369（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1497（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:378（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:379（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:380（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:381（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:382（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_phi_predecessor_set(void)
 *
 * ### Summary
 * 检查phi incoming引用不在当前块前驱集合中的块。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：块前驱为99，incoming.predecessor为42；要求PHI_PREDECESSOR_MISMATCH。该phi前置拒绝不证明已进入回边分类器的INVALID_BLOCK分支。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:385（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1498（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:394（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:398（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:400（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:401（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:402（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:403（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_malformed_memory_range_is_rejected(void)
 *
 * ### Summary
 * 检查LOAD使用超出逻辑池的memoryIn范围时的拒绝和块位置。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：memoryIn为start7/count1而池为空；要求FALSE、INVALID_RANGE及entry block，不检查完整诊断。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:406（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1499（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:416（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:419（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:420（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:422（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_memory_inputs_must_advance_across_instructions(void)
 *
 * ### Summary
 * 检查连续LOAD之间的未标记memory输入不能由2倒退至1。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：追加两个LOAD并覆盖当前块instruction范围；要求FALSE和MEMORY_TOKEN，不检查完整位置或版本。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:425（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1500（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:437（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:439（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:442（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:444（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:447（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:449（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:451（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_tagged_memory_tokens_are_region_local(void)
 *
 * ### Summary
 * 检查managed-heap与ownership区域的版本比较相互独立。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：先STORE写heap1，再DROP读ownership1写ownership2并形成effect2至3；要求VerifyEffects成功，不穷举其它memory class。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:454（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1501（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:472（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:475（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:478（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:482（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:484（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_tagged_call_requires_all_declared_memory_regions(void)
 *
 * ### Summary
 * 检查tagged CALL所声明的heap/native输入输出范围均存在。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：完整两区域CALL先要求成功；分别将memoryIn.count和memoryOut.count减为1要求FALSE、MEMORY_TOKEN及functionToken1/block1/instruction1/source481；没有断言expected/actual字段，也没有最终修复成功断言。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:487（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1502（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:506（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:509（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:512（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:516（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:520（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:522（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:529（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:531（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:536（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_tagged_memory_token_region_must_match_schema(void)
 *
 * ### Summary
 * 检查LOAD不能使用native-FFI区域token冒充schema所需区域。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：向LOAD memoryIn写native区域版本1，要求FALSE与MEMORY_TOKEN；仅检查错误码。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:539（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1503（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:550（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:553（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:556（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:558（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:560（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_tagged_store_and_load_share_region_version(void)
 *
 * ### Summary
 * 检查LOAD消费同区域STORE产生的版本，拒绝没有写入却跳到未来版本。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：STORE写heap1后LOAD读heap1先要求成功；再加source903的LOAD读heap2要求MEMORY_TOKEN、entry、instruction3、source903、expected1/actual2。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:563（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1504（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:580（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:583（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:588（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:591（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:595（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:601（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:604（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:607（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:609（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:616（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_phi_incoming_order_matches_predecessors(void)
 *
 * ### Summary
 * 检查phi incoming的有序位置与前驱边槽一致。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：前驱按2、3，incoming按3、2排列且value为INVALID；要求FALSE与PHI_PREDECESSOR_MISMATCH；不将拒绝误归SSA支配或重复源检查。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:619（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1505（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:628（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:629（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:630（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:637（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:639（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:641（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:643（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:645（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_phi_accepts_parallel_edge_occurrences(void)
 *
 * ### Summary
 * 检查来自同一源块的两条平行CFG边有两个有序phi incoming槽。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：两个successor均指block2，block2两个predecessor均为block1，两个phi incoming均为1/0；要求VerifyEffects成功，不证明任意重复池布局均合法。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:648（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1506（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:658（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:660（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:663（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:666（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:668（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:670（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:672（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_call_binding_row_zero_does_not_require_all_dynamic_flags(void)
 *
 * ### Summary
 * 检查bindingRow为0的普通CALL不被迫携带全部可选动态flag。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：仅设置MAY_ALLOCATE|MAY_THROW、effect1至2和未标记memory1至2；要求VerifyEffects成功，不声称忽略schema必须标记。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:675（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1507（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:689（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:691（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:693（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:696（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:698（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_observable_effect_chain_rejects_skipped_version(void)
 *
 * ### Summary
 * 检查两次可观察CALL之间即使隔有NOP也不能跳过effect版本。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：第二CALL初始effectIn3而前一输出2，要求EFFECT_TOKEN、entry/instruction3/source552及expected2/actual3；改为2后要求成功。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:701（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1508（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:717（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:719（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:721（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:726（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:731（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:733（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:739（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:741（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_malformed_block_instruction_range_is_rejected(void)
 *
 * ### Summary
 * 检查空函数中block instruction范围start1/count1越界的前置拒绝。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：要求FALSE、INVALID_RANGE及entry block；该storage前置失败未进入回边分类器。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:744（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1509（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:752（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:754（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:757（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_empty_range_with_invalid_start_is_rejected(void)
 *
 * ### Summary
 * 检查零长度instruction range也不能使用UINT32_MAX起点越过空池。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：block range为UINT32_MAX/0，要求FALSE、INVALID_RANGE及entry；零长度不免除起点检查，该前置失败未进入分类器。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:760（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1510（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:768（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:770（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:773（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 */

/**
 * @fn static void test_ssa_rejects_use_before_definition_in_linear_ir(void)
 *
 * ### Summary
 * 检查线性IR在CONSTANT定义前COPY使用该值的SSA拒绝。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：仅请求STRUCTURE|SSA；要求FALSE、DOMINANCE、instruction1/source901、expected定义位置2及actual source值ID；不请求EFFECT验证或授予回边分支覆盖。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:776（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1511（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:788（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:797（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:800（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:803（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:810（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:813（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:820（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:823（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:829（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:833（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:899（SSA validation route）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:905（EFFECT route conditional, not selected by STRUCTURE|SSA-only cases）。
 */

/**
 * @fn static void test_ssa_rejects_cross_branch_use_not_dominated(void)
 *
 * ### Summary
 * 检查左分支定义的值不能供不被其支配的右分支RETURN使用。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：仅请求STRUCTURE|SSA；要求DOMINANCE、right block/instruction4/source914、expected2及actual source值ID；未请求EFFECT验证。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:836（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1512（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:854（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:863（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:870（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:875（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:880（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:884（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:888（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:896（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:899（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:907（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:909（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:916（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:922（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:935（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:941（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:946（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:899（SSA validation route）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:905（EFFECT route conditional, not selected by STRUCTURE|SSA-only cases）。
 */

/**
 * @fn static void test_ssa_accepts_phi_edge_definitions_and_rejects_wrong_edge(void)
 *
 * ### Summary
 * 检查SSA phi在各自前驱边选择可用定义，并拒绝在右边选择左边值。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：正确left/right incoming先要求STRUCTURE|SSA成功；将右incoming改leftValue后要求DOMINANCE、merge/instruction6/source926、expected2及actual leftValue；未请求EFFECT验证。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:949（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1513（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:976（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:991（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1001（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1006（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1010（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1014（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1019（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1023（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1029（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1034（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1042（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1045（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1053（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1059（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1062（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1070（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1076（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1079（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1086（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1106（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1112（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1116（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1124（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1130（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1135（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:899（SSA validation route）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:905（EFFECT route conditional, not selected by STRUCTURE|SSA-only cases）。
 */

/**
 * @fn static void test_ssa_rejects_invoke_result_on_exception_edge(void)
 *
 * ### Summary
 * 检查INVOKE结果仅能在正常边使用，异常边及其cleanup后继的使用被拒绝。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：先要求正常result RETURN的STRUCTURE|SSA成功；随后分别让exception和cleanup RETURN使用result，要求EXCEPTION_EDGE及对应block/instruction3或4/source935或936、expected1/actual result。每次诊断&&只覆盖列出的字段，未请求EFFECT验证。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:1138（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1514（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1158（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1167（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1177（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1182（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1187（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1191（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1195（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1200（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1211（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1215（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1218（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1225（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1228（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1235（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1243（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1262（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1268（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1279（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1285（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1299（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1306（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1312（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1317（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:899（SSA validation route）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:905（EFFECT route conditional, not selected by STRUCTURE|SSA-only cases）。
 */

/**
 * @fn static void test_ssa_rejects_invoke_result_in_exception_phi(void)
 *
 * ### Summary
 * 检查INVOKE正常边结果可作正常phi incoming，却不能在异常前驱边复用。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * 无返回值；所有构造和验证断言正常满足后释放本例module并返回main，失败由ok结束进程。
 *
 * ### Constraints and lifecycle
 * 使用本例的局部module独占所分配函数及池；function/数组指针是其借用，不转移所有权。按源码顺序构造、变异和调用验证器，诊断仅在其对应拒绝断言后读取。正常完成到达本例末尾FreeModule；任一ok条件为FALSE会fprintf后exit(EXIT_FAILURE)，后续恢复、FreeModule及后续case不执行，没有Unity保护、longjmp恢复或tearDown。断言中的&&按C规则短路，未被求值的右侧不能另授覆盖。仅检查本例列出的结果/字段；未遍历所有诊断字段，也不注入分配失败。 实际断言范围：正常incoming为result、异常incoming为external argument先要求STRUCTURE|SSA成功；将异常incoming改result后要求EXCEPTION_EDGE、merge/instruction4/source944、expected1/actual result；未请求EFFECT验证。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:1320（definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1515（direct_call）。
 * - tests/parser/test_ssa_effects_verifier.c:1344（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1356（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1366（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1372（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1376（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1380（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1386（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1390（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1396（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1407（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1411（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1418（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1426（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1429（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1436（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1456（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1462（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1466（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1473（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1479（actual ok assertion）。
 * - tests/parser/test_ssa_effects_verifier.c:1484（validation or normal release）。
 * - tests/parser/test_ssa_effects_verifier.c:8（ordinary C assertion exits process）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:237（module teardown）。
 * - tests/cmake/ssa-tests.cmake:149（CTest process entry）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:899（SSA validation route）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:905（EFFECT route conditional, not selected by STRUCTURE|SSA-only cases）。
 */

/**
 * @file tests/parser/test_ssa_effects_verifier.c
 * @brief 文本包含组合块：test_ssa_effects_verifier_invoke.inc。
 *
 * ### Summary
 * 把INVOKE memory扩展的三个静态helper和一个测试定义组合进普通effects验证器测试翻译单元。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Constraints and lifecycle
 * 该#include是文本组合，.inc不是独立编译单元，也不是四份额外编译实例。必须在本文件ok/new_function已定义后、main之前恰好包含一次；.inc在相同翻译单元使用这些static helper，并自行引入Parser builder声明。其三个helper与一个case各在.inc物理路径单独登记，组合块不重复取得这些定义或调用的覆盖。main先调用该case，再调用主文件25个case；任一ok失败直接exit，后续case不执行。包含顺序、static可见性和目标source列表共同确定入口，不将无其它跨TU引用解释为死代码。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:1487（definition）。
 * - tests/cmake/ssa-tests.cmake:125（source_list_compilation_entry）。
 * - tests/parser/test_ssa_effects_verifier.c:1490（main_invokes_the_included_test）。
 * - tests/parser/test_ssa_effects_verifier.c:7（ok must precede textual include）。
 * - tests/parser/test_ssa_effects_verifier.c:11（new_function must precede textual include）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:1（declaration include）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:3（included helper definition）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:26（included helper definition）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:79（included assertion helper）。
 * - tests/parser/test_ssa_effects_verifier_invoke.inc:90（included case definition）。
 * - tests/parser/test_ssa_effects_verifier.c:1489（main follows include）。
 * - tests/parser/test_ssa_effects_verifier.c:1491（remaining main-file cases begin）。
 * - tests/parser/test_ssa_effects_verifier.c:1515（remaining main-file cases end）。
 */

/**
 * @fn int main(void)
 *
 * ### Summary
 * 按固定顺序运行26个普通C验证场景，全部正常返回后输出总PASS并返回进程成功码。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Returns
 * @return 全部26个case返回后返回EXIT_SUCCESS；任一ok失败会exit(EXIT_FAILURE)，不会到达总PASS。
 *
 * ### Constraints and lifecycle
 * 不使用Unity、RUN_TEST、setUp/tearDown或逐case恢复；先运行文本包含的INVOKE memory case，再运行25个主文件case。各case管理自己的module；本函数不补偿失败case跳过的Free。总PASS是整程序到达末尾的标记，不输出独立逐case PASS记录。
 *
 * ### References
 * - tests/parser/test_ssa_effects_verifier.c:1489（definition）。
 * - tests/cmake/ssa-tests.cmake:149（CTest_process_entry）。
 * - tests/parser/test_ssa_effects_verifier.c:1490（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1491（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1492（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1493（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1494（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1495（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1496（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1497（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1498（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1499（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1500（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1501（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1502（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1503（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1504（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1505（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1506（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1507（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1508（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1509（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1510（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1511（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1512（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1513（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1514（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1515（call or function-like macro）。
 * - tests/parser/test_ssa_effects_verifier.c:1516（call or function-like macro）。
 * - tests/cmake/ssa-tests.cmake:125（source compilation entry）。
 * - tests/cmake/ssa-tests.cmake:136（production source compilation entry）。
 */
