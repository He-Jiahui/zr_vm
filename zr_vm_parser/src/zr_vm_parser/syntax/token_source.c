#include "syntax/token_source.h"
#include "syntax/token_kind.h"
#include "parser/parser_ownership.h"
#include "parser/parser_source_projection.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>
#include <stdint.h>

typedef enum EZrTokenSourceItemKind {
    ZR_TOKEN_SOURCE_ITEM_LEAF = 0,
    ZR_TOKEN_SOURCE_ITEM_OPEN,
    ZR_TOKEN_SOURCE_ITEM_CLOSE
} EZrTokenSourceItemKind;

typedef struct SZrTokenSourceItem {
    EZrTokenSourceItemKind kind;
    const SZrTokenTree *tree;
    EZrTokenDelimiter delimiter;
    SZrTokenSpan span;
    SZrSyntaxContext context;
    const TZrChar *rawText;
    TZrSize rawLength;
    /* Punctuation leaves and delimiters are at most one byte.  A compound
     * token reuses the first leaf's storage so its raw spelling survives a
     * later lookahead. */
    TZrChar raw[4];
} SZrTokenSourceItem;

typedef struct SZrTokenSourceFrame {
    const SZrTokenTree *tree;
    TZrSize childIndex;
    TZrBool openEmitted;
} SZrTokenSourceFrame;

typedef struct SZrTokenSourceState {
    const SZrTokenStream *stream;
    SZrTokenSourceItem *items;
    TZrSize itemCount;
    TZrSize itemCapacity;
    TZrSize index;
    TZrBool hasPending;
    SZrToken pending;
    SZrParserSourceNameCache sourceNames;
} SZrTokenSourceState;

enum {
    ZR_TOKEN_SOURCE_MAX_PUNCT_PIECES = 3
};

static TZrBool token_source_size_mul(
        TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL ||
        (left != 0U && right > ((TZrSize)-1) / left)) {
        return ZR_FALSE;
    }
    *out = left * right;
    return ZR_TRUE;
}

static TZrBool token_source_size_add(
        TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || left > (TZrSize)-1 - right) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

static void *token_source_alloc(SZrLexState *lexer, TZrSize size) {
    if (lexer == ZR_NULL || lexer->state == ZR_NULL ||
        lexer->state->global == ZR_NULL || size == 0U) {
        return ZR_NULL;
    }
    return ZrParser_Ownership_Allocate(
            lexer->ownership, size, ZR_MEMORY_NATIVE_TYPE_STRING, ZR_FALSE);
}

static void token_source_free(SZrLexState *lexer, void *memory, TZrSize size) {
    if (lexer == ZR_NULL || lexer->state == ZR_NULL ||
        lexer->state->global == ZR_NULL || memory == ZR_NULL || size == 0U) {
        return;
    }
    ZrCore_Memory_RawFreeWithType(
            lexer->state->global, memory, size, ZR_MEMORY_NATIVE_TYPE_STRING);
}

static TZrBool token_source_reserve_items(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        TZrSize required) {
    TZrSize capacity;
    TZrSize bytes;
    SZrTokenSourceItem *items;

    if (source == ZR_NULL || required <= source->itemCapacity) {
        return source != ZR_NULL;
    }
    if (required == 0U) {
        return ZR_FALSE;
    }
    capacity = source->itemCapacity > 0U ? source->itemCapacity : 16U;
    while (capacity < required) {
        if (capacity > ((TZrSize)-1) / 2U) {
            capacity = required;
            break;
        }
        capacity *= 2U;
    }
    if (capacity < required ||
        !token_source_size_mul(capacity, sizeof(*items), &bytes)) {
        return ZR_FALSE;
    }
    items = (SZrTokenSourceItem *)token_source_alloc(
            lexer, bytes);
    if (items == ZR_NULL) {
        return ZR_FALSE;
    }
    if (source->items != ZR_NULL && source->itemCount > 0U) {
        memcpy(items, source->items,
               source->itemCount * sizeof(*items));
        /* Punctuation and delimiter raw bytes live in each item.  Rebase
         * those self-pointers before releasing the old backing allocation. */
        for (TZrSize index = 0U; index < source->itemCount; index++) {
            if (items[index].kind != ZR_TOKEN_SOURCE_ITEM_LEAF ||
                (items[index].tree != ZR_NULL &&
                 ZrParser_TokenTree_Kind(items[index].tree) ==
                         ZR_TOKEN_TREE_PUNCT)) {
                items[index].rawText = items[index].raw;
            }
        }
    }
    token_source_free(lexer, source->items,
                      source->itemCapacity * sizeof(*items));
    source->items = items;
    source->itemCapacity = capacity;
    return ZR_TRUE;
}

