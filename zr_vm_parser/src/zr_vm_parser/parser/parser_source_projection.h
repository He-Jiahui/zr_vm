#ifndef ZR_VM_PARSER_SOURCE_PROJECTION_H
#define ZR_VM_PARSER_SOURCE_PROJECTION_H

#include "zr_vm_parser/parser.h"
#include "parser/parser_range_evidence.h"

typedef struct SZrParserSourceNameEntry {
    TZrTokenSourceId sourceId;
    SZrString *name;
} SZrParserSourceNameEntry;

/* Native cursor storage borrows strings rooted by the AST ownership object. */
typedef struct SZrParserSourceNameCache {
    SZrParserSourceNameEntry *entries;
    TZrSize count;
    TZrSize capacity;
} SZrParserSourceNameCache;

TZrBool ZrParser_SourceProjection_CacheSpan(
        SZrLexState *lexer, const SZrTokenStream *stream,
        SZrParserSourceNameCache *cache, SZrTokenSpan span);
TZrBool ZrParser_SourceProjection_FindName(
        const SZrParserSourceNameCache *cache, TZrTokenSourceId sourceId,
        SZrString **outName);
void ZrParser_SourceProjection_FreeCache(
        SZrLexState *lexer, SZrParserSourceNameCache *cache);
TZrBool ZrParser_SourceProjection_AppendSpanEvidence(
        SZrLexState *lexer, const SZrTokenStream *stream,
        SZrParserSourceNameCache *cache, SZrParserRangeEvidence *stage,
        SZrTokenSpan span, TZrBool absent);
TZrBool ZrParser_SourceProjection_FailLexer(SZrLexState *lexer);

/* Source selection is allocation-free after TokenSource initialization. */
TZrBool ZrParser_SourceProjection_SourceName(
        SZrParserState *ps, SZrTokenSpan origin, SZrString **outName);
SZrFileRange ZrParser_SourceProjection_CurrentLocation(SZrParserState *ps);
TZrBool ZrParser_SourceProjection_FirstPunctPieceLocation(
        SZrParserState *ps, SZrFileRange *outRange);
TZrBool ZrParser_MergeLocations(
        SZrParserState *ps, SZrFileRange a, SZrFileRange b, SZrFileRange *outRange);

#endif
