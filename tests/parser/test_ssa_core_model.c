#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
void ZrCore_ExecIr_TestAllocatorReset(void);
void ZrCore_ExecIr_TestAllocatorFailAt(TZrUInt64 allocationIndex);
TZrUInt64 ZrCore_ExecIr_TestAllocatorCount(void);
TZrUInt64 ZrCore_ExecIr_TestAllocatorLiveCount(void);
#endif

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

static void test_block_accessor_respects_publication_boundary(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrBlock *mutableBlock;
    const SZrExecIrBlock *publishedBlock;
    TZrExecIrInstructionId instructionId;
    TZrExecIrBlockId blockId;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = 1u;
    blockId = ZrCore_ExecIr_FunctionAddBlock(&function,
                                             ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    expect_true(blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY,
                "publication fixture entry block append failed");

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(&function,
                                                         &instruction,
                                                         &instructionId),
                "publication fixture return append failed");
    mutableBlock = ZrCore_ExecIr_FunctionBlockAt(&function, blockId);
    expect_true(mutableBlock != ZR_NULL,
                "unsealed builder could not access its block");
    mutableBlock->instructionRange.count = 1u;
    mutableBlock->terminatorInstructionId = instructionId;
    expect_true(mutableBlock->instructionRange.count == 1u,
                "unsealed builder could not mutate its block");

    expect_true(ZrCore_ExecIr_FunctionSeal(&function, &diagnostic),
                "publication fixture seal failed");
    expect_true(ZrCore_ExecIr_FunctionBlockAt(&function, blockId) == ZR_NULL,
                "mutable block accessor exposed a sealed function");
    publishedBlock = ZrCore_ExecIr_FunctionBlockAtConst(&function, blockId);
    expect_true(publishedBlock != ZR_NULL &&
                    publishedBlock->instructionRange.count == 1u,
                "const block accessor lost the sealed block");

    ZrCore_ExecIr_FreeFunction(&function);
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
    SZrExecIrRange constantRange = {0}, constantResult = {0};
    SZrExecIrRange callOperand = {0}, callResult = {0};
    SZrExecIrRange returnOperand = {0}, memoryIn = {0}, memoryOut = {0};
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
    /* 第二源函数缺少声称存在的 instruction 池，先在 shape 预检拒绝；候选 module 回滚此前函数及已初始化的当前槽。 */
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

static void test_module_clone_initializes_early_rejected_function_slot(void) {
    SZrExecIrModule source;
    SZrExecIrModule destination;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId id;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&source);
    ZrCore_ExecIr_ModuleInit(&destination);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&destination, 173u, 191u, &id),
                "early-rejection destination setup failed");
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&source, 181u, 201u, &id),
                "early-rejection first source function setup failed");
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&source, 182u, 202u, &id),
                "early-rejection second source function setup failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&source, id);
    expect_true(function != ZR_NULL, "early-rejection source function lookup failed");
    function->valueCount = 2u;
    function->valueCapacity = 1u;

    expect_true(!ZrCore_ExecIr_CloneModule(&source, &destination, &diagnostic),
                "clone accepted an early-rejected function shape");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    diagnostic.functionToken == 182u &&
                    destination.functionCount == 1u &&
                    destination.functions[0].functionToken == 173u,
                "early shape rejection did not preserve the published destination");

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

