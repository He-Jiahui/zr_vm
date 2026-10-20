#include "parser/parser_source_projection.h"
#include "parser/parser_ownership.h"
#include "syntax/token_source.h"
#include "syntax/token_stream_internal.h"

#include <stdint.h>
#include <string.h>

enum {
    ZR_PARSER_SOURCE_NAME_INITIAL_CAPACITY = 8
};

static TZrSize source_projection_slot(TZrTokenSourceId sourceId, TZrSize capacity) {
    return (TZrSize)sourceId & (capacity - 1U);
}

TZrBool ZrParser_SourceProjection_FindName(
        const SZrParserSourceNameCache *cache, TZrTokenSourceId sourceId,
        SZrString **outName) {
    TZrSize slot;

    if (outName == ZR_NULL) {
        return ZR_FALSE;
    }
    *outName = ZR_NULL;
    if (cache == ZR_NULL || sourceId == ZR_TOKEN_SOURCE_INVALID ||
        cache->entries == ZR_NULL || cache->capacity == 0U) {
        return ZR_FALSE;
    }
    slot = source_projection_slot(sourceId, cache->capacity);
    for (TZrSize probes = 0U; probes < cache->capacity; probes++) {
        const SZrParserSourceNameEntry *entry = &cache->entries[slot];
        if (entry->sourceId == ZR_TOKEN_SOURCE_INVALID) {
            return ZR_FALSE;
        }
        if (entry->sourceId == sourceId) {
            *outName = entry->name;
            return entry->name != ZR_NULL;
        }
        slot = (slot + 1U) & (cache->capacity - 1U);
    }
    return ZR_FALSE;
}

