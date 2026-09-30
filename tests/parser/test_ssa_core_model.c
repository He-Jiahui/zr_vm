#include "zr_vm_core/exec_ir.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void test_empty_module_and_entry_block(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId functionId;
    TZrExecIrBlockId blockId;
    SZrExecIrFunction *function;

    ZrCore_ExecIr_ModuleInit(&module);
    expect_true(module.functionCount == 0u, "new module has functions");
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&module,
                                                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u),
                                                UINT64_C(0x1234),
                                                &functionId),
                "function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    expect_true(function != ZR_NULL, "function query failed");
    blockId = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    expect_true(blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY, "entry block did not receive explicit ID");
    expect_true(ZrCore_ExecIr_FunctionBlockAt(function, blockId) != ZR_NULL,
                "entry block query failed");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_value_builder_rejects_unknown_enums(void) {
    SZrExecIrFunction function;

    ZrCore_ExecIr_FunctionInit(&function);
    expect_true(ZrCore_ExecIr_FunctionAddValue(
                        &function, 1u, (EZrExecIrOwnership)-1,
                        ZR_EXEC_IR_NULLABILITY_NONNULL) ==
                    ZR_EXEC_IR_VALUE_ID_INVALID,
                "negative ownership was accepted");
    expect_true(ZrCore_ExecIr_FunctionAddValue(
                        &function, 1u, ZR_EXEC_IR_OWNERSHIP_GC,
                        (EZrExecIrNullability)-1) ==
                    ZR_EXEC_IR_VALUE_ID_INVALID,
                "negative nullability was accepted");
    expect_true(function.valueCount == 0u,
                "invalid value enum changed the function");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_side_arrays_clone_without_aliasing(void) {
    SZrExecIrModule source;
    SZrExecIrModule clone;
    SZrExecIrFunction *function;
    SZrExecIrFunction *clonedFunction;
    SZrExecIrInstruction instruction;
    TZrExecIrFunctionId functionId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrValueId valueId;
    TZrExecIrValueId operandIds[2];
    TZrExecIrValueId resultIds[1];
    TZrExecIrMemoryTokenId memoryTokens[1] = {7u};
    SZrExecIrPhi phi;
    TZrExecIrBlockId entryBlock;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&source);
    ZrCore_ExecIr_ModuleInit(&clone);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&source,
                                                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 8u),
                                                UINT64_C(0xabcdef),
                                                &functionId),
                "source function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&source, functionId);
    entryBlock = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    expect_true(entryBlock == ZR_EXEC_IR_BLOCK_ID_ENTRY, "source entry block append failed");
    valueId = ZrCore_ExecIr_FunctionAddValue(function,
                                              ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u),
                                              ZR_EXEC_IR_OWNERSHIP_BORROWED,
                                              ZR_EXEC_IR_NULLABILITY_NONNULL);
    expect_true(valueId != ZR_EXEC_IR_VALUE_ID_INVALID, "value append failed");
    operandIds[0] = valueId;
    operandIds[1] = valueId;
    expect_true(ZrCore_ExecIr_FunctionAppendOperands(function, operandIds, 2u, NULL),
                "operand side array append failed");
    resultIds[0] = valueId;
    expect_true(ZrCore_ExecIr_FunctionAppendResults(function, resultIds, 1u, NULL),
                "result side array append failed");
    expect_true(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryTokens, 1u, NULL),
                "memory token side array append failed");
    memset(&phi, 0, sizeof(phi));
    phi.result = valueId;
    expect_true(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u, NULL),
                "phi side array append failed");
    function->blocks[0].phis.count = 1u;
    function->frameLayout = (SZrExecIrFrameLayout *)calloc(1u, sizeof(*function->frameLayout));
    expect_true(function->frameLayout != ZR_NULL, "frame layout allocation failed");
    function->frameLayout->slotCount = 1u;
    function->frameLayout->slotCapacity = 1u;
    function->frameLayout->slots = (SZrExecIrFrameSlot *)calloc(1u, sizeof(*function->frameLayout->slots));
    expect_true(function->frameLayout->slots != ZR_NULL, "frame slot allocation failed");
    function->gcMap = (SZrExecIrGcMap *)calloc(1u, sizeof(*function->gcMap));
    expect_true(function->gcMap != ZR_NULL, "gc map allocation failed");
    function->gcMap->entryCount = 1u;
    function->gcMap->entryCapacity = 1u;
    function->gcMap->entries = (SZrExecIrGcMapEntry *)calloc(1u, sizeof(*function->gcMap->entries));
    expect_true(function->gcMap->entries != ZR_NULL, "gc map entry allocation failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange.start = 0u;
    instruction.operandRange.count = 2u;
    instruction.resultRange.start = 0u;
    instruction.resultRange.count = 1u;
    instruction.sourceId = 41u;
    instruction.typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                         &instruction,
                                                         &instructionId),
                "instruction append failed");
    expect_true(instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID,
                "instruction did not receive an ID");
    expect_true(ZrCore_ExecIr_ValidateModule(&source, &diagnostic),
                "valid source module was rejected");
    expect_true(ZrCore_ExecIr_CloneModule(&source, &clone, &diagnostic),
                "module clone failed");
    clonedFunction = ZrCore_ExecIr_ModuleFunctionAt(&clone, functionId);
    expect_true(clonedFunction != function, "clone unexpectedly aliases function");
    expect_true(clonedFunction->operandCount == function->operandCount &&
                    clonedFunction->instructionCount == function->instructionCount,
                "clone lost side-array counts");
    clonedFunction->operands[0] = ZR_EXEC_IR_VALUE_ID_INVALID;
    clonedFunction->instructions[0].sourceId = 99u;
    clonedFunction->frameLayout->slots[0].slotId = 99u;
    clonedFunction->gcMap->entries[0].site = 99u;
    expect_true(function->operands[0] == valueId && function->instructions[0].sourceId == 41u,
                "clone side arrays alias source");
    expect_true(function->frameLayout->slots[0].slotId == 0u &&
                    function->gcMap->entries[0].site == 0u,
                "clone state maps alias source");

    ZrCore_ExecIr_FreeModule(&clone);
    ZrCore_ExecIr_FreeModule(&source);
}