static void test_clone_rejects_side_array_count_beyond_capacity(void) {
    SZrExecIrFunction source;
    SZrExecIrFunction destination;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&source);
    ZrCore_ExecIr_FunctionInit(&destination);
    source.id = 1u;
    source.functionToken = 91u;
    source.values = (SZrExecIrValue *)calloc(2u, sizeof(*source.values));
    expect_true(source.values != ZR_NULL, "malformed clone fixture allocation failed");
    source.valueCapacity = 1u;
    source.valueCount = 2u;
    source.values[0].id = 1u;
    source.values[1].id = 2u;

    destination.id = 7u;
    destination.functionToken = 73u;
    expect_true(!ZrCore_ExecIr_CloneFunction(&source, &destination, &diagnostic),
                "clone accepted a side-array count beyond capacity");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    diagnostic.functionToken == source.functionToken &&
                    diagnostic.instructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
                    diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_INVALID &&
                    destination.id == 7u && destination.functionToken == 73u,
                "malformed clone changed the published destination");

    ZrCore_ExecIr_FreeFunction(&destination);
    ZrCore_ExecIr_FunctionInit(&destination);
    destination.id = 8u;
    destination.functionToken = 74u;
    free(source.values);
    source.values = ZR_NULL;
    source.valueCount = 0u;
    source.valueCapacity = 0u;
    source.gcMap = (SZrExecIrGcMap *)calloc(1u, sizeof(*source.gcMap));
    expect_true(source.gcMap != ZR_NULL, "malformed GC-map fixture allocation failed");
    source.gcMap->entries = (SZrExecIrGcMapEntry *)calloc(
            2u, sizeof(*source.gcMap->entries));
    expect_true(source.gcMap->entries != ZR_NULL,
                "malformed GC-map entry allocation failed");
    source.gcMap->entryCount = 2u;
    source.gcMap->entryCapacity = 1u;
    expect_true(!ZrCore_ExecIr_CloneFunction(&source, &destination, &diagnostic),
                "clone accepted a GC-map count beyond capacity");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    destination.id == 8u && destination.functionToken == 74u,
                "malformed GC-map clone changed the published destination");

    ZrCore_ExecIr_FreeFunction(&destination);
    ZrCore_ExecIr_FunctionInit(&destination);
    destination.id = 9u;
    destination.functionToken = 75u;
    ZrCore_ExecIr_FreeFunction(&source);
    ZrCore_ExecIr_FunctionInit(&source);
    source.id = 1u;
    source.functionToken = 91u;
    source.frameLayout = (SZrExecIrFrameLayout *)calloc(
            1u, sizeof(*source.frameLayout));
    expect_true(source.frameLayout != ZR_NULL,
                "malformed frame-layout fixture allocation failed");
    source.frameLayout->slots = (SZrExecIrFrameSlot *)calloc(
            2u, sizeof(*source.frameLayout->slots));
    expect_true(source.frameLayout->slots != ZR_NULL,
                "malformed frame-slot allocation failed");
    source.frameLayout->slotCount = 2u;
    source.frameLayout->slotCapacity = 1u;
    expect_true(!ZrCore_ExecIr_CloneFunction(&source, &destination, &diagnostic),
                "clone accepted a frame-slot count beyond capacity");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
                    destination.id == 9u && destination.functionToken == 75u,
                "malformed frame-layout clone changed the published destination");

    ZrCore_ExecIr_FreeFunction(&destination);
    ZrCore_ExecIr_FreeFunction(&source);
}

#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
#include "exec_ir_clone_test_allocator_cases.inc"
#endif

int main(int argc, char **argv) {
#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
    if (argc == 2 && strcmp(argv[1], "clone-function-faults") == 0) {
        test_function_clone_reports_allocation_failures_atomically();
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "clone-module-faults") == 0) {
        test_module_clone_reports_allocation_failures_atomically();
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "clone-input-errors") == 0) {
        test_clone_distinguishes_invalid_input_from_capacity_overflow();
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "clone-function-capacity") == 0) {
        test_function_clone_rejects_byte_overflow();
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "clone-module-capacity") == 0) {
        test_module_clone_rejects_byte_overflow();
        return EXIT_SUCCESS;
    }
#else
    (void)argc;
    (void)argv;
#endif
    test_empty_module_and_entry_block();
    test_block_accessor_respects_publication_boundary();
    test_value_builder_rejects_unknown_enums();
    test_side_arrays_clone_without_aliasing();
    test_binding_rows_are_owned_typed_and_transactional();
    test_binding_rows_pass_complete_verifier_gate();
    test_invalid_opcode_and_overflow_fail_before_allocation();
    test_validation_rejects_null_operand_pool_without_dereference();
    test_failed_module_clone_reclaims_partially_copied_function();
    test_module_clone_initializes_early_rejected_function_slot();
    test_structure_requires_reciprocal_cfg_edges();
    test_structure_rejects_zero_value_phi_incoming();
    test_structure_rejects_early_terminator();
    test_clone_rejects_side_array_count_beyond_capacity();