static TZrBool source_projection_reserve(
        SZrLexState *lexer, SZrParserSourceNameCache *cache) {
    SZrParserSourceNameEntry *entries;
    TZrSize capacity;

    if (cache->capacity != 0U && cache->count < cache->capacity / 2U) {
        return ZR_TRUE;
    }
    if (cache->capacity > ZR_MAX_SIZE / 2U) {
        return ZR_FALSE;
    }
    capacity = cache->capacity == 0U
                       ? ZR_PARSER_SOURCE_NAME_INITIAL_CAPACITY
                       : cache->capacity * 2U;
    if (capacity > ZR_MAX_SIZE / sizeof(*entries)) {
        return ZR_FALSE;
    }
    entries = ZrParser_Ownership_Allocate(
            lexer->ownership, capacity * sizeof(*entries),
            ZR_MEMORY_NATIVE_TYPE_ARRAY, ZR_FALSE);
    if (entries == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(entries, 0, capacity * sizeof(*entries));
    for (TZrSize index = 0U; index < cache->capacity; index++) {
        SZrParserSourceNameEntry entry = cache->entries[index];
        if (entry.sourceId != ZR_TOKEN_SOURCE_INVALID) {
            TZrSize slot = source_projection_slot(entry.sourceId, capacity);
            while (entries[slot].sourceId != ZR_TOKEN_SOURCE_INVALID) {
                slot = (slot + 1U) & (capacity - 1U);
            }
            entries[slot] = entry;
        }
    }
    if (cache->entries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                lexer->state->global, cache->entries,
                cache->capacity * sizeof(*cache->entries), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    cache->entries = entries;
    cache->capacity = capacity;
    return ZR_TRUE;
}

TZrBool ZrParser_SourceProjection_CacheSpan(
        SZrLexState *lexer, const SZrTokenStream *stream,
        SZrParserSourceNameCache *cache, SZrTokenSpan span) {
    const TZrChar *bytes;
    TZrSize length = 0U;
    TZrSize slot;
    SZrString *name;

    if (lexer == ZR_NULL || lexer->state == ZR_NULL || stream == ZR_NULL ||
        cache == ZR_NULL || !ZrParser_TokenSpan_IsValid(span)) {
        return ZR_FALSE;
    }
    if (ZrParser_SourceProjection_FindName(cache, span.sourceId, &name)) {
        return ZR_TRUE;
    }
    bytes = ZrParser_TokenStream_SourceName(stream, span.sourceId, &length);
    if (bytes == ZR_NULL || !source_projection_reserve(lexer, cache)) {
        return ZR_FALSE;
    }
    name = ZrParser_Lexer_CreateString(lexer, bytes, length);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    slot = source_projection_slot(span.sourceId, cache->capacity);
    while (cache->entries[slot].sourceId != ZR_TOKEN_SOURCE_INVALID) {
        slot = (slot + 1U) & (cache->capacity - 1U);
    }
    cache->entries[slot].sourceId = span.sourceId;
    cache->entries[slot].name = name;
    cache->count++;
    return ZR_TRUE;
}

void ZrParser_SourceProjection_FreeCache(
        SZrLexState *lexer, SZrParserSourceNameCache *cache) {
    if (cache == ZR_NULL) {
        return;
    }
    if (cache->entries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                lexer->state->global, cache->entries,
                cache->capacity * sizeof(*cache->entries), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    memset(cache, 0, sizeof(*cache));
}

TZrBool ZrParser_SourceProjection_AppendSpanEvidence(
        SZrLexState *lexer, const SZrTokenStream *stream,
        SZrParserSourceNameCache *cache, SZrParserRangeEvidence *stage,
        SZrTokenSpan span, TZrBool absent) {
    SZrString *name = ZR_NULL;
    TZrUInt64 originKey = 0U;
    if (!absent && (!ZrParser_SourceProjection_CacheSpan(lexer, stream, cache, span) ||
                    !ZrParser_SourceProjection_FindName(cache, span.sourceId, &name) ||
                    !token_stream_source_origin(stream, span.sourceId, &originKey))) {
        return ZR_FALSE;
    }
    return ZrParser_RangeEvidence_Append(stage, span, originKey, name, absent);
}

TZrBool ZrParser_SourceProjection_FailLexer(SZrLexState *lexer) {
    if (lexer != ZR_NULL) {
        lexer->hasSourceEvidenceError = ZR_TRUE;
        lexer->t.hasLexError = ZR_TRUE;
        lexer->t.lexErrorMessage = "Invalid token source evidence";
    }
    return ZR_FALSE;
}

static TZrBool source_projection_fail(SZrParserState *ps) {
    if (ps != ZR_NULL) {
        ps->hasError = ZR_TRUE;
        ps->hasFatalError = ZR_TRUE;
        ps->errorMessage = "Invalid token source evidence";
        if (ps->lexer != ZR_NULL) {
            ZrParser_SourceProjection_FailLexer(ps->lexer);
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_SourceProjection_SourceName(
        SZrParserState *ps, SZrTokenSpan origin, SZrString **outName) {
    if (outName == ZR_NULL) {
        return source_projection_fail(ps);
    }
    *outName = ZR_NULL;
    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return source_projection_fail(ps);
    }
    if (ZrParser_TokenSource_IsStream(ps->lexer)) {
        if (!ZrParser_TokenSpan_IsValid(origin) ||
            !ZrParser_TokenSource_SourceName(ps->lexer, origin.sourceId, outName)) {
            return source_projection_fail(ps);
        }
        return ZR_TRUE;
    }
    if (ps->lexer->hasInheritedSpan &&
        (!ZrParser_TokenSpan_IsValid(origin) ||
         origin.sourceId != ps->lexer->inheritedSpan.sourceId)) {
        return source_projection_fail(ps);
    }
    *outName = ps->lexer->sourceName;
    return ZR_TRUE;
}

SZrFileRange ZrParser_SourceProjection_CurrentLocation(SZrParserState *ps) {
    SZrFilePosition zero = ZrParser_FilePosition_Create(0U, 1, 1);
    SZrFileRange result = ZrParser_FileRange_Create(zero, zero, ZR_NULL);
    SZrTokenSpan span;
    SZrString *name;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->lexer->hasSourceEvidenceError) {
        source_projection_fail(ps);
        return result;
    }
    span = ps->lexer->t.span;
    /* A source-less, empty stream has no filename to project, even at EOS. */
    if (ZrParser_TokenSource_IsStream(ps->lexer) &&
        ps->lexer->t.token == ZR_TK_EOS && !ps->lexer->t.hasLexError &&
        ZrParser_TokenStream_Count(ps->lexer->tokenStream) == 0U &&
        span.sourceId == ZR_TOKEN_SOURCE_INVALID) {
        return result;
    }
    if (ZrParser_TokenSource_IsStream(ps->lexer)) {
        if (!ZrParser_RangeEvidence_TokenLocation(ps->lexer->ownership,
                    ps->lexer->t.evidenceIndex, ps->lexer->t.firstNativeOrdinal,
                    ps->lexer->t.lastNativeOrdinal, &result)) {
            source_projection_fail(ps);
        }
        return result;
    }
    if (ps->lexer->hasInheritedLocation) {
        if (!ZrParser_RangeEvidence_Validate(ps->lexer->ownership, ps->lexer->inheritedLocation)) {
            source_projection_fail(ps);
            return result;
        }
        return ps->lexer->inheritedLocation;
    }
    if (!ZrParser_TokenSpan_IsValid(span) ||
        !ZrParser_SourceProjection_SourceName(ps, span, &name)) {
        source_projection_fail(ps);
        return result;
    }
    result.start = ZrParser_FilePosition_Create(span.startOffset, span.startLine, span.startColumn);
    result.end = ZrParser_FilePosition_Create(span.endOffset, span.endLine, span.endColumn);
    result.source = name;
    result.originKey = ZrParser_Ownership_SourceOrigin(ps->lexer->ownership);
    result.ownerKey = ZrParser_Ownership_Key(ps->lexer->ownership);
    if (!ZrParser_RangeEvidence_Validate(ps->lexer->ownership, result)) {
        memset(&result, 0, sizeof(result));
        source_projection_fail(ps);
    }
    return result;
}

TZrBool ZrParser_SourceProjection_FirstPunctPieceLocation(
        SZrParserState *ps, SZrFileRange *outRange) {
    SZrFileRange range;
    SZrTokenPunctPiece *piece;
    if (outRange == ZR_NULL) {
        return source_projection_fail(ps);
    }
    memset(outRange, 0, sizeof(*outRange));
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->lexer->hasSourceEvidenceError) {
        return source_projection_fail(ps);
    }
    if (ZrParser_TokenSource_IsStream(ps->lexer)) {
        if (ps->lexer->t.punctPieceCount == 0U || ps->lexer->t.punctPieceCount > 3U) {
            return source_projection_fail(ps);
        }
        piece = &ps->lexer->t.punctPieces[0];
        if (!ZrParser_RangeEvidence_TokenLocation(ps->lexer->ownership,
                    piece->evidenceIndex, piece->nativeOrdinal, piece->nativeOrdinal, outRange)) {
            return source_projection_fail(ps);
        }
        return ZR_TRUE;
    }
    range = ZrParser_SourceProjection_CurrentLocation(ps);
    if (ps->lexer->hasSourceEvidenceError) {
        return source_projection_fail(ps);
    }
    if (ps->lexer->hasInheritedLocation) {
        *outRange = range;
        return ZR_TRUE;
    }
    if (ps->lexer->t.rawText == ZR_NULL || ps->lexer->t.rawLength == 0U ||
        range.start.offset >= range.end.offset || range.start.column == INT32_MAX) {
        return source_projection_fail(ps);
    }
    range.end = range.start;
    range.end.offset++;
    range.end.column++;
    *outRange = range;
    return ZR_TRUE;
}

TZrBool ZrParser_MergeLocations(
        SZrParserState *ps, SZrFileRange a, SZrFileRange b, SZrFileRange *outRange) {
    if (outRange == ZR_NULL) {
        return source_projection_fail(ps);
    }
    memset(outRange, 0, sizeof(*outRange));
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || ps->lexer->hasSourceEvidenceError) {
        return source_projection_fail(ps);
    }
    /* The empty source-less stream has no physical anchor to fabricate. */
    if (ZrParser_TokenSource_IsStream(ps->lexer) &&
        ZrParser_TokenStream_Count(ps->lexer->tokenStream) == 0U &&
        ps->lexer->t.token == ZR_TK_EOS && !ps->lexer->t.hasLexError &&
        ps->lexer->t.span.sourceId == ZR_TOKEN_SOURCE_INVALID &&
        a.originKey == 0U && b.originKey == 0U && a.source == ZR_NULL && b.source == ZR_NULL &&
        a.ownerKey == 0U && b.ownerKey == 0U && a.evidenceIndex == 0U && b.evidenceIndex == 0U &&
        a.flags == 0U && b.flags == 0U &&
        a.start.offset == 0U && a.end.offset == 0U && b.start.offset == 0U && b.end.offset == 0U &&
        a.start.line == 1 && a.end.line == 1 && b.start.line == 1 && b.end.line == 1 &&
        a.start.column == 1 && a.end.column == 1 && b.start.column == 1 && b.end.column == 1) {
        *outRange = a;
        return ZR_TRUE;
    }
    if (!ZrParser_RangeEvidence_Merge(ps->lexer->ownership, a, b, outRange)) {
        return source_projection_fail(ps);
    }
    return ZR_TRUE;
}