static TZrBool token_source_append_item(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        SZrTokenSourceItem item) {
    SZrTokenSourceItem *destination;
    if (source == ZR_NULL || source->itemCount == (TZrSize)-1 ||
        !token_source_reserve_items(lexer, source, source->itemCount + 1U)) {
        return ZR_FALSE;
    }
    destination = &source->items[source->itemCount++];
    *destination = item;
    if (item.kind != ZR_TOKEN_SOURCE_ITEM_LEAF ||
        (item.tree != ZR_NULL &&
         ZrParser_TokenTree_Kind(item.tree) == ZR_TOKEN_TREE_PUNCT)) {
        destination->rawText = destination->raw;
    }
    return ZR_TRUE;
}

static TZrBool token_source_append_leaf(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        const SZrTokenTree *tree) {
    SZrTokenText rawText;
    const TZrChar *raw;
    SZrTokenSourceItem item;

    if (source == ZR_NULL || tree == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&item, 0, sizeof(item));
    item.kind = ZR_TOKEN_SOURCE_ITEM_LEAF;
    item.tree = tree;
    item.span = ZrParser_TokenTree_Span(tree);
    item.context = ZrParser_TokenTree_Context(tree);
    rawText = ZrParser_TokenTree_Text(tree);
    raw = ZrParser_TokenStream_TextBytes(source->stream, rawText);
    if (rawText.length > 0U && raw == ZR_NULL) {
        return ZR_FALSE;
    }
    item.rawText = raw;
    item.rawLength = rawText.length;
    if (ZrParser_TokenTree_Kind(tree) == ZR_TOKEN_TREE_PUNCT) {
        if (rawText.length == 0U || rawText.length > sizeof(item.raw)) {
            return ZR_FALSE;
        }
        memcpy(item.raw, raw, rawText.length);
        item.rawText = item.raw;
    }
    return token_source_append_item(lexer, source, item);
}

static TZrBool token_source_reserve_frames(
        SZrLexState *lexer,
        SZrTokenSourceFrame **frames,
        TZrSize *capacity,
        TZrSize used,
        TZrSize required) {
    TZrSize nextCapacity;
    TZrSize bytes;
    SZrTokenSourceFrame *nextFrames;

    if (frames == ZR_NULL || capacity == ZR_NULL || required == 0U) {
        return ZR_FALSE;
    }
    if (required <= *capacity) {
        return ZR_TRUE;
    }
    nextCapacity = *capacity > 0U ? *capacity : 16U;
    while (nextCapacity < required) {
        if (nextCapacity > ((TZrSize)-1) / 2U) {
            nextCapacity = required;
            break;
        }
        nextCapacity *= 2U;
    }
    if (nextCapacity < required ||
        !token_source_size_mul(nextCapacity, sizeof(*nextFrames), &bytes)) {
        return ZR_FALSE;
    }
    nextFrames = (SZrTokenSourceFrame *)token_source_alloc(lexer, bytes);
    if (nextFrames == ZR_NULL) {
        return ZR_FALSE;
    }
    if (*frames != ZR_NULL && used > 0U) {
        memcpy(nextFrames, *frames, used * sizeof(**frames));
    }
    token_source_free(lexer, *frames,
                      *capacity * sizeof(**frames));
    *frames = nextFrames;
    *capacity = nextCapacity;
    return ZR_TRUE;
}

static const TZrChar *token_source_delimiter_text(
        EZrTokenDelimiter delimiter, TZrBool opening);