#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
    test_function_clone_reports_allocation_failures_atomically();
    test_module_clone_reports_allocation_failures_atomically();
    test_clone_distinguishes_invalid_input_from_capacity_overflow();
    expect_true(ZrCore_ExecIr_TestAllocatorLiveCount() == 0u,
                "core-model test leaked an owned allocation");
#endif
    puts("ssa core model PASS");
    return EXIT_SUCCESS;
}

/**
 * @fn void ZrCore_ExecIr_TestAllocatorReset(void)
 *
 * ## Summary
 * 声明故障支持的串行重置入口：清除累计分配尝试数和失败ordinal。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；正常返回时attempt count与fail ordinal均置零，既有live分配保持。
 *
 * ## Constraints and lifecycle
 * 只在ZR_EXEC_IR_TEST_ALLOCATOR编译开启时可用。实际状态在独立allocator C中；Reset不清空已跟踪地址链或liveCount，不释放分配。不得与分配、Free或其他测试并发调用。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:10
 * @see tests/parser/exec_ir_clone_test_allocator.c:84
 * @see tests/parser/exec_ir_clone_test_allocator.c:85
 * @see tests/parser/exec_ir_clone_test_allocator.c:86
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:218
 */

/**
 * @fn void ZrCore_ExecIr_TestAllocatorFailAt(TZrUInt64 allocationIndex)
 *
 * ## Summary
 * 声明故障支持的ordinal设置入口：将尝试计数重新置零并指定唯一应失败的分配序号。
 *
 * ## Parameters
 * @param[in] allocationIndex 从下一次包装分配开始计数的一基失败ordinal；零禁用故障。
 *
 * ## Returns
 * 无C返回值；设置新计数基点/ordinal，不尝试或释放分配。
 *
 * ## Constraints and lifecycle
 * allocationIndex为零关闭注入；非零与随后包装malloc/calloc/realloc的累计调用序号精确相等时失败。wrapper记账节点用未替换的宿主malloc，不纳入该ordinal；其他未被force-include的TU/库不在此范围。状态非原子，要求串行。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:11
 * @see tests/parser/exec_ir_clone_test_allocator.c:45
 * @see tests/parser/exec_ir_clone_test_allocator.c:89
 * @see tests/parser/exec_ir_clone_test_allocator.c:90
 * @see tests/parser/exec_ir_clone_test_allocator.c:91
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:227
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:268
 */

/**
 * @fn TZrUInt64 ZrCore_ExecIr_TestAllocatorCount(void)
 *
 * ## Summary
 * 声明读取包装分配尝试次数的接口，供.inc的有限故障遍历核对命中点。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 当前包装分配尝试计数的值；不转移资源，也不重置计数。
 *
 * ## Constraints and lifecycle
 * 统计malloc/calloc/realloc的尝试，包括被注入拒绝的调用；不含free和链表记账的宿主malloc。Reset/FailAt会重新置零，不是全进程分配总数；只串行读取。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:12
 * @see tests/parser/exec_ir_clone_test_allocator.c:45
 * @see tests/parser/exec_ir_clone_test_allocator.c:94
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:221
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:231
 */

/**
 * @fn TZrUInt64 ZrCore_ExecIr_TestAllocatorLiveCount(void)
 *
 * ## Summary
 * 声明读取allocator包装域内当前已跟踪非NULL分配数，用作默认Core model入口末尾的泄漏守卫。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 当前跟踪live地址数；这是计数快照，不提供池借用或释放操作。
 *
 * ## Constraints and lifecycle
 * 只覆盖同一链接支持状态中由包装器登记/注销的地址，不证明全堆、GC或未替换库无泄漏。Reset并不清liveCount；realloc成功更新原节点，free只注销匹配地址。不得并发修改链或计数。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:13
 * @see tests/parser/exec_ir_clone_test_allocator.c:20
 * @see tests/parser/exec_ir_clone_test_allocator.c:31
 * @see tests/parser/exec_ir_clone_test_allocator.c:67
 * @see tests/parser/exec_ir_clone_test_allocator.c:79
 * @see tests/parser/exec_ir_clone_test_allocator.c:98
 * @see tests/parser/test_ssa_core_model.c:950
 */

