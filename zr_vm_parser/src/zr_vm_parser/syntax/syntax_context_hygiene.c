#include "syntax/syntax_context_internal.h"

#include <stdint.h>
#include <string.h>

#include "zr_vm_core/memory.h"

static TZrSize hygiene_next_capacity(
        TZrSize current, TZrSize required, TZrSize initial) {
    TZrSize capacity = current == 0u ? initial : current;

    while (capacity < required) {
        if (capacity > ZR_MAX_SIZE / 2u) {
            return required;
        }
        capacity *= 2u;
    }
    return capacity;
}

static TZrBool hygiene_capacity_bytes(
        TZrSize current,
        TZrSize required,
        TZrSize initial,
        TZrSize elementSize,
        TZrSize *outBytes) {
    TZrSize capacity;

    if (outBytes == ZR_NULL) {
        return ZR_FALSE;
    }
    *outBytes = 0u;
    if (required <= current) {
        return ZR_TRUE;
    }
    capacity = hygiene_next_capacity(current, required, initial);
    return ZrParser_SyntaxContext_SizeMul(capacity, elementSize, outBytes);
}

static TZrBool hygiene_mark_is_valid(
        const SZrSyntaxContextTable *table,
        TZrUInt32 markId) {
    return table != ZR_NULL && markId != 0u &&
           markId != ZR_SYNTAX_CONTEXT_INVALID_MARK_ID &&
           (TZrSize)markId <= table->markCount;
}

static SZrSyntaxContext hygiene_append_mark(
        SZrSyntaxContextTable *table,
        SZrSyntaxContext parent,
        TZrUInt32 markerId,
        EZrSyntaxTransparency transparency) {
    SZrSyntaxContext invalid = ZrParser_SyntaxContext_Invalid(table);
    TZrSize required;
    TZrSize capacityBytes;
    TZrSize heapBytes;
    TZrSize previousCount;
    SZrSyntaxMarkRecord *originalMarks;
    TZrSize originalMarkCapacity;
    SZrSyntaxExpansionRecord *originalExpansions;
    TZrSize originalExpansionCapacity;
    SZrParserComptimeBudgetTransaction localTransaction;
    SZrParserComptimeBudgetTransaction *transaction;
    TZrBool ownsTransaction;

    if (table == ZR_NULL || !ZrParser_SyntaxContext_IsValid(parent) ||
        parent.table != table || markerId == 0u ||
        markerId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
        (TZrSize)markerId > table->expansionCount ||
        !ZrParser_SyntaxContext_TransparencyIsValid(transparency) ||
        table->markCount >=
                (TZrSize)ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
        !ZrParser_SyntaxContext_SizeAdd(
                table->markCount, 1u, &required) ||
        !hygiene_capacity_bytes(
                table->markCapacity, required, 16u,
                sizeof(SZrSyntaxMarkRecord), &capacityBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                sizeof(SZrSyntaxMarkRecord), capacityBytes, &heapBytes)) {
        return invalid;
    }
    transaction = table->activeTransaction;
    ownsTransaction = transaction == ZR_NULL;
    originalMarks = table->marks;
    originalMarkCapacity = table->markCapacity;
    originalExpansions = table->expansions;
    originalExpansionCapacity = table->expansionCapacity;
    if (ownsTransaction) {
        ZrParser_ComptimeBudget_BeginTransaction(
                ZrParser_SyntaxContext_Budget(table), &localTransaction);
        transaction = &localTransaction;
        table->activeTransaction = transaction;
    }
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, 1u) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)heapBytes) ||
        !ZrParser_SyntaxContext_EnsureMarks(
                table, required, &capacityBytes)) {
        if (ownsTransaction) {
            table->activeTransaction = ZR_NULL;
            ZrParser_SyntaxContext_RollbackDeferredBuffers(
                    table, originalMarks, originalMarkCapacity,
                    originalExpansions, originalExpansionCapacity);
            ZrParser_ComptimeBudget_AbortTransaction(transaction);
        }
        return invalid;
    }
    previousCount = table->markCount;
    ZrParser_SyntaxContext_InitializeMark(
            table, &table->marks[previousCount], parent.markId,
            markerId, transparency);
    table->markCount++;
    if (ownsTransaction &&
        !ZrParser_ComptimeBudget_CommitTransaction(transaction)) {
        table->markCount = previousCount;
        memset(&table->marks[previousCount], 0,
               sizeof(table->marks[previousCount]));
        table->activeTransaction = ZR_NULL;
        ZrParser_SyntaxContext_RollbackDeferredBuffers(
                table, originalMarks, originalMarkCapacity,
                originalExpansions, originalExpansionCapacity);
        ZrParser_ComptimeBudget_AbortTransaction(transaction);
        return invalid;
    }
    if (ownsTransaction) {
        table->activeTransaction = ZR_NULL;
        ZrParser_SyntaxContext_CommitDeferredBuffers(table);
    }
    invalid.markId = (TZrUInt32)table->markCount;
    return invalid;
}

