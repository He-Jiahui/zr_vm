#include "syntax/token_stream_internal.h"

#include <limits.h>
#include <string.h>

TZrSize ZrParser_TokenStream_Count(const SZrTokenStream *stream) {
    return stream == ZR_NULL ? 0u : stream->rootCount;
}

SZrTokenSpan ZrParser_TokenStream_EndSpan(const SZrTokenStream *stream) {
    if (stream == ZR_NULL) {
        return ZrParser_TokenSpan_None();
    }
    if (ZrParser_TokenSpan_IsValid(stream->endSpan)) {
        return stream->endSpan;
    }
    if (stream->rootCount != 0u) {
        SZrTokenSpan span = stream->roots[stream->rootCount - 1u]->span;
        span.startOffset = span.endOffset;
        span.startLine = span.endLine;
        span.startColumn = span.endColumn;
        return span;
    }
    return ZrParser_TokenSpan_None();
}

const SZrTokenTree *ZrParser_TokenStream_At(
        const SZrTokenStream *stream, TZrSize index) {
    if (stream == ZR_NULL || index >= stream->rootCount) {
        return ZR_NULL;
    }
    return stream->roots[index];
}

EZrTokenTreeKind ZrParser_TokenTree_Kind(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? ZR_TOKEN_TREE_GROUP : tree->kind;
}

SZrTokenSpan ZrParser_TokenTree_Span(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? ZrParser_TokenSpan_None() : tree->span;
}

SZrSyntaxContext ZrParser_TokenTree_Context(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? ZrParser_SyntaxContext_Root() : tree->context;
}

TZrBool ZrParser_TokenTree_NativeSemInfo(
        const SZrTokenTree *tree,
        SZrTokenNativeSemInfo *outSemInfo) {
    if (tree == ZR_NULL || outSemInfo == ZR_NULL ||
        tree->kind == ZR_TOKEN_TREE_GROUP) {
        return ZR_FALSE;
    }
    *outSemInfo = tree->semInfo;
    return ZR_TRUE;
}

SZrTokenText ZrParser_TokenTree_Text(const SZrTokenTree *tree) {
    SZrTokenText text = {0};
    return tree == ZR_NULL ? text : tree->rawText;
}

SZrTokenText ZrParser_TokenTree_Suffix(const SZrTokenTree *tree) {
    SZrTokenText text = {0};
    return tree == ZR_NULL ? text : tree->semInfo.suffix;
}

TZrUInt32 ZrParser_TokenTree_PunctCodepoint(const SZrTokenTree *tree) {
    return tree == ZR_NULL || tree->kind != ZR_TOKEN_TREE_PUNCT
                   ? 0u
                   : tree->punctCodepoint;
}

EZrTokenSpacing ZrParser_TokenTree_PunctSpacing(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? ZR_TOKEN_SPACING_ALONE : tree->punctSpacing;
}

EZrTokenDelimiter ZrParser_TokenTree_GroupDelimiter(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? ZR_TOKEN_DELIMITER_NONE : tree->delimiter;
}

SZrTokenSpan ZrParser_TokenTree_GroupOpenSpan(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? ZrParser_TokenSpan_None() : tree->openSpan;
}

SZrTokenSpan ZrParser_TokenTree_GroupCloseSpan(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? ZrParser_TokenSpan_None() : tree->closeSpan;
}

TZrSize ZrParser_TokenTree_GroupChildCount(const SZrTokenTree *tree) {
    return tree == ZR_NULL || tree->kind != ZR_TOKEN_TREE_GROUP
                   ? 0u
                   : tree->childCount;
}

const SZrTokenTree *ZrParser_TokenTree_GroupChildAt(
        const SZrTokenTree *tree, TZrSize index) {
    if (tree == ZR_NULL || tree->kind != ZR_TOKEN_TREE_GROUP ||
        index >= tree->childCount) {
        return ZR_NULL;
    }
    return tree->children[index];
}

TZrSize ZrParser_TokenTree_GroupInnerTriviaCount(const SZrTokenTree *tree) {
    return tree == ZR_NULL || tree->kind != ZR_TOKEN_TREE_GROUP
                   ? 0u : tree->innerTriviaCount;
}

TZrSize ZrParser_TokenStream_EmptyTriviaCount(const SZrTokenStream *stream) {
    return stream == ZR_NULL ? 0u : stream->emptyTriviaCount;
}

TZrBool ZrParser_TokenStream_EmptyTriviaAt(
        const SZrTokenStream *stream, TZrSize index, SZrTokenTrivia *outTrivia) {
    if (stream == ZR_NULL || outTrivia == ZR_NULL || index >= stream->emptyTriviaCount) {
        return ZR_FALSE;
    }
    *outTrivia = stream->emptyTrivia[index];
    return ZR_TRUE;
}

TZrBool ZrParser_TokenTree_GroupInnerTriviaAt(
        const SZrTokenTree *tree, TZrSize index, SZrTokenTrivia *outTrivia) {
    if (tree == ZR_NULL || tree->kind != ZR_TOKEN_TREE_GROUP ||
        outTrivia == ZR_NULL || index >= tree->innerTriviaCount) {
        return ZR_FALSE;
    }
    *outTrivia = tree->innerTrivia[index];
    return ZR_TRUE;
}

TZrSize ZrParser_TokenTree_LeadingTriviaCount(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? 0u : tree->leadingCount;
}

TZrBool ZrParser_TokenTree_LeadingTriviaAt(
        const SZrTokenTree *tree, TZrSize index, SZrTokenTrivia *outTrivia) {
    if (tree == ZR_NULL || outTrivia == ZR_NULL || index >= tree->leadingCount) {
        return ZR_FALSE;
    }
    *outTrivia = tree->leading[index];
    return ZR_TRUE;
}

TZrSize ZrParser_TokenTree_TrailingTriviaCount(const SZrTokenTree *tree) {
    return tree == ZR_NULL ? 0u : tree->trailingCount;
}

TZrBool ZrParser_TokenTree_TrailingTriviaAt(
        const SZrTokenTree *tree, TZrSize index, SZrTokenTrivia *outTrivia) {
    if (tree == ZR_NULL || outTrivia == ZR_NULL || index >= tree->trailingCount) {
        return ZR_FALSE;
    }
    *outTrivia = tree->trailing[index];
    return ZR_TRUE;
}