/* Walk groups iteratively so nested macro input does not consume the C stack. */
static TZrBool token_source_flatten(
        SZrLexState *lexer,
        SZrTokenSourceState *source) {
    SZrTokenSourceFrame *frames = ZR_NULL;
    TZrSize frameCount = 0U;
    TZrSize frameCapacity = 0U;
    TZrSize rootIndex;

    if (lexer == ZR_NULL || source == ZR_NULL || source->stream == ZR_NULL) {
        return ZR_FALSE;
    }

    for (rootIndex = 0U;
         rootIndex < ZrParser_TokenStream_Count(source->stream);
         rootIndex++) {
        const SZrTokenTree *root = ZrParser_TokenStream_At(source->stream, rootIndex);
        if (root == ZR_NULL) {
            token_source_free(lexer, frames,
                              frameCapacity * sizeof(SZrTokenSourceFrame));
            return ZR_FALSE;
        }
        if (!token_source_reserve_frames(
                    lexer, &frames, &frameCapacity, frameCount, frameCount + 1U)) {
                token_source_free(lexer, frames,
                                  frameCapacity * sizeof(SZrTokenSourceFrame));
                return ZR_FALSE;
        }
        frames[frameCount].tree = root;
        frames[frameCount].childIndex = 0U;
        frames[frameCount].openEmitted = ZR_FALSE;
        frameCount++;

        while (frameCount > 0U) {
            SZrTokenSourceFrame *frame = &frames[frameCount - 1U];
            const SZrTokenTree *tree = frame->tree;
            EZrTokenTreeKind kind = ZrParser_TokenTree_Kind(tree);

            if (kind != ZR_TOKEN_TREE_GROUP) {
                if (!token_source_append_leaf(lexer, source, tree)) {
                    token_source_free(lexer, frames,
                                      frameCapacity * sizeof(SZrTokenSourceFrame));
                    return ZR_FALSE;
                }
                frameCount--;
                continue;
            }

            {
                EZrTokenDelimiter delimiter = ZrParser_TokenTree_GroupDelimiter(tree);
                if (delimiter != ZR_TOKEN_DELIMITER_NONE && !frame->openEmitted) {
                    SZrTokenSourceItem openItem;
                    memset(&openItem, 0, sizeof(openItem));
                    openItem.kind = ZR_TOKEN_SOURCE_ITEM_OPEN;
                    openItem.tree = tree;
                    openItem.delimiter = delimiter;
                    openItem.span = ZrParser_TokenTree_GroupOpenSpan(tree);
                    openItem.context = ZrParser_TokenTree_Context(tree);
                    openItem.rawText = openItem.raw;
                    openItem.rawLength = 1U;
                    openItem.raw[0] = token_source_delimiter_text(
                            delimiter, ZR_TRUE)[0];
                    if (!token_source_append_item(lexer, source, openItem)) {
                        token_source_free(lexer, frames,
                                          frameCapacity * sizeof(SZrTokenSourceFrame));
                        return ZR_FALSE;
                    }
                    frame->openEmitted = ZR_TRUE;
                }
            }

            if (frame->childIndex < ZrParser_TokenTree_GroupChildCount(tree)) {
                const SZrTokenTree *child = ZrParser_TokenTree_GroupChildAt(
                        tree, frame->childIndex++);
                if (child == ZR_NULL) {
                    token_source_free(lexer, frames,
                                      frameCapacity * sizeof(SZrTokenSourceFrame));
                    return ZR_FALSE;
                }
                if (!token_source_reserve_frames(
                            lexer, &frames, &frameCapacity,
                            frameCount, frameCount + 1U)) {
                        token_source_free(lexer, frames,
                                          frameCapacity * sizeof(SZrTokenSourceFrame));
                        return ZR_FALSE;
                }
                frames[frameCount].tree = child;
                frames[frameCount].childIndex = 0U;
                frames[frameCount].openEmitted = ZR_FALSE;
                frameCount++;
                continue;
            }

            if (ZrParser_TokenTree_GroupDelimiter(tree) != ZR_TOKEN_DELIMITER_NONE) {
                SZrTokenSourceItem closeItem;
                memset(&closeItem, 0, sizeof(closeItem));
                closeItem.kind = ZR_TOKEN_SOURCE_ITEM_CLOSE;
                closeItem.tree = tree;
                closeItem.delimiter = ZrParser_TokenTree_GroupDelimiter(tree);
                closeItem.span = ZrParser_TokenTree_GroupCloseSpan(tree);
                closeItem.context = ZrParser_TokenTree_Context(tree);
                closeItem.rawText = closeItem.raw;
                closeItem.rawLength = 1U;
                closeItem.raw[0] = token_source_delimiter_text(
                        closeItem.delimiter, ZR_FALSE)[0];
                if (!token_source_append_item(lexer, source, closeItem)) {
                    token_source_free(lexer, frames,
                                      frameCapacity * sizeof(SZrTokenSourceFrame));
                    return ZR_FALSE;
                }
            }
            frameCount--;
        }
    }

    token_source_free(lexer, frames,
                      frameCapacity * sizeof(SZrTokenSourceFrame));
    return ZR_TRUE;
}

static EZrToken token_source_delimiter_token(EZrTokenDelimiter delimiter,
                                             TZrBool opening) {
    if (opening) {
        switch (delimiter) {
            case ZR_TOKEN_DELIMITER_PAREN: return ZR_TK_LPAREN;
            case ZR_TOKEN_DELIMITER_BRACE: return ZR_TK_LBRACE;
            case ZR_TOKEN_DELIMITER_BRACKET: return ZR_TK_LBRACKET;
            default: return ZR_TK_EOS;
        }
    }
    switch (delimiter) {
        case ZR_TOKEN_DELIMITER_PAREN: return ZR_TK_RPAREN;
        case ZR_TOKEN_DELIMITER_BRACE: return ZR_TK_RBRACE;
        case ZR_TOKEN_DELIMITER_BRACKET: return ZR_TK_RBRACKET;
        default: return ZR_TK_EOS;
    }
}