SZrSyntaxContext ZrParser_SyntaxContext_ApplyMark(
        SZrSyntaxContext parent,
        TZrUInt32 markerId,
        EZrSyntaxTransparency transparency) {
    SZrSyntaxContextTable *table = parent.table;
    TZrUInt32 existingMark;
    SZrSyntaxContext result;

    result = ZrParser_SyntaxContext_Invalid(table);
    if (table == ZR_NULL || !ZrParser_SyntaxContext_IsValid(parent) ||
        parent.table != table || markerId == 0u ||
        markerId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
        (TZrSize)markerId > table->expansionCount ||
        !ZrParser_SyntaxContext_TransparencyIsValid(transparency)) {
        return result;
    }
    if (ZrParser_SyntaxContext_FindMark(
                table, parent.markId, markerId, transparency,
                &existingMark)) {
        result.markId = existingMark;
        return result;
    }
    return hygiene_append_mark(table, parent, markerId, transparency);
}

SZrSyntaxContext ZrParser_SyntaxContext_ResolveAt(
        SZrSyntaxContext context,
        SZrSyntaxContext target) {
    SZrSyntaxContextTable *table = context.table != ZR_NULL
            ? context.table : target.table;

    if (!ZrParser_SyntaxContext_IsValid(context) ||
        !ZrParser_SyntaxContext_IsValid(target) ||
        !ZrParser_SyntaxContext_IsSameTable(context, target)) {
        return ZrParser_SyntaxContext_Invalid(table);
    }
    return target;
}

static TZrBool hygiene_collect_chain(
        SZrSyntaxContext context,
        TZrUInt32 **outIds,
        TZrSize *outCount,
        SZrParserComptimeBudgetTransaction *transaction) {
    const SZrSyntaxContextTable *table = context.table;
    TZrUInt32 *ids;
    TZrSize count = 0u;
    TZrSize bytes;
    TZrUInt32 markId;

    if (outIds == ZR_NULL || outCount == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(context)) {
        return ZR_FALSE;
    }
    *outIds = ZR_NULL;
    *outCount = 0u;
    if (context.markId == 0u) {
        return ZR_TRUE;
    }
    markId = context.markId;
    while (markId != 0u) {
        if (!hygiene_mark_is_valid(table, markId) ||
            count == ZR_MAX_SIZE) {
            return ZR_FALSE;
        }
        count++;
        markId = table->marks[(TZrSize)markId - 1u].parentMarkId;
    }
    if (!ZrParser_SyntaxContext_SizeMul(
                count, sizeof(*ids), &bytes) || bytes == 0u) {
        return ZR_FALSE;
    }
    if (transaction != ZR_NULL &&
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)bytes)) {
        return ZR_FALSE;
    }
    ids = ZrCore_Memory_RawMallocWithType(
            table->global, bytes,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (ids == ZR_NULL) {
        if (transaction != ZR_NULL) {
            transaction->failed = ZR_TRUE;
        }
        return ZR_FALSE;
    }
    markId = context.markId;
    for (TZrSize index = count; index != 0u; index--) {
        ids[index - 1u] = markId;
        markId = table->marks[(TZrSize)markId - 1u].parentMarkId;
    }
    *outIds = ids;
    *outCount = count;
    return ZR_TRUE;
}

