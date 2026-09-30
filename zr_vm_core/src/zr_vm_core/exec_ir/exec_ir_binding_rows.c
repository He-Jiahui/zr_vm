#include "zr_vm_core/exec_ir.h"
#include "../call_binding_contract_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define ZR_EXEC_IR_BINDING_ROWS_HASH_OFFSET UINT64_C(14695981039346656037)
#define ZR_EXEC_IR_BINDING_ROWS_HASH_PRIME UINT64_C(1099511628211)

static void binding_rows_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                    EZrExecutionDiagnosticCode code,
                                    const SZrExecIrFunction *function,
                                    TZrExecIrInstructionId instructionId,
                                    TZrUInt32 expected,
                                    TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    if (function != ZR_NULL && function->instructions != ZR_NULL &&
        instructionId != 0u && instructionId <= function->instructionCount) {
        diagnostic->sourceId = function->instructions[instructionId - 1u].sourceId;
    }
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrBool binding_row_location_valid(const SZrExecIrBindingRow *row) {
    if (row == ZR_NULL ||
        row->location.kind > ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
        row->location.ownerDepth == ZR_CALL_BINDING_SLOT_NONE ||
        row->location.flags != 0u) {
        return ZR_FALSE;
    }
    if (row->location.kind == ZR_CALL_BINDING_RELOCATION_NONE) {
        if (row->location.targetIndex != ZR_CALL_BINDING_SLOT_NONE) return ZR_FALSE;
    } else if (row->location.targetIndex == ZR_CALL_BINDING_SLOT_NONE) {
        return ZR_FALSE;
    }
    if (row->contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
        (row->location.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
         row->location.targetIndex != ZR_CALL_BINDING_SLOT_NONE ||
         row->location.ownerDepth != 0u)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool binding_rows_entries_valid(
        const SZrExecIrFunction *function,
        const SZrExecIrBindingRow *rows,
        TZrUInt32 rowCount,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (rowCount >= UINT32_MAX || (rowCount != 0u && rows == ZR_NULL)) {
        binding_rows_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                function, 0u, UINT32_MAX - 1u, rowCount);
        return ZR_FALSE;
    }
    for (index = 0u; index < rowCount; ++index) {
        const SZrExecIrBindingRow *row = &rows[index];
        const SZrExecIrInstruction *instruction;
        TZrUInt32 prior;
        if (row->rowIndex != index || row->instructionId == 0u ||
            row->instructionId > function->instructionCount) {
            binding_rows_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                    function, row->instructionId, index,
                                    row->rowIndex);
            return ZR_FALSE;
        }
        instruction = &function->instructions[row->instructionId - 1u];
        if (instruction->opcode != ZR_EXEC_IR_OPCODE_CALL &&
            instruction->opcode != ZR_EXEC_IR_OPCODE_INVOKE) {
            EZrExecutionDiagnosticCode code =
                    ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode) ==
                                    ZR_NULL
                            ? ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE
                            : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
            binding_rows_diagnostic(diagnostic, code,
                                    function, row->instructionId,
                                    ZR_EXEC_IR_OPCODE_CALL, instruction->opcode);
            return ZR_FALSE;
        }
        if (zr_core_call_binding_check_contract(&row->contract, ZR_NULL) !=
                ZR_CALL_BINDING_OK ||
            !binding_row_location_valid(row) ||
            row->contract.moduleSignatureHash != function->contract.moduleHash) {
            binding_rows_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    function, row->instructionId, 1u, 0u);
            return ZR_FALSE;
        }
        for (prior = 0u; prior < index; ++prior) {
            if (rows[prior].instructionId == row->instructionId) {
                binding_rows_diagnostic(diagnostic,
                                        ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                        function, row->instructionId, 0u,
                                        index + 1u);
                return ZR_FALSE;
            }
            if (row->segmentIndex != ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE &&
                rows[prior].segmentIndex == row->segmentIndex) {
                binding_rows_diagnostic(diagnostic,
                                        ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                        function, row->instructionId, prior,
                                        index);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_FunctionValidateBindingRows(
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL) {
        binding_rows_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                ZR_NULL, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (function->bindingRowsSchemaVersion ==
        ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY) {
        if (function->bindingRows != ZR_NULL || function->bindingRowCount != 0u ||
            function->bindingRowCapacity != 0u) {
            binding_rows_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    function, 0u, 0u,
                                    function->bindingRowCount);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (function->bindingRowsSchemaVersion !=
        ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED) {
        binding_rows_diagnostic(diagnostic,
                                ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                                function, 0u,
                                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED,
                                function->bindingRowsSchemaVersion);
        return ZR_FALSE;
    }
    if (function->bindingRowCount > function->bindingRowCapacity ||
        function->bindingRowCount >= UINT32_MAX ||
        (function->bindingRowCapacity != 0u &&
         function->bindingRows == ZR_NULL) ||
        (function->bindingRows != ZR_NULL &&
         function->bindingRowCapacity == 0u) ||
        (function->bindingRowCount != 0u && function->bindingRows == ZR_NULL) ||
        function->instructionCount > function->instructionCapacity ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL)) {
        binding_rows_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                function, 0u,
                                function->bindingRowCount,
                                function->bindingRowCapacity);
        return ZR_FALSE;
    }
    if (!binding_rows_entries_valid(function, function->bindingRows,
                                    function->bindingRowCount, diagnostic)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 rowReference = instruction->bindingRow;
        if (rowReference == ZR_EXEC_IR_BINDING_ROW_REF_NONE) continue;
        if (rowReference > function->bindingRowCount ||
            function->bindingRows[rowReference - 1u].instructionId != index + 1u ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_CALL &&
             instruction->opcode != ZR_EXEC_IR_OPCODE_INVOKE)) {
            binding_rows_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    function, index + 1u,
                                    function->bindingRowCount, rowReference);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->bindingRowCount; ++index) {
        const SZrExecIrBindingRow *row = &function->bindingRows[index];
        if (function->instructions[row->instructionId - 1u].bindingRow !=
            index + 1u) {
            binding_rows_diagnostic(diagnostic,
                                    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    function, row->instructionId, index + 1u,
                                    function->instructions[row->instructionId - 1u]
                                            .bindingRow);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_FunctionSetBindingRows(
        SZrExecIrFunction *function,
        const SZrExecIrBindingRow *rows,
        TZrUInt32 rowCount,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrBindingRow *copy = ZR_NULL;
    SZrExecIrBindingRow *oldRows;
    size_t bytes;
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL || function->sealed ||
        function->instructionCount > function->instructionCapacity ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL)) {
        binding_rows_diagnostic(
                diagnostic,
                function != ZR_NULL && function->sealed
                    ? ZR_EXEC_IR_DIAGNOSTIC_SEALED
                    : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                function, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_FunctionValidateBindingRows(function, diagnostic)) {
        return ZR_FALSE;
    }
    if (rowCount >= UINT32_MAX || (rowCount != 0u && rows == ZR_NULL) ||
        (size_t)rowCount > SIZE_MAX / sizeof(*copy)) {
        binding_rows_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                function, 0u, UINT32_MAX - 1u, rowCount);
        return ZR_FALSE;
    }
    if (rowCount != 0u) {
        bytes = (size_t)rowCount * sizeof(*copy);
        copy = (SZrExecIrBindingRow *)malloc(bytes);
        if (copy == ZR_NULL) {
            binding_rows_diagnostic(diagnostic,
                                    ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                    function, 0u, (TZrUInt32)bytes, 0u);
            return ZR_FALSE;
        }
        memcpy(copy, rows, bytes);
    }
    if (!binding_rows_entries_valid(function, copy, rowCount, diagnostic)) {
        free(copy);
        return ZR_FALSE;
    }

    oldRows = function->bindingRows;
    function->bindingRowsSchemaVersion = ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED;
    function->bindingRows = copy;
    function->bindingRowCount = rowCount;
    function->bindingRowCapacity = rowCount;
    for (index = 0u; index < function->instructionCount; ++index)
        function->instructions[index].bindingRow = ZR_EXEC_IR_BINDING_ROW_REF_NONE;
    for (index = 0u; index < rowCount; ++index) {
        function->instructions[copy[index].instructionId - 1u].bindingRow = index + 1u;
    }
    free(oldRows);
    return ZR_TRUE;
}

const SZrExecIrBindingRow *ZrCore_ExecIr_FunctionBindingRowAt(
        const SZrExecIrFunction *function, TZrUInt32 rowReference) {
    if (function == ZR_NULL ||
        function->bindingRowsSchemaVersion !=
                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED ||
        function->bindingRowCount > function->bindingRowCapacity ||
        function->bindingRowCount >= UINT32_MAX ||
        (function->bindingRowCapacity != 0u &&
         function->bindingRows == ZR_NULL) ||
        (function->bindingRows != ZR_NULL &&
         function->bindingRowCapacity == 0u) ||
        rowReference == ZR_EXEC_IR_BINDING_ROW_REF_NONE ||
        rowReference > function->bindingRowCount) {
        return ZR_NULL;
    }
    return &function->bindingRows[rowReference - 1u];
}

static TZrUInt64 binding_rows_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    TZrUInt32 byteIndex;
    for (byteIndex = 0u; byteIndex < 4u; ++byteIndex) {
        hash = (hash ^ (TZrUInt8)(value >> (byteIndex * 8u))) *
               ZR_EXEC_IR_BINDING_ROWS_HASH_PRIME;
    }
    return hash;
}

