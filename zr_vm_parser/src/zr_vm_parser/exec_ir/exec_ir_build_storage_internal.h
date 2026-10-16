#ifndef ZR_PARSER_EXEC_IR_BUILD_STORAGE_INTERNAL_H
#define ZR_PARSER_EXEC_IR_BUILD_STORAGE_INTERNAL_H

#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Borrowed diagnostic coordinates. sourceId names a semantic row; an ExecIR
 * instructionId is supplied only after that actual instruction is published. */
typedef struct SExecIrBuildStorageSite {
    SZrExecIrDiagnostic *diagnostic;
    const SZrExecIrFunction *function;
    TZrUInt32 blockId, instructionId, sourceId;
} SExecIrBuildStorageSite;

static TZrBool build_storage_fail(const SExecIrBuildStorageSite *site,
                                  EZrExecutionDiagnosticCode code) {
    if (site->diagnostic != ZR_NULL) {
        memset(site->diagnostic, 0, sizeof(*site->diagnostic));
        site->diagnostic->code = code;
        site->diagnostic->functionToken = site->function != ZR_NULL ? site->function->functionToken : 0u;
        site->diagnostic->blockId = site->blockId;
        site->diagnostic->instructionId = site->instructionId;
        site->diagnostic->sourceId = site->sourceId;
    }
    return ZR_FALSE;
}

static TZrBool build_storage_mutable(SZrExecIrFunction *function,
                                     const SExecIrBuildStorageSite *site) {
    if (function == ZR_NULL)
        return build_storage_fail(site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    if (function->sealed) return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_SEALED);
    return ZR_TRUE;
}

/* Numeric geometry is checked even when no growth is required. It does not
 * certify arbitrary-pointer readability; all storage is the private builder's. */