/**
 * @fn void expect_true(int condition, const char *message)
 *
 * ## Summary
 * 将Core model及条件文本包含故障场景的单个布尔检查转为进程级失败。它只在失败时打印message并立即退出，不建立测试框架或清理栈。
 *
 * ## Parameters
 * @param[in] condition 已求值的检查结果；零表示失败。
 * @param[in] message 失败时借用并写入stderr的有效字符串；不复制或接管。
 *
 * ## Returns
 * 无C返回值；非零条件正常返回，零条件调用exit(EXIT_FAILURE)，不会回到当前case。
 *
 * ## Constraints and lifecycle
 * condition只求值后以int传入；message须为可读的NUL结尾消息，失败期间保持有效。成功不写被测对象；失败不释放调用方局部资源。其调用属于同一TU及有条件包含的.inc，其他文件同名static函数不属于本实体。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:16
 * @see tests/parser/test_ssa_core_model.c:17
 * @see tests/parser/test_ssa_core_model.c:19
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:13
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:93
 */

/**
 * @fn void test_empty_module_and_entry_block(void)
 *
 * ## Summary
 * 核对ModuleInit的空函数计数、ModuleAddFunction/ModuleFunctionAt的基本关联及首个ENTRY block的显式ID和查询结果。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * Module是局部独占owner，Function指针借自module，仅在FreeModule前使用。只检查此有限构造，不执行VerifyFunction、Seal、VM或函数内容。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:23
 * @see tests/parser/test_ssa_core_model.c:29
 * @see tests/parser/test_ssa_core_model.c:30
 * @see tests/parser/test_ssa_core_model.c:31
 * @see tests/parser/test_ssa_core_model.c:36
 * @see tests/parser/test_ssa_core_model.c:38
 * @see tests/parser/test_ssa_core_model.c:40
 * @see tests/parser/test_ssa_core_model.c:42
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:496
 */

/**
 * @fn void test_block_accessor_respects_publication_boundary(void)
 *
 * ## Summary
 * 构造单RETURN块，先通过可变block借用设置范围和terminator，再Seal并检查可变访问器拒绝已发布function、const访问器仍能读取块。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * Seal前获得的mutableBlock只是池内借用；此case不在Seal后继续写旧借用。发布边界检查是访问器返回行为，不会撤销C语言中已拿到的原指针。FunctionSeal调用STRUCTURE校验，未执行SSA/EFFECT或VM。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:45
 * @see tests/parser/test_ssa_core_model.c:68
 * @see tests/parser/test_ssa_core_model.c:71
 * @see tests/parser/test_ssa_core_model.c:76
 * @see tests/parser/test_ssa_core_model.c:78
 * @see tests/parser/test_ssa_core_model.c:80
 * @see tests/parser/test_ssa_core_model.c:85
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:995
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1006
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1013
 */

/**
 * @fn void test_value_builder_rejects_unknown_enums(void)
 *
 * ## Summary
 * 将负枚举值分别传给value builder的ownership和nullability，检查均返回INVALID且逻辑valueCount保持零。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 仅采用两种显式非法枚举输入；不读取无效ValueId，也不把valueCount不变扩成所有池地址/容量或分配次数均不变。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:88
 * @see tests/parser/test_ssa_core_model.c:92
 * @see tests/parser/test_ssa_core_model.c:97
 * @see tests/parser/test_ssa_core_model.c:102
 * @see tests/parser/test_ssa_core_model.c:104
 */

/**
 * @fn void test_side_arrays_clone_without_aliasing(void)
 *
 * ## Summary
 * 用实际Module/Function构造器填入有限side arrays及frameLayout/gcMap，ValidateModule后CloneModule；修改clone的operand、instruction.sourceId、frame slot及GC entry，并检查对应源元素保持原值。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 两份module分别拥有自己的分配并依次Free；Function查询结果借自各module。测试只比较operand/instruction计数及上述四类元素的独立性；results、memory tokens、PHI和其他池虽参与构造，不代表逐池地址或元素均被断言。这里只调用一般ValidateModule，ADD把同一value作为operand/result不构成完整SSA接受证明。源释放后未再读clone。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:107
 * @see tests/parser/test_ssa_core_model.c:140
 * @see tests/parser/test_ssa_core_model.c:152
 * @see tests/parser/test_ssa_core_model.c:158
 * @see tests/parser/test_ssa_core_model.c:178
 * @see tests/parser/test_ssa_core_model.c:180
 * @see tests/parser/test_ssa_core_model.c:183
 * @see tests/parser/test_ssa_core_model.c:187
 * @see tests/parser/test_ssa_core_model.c:191
 * @see tests/parser/test_ssa_core_model.c:193
 * @see tests/parser/test_ssa_core_model.c:197
 * @see tests/parser/test_ssa_core_model.c:198
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:691
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1068
 */