SZrSyntaxContext ZrParser_SyntaxContext_Normalize(
        SZrSyntaxContext context,
        EZrSyntaxNamespace namespaceKind) {
    SZrSyntaxContextTable *table = context.table;
    TZrUInt32 *ids = ZR_NULL;
    TZrSize idCount = 0u;
    TZrSize newCount = 0u;
    TZrSize required = 0u;
    TZrSize capacityBytes = 0u;
    TZrSize heapBytes = 0u;
    TZrSize pendingBytes = 0u;
    TZrUInt32 parentMark = 0u;
    SZrSyntaxMarkRecord *pending = ZR_NULL;
    SZrParserComptimeBudgetTransaction localTransaction;
    SZrParserComptimeBudgetTransaction *transaction;
    TZrBool ownsTransaction;
    SZrSyntaxContext result;
    SZrSyntaxMarkRecord *originalMarks;
    TZrSize originalMarkCapacity;
    SZrSyntaxExpansionRecord *originalExpansions;
    TZrSize originalExpansionCapacity;
    TZrSize originalMarkCount;
    TZrSize originalExpansionCount;

    if (!ZrParser_SyntaxContext_NamespaceIsValid(namespaceKind) ||
        !ZrParser_SyntaxContext_IsValid(context)) {
        return ZrParser_SyntaxContext_Invalid(table);
    }
    if (context.markId == 0u) {
        return context;
    }
    originalMarks = table->marks;
    originalMarkCapacity = table->markCapacity;
    originalExpansions = table->expansions;
    originalExpansionCapacity = table->expansionCapacity;
    originalMarkCount = table->markCount;
    originalExpansionCount = table->expansionCount;
    transaction = table->activeTransaction;
    ownsTransaction = transaction == ZR_NULL;
    if (ownsTransaction) {
        ZrParser_ComptimeBudget_BeginTransaction(
                ZrParser_SyntaxContext_Budget(table), &localTransaction);
        transaction = &localTransaction;
        table->activeTransaction = transaction;
    }

    if (!hygiene_collect_chain(context, &ids, &idCount, transaction) ||
        !ZrParser_SyntaxContext_SizeMul(
                idCount, sizeof(*pending), &pendingBytes) ||
        (pendingBytes != 0u &&
         (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                  transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                  (TZrUInt64)pendingBytes) ||
          (pending = ZrCore_Memory_RawMallocWithType(
                   table->global, pendingBytes,
                   ZR_MEMORY_NATIVE_TYPE_ARRAY)) == ZR_NULL))) {
        if (pendingBytes != 0u && pending == ZR_NULL) {
            transaction->failed = ZR_TRUE;
        }
        goto normalize_failure;
    }
    if (pendingBytes != 0u) {
        memset(pending, 0, pendingBytes);
    }
    for (TZrSize index = 0u; index < idCount; index++) {
        const SZrSyntaxMarkRecord *record =
                &table->marks[(TZrSize)ids[index] - 1u];
        TZrBool keep = namespaceKind == ZR_SYNTAX_NAMESPACE_LOCAL
                ? record->transparency != ZR_SYNTAX_TRANSPARENT
                : record->transparency == ZR_SYNTAX_OPAQUE;
        TZrUInt32 mapped;

        if (!keep) {
            continue;
        }
        if (!ZrParser_SyntaxContext_FindMark(
                    table, parentMark, record->markerId,
                    record->transparency, &mapped)) {
            mapped = 0u;
            for (TZrSize pendingIndex = 0u;
                 pendingIndex < newCount; pendingIndex++) {
                if (pending[pendingIndex].parentMarkId == parentMark &&
                    pending[pendingIndex].markerId == record->markerId &&
                    pending[pendingIndex].transparency ==
                            record->transparency) {
                    mapped = (TZrUInt32)(table->markCount + pendingIndex + 1u);
                    break;
                }
            }
            if (mapped == 0u) {
                pending[newCount].parentMarkId = parentMark;
                pending[newCount].markerId = record->markerId;
                pending[newCount].transparency = record->transparency;
                mapped = (TZrUInt32)(table->markCount + newCount + 1u);
                newCount++;
            }
        }
        parentMark = mapped;
    }
    if (!ZrParser_SyntaxContext_SizeAdd(
                table->markCount, newCount, &required) ||
        !hygiene_capacity_bytes(
                table->markCapacity, required, 16u,
                sizeof(SZrSyntaxMarkRecord), &capacityBytes) ||
        !ZrParser_SyntaxContext_SizeMul(
                newCount, sizeof(SZrSyntaxMarkRecord), &heapBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                heapBytes, capacityBytes, &heapBytes) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT,
                (TZrUInt64)newCount) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)heapBytes) ||
        !ZrParser_SyntaxContext_EnsureMarks(
                table, required, &capacityBytes)) {
        goto normalize_failure;
    }
    result = ZrParser_SyntaxContext_TableRoot(table);
    for (TZrSize index = 0u; index < newCount; index++) {
        ZrParser_SyntaxContext_InitializeMark(
                table, &table->marks[table->markCount],
                pending[index].parentMarkId, pending[index].markerId,
                pending[index].transparency);
        table->markCount++;
    }
    result.markId = parentMark;
    if (ownsTransaction &&
        !ZrParser_ComptimeBudget_CommitTransaction(transaction)) {
        goto normalize_failure;
    }
    if (ownsTransaction) {
        table->activeTransaction = ZR_NULL;
        ZrParser_SyntaxContext_CommitDeferredBuffers(table);
    }
    if (pending != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, pending, pendingBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_Memory_RawFreeWithType(
            table->global, ids, idCount * sizeof(*ids),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return result;

normalize_failure:
    if (ownsTransaction) {
        table->activeTransaction = ZR_NULL;
        while (table->expansionCount > originalExpansionCount) {
            table->expansionCount--;
            ZrParser_SyntaxContext_FreeExpansionRecord(
                    table, &table->expansions[table->expansionCount]);
        }
        while (table->markCount > originalMarkCount) {
            table->markCount--;
            memset(&table->marks[table->markCount], 0,
                   sizeof(table->marks[table->markCount]));
        }
        ZrParser_SyntaxContext_RollbackDeferredBuffers(
                table, originalMarks, originalMarkCapacity,
                originalExpansions, originalExpansionCapacity);
        ZrParser_ComptimeBudget_AbortTransaction(transaction);
    }
    if (pending != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, pending, pendingBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (ids != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, ids, idCount * sizeof(*ids),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZrParser_SyntaxContext_Invalid(table);
}
