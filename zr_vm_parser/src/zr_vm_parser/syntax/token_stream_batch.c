#include "syntax/token_stream_internal.h"

#include <string.h>

void token_stream_batch_begin(
        SZrTokenStream *stream, SZrTokenStreamBatch *batch) {
    memset(batch, 0, sizeof(*batch));
    batch->stream = stream;
    batch->budget = token_stream_budget(stream);
    batch->shadow = *batch->budget;
    batch->shadow.exceededResource = ZR_PARSER_COMPTIME_BUDGET_NONE;
    batch->initialUsage = batch->budget->usage;
    batch->sources = stream->sources;
    batch->sourceCount = stream->sourceCount;
    batch->sourceCapacity = stream->sourceCapacity;
    batch->trees = stream->trees;
    batch->treeCount = stream->treeCount;
    batch->treeCapacity = stream->treeCapacity;
    batch->roots = stream->roots;
    batch->rootCount = stream->rootCount;
    batch->rootCapacity = stream->rootCapacity;
    batch->endSpan = stream->endSpan;
    stream->batch = batch;
    stream->budget = &batch->shadow;
    batch->blockedByContextBatch = stream->contextTable->activeTransaction != ZR_NULL;
    if (stream->contextTable->activeTransaction == ZR_NULL) {
        batch->contextBudget = ZrParser_SyntaxContext_Budget(stream->contextTable);
        if (batch->contextBudget != batch->budget) {
            batch->contextShadow = *batch->contextBudget;
            batch->contextShadow.exceededResource = ZR_PARSER_COMPTIME_BUDGET_NONE;
            batch->contextInitialUsage = batch->contextBudget->usage;
        }
        stream->contextTable->budget = &batch->shadow;
        batch->ownsContextBatch = ZrParser_SyntaxContext_BeginBatch(
                stream->contextTable, &batch->contextBatch);
    }
}

TZrBool token_stream_flush_context_charges(SZrTokenStream *stream) {
    SZrTokenStreamBatch *batch = stream->batch;
    SZrParserComptimeBudgetTransaction *transaction;
    SZrParserComptimeBudgetUsage before;
    if (batch != ZR_NULL && batch->blockedByContextBatch) {
        return ZR_FALSE;
    }
    if (batch == ZR_NULL || !batch->ownsContextBatch ||
        batch->contextBatch.prepared) {
        return ZR_TRUE;
    }
    transaction = &batch->contextBatch.transaction;
    before = batch->shadow.usage;
    if (!ZrParser_ComptimeBudget_CommitTransaction(transaction)) {
        return ZR_FALSE;
    }
    if (batch->contextBudget != batch->budget &&
        !token_stream_commit_shadow(
                &batch->contextShadow, &before, &batch->shadow)) {
        return ZR_FALSE;
    }
    /* The stream admits native and context allocations together; a separate
     * table ledger admits only this transaction's context delta. */
    ZrParser_ComptimeBudget_BeginTransaction(&batch->shadow, transaction);
    return ZR_TRUE;
}

TZrBool token_stream_commit_shadow(
        SZrParserComptimeBudget *budget,
        const SZrParserComptimeBudgetUsage *before,
        const SZrParserComptimeBudget *shadow) {
    SZrParserComptimeBudgetTransaction transaction;
    const SZrParserComptimeBudgetUsage *after = &shadow->usage;
    struct {
        EZrParserComptimeBudgetResource resource;
        TZrUInt64 delta;
    } charges[] = {
        {ZR_PARSER_COMPTIME_BUDGET_FUEL, after->fuel - before->fuel},
        {ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH,
         after->callDepth - before->callDepth},
        {ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
         after->heapBytes - before->heapBytes},
        {ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
         after->aggregateCount - before->aggregateCount},
        {ZR_PARSER_COMPTIME_BUDGET_GENERATED_DECLARATION_COUNT,
         after->generatedDeclarationCount - before->generatedDeclarationCount},
        {ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT,
         after->diagnosticCount - before->diagnosticCount},
        {ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT,
         after->tokenCount - before->tokenCount},
        {ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT,
         after->nodeCount - before->nodeCount},
        {ZR_PARSER_COMPTIME_BUDGET_MATCHING_COUNT,
         after->matchingCount - before->matchingCount},
        {ZR_PARSER_COMPTIME_BUDGET_EXPANSION_COUNT,
         after->expansionCount - before->expansionCount}
    };

    ZrParser_ComptimeBudget_BeginTransaction(budget, &transaction);
    for (TZrSize index = 0u; index < sizeof(charges) / sizeof(charges[0]); index++) {
        if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                    &transaction, charges[index].resource, charges[index].delta)) {
            ZrParser_ComptimeBudget_AbortTransaction(&transaction);
            return ZR_FALSE;
        }
    }
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void token_stream_copy_failure(
        SZrParserComptimeBudget *budget,
        const SZrParserComptimeBudget *failed) {
    if (failed->exceededResource != ZR_PARSER_COMPTIME_BUDGET_NONE) {
        budget->exceededResource = failed->exceededResource;
        budget->exceededLimit = failed->exceededLimit;
        budget->requestedUsage = failed->requestedUsage;
    }
}