static SZrExecIrBindingRow make_owned_binding_row(TZrUInt32 rowIndex,
                                                  TZrUInt32 instructionId,
                                                  TZrUInt32 segmentIndex,
                                                  TZrUInt64 moduleHash,
                                                  TZrUInt32 sourceId) {
    SZrExecIrBindingRow row;
    memset(&row, 0, sizeof(row));
    row.rowIndex = rowIndex;
    row.instructionId = instructionId;
    row.segmentIndex = segmentIndex;
    row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    row.contract.targetMetadataToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 77u + rowIndex);
    row.contract.signatureToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u + rowIndex);
    row.contract.signatureHash = UINT64_C(0x9100) + rowIndex;
    row.contract.moduleSignatureHash = moduleHash;
    row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    row.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_NONE;
    row.location.targetIndex = ZR_CALL_BINDING_SLOT_NONE;
    row.sourceId = sourceId;
    return row;
}

static void test_binding_rows_are_owned_typed_and_transactional(void) {
    const TZrUInt64 moduleHash = UINT64_C(0x99887766);
    SZrExecIrFunction source;
    SZrExecIrFunction clone;
    SZrExecIrInstruction instruction;
    SZrExecIrBindingRow rows[2];
    SZrExecIrBindingRow invalidRows[2];
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 sourceHash;
    SZrExecIrBindingRow *sourceRows;
    SZrExecIrBindingRow *reservedRows;

    ZrCore_ExecIr_FunctionInit(&source);
    ZrCore_ExecIr_FunctionInit(&clone);
    source.contract.moduleHash = moduleHash;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.sourceId = 51u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(&source, &instruction, NULL),
                "first binding call append failed");
    instruction.sourceId = 52u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(&source, &instruction, NULL),
                "second binding call append failed");
    rows[0] = make_owned_binding_row(0u, 1u, 0u, moduleHash, 51u);
    rows[1] = make_owned_binding_row(1u, 2u, 1u, moduleHash, 52u);

    expect_true(ZrCore_ExecIr_FunctionSetBindingRows(&source, rows, 2u,
                                                      &diagnostic),
                "Core did not take ownership of valid binding rows");
    expect_true(source.bindingRowsSchemaVersion ==
                        ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED &&
                    source.bindingRowCount == 2u &&
                    source.instructions[0].bindingRow == 1u &&
                    source.instructions[1].bindingRow == 2u,
                "typed row references are not one-based");
    expect_true(ZrCore_ExecIr_FunctionBindingRowAt(&source, 1u) ==
                        &source.bindingRows[0] &&
                    ZrCore_ExecIr_FunctionBindingRowAt(&source, 2u) ==
                        &source.bindingRows[1] &&
                    ZrCore_ExecIr_FunctionBindingRowAt(&source, 0u) == ZR_NULL,
                "typed row accessor decoded the wrong reference");
    expect_true(source.bindingRows != rows && source.bindingRows[1].sourceId == 52u,
                "Core retained a borrowed producer row pointer");
    sourceHash = ZrCore_ExecIr_FunctionBindingRowsHash(&source);
    expect_true(sourceHash != 0u, "typed binding rows have no stable hash");

    expect_true(ZrCore_ExecIr_CloneFunction(&source, &clone, &diagnostic),
                "function clone failed with typed binding rows");
    expect_true(clone.bindingRows != source.bindingRows &&
                    clone.bindingRowCount == source.bindingRowCount &&
                    clone.instructions[1].bindingRow == 2u,
                "function clone did not independently own its row table");
    clone.bindingRows[0].sourceId++;
    expect_true(source.bindingRows[0].sourceId == 51u &&
                    ZrCore_ExecIr_FunctionBindingRowsHash(&clone) != sourceHash,
                "clone row mutation aliased source or escaped the row hash");

    memcpy(invalidRows, rows, sizeof(rows));
    invalidRows[1].instructionId = 3u;
    sourceRows = source.bindingRows;
    expect_true(!ZrCore_ExecIr_FunctionSetBindingRows(&source, invalidRows, 2u,
                                                       &diagnostic),
                "out-of-range row association was accepted");
    expect_true(source.bindingRows == sourceRows &&
                    source.instructions[0].bindingRow == 1u &&
                    source.instructions[1].bindingRow == 2u &&
                    ZrCore_ExecIr_FunctionBindingRowsHash(&source) == sourceHash,
                "failed table replacement partially changed owned rows");

    memcpy(invalidRows, rows, sizeof(rows));
    invalidRows[1].segmentIndex = invalidRows[0].segmentIndex;
    expect_true(!ZrCore_ExecIr_FunctionSetBindingRows(&source, invalidRows, 2u,
                                                       &diagnostic),
                "duplicate source-segment association was accepted");
    expect_true(source.bindingRows == sourceRows &&
                    ZrCore_ExecIr_FunctionBindingRowsHash(&source) == sourceHash,
                "invalid segment association changed the prior table");

    source.instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    expect_true(!ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic) &&
                    diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    diagnostic.instructionId == 1u &&
                    diagnostic.sourceId == 51u,
                "known non-call opcode did not produce a binding-row mismatch diagnostic");
    source.instructions[0].opcode = (TZrUInt16)ZR_EXEC_IR_OPCODE_COUNT;
    expect_true(!ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE &&
                    diagnostic.instructionId == 1u,
                "unknown opcode lost its unknown-opcode classification");
    source.instructions[0].opcode = ZR_EXEC_IR_OPCODE_CALL;

    source.contract.moduleHash++;
    expect_true(!ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic),
                "row contract with a different module identity was accepted");
    source.contract.moduleHash--;
    source.instructions[0].bindingRow = 2u;
    expect_true(!ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic),
                "row reference pointing at another instruction was accepted");
    source.instructions[0].bindingRow = 1u;
    expect_true(ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic),
                "restored row association did not validate");

    expect_true(ZrCore_ExecIr_FunctionSetBindingRows(&source, rows, 1u,
                                                      &diagnostic),
                "single-row accessor fixture install failed");
    sourceRows = source.bindingRows;
    expect_true(sourceRows != ZR_NULL && source.bindingRowCount == 1u &&
                    source.bindingRowCapacity == 1u,
                "single-row accessor fixture was not owned at exact capacity");
    source.bindingRowCount = 2u;
    expect_true(ZrCore_ExecIr_FunctionBindingRowAt(&source, 2u) == ZR_NULL,
                "accessor returned a row beyond its owned allocation after count corruption");
    source.bindingRowCount = 1u;
    source.bindingRowCapacity = 0u;
    expect_true(ZrCore_ExecIr_FunctionBindingRowAt(&source, 1u) == ZR_NULL,
                "accessor accepted an owned pointer with zero capacity");
    source.bindingRowCapacity = 1u;
    source.bindingRows = ZR_NULL;
    expect_true(ZrCore_ExecIr_FunctionBindingRowAt(&source, 1u) == ZR_NULL,
                "accessor accepted nonzero capacity with missing storage");
    source.bindingRows = sourceRows;
    expect_true(ZrCore_ExecIr_FunctionBindingRowAt(&source, 1u) == sourceRows,
                "restored owned table was not accessible after metadata corruption");

    expect_true(ZrCore_ExecIr_FunctionSetBindingRows(&source, ZR_NULL, 0u,
                                                      &diagnostic),
                "typed empty row table could not replace a populated table");
    expect_true(source.bindingRowsSchemaVersion ==
                        ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED &&
                    source.bindingRows == ZR_NULL && source.bindingRowCount == 0u &&
                    source.bindingRowCapacity == 0u &&
                    source.instructions[0].bindingRow == 0u &&
                    source.instructions[1].bindingRow == 0u &&
                    ZrCore_ExecIr_FunctionBindingRowsHash(&source) != 0u &&
                    ZrCore_ExecIr_FunctionBindingRowsHash(&source) != sourceHash,
                "empty projection erased typed mode or retained stale rows");
    expect_true(ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic),
                "typed empty table was rejected");

    source.bindingRows = (SZrExecIrBindingRow *)calloc(1u, sizeof(*source.bindingRows));
    expect_true(source.bindingRows != ZR_NULL, "reserved empty table allocation failed");
    reservedRows = source.bindingRows;
    source.bindingRowCapacity = 1u;
    expect_true(ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic),
                "valid reserved empty table was rejected");
    source.bindingRowCapacity = 0u;
    expect_true(!ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic),
                "non-null zero-capacity table was accepted");
    source.bindingRowCapacity = 1u;
    source.bindingRows = ZR_NULL;
    expect_true(!ZrCore_ExecIr_FunctionValidateBindingRows(&source, &diagnostic),
                "nonzero-capacity null table was accepted");
    source.bindingRows = reservedRows;
    ZrCore_ExecIr_FreeFunction(&clone);
    ZrCore_ExecIr_FreeFunction(&source);
}

