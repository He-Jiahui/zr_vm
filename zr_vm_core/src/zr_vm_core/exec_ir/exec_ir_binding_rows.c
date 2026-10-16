#include "zr_vm_core/exec_ir.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "call/call_binding_contract_internal.h"

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

/**
 * @def ZR_EXEC_IR_BINDING_ROWS_HASH_OFFSET
 *
 * ## Summary
 * 为typed binding-row摘要提供固定64位FNV-1a初始状态，使空typed表仍包含schema和逻辑行数的编码。
 *
 * ## Constraints and lifecycle
 * 对象宏只展开为UINT64_C常量，不保存状态、不分配内存，也不读取function。仅作为本翻译单元hash入口的初始值；它不代表哈希唯一性、密码学安全或某份metadata的真实性。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:8
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:294
 */

/**
 * @def ZR_EXEC_IR_BINDING_ROWS_HASH_PRIME
 *
 * ## Summary
 * 为32位和64位字段逐字节FNV-1a更新提供固定64位乘数。
 *
 * ## Constraints and lifecycle
 * 对象宏不求值任何参数，也没有可变全局状态。两个字段helper先异或低位字节再乘此常量；无符号64位回绕属于摘要算法。该常量不赋予摘要无碰撞保证。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:9
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:278
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:287
 */

/**
 * @fn static void binding_rows_diagnostic(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, TZrExecIrInstructionId instructionId, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ## Summary
 * 把binding-row校验或替换的首个失败写入统一ExecIR诊断，并在可定位的指令上补充其sourceId。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空须为独立于function、指令池、行表和其它输入的可写诊断记录，先整体清零。
 * @param[in] code 本次失败类别，原值写入code字段。
 * @param[in] function 可空的借用函数；非空时须可读，functionToken及可定位指令的sourceId用于诊断上下文。
 * @param[in] instructionId 1基指令ID；0或超出instructionCount时不读取指令sourceId。
 * @param[in] expected 本次规则期望值，写入expectedVersion字段，不限定为版本号。
 * @param[in] actual 本次实际值，写入actualVersion字段，不限定为版本号。
 *
 * ## Returns
 * 无C返回值；diagnostic为空时直接返回，否则建立完整失败记录。
 *
 * ## Constraints and lifecycle
 * 只在当前调用内借用function和输入值，不保存参数、不拥有或释放任何资源。只有function及instructions非空、instructionId在1..instructionCount时才取sourceId；这不是分配extent、instructionCapacity或指针可读性验证。调用者须提供稳定可读的记录/池和独立诊断槽，禁止诊断清零覆盖借用输入；并发调用应使用各自诊断。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:11
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:17
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:22
 * - tests/parser/test_ssa_core_model.c:305
 * - tests/parser/test_ssa_core_model.c:311
 */

/**
 * @fn static TZrBool binding_row_location_valid(const SZrExecIrBindingRow *row)
 *
 * ## Summary
 * 检查单条行的重定位位置字段是否满足本层形状规则，并收紧typed-function的无重定位形式。
 *
 * ## Parameters
 * @param[in] row 可空的只读行借用；非空时整个记录须在调用期间稳定可读。
 *
 * ## Returns
 * @return row非空、kind未越过VM_MODULE、ownerDepth不是SLOT_NONE且flags为0，并满足kind/targetIndex和typed-function附加条件时返回TRUE；否则FALSE。
 *
 * ## Constraints and lifecycle
 * NONE重定位必须配targetIndex=SLOT_NONE，其它kind必须配非SLOT_NONE；typed-function还要求NONE、SLOT_NONE和ownerDepth=0。非typed-function的ownerDepth仅排除哨兵，不核对实际owner链；此helper不解析目标metadata、真实模块/类型/签名或地址，也不验证contract其余字段。调用内借用而不保存/修改row，无分配、释放、诊断或同步设施。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:30
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:31
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:42
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:88
 * - zr_vm_core/src/zr_vm_core/call/call_binding_contract_internal.h:45
 */