static TZrBool token_stream_batch_commit_budgets(SZrTokenStreamBatch *batch) {
    SZrParserComptimeBudget streamCandidate = *batch->budget;
    SZrParserComptimeBudget contextCandidate;
    TZrBool separateContextBudget = batch->ownsContextBatch &&
            batch->contextBudget != batch->budget;

    if (!token_stream_commit_shadow(
                &streamCandidate, &batch->initialUsage, &batch->shadow)) {
        token_stream_copy_failure(batch->budget, &streamCandidate);
        return ZR_FALSE;
    }
    if (separateContextBudget) {
        contextCandidate = *batch->contextBudget;
        if (!token_stream_commit_shadow(
                    &contextCandidate, &batch->contextInitialUsage,
                    &batch->contextShadow)) {
            token_stream_copy_failure(batch->contextBudget, &contextCandidate);
            return ZR_FALSE;
        }
    }
    /* No allocation or callback intervenes between admission and publication.
     * Only usage is published, preserving each ledger's limits and history. */
    batch->budget->usage = streamCandidate.usage;
    if (separateContextBudget) {
        batch->contextBudget->usage = contextCandidate.usage;
    }
    return ZR_TRUE;
}

static void token_stream_batch_free_sources(SZrTokenStreamBatch *batch) {
    SZrTokenStream *stream = batch->stream;

    while (stream->sourceCount > batch->sourceCount) {
        SZrTokenSourceRecord *source = &stream->sources[--stream->sourceCount];
        token_stream_free(stream, source->name, source->nameLength + 1u,
                          ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        token_stream_free(stream, source->bytes, source->byteLength + 1u,
                          ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        memset(source, 0, sizeof(*source));
    }
}

TZrBool token_stream_batch_finish(
        SZrTokenStreamBatch *batch, TZrBool success) {
    SZrTokenStream *stream = batch->stream;

    if (batch->blockedByContextBatch) {
        success = ZR_FALSE;
    }
    if (success && !token_stream_flush_context_charges(stream)) {
        success = ZR_FALSE;
    }
    if (success && batch->ownsContextBatch &&
        !ZrParser_SyntaxContext_PrepareBatch(&batch->contextBatch)) {
        success = ZR_FALSE;
    }
    if (success && !token_stream_batch_commit_budgets(batch)) {
        success = ZR_FALSE;
    }
    if (!success) {
        if (batch->ownsContextBatch) {
            ZrParser_SyntaxContext_AbortBatch(&batch->contextBatch);
        }
        while (stream->treeCount > batch->treeCount) {
            token_tree_destroy(stream, stream->trees[--stream->treeCount]);
            stream->trees[stream->treeCount] = ZR_NULL;
        }
        token_stream_batch_free_sources(batch);
        if (stream->sources != batch->sources) {
            token_stream_free(stream, stream->sources,
                              stream->sourceCapacity * sizeof(*stream->sources),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (stream->trees != batch->trees) {
            token_stream_free(stream, stream->trees,
                              stream->treeCapacity * sizeof(*stream->trees),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (stream->roots != batch->roots) {
            token_stream_free(stream, stream->roots,
                              stream->rootCapacity * sizeof(*stream->roots),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        stream->sources = batch->sources;
        stream->sourceCapacity = batch->sourceCapacity;
        stream->trees = batch->trees;
        stream->treeCapacity = batch->treeCapacity;
        stream->roots = batch->roots;
        stream->rootCount = batch->rootCount;
        stream->rootCapacity = batch->rootCapacity;
        stream->endSpan = batch->endSpan;
        token_stream_copy_failure(batch->budget, &batch->shadow);
        if (batch->ownsContextBatch && batch->contextBudget != batch->budget) {
            token_stream_copy_failure(batch->contextBudget, &batch->contextShadow);
        }
    } else {
        if (batch->ownsContextBatch) {
            ZrParser_SyntaxContext_PublishBatch(&batch->contextBatch);
        }
        if (stream->sources != batch->sources) {
            token_stream_free(stream, batch->sources,
                              batch->sourceCapacity * sizeof(*batch->sources),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (stream->trees != batch->trees) {
            token_stream_free(stream, batch->trees,
                              batch->treeCapacity * sizeof(*batch->trees),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (stream->roots != batch->roots) {
            token_stream_free(stream, batch->roots,
                              batch->rootCapacity * sizeof(*batch->roots),
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
    }
    stream->budget = batch->budget == &stream->fallbackBudget
                             ? ZR_NULL : batch->budget;
    stream->batch = ZR_NULL;
    if (batch->ownsContextBatch) {
        stream->contextTable->budget =
                batch->contextBudget == &stream->contextTable->fallbackBudget
                        ? ZR_NULL : batch->contextBudget;
    }
    return success;
}