/**
 * @fn SZrExecIrBindingRow make_owned_binding_row(TZrUInt32 rowIndex, TZrUInt32 instructionId, TZrUInt32 segmentIndex, TZrUInt64 moduleHash, TZrUInt32 sourceId)
 *
 * ## Summary
 * 返回一个清零后填妥直接CALL绑定契约及NONE relocation的测试行；rowIndex/instruction/segment/module/source身份由调用者指定。
 *
 * ## Parameters
 * @param[in] rowIndex 测试行的零基表索引；同时用于生成有限metadata token和signatureHash。
 * @param[in] instructionId 目标CALL/INVOKE的调用方指定一基instruction ID；本函数不查表。
 * @param[in] segmentIndex 调用方指定的来源segment关联。
 * @param[in] moduleHash 写入contract.moduleSignatureHash的值。
 * @param[in] sourceId 写入行sourceId的定位值。
 *
 * ## Returns
 * @return 按值返回SZrExecIrBindingRow；所有未显式设置字段保持零，不返回资源或错误状态。
 *
 * ## Constraints and lifecycle
 * 这是无分配的按值fixture，不验证行索引、instruction关联、metadata arithmetic范围或moduleHash合法性。当前调用只用有限rowIndex0/1等已构造场景；返回值没有堆所有权。将行交给FunctionSetBindingRows后由该API创建独立副本，本函数不转移调用者数组。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:201
 * @see tests/parser/test_ssa_core_model.c:207
 * @see tests/parser/test_ssa_core_model.c:211
 * @see tests/parser/test_ssa_core_model.c:223
 * @see tests/parser/test_ssa_core_model.c:249
 * @see tests/parser/test_ssa_core_model.c:250
 * @see tests/parser/test_ssa_core_model.c:489
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:135
 */

/**
 * @fn void test_binding_rows_are_owned_typed_and_transactional(void)
 *
 * ## Summary
 * 验证typed binding row安装和一基引用/查询、按值副本及clone行独立性；逐次制造越界instruction、重复segment、opcode/module/反向引用及存储形状错误，检查有限拒绝、旧表保留与恢复，再验证typed空表和预留空表。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * source/clone独立Free；rows/invalidRows是栈数组，SetBindingRows须复制后才能离开调用。手工破坏count/capacity/pointer及opcode后都在下一合法操作或Free前恢复相应字段；reservedRows保存真正拥有的分配。旧表成功替换后旧借用不可继续用。该case只断言列明的指针、引用、hash及诊断字段；不把hash非零当作加密认证/碰撞排除或OOM事务证明。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:226
 * @see tests/parser/test_ssa_core_model.c:252
 * @see tests/parser/test_ssa_core_model.c:261
 * @see tests/parser/test_ssa_core_model.c:267
 * @see tests/parser/test_ssa_core_model.c:269
 * @see tests/parser/test_ssa_core_model.c:272
 * @see tests/parser/test_ssa_core_model.c:278
 * @see tests/parser/test_ssa_core_model.c:286
 * @see tests/parser/test_ssa_core_model.c:297
 * @see tests/parser/test_ssa_core_model.c:305
 * @see tests/parser/test_ssa_core_model.c:311
 * @see tests/parser/test_ssa_core_model.c:318
 * @see tests/parser/test_ssa_core_model.c:322
 * @see tests/parser/test_ssa_core_model.c:325
 * @see tests/parser/test_ssa_core_model.c:335
 * @see tests/parser/test_ssa_core_model.c:350
 * @see tests/parser/test_ssa_core_model.c:365
 * @see tests/parser/test_ssa_core_model.c:379
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:66
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:74
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:103
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:147
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:183
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:237
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:243
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:256
 */

