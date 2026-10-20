#ifndef ZR_VM_PARSER_TOKEN_STREAM_INTERNAL_H
#define ZR_VM_PARSER_TOKEN_STREAM_INTERNAL_H

#include "zr_vm_parser/token_stream.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "syntax/syntax_context_internal.h"

typedef struct SZrTokenSourceRecord {
    TZrUInt64 originKey;
    TZrChar *name;
    TZrSize nameLength;
    TZrChar *bytes;
    TZrSize byteLength;
} SZrTokenSourceRecord;

TZrBool token_stream_import_source(
        SZrTokenStream *stream, const SZrTokenSourceRecord *record,
        TZrTokenSourceId *outSourceId);
TZrBool token_stream_source_origin(
        const SZrTokenStream *stream, TZrTokenSourceId sourceId,
        TZrUInt64 *outOrigin);

struct SZrTokenTree {
    struct SZrTokenStream *owner;
    EZrTokenTreeKind kind;
    SZrTokenSpan span;
    SZrSyntaxContext context;
    SZrTokenNativeSemInfo semInfo;
    SZrTokenText rawText;
    TZrUInt32 punctCodepoint;
    EZrTokenSpacing punctSpacing;
    EZrTokenDelimiter delimiter;
    SZrTokenSpan openSpan;
    SZrTokenSpan closeSpan;
    struct SZrTokenTree **children;
    TZrSize childCount;
    SZrTokenTrivia *leading;
    TZrSize leadingCount;
    SZrTokenTrivia *trailing;
    TZrSize trailingCount;
    SZrTokenTrivia *innerTrivia;
    TZrSize innerTriviaCount;
};

typedef struct SZrTokenStreamBatch SZrTokenStreamBatch;

struct SZrTokenStream {
    SZrGlobalState *global;
    TZrUInt32 refCount;
    TZrBool sealed;
    SZrSyntaxContextTable *contextTable;
    SZrParserComptimeBudget *budget;
    SZrParserComptimeBudget fallbackBudget;
    SZrTokenSourceRecord *sources;
    TZrSize sourceCount;
    TZrSize sourceCapacity;
    SZrTokenTree **trees;
    TZrSize treeCount;
    TZrSize treeCapacity;
    SZrTokenTree **roots;
    TZrSize rootCount;
    TZrSize rootCapacity;
    SZrTokenSpan endSpan;
    SZrTokenTrivia *emptyTrivia;
    TZrSize emptyTriviaCount;
    SZrTokenStreamBatch *batch;
};

struct SZrTokenStreamBatch {
    SZrTokenStream *stream;
    SZrParserComptimeBudget *budget;
    SZrParserComptimeBudget shadow;
    SZrParserComptimeBudgetUsage initialUsage;
    SZrTokenSourceRecord *sources;
    TZrSize sourceCount;
    TZrSize sourceCapacity;
    SZrTokenTree **trees;
    TZrSize treeCount;
    TZrSize treeCapacity;
    SZrTokenTree **roots;
    TZrSize rootCount;
    TZrSize rootCapacity;
    SZrTokenSpan endSpan;
    SZrSyntaxContextBatch contextBatch;
    SZrParserComptimeBudget *contextBudget;
    SZrParserComptimeBudget contextShadow;
    SZrParserComptimeBudgetUsage contextInitialUsage;
    TZrBool ownsContextBatch;
    TZrBool blockedByContextBatch;
};

void token_stream_batch_begin(
        SZrTokenStream *stream, SZrTokenStreamBatch *batch);
TZrBool token_stream_batch_finish(
        SZrTokenStreamBatch *batch, TZrBool success);
TZrBool token_stream_flush_context_charges(SZrTokenStream *stream);
TZrBool token_stream_commit_shadow(
        SZrParserComptimeBudget *budget,
        const SZrParserComptimeBudgetUsage *before,
        const SZrParserComptimeBudget *shadow);
void token_stream_default_budget(SZrParserComptimeBudget *budget);

SZrParserComptimeBudget *token_stream_budget(SZrTokenStream *stream);

TZrBool token_stream_size_add(
        TZrSize left, TZrSize right, TZrSize *out);

TZrBool token_stream_size_mul(
        TZrSize left, TZrSize right, TZrSize *out);

TZrBool token_stream_reserve(
        SZrTokenStream *stream,
        EZrParserComptimeBudgetResource resource1,
        TZrUInt64 amount1,
        EZrParserComptimeBudgetResource resource2,
        TZrUInt64 amount2,
        SZrParserComptimeBudgetTransaction *outTransaction);

void token_stream_abort(
        SZrParserComptimeBudgetTransaction *transaction);

TZrBool token_stream_commit(
        SZrParserComptimeBudgetTransaction *transaction);

void *token_stream_alloc(
        SZrTokenStream *stream, TZrSize size, EZrMemoryNativeType type);

void token_stream_free(
        SZrTokenStream *stream,
        void *memory,
        TZrSize size,
        EZrMemoryNativeType type);

TZrBool token_stream_ensure_trees(
        SZrTokenStream *stream, TZrSize required);

TZrBool token_stream_ensure_roots(
        SZrTokenStream *stream, TZrSize required);

TZrBool token_stream_valid_source_text(
        const SZrTokenStream *stream, SZrTokenText text);

TZrBool token_stream_valid_span(
        const SZrTokenStream *stream, SZrTokenSpan span);

TZrBool token_stream_copy_trivia(
        SZrTokenStream *stream,
        const SZrTokenTrivia *input,
        TZrSize count,
        SZrTokenTrivia **outCopy);

TZrBool token_stream_context_valid(
        const SZrTokenStream *stream, SZrSyntaxContext context);

void token_tree_destroy(SZrTokenStream *stream, SZrTokenTree *tree);

TZrBool token_stream_make_group(
        SZrTokenStream *stream,
        EZrTokenDelimiter delimiter,
        SZrTokenSpan openSpan,
        SZrTokenSpan closeSpan,
        SZrTokenTree *const *children,
        TZrSize childCount,
        SZrTokenSpan span,
        SZrSyntaxContext context,
        const SZrTokenTrivia *leading,
        TZrSize leadingCount,
        const SZrTokenTrivia *trailing,
        TZrSize trailingCount,
        TZrSize *outIndex);

#endif