static const TZrChar *token_source_delimiter_text(EZrTokenDelimiter delimiter,
                                                   TZrBool opening) {
    if (opening) {
        switch (delimiter) {
            case ZR_TOKEN_DELIMITER_PAREN: return "(";
            case ZR_TOKEN_DELIMITER_BRACE: return "{";
            case ZR_TOKEN_DELIMITER_BRACKET: return "[";
            default: return "";
        }
    }
    switch (delimiter) {
        case ZR_TOKEN_DELIMITER_PAREN: return ")";
        case ZR_TOKEN_DELIMITER_BRACE: return "}";
        case ZR_TOKEN_DELIMITER_BRACKET: return "]";
        default: return "";
    }
}

static TZrBool token_source_make_leaf(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        const SZrTokenSourceItem *item,
        SZrToken *outToken) {
    SZrTokenNativeSemInfo native;
    SZrTokenText raw;
    const TZrChar *rawText;
    const TZrChar *semanticText;

    if (lexer == ZR_NULL || source == ZR_NULL || item == ZR_NULL ||
        outToken == ZR_NULL || item->tree == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outToken, 0, sizeof(*outToken));
    outToken->span = item->span;
    outToken->syntaxContext = item->context;
    raw = ZrParser_TokenTree_Text(item->tree);
    rawText = item->rawText;
    if (raw.length != item->rawLength ||
        (raw.length > 0U && rawText == ZR_NULL)) {
        return ZR_FALSE;
    }
    outToken->rawText = rawText;
    outToken->rawLength = item->rawLength;
    if (!ZrParser_TokenTree_NativeSemInfo(item->tree, &native)) {
        return ZR_FALSE;
    }
    outToken->token = native.token;
    outToken->seminfo.booleanValue = native.booleanValue;
    outToken->seminfo.intValue = native.intValue;
    outToken->seminfo.floatValue = native.floatValue;
    outToken->seminfo.charValue = (TZrChar)native.charValue;
    if (ZrParser_TokenTree_Kind(item->tree) == ZR_TOKEN_TREE_PUNCT) {
        outToken->punctPieceCount = 1U;
        outToken->punctPieces[0].span = item->span;
        outToken->punctPieces[0].syntaxContext = item->context;
        outToken->punctPieces[0].rawText = item->rawText;
        outToken->punctPieces[0].rawLength = item->rawLength;
    }
    semanticText = ZrParser_TokenStream_TextBytes(source->stream, native.text);
    if (semanticText != ZR_NULL &&
        (native.token == ZR_TK_IDENTIFIER || native.token == ZR_TK_INTEGER ||
         native.token == ZR_TK_FLOAT || native.token == ZR_TK_STRING ||
         native.token == ZR_TK_TEMPLATE_STRING || native.token == ZR_TK_CHAR)) {
        outToken->seminfo.stringValue = ZrParser_Lexer_CreateString(
                lexer, semanticText, native.text.length);
        if (outToken->seminfo.stringValue == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool token_source_make_item(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        TZrSize index,
        SZrToken *outToken) {
    const SZrTokenSourceItem *item;
    const TZrChar *delimiterText;

    if (source == ZR_NULL || outToken == ZR_NULL || index >= source->itemCount) {
        return ZR_FALSE;
    }
    item = &source->items[index];
    if (item->kind == ZR_TOKEN_SOURCE_ITEM_LEAF) {
        if (!token_source_make_leaf(lexer, source, item, outToken)) {
            return ZR_FALSE;
        }
    } else {
        memset(outToken, 0, sizeof(*outToken));
        outToken->token = token_source_delimiter_token(
                item->delimiter, item->kind == ZR_TOKEN_SOURCE_ITEM_OPEN);
        outToken->span = item->span;
        outToken->syntaxContext = item->context;
        delimiterText = item->rawText;
        outToken->rawText = delimiterText;
        outToken->rawLength = item->rawLength;
    }
    outToken->firstNativeOrdinal = (TZrUInt32)index + 1U;
    outToken->lastNativeOrdinal = outToken->firstNativeOrdinal;
    outToken->evidenceIndex = outToken->firstNativeOrdinal;
    if (outToken->punctPieceCount == 1U) {
        outToken->punctPieces[0].nativeOrdinal = outToken->firstNativeOrdinal;
        outToken->punctPieces[0].evidenceIndex = outToken->evidenceIndex;
    }
    return item->kind == ZR_TOKEN_SOURCE_ITEM_LEAF || outToken->token != ZR_TK_EOS;
}

static TZrBool token_source_can_glue(const SZrTokenSourceItem *left,
                                     const SZrTokenSourceItem *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->kind != ZR_TOKEN_SOURCE_ITEM_LEAF ||
        right->kind != ZR_TOKEN_SOURCE_ITEM_LEAF ||
        ZrParser_TokenTree_Kind(left->tree) != ZR_TOKEN_TREE_PUNCT ||
        ZrParser_TokenTree_Kind(right->tree) != ZR_TOKEN_TREE_PUNCT ||
        ZrParser_TokenTree_PunctSpacing(left->tree) != ZR_TOKEN_SPACING_JOINT ||
        left->rawLength != 1U || right->rawLength != 1U ||
        left->rawText == ZR_NULL || right->rawText == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void token_source_set_piece(
        SZrTokenPunctPiece *piece,
        const SZrTokenSourceItem *item, TZrUInt32 ordinal) {
    if (piece == ZR_NULL || item == ZR_NULL) {
        return;
    }
    piece->span = item->span;
    piece->syntaxContext = item->context;
    piece->rawText = item->rawText;
    piece->rawLength = item->rawLength;
    piece->nativeOrdinal = ordinal;
    piece->evidenceIndex = ordinal;
}

static SZrTokenSpan token_source_join_piece_spans(
        const SZrTokenPunctPiece *pieces, TZrSize count) {
    SZrTokenSpan result;

    if (pieces == ZR_NULL || count == 0U) {
        return ZrParser_TokenSpan_None();
    }
    result = pieces[0].span;
    for (TZrSize index = 1U; index < count; index++) {
        SZrTokenSpan joined;
        if (result.sourceId != pieces[index].span.sourceId ||
            !ZrParser_TokenSpan_Join(result, pieces[index].span, &joined)) {
            /* A single span cannot represent two source buffers.  The
             * per-piece origins remain authoritative in this case. */
            return pieces[0].span;
        }
        result = joined;
    }
    return result;
}

static TZrBool token_source_build_compound(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        const SZrTokenSourceItem *const *items,
        TZrSize count,
        SZrToken *outToken) {
    TZrSize rawLength = 0U;
    TZrUInt32 codepoints[3] = {0U, 0U, 0U};
    TZrBool hasThird;
    EZrToken compound;
    TZrChar *raw;

    if (source == ZR_NULL || items == ZR_NULL || outToken == ZR_NULL ||
        count < 2U || count > ZR_TOKEN_SOURCE_MAX_PUNCT_PIECES ||
        items[0] == ZR_NULL || items[0]->rawText == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < count; index++) {
        const SZrTokenSourceItem *item = items[index];
        if (item == ZR_NULL || item->rawText == ZR_NULL ||
            item->rawLength != 1U ||
            !token_source_size_add(rawLength, item->rawLength, &rawLength) ||
            rawLength > 3U) {
            return ZR_FALSE;
        }
        codepoints[index] = (TZrUInt8)item->rawText[0];
    }
    hasThird = count == 3U && codepoints[0] == '.' &&
               codepoints[1] == '.' && codepoints[2] == '.';
    compound = ZrParser_Token_CompoundPunct(
            codepoints[0], codepoints[1], codepoints[2], hasThird);
    if (compound == ZR_TK_EOS || (count == 3U && !hasThird)) {
        return ZR_FALSE;
    }
    raw = (TZrChar *)items[0]->rawText;
    for (TZrSize index = 1U, offset = 1U; index < count; index++, offset++) {
        raw[offset] = items[index]->rawText[0];
    }
    if (rawLength < sizeof(items[0]->raw)) {
        raw[rawLength] = '\0';
    }
    memset(outToken, 0, sizeof(*outToken));
    outToken->token = compound;
    outToken->rawText = raw;
    outToken->rawLength = rawLength;
    outToken->span = items[0]->span;
    outToken->syntaxContext = items[0]->context;
    outToken->punctPieceCount = count;
    for (TZrSize index = 0U; index < count; index++) {
        token_source_set_piece(&outToken->punctPieces[index], items[index],
                (TZrUInt32)(items[index] - source->items) + 1U);
    }
    outToken->span = token_source_join_piece_spans(
            outToken->punctPieces, outToken->punctPieceCount);
    outToken->firstNativeOrdinal = outToken->punctPieces[0].nativeOrdinal;
    outToken->lastNativeOrdinal = outToken->punctPieces[count - 1U].nativeOrdinal;
    if (!ZrParser_RangeEvidence_Interval(lexer->ownership, outToken->firstNativeOrdinal,
                outToken->lastNativeOrdinal, &outToken->evidenceIndex)) {
        return ZrParser_SourceProjection_FailLexer(lexer);
    }
    return ZR_TRUE;
}

static TZrBool token_source_build_pending_remainder(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        const SZrToken *current,
        TZrSize firstRemainder,
        TZrSize remainderCount) {
    SZrToken pending;
    TZrChar *raw;
    TZrUInt32 codepoints[3] = {0U, 0U, 0U};
    TZrSize rawLength = 0U;
    TZrBool hasThird;
    EZrToken compound;

    if (source == ZR_NULL || current == ZR_NULL ||
        firstRemainder >= current->punctPieceCount || remainderCount == 0U ||
        remainderCount > ZR_TOKEN_SOURCE_MAX_PUNCT_PIECES ||
        firstRemainder + remainderCount > current->punctPieceCount) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < remainderCount; index++) {
        const SZrTokenPunctPiece *piece =
                &current->punctPieces[firstRemainder + index];
        if (piece->rawText == ZR_NULL || piece->rawLength != 1U ||
            !token_source_size_add(rawLength, piece->rawLength, &rawLength) ||
            rawLength > 3U) {
            return ZR_FALSE;
        }
        codepoints[index] = (TZrUInt8)piece->rawText[0];
    }
    raw = (TZrChar *)current->punctPieces[firstRemainder].rawText;
    if (raw == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 1U; index < remainderCount; index++) {
        raw[index] = current->punctPieces[firstRemainder + index].rawText[0];
    }
    if (rawLength < 4U) {
        raw[rawLength] = '\0';
    }
    memset(&pending, 0, sizeof(pending));
    if (remainderCount == 1U) {
        pending.token = ZrParser_Token_SinglePunct(codepoints[0]);
    } else {
        hasThird = remainderCount == 3U && codepoints[0] == '.' &&
                   codepoints[1] == '.' && codepoints[2] == '.';
        compound = ZrParser_Token_CompoundPunct(
                codepoints[0], codepoints[1], codepoints[2], hasThird);
        if (compound == ZR_TK_EOS || (remainderCount == 3U && !hasThird)) {
            return ZR_FALSE;
        }
        pending.token = compound;
    }
    pending.rawText = raw;
    pending.rawLength = rawLength;
    pending.punctPieceCount = remainderCount;
    pending.syntaxContext = current->punctPieces[firstRemainder].syntaxContext;
    pending.span = current->punctPieces[firstRemainder].span;
    for (TZrSize index = 0U; index < remainderCount; index++) {
        pending.punctPieces[index] =
                current->punctPieces[firstRemainder + index];
    }
    pending.span = token_source_join_piece_spans(
            pending.punctPieces, pending.punctPieceCount);
    pending.firstNativeOrdinal = pending.punctPieces[0].nativeOrdinal;
    pending.lastNativeOrdinal = pending.punctPieces[remainderCount - 1U].nativeOrdinal;
    if (!ZrParser_RangeEvidence_Interval(lexer->ownership, pending.firstNativeOrdinal,
                pending.lastNativeOrdinal, &pending.evidenceIndex)) {
        return ZrParser_SourceProjection_FailLexer(lexer);
    }
    source->pending = pending;
    source->hasPending = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool token_source_next_internal(
        SZrLexState *lexer,
        SZrTokenSourceState *source,
        SZrToken *outToken) {
    TZrSize index;
    const SZrTokenSourceItem *item;
    if (source->index >= source->itemCount) {
        memset(outToken, 0, sizeof(*outToken));
        outToken->token = ZR_TK_EOS;
        outToken->span = ZrParser_TokenStream_EndSpan(source->stream);
        outToken->syntaxContext = ZrParser_SyntaxContext_Root();
        outToken->firstNativeOrdinal = (TZrUInt32)source->itemCount + 1U;
        outToken->lastNativeOrdinal = outToken->firstNativeOrdinal;
        outToken->evidenceIndex = outToken->firstNativeOrdinal;
        return ZR_TRUE;
    }
    index = source->index;
    item = &source->items[index];
    if (index + 1U < source->itemCount &&
        token_source_can_glue(item, &source->items[index + 1U])) {
        const SZrTokenSourceItem *parts[3];
        TZrSize consumed = 2U;
        SZrToken firstToken;

        parts[0] = item;
        parts[1] = &source->items[index + 1U];
        if (index + 2U < source->itemCount &&
            token_source_can_glue(parts[1], &source->items[index + 2U]) &&
            parts[0]->rawText[0] == '.' && parts[1]->rawText[0] == '.' &&
            source->items[index + 2U].rawText[0] == '.') {
            parts[2] = &source->items[index + 2U];
            consumed = 3U;
        }
        if (token_source_build_compound(lexer, source, parts, consumed, &firstToken)) {
            source->index += consumed;
            *outToken = firstToken;
            return ZR_TRUE;
        }
        if (lexer->hasSourceEvidenceError) {
            return ZR_FALSE;
        }
    }
    if (!token_source_make_item(lexer, source, index, outToken)) {
        return ZR_FALSE;
    }
    source->index++;
    return ZR_TRUE;
}

static TZrBool token_source_cache_names(
        SZrLexState *lexer, SZrTokenSourceState *source) {
    SZrTokenSpan end = ZrParser_TokenStream_EndSpan(source->stream);
    SZrParserRangeEvidence *stage = ZR_NULL;
    TZrBool absent = ZrParser_TokenStream_Count(source->stream) == 0U &&
                     end.sourceId == ZR_TOKEN_SOURCE_INVALID;

    if (source->itemCount >= UINT32_MAX || !ZrParser_RangeEvidence_Begin(
                lexer->ownership, lexer->state, source->itemCount + 1U, &stage)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < source->itemCount; index++) {
        if (!ZrParser_SourceProjection_AppendSpanEvidence(lexer, source->stream,
                    &source->sourceNames, stage, source->items[index].span, ZR_FALSE)) {
            ZrParser_RangeEvidence_Free(stage);
            return ZR_FALSE;
        }
    }
    if (!ZrParser_SourceProjection_AppendSpanEvidence(lexer, source->stream,
                &source->sourceNames, stage, end, absent) || !ZrParser_RangeEvidence_Commit(stage)) {
        ZrParser_RangeEvidence_Free(stage);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void token_source_destroy(SZrLexState *lexer, SZrTokenSourceState *source) {
    TZrSize itemBytes = 0U;

    ZrParser_SourceProjection_FreeCache(lexer, &source->sourceNames);
    if (token_source_size_mul(source->itemCapacity, sizeof(*source->items), &itemBytes)) {
        token_source_free(lexer, source->items, itemBytes);
    }
    ZrParser_TokenStream_Release((SZrTokenStream *)source->stream);
    token_source_free(lexer, source, sizeof(*source));
}

void ZrParser_TokenSource_InitStream(
        SZrLexState *lexer,
        const SZrTokenStream *stream) {
    SZrTokenSourceState *source;
    if (lexer == ZR_NULL || stream == ZR_NULL ||
        !ZrParser_TokenStream_IsSealed(stream)) {
        return;
    }
    source = (SZrTokenSourceState *)token_source_alloc(
            lexer, sizeof(SZrTokenSourceState));
    if (source == ZR_NULL) {
        ZrParser_SourceProjection_FailLexer(lexer);
        return;
    }
    memset(source, 0, sizeof(*source));
    source->stream = stream;
    ZrParser_TokenStream_Retain((SZrTokenStream *)stream);
    if (!token_source_flatten(lexer, source) || !token_source_cache_names(lexer, source)) {
        token_source_destroy(lexer, source);
        ZrParser_SourceProjection_FailLexer(lexer);
        return;
    }
    lexer->tokenStream = (SZrTokenStream *)stream;
    lexer->tokenSourceState = source;
}

void ZrParser_TokenSource_FreeStream(SZrLexState *lexer) {
    SZrTokenSourceState *source;
    if (lexer == ZR_NULL || lexer->tokenSourceState == ZR_NULL) {
        return;
    }
    source = (SZrTokenSourceState *)lexer->tokenSourceState;
    token_source_destroy(lexer, source);
    lexer->tokenSourceState = ZR_NULL;
    lexer->tokenStream = ZR_NULL;
}

TZrBool ZrParser_TokenSource_IsStream(const SZrLexState *lexer) {
    return lexer != ZR_NULL && lexer->tokenSourceState != ZR_NULL;
}

TZrBool ZrParser_TokenSource_SourceName(
        const SZrLexState *lexer, TZrTokenSourceId sourceId, SZrString **outName) {
    const SZrTokenSourceState *source;

    if (outName == ZR_NULL) {
        return ZR_FALSE;
    }
    *outName = ZR_NULL;
    if (!ZrParser_TokenSource_IsStream(lexer)) {
        return ZR_FALSE;
    }
    source = (const SZrTokenSourceState *)lexer->tokenSourceState;
    return ZrParser_SourceProjection_FindName(&source->sourceNames, sourceId, outName);
}

TZrBool ZrParser_TokenSource_Next(SZrLexState *lexer, SZrToken *outToken) {
    SZrTokenSourceState *source;
    if (!ZrParser_TokenSource_IsStream(lexer) || outToken == ZR_NULL) {
        return ZR_FALSE;
    }
    source = (SZrTokenSourceState *)lexer->tokenSourceState;
    if (source->hasPending) {
        *outToken = source->pending;
        source->hasPending = ZR_FALSE;
        return ZR_TRUE;
    }
    if (lexer->hasLookahead) {
        *outToken = lexer->lookahead;
        lexer->hasLookahead = ZR_FALSE;
        return ZR_TRUE;
    }
    return token_source_next_internal(lexer, source, outToken);
}

TZrBool ZrParser_TokenSource_Lookahead(SZrLexState *lexer, SZrToken *outToken) {
    SZrTokenSourceState *source;
    if (!ZrParser_TokenSource_IsStream(lexer) || outToken == ZR_NULL) {
        return ZR_FALSE;
    }
    source = (SZrTokenSourceState *)lexer->tokenSourceState;
    if (source->hasPending) {
        *outToken = source->pending;
        return ZR_TRUE;
    }
    if (!lexer->hasLookahead) {
        if (!token_source_next_internal(lexer, source, &lexer->lookahead)) {
            return ZR_FALSE;
        }
        lexer->hasLookahead = ZR_TRUE;
    }
    *outToken = lexer->lookahead;
    return ZR_TRUE;
}

TZrBool ZrParser_TokenSource_SplitCurrentPunct(
        SZrLexState *lexer, TZrChar firstPunct) {
    SZrTokenSourceState *source;
    SZrToken current;
    if (!ZrParser_TokenSource_IsStream(lexer) || lexer == ZR_NULL ||
        lexer->t.rawLength < 2U || lexer->t.rawText == ZR_NULL ||
        lexer->t.rawText[0] != firstPunct) {
        return ZR_FALSE;
    }
    source = (SZrTokenSourceState *)lexer->tokenSourceState;
    current = lexer->t;
    if (current.punctPieceCount < 2U ||
        current.punctPieceCount > ZR_TOKEN_SOURCE_MAX_PUNCT_PIECES ||
        current.punctPieces[0].rawText == ZR_NULL ||
        current.punctPieces[0].rawLength != 1U ||
        current.punctPieces[0].rawText[0] != firstPunct ||
        source->hasPending ||
        !token_source_build_pending_remainder(
                lexer, source, &current, 1U, current.punctPieceCount - 1U)) {
        return ZR_FALSE;
    }
    memset(&lexer->t.punctPieces[0], 0, sizeof(lexer->t.punctPieces[0]));
    lexer->t.punctPieceCount = 1U;
    lexer->t.punctPieces[0] = current.punctPieces[0];
    lexer->t.token = ZrParser_Token_SinglePunct((TZrUInt32)(TZrUInt8)firstPunct);
    lexer->t.rawText = current.punctPieces[0].rawText;
    lexer->t.rawLength = 1U;
    lexer->t.span = current.punctPieces[0].span;
    lexer->t.syntaxContext = current.punctPieces[0].syntaxContext;
    lexer->t.firstNativeOrdinal = current.punctPieces[0].nativeOrdinal;
    lexer->t.lastNativeOrdinal = current.punctPieces[0].nativeOrdinal;
    lexer->t.evidenceIndex = current.punctPieces[0].evidenceIndex;
    return ZR_TRUE;
}

void ZrParser_TokenSource_Save(
        const SZrLexState *lexer,
        SZrTokenSourceCheckpoint *checkpoint) {
    const SZrTokenSourceState *source;
    if (checkpoint == ZR_NULL) {
        return;
    }
    memset(checkpoint, 0, sizeof(*checkpoint));
    if (lexer == ZR_NULL) {
        return;
    }
    checkpoint->hasLookahead = lexer->hasLookahead;
    checkpoint->lookahead = lexer->lookahead;
    checkpoint->hasSplitPending = lexer->hasSplitPending;
    checkpoint->splitPending = lexer->splitPending;
    if (!ZrParser_TokenSource_IsStream(lexer)) {
        return;
    }
    source = (const SZrTokenSourceState *)lexer->tokenSourceState;
    checkpoint->isStream = ZR_TRUE;
    checkpoint->streamIndex = source->index;
    checkpoint->hasPending = source->hasPending;
    checkpoint->pending = source->pending;
}

void ZrParser_TokenSource_Restore(
        SZrLexState *lexer,
        const SZrTokenSourceCheckpoint *checkpoint) {
    SZrTokenSourceState *source;
    if (lexer == ZR_NULL || checkpoint == ZR_NULL) {
        return;
    }
    lexer->hasLookahead = checkpoint->hasLookahead;
    lexer->lookahead = checkpoint->lookahead;
    lexer->hasSplitPending = checkpoint->hasSplitPending;
    lexer->splitPending = checkpoint->splitPending;
    if (!checkpoint->isStream || !ZrParser_TokenSource_IsStream(lexer)) {
        return;
    }
    source = (SZrTokenSourceState *)lexer->tokenSourceState;
    source->index = checkpoint->streamIndex;
    source->hasPending = checkpoint->hasPending;
    source->pending = checkpoint->pending;
}