/**
 * @fn static TZrBool binding_rows_entries_valid( const SZrExecIrFunction *function, const SZrExecIrBindingRow *rows, TZrUInt32 rowCount, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 按数组位置检查候选行、CALL/INVOKE关联、字段合同及行间唯一性，供旧表验证和替换前验证共用。
 *
 * ## Parameters
 * @param[in] function 借用的函数/指令视图；读取行时调用者须已保证非空、instructionCount及实际指令存储可读，本helper不重新验证其完整shape。
 * @param[in] rows rowCount个稳定可读行的借用；rowCount=0时可空且不读取行。
 * @param[in] rowCount 逻辑行数，必须小于UINT32_MAX；0时循环零次并成功，不读取function字段。
 * @param[out] diagnostic 可空、独立可写的诊断槽；失败时由diagnostic helper整体覆盖，成功时不主动清零。
 *
 * ## Returns
 * @return 行数/指针、各行位置/指令/字段合同和两类唯一性均通过时TRUE；任一规则失败时FALSE，并在提供诊断时记录首失败。
 *
 * ## Constraints and lifecycle
 * 要求rowIndex等于当前数组下标、instructionId在1..instructionCount且引用CALL或INVOKE；未知opcode与已知非call opcode采用不同诊断。共享contract检查通过、location形状通过且moduleSignatureHash等于function.contract.moduleHash后，逐一排除重复instructionId及重复非NONE segmentIndex，多个NONE segmentIndex允许共存。不按instructionId或segmentIndex排序，也不要求二者单调；sourceId不要求等于指令sourceId，segmentIndex不作外部segment池边界检查。不检查instruction.bindingRow反向关联，此项由公开validator完成。双层扫描最坏为行数平方，不分配或发布状态；调用内只读借用、不保留参数。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:51
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:57
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:66
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:74
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:86
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:95
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:162
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:237
 * - tests/parser/test_ssa_core_model.c:286
 * - tests/parser/test_ssa_core_model.c:297
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionValidateBindingRows( const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 验证函数的legacy/typed行表形状及typed模式下行与指令的双向1基关联，作为复制、验证、投影和摘要的共享门禁。
 *
 * ## Parameters
 * @param[in] function 可空；非空时为调用期间稳定可读的函数及被访问的行/指令池借用，实际存储extent由调用者保证。
 * @param[out] diagnostic 可空；非空须为不与function或其池重叠的独立可写诊断槽，入口整体清零。
 *
 * ## Returns
 * @return legacy且行指针/count/capacity全零，或typed表形状、逐行规则和全部双向关联均通过时TRUE；空函数、未知schema及实际检测到的形状/关联错误返回FALSE。
 *
 * ## Constraints and lifecycle
 * legacy分支在行字段全零时直接成功，不校验指令数组shape或legacy bindingRow编码。typed分支检查count<=capacity、count<UINT32_MAX、行pointer/capacity对应关系、instructionCount<=instructionCapacity以及非空指令池；允许count0/capacity>0且有存储的预留空表。随后按数组顺序校验每行，再检查每个非零instruction.bindingRow指向正确行，最后要求每行对应指令回指index+1；CALL/INVOKE可以没有行事实，但已有行不能失去反向引用。本入口不做完整CFG/SSA/effect、实际metadata目标解析、分配来源/extent或任意alias检查；不修改IR、不分配/释放/缓存输入。成功诊断保持清零，失败只反映实际首规则。借用池不得在调用中被并发修改、替换或释放。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:116
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:564
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:120
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:126
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:147
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:166
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:181
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:352
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:366
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c:78
 * - tests/parser/test_ssa_core_model.c:305
 * - tests/parser/test_ssa_core_model.c:369
 */

/**
 * @fn TZrBool ZrCore_ExecIr_FunctionSetBindingRows( SZrExecIrFunction *function, const SZrExecIrBindingRow *rows, TZrUInt32 rowCount, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 复制并验证借用候选表，成功后替换函数拥有的typed行表并重建全部指令bindingRow引用。
 *
 * ## Parameters
 * @param[in,out] function 非空、未sealed且原行表已通过本层validator的目标；旧表须属于可由本编译实例free释放的正常拥有存储，指令池须足量可读写。
 * @param[in] rows rowCount条候选行的调用期间只读借用；count0时可空且不读取。可以来自旧表的有效可读区间，复制发生于旧表释放之前；不得与diagnostic重叠。
 * @param[in] rowCount 新表逻辑行数，须小于UINT32_MAX且乘sizeof(row)不超SIZE_MAX；每行rowIndex须已等于其数组位置。
 * @param[out] diagnostic 可空；非空为独立于function、旧池、候选rows及指令池的可写诊断槽，入口及下层validator会同步清零/写入。
 *
 * ## Returns
 * @return 候选复制与逐行检查成功、typed表已发布且指令引用全部重建时TRUE；空/已封目标、原表无效、候选形状/字段错误或分配失败时FALSE。
 *
 * ## Constraints and lifecycle
 * 先检查目标/指令shape，再验证原表，因此不能用本接口修复本已损坏的旧表。非空候选经malloc+memcpy建立私有副本后验证；失败释放该副本而不发布，在独立诊断且正常不重叠拥有布局前提下保留旧表、schema/count/capacity及指令引用。成功把schema设为TYPED，pointer/count/capacity设为copy/rowCount/rowCount，先清全部指令引用再按候选写index+1，最后free旧表；count0也执行typed空表替换与引用清零，但不分配。普通rows借用不会被接管，旧表借用在成功free时失效。记录与instructions/旧表须有互不覆盖的合法拥有布局；本函数不验证任意别名、实际extent、可写权限或allocator来源，也不承诺重叠诊断下的保持原值。不排序、不补全metadata、不递增generation或自动刷新下游缓存；写目标及释放旧表须独占，无内部锁或并发原子发布保证。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:196
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:561
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:205
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:217
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:220
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:228
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:237
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:242
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:252
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_binding_facts.c:939
 * - zr_vm_core/src/zr_vm_core/artifact/scalar/artifact_exec_ir_scalar_eis6_read.c:316
 * - tests/parser/test_ssa_core_model.c:252
 * - tests/parser/test_ssa_core_model.c:286
 * - tests/parser/test_ssa_core_model.c:350
 */

