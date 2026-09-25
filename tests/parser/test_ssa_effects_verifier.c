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
    TZrExecIrMemoryTokenId heapIn = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    TZrExecIrMemoryTokenId heapOut = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u);
    TZrExecIrMemoryTokenId nativeIn = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u);
    TZrExecIrMemoryTokenId nativeOut = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u);

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &heapIn, 1u,
                                                 &instruction.memoryIn),
       "append tagged heap input");
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &heapOut, 1u,
                                                 &instruction.memoryOut),
       "append tagged heap output");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append tagged heap call");

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW;
    instruction.effectIn = 2u;
    instruction.effectOut = 3u;
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &nativeIn, 1u,
                                                 &instruction.memoryIn),
       "append tagged native input");
    ok(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, &nativeOut, 1u,
                                                 &instruction.memoryOut),
       "append tagged native output");
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "append tagged native call");
    function->blocks[0].instructions.count = function->instructionCount;

    ok(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
       "independent tagged memory regions imposed a false order");
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

int main(void) {
    test_effect_phi_joins_distinct_cfg_chains();
    test_effect_join_requires_phi_for_distinct_predecessors();
    test_throw_requires_flag();
    test_memory_tokens_must_be_monotonic();
    test_phi_predecessor_set();
    test_malformed_memory_range_is_rejected();
    test_memory_inputs_must_advance_across_instructions();
    test_tagged_memory_tokens_are_region_local();
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
