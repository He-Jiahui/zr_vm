#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static SZrExecIrRange range(TZrUInt32 start, TZrUInt32 count) {
    SZrExecIrRange result;
    result.start = start;
    result.count = count;
    return result;
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
                               TZrExecIrEffectTokenId effectIn,
                               TZrExecIrEffectTokenId effectOut,
                               TZrExecIrSourceId sourceId) {
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId id = 0u;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.operands = operands;
    instruction.results = results;
    instruction.layoutId = layoutId;
    instruction.flags = flags;
    instruction.effectIn = effectIn;
    instruction.effectOut = effectOut;
    instruction.sourceId = sourceId;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
    assert(id == function->instructionCount);
}

static void init_function(SZrExecIrFunction *function) {
    ZrCore_ExecIr_FunctionInit(function);
    function->id = 1u;
    function->functionToken = 7u;
    function->signatureHash = 99u;
}

static void build_constant_copy_function(SZrExecIrFunction *function) {
    TZrExecIrValueId first, second, sum, copy;
    SZrExecIrRange firstResult, secondResult, sumResult, copyResult;
    SZrExecIrRange addOperands, copyOperands, returnOperands;
    init_function(function);
    first = add_value(function);
    second = add_value(function);
    sum = add_value(function);
    copy = add_value(function);
    assert(first != 0u && second != 0u && sum != 0u && copy != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &first, 1u, &firstResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &second, 1u, &secondResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &sum, 1u, &sumResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &copy, 1u, &copyResult));
    {
        TZrExecIrValueId operands[2] = {first, second};
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, operands, 2u,
                                                     &addOperands));
    }
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &sum, 1u, &copyOperands));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &copy, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       firstResult, 2u, 0u, 0u, 0u, 101u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       secondResult, 3u, 0u, 0u, 0u, 102u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_ADD, addOperands, sumResult,
                       0u, 0u, 0u, 0u, 103u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_COPY, copyOperands, copyResult,
                       0u, 0u, 0u, 0u, 104u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands,
                       range(0u, 0u), 0u, 0u, 0u, 0u, 105u);
}

static void build_throwing_division_function(SZrExecIrFunction *function) {
    TZrExecIrValueId left, right, result;
    SZrExecIrRange leftResult, rightResult, resultRange, operands;
    init_function(function);
    left = add_value(function);
    right = add_value(function);
    result = add_value(function);
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &left, 1u, &leftResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &right, 1u, &rightResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &resultRange));
    {
        TZrExecIrValueId values[2] = {left, right};
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, values, 2u, &operands));
    }
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       leftResult, 10u, 0u, 0u, 0u, 201u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       rightResult, 0u, 0u, 0u, 0u, 202u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_DIV, operands, resultRange,
                       0u, ZR_EXEC_IR_FLAG_MAY_THROW, 1u, 2u, 203u);
}

static void build_overflow_function(SZrExecIrFunction *function) {
    TZrExecIrValueId left, right, result;
    SZrExecIrRange leftResult, rightResult, resultRange, operands, returnOperands;
    init_function(function);
    left = add_value(function);
    right = add_value(function);
    result = add_value(function);
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &left, 1u, &leftResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &right, 1u, &rightResult));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &resultRange));
    {
        TZrExecIrValueId values[2] = {left, right};
        assert(ZrCore_ExecIr_FunctionAppendOperands(function, values, 2u, &operands));
    }
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &result, 1u, &returnOperands));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       leftResult, UINT32_MAX, 0u, 0u, 0u, 301u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT, range(0u, 0u),
                       rightResult, 1u, 0u, 0u, 0u, 302u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_ADD, operands, resultRange,
                       0u, 0u, 0u, 0u, 303u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands,
                       range(0u, 0u), 0u, 0u, 0u, 0u, 304u);
}

static void build_unused_call_function(SZrExecIrFunction *function) {
    TZrExecIrValueId result;
    SZrExecIrRange resultRange, memoryIn, memoryOut;
    TZrExecIrMemoryTokenId memoryTokens[2] = {1u, 2u};
    init_function(function);
    result = add_value(function);
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u, &resultRange));
    assert(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryTokens, 2u, &memoryIn));
    memoryIn = range(0u, 1u);
    memoryOut = range(1u, 1u);
    {
        SZrExecIrInstruction instruction;
        TZrExecIrInstructionId id = 0u;
        memset(&instruction, 0, sizeof(instruction));
        instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CALL;
        instruction.results = resultRange;
        instruction.memoryIn = memoryIn;
        instruction.memoryOut = memoryOut;
        instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                        ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
        instruction.effectIn = 1u;
        instruction.effectOut = 2u;
        instruction.sourceId = 401u;
        assert(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, &id));
        assert(id == 1u);
    }
}

static void attach_source_map(SZrExecIrFunction *function, TZrExecIrInstructionId id) {
    function->sourceMaps = (SZrExecIrSourceMap *)calloc(1u, sizeof(*function->sourceMaps));
    assert(function->sourceMaps != ZR_NULL);
    function->sourceMapCapacity = 1u;
    function->sourceMapCount = 1u;
    function->sourceMaps[0].sourceId = 402u;
    function->sourceMaps[0].instructionId = id;
}