static void test_binding_rows_pass_complete_verifier_gate(void) {
    const TZrUInt64 moduleHash = UINT64_C(0x77112233);
    const TZrUInt64 signatureHash = UINT64_C(0x44556677);
    const TZrExecIrTypeToken i64Type = ZR_VALUE_TYPE_INT64;
    const TZrExecIrMemoryTokenId memoryInTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)
    };
    const TZrExecIrMemoryTokenId memoryOutTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)
    };
    const SZrExecIrConstant constant = {ZR_VALUE_TYPE_INT64, 0u, 42u};
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    SZrExecIrBindingRow row;
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrRange constantRange, constantResult, callOperand, callResult;
    SZrExecIrRange returnOperand, memoryIn, memoryOut;
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId inputValue, callValue;
    TZrExecIrInstructionId instructionId;

    ZrCore_ExecIr_ModuleInit(&module);
    module.id = 1u;
    module.moduleHash = moduleHash;
    expect_true(ZrCore_ExecIr_ModuleAppendConstant(&module, &constant, 1u,
                                                    &constantRange),
                "verifier fixture constant append failed");
    expect_true(ZrCore_ExecIr_ModuleAddFunction(
                        &module,
                        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 91u),
                        signatureHash, &functionId),
                "verifier fixture function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    expect_true(function != ZR_NULL, "verifier fixture function lookup failed");
    function->contract.moduleHash = moduleHash;
    expect_true(ZrCore_ExecIr_FunctionAddBlock(
                        function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
                        ZR_EXEC_IR_BLOCK_ID_ENTRY,
                "verifier fixture entry block append failed");

    inputValue = ZrCore_ExecIr_FunctionAddValue(
            function, i64Type, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    expect_true(inputValue == 1u &&
                    ZrCore_ExecIr_FunctionAppendResults(
                            function, &inputValue, 1u, &constantResult),
                "verifier fixture constant result setup failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = constantResult;
    instruction.layoutId = constantRange.start;
    instruction.sourceId = 501u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(
                        function, &instruction, &instructionId) &&
                    instructionId == 1u,
                "verifier fixture constant instruction append failed");

    callValue = ZrCore_ExecIr_FunctionAddValue(
            function, i64Type, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    expect_true(callValue == 2u &&
                    ZrCore_ExecIr_FunctionAppendResults(
                            function, &callValue, 1u, &callResult) &&
                    ZrCore_ExecIr_FunctionAppendOperands(
                            function, &inputValue, 1u, &callOperand),
                "verifier fixture call value setup failed");
    expect_true(ZrCore_ExecIr_FunctionAppendMemoryTokens(
                        function, memoryInTokens, 2u, &memoryIn) &&
                    ZrCore_ExecIr_FunctionAppendMemoryTokens(
                            function, memoryOutTokens, 2u, &memoryOut),
                "verifier fixture call memory chain setup failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    instruction.operands = callOperand;
    instruction.results = callResult;
    instruction.memoryIn = memoryIn;
    instruction.memoryOut = memoryOut;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 502u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(
                        function, &instruction, &instructionId) &&
                    instructionId == 2u,
                "verifier fixture CALL append failed");

    expect_true(ZrCore_ExecIr_FunctionAppendOperands(
                        function, &callValue, 1u, &returnOperand),
                "verifier fixture return operand setup failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperand;
    instruction.sourceId = 503u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(
                        function, &instruction, &instructionId) &&
                    instructionId == 3u,
                "verifier fixture RETURN append failed");
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = 3u;
    function->blocks[0].terminatorInstructionId = 3u;

    row = make_owned_binding_row(0u, 2u, 0u, moduleHash, 502u);
    expect_true(ZrCore_ExecIr_FunctionSetBindingRows(function, &row, 1u,
                                                      &diagnostic),
                "verifier fixture typed binding row setup failed");
    expect_true(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                              &diagnostic),
                "complete typed-row CALL fixture failed VerifyFunction");
    expect_true(ZrCore_ExecIr_VerifyModule(&module, &diagnostic),
                "complete typed-row CALL fixture failed VerifyModule");

    function->instructions[1].bindingRow = ZR_EXEC_IR_BINDING_ROW_REF_NONE;
    expect_true(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL,
                                               &diagnostic) &&
                    diagnostic.code ==
                            ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    diagnostic.instructionId == 2u &&
                    diagnostic.sourceId == 502u &&
                    diagnostic.expectedVersion == 1u &&
                    diagnostic.actualVersion ==
                            ZR_EXEC_IR_BINDING_ROW_REF_NONE,
                "VerifyFunction accepted a broken typed-row CALL association");
    expect_true(!ZrCore_ExecIr_VerifyModule(&module, &diagnostic) &&
                    diagnostic.code ==
                            ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    diagnostic.instructionId == 2u &&
                    diagnostic.sourceId == 502u &&
                    diagnostic.expectedVersion == 1u &&
                    diagnostic.actualVersion ==
                            ZR_EXEC_IR_BINDING_ROW_REF_NONE,
                "VerifyModule accepted a broken typed-row CALL association");
    function->instructions[1].bindingRow = 1u;
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_invalid_opcode_and_overflow_fail_before_allocation(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId functionId;
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&module);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&module,
                                                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 9u),
                                                UINT64_C(0x55),
                                                &functionId),
                "overflow fixture function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    expect_true(!ZrCore_ExecIr_FunctionReserveOperandsEx(function, SIZE_MAX, &diagnostic),
                "operand reserve overflow was accepted");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                "operand reserve overflow lost its diagnostic");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)UINT16_MAX;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                         &instruction,
                                                         NULL),
                "invalid opcode should be stored for verifier diagnostics");
    expect_true(!ZrCore_ExecIr_ValidateModule(&module, &diagnostic),
                "unknown opcode was accepted by structural validation");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE &&
                    diagnostic.instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID,
                "unknown opcode diagnostic lost instruction identity");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_validation_rejects_null_operand_pool_without_dereference(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId functionId;
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&module);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(
                        &module,
                        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 10u),
                        UINT64_C(0x66),
                        &functionId),
                "null operand fixture function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    expect_true(ZrCore_ExecIr_FunctionAddBlock(function,
                                                ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
                        ZR_EXEC_IR_BLOCK_ID_ENTRY,
                "null operand fixture entry append failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_COPY;
    instruction.operandRange.count = 1u;
    /* Deliberately claim one occupied operand without providing a pool. */
    function->operandCount = 1u;
    function->operandCapacity = 1u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
                "null operand fixture instruction append failed");
    expect_true(!ZrCore_ExecIr_ValidateModule(&module, &diagnostic),
                "null operand pool was accepted");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                "null operand pool reported the wrong diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_failed_module_clone_reclaims_partially_copied_function(void) {
    SZrExecIrModule source;
    SZrExecIrModule destination;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId id;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&source);
    ZrCore_ExecIr_ModuleInit(&destination);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&destination, 73u, 91u, &id),
                "clone destination setup failed");
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&source, 81u, 101u, &id),
                "first clone source function setup failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&source, id);
    expect_true(ZrCore_ExecIr_FunctionAddValue(function, 7u,
                    ZR_EXEC_IR_OWNERSHIP_BORROWED,
                    ZR_EXEC_IR_NULLABILITY_NONNULL) != ZR_EXEC_IR_VALUE_ID_INVALID,
                "first clone source value setup failed");
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&source, 82u, 102u, &id),
                "second clone source function setup failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&source, id);
    expect_true(ZrCore_ExecIr_FunctionAddValue(function, 8u,
                    ZR_EXEC_IR_OWNERSHIP_BORROWED,
                    ZR_EXEC_IR_NULLABILITY_NONNULL) != ZR_EXEC_IR_VALUE_ID_INVALID,
                "second clone source value setup failed");
    /* Fail after the second temporary function has allocated its value pool. */
    function->instructionCount = 1u;
    function->instructionCapacity = 1u;
    expect_true(!ZrCore_ExecIr_CloneModule(&source, &destination, &diagnostic),
                "malformed source pool was cloned");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    diagnostic.functionToken == 82u,
                "failed clone did not identify the source function");
    expect_true(destination.functionCount == 1u &&
                    destination.functions[0].functionToken == 73u,
                "failed clone changed the published destination");
    ZrCore_ExecIr_FreeModule(&destination);
    ZrCore_ExecIr_FreeModule(&source);
}