static TZrUInt64 binding_rows_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    TZrUInt32 byteIndex;
    for (byteIndex = 0u; byteIndex < 8u; ++byteIndex) {
        hash = (hash ^ (TZrUInt8)(value >> (byteIndex * 8u))) *
               ZR_EXEC_IR_BINDING_ROWS_HASH_PRIME;
    }
    return hash;
}

TZrUInt64 ZrCore_ExecIr_FunctionBindingRowsHash(
        const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZR_EXEC_IR_BINDING_ROWS_HASH_OFFSET;
    TZrUInt32 index;
    if (function == ZR_NULL ||
        function->bindingRowsSchemaVersion ==
                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY) {
        return 0u;
    }
    if (!ZrCore_ExecIr_FunctionValidateBindingRows(function, ZR_NULL)) return 0u;
    hash = binding_rows_hash_u32(hash, function->bindingRowsSchemaVersion);
    hash = binding_rows_hash_u32(hash, function->bindingRowCount);
    for (index = 0u; index < function->bindingRowCount; ++index) {
        const SZrExecIrBindingRow *row = &function->bindingRows[index];
        const SZrCallBindingContract *contract = &row->contract;
        hash = binding_rows_hash_u32(hash, row->rowIndex);
        hash = binding_rows_hash_u32(hash, row->instructionId);
        hash = binding_rows_hash_u32(hash, row->segmentIndex);
        hash = binding_rows_hash_u32(hash, contract->bindingKind);
        hash = binding_rows_hash_u32(hash, contract->targetMetadataToken);
        hash = binding_rows_hash_u32(hash, contract->signatureToken);
        hash = binding_rows_hash_u32(hash, contract->ownerTypeToken);
        hash = binding_rows_hash_u64(hash, contract->signatureHash);
        hash = binding_rows_hash_u64(hash, contract->moduleSignatureHash);
        hash = binding_rows_hash_u32(hash, contract->layoutVersion);
        hash = binding_rows_hash_u32(hash, contract->dispatchSlot);
        hash = binding_rows_hash_u64(hash, contract->layoutHash);
        hash = binding_rows_hash_u32(hash, contract->operation);
        hash = binding_rows_hash_u32(hash, contract->reserved0);
        hash = binding_rows_hash_u64(hash, contract->reserved1);
        hash = binding_rows_hash_u32(hash, row->location.kind);
        hash = binding_rows_hash_u32(hash, row->location.targetIndex);
        hash = binding_rows_hash_u32(hash, row->location.ownerDepth);
        hash = binding_rows_hash_u32(hash, row->location.flags);
        hash = binding_rows_hash_u32(hash, row->sourceId);
    }
    return hash != 0u ? hash : 1u;
}
