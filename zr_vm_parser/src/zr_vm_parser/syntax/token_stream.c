#include "syntax/token_stream_internal.h"
#include "syntax/source_identity.h"
#include "syntax/token_kind.h"

#include <limits.h>
#include <string.h>

void token_stream_default_budget(SZrParserComptimeBudget *budget) {
    SZrParserComptimeBudgetLimits limits = {0};

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

SZrParserComptimeBudget *token_stream_budget(SZrTokenStream *stream) {
    return stream->budget != ZR_NULL ? stream->budget : &stream->fallbackBudget;
}

TZrBool token_stream_size_add(
        TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || left > ZR_MAX_SIZE - right) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

TZrBool token_stream_size_mul(
        TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || (left != 0u && right > ZR_MAX_SIZE / left)) {
        return ZR_FALSE;
    }
    *out = left * right;
    return ZR_TRUE;
}

TZrBool token_stream_reserve(
        SZrTokenStream *stream,
        EZrParserComptimeBudgetResource resource1,
        TZrUInt64 amount1,
        EZrParserComptimeBudgetResource resource2,
        TZrUInt64 amount2,
        SZrParserComptimeBudgetTransaction *outTransaction) {
    SZrParserComptimeBudgetTransaction transaction;

    if (stream == ZR_NULL || outTransaction == ZR_NULL ||
        !token_stream_flush_context_charges(stream)) {
        return ZR_FALSE;
    }
    ZrParser_ComptimeBudget_BeginTransaction(
            token_stream_budget(stream), &transaction);
    if ((resource1 != ZR_PARSER_COMPTIME_BUDGET_NONE &&
         !ZrParser_ComptimeBudget_TryConsumeTransaction(
                 &transaction, resource1, amount1)) ||
        (resource2 != ZR_PARSER_COMPTIME_BUDGET_NONE &&
         !ZrParser_ComptimeBudget_TryConsumeTransaction(
                 &transaction, resource2, amount2))) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_FALSE;
    }
    *outTransaction = transaction;
    return ZR_TRUE;
}

void token_stream_abort(
        SZrParserComptimeBudgetTransaction *transaction) {
    if (transaction != ZR_NULL) {
        ZrParser_ComptimeBudget_AbortTransaction(transaction);
    }
}

