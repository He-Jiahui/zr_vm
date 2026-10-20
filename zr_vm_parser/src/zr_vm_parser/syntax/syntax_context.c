#include "syntax/syntax_context_internal.h"

#include <limits.h>
#include <string.h>

#include "zr_vm_core/memory.h"

static TZrBool context_owned_bytes(TZrSize length, TZrSize *outBytes) {
    if (outBytes == ZR_NULL || length == ZR_MAX_SIZE) {
        return ZR_FALSE;
    }
    *outBytes = length + 1u;
    return ZR_TRUE;
}

typedef struct SZrContextCheckpoint {
    TZrSize markCount;
    TZrSize expansionCount;
    SZrSyntaxMarkRecord *marks;
    TZrSize markCapacity;
    SZrSyntaxExpansionRecord *expansions;
    TZrSize expansionCapacity;
} SZrContextCheckpoint;

static void context_rollback(
        SZrSyntaxContextTable *table,
        const SZrContextCheckpoint *checkpoint) {
    if (table == ZR_NULL || checkpoint == ZR_NULL) {
        return;
    }
    /* An enclosing import owns the transaction and checkpoint.  A nested
     * registration must not erase earlier records or charges from that
     * operation. */
    if (table->activeTransaction != ZR_NULL) {
        return;
    }
    while (table->expansionCount > checkpoint->expansionCount) {
        table->expansionCount--;
        ZrParser_SyntaxContext_FreeExpansionRecord(
                table, &table->expansions[table->expansionCount]);
    }
    while (table->markCount > checkpoint->markCount) {
        table->markCount--;
        memset(&table->marks[table->markCount], 0,
               sizeof(table->marks[table->markCount]));
    }
    ZrParser_SyntaxContext_RollbackDeferredBuffers(
            table, checkpoint->marks, checkpoint->markCapacity,
            checkpoint->expansions, checkpoint->expansionCapacity);
}

void ZrParser_SyntaxContext_DefaultBudget(
        SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits = {0};

    if (budget == ZR_NULL) {
        return;
    }
    limits.fuel = ZR_PARSER_COMPTIME_DEFAULT_FUEL;
    limits.callDepth = ZR_PARSER_COMPTIME_DEFAULT_CALL_DEPTH;
    limits.heapBytes = ZR_PARSER_COMPTIME_DEFAULT_HEAP_BYTES;
    limits.aggregateCount = ZR_PARSER_COMPTIME_DEFAULT_AGGREGATE_COUNT;
    limits.generatedDeclarationCount =
            ZR_PARSER_COMPTIME_DEFAULT_GENERATED_DECLARATION_COUNT;
    limits.diagnosticCount = ZR_PARSER_COMPTIME_DEFAULT_DIAGNOSTIC_COUNT;
    limits.tokenCount = ZR_PARSER_COMPTIME_DEFAULT_TOKEN_COUNT;
    limits.nodeCount = ZR_PARSER_COMPTIME_DEFAULT_NODE_COUNT;
    limits.matchingCount = ZR_PARSER_COMPTIME_DEFAULT_MATCHING_COUNT;
    limits.expansionCount = ZR_PARSER_COMPTIME_DEFAULT_EXPANSION_COUNT;
    ZrParser_ComptimeBudget_Init(budget, &limits);
}

SZrParserComptimeBudget *ZrParser_SyntaxContext_Budget(
        SZrSyntaxContextTable *table) {
    if (table == ZR_NULL) {
        return ZR_NULL;
    }
    return table->budget != ZR_NULL ? table->budget : &table->fallbackBudget;
}

TZrBool ZrParser_SyntaxContext_SizeAdd(
        TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || left > ZR_MAX_SIZE - right) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

TZrBool ZrParser_SyntaxContext_SizeMul(
        TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || (left != 0u && right > ZR_MAX_SIZE / left)) {
        return ZR_FALSE;
    }
    *out = left * right;
    return ZR_TRUE;
}