/**
 * @fn void test_binding_rows_pass_complete_verifier_gate(void)
 *
 * ## Summary
 * 构造包含真实constant pool、CONSTANT/CALL/RETURN、两类tagged memory链和typed row的module，检查VerifyFunction(ALL)/VerifyModule正例；仅把CALL.bindingRow置NONE后检查两入口的精确绑定诊断。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 所测负例先经一般function校验中的binding-row门禁，不能给后续SSA/effects失败分支信用。恢复ref后释放module。正例虽调用完整验证入口，仍只是这一有限手工IR，不代表源码编译/运行或任意binding contract。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:383
 * @see tests/parser/test_ssa_core_model.c:411
 * @see tests/parser/test_ssa_core_model.c:439
 * @see tests/parser/test_ssa_core_model.c:469
 * @see tests/parser/test_ssa_core_model.c:481
 * @see tests/parser/test_ssa_core_model.c:489
 * @see tests/parser/test_ssa_core_model.c:493
 * @see tests/parser/test_ssa_core_model.c:496
 * @see tests/parser/test_ssa_core_model.c:499
 * @see tests/parser/test_ssa_core_model.c:500
 * @see tests/parser/test_ssa_core_model.c:510
 * @see tests/parser/test_ssa_core_model.c:519
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:366
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:736
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:183
 */

/**
 * @fn void test_invalid_opcode_and_overflow_fail_before_allocation(void)
 *
 * ## Summary
 * 对operand reserve请求SIZE_MAX并检查CAPACITY_OVERFLOW；随后储存UINT16_MAX未知opcode，再由ValidateModule拒绝并检查未知opcode及有效instruction定位。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 测试名含before_allocation，但本case没有读取allocator Count；它直接证明返回/诊断，分配前拒绝顺序仅由实现静态路径支持。未知opcode允许先被append保存，不能把append成功当作合法指令。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:523
 * @see tests/parser/test_ssa_core_model.c:537
 * @see tests/parser/test_ssa_core_model.c:539
 * @see tests/parser/test_ssa_core_model.c:543
 * @see tests/parser/test_ssa_core_model.c:547
 * @see tests/parser/test_ssa_core_model.c:549
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:804
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:554
 */

/**
 * @fn void test_validation_rejects_null_operand_pool_without_dereference(void)
 *
 * ## Summary
 * 故意令逻辑operandCount/capacity为1而operands为空，append含operand的COPY后检查ValidateModule以INVALID_ARGUMENT拒绝。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 源池为空是有意构造的畸形输入；本case不读取operands[0]。一般function入口在指令operand值扫描前检查nonnull条件；不把本次拒绝扩成任意伪指针内存安全证明。FreeModule可以释放NULL池。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:555
 * @see tests/parser/test_ssa_core_model.c:578
 * @see tests/parser/test_ssa_core_model.c:580
 * @see tests/parser/test_ssa_core_model.c:582
 * @see tests/parser/test_ssa_core_model.c:584
 * @see tests/parser/test_ssa_core_model.c:586
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:343
 */

/**
 * @fn void test_failed_module_clone_reclaims_partially_copied_function(void)
 *
 * ## Summary
 * 构造两个源函数及一个已发布目标函数，再令第二源函数声称有instruction而无池，检查CloneModule拒绝、诊断归属82且目标functionCount/token73保留。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 当前实现对每个源函数先检查全部池shape：第二函数在instruction shape门禁失败，早于它自己的value池复制。原614行描述的第二函数分配时序不再成立。此前候选函数及预先初始化的当前槽由CloneModule统一回滚；本case直接断言拒绝/定位与目标有限字段，不直接计数每次临时释放。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:589
 * @see tests/parser/test_ssa_core_model.c:603
 * @see tests/parser/test_ssa_core_model.c:610
 * @see tests/parser/test_ssa_core_model.c:614
 * @see tests/parser/test_ssa_core_model.c:615
 * @see tests/parser/test_ssa_core_model.c:617
 * @see tests/parser/test_ssa_core_model.c:619
 * @see tests/parser/test_ssa_core_model.c:622
 * @see tests/parser/test_ssa_core_model.c:625
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:279
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:308
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:351
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:378
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1136
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1138
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1140
 */