TZrBool token_stream_commit(
        SZrParserComptimeBudgetTransaction *transaction) {
    if (transaction == ZR_NULL ||
        !ZrParser_ComptimeBudget_CommitTransaction(transaction)) {
        token_stream_abort(transaction);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void *token_stream_alloc(
        SZrTokenStream *stream, TZrSize size, EZrMemoryNativeType type) {
    SZrParserComptimeBudgetTransaction transaction;
    void *memory;
    if (stream == ZR_NULL || stream->global == ZR_NULL || size == 0u ||
        !token_stream_flush_context_charges(stream)) {
        return ZR_NULL;
    }
    ZrParser_ComptimeBudget_BeginTransaction(token_stream_budget(stream), &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, size)) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    memory = ZrCore_Memory_RawMallocWithType(stream->global, size, type);
    if (memory == ZR_NULL ||
        !ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        if (memory != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(stream->global, memory, size, type);
        }
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    return memory;
}

void token_stream_free(
        SZrTokenStream *stream,
        void *memory,
        TZrSize size,
        EZrMemoryNativeType type) {
    if (stream != ZR_NULL && stream->global != ZR_NULL &&
        memory != ZR_NULL && size != 0u) {
        ZrCore_Memory_RawFreeWithType(stream->global, memory, size, type);
    }
}

static TZrBool token_stream_ensure_sources(
        SZrTokenStream *stream, TZrSize required) {
    TZrSize capacity;
    TZrSize bytes;
    SZrTokenSourceRecord *sources;

    if (required <= stream->sourceCapacity) {
        return ZR_TRUE;
    }
    capacity = stream->sourceCapacity == 0u ? 8u : stream->sourceCapacity;
    while (capacity < required) {
        if (capacity > ZR_MAX_SIZE / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    if (!token_stream_size_mul(capacity, sizeof(*sources), &bytes)) {
        return ZR_FALSE;
    }
    sources = token_stream_alloc(stream, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (sources == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(sources, 0, bytes);
    if (stream->sources != ZR_NULL && stream->sourceCount != 0u) {
        memcpy(sources, stream->sources,
               stream->sourceCount * sizeof(*sources));
    }
    if (stream->batch == ZR_NULL || stream->sources != stream->batch->sources) {
        token_stream_free(stream, stream->sources,
                          stream->sourceCapacity * sizeof(*sources),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    stream->sources = sources;
    stream->sourceCapacity = capacity;
    return ZR_TRUE;
}

TZrBool token_stream_ensure_trees(
        SZrTokenStream *stream, TZrSize required) {
    TZrSize capacity;
    TZrSize bytes;
    SZrTokenTree **trees;

    if (required <= stream->treeCapacity) {
        return ZR_TRUE;
    }
    capacity = stream->treeCapacity == 0u ? 16u : stream->treeCapacity;
    while (capacity < required) {
        if (capacity > ZR_MAX_SIZE / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    if (!token_stream_size_mul(capacity, sizeof(*trees), &bytes)) {
        return ZR_FALSE;
    }
    trees = token_stream_alloc(stream, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (trees == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(trees, 0, bytes);
    if (stream->trees != ZR_NULL && stream->treeCount != 0u) {
        memcpy(trees, stream->trees, stream->treeCount * sizeof(*trees));
    }
    if (stream->batch == ZR_NULL || stream->trees != stream->batch->trees) {
        token_stream_free(stream, stream->trees,
                          stream->treeCapacity * sizeof(*trees),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    stream->trees = trees;
    stream->treeCapacity = capacity;
    return ZR_TRUE;
}

TZrBool token_stream_ensure_roots(
        SZrTokenStream *stream, TZrSize required) {
    TZrSize capacity = stream->rootCapacity == 0u ? 16u : stream->rootCapacity;
    TZrSize bytes;
    SZrTokenTree **roots;

    if (required <= stream->rootCapacity) {
        return ZR_TRUE;
    }
    while (capacity < required) {
        if (capacity > ZR_MAX_SIZE / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    if (!token_stream_size_mul(capacity, sizeof(*roots), &bytes)) {
        return ZR_FALSE;
    }
    roots = token_stream_alloc(stream, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (roots == ZR_NULL) {
        return ZR_FALSE;
    }
    if (stream->rootCount != 0u) {
        memcpy(roots, stream->roots, stream->rootCount * sizeof(*roots));
    }
    if (stream->batch == ZR_NULL || stream->roots != stream->batch->roots) {
        token_stream_free(stream, stream->roots,
                          stream->rootCapacity * sizeof(*roots),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    stream->roots = roots;
    stream->rootCapacity = capacity;
    return ZR_TRUE;
}

TZrBool token_stream_valid_source_text(
        const SZrTokenStream *stream, SZrTokenText text) {
    const SZrTokenSourceRecord *source;

    if (stream == ZR_NULL || text.sourceId == ZR_TOKEN_SOURCE_INVALID ||
        text.sourceId > stream->sourceCount) {
        return ZR_FALSE;
    }
    source = &stream->sources[text.sourceId - 1u];
    return text.offset <= source->byteLength &&
           text.length <= source->byteLength - text.offset;
}

TZrBool token_stream_valid_span(
        const SZrTokenStream *stream, SZrTokenSpan span) {
    SZrTokenText text;

    if (!ZrParser_TokenSpan_IsValid(span)) {
        return ZR_FALSE;
    }
    text.sourceId = span.sourceId;
    text.offset = span.startOffset;
    text.length = span.endOffset - span.startOffset;
    return token_stream_valid_source_text(stream, text);
}

static TZrBool token_stream_copy_bytes(
        SZrTokenStream *stream,
        const TZrChar *bytes,
        TZrSize length,
        TZrChar **outCopy) {
    TZrChar *copy;
    TZrSize allocationSize;

    if (outCopy == ZR_NULL || (length != 0u && bytes == ZR_NULL) ||
        !token_stream_size_add(length, 1u, &allocationSize)) {
        return ZR_FALSE;
    }
    copy = token_stream_alloc(stream, allocationSize, ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
    if (copy == ZR_NULL) {
        return ZR_FALSE;
    }
    if (length != 0u) {
        memcpy(copy, bytes, length);
    }
    copy[length] = '\0';
    *outCopy = copy;
    return ZR_TRUE;
}

SZrTokenStream *ZrParser_TokenStream_Create(
        SZrGlobalState *global,
        SZrSyntaxContextTable *contextTable,
        SZrParserComptimeBudget *budget) {
    SZrTokenStream *stream;
    SZrParserComptimeBudget fallback;
    SZrParserComptimeBudgetTransaction transaction;

    if (global == ZR_NULL || contextTable == ZR_NULL ||
        global != contextTable->global || contextTable->activeTransaction != ZR_NULL) {
        return ZR_NULL;
    }
    if (budget == ZR_NULL) {
        token_stream_default_budget(&fallback);
    }
    ZrParser_ComptimeBudget_BeginTransaction(
            budget != ZR_NULL ? budget : &fallback, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, sizeof(*stream))) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    stream = ZrCore_Memory_RawMallocWithType(
            global, sizeof(*stream), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (stream == ZR_NULL) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        return ZR_NULL;
    }
    memset(stream, 0, sizeof(*stream));
    stream->global = global;
    stream->refCount = 1u;
    stream->budget = budget;
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        token_stream_free(stream, stream, sizeof(*stream),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }
    if (budget == ZR_NULL) {
        stream->fallbackBudget = fallback;
    }
    stream->contextTable = contextTable;
    ZrParser_SyntaxContextTable_Retain(contextTable);
    return stream;
}

void ZrParser_TokenStream_Retain(SZrTokenStream *stream) {
    if (stream != ZR_NULL && stream->refCount != (TZrUInt32)-1) {
        stream->refCount++;
    }
}

void token_tree_destroy(SZrTokenStream *stream, SZrTokenTree *tree) {
    TZrSize childBytes;
    TZrSize triviaBytes;

    if (stream == ZR_NULL || tree == ZR_NULL) {
        return;
    }
    if (tree->children != ZR_NULL &&
        token_stream_size_mul(tree->childCount,
                              sizeof(*tree->children), &childBytes)) {
        token_stream_free(stream, tree->children, childBytes,
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (tree->leading != ZR_NULL &&
        token_stream_size_mul(tree->leadingCount,
                              sizeof(*tree->leading), &triviaBytes)) {
        token_stream_free(stream, tree->leading, triviaBytes,
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (tree->trailing != ZR_NULL &&
        token_stream_size_mul(tree->trailingCount,
                              sizeof(*tree->trailing), &triviaBytes)) {
        token_stream_free(stream, tree->trailing, triviaBytes,
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    token_stream_free(stream, tree->innerTrivia,
                      tree->innerTriviaCount * sizeof(*tree->innerTrivia),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    token_stream_free(stream, tree, sizeof(*tree), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

void ZrParser_TokenStream_Release(SZrTokenStream *stream) {
    if (stream == ZR_NULL || stream->refCount == 0u) {
        return;
    }
    stream->refCount--;
    if (stream->refCount != 0u) {
        return;
    }
    for (TZrSize index = 0u; index < stream->treeCount; index++) {
        token_tree_destroy(stream, stream->trees[index]);
    }
    if (stream->trees != ZR_NULL) {
        token_stream_free(stream, stream->trees,
                          stream->treeCapacity * sizeof(*stream->trees),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    token_stream_free(stream, stream->roots,
                      stream->rootCapacity * sizeof(*stream->roots),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    token_stream_free(stream, stream->emptyTrivia,
                      stream->emptyTriviaCount * sizeof(*stream->emptyTrivia),
                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    for (TZrSize index = 0u; index < stream->sourceCount; index++) {
        if (stream->sources[index].name != ZR_NULL) {
            token_stream_free(stream, stream->sources[index].name,
                              stream->sources[index].nameLength + 1u,
                              ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        }
        if (stream->sources[index].bytes != ZR_NULL) {
            token_stream_free(stream, stream->sources[index].bytes,
                              stream->sources[index].byteLength + 1u,
                              ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        }
    }
    if (stream->sources != ZR_NULL) {
        token_stream_free(stream, stream->sources,
                          stream->sourceCapacity * sizeof(*stream->sources),
                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrParser_SyntaxContextTable_Release(stream->contextTable);
    token_stream_free(stream, stream, sizeof(*stream), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

TZrBool ZrParser_TokenStream_Seal(SZrTokenStream *stream) {
    if (stream == ZR_NULL || stream->sealed) {
        return stream != ZR_NULL && stream->sealed;
    }
    stream->fallbackBudget = *token_stream_budget(stream);
    stream->budget = ZR_NULL;
    stream->sealed = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_TokenStream_IsSealed(const SZrTokenStream *stream) {
    return stream != ZR_NULL && stream->sealed;
}

SZrSyntaxContextTable *ZrParser_TokenStream_ContextTable(
        const SZrTokenStream *stream) {
    return stream == ZR_NULL ? ZR_NULL : stream->contextTable;
}

static TZrBool ZrParser_TokenStream_AddSource_impl(
        SZrTokenStream *stream,
        const TZrChar *sourceName,
        TZrSize sourceNameLength,
        const TZrChar *bytes,
        TZrSize byteLength,
        TZrTokenSourceId *outSourceId, TZrUInt64 originKey) {
    SZrParserComptimeBudgetTransaction transaction;
    SZrTokenSourceRecord *source;
    TZrChar *nameCopy = ZR_NULL;
    TZrChar *byteCopy = ZR_NULL;
    TZrSize heapBytes = 0u;

    if (stream == ZR_NULL || stream->sealed || outSourceId == ZR_NULL ||
        (sourceNameLength != 0u && sourceName == ZR_NULL) ||
        (byteLength != 0u && bytes == ZR_NULL) ||
        stream->sourceCount >= (TZrSize)UINT32_MAX) {
        return ZR_FALSE;
    }
    if (!token_stream_size_add(sourceNameLength, 1u, &heapBytes) ||
        !token_stream_size_add(heapBytes, byteLength, &heapBytes) ||
        !token_stream_size_add(heapBytes, 1u, &heapBytes)) {
        return ZR_FALSE;
    }
    if (originKey == 0U && !ZrParser_SourceIdentity_Issue(&originKey)) {
        return ZR_FALSE;
    }
    if (!token_stream_reserve(stream, ZR_PARSER_COMPTIME_BUDGET_NONE, 0u,
                              ZR_PARSER_COMPTIME_BUDGET_NONE, 0u,
                              &transaction)) {
        return ZR_FALSE;
    }
    if (!token_stream_copy_bytes(stream, sourceName, sourceNameLength,
                                 &nameCopy) ||
        !token_stream_copy_bytes(stream, bytes, byteLength, &byteCopy) ||
        !token_stream_ensure_sources(stream, stream->sourceCount + 1u)) {
        token_stream_free(stream, nameCopy, sourceNameLength + 1u,
                          ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        token_stream_free(stream, byteCopy, byteLength + 1u,
                          ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        token_stream_abort(&transaction);
        return ZR_FALSE;
    }
    source = &stream->sources[stream->sourceCount];
    source->originKey = originKey;
    source->name = nameCopy;
    source->nameLength = sourceNameLength;
    source->bytes = byteCopy;
    source->byteLength = byteLength;
    stream->sourceCount++;
    if (!token_stream_commit(&transaction)) {
        stream->sourceCount--;
        token_stream_free(stream, nameCopy, sourceNameLength + 1u,
                          ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        token_stream_free(stream, byteCopy, byteLength + 1u,
                          ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        memset(source, 0, sizeof(*source));
        return ZR_FALSE;
    }
    *outSourceId = (TZrTokenSourceId)stream->sourceCount;
    return ZR_TRUE;
}

static TZrBool token_stream_add_source_with_origin(
        SZrTokenStream *stream,
        const TZrChar *sourceName,
        TZrSize sourceNameLength,
        const TZrChar *bytes,
        TZrSize byteLength,
        TZrTokenSourceId *outSourceId, TZrUInt64 originKey) {
    SZrTokenStreamBatch batch;
    TZrBool ownBatch;
    TZrBool success;
    TZrTokenSourceId result;

    if (stream == ZR_NULL || stream->sealed || outSourceId == ZR_NULL) {
        return ZR_FALSE;
    }
    ownBatch = stream->batch == ZR_NULL;
    if (ownBatch) {
        token_stream_batch_begin(stream, &batch);
    }
    success = ZrParser_TokenStream_AddSource_impl(
            stream, sourceName, sourceNameLength, bytes, byteLength, &result, originKey);
    if (ownBatch) {
        success = token_stream_batch_finish(&batch, success);
    }
    if (success) {
        *outSourceId = result;
    }
    return success;
}

TZrBool ZrParser_TokenStream_AddSource(
        SZrTokenStream *stream, const TZrChar *sourceName, TZrSize sourceNameLength,
        const TZrChar *bytes, TZrSize byteLength, TZrTokenSourceId *outSourceId) {
    return token_stream_add_source_with_origin(stream, sourceName, sourceNameLength,
                                               bytes, byteLength, outSourceId, 0U);
}

TZrBool token_stream_import_source(
        SZrTokenStream *stream, const SZrTokenSourceRecord *record,
        TZrTokenSourceId *outSourceId) {
    return record != ZR_NULL && record->originKey != 0U &&
           token_stream_add_source_with_origin(stream, record->name, record->nameLength,
                   record->bytes, record->byteLength, outSourceId, record->originKey);
}

TZrBool token_stream_source_origin(
        const SZrTokenStream *stream, TZrTokenSourceId sourceId, TZrUInt64 *outOrigin) {
    if (outOrigin == ZR_NULL) {
        return ZR_FALSE;
    }
    *outOrigin = 0U;
    if (stream == ZR_NULL || sourceId == ZR_TOKEN_SOURCE_INVALID || sourceId > stream->sourceCount) {
        return ZR_FALSE;
    }
    *outOrigin = stream->sources[sourceId - 1U].originKey;
    return *outOrigin != 0U;
}



TZrBool ZrParser_TokenStream_CopyText(
        SZrTokenStream *stream,
        const TZrChar *bytes,
        TZrSize byteLength,
        SZrTokenText *outText) {
    TZrTokenSourceId sourceId;

    if (outText == ZR_NULL || !ZrParser_TokenStream_AddSource(
                stream, ZR_NULL, 0u, bytes, byteLength, &sourceId)) {
        return ZR_FALSE;
    }
    outText->sourceId = sourceId;
    outText->offset = 0u;
    outText->length = byteLength;
    return ZR_TRUE;
}

const TZrChar *ZrParser_TokenStream_SourceBytes(
        const SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize *outLength) {
    const SZrTokenSourceRecord *source;

    if (outLength != ZR_NULL) {
        *outLength = 0u;
    }
    if (stream == ZR_NULL || sourceId == ZR_TOKEN_SOURCE_INVALID ||
        sourceId > stream->sourceCount) {
        return ZR_NULL;
    }
    source = &stream->sources[sourceId - 1u];
    if (outLength != ZR_NULL) {
        *outLength = source->byteLength;
    }
    return source->bytes;
}

const TZrChar *ZrParser_TokenStream_SourceName(
        const SZrTokenStream *stream,
        TZrTokenSourceId sourceId,
        TZrSize *outLength) {
    const SZrTokenSourceRecord *source;

    if (outLength != ZR_NULL) {
        *outLength = 0u;
    }
    if (stream == ZR_NULL || sourceId == ZR_TOKEN_SOURCE_INVALID ||
        sourceId > stream->sourceCount) {
        return ZR_NULL;
    }
    source = &stream->sources[sourceId - 1u];
    if (outLength != ZR_NULL) {
        *outLength = source->nameLength;
    }
    return source->name;
}

const TZrChar *ZrParser_TokenStream_TextBytes(
        const SZrTokenStream *stream, SZrTokenText text) {
    const TZrChar *bytes;
    TZrSize length;

    if (!token_stream_valid_source_text(stream, text)) {
        return ZR_NULL;
    }
    bytes = ZrParser_TokenStream_SourceBytes(stream, text.sourceId, &length);
    if (bytes == ZR_NULL || text.offset > length) {
        return ZR_NULL;
    }
    return bytes + text.offset;
}

TZrBool token_stream_copy_trivia(
        SZrTokenStream *stream,
        const SZrTokenTrivia *input,
        TZrSize count,
        SZrTokenTrivia **outCopy) {
    TZrSize bytes;

    if (outCopy == ZR_NULL || (count != 0u && input == ZR_NULL) ||
        !token_stream_size_mul(count, sizeof(*input), &bytes)) {
        return ZR_FALSE;
    }
    *outCopy = ZR_NULL;
    if (count == 0u) {
        return ZR_TRUE;
    }
    *outCopy = token_stream_alloc(stream, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (*outCopy == ZR_NULL) {
        return ZR_FALSE;
    }
    memcpy(*outCopy, input, bytes);
    for (TZrSize index = 0u; index < count; index++) {
        if (input[index].kind < ZR_TOKEN_TRIVIA_WHITESPACE ||
            input[index].kind > ZR_TOKEN_TRIVIA_OTHER ||
            !token_stream_valid_source_text(stream, input[index].text)) {
            token_stream_free(stream, *outCopy, bytes,
                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            *outCopy = ZR_NULL;
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool token_stream_context_valid(
        const SZrTokenStream *stream, SZrSyntaxContext context) {
    return stream != ZR_NULL &&
           (context.table == ZR_NULL || context.table == stream->contextTable) &&
           ZrParser_SyntaxContext_IsValid(context);
}

static TZrBool token_stream_append_leaf_impl(
        SZrTokenStream *stream,
        EZrTokenTreeKind kind,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        TZrUInt32 punctCodepoint,
        EZrTokenSpacing punctSpacing,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    SZrParserComptimeBudgetTransaction transaction;
    SZrTokenTree *tree;
    TZrSize leadingBytes;
    TZrSize trailingBytes;
    TZrSize treeBytes;
    TZrSize heapBytes;

    if (stream == ZR_NULL || stream->sealed || outIndex == ZR_NULL ||
        (kind == ZR_TOKEN_TREE_IDENT &&
         (semInfo.token < ZR_FIRST_RESERVED || semInfo.token > ZR_TK_DROP ||
          semInfo.token == ZR_TK_EOS || ZrParser_Token_IsLiteral(semInfo.token) ||
          ZrParser_Token_IsPunct(semInfo.token))) ||
        (kind == ZR_TOKEN_TREE_LITERAL && !ZrParser_Token_IsLiteral(semInfo.token)) ||
        (kind == ZR_TOKEN_TREE_PUNCT &&
         (semInfo.token != ZrParser_Token_SinglePunct(punctCodepoint) ||
          punctCodepoint >= ZR_FIRST_RESERVED || punctCodepoint == 0u ||
          strchr("!#$%&'*+,-./:;<=>?@^|~", (int)punctCodepoint) == ZR_NULL ||
          rawText.length != 1u ||
          (punctSpacing != ZR_TOKEN_SPACING_ALONE &&
           punctSpacing != ZR_TOKEN_SPACING_JOINT))) ||
        ((semInfo.token == ZR_TK_IDENTIFIER || semInfo.token == ZR_TK_STRING ||
          semInfo.token == ZR_TK_TEMPLATE_STRING) &&
         semInfo.text.sourceId == ZR_TOKEN_SOURCE_INVALID) ||
        (semInfo.text.sourceId != ZR_TOKEN_SOURCE_INVALID
                 ? !token_stream_valid_source_text(stream, semInfo.text)
                 : (semInfo.text.offset != 0u || semInfo.text.length != 0u)) ||
        (semInfo.suffix.sourceId != ZR_TOKEN_SOURCE_INVALID
                 ? !token_stream_valid_source_text(stream, semInfo.suffix)
                 : (semInfo.suffix.offset != 0u || semInfo.suffix.length != 0u)) ||
        (kind != ZR_TOKEN_TREE_IDENT && kind != ZR_TOKEN_TREE_PUNCT &&
         kind != ZR_TOKEN_TREE_LITERAL) ||
        !token_stream_valid_source_text(stream, rawText) ||
        !token_stream_valid_span(stream, span) ||
        !token_stream_context_valid(stream, context) ||
        !token_stream_size_mul(leadingCount, sizeof(*leading), &leadingBytes) ||
        !token_stream_size_mul(trailingCount, sizeof(*trailing), &trailingBytes) ||
        !token_stream_size_add(sizeof(*tree), leadingBytes, &treeBytes) ||
        !token_stream_size_add(treeBytes, trailingBytes, &heapBytes)) {
        return ZR_FALSE;
    }
    if (kind == ZR_TOKEN_TREE_PUNCT &&
        (TZrUInt8)ZrParser_TokenStream_TextBytes(stream, rawText)[0] != punctCodepoint) {
        return ZR_FALSE;
    }
    if (!token_stream_reserve(stream,
                              ZR_PARSER_COMPTIME_BUDGET_NODE_COUNT, 1u,
                              ZR_PARSER_COMPTIME_BUDGET_TOKEN_COUNT, 1u,
                              &transaction)) {
        return ZR_FALSE;
    }
    tree = token_stream_alloc(stream, sizeof(*tree), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (tree == ZR_NULL) {
        token_stream_abort(&transaction);
        return ZR_FALSE;
    }
    memset(tree, 0, sizeof(*tree));
    tree->owner = stream;
    tree->kind = kind;
    tree->span = span;
    tree->context = context.table == ZR_NULL
                            ? ZrParser_SyntaxContext_TableRoot(stream->contextTable)
                            : context;
    tree->semInfo = semInfo;
    tree->rawText = rawText;
    tree->punctCodepoint = punctCodepoint;
    tree->punctSpacing = punctSpacing;
    tree->leadingCount = leadingCount;
    tree->trailingCount = trailingCount;
    if (!token_stream_copy_trivia(stream, leading, leadingCount,
                                  &tree->leading) ||
        !token_stream_copy_trivia(stream, trailing, trailingCount,
                                  &tree->trailing) ||
        !token_stream_ensure_trees(stream, stream->treeCount + 1u) ||
        !token_stream_ensure_roots(stream, stream->rootCount + 1u)) {
        token_tree_destroy(stream, tree);
        token_stream_abort(&transaction);
        return ZR_FALSE;
    }
    if (!token_stream_commit(&transaction)) {
        token_tree_destroy(stream, tree);
        return ZR_FALSE;
    }
    stream->trees[stream->treeCount++] = tree;
    stream->roots[stream->rootCount] = tree;
    *outIndex = stream->rootCount++;
    return ZR_TRUE;
}

static TZrBool token_stream_append_leaf(
        SZrTokenStream *stream,
        EZrTokenTreeKind kind,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        TZrUInt32 punctCodepoint,
        EZrTokenSpacing punctSpacing,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    SZrTokenStreamBatch batch;
    TZrBool ownBatch;
    TZrBool success;
    TZrSize result;

    if (stream == ZR_NULL || stream->sealed || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }
    ownBatch = stream->batch == ZR_NULL;
    if (ownBatch) {
        token_stream_batch_begin(stream, &batch);
    }
    success = token_stream_append_leaf_impl(
            stream, kind, semInfo, rawText, punctCodepoint, punctSpacing, span, context, leading, leadingCount, trailing, trailingCount, &result);
    if (ownBatch) {
        success = token_stream_batch_finish(&batch, success);
    }
    if (success) {
        *outIndex = result;
    }
    return success;
}



TZrBool ZrParser_TokenStream_AppendIdent(
        SZrTokenStream *stream,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    return token_stream_append_leaf(
            stream, ZR_TOKEN_TREE_IDENT, semInfo, rawText, 0u,
            ZR_TOKEN_SPACING_ALONE, span, context, leading, leadingCount,
            trailing, trailingCount, outIndex);
}

TZrBool ZrParser_TokenStream_AppendPunct(
        SZrTokenStream *stream,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        TZrUInt32 codepoint,
        EZrTokenSpacing spacing,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    return token_stream_append_leaf(
            stream, ZR_TOKEN_TREE_PUNCT, semInfo, rawText, codepoint, spacing,
            span, context, leading, leadingCount, trailing, trailingCount,
            outIndex);
}

TZrBool ZrParser_TokenStream_AppendLiteral(
        SZrTokenStream *stream,
        SZrTokenNativeSemInfo semInfo,
        SZrTokenText rawText,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex) {
    return token_stream_append_leaf(
            stream, ZR_TOKEN_TREE_LITERAL, semInfo, rawText, 0u,
            ZR_TOKEN_SPACING_ALONE, span, context, leading, leadingCount,
            trailing, trailingCount, outIndex);
}