TZrBool ZrParser_SyntaxContext_TransparencyIsValid(
        EZrSyntaxTransparency transparency) {
    switch (transparency) {
        case ZR_SYNTAX_TRANSPARENT:
        case ZR_SYNTAX_SEMI_OPAQUE:
        case ZR_SYNTAX_OPAQUE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrParser_SyntaxContext_NamespaceIsValid(
        EZrSyntaxNamespace namespaceKind) {
    switch (namespaceKind) {
        case ZR_SYNTAX_NAMESPACE_LOCAL:
        case ZR_SYNTAX_NAMESPACE_ITEM:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

SZrSyntaxContext ZrParser_SyntaxContext_Invalid(
        SZrSyntaxContextTable *table) {
    SZrSyntaxContext context;

    context.table = table;
    context.markId = ZR_SYNTAX_CONTEXT_INVALID_MARK_ID;
    return context;
}

static TZrBool context_mark_id_is_valid(
        const SZrSyntaxContextTable *table, TZrUInt32 markId) {
    return table != ZR_NULL && markId != 0u &&
           markId != ZR_SYNTAX_CONTEXT_INVALID_MARK_ID &&
           (TZrSize)markId <= table->markCount;
}

TZrBool ZrParser_SyntaxContext_ValidateMarkChain(
        const SZrSyntaxContextTable *table, TZrUInt32 markId) {
    TZrSize steps = 0u;

    if (table == ZR_NULL) {
        return markId == 0u;
    }
    if (markId == 0u) {
        return ZR_TRUE;
    }
    while (markId != 0u) {
        const SZrSyntaxMarkRecord *record;

        if (!context_mark_id_is_valid(table, markId) ||
            steps++ > table->markCount) {
            return ZR_FALSE;
        }
        record = &table->marks[(TZrSize)markId - 1u];
        if (!ZrParser_SyntaxContext_TransparencyIsValid(
                    record->transparency) ||
            record->markerId == 0u ||
            record->markerId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
            (TZrSize)record->markerId > table->expansionCount) {
            return ZR_FALSE;
        }
        if (record->parentMarkId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
            (TZrSize)record->parentMarkId > table->markCount) {
            return ZR_FALSE;
        }
        markId = record->parentMarkId;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_SyntaxContext_FindMark(
        const SZrSyntaxContextTable *table,
        TZrUInt32 parentMarkId,
        TZrUInt32 markerId,
        EZrSyntaxTransparency transparency,
        TZrUInt32 *outMarkId) {
    if (table == ZR_NULL || outMarkId == ZR_NULL ||
        !ZrParser_SyntaxContext_TransparencyIsValid(transparency)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < table->markCount; index++) {
        const SZrSyntaxMarkRecord *record = &table->marks[index];

        if (record->parentMarkId == parentMarkId &&
            record->markerId == markerId &&
            record->transparency == transparency) {
            *outMarkId = (TZrUInt32)(index + 1u);
            return ZR_TRUE;
        }
    }
    *outMarkId = 0u;
    return ZR_FALSE;
}

static TZrSize context_next_capacity(
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

TZrBool ZrParser_SyntaxContext_EnsureMarks(
        SZrSyntaxContextTable *table,
        TZrSize required,
        TZrSize *outCapacityBytes) {
    TZrSize capacity;
    TZrSize bytes;
    SZrSyntaxMarkRecord *records;

    if (table == ZR_NULL || outCapacityBytes == ZR_NULL) {
        return ZR_FALSE;
    }
    *outCapacityBytes = 0u;
    if (required <= table->markCapacity) {
        return ZR_TRUE;
    }
    capacity = context_next_capacity(table->markCapacity, required, 16u);
    if (!ZrParser_SyntaxContext_SizeMul(
                capacity, sizeof(*records), &bytes)) {
        return ZR_FALSE;
    }
    records = ZrCore_Memory_RawMallocWithType(
            table->global, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (records == ZR_NULL) {
        return ZR_FALSE;
    }
    if (table->marks != ZR_NULL && table->markCount != 0u) {
        memcpy(records, table->marks,
               table->markCount * sizeof(*records));
        if (table->activeTransaction != ZR_NULL) {
            if (table->deferredMarks == ZR_NULL) {
                table->deferredMarks = table->marks;
                table->deferredMarkCapacity = table->markCapacity;
            } else {
                ZrCore_Memory_RawFreeWithType(
                        table->global, table->marks,
                        table->markCapacity * sizeof(*records),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        } else {
            ZrCore_Memory_RawFreeWithType(
                    table->global, table->marks,
                    table->markCapacity * sizeof(*records),
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
    }
    table->marks = records;
    table->markCapacity = capacity;
    *outCapacityBytes = bytes;
    return ZR_TRUE;
}

TZrBool ZrParser_SyntaxContext_EnsureExpansions(
        SZrSyntaxContextTable *table,
        TZrSize required,
        TZrSize *outCapacityBytes) {
    TZrSize capacity;
    TZrSize bytes;
    SZrSyntaxExpansionRecord *records;

    if (table == ZR_NULL || outCapacityBytes == ZR_NULL) {
        return ZR_FALSE;
    }
    *outCapacityBytes = 0u;
    if (required <= table->expansionCapacity) {
        return ZR_TRUE;
    }
    capacity = context_next_capacity(table->expansionCapacity, required, 8u);
    if (!ZrParser_SyntaxContext_SizeMul(
                capacity, sizeof(*records), &bytes)) {
        return ZR_FALSE;
    }
    records = ZrCore_Memory_RawMallocWithType(
            table->global, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (records == ZR_NULL) {
        return ZR_FALSE;
    }
    if (table->expansions != ZR_NULL && table->expansionCount != 0u) {
        memcpy(records, table->expansions,
               table->expansionCount * sizeof(*records));
        if (table->activeTransaction != ZR_NULL) {
            if (table->deferredExpansions == ZR_NULL) {
                table->deferredExpansions = table->expansions;
                table->deferredExpansionCapacity = table->expansionCapacity;
            } else {
                ZrCore_Memory_RawFreeWithType(
                        table->global, table->expansions,
                        table->expansionCapacity * sizeof(*records),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        } else {
            ZrCore_Memory_RawFreeWithType(
                    table->global, table->expansions,
                    table->expansionCapacity * sizeof(*records),
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
    }
    table->expansions = records;
    table->expansionCapacity = capacity;
    *outCapacityBytes = bytes;
    return ZR_TRUE;
}

TZrChar *ZrParser_SyntaxContext_CopyBytes(
        SZrSyntaxContextTable *table,
        const TZrChar *bytes,
        TZrSize length) {
    TZrSize allocationSize;
    TZrChar *copy;

    if (table == ZR_NULL || !context_owned_bytes(length, &allocationSize) ||
        (length != 0u && bytes == ZR_NULL)) {
        return ZR_NULL;
    }
    if (length == 0u) {
        return ZR_NULL;
    }
    copy = ZrCore_Memory_RawMallocWithType(
            table->global, allocationSize, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(copy, bytes, length);
    copy[length] = '\0';
    return copy;
}

void ZrParser_SyntaxContext_FreeExpansionRecord(
        SZrSyntaxContextTable *table,
        SZrSyntaxExpansionRecord *record) {
    if (table == ZR_NULL || record == ZR_NULL) {
        return;
    }
    if (record->definitionModule != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, record->definitionModule,
                record->info.definitionModuleLength + 1u,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (record->callSiteSource != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, record->callSiteSource,
                record->info.callSiteSourceLength + 1u,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (record->definitionSiteSource != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, record->definitionSiteSource,
                record->info.definitionSiteSourceLength + 1u,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    memset(record, 0, sizeof(*record));
}

void ZrParser_SyntaxContext_CommitDeferredBuffers(
        SZrSyntaxContextTable *table) {
    if (table == ZR_NULL) {
        return;
    }
    if (table->deferredMarks != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->deferredMarks,
                table->deferredMarkCapacity * sizeof(*table->deferredMarks),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (table->deferredExpansions != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->deferredExpansions,
                table->deferredExpansionCapacity *
                        sizeof(*table->deferredExpansions),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    table->deferredMarks = ZR_NULL;
    table->deferredMarkCapacity = 0u;
    table->deferredExpansions = ZR_NULL;
    table->deferredExpansionCapacity = 0u;
}

void ZrParser_SyntaxContext_RollbackDeferredBuffers(
        SZrSyntaxContextTable *table,
        SZrSyntaxMarkRecord *originalMarks,
        TZrSize originalMarkCapacity,
        SZrSyntaxExpansionRecord *originalExpansions,
        TZrSize originalExpansionCapacity) {
    if (table == ZR_NULL) {
        return;
    }
    if (table->marks != originalMarks && table->marks != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->marks,
                table->markCapacity * sizeof(*table->marks),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (table->deferredMarks != ZR_NULL &&
        table->deferredMarks != originalMarks) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->deferredMarks,
                table->deferredMarkCapacity * sizeof(*table->deferredMarks),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (table->expansions != originalExpansions &&
        table->expansions != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->expansions,
                table->expansionCapacity * sizeof(*table->expansions),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (table->deferredExpansions != ZR_NULL &&
        table->deferredExpansions != originalExpansions) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->deferredExpansions,
                table->deferredExpansionCapacity *
                        sizeof(*table->deferredExpansions),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    table->marks = originalMarks;
    table->markCapacity = originalMarkCapacity;
    table->expansions = originalExpansions;
    table->expansionCapacity = originalExpansionCapacity;
    table->deferredMarks = ZR_NULL;
    table->deferredMarkCapacity = 0u;
    table->deferredExpansions = ZR_NULL;
    table->deferredExpansionCapacity = 0u;
}

static void context_batch_restore(SZrSyntaxContextBatch *batch) {
    SZrSyntaxContextTable *table;

    if (batch == ZR_NULL || batch->table == ZR_NULL) {
        return;
    }
    table = batch->table;
    while (table->expansionCount > batch->expansionCount) {
        table->expansionCount--;
        ZrParser_SyntaxContext_FreeExpansionRecord(
                table, &table->expansions[table->expansionCount]);
    }
    while (table->markCount > batch->markCount) {
        table->markCount--;
        memset(&table->marks[table->markCount], 0,
               sizeof(table->marks[table->markCount]));
    }
    ZrParser_SyntaxContext_RollbackDeferredBuffers(
            table, batch->marks, batch->markCapacity,
            batch->expansions, batch->expansionCapacity);
}

TZrBool ZrParser_SyntaxContext_BeginBatch(
        SZrSyntaxContextTable *table,
        SZrSyntaxContextBatch *batch) {
    if (table == ZR_NULL || batch == ZR_NULL ||
        table->activeTransaction != ZR_NULL) {
        return ZR_FALSE;
    }
    memset(batch, 0, sizeof(*batch));
    batch->table = table;
    batch->marks = table->marks;
    batch->markCapacity = table->markCapacity;
    batch->expansions = table->expansions;
    batch->expansionCapacity = table->expansionCapacity;
    batch->markCount = table->markCount;
    batch->expansionCount = table->expansionCount;
    ZrParser_ComptimeBudget_BeginTransaction(
            ZrParser_SyntaxContext_Budget(table), &batch->transaction);
    table->activeTransaction = &batch->transaction;
    batch->active = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_SyntaxContext_CommitBatch(
        SZrSyntaxContextBatch *batch) {
    if (!ZrParser_SyntaxContext_PrepareBatch(batch)) {
        ZrParser_SyntaxContext_AbortBatch(batch);
        return ZR_FALSE;
    }
    return ZrParser_SyntaxContext_PublishBatch(batch);
}

TZrBool ZrParser_SyntaxContext_PrepareBatch(
        SZrSyntaxContextBatch *batch) {
    if (batch == ZR_NULL || !batch->active || batch->table == ZR_NULL ||
        batch->table->activeTransaction != &batch->transaction) {
        return ZR_FALSE;
    }
    batch->table->activeTransaction = ZR_NULL;
    if (!ZrParser_ComptimeBudget_CommitTransaction(&batch->transaction)) {
        batch->table->activeTransaction = &batch->transaction;
        return ZR_FALSE;
    }
    batch->prepared = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_SyntaxContext_PublishBatch(
        SZrSyntaxContextBatch *batch) {
    if (batch == ZR_NULL || !batch->active || batch->table == ZR_NULL ||
        !batch->prepared || batch->table->activeTransaction != ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_SyntaxContext_CommitDeferredBuffers(batch->table);
    batch->active = ZR_FALSE;
    batch->prepared = ZR_FALSE;
    return ZR_TRUE;
}

void ZrParser_SyntaxContext_AbortBatch(
        SZrSyntaxContextBatch *batch) {
    if (batch == ZR_NULL || !batch->active || batch->table == ZR_NULL) {
        return;
    }
    if (batch->table->activeTransaction == &batch->transaction) {
        batch->table->activeTransaction = ZR_NULL;
    }
    if (!batch->transaction.active) {
        /* A prepared transaction has already published its local budget.  Its
         * deferred buffers still carry the checkpoint and must be restored;
         * the caller owns any external shadow-budget merge. */
        context_batch_restore(batch);
        batch->active = ZR_FALSE;
        batch->prepared = ZR_FALSE;
        return;
    }
    context_batch_restore(batch);
    ZrParser_ComptimeBudget_AbortTransaction(&batch->transaction);
    batch->active = ZR_FALSE;
    batch->prepared = ZR_FALSE;
}

SZrSyntaxContext ZrParser_SyntaxContext_Root(void) {
    SZrSyntaxContext context = {ZR_NULL, 0u};
    return context;
}

SZrSyntaxContextTable *ZrParser_SyntaxContextTable_Create(
        SZrGlobalState *global,
        SZrParserComptimeBudget *budget) {
    SZrSyntaxContextTable *table;
    SZrParserComptimeBudget fallback;
    SZrParserComptimeBudgetTransaction transaction;
    SZrParserComptimeBudget *chargeBudget;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    if (budget == ZR_NULL) {
        ZrParser_SyntaxContext_DefaultBudget(&fallback);
        chargeBudget = &fallback;
    } else {
        chargeBudget = budget;
    }
    ZrParser_ComptimeBudget_BeginTransaction(chargeBudget, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                sizeof(*table))) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    table = ZrCore_Memory_RawMallocWithType(
            global, sizeof(*table), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (table == ZR_NULL) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    memset(table, 0, sizeof(*table));
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ZrCore_Memory_RawFreeWithType(
                global, table, sizeof(*table), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    table->global = global;
    table->refCount = 1u;
    table->budget = budget;
    if (budget == ZR_NULL) {
        table->fallbackBudget = fallback;
    }
    return table;
}

void ZrParser_SyntaxContextTable_Retain(SZrSyntaxContextTable *table) {
    if (table != ZR_NULL && table->refCount != (TZrUInt32)-1) {
        table->refCount++;
    }
}

void ZrParser_SyntaxContextTable_Release(SZrSyntaxContextTable *table) {
    if (table == ZR_NULL || table->refCount == 0u) {
        return;
    }
    table->refCount--;
    if (table->refCount != 0u) {
        return;
    }
    for (TZrSize index = 0u; index < table->expansionCount; index++) {
        ZrParser_SyntaxContext_FreeExpansionRecord(
                table, &table->expansions[index]);
    }
    if (table->marks != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->marks,
                table->markCapacity * sizeof(*table->marks),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (table->expansions != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                table->global, table->expansions,
                table->expansionCapacity * sizeof(*table->expansions),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrParser_SyntaxContext_CommitDeferredBuffers(table);
    ZrCore_Memory_RawFreeWithType(
            table->global, table, sizeof(*table), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

static TZrBool context_copy_registration_context(
        SZrSyntaxContextTable *table,
        SZrSyntaxContext source,
        SZrSyntaxContext *outContext) {
    if (outContext == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(source)) {
        return ZR_FALSE;
    }
    /* A null-table root means that the expansion did not supply an origin
     * context.  Preserve that distinction so CallSite/DefSite can synthesize
     * their own marks; imported token roots are normalized separately. */
    if (source.table == ZR_NULL) {
        *outContext = source;
        return ZR_TRUE;
    }
    if (source.table == table) {
        *outContext = source;
        return ZR_TRUE;
    }
    return ZrParser_SyntaxContext_Import(table, source, outContext);
}

TZrBool ZrParser_SyntaxContextTable_RegisterExpansion(
        SZrSyntaxContextTable *table,
        const SZrSyntaxExpansionInfo *info,
        TZrSyntaxExpansionId *outExpansionId) {
    SZrSyntaxExpansionRecord *record;
    SZrSyntaxExpansionInfo copiedInfo;
    TZrSize definitionBytes;
    TZrSize callSiteBytes;
    TZrSize definitionSiteBytes;
    TZrSize totalBytes;
    TZrSize capacityBytes;
    TZrSize required;
    SZrContextCheckpoint checkpoint;
    SZrParserComptimeBudgetTransaction localTransaction;
    SZrParserComptimeBudgetTransaction *transaction;
    TZrBool ownsTransaction;

    if (table == ZR_NULL || info == ZR_NULL || outExpansionId == ZR_NULL ||
        table->expansionCount >= (TZrSize)ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
        !context_owned_bytes(info->definitionModuleLength, &definitionBytes) ||
        !context_owned_bytes(info->callSiteSourceLength, &callSiteBytes) ||
        !context_owned_bytes(info->definitionSiteSourceLength,
                             &definitionSiteBytes) ||
        (info->definitionModuleLength != 0u &&
         info->definitionModule == ZR_NULL) ||
        (info->callSiteSourceLength != 0u && info->callSiteSource == ZR_NULL) ||
        (info->definitionSiteSourceLength != 0u &&
         info->definitionSiteSource == ZR_NULL) ||
        (info->parentExpansionId != 0u &&
         (TZrSize)info->parentExpansionId > table->expansionCount)) {
        return ZR_FALSE;
    }
    checkpoint.markCount = table->markCount;
    checkpoint.expansionCount = table->expansionCount;
    checkpoint.marks = table->marks;
    checkpoint.markCapacity = table->markCapacity;
    checkpoint.expansions = table->expansions;
    checkpoint.expansionCapacity = table->expansionCapacity;
    transaction = table->activeTransaction;
    ownsTransaction = transaction == ZR_NULL;
    if (ownsTransaction) {
        ZrParser_ComptimeBudget_BeginTransaction(
                ZrParser_SyntaxContext_Budget(table), &localTransaction);
        transaction = &localTransaction;
        table->activeTransaction = transaction;
    }
    if (!context_copy_registration_context(
                table, info->callSiteContext, &copiedInfo.callSiteContext) ||
        !context_copy_registration_context(
                table, info->definitionSiteContext,
                &copiedInfo.definitionSiteContext)) {
        if (ownsTransaction) {
            table->activeTransaction = ZR_NULL;
            ZrParser_ComptimeBudget_AbortTransaction(transaction);
        }
        context_rollback(table, &checkpoint);
        return ZR_FALSE;
    }
    if (!ZrParser_SyntaxContext_SizeAdd(
                sizeof(*record), definitionBytes, &totalBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                totalBytes, callSiteBytes, &totalBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                totalBytes, definitionSiteBytes, &totalBytes) ||
        !ZrParser_SyntaxContext_SizeAdd(
                table->expansionCount, 1u, &required)) {
        if (ownsTransaction) {
            table->activeTransaction = ZR_NULL;
            ZrParser_ComptimeBudget_AbortTransaction(transaction);
        }
        context_rollback(table, &checkpoint);
        return ZR_FALSE;
    }
    capacityBytes = 0u;
    if (required > table->expansionCapacity) {
        TZrSize nextCapacity = context_next_capacity(
                table->expansionCapacity, required, 8u);
        if (!ZrParser_SyntaxContext_SizeMul(
                    nextCapacity, sizeof(*record), &capacityBytes) ||
            !ZrParser_SyntaxContext_SizeAdd(
                    totalBytes, capacityBytes, &totalBytes)) {
            if (ownsTransaction) {
                table->activeTransaction = ZR_NULL;
                ZrParser_ComptimeBudget_AbortTransaction(transaction);
            }
            context_rollback(table, &checkpoint);
            return ZR_FALSE;
        }
    }
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT, 1u) ||
        !ZrParser_ComptimeBudget_TryConsumeTransaction(
                transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)totalBytes) ||
        !ZrParser_SyntaxContext_EnsureExpansions(
                table, required, &capacityBytes)) {
        if (ownsTransaction) {
            table->activeTransaction = ZR_NULL;
            ZrParser_ComptimeBudget_AbortTransaction(transaction);
        }
        context_rollback(table, &checkpoint);
        return ZR_FALSE;
    }
    record = &table->expansions[table->expansionCount];
    memset(record, 0, sizeof(*record));
    copiedInfo.parentExpansionId = info->parentExpansionId;
    copiedInfo.stableIdentity = info->stableIdentity;
    copiedInfo.definitionModule = info->definitionModule;
    copiedInfo.definitionModuleLength = info->definitionModuleLength;
    copiedInfo.callSiteSource = info->callSiteSource;
    copiedInfo.callSiteSourceLength = info->callSiteSourceLength;
    copiedInfo.definitionSiteSource = info->definitionSiteSource;
    copiedInfo.definitionSiteSourceLength = info->definitionSiteSourceLength;
    copiedInfo.callSiteSpan = info->callSiteSpan;
    copiedInfo.definitionSiteSpan = info->definitionSiteSpan;
    record->info = copiedInfo;
    record->definitionModule = ZrParser_SyntaxContext_CopyBytes(
            table, info->definitionModule, info->definitionModuleLength);
    record->callSiteSource = ZrParser_SyntaxContext_CopyBytes(
            table, info->callSiteSource, info->callSiteSourceLength);
    record->definitionSiteSource = ZrParser_SyntaxContext_CopyBytes(
            table, info->definitionSiteSource,
            info->definitionSiteSourceLength);
    if ((info->definitionModuleLength != 0u &&
         record->definitionModule == ZR_NULL) ||
        (info->callSiteSourceLength != 0u &&
         record->callSiteSource == ZR_NULL) ||
        (info->definitionSiteSourceLength != 0u &&
         record->definitionSiteSource == ZR_NULL) ||
         (ownsTransaction &&
         !ZrParser_ComptimeBudget_CommitTransaction(transaction))) {
        ZrParser_SyntaxContext_FreeExpansionRecord(table, record);
        if (ownsTransaction) {
            table->activeTransaction = ZR_NULL;
            ZrParser_ComptimeBudget_AbortTransaction(transaction);
        }
        context_rollback(table, &checkpoint);
        return ZR_FALSE;
    }
    record->info.definitionModule = record->definitionModule;
    record->info.callSiteSource = record->callSiteSource;
    record->info.definitionSiteSource = record->definitionSiteSource;
    table->expansionCount++;
    if (ownsTransaction) {
        table->activeTransaction = ZR_NULL;
        ZrParser_SyntaxContext_CommitDeferredBuffers(table);
    }
    *outExpansionId = (TZrSyntaxExpansionId)table->expansionCount;
    return ZR_TRUE;
}

const SZrSyntaxExpansionInfo *ZrParser_SyntaxContextTable_Expansion(
        const SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId) {
    if (table == ZR_NULL || expansionId == 0u ||
        expansionId == ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ||
        (TZrSize)expansionId > table->expansionCount) {
        return ZR_NULL;
    }
    return &table->expansions[(TZrSize)expansionId - 1u].info;
}

SZrSyntaxContext ZrParser_SyntaxContext_TableRoot(
        SZrSyntaxContextTable *table) {
    SZrSyntaxContext context = {table, 0u};
    return context;
}

static SZrSyntaxContext context_expansion_site(
        SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId,
        EZrSyntaxTransparency transparency) {
    const SZrSyntaxExpansionInfo *info;

    if (table == ZR_NULL ||
        !ZrParser_SyntaxContext_TransparencyIsValid(transparency)) {
        return ZrParser_SyntaxContext_Invalid(table);
    }
    info = ZrParser_SyntaxContextTable_Expansion(table, expansionId);
    if (info == ZR_NULL) {
        return ZrParser_SyntaxContext_Invalid(table);
    }
    if (transparency == ZR_SYNTAX_TRANSPARENT &&
        ZrParser_SyntaxContext_IsValid(info->callSiteContext) &&
        info->callSiteContext.markId != 0u &&
        info->callSiteContext.table == table) {
        return info->callSiteContext;
    }
    if (transparency == ZR_SYNTAX_OPAQUE &&
        ZrParser_SyntaxContext_IsValid(info->definitionSiteContext) &&
        info->definitionSiteContext.markId != 0u &&
        info->definitionSiteContext.table == table) {
        return info->definitionSiteContext;
    }
    return ZrParser_SyntaxContext_ApplyMark(
            ZrParser_SyntaxContext_TableRoot(table), expansionId,
            transparency);
}

SZrSyntaxContext ZrParser_SyntaxContext_CallSite(
        SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId) {
    return context_expansion_site(table, expansionId, ZR_SYNTAX_TRANSPARENT);
}

SZrSyntaxContext ZrParser_SyntaxContext_DefSite(
        SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId) {
    return context_expansion_site(table, expansionId, ZR_SYNTAX_OPAQUE);
}

SZrSyntaxContext ZrParser_SyntaxContext_MixedSite(
        SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId) {
    return context_expansion_site(table, expansionId, ZR_SYNTAX_SEMI_OPAQUE);
}