static void test_structure_requires_reciprocal_cfg_edges(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId id;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId target;
    SZrExecIrInstruction branch;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&module);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&module, 1u, 1u, &id),
                "CFG fixture function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, id);
    entry = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    target = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    expect_true(entry == ZR_EXEC_IR_BLOCK_ID_ENTRY && target == 2u,
                "CFG fixture block IDs invalid");
    expect_true(ZrCore_ExecIr_FunctionAppendSuccessors(
                    function, &target, 1u, &function->blocks[entry - 1u].successorRange),
                "CFG fixture successor append failed");
    expect_true(ZrCore_ExecIr_FunctionAppendPredecessors(
                    function, &entry, 1u, &function->blocks[target - 1u].predecessorRange),
                "CFG fixture predecessor append failed");
    memset(&branch, 0, sizeof(branch));
    branch.opcode = ZR_EXEC_IR_OPCODE_BRANCH;
    branch.successorRange = function->blocks[entry - 1u].successorRange;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(function, &branch, NULL),
                "CFG fixture terminator append failed");
    function->blocks[entry - 1u].instructionRange.count = 1u;
    expect_true(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                               &diagnostic),
                "reciprocal CFG edge rejected");

    function->blocks[target - 1u].predecessorRange.count = 0u;
    expect_true(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                                &diagnostic),
                "forward CFG edge without reciprocal predecessor accepted");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
                    diagnostic.blockId == entry && diagnostic.expectedVersion == entry &&
                    diagnostic.actualVersion == target,
                "forward CFG edge diagnostic lost its endpoints");

    function->blocks[target - 1u].predecessorRange.count = 1u;
    function->blocks[entry - 1u].successorRange.count = 0u;
    function->instructions[0].successorRange.count = 0u;
    expect_true(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                                &diagnostic),
                "predecessor without reciprocal successor accepted");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
                    diagnostic.blockId == target && diagnostic.expectedVersion == entry &&
                    diagnostic.actualVersion == target,
                "reverse CFG edge diagnostic lost its endpoints");

    function->blocks[entry - 1u].successorRange.count = 1u;
    function->instructions[0].successorRange.count = 1u;
    expect_true(ZrCore_ExecIr_FunctionAppendPredecessors(function, &entry, 1u, NULL),
                "duplicate predecessor append failed");
    function->blocks[target - 1u].predecessorRange.count = 2u;
    expect_true(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                               &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
                    diagnostic.blockId == target && diagnostic.expectedVersion == entry &&
                    diagnostic.actualVersion == target,
                "unmatched duplicate predecessor accepted");

    function->blocks[target - 1u].predecessorRange.count = 1u;
    expect_true(ZrCore_ExecIr_FunctionAppendSuccessors(function, &target, 1u, NULL),
                "duplicate successor append failed");
    function->blocks[entry - 1u].successorRange.count = 2u;
    expect_true(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                               &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
                    diagnostic.blockId == entry && diagnostic.expectedVersion == entry &&
                    diagnostic.actualVersion == target,
                "unmatched duplicate successor accepted");

    function->blocks[target - 1u].predecessorRange.count = 2u;
    {
        TZrExecIrValueId condition = ZrCore_ExecIr_FunctionAddValue(
                function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        SZrExecIrRange conditionOperand = {0};
        expect_true(condition != ZR_EXEC_IR_VALUE_ID_INVALID &&
                        ZrCore_ExecIr_FunctionAppendOperands(
                                function, &condition, 1u, &conditionOperand),
                    "paired duplicate CFG condition append failed");
        function->instructions[0].opcode = ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH;
        function->instructions[0].operandRange = conditionOperand;
        function->instructions[0].successorRange.count = 2u;
    }
    expect_true(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                              &diagnostic),
                "matched duplicate CFG edges rejected");
    function->instructions[0].successorRange.count = 1u;
    expect_true(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                               &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
                    diagnostic.blockId == entry && diagnostic.instructionId == 1u &&
                    diagnostic.expectedVersion == 2u && diagnostic.actualVersion == 1u,
                "terminator successor count diverged from block CFG without a diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_structure_rejects_zero_value_phi_incoming(void) {
    SZrExecIrFunction function;
    SZrExecIrPhiIncoming incoming = {1u, ZR_EXEC_IR_VALUE_ID_INVALID};
    SZrExecIrPhi phi = {0};
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 1u;
    expect_true(ZrCore_ExecIr_FunctionAddBlock(&function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                "value phi fixture entry append failed");
    phi.result = ZrCore_ExecIr_FunctionAddValue(&function, 1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_NULLABLE);
    expect_true(phi.result != ZR_EXEC_IR_VALUE_ID_INVALID,
                "value phi fixture result append failed");
    expect_true(ZrCore_ExecIr_FunctionAppendPhiIncoming(&function, &incoming, 1u, &phi.incomings),
                "value phi fixture incoming append failed");
    expect_true(ZrCore_ExecIr_FunctionAppendPhis(&function, &phi, 1u, NULL),
                "value phi fixture phi append failed");
    function.blocks[0].phis.count = 1u;
    expect_true(!ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                               &diagnostic),
                "zero ordinary value phi incoming was accepted");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
                    diagnostic.actualVersion == ZR_EXEC_IR_VALUE_ID_INVALID,
                "zero ordinary value phi lost its invalid-value diagnostic");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_structure_rejects_early_terminator(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction ret = {0};
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 2u;
    expect_true(ZrCore_ExecIr_FunctionAddBlock(&function,
                    ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                "early terminator fixture entry append failed");
    ret.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(&function, &ret, NULL),
                "early terminator fixture first return append failed");
    function.blocks[0].instructionRange.count = 1u;
    function.blocks[0].terminatorInstructionId = 1u;
    expect_true(ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                              &diagnostic),
                "single return fixture rejected");
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(&function, &ret, NULL),
                "early terminator fixture second return append failed");
    function.blocks[0].instructionRange.count = 2u;
    function.blocks[0].terminatorInstructionId = 2u;
    expect_true(!ZrCore_ExecIr_VerifyFunction(&function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                               &diagnostic) &&
                    diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR &&
                    diagnostic.blockId == 1u && diagnostic.instructionId == 1u &&
                    diagnostic.expectedVersion == 2u && diagnostic.actualVersion == 1u,
                "structure accepted a terminator before the end of its block");
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_empty_module_and_entry_block();
    test_value_builder_rejects_unknown_enums();
    test_side_arrays_clone_without_aliasing();
    test_binding_rows_are_owned_typed_and_transactional();
    test_binding_rows_pass_complete_verifier_gate();
    test_invalid_opcode_and_overflow_fail_before_allocation();
    test_validation_rejects_null_operand_pool_without_dereference();
    test_failed_module_clone_reclaims_partially_copied_function();
    test_structure_requires_reciprocal_cfg_edges();
    test_structure_rejects_zero_value_phi_incoming();
    test_structure_rejects_early_terminator();
    puts("ssa core model PASS");
    return EXIT_SUCCESS;
}