/**
 * @fn void test_module_clone_initializes_early_rejected_function_slot(void)
 *
 * ## Summary
 * 令第二个源函数valueCount2超出valueCapacity1，检查CloneModule早期拒绝时仍给出该源functionToken182，并保留目标functionCount1/token173。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 用于候选module在当前function尚未复制时的回滚槽初始化路径；直接断言只覆盖返回/诊断和目标有限字段，不能仅由测试名证明无所有未初始化读取或完整内存回收。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:629
 * @see tests/parser/test_ssa_core_model.c:646
 * @see tests/parser/test_ssa_core_model.c:649
 * @see tests/parser/test_ssa_core_model.c:651
 * @see tests/parser/test_ssa_core_model.c:657
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:279
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:307
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1136
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:1140
 */

/**
 * @fn void test_structure_requires_reciprocal_cfg_edges(void)
 *
 * ## Summary
 * 构造entry到target的BRANCH，对双向邻接、缺失反向边、未配对重复predecessor/successor、配对平行边及terminator与block successor计数偏差逐次验证。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 每次突变按源码恢复/继续构造，最后把分支改为CONDITIONAL_BRANCH才测试两条平行边正例。只请求STRUCTURE，条件value未证明SSA定义；不可据此授SSA/effects或运行信用。未配对重复边的occurrence匹配有别于简单集合成员检查。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:661
 * @see tests/parser/test_ssa_core_model.c:690
 * @see tests/parser/test_ssa_core_model.c:694
 * @see tests/parser/test_ssa_core_model.c:698
 * @see tests/parser/test_ssa_core_model.c:703
 * @see tests/parser/test_ssa_core_model.c:709
 * @see tests/parser/test_ssa_core_model.c:716
 * @see tests/parser/test_ssa_core_model.c:727
 * @see tests/parser/test_ssa_core_model.c:737
 * @see tests/parser/test_ssa_core_model.c:747
 * @see tests/parser/test_ssa_core_model.c:751
 * @see tests/parser/test_ssa_core_model.c:754
 * @see tests/parser/test_ssa_core_model.c:757
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:782
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:810
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:861
 */

/**
 * @fn void test_structure_rejects_zero_value_phi_incoming(void)
 *
 * ## Summary
 * 构造普通value PHI，其incoming predecessor有效而value为INVALID，调用STRUCTURE入口并核对INVALID_VALUE/actualINVALID。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 一般function验证的PHI incoming扫描在可选STRUCTURE/SSA阶段之前执行，因此此case直接覆盖公共入口的早期拒绝，不覆盖PHI predecessor顺序、dominance或SSA definition。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:764
 * @see tests/parser/test_ssa_core_model.c:766
 * @see tests/parser/test_ssa_core_model.c:779
 * @see tests/parser/test_ssa_core_model.c:784
 * @see tests/parser/test_ssa_core_model.c:787
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:432
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:438
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:736
 */

/**
 * @fn void test_structure_rejects_early_terminator(void)
 *
 * ## Summary
 * 先验证单RETURN块，再追加第二RETURN并把块末terminator指向它，检查块内较早RETURN被报告MISSING_TERMINATOR以及精确前后instruction信息。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 两个RETURN均由实际append加入，第二次验证只请求STRUCTURE；诊断命中“块末之前已出现terminator”循环，不是最终terminator本身不存在。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:793
 * @see tests/parser/test_ssa_core_model.c:809
 * @see tests/parser/test_ssa_core_model.c:812
 * @see tests/parser/test_ssa_core_model.c:816
 * @see tests/parser/test_ssa_core_model.c:818
 * @see tests/parser/test_ssa_core_model.c:822
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:830
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:834
 */