static TZrBool build_storage_geometry(const SExecIrBuildStorageSite *site,
        const void *storage, TZrUInt32 count, TZrUInt32 capacity, size_t width, size_t alignment) {
    if (width == 0u || alignment == 0u)
        return build_storage_fail(site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    if (count > capacity || (storage != ZR_NULL) != (capacity != 0u) ||
        (uintptr_t)storage % alignment != 0u)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    if ((uintmax_t)capacity > SIZE_MAX / width)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    if ((size_t)capacity * width > UINTPTR_MAX - (uintptr_t)storage)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    return ZR_TRUE;
}

/* Match Core exec_ir.c's private reserve policy exactly: initial capacity 4,
 * doubling, then requested near UINT32_MAX. Only Core performs allocation. */
static TZrBool build_storage_growth(const SExecIrBuildStorageSite *site,
        const void *storage, TZrUInt32 count, TZrUInt32 capacity, TZrSize additional,
        size_t width, size_t alignment, TZrBool *needGrowth) {
    TZrSize requested, grown;
    if (needGrowth == ZR_NULL)
        return build_storage_fail(site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    if (!build_storage_geometry(site, storage, count, capacity, width, alignment)) return ZR_FALSE;
    if (additional > (TZrSize)UINT32_MAX - count)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    requested = (TZrSize)count + additional;
    if ((uintmax_t)requested > SIZE_MAX / width)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    grown = requested <= capacity ? capacity : (capacity == 0u ? 4u : capacity);
    while (grown < requested) {
        if (grown > (TZrSize)UINT32_MAX / 2u) { grown = requested; break; }
        grown *= 2u;
    }
    if (grown > UINT32_MAX || (uintmax_t)grown > SIZE_MAX / width)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    *needGrowth = (TZrBool)(requested > capacity);
    return ZR_TRUE;
}

static TZrExecIrValueId build_storage_add_value(SZrExecIrFunction *function,
        TZrMetadataToken type, EZrExecIrOwnership ownership, EZrExecIrNullability nullability,
        TZrBool external, SZrExecIrDiagnostic *diagnostic) {
    const SExecIrBuildStorageSite site = {diagnostic, function, 0u, 0u, 0u};
    TZrBool growth;
    TZrExecIrValueId id;
    if (!build_storage_mutable(function, &site)) return ZR_EXEC_IR_VALUE_ID_INVALID;
    if ((TZrUInt32)ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT ||
        (TZrUInt32)nullability >= ZR_EXEC_IR_NULLABILITY_COUNT) {
        build_storage_fail(&site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
        return ZR_EXEC_IR_VALUE_ID_INVALID;
    }
    if (!build_storage_growth(&site, function->values, function->valueCount, function->valueCapacity,
            1u, sizeof(*function->values), _Alignof(SZrExecIrValue), &growth))
        return ZR_EXEC_IR_VALUE_ID_INVALID;
    id = external ? ZrCore_ExecIr_FunctionAddExternalValue(function, type, ownership, nullability)
                  : ZrCore_ExecIr_FunctionAddValue(function, type, ownership, nullability);
    if (id == ZR_EXEC_IR_VALUE_ID_INVALID)
        build_storage_fail(&site, growth ? ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY
                                        : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    return id;
}

static TZrExecIrBlockId build_storage_add_block(SZrExecIrFunction *function,
        TZrUInt32 flags, SZrExecIrDiagnostic *diagnostic) {
    const SExecIrBuildStorageSite site = {diagnostic, function, 0u, 0u, 0u};
    TZrBool growth;
    TZrExecIrBlockId id;
    if (!build_storage_mutable(function, &site)) return ZR_EXEC_IR_BLOCK_ID_INVALID;
    if ((flags & ~(ZR_EXEC_IR_BLOCK_FLAG_ENTRY | ZR_EXEC_IR_BLOCK_FLAG_COLD |
                   ZR_EXEC_IR_BLOCK_FLAG_CLEANUP | ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION)) != 0u ||
        ((flags & ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 0u &&
         function->entryBlockId != ZR_EXEC_IR_BLOCK_ID_INVALID)) {
        build_storage_fail(&site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    if (!build_storage_growth(&site, function->blocks, function->blockCount, function->blockCapacity,
            1u, sizeof(*function->blocks), _Alignof(SZrExecIrBlock), &growth))
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    id = ZrCore_ExecIr_FunctionAddBlock(function, flags);
    if (id == ZR_EXEC_IR_BLOCK_ID_INVALID)
        build_storage_fail(&site, growth ? ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY
                                        : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    return id;
}

static TZrBool build_storage_append_ids(SZrExecIrFunction *function,
        const TZrExecIrValueId *items, TZrSize count, SZrExecIrRange *range,
        const SExecIrBuildStorageSite *site, TZrBool results) {
    TZrUInt32 used, capacity;
    const TZrExecIrValueId *storage;
    TZrBool reserved, appended;
    if (!build_storage_mutable(function, site)) return ZR_FALSE;
    used = results ? function->resultCount : function->operandCount;
    capacity = results ? function->resultCapacity : function->operandCapacity;
    storage = results ? function->resultPool : function->operandPool;
    if (count != 0u && items == ZR_NULL)
        return build_storage_fail(site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    if (!build_storage_geometry(site, storage, used, capacity,
            sizeof(*storage), _Alignof(TZrExecIrValueId))) return ZR_FALSE;
    if (count > (TZrSize)UINT32_MAX - used)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    /* Ex owns the real growth arithmetic and preserves its failure category. */
    reserved = results ? ZrCore_ExecIr_FunctionReserveResultsEx(function, (TZrSize)used + count, site->diagnostic)
                       : ZrCore_ExecIr_FunctionReserveOperandsEx(function, (TZrSize)used + count, site->diagnostic);
    if (!reserved) {
        if (site->diagnostic != ZR_NULL) {
            site->diagnostic->blockId = site->blockId;
            site->diagnostic->instructionId = site->instructionId;
            site->diagnostic->sourceId = site->sourceId;
        }
        return ZR_FALSE;
    }
    appended = results ? ZrCore_ExecIr_FunctionAppendResults(function, items, count, range)
                       : ZrCore_ExecIr_FunctionAppendOperands(function, items, count, range);
    return appended ? ZR_TRUE : build_storage_fail(site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
}

static TZrBool build_storage_append_results(SZrExecIrFunction *function,
        const TZrExecIrValueId *items, TZrSize count, SZrExecIrRange *range,
        const SExecIrBuildStorageSite *site) {
    return build_storage_append_ids(function, items, count, range, site, ZR_TRUE);
}
static TZrBool build_storage_append_operands(SZrExecIrFunction *function,
        const TZrExecIrValueId *items, TZrSize count, SZrExecIrRange *range,
        const SExecIrBuildStorageSite *site) {
    return build_storage_append_ids(function, items, count, range, site, ZR_FALSE);
}

static TZrBool build_storage_append_instruction(SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction, TZrExecIrInstructionId *outId,
        const SExecIrBuildStorageSite *site) {
    TZrBool growth;
    if (!build_storage_mutable(function, site)) return ZR_FALSE;
    if (instruction == ZR_NULL)
        return build_storage_fail(site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    if (!build_storage_geometry(site, function->resultPool, function->resultCount, function->resultCapacity,
                sizeof(*function->resultPool), _Alignof(TZrExecIrValueId)) ||
        !build_storage_geometry(site, function->values, function->valueCount, function->valueCapacity,
                sizeof(*function->values), _Alignof(SZrExecIrValue))) return ZR_FALSE;
    if (instruction->resultRange.start > function->resultCount ||
        instruction->resultRange.count > function->resultCount - instruction->resultRange.start)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    for (TZrUInt32 i = 0u; i < instruction->resultRange.count; ++i) {
        TZrExecIrValueId id = function->resultPool[instruction->resultRange.start + i];
        if (id == ZR_EXEC_IR_VALUE_ID_INVALID || id > function->valueCount ||
            function->values[id - 1u].definition != ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            (function->values[id - 1u].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u)
            return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    }
    if (!build_storage_growth(site, function->instructions, function->instructionCount, function->instructionCapacity,
            1u, sizeof(*function->instructions), _Alignof(SZrExecIrInstruction), &growth)) return ZR_FALSE;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, instruction, outId))
        return build_storage_fail(site, growth ? ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY
                                              : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    return ZR_TRUE;
}

static TZrBool build_storage_append_source(SZrExecIrFunction *function,
        const SZrSemanticIrInstruction *instruction, const SExecIrBuildStorageSite *site) {
    SZrExecIrSourceMap *map;
    uintmax_t capacity;
    if (!build_storage_mutable(function, site)) return ZR_FALSE;
    if (instruction == ZR_NULL)
        return build_storage_fail(site, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    if (!build_storage_geometry(site, function->sourceMaps, function->sourceMapCount, function->sourceMapCapacity,
            sizeof(*function->sourceMaps), _Alignof(SZrExecIrSourceMap))) return ZR_FALSE;
    if (function->sourceMapCount == UINT32_MAX)
        return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    if (function->sourceMapCount == function->sourceMapCapacity) {
        capacity = function->sourceMapCapacity == 0u ? 8u : (uintmax_t)function->sourceMapCapacity * 2u;
        if (capacity > UINT32_MAX || capacity > SIZE_MAX / sizeof(*map))
            return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
        map = (SZrExecIrSourceMap *)realloc(function->sourceMaps, (size_t)capacity * sizeof(*map));
        if (map == ZR_NULL) return build_storage_fail(site, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
        function->sourceMaps = map;
        function->sourceMapCapacity = (TZrUInt32)capacity;
    }
    map = &function->sourceMaps[function->sourceMapCount++];
    map->sourceId = instruction->id; map->instructionId = site->instructionId;
    map->startOffset = (TZrUInt32)instruction->sourceRange.start.offset;
    map->endOffset = (TZrUInt32)instruction->sourceRange.end.offset;
    map->startLine = (TZrUInt32)instruction->sourceRange.start.line;
    map->startColumn = (TZrUInt32)instruction->sourceRange.start.column;
    map->endLine = (TZrUInt32)instruction->sourceRange.end.line;
    map->endColumn = (TZrUInt32)instruction->sourceRange.end.column;
    return ZR_TRUE;
}
#endif
