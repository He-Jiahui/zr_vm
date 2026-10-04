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

static void build_unused_move_function(SZrExecIrFunction *function) {
    TZrExecIrValueId source, destination;
    SZrExecIrRange sourceResult, moveOperands, moveResult;

    init_function(function);
    source = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNIQUE,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    destination = ZrCore_ExecIr_FunctionAddValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNIQUE,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    assert(source != ZR_EXEC_IR_VALUE_ID_INVALID &&
           destination != ZR_EXEC_IR_VALUE_ID_INVALID);
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &source, 1u,
                                                &sourceResult));
    assert(ZrCore_ExecIr_FunctionAppendOperands(function, &source, 1u,
                                                 &moveOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(function, &destination, 1u,
                                                &moveResult));
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), sourceResult, 1u, 0u, 0u, 0u, 407u);
    append_instruction(function, ZR_EXEC_IR_OPCODE_MOVE, moveOperands,
                       moveResult, 0u, 0u, 0u, 0u, 408u);
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
    TZrUInt32 index;
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
    for (index = 0u; index < remarks.count; ++index)
        assert(remarks.items[index].verifierChecks == 2u);
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

static void test_unused_move_is_preserved(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;

    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    build_unused_move_function(&function);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));
    assert(ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks,
                                          &diagnostic));
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_MOVE);
    assert(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                        &diagnostic));
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_typed_call_row_survives_scalar_optimization(void) {
    const TZrUInt64 moduleHash = UINT64_C(0x12344321);
    const TZrExecIrMemoryTokenId memoryReadTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)
    };
    const TZrExecIrMemoryTokenId memoryWriteTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)
    };
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrBindingRow row;
    TZrUInt64 originalHash;
    TZrUInt64 changedHash;
    TZrExecIrValueId result;
    SZrExecIrRange resultRange, returnOperands, memoryIn, memoryOut;

    init_function(&function);
    function.contract.moduleHash = moduleHash;
    assert(ZrCore_ExecIr_FunctionAddBlock(
                   &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
           ZR_EXEC_IR_BLOCK_ID_ENTRY);
    function.entryBlockId = ZR_EXEC_IR_BLOCK_ID_ENTRY;
    result = add_value(&function);
    assert(result != ZR_EXEC_IR_VALUE_ID_INVALID);
    assert(ZrCore_ExecIr_FunctionAppendResults(&function, &result, 1u,
                                                &resultRange));
    assert(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                   &function, memoryReadTokens, 2u, &memoryIn));
    assert(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                   &function, memoryWriteTokens, 2u, &memoryOut));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_CALL;
    instruction.results = resultRange;
    instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    instruction.memoryIn = memoryIn;
    instruction.memoryOut = memoryOut;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 405u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction,
                                                    ZR_NULL));
    assert(ZrCore_ExecIr_FunctionAppendOperands(&function, &result, 1u,
                                                &returnOperands));
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN, returnOperands,
                       range(0u, 0u), 0u, 0u, 0u, 0u, 406u);
    function.blocks[0].instructionRange = range(0u, 2u);
    function.blocks[0].terminatorInstructionId = 2u;

    memset(&row, 0, sizeof(row));
    row.rowIndex = 0u;
    row.instructionId = 1u;
    row.segmentIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
    row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    row.contract.targetMetadataToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 405u);
    row.contract.signatureToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 405u);
    row.contract.signatureHash = UINT64_C(0x405);
    row.contract.moduleSignatureHash = moduleHash;
    row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    row.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE;
    row.location.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
    row.sourceId = 405u;
    assert(ZrCore_ExecIr_FunctionSetBindingRows(&function, &row, 1u,
                                                 &diagnostic));
    assert(ZrCore_ExecIr_FunctionValidateBindingRows(&function, &diagnostic));
    assert(function.bindingRows != ZR_NULL &&
           function.bindingRowCount == 1u);
    originalHash = ZrParser_ExecIr_FunctionHash(&function);
    assert(originalHash != 0u);
    function.bindingRows[0].contract.signatureHash =
            row.contract.signatureHash + 1u;
    assert(ZrCore_ExecIr_FunctionValidateBindingRows(&function, &diagnostic));
    changedHash = ZrParser_ExecIr_FunctionHash(&function);
    assert(changedHash != 0u && changedHash != originalHash);
    function.bindingRows[0].contract.signatureHash = row.contract.signatureHash;
    assert(ZrCore_ExecIr_FunctionValidateBindingRows(&function, &diagnostic));
    assert(ZrParser_ExecIr_FunctionHash(&function) == originalHash);
    function.bindingRowsSchemaVersion = ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY;
    assert(ZrParser_ExecIr_FunctionHash(&function) == 0u);
    function.bindingRowsSchemaVersion = ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED;
    assert(ZrCore_ExecIr_FunctionValidateBindingRows(&function, &diagnostic));
    assert(ZrParser_ExecIr_FunctionHash(&function) == originalHash);
    assert(function.instructions[0].flags ==
                   (ZR_EXEC_IR_FLAG_MAY_THROW | ZR_EXEC_IR_FLAG_MAY_ALLOCATE) &&
           function.instructions[0].effectIn == 1u &&
           function.instructions[0].effectOut == 2u &&
           function.instructions[0].memoryIn.count == 2u &&
           function.instructions[0].memoryOut.count == 2u);
    if (!ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                      &diagnostic)) {
        fail_with_exec_ir_diagnostic("typed binding-row fixture before scalar optimization",
                                     &diagnostic);
    }

    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    if (!ZrParser_ExecIr_OptimizeScalar(&function, ZR_NULL, &remarks,
                                        &diagnostic)) {
        fail_with_exec_ir_diagnostic("OptimizeScalar typed binding-row fixture",
                                     &diagnostic);
    }
    assert(function.instructions[0].opcode == ZR_EXEC_IR_OPCODE_CALL);
    assert(function.instructions[0].bindingRow == 1u);
    assert(ZrCore_ExecIr_FunctionBindingRowAt(&function, 1u) != ZR_NULL);
    assert(ZrCore_ExecIr_FunctionValidateBindingRows(&function, &diagnostic));
    if (!ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_ALL,
                                      &diagnostic)) {
        fail_with_exec_ir_diagnostic("typed binding-row fixture after scalar optimization",
                                     &diagnostic);
    }
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