/**
 * @fn void test_clone_rejects_side_array_count_beyond_capacity(void)
 *
 * ## Summary
 * 顺序构造values、gcMap.entries和frameLayout.slots的count2/capacity1畸形shape，检查CloneFunction拒绝且目标id/token保留；首轮还检查诊断位置为INVALID。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无C返回值；到达末尾表示本次已执行的检查均通过。检查失败由本TU断言助手终止整个测试进程，不通过返回码或Unity longjmp报告。
 *
 * ## Constraints and lifecycle
 * 实际分配为两个元素但故意公布容量1，避免用越界读取制造失败；失败后按各阶段Free/Init或手动释放恢复。只断言三个具体shape与有限目标字段，不把destination整体字节或所有池别名视为已测。本TU使用普通C调用；expect_true/check失败会输出消息并exit(EXIT_FAILURE)，终止当前可执行进程，未到达的局部Free和后续case不会执行。正常路径释放本case持有的Function/Module；不把测试失败下的进程退出当成局部资源回收证明。由本文件main串行调用；局部记录与其池只在case内使用，没有并发或线程安全验证。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:825
 * @see tests/parser/test_ssa_core_model.c:834
 * @see tests/parser/test_ssa_core_model.c:843
 * @see tests/parser/test_ssa_core_model.c:845
 * @see tests/parser/test_ssa_core_model.c:856
 * @see tests/parser/test_ssa_core_model.c:860
 * @see tests/parser/test_ssa_core_model.c:868
 * @see tests/parser/test_ssa_core_model.c:878
 * @see tests/parser/test_ssa_core_model.c:882
 * @see tests/parser/test_ssa_core_model.c:892
 * @see tests/parser/test_ssa_core_model.c:898
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:279
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:307
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:325
 * @see zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:330
 */

/**
 * @anchor zr_exec_ir_core_model_clone_fault_cases_include
 *
 * ## Summary
 * 在已定义本文件expect_true/make_owned_binding_row之后、main之前有条件文本包含现有clone fault场景，形成同一TU内的helper与case调用闭包。
 *
 * ## Constraints and lifecycle
 * 条件由构建的ZR_EXEC_IR_TEST_ALLOCATOR提供；force-include allocator H重定向本目标C TUs的malloc/calloc/realloc/free，support C自己先undef使用宿主分配。包含.inc中的物理函数/宏/类型属于依赖，不能因编译实例或本块重复计为本文件定义。没有运行期资源或独立回调；main的selector和默认路径决定实际case可达性。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:902
 * @see tests/parser/test_ssa_core_model.c:903
 * @see tests/parser/test_ssa_core_model.c:909
 * @see tests/parser/test_ssa_core_model.c:947
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:1
 * @see tests/parser/exec_ir_clone_test_allocator_cases.inc:135
 * @see tests/cmake/ssa-tests.cmake:86
 * @see tests/cmake/ssa-tests.cmake:90
 * @see tests/cmake/ssa-tests.cmake:93
 */

/**
 * @fn int main(int argc, char **argv)
 *
 * ## Summary
 * 作为Core model可执行入口，按编译flag和可选单一selector分派现有测试；默认顺序运行14个本文件case，fault模式再运行3个.inc case并检查累计liveCount为零。
 *
 * ## Parameters
 * @param[in] argc C运行时传入的参数数目，selector仅接受恰好一个额外参数。
 * @param[in] argv 在main调用期间有效的参数字符串数组；只读，不接管或修改。
 *
 * ## Returns
 * @return 正常完成所选路径返回EXIT_SUCCESS；expect_true或依赖检查失败可直接exit(EXIT_FAILURE)，没有保证返回main。
 *
 * ## Constraints and lifecycle
 * argc/argv由C运行时提供。仅argc==2且字符串精确匹配时运行一个指定故障/容量case并提前成功返回；未识别参数仍走默认序列。无flag时不编入4声明和.inc，参数显式忽略。inc定义仅是依赖，不在本批重复授信；默认序列未调用两个byte-overflow selector用例。无Unity调度。任一fatal检查退出进程并跳过余下case；此返回成功只限实际走完的路径。全局allocator计数器无锁，测试入口要求串行。
 *
 * ## References
 * @see tests/parser/test_ssa_core_model.c:906
 * @see tests/parser/test_ssa_core_model.c:907
 * @see tests/parser/test_ssa_core_model.c:908
 * @see tests/parser/test_ssa_core_model.c:912
 * @see tests/parser/test_ssa_core_model.c:916
 * @see tests/parser/test_ssa_core_model.c:920
 * @see tests/parser/test_ssa_core_model.c:924
 * @see tests/parser/test_ssa_core_model.c:932
 * @see tests/parser/test_ssa_core_model.c:945
 * @see tests/parser/test_ssa_core_model.c:946
 * @see tests/parser/test_ssa_core_model.c:950
 * @see tests/parser/test_ssa_core_model.c:953
 * @see tests/cmake/ssa-tests.cmake:61
 * @see tests/cmake/ssa-tests.cmake:86
 * @see tests/cmake/ssa-tests.cmake:95
 */