/**
 * @fn const SZrExecIrBindingRow *ZrCore_ExecIr_FunctionBindingRowAt( const SZrExecIrFunction *function, TZrUInt32 rowReference)
 *
 * ## Summary
 * 在typed行表的轻量shape和引用范围检查后，把1基rowReference投影为借用行地址。
 *
 * ## Parameters
 * @param[in] function 可空；非空为稳定可读的函数记录，返回非空时对应行表的真实存储须有效。
 * @param[in] rowReference 1..bindingRowCount的引用；REF_NONE即0以及越界引用返回空。
 *
 * ## Returns
 * @return typed schema、行pointer/count/capacity形状及引用范围均通过时返回bindingRows[rowReference-1]的只读借用地址；否则ZR_NULL。
 *
 * ## Constraints and lifecycle
 * 只做访问所需的轻量检查，不调用完整validator，不读取instructions，也不验证rowIndex、CALL/INVOKE、contract、location或双向引用；因此非空结果不代表行语义已验证。函数不分配/释放/保存参数或取得所有权。返回地址仅在同一行表实际存活且未被替换、重分配或释放期间有效；内容修改也要求调用者重新确认其语义，不能依靠const返回类型获得快照或并发保护。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:256
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:566
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:258
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:271
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_target.c:156
 * - tests/parser/test_ssa_core_model.c:261
 * - tests/parser/test_ssa_core_model.c:336
 * - tests/parser/test_ssa_core_model.c:340
 * - tests/parser/test_ssa_core_model.c:344
 */

/**
 * @fn static TZrUInt64 binding_rows_hash_u32(TZrUInt64 hash, TZrUInt32 value)
 *
 * ## Summary
 * 把一个32位字段按低字节到高字节顺序折入64位FNV-1a状态，避免依赖宿主内存字节序或结构padding。
 *
 * ## Parameters
 * @param[in] hash 调用方传入的64位累计状态，按值传递。
 * @param[in] value 待编码的32位无符号字段，按值传递。
 *
 * ## Returns
 * @return 依次处理4个字节后的64位无符号摘要状态；允许自然回绕，也可能为0。
 *
 * ## Constraints and lifecycle
 * 无指针参数、分配、释放、诊断、输入保存或可变全局状态。每字节先异或再乘固定prime；该helper本身不做最终非零归一化，不证明字段有效或摘要无碰撞，可在无共享可变输入时独立使用。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:274
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:276
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:307
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:326
 * - tests/parser/test_ssa_core_model.c:280
 */

/**
 * @fn static TZrUInt64 binding_rows_hash_u64(TZrUInt64 hash, TZrUInt64 value)
 *
 * ## Summary
 * 把一个64位字段按低字节到高字节顺序折入64位FNV-1a状态，供签名与layout哈希字段共用。
 *
 * ## Parameters
 * @param[in] hash 调用方传入的64位累计状态，按值传递。
 * @param[in] value 待编码的64位无符号字段，按值传递。
 *
 * ## Returns
 * @return 依次处理8个字节后的64位无符号摘要状态；允许自然回绕，也可能为0。
 *
 * ## Constraints and lifecycle
 * 只对值运算，不验证签名或layout身份，不分配、释放、记录诊断或保存状态。字节编码与宿主端序无关；helper不负责最终非零归一化，不提供密码学或无碰撞保证。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:283
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:285
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:314
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:315
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:318
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:321
 */

/**
 * @fn TZrUInt64 ZrCore_ExecIr_FunctionBindingRowsHash( const SZrExecIrFunction *function)
 *
 * ## Summary
 * 对合法typed行表的schema、逻辑行数及按数组顺序编码的全部行字段生成确定性非零摘要，供分析和缓存身份比对使用。
 *
 * ## Parameters
 * @param[in] function 可空的只读函数借用；typed校验会访问行表与指令池，调用期间须保持真实可读且稳定。
 *
 * ## Returns
 * @return NULL、legacy模式或validator拒绝的函数返回0；合法typed表返回逐字段FNV-1a结果，并把最终0映射为1，所以typed空表也返回非零。
 *
 * ## Constraints and lifecycle
 * legacy分支先于validator直接返回0，不能用0区分合法legacy、损坏legacy与其它被拒输入。typed模式先运行完整binding-row validator，但这仍不是完整VerifyFunction或真实metadata解析。字段流含schema/count、每行rowIndex/instructionId/segmentIndex、contract的各数值字段、location各字段及row.sourceId；不散列capacity、指针、结构padding、未使用容量字节或函数generation。顺序与字段值改变会改变编码流，但摘要可能碰撞，不能替代完整相等检查；最终0→1映射也不保留唯一性。只在当前调用内借用输入，不分配、释放、保存指针或写诊断；调用者负责同步并在更改行/关联后重新计算。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:292
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:570
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:296
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:301
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:304
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c:328
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c:84
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c:140
 * - tests/parser/test_ssa_core_model.c:269
 * - tests/parser/test_ssa_core_model.c:280
 * - tests/parser/test_ssa_core_model.c:359
 * - tests/parser/test_ssa_binding_rows_artifact.c:629
 */
