#include "syntax/syntax_context_internal.h"

#include <string.h>

#include "zr_vm_core/memory.h"

typedef struct SZrImportStackEntry {
    TZrUInt32 expansionId;
    TZrBool post;
} SZrImportStackEntry;

typedef struct SZrImportCheckpoint {
    TZrSize markCount;
    TZrSize expansionCount;
    SZrSyntaxMarkRecord *marks;
    TZrSize markCapacity;
    SZrSyntaxExpansionRecord *expansions;
    TZrSize expansionCapacity;
    TZrBool ownsTransaction;
} SZrImportCheckpoint;

static TZrBool import_reserve_heap(
        SZrSyntaxContextTable *table, TZrSize bytes) {
    if (bytes == 0u) {
        return ZR_TRUE;
    }
    if (table == ZR_NULL || table->activeTransaction == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrParser_ComptimeBudget_TryConsumeTransaction(
            table->activeTransaction,
            ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            (TZrUInt64)bytes);
}

static TZrBool import_capacity_bytes(
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
    capacity = current == 0u ? initial : current;
    while (capacity < required) {
        if (capacity > ZR_MAX_SIZE / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    return ZrParser_SyntaxContext_SizeMul(capacity, elementSize, outBytes);
}

static TZrBool import_stack_push(
        SZrSyntaxContextTable *table,
        SZrImportStackEntry **entries,
        TZrSize *count,
        TZrSize *capacity,
        TZrUInt32 expansionId,
        TZrBool post) {
    SZrImportStackEntry *next;
    TZrSize nextCapacity;
    TZrSize bytes;

    if (table == ZR_NULL || entries == ZR_NULL || count == ZR_NULL ||
        capacity == ZR_NULL || expansionId == 0u ||
        expansionId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID) {
        return ZR_FALSE;
    }
    if (*count == *capacity) {
        nextCapacity = *capacity == 0u ? 16u : *capacity;
        if (nextCapacity > ZR_MAX_SIZE / 2u) {
            nextCapacity = *count + 1u;
        } else {
            nextCapacity *= 2u;
        }
        if (!ZrParser_SyntaxContext_SizeMul(
                    nextCapacity, sizeof(*next), &bytes)) {
            return ZR_FALSE;
        }
        if (!import_reserve_heap(table, bytes)) {
            return ZR_FALSE;
        }
        next = ZrCore_Memory_RawMallocWithType(
                table->global, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (next == ZR_NULL) {
            return ZR_FALSE;
        }
        if (*entries != ZR_NULL && *count != 0u) {
            memcpy(next, *entries, *count * sizeof(*next));
            ZrCore_Memory_RawFreeWithType(
                    table->global, *entries,
                    *capacity * sizeof(*next), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        *entries = next;
        *capacity = nextCapacity;
    }
    (*entries)[*count].expansionId = expansionId;
    (*entries)[*count].post = post;
    (*count)++;
    return ZR_TRUE;
}

static void import_stack_free(
        SZrSyntaxContextTable *table,
        SZrImportStackEntry *entries,
        TZrSize capacity) {
    if (table != ZR_NULL && entries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, entries, capacity * sizeof(*entries),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
}

static TZrBool import_mark_dependencies(
        SZrSyntaxContextTable *destination,
        const SZrSyntaxContextTable *source,
        SZrSyntaxContext context,
        TZrUInt8 *state,
        TZrUInt32 *map,
        SZrImportStackEntry **stack,
        TZrSize *stackCount,
        TZrSize *stackCapacity) {
    TZrUInt32 markId;

    (void)map;
    if (!ZrParser_SyntaxContext_IsValid(context)) {
        return ZR_FALSE;
    }
    if (context.table == ZR_NULL) {
        return ZR_TRUE;
    }
    if (context.table != source ||
        !ZrParser_SyntaxContext_ValidateMarkChain(source, context.markId)) {
        return ZR_FALSE;
    }
    markId = context.markId;
    while (markId != 0u) {
        const SZrSyntaxMarkRecord *mark = &source->marks[(TZrSize)markId - 1u];

        if ((TZrSize)mark->markerId > source->expansionCount ||
            !ZrParser_SyntaxContext_TransparencyIsValid(mark->transparency)) {
            return ZR_FALSE;
        }
        if (state[mark->markerId] == 0u &&
            !import_stack_push(destination, stack, stackCount,
                               stackCapacity, mark->markerId, ZR_FALSE)) {
            return ZR_FALSE;
        }
        markId = mark->parentMarkId;
    }
    return ZR_TRUE;
}

static TZrBool import_expansion_dependencies(
        SZrSyntaxContextTable *destination,
        const SZrSyntaxContextTable *source,
        TZrUInt32 sourceExpansionId,
        TZrUInt8 *state,
        TZrUInt32 *map,
        SZrImportStackEntry **stack,
        TZrSize *stackCount,
        TZrSize *stackCapacity) {
    const SZrSyntaxExpansionRecord *record;

    if (sourceExpansionId == 0u ||
        (TZrSize)sourceExpansionId > source->expansionCount) {
        return ZR_FALSE;
    }
    record = &source->expansions[(TZrSize)sourceExpansionId - 1u];
    if (record->info.parentExpansionId != 0u &&
        (TZrSize)record->info.parentExpansionId > source->expansionCount) {
        return ZR_FALSE;
    }
    if (record->info.parentExpansionId != 0u &&
        state[record->info.parentExpansionId] == 0u &&
        !import_stack_push(destination, stack, stackCount,
                            stackCapacity, record->info.parentExpansionId,
                            ZR_FALSE)) {
        return ZR_FALSE;
    }
    if (!import_mark_dependencies(
                destination, source, record->info.callSiteContext,
                state, map, stack, stackCount, stackCapacity) ||
        !import_mark_dependencies(
                destination, source, record->info.definitionSiteContext,
                state, map, stack, stackCount, stackCapacity)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool import_mark_chain(
        SZrSyntaxContextTable *destination,
        const SZrSyntaxContextTable *source,
        SZrSyntaxContext sourceContext,
        const TZrUInt32 *map,
        SZrSyntaxContext *outContext) {
    TZrUInt32 *ids = ZR_NULL;
    TZrSize count = 0u;
    TZrUInt32 markId;
    SZrSyntaxContext result;

    if (outContext == ZR_NULL || !ZrParser_SyntaxContext_IsValid(sourceContext)) {
        return ZR_FALSE;
    }
    if (sourceContext.table == ZR_NULL) {
        *outContext = ZrParser_SyntaxContext_TableRoot(destination);
        return ZR_TRUE;
    }
    if (sourceContext.table != source ||
        !ZrParser_SyntaxContext_ValidateMarkChain(source, sourceContext.markId)) {
        return ZR_FALSE;
    }
    markId = sourceContext.markId;
    while (markId != 0u) {
        if (count == ZR_MAX_SIZE) {
            return ZR_FALSE;
        }
        count++;
        markId = source->marks[(TZrSize)markId - 1u].parentMarkId;
    }
    if (count != 0u) {
        TZrSize bytes;

        if (!ZrParser_SyntaxContext_SizeMul(
                    count, sizeof(*ids), &bytes)) {
            return ZR_FALSE;
        }
        if (!import_reserve_heap(destination, bytes)) {
            return ZR_FALSE;
        }
        ids = ZrCore_Memory_RawMallocWithType(
                destination->global, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (ids == ZR_NULL) {
            return ZR_FALSE;
        }
        markId = sourceContext.markId;
        for (TZrSize index = count; index != 0u; index--) {
            ids[index - 1u] = markId;
            markId = source->marks[(TZrSize)markId - 1u].parentMarkId;
        }
    }
    result = ZrParser_SyntaxContext_TableRoot(destination);
    for (TZrSize index = 0u; index < count; index++) {
        const SZrSyntaxMarkRecord *record =
                &source->marks[(TZrSize)ids[index] - 1u];
        TZrUInt32 importedMarker;

        if (record->markerId == 0u ||
            (TZrSize)record->markerId > source->expansionCount ||
            map[record->markerId] == 0u ||
            map[record->markerId] == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID) {
            if (ids != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        destination->global, ids, count * sizeof(*ids),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_FALSE;
        }
        importedMarker = map[record->markerId];
        result = ZrParser_SyntaxContext_ApplyMark(
                result, importedMarker, record->transparency);
        if (!ZrParser_SyntaxContext_IsValid(result)) {
            if (ids != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        destination->global, ids, count * sizeof(*ids),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_FALSE;
        }
    }
    if (ids != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                destination->global, ids, count * sizeof(*ids),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    *outContext = result;
    return ZR_TRUE;
}

static TZrUInt32 import_existing_expansion(
        const SZrSyntaxContextTable *destination,
        const SZrSyntaxExpansionInfo *sourceInfo) {
    if (sourceInfo->stableIdentity == 0u) {
        return 0u;
    }
    for (TZrSize index = 0u; index < destination->expansionCount; index++) {
        const SZrSyntaxExpansionInfo *candidate =
                &destination->expansions[index].info;

        if (ZrParser_SyntaxContext_ExpansionInfoEqual(candidate, sourceInfo)) {
            return (TZrUInt32)(index + 1u);
        }
    }
    return 0u;
}

static void import_rollback(
        SZrSyntaxContextTable *destination,
        const SZrImportCheckpoint *checkpoint) {
    while (destination->expansionCount > checkpoint->expansionCount) {
        destination->expansionCount--;
        ZrParser_SyntaxContext_FreeExpansionRecord(
                destination,
                &destination->expansions[destination->expansionCount]);
    }
    while (destination->markCount > checkpoint->markCount) {
        destination->markCount--;
        memset(&destination->marks[destination->markCount], 0,
               sizeof(destination->marks[destination->markCount]));
    }
    if (checkpoint->ownsTransaction) {
        ZrParser_SyntaxContext_RollbackDeferredBuffers(
                destination, checkpoint->marks, checkpoint->markCapacity,
                checkpoint->expansions, checkpoint->expansionCapacity);
    }
}

TZrBool ZrParser_SyntaxContext_Import(
        SZrSyntaxContextTable *destination,
        SZrSyntaxContext sourceContext,
        SZrSyntaxContext *outContext) {
    TZrUInt32 *map = ZR_NULL;
    TZrUInt8 *state = ZR_NULL;
    SZrImportStackEntry *stack = ZR_NULL;
    TZrSize stackCount = 0u;
    TZrSize stackCapacity = 0u;
    TZrSize sourceExpansionCount;
    TZrSize sourceMarkCount;
    TZrSize mapCount;
    TZrSize mapBytes;
    TZrSize stateBytes;
    TZrSize expansionCapacityBytes;
    TZrSize markCapacityBytes;
    TZrSize requiredExpansions;
    TZrSize requiredMarks;
    TZrSize scratchBytes;
    SZrParserComptimeBudgetTransaction localTransaction;
    SZrParserComptimeBudgetTransaction *transaction;
    TZrBool ownsTransaction;
    SZrImportCheckpoint checkpoint;

    if (destination == ZR_NULL || outContext == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(sourceContext)) {
        return ZR_FALSE;
    }
    if (sourceContext.table == ZR_NULL || sourceContext.markId == 0u) {
        *outContext = ZrParser_SyntaxContext_TableRoot(destination);
        return ZR_TRUE;
    }
    if (sourceContext.table == destination) {
        *outContext = sourceContext;
        return ZR_TRUE;
    }
    sourceExpansionCount = sourceContext.table->expansionCount;
    sourceMarkCount = sourceContext.table->markCount;
    if (sourceExpansionCount >=
                (TZrSize)ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
        !ZrParser_SyntaxContext_SizeAdd(
                sourceExpansionCount, 1u, &mapCount) ||
        !ZrParser_SyntaxContext_SizeMul(
                mapCount, sizeof(*map), &mapBytes) ||
        !ZrParser_SyntaxContext_SizeMul(
                mapCount, sizeof(*state), &stateBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                destination->expansionCount, sourceExpansionCount,
                &requiredExpansions) ||
        !import_capacity_bytes(
                destination->expansionCapacity, requiredExpansions, 8u,
                sizeof(SZrSyntaxExpansionRecord), &expansionCapacityBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                destination->markCount, sourceMarkCount, &requiredMarks) ||
        !import_capacity_bytes(
                destination->markCapacity, requiredMarks, 16u,
                sizeof(SZrSyntaxMarkRecord), &markCapacityBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                mapBytes, stateBytes, &scratchBytes)) {
        return ZR_FALSE;
    }
    transaction = destination->activeTransaction;
    ownsTransaction = transaction == ZR_NULL;
    if (ownsTransaction) {
        ZrParser_ComptimeBudget_BeginTransaction(
                ZrParser_SyntaxContext_Budget(destination), &localTransaction);
        transaction = &localTransaction;
    }
    checkpoint.markCount = destination->markCount;
    checkpoint.expansionCount = destination->expansionCount;
    checkpoint.marks = destination->marks;
    checkpoint.markCapacity = destination->markCapacity;
    checkpoint.expansions = destination->expansions;
    checkpoint.expansionCapacity = destination->expansionCapacity;
    checkpoint.ownsTransaction = ownsTransaction;
    if (ownsTransaction) {
        destination->activeTransaction = transaction;
    }
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)scratchBytes) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)expansionCapacityBytes) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)markCapacityBytes) ||
        !ZrParser_SyntaxContext_EnsureExpansions(
                destination,
                requiredExpansions,
                &expansionCapacityBytes) ||
        !ZrParser_SyntaxContext_EnsureMarks(
                destination, requiredMarks, &markCapacityBytes)) {
        goto import_failure;
    }
    if (!ZrParser_SyntaxContext_SizeMul(
                mapCount, sizeof(*map), &mapBytes) ||
        !ZrParser_SyntaxContext_SizeMul(
                mapCount, sizeof(*state), &stateBytes)) {
        goto import_failure;
    }
    map = ZrCore_Memory_RawMallocWithType(
            destination->global, mapBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    state = ZrCore_Memory_RawMallocWithType(
            destination->global, stateBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (map == ZR_NULL || state == ZR_NULL) {
        goto import_failure;
    }
    memset(map, 0, mapBytes);
    memset(state, 0, stateBytes);
    if (!import_stack_push(
                destination, &stack, &stackCount, &stackCapacity,
                sourceContext.table->marks[(TZrSize)sourceContext.markId - 1u]
                        .markerId,
                ZR_FALSE)) {
        goto import_failure;
    }
    while (stackCount != 0u) {
        SZrImportStackEntry entry = stack[--stackCount];
        const SZrSyntaxExpansionRecord *record;

        if ((TZrSize)entry.expansionId > sourceExpansionCount) {
            goto import_failure;
        }
        if (entry.post == ZR_FALSE) {
            if (state[entry.expansionId] == 2u) {
                continue;
            }
            if (state[entry.expansionId] == 1u) {
                goto import_failure;
            }
            state[entry.expansionId] = 1u;
            if (!import_stack_push(
                        destination, &stack, &stackCount, &stackCapacity,
                        entry.expansionId, ZR_TRUE) ||
                !import_expansion_dependencies(
                        destination, sourceContext.table, entry.expansionId,
                        state, map, &stack, &stackCount, &stackCapacity)) {
                goto import_failure;
            }
            continue;
        }
        if (state[entry.expansionId] != 1u) {
            goto import_failure;
        }
        record = &sourceContext.table->expansions[
                (TZrSize)entry.expansionId - 1u];
        {
            SZrSyntaxExpansionInfo copied = record->info;
            SZrSyntaxContext callSite;
            SZrSyntaxContext definitionSite;
            TZrUInt32 parent = 0u;
            TZrUInt32 existing;

            if (record->info.parentExpansionId != 0u) {
                parent = map[record->info.parentExpansionId];
                if (parent == 0u ||
                    parent == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID) {
                    goto import_failure;
                }
            }
            if (!import_mark_chain(
                        destination, sourceContext.table,
                        record->info.callSiteContext, map, &callSite) ||
                !import_mark_chain(
                        destination, sourceContext.table,
                        record->info.definitionSiteContext, map,
                        &definitionSite)) {
                goto import_failure;
            }
            copied.parentExpansionId = parent;
            copied.callSiteContext = callSite;
            copied.definitionSiteContext = definitionSite;
            existing = import_existing_expansion(destination, &copied);
            if (existing != 0u) {
                map[entry.expansionId] = existing;
            } else {
                TZrSyntaxExpansionId destinationId;

                if (!ZrParser_SyntaxContextTable_RegisterExpansion(
                            destination, &copied, &destinationId)) {
                    goto import_failure;
                }
                map[entry.expansionId] = destinationId;
            }
        }
        state[entry.expansionId] = 2u;
    }
    if (!import_mark_chain(
                destination, sourceContext.table, sourceContext, map,
                outContext)) {
        goto import_failure;
    }
    if (ownsTransaction) {
        destination->activeTransaction = ZR_NULL;
        if (!ZrParser_ComptimeBudget_CommitTransaction(transaction)) {
            goto import_failure;
        }
        ZrParser_SyntaxContext_CommitDeferredBuffers(destination);
    }
    import_stack_free(destination, stack, stackCapacity);
    ZrCore_Memory_RawFreeWithType(
            destination->global, map, mapBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrCore_Memory_RawFreeWithType(
            destination->global, state, stateBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    return ZR_TRUE;

import_failure:
    import_rollback(destination, &checkpoint);
    if (ownsTransaction) {
        destination->activeTransaction = ZR_NULL;
        ZrParser_ComptimeBudget_AbortTransaction(transaction);
    }
    import_stack_free(destination, stack, stackCapacity);
    if (map != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                destination->global, map, mapBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (state != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                destination->global, state, stateBytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZR_FALSE;
}