static void test_scalar_pipeline_and_fixed_point(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 firstHash, secondHash;
    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    build_constant_copy_function(&function);
    assert(ZrParser_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                          &diagnostic));
    assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks, &diagnostic));
    firstHash = ZrParser_ExecIr_FunctionHash(&function);
    assert(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_CONSTANT);
    assert(function.instructions[3].opcode == ZR_EXEC_IR_OPCODE_NOP);
    assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks, &diagnostic));
    secondHash = ZrParser_ExecIr_FunctionHash(&function);
    assert(firstHash == secondHash);
    assert(remarks.count >= 4u);
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_throwing_instruction_is_observable(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    build_throwing_division_function(&function);
    assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks, &diagnostic));
    assert(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_DIV);
    assert((function.instructions[2].flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u);
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_checked_overflow_is_not_folded(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    build_overflow_function(&function);
    assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks, &diagnostic));
    assert(function.instructions[2].opcode == ZR_EXEC_IR_OPCODE_ADD);
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_unused_call_is_preserved(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    build_unused_call_function(&function);
    assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks, &diagnostic));
    assert(function.instructions[0].opcode == ZR_EXEC_IR_OPCODE_CALL);
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_dead_source_mapping_is_removed(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    init_function(&function);
    {
        TZrExecIrValueId value = add_value(&function);
        SZrExecIrRange result;
        SZrExecIrInstruction instruction;
        TZrExecIrInstructionId id = 0u;
        assert(ZrCore_ExecIr_FunctionAppendResults(&function, &value, 1u, &result));
        memset(&instruction, 0, sizeof(instruction));
        instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CONSTANT;
        instruction.results = result;
        instruction.layoutId = 9u;
        instruction.sourceId = 403u;
        assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));
        assert(id == 1u);
    }
    attach_source_map(&function, 1u);
    assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks, &diagnostic));
    assert(function.instructions[0].opcode == ZR_EXEC_IR_OPCODE_NOP);
    assert(function.sourceMapCount == 0u);
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_direct_pass_rejects_malformed_storage(void) {
    SZrExecIrFunction function;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    const SZrExecIrPassInfo *passes;
    SZrExecIrDiagnostic diagnostic;
    ZrCore_ExecIr_FunctionInit(&function);
    function.instructionCount = 1u;
    function.instructionCapacity = 1u;
    memset(&context, 0, sizeof(context));
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    context.cache = &cache;
    passes = ZrParser_ExecIr_GetScalarPasses(NULL);
    assert(!ZrParser_ExecIr_RunPassPipeline(&function, &passes[1], 1u,
                                            &context, &diagnostic));
    assert(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);

    ZrCore_ExecIr_FunctionInit(&function);
    function.blockCount = 1u;
    function.blockCapacity = 1u;
    function.id = 1u;
    function.functionToken = 7u;
    function.blocks = (SZrExecIrBlock *)calloc(1u, sizeof(*function.blocks));
    assert(function.blocks != ZR_NULL);
    function.blocks[0].id = 9u;
    function.entryBlockId = 1u;
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    memset(&context, 0, sizeof(context));
    context.cache = &cache;
    assert(!ZrParser_ExecIr_RunPassPipeline(&function, &passes[0], 1u,
                                            &context, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK);
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&function);
}

static TZrBool broken_pass(SZrExecIrFunction *function,
                           SZrExecIrPassContext *context,
                           TZrBool *changed,
                           SZrExecIrDiagnostic *diagnostic) {
    (void)context;
    (void)diagnostic;
    function->instructions[0].opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_COUNT;
    *changed = ZR_TRUE;
    return ZR_TRUE;
}

static void test_failed_pass_rolls_back(void) {
    SZrExecIrFunction function;
    SZrExecIrFunction before;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    SZrExecIrPassInfo pass;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 hash;
    build_constant_copy_function(&function);
    ZrCore_ExecIr_FunctionInit(&before);
    assert(ZrCore_ExecIr_CloneFunction(&function, &before, &diagnostic));
    hash = ZrParser_ExecIr_FunctionHash(&function);
    memset(&pass, 0, sizeof(pass));
    pass.name = "broken";
    pass.run = broken_pass;
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    memset(&context, 0, sizeof(context));
    context.cache = &cache;
    assert(!ZrParser_ExecIr_RunPassPipeline(&function, &pass, 1u,
                                            &context, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE);
    assert(hash == ZrParser_ExecIr_FunctionHash(&function));
    ZrParser_ExecIr_AnalysisCacheFree(&cache);
    ZrCore_ExecIr_FreeFunction(&before);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_budget_is_bounded(void) {
    SZrExecIrFunction function;
    SZrExecIrPassBudget budget;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    build_constant_copy_function(&function);
    memset(&budget, 0, sizeof(budget));
    budget.maxWork = 1u;
    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    assert(ZrParser_ExecIr_OptimizeScalar(&function, &budget, &remarks, &diagnostic));
    assert(remarks.count != 0u);
    assert(remarks.items[0].outcome == ZR_EXEC_IR_REMARK_BLOCKED);
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_scalar_pipeline_and_fixed_point();
    test_throwing_instruction_is_observable();
    test_checked_overflow_is_not_folded();
    test_unused_call_is_preserved();
    test_dead_source_mapping_is_removed();
    test_direct_pass_rejects_malformed_storage();
    test_failed_pass_rolls_back();
    test_budget_is_bounded();
    return 0;
}