static void test_type_test_identity_survives_hash_and_dead_code_cleanup(void) {
    SZrExecIrFunction function;
    SZrExecIrRemarkSink remarks;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrRange sourceResult, testOperands, testResult;
    TZrExecIrValueId sourceValue, resultValue;
    TZrExecIrInstructionId id = 0u;
    TZrUInt64 originalHash, changedHash;

    init_function(&function);
    sourceValue = add_value(&function);
    resultValue = add_value(&function);
    assert(sourceValue != 0u && resultValue != 0u);
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &sourceValue, 1u, &sourceResult));
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CONSTANT,
                       range(0u, 0u), sourceResult, 9u, 0u, 0u, 0u, 404u);
    assert(ZrCore_ExecIr_FunctionAppendOperands(
            &function, &sourceValue, 1u, &testOperands));
    assert(ZrCore_ExecIr_FunctionAppendResults(
            &function, &resultValue, 1u, &testResult));
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_TYPE_TEST;
    instruction.operands = testOperands;
    instruction.results = testResult;
    instruction.typeToken = 1u;
    instruction.matchTypeToken = 7u;
    instruction.sourceId = 405u;
    assert(ZrCore_ExecIr_FunctionAppendInstruction(&function, &instruction, &id));

    originalHash = ZrParser_ExecIr_FunctionHash(&function);
    function.instructions[1].matchTypeToken = 8u;
    changedHash = ZrParser_ExecIr_FunctionHash(&function);
    assert(originalHash != changedHash);
    function.instructions[1].matchTypeToken = 7u;

    ZrParser_ExecIr_RemarkSinkInit(&remarks);
    assert(ZrParser_ExecIr_OptimizeScalar(
            &function, ZR_NULL, &remarks, &diagnostic));
    assert(function.instructions[1].opcode == ZR_EXEC_IR_OPCODE_NOP);
    assert(function.instructions[1].matchTypeToken == 0u);
    assert(ZrParser_ExecIr_VerifyFunction(
            &function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    ZrParser_ExecIr_RemarkSinkFree(&remarks);
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_token_phi_metadata_participates_in_hash(void) {
    SZrExecIrFunction function;
    SZrExecIrBlock *block;
    TZrUInt64 baseline;
    TZrUInt32 region;

    init_function(&function);
    assert(ZrCore_ExecIr_FunctionAddBlock(&function,
                                         ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u);
    block = &function.blocks[0];
    baseline = ZrParser_ExecIr_FunctionHash(&function);
    assert(baseline != 0u);

    block->effectPhiResult = 2u;
    assert(ZrParser_ExecIr_FunctionHash(&function) != baseline);
    block->effectPhiResult = 0u;
    block->effectPhiIncomings.count = 1u;
    assert(ZrParser_ExecIr_FunctionHash(&function) != baseline);
    block->effectPhiIncomings.count = 0u;
    block->effectPhiIncomings.start = 1u;
    assert(ZrParser_ExecIr_FunctionHash(&function) != baseline);
    block->effectPhiIncomings.start = 0u;

    for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region) {
        block->memoryPhiResults[region] = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                (EZrExecIrMemoryClass)region, 2u);
        assert(ZrParser_ExecIr_FunctionHash(&function) != baseline);
        block->memoryPhiResults[region] = 0u;
        block->memoryPhiIncomings[region].count = 1u;
        assert(ZrParser_ExecIr_FunctionHash(&function) != baseline);
        block->memoryPhiIncomings[region].count = 0u;
        block->memoryPhiIncomings[region].start = 1u;
        assert(ZrParser_ExecIr_FunctionHash(&function) != baseline);
        block->memoryPhiIncomings[region].start = 0u;
    }
    assert(ZrParser_ExecIr_FunctionHash(&function) == baseline);
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

static TZrUInt32 unsupported_requirement_pass_invocations;
static TZrUInt32 preceding_pass_invocations;

static TZrBool preceding_mutating_pass(SZrExecIrFunction *function,
                                        SZrExecIrPassContext *context,
                                        TZrBool *changed,
                                        SZrExecIrDiagnostic *diagnostic) {
    (void)context;
    (void)diagnostic;
    ++preceding_pass_invocations;
    if (function != ZR_NULL && function->instructionCount != 0u)
        function->instructions[0].sourceId += 1u;
    if (changed != ZR_NULL) *changed = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool unsupported_requirement_pass(SZrExecIrFunction *function,
                                             SZrExecIrPassContext *context,
                                             TZrBool *changed,
                                             SZrExecIrDiagnostic *diagnostic) {
    (void)context;
    (void)diagnostic;
    ++unsupported_requirement_pass_invocations;
    if (function != ZR_NULL && function->instructionCount != 0u)
        function->instructions[0].sourceId += 1u;
    if (changed != ZR_NULL) *changed = ZR_TRUE;
    return ZR_TRUE;
}

static void test_unsupported_analysis_requirements_roll_back(void) {
    static const TZrUInt32 requirements[] = {
        ZR_EXEC_IR_ANALYSIS_LOOPS,
        ZR_EXEC_IR_ANALYSIS_LIVENESS
    };
    static const TZrChar *const names[] = {
        "requires-loops",
        "requires-liveness"
    };
    TZrUInt32 index;

    for (index = 0u; index < (TZrUInt32)(sizeof(requirements) /
                                        sizeof(requirements[0])); ++index) {
        SZrExecIrFunction function;
        SZrExecIrAnalysisCache cache;
        SZrExecIrPassContext context;
        SZrExecIrPassInfo passes[2];
        SZrExecIrPassFailure failure;
        SZrExecIrDiagnostic diagnostic;
        TZrUInt64 beforeHash;

        build_constant_copy_function(&function);
        beforeHash = ZrParser_ExecIr_FunctionHash(&function);
        memset(passes, 0, sizeof(passes));
        passes[0].name = "preceding-mutation";
        passes[0].run = preceding_mutating_pass;
        passes[1].name = names[index];
        passes[1].requiresAnalysis = requirements[index];
        passes[1].run = unsupported_requirement_pass;
        ZrParser_ExecIr_AnalysisCacheInit(&cache);
        ZrParser_ExecIr_PassFailureInit(&failure);
        memset(&context, 0, sizeof(context));
        context.cache = &cache;
        context.failure = &failure;
        preceding_pass_invocations = 0u;
        unsupported_requirement_pass_invocations = 0u;

        assert(!ZrParser_ExecIr_RunPassPipeline(&function, passes, 2u,
                                                &context, &diagnostic));
        assert(preceding_pass_invocations == 1u);
        assert(unsupported_requirement_pass_invocations == 0u);
        assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        assert(diagnostic.expectedVersion == ZR_EXEC_IR_ANALYSIS_ALL);
        assert(diagnostic.actualVersion == requirements[index]);
        assert(ZrParser_ExecIr_FunctionHash(&function) == beforeHash);
        assert(function.instructions[0].sourceId == 101u);
        assert(context.lastReasonCode == ZR_EXEC_IR_PASS_REASON_NONE);
        assert(context.passesRun == 0u);
        assert(failure.passName != ZR_NULL);
        assert(strcmp(failure.passName, names[index]) == 0);
        assert(failure.function.instructions[0].sourceId == 102u);
        assert(failure.diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        assert(failure.diagnostic.expectedVersion == ZR_EXEC_IR_ANALYSIS_ALL);
        assert(failure.diagnostic.actualVersion == requirements[index]);

        ZrParser_ExecIr_PassFailureFree(&failure);
        ZrParser_ExecIr_AnalysisCacheFree(&cache);
        ZrCore_ExecIr_FreeFunction(&function);
    }
}

static void test_failed_pass_rolls_back(void) {
    SZrExecIrFunction function;
    SZrExecIrFunction before;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    SZrExecIrPassInfo pass;
    SZrExecIrPassFailure failure;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrDiagnostic capturedDiagnostic;
    TZrChar passName[] = "broken";
    TZrUInt64 hash;
    build_constant_copy_function(&function);
    ZrCore_ExecIr_FunctionInit(&before);
    assert(ZrCore_ExecIr_CloneFunction(&function, &before, &diagnostic));
    hash = ZrParser_ExecIr_FunctionHash(&function);
    memset(&pass, 0, sizeof(pass));
    pass.name = passName;
    pass.run = broken_pass;
    ZrParser_ExecIr_AnalysisCacheInit(&cache);
    ZrParser_ExecIr_PassFailureInit(&failure);
    memset(&context, 0, sizeof(context));
    context.cache = &cache;
    context.failure = &failure;
    assert(!ZrParser_ExecIr_RunPassPipeline(&function, &pass, 1u,
                                            &context, &diagnostic));
    assert(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE);
    assert(diagnostic.instructionId == 1u);
    assert(diagnostic.sourceId == 101u);
    assert(hash == ZrParser_ExecIr_FunctionHash(&function));
    assert(failure.passName != ZR_NULL);
    assert(strcmp(failure.passName, "broken") == 0);
    assert(failure.function.id == function.id);
    assert(failure.function.instructionCount == before.instructionCount);
    assert(failure.function.instructions[0].opcode == ZR_EXEC_IR_OPCODE_COUNT);
    assert(failure.diagnostic.code == diagnostic.code);
    assert(failure.diagnostic.instructionId == diagnostic.instructionId);
    assert(failure.diagnostic.sourceId == diagnostic.sourceId);
    capturedDiagnostic = failure.diagnostic;
    assert(!ZrParser_ExecIr_VerifyFunction(&failure.function,
                                           ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    assert(diagnostic.code == capturedDiagnostic.code);
    passName[0] = 'x';
    assert(strcmp(failure.passName, "broken") == 0);
    assert(ZrParser_ExecIr_RunPassPipeline(&function, ZR_NULL, 0u,
                                            &context, &diagnostic));
    assert(failure.passName == ZR_NULL);
    assert(failure.function.instructionCount == 0u);
    ZrParser_ExecIr_PassFailureFree(&failure);
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

#include "ssa_pass_manager_state_maps_cases.inc"

int main(void) {
    test_scalar_pipeline_rebuilds_checkpoint_liveness();
    test_pipeline_rolls_back_failed_map_rebuild();
    test_pipeline_verifies_storage_before_hashing();
    test_scalar_pipeline_and_fixed_point();
    test_throwing_instruction_is_observable();
    test_checked_overflow_is_not_folded();
    test_unused_call_is_preserved();
    test_unused_move_is_preserved();
    test_typed_call_row_survives_scalar_optimization();
    test_dead_source_mapping_is_removed();
    test_type_test_identity_survives_hash_and_dead_code_cleanup();
    test_token_phi_metadata_participates_in_hash();
    test_direct_pass_rejects_malformed_storage();
    test_unsupported_analysis_requirements_roll_back();
    test_failed_pass_rolls_back();
    test_budget_is_bounded();
    return 0;
}
