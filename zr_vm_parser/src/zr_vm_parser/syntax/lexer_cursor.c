#include "syntax/lexer_internal.h"
#include "parser/parser_ownership.h"
#include "parser/parser_source_projection.h"
#include "syntax/token_kind.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#include <ctype.h>
#include <string.h>

#include "syntax/token_source.h"

void ZrParser_Lexer_Init(SZrLexState *ls, SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName) {
    ZrParser_Lexer_InitBudget(ls, state, source, sourceLength, sourceName, ZR_NULL);
}

void ZrParser_Lexer_InitBudget(
        SZrLexState *ls, SZrState *state, const TZrChar *source,
        TZrSize sourceLength, SZrString *sourceName,
        SZrParserComptimeBudget *budget) {
    ZrParser_Lexer_InitOwnedSource(ls, state, source, sourceLength, sourceName,
                                 ZR_NULL, budget, ZrParser_SyntaxContext_Root(),
                                 ZrParser_TokenSpan_None(), ZR_NULL);
}

void ZrParser_Lexer_InitOwnedSource(
        SZrLexState *ls, SZrState *state, const TZrChar *source,
        TZrSize sourceLength, SZrString *sourceName,
        SZrAstOwnership *owner, SZrParserComptimeBudget *budget,
        SZrSyntaxContext context, SZrTokenSpan origin,
        const SZrFileRange *originLocation) {
    ZR_ASSERT(ls != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(source != ZR_NULL);

    memset(ls, 0, sizeof(*ls));
    ls->state = state;
    ls->source = source;
    ls->sourceLength = sourceLength;
    ls->currentPos = 0;
    ls->lineNumber = 1;
    ls->lastLine = 1;
    ls->currentLineStartOffset = 0;
    ls->tokenStartOffset = 0;
    ls->tokenStartLineStart = 0;
    ls->tokenStartLine = 1;
    ls->sourceName = sourceName;
    ls->textSyntaxContext = context;
    ls->inheritedSpan = origin;
    ls->hasInheritedSpan = origin.sourceId != ZR_TOKEN_SOURCE_INVALID;
    ls->currentTokenHadError = ZR_FALSE;
    ls->currentTokenErrorMessage = ZR_NULL;
    ls->buffer = ZR_NULL;
    ls->bufferSize = 0;
    ls->bufferLength = 0;
    ls->tokenStream = ZR_NULL;
    ls->tokenSourceState = ZR_NULL;
    ls->hasLookahead = ZR_FALSE;
    
    // 初始化 lookahead 相关字段
    ls->lookahead.token = ZR_TK_EOS;
    ls->lookaheadPos = 0;
    ls->lookaheadChar = 0;
    ls->lookaheadLine = 1;
    ls->lookaheadLastLine = 1;
    ls->lookaheadCurrentLineStartOffset = 0;
    ls->lookaheadTokenStartOffset = 0;
    ls->lookaheadTokenStartLineStart = 0;
    ls->lookaheadTokenStartLine = 1;
    ls->filePositionCacheOffset = 0;
    ls->filePositionCacheLineStart = 0;
    ls->filePositionCacheLine = 1;
    ls->sharesOwnership = owner != ZR_NULL;
    if (owner != ZR_NULL) {
        ls->ownership = owner;
        ZrParser_Ownership_Retain(owner);
    } else {
        ls->ownership = ZrParser_Ownership_Create(state, ZR_NULL, budget);
    }
    if (ls->ownership == ZR_NULL ||
        !ZrParser_Lexer_HoldString(ls, sourceName)) {
        ls->t.token = ZR_TK_EOS;
        ls->t.hasLexError = ZR_TRUE;
        ls->t.lexErrorMessage = "Failed to initialize parser resource ownership";
        return;
    }
    if (owner != ZR_NULL) {
        if (originLocation == ZR_NULL ||
            !ZrParser_RangeEvidence_Validate(owner, *originLocation)) {
            ZrParser_SourceProjection_FailLexer(ls);
            return;
        }
        ls->inheritedLocation = *originLocation;
        ls->hasInheritedLocation = ZR_TRUE;
    } else if (originLocation != ZR_NULL || ls->hasInheritedSpan) {
        ZrParser_SourceProjection_FailLexer(ls);
        return;
    }

    // 分配初始缓冲区
    ls->bufferSize = ZR_PARSER_LEXER_BUFFER_INITIAL_SIZE;
    ls->buffer = ZrParser_Ownership_Allocate(
            ls->ownership, ls->bufferSize, ZR_MEMORY_NATIVE_TYPE_STRING, ZR_FALSE);
    if (ls->buffer == ZR_NULL) {
        ls->t.token = ZR_TK_EOS;
        ls->t.hasLexError = ZR_TRUE;
        ls->t.lexErrorMessage = "Failed to allocate lexer buffer";
        return;
    }
    ls->buffer[0] = '\0';

    // 初始化 token
    ls->t.token = ZR_TK_EOS;
    ls->t.hasLexError = ZR_FALSE;
    ls->t.lexErrorMessage = ZR_NULL;
    ls->t.rawText = ZR_NULL;
    ls->t.rawLength = 0U;
    ls->t.span = ZrParser_TokenSpan_None();
    ls->t.syntaxContext = ZrParser_SyntaxContext_Root();
    ls->lookahead.token = ZR_TK_EOS;
    ls->lookahead.hasLexError = ZR_FALSE;
    ls->lookahead.lexErrorMessage = ZR_NULL;
    ls->lookahead.rawText = ZR_NULL;
    ls->lookahead.rawLength = 0U;
    ls->lookahead.span = ZrParser_TokenSpan_None();
    ls->lookahead.syntaxContext = ZrParser_SyntaxContext_Root();

    // 读取第一个字符
    ZrParser_Lexer_ReadChar(ls);

    // 读取第一个 token
    ZrParser_Lexer_Next(ls);
}

void ZrParser_Lexer_InitTokenStream(
        SZrLexState *ls,
        SZrState *state,
        const SZrTokenStream *stream,
        SZrString *sourceName) {
    ZrParser_Lexer_InitTokenStreamBudget(ls, state, stream, sourceName, ZR_NULL);
}

void ZrParser_Lexer_InitTokenStreamBudget(
        SZrLexState *ls, SZrState *state, const SZrTokenStream *stream,
        SZrString *sourceName, SZrParserComptimeBudget *budget) {
    ZR_ASSERT(ls != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stream != ZR_NULL);

    memset(ls, 0, sizeof(*ls));
    ls->state = state;
    ls->sourceName = sourceName;
    ls->lineNumber = 1;
    ls->lastLine = 1;
    ls->tokenStartLine = 1;
    ls->tokenStartOffset = 0;
    ls->tokenStartLineStart = 0;
    ls->t.token = ZR_TK_EOS;
    ls->lookahead.token = ZR_TK_EOS;
    ls->t.syntaxContext = ZrParser_SyntaxContext_Root();
    ls->lookahead.syntaxContext = ZrParser_SyntaxContext_Root();
    ls->ownership = ZrParser_Ownership_Create(state, stream, budget);
    if (ls->ownership == ZR_NULL ||
        !ZrParser_Lexer_HoldString(ls, sourceName)) {
        ls->t.hasLexError = ZR_TRUE;
        ls->t.lexErrorMessage = "Failed to initialize token stream ownership";
        return;
    }
    ZrParser_TokenSource_InitStream(ls, stream);
    if (ls->tokenSourceState == ZR_NULL) {
        ls->t.hasLexError = ZR_TRUE;
        ls->t.lexErrorMessage = "Failed to allocate token stream cursor";
        return;
    }
    ZrParser_Lexer_Next(ls);
}

void ZrParser_Lexer_Free(SZrLexState *ls) {
    if (ls == ZR_NULL) {
        return;
    }

    if (ZrParser_TokenSource_IsStream(ls)) {
        ZrParser_TokenSource_FreeStream(ls);
    }

    if (ls->buffer != ZR_NULL && ls->state != ZR_NULL && ls->state->global != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(ls->state->global,
                                      ls->buffer,
                                      ls->bufferSize,
                                      ZR_MEMORY_NATIVE_TYPE_STRING);
    }
    ls->buffer = ZR_NULL;
    ls->bufferSize = 0u;
    ls->bufferLength = 0u;
    if (!ls->sharesOwnership) {
        ZrParser_Ownership_DetachBudget(ls->ownership);
    }
    ZrParser_Ownership_Release(ls->ownership);
    ls->ownership = ZR_NULL;
}

// 消费缓存的前瞻 token 时恢复扫描后的完整游标，否则从当前字符重新扫描。
void ZrParser_Lexer_Next(SZrLexState *ls) {
    if (ls == ZR_NULL) {
        return;
    }
    if (ZrParser_TokenSource_IsStream(ls)) {
        if (!ZrParser_TokenSource_Next(ls, &ls->t)) {
            memset(&ls->t, 0, sizeof(ls->t));
            ls->t.token = ZR_TK_EOS;
            ls->t.hasLexError = ZR_TRUE;
            ls->t.lexErrorMessage = "Failed to materialize native token";
        }
        ls->tokenStartOffset = ls->t.span.startOffset;
        ls->tokenStartLine = ls->t.span.startLine;
        ls->tokenStartLineStart = ls->t.span.startOffset >=
                                           (TZrSize)(ls->t.span.startColumn > 0
                                                             ? ls->t.span.startColumn - 1
                                                             : 0)
                                           ? ls->t.span.startOffset -
                                                     (TZrSize)(ls->t.span.startColumn > 0
                                                                       ? ls->t.span.startColumn - 1
                                                                       : 0)
                                           : ls->t.span.startOffset;
        ls->currentPos = ls->t.span.endOffset;
        ls->currentChar = ZR_PARSER_LEXER_EOZ;
        ls->lineNumber = ls->t.span.endLine > 0 ? ls->t.span.endLine : 1;
        ls->lastLine = ls->t.span.startLine > 0 ? ls->t.span.startLine : ls->lineNumber;
        ls->currentLineStartOffset = ls->tokenStartLineStart;
        return;
    }
    if (ls->hasSplitPending) {
        ls->t = ls->splitPending;
        ls->hasSplitPending = ZR_FALSE;
        ls->tokenStartOffset = ls->t.span.startOffset;
        ls->tokenStartLine = ls->t.span.startLine;
        ls->tokenStartLineStart = ls->t.span.startOffset >=
                                           (TZrSize)(ls->t.span.startColumn > 0
                                                             ? ls->t.span.startColumn - 1
                                                             : 0)
                                           ? ls->t.span.startOffset -
                                                     (TZrSize)(ls->t.span.startColumn > 0
                                                                       ? ls->t.span.startColumn - 1
                                                                       : 0)
                                           : ls->t.span.startOffset;
        return;
    }
    // 如果 lookahead 已经被缓存，使用它而不是重新读取
    // 这样可以避免重复读取，提高性能，并且确保 lookahead 缓存被正确清除
    if (ls->hasLookahead) {
        ls->t = ls->lookahead;
        // 恢复下一个 token 的位置（推进 lexer 状态）
        ls->currentPos = ls->lookaheadPos;
        ls->currentChar = ls->lookaheadChar;
        ls->lineNumber = ls->lookaheadLine;
        ls->lastLine = ls->lookaheadLastLine;
        ls->currentLineStartOffset = ls->lookaheadCurrentLineStartOffset;
        ls->tokenStartOffset = ls->lookaheadTokenStartOffset;
        ls->tokenStartLineStart = ls->lookaheadTokenStartLineStart;
        ls->tokenStartLine = ls->lookaheadTokenStartLine;
        ls->hasLookahead = ZR_FALSE;
    } else {
        TZrSemInfo seminfo;
        memset(&seminfo, 0, sizeof(seminfo));
        ls->currentTokenHadError = ZR_FALSE;
        ls->currentTokenErrorMessage = ZR_NULL;
        ls->t.token = ZrParser_Lexer_Scan(ls, &seminfo);
        ls->t.seminfo = seminfo;
        ls->t.hasLexError = ls->currentTokenHadError;
        ls->t.lexErrorMessage = ls->currentTokenErrorMessage;
        ZrParser_Lexer_SetTextTokenMetadata(ls);
    }
}

// 扫描一次后保存游标终态，再恢复当前 token；下次 Next 才提交前瞻状态。
EZrToken ZrParser_Lexer_Lookahead(SZrLexState *ls) {
    if (ls == ZR_NULL) {
        return ZR_TK_EOS;
    }
    if (ZrParser_TokenSource_IsStream(ls)) {
        SZrToken token;
        return ZrParser_TokenSource_Lookahead(ls, &token) ? token.token : ZR_TK_EOS;
    }
    if (ls->hasSplitPending) {
        return ls->splitPending.token;
    }
    if (!ls->hasLookahead) {
        // 保存当前状态（包括所有可能被 ZrParser_Lexer_Scan 修改的字段）
        TZrSize savedPos = ls->currentPos;
        TZrInt32 savedChar = ls->currentChar;
        TZrInt32 savedLine = ls->lineNumber;
        TZrInt32 savedLastLine = ls->lastLine;
        TZrSize savedCurrentLineStartOffset = ls->currentLineStartOffset;
        TZrSize savedTokenStartOffset = ls->tokenStartOffset;
        TZrSize savedTokenStartLineStart = ls->tokenStartLineStart;
        TZrInt32 savedTokenStartLine = ls->tokenStartLine;
        SZrToken savedToken = ls->t;

        // 读取下一个 token
        TZrSemInfo seminfo;
        memset(&seminfo, 0, sizeof(seminfo));
        ls->currentTokenHadError = ZR_FALSE;
        ls->currentTokenErrorMessage = ZR_NULL;
        ls->lookahead.token = ZrParser_Lexer_Scan(ls, &seminfo);
        ls->lookahead.seminfo = seminfo;
        ls->lookahead.hasLexError = ls->currentTokenHadError;
        ls->lookahead.lexErrorMessage = ls->currentTokenErrorMessage;
        ZrParser_Lexer_SetTextTokenMetadata(ls);
        ls->lookahead.rawText = ls->t.rawText;
        ls->lookahead.rawLength = ls->t.rawLength;
        ls->lookahead.span = ls->t.span;
        ls->lookahead.syntaxContext = ls->t.syntaxContext;
        ls->lookahead.punctPieceCount = ls->t.punctPieceCount;
        memcpy(ls->lookahead.punctPieces, ls->t.punctPieces,
               sizeof(ls->lookahead.punctPieces));

        // 保存下一个 token 的位置（当使用缓存的 lookahead 时，需要恢复到这个位置）
        ls->lookaheadPos = ls->currentPos;
        ls->lookaheadChar = ls->currentChar;
        ls->lookaheadLine = ls->lineNumber;
        ls->lookaheadLastLine = ls->lastLine;
        ls->lookaheadCurrentLineStartOffset = ls->currentLineStartOffset;
        ls->lookaheadTokenStartOffset = ls->tokenStartOffset;
        ls->lookaheadTokenStartLineStart = ls->tokenStartLineStart;
        ls->lookaheadTokenStartLine = ls->tokenStartLine;

        // 恢复状态（必须恢复所有被修改的字段）
        ls->currentPos = savedPos;
        ls->currentChar = savedChar;
        ls->lineNumber = savedLine;
        ls->lastLine = savedLastLine;
        ls->currentLineStartOffset = savedCurrentLineStartOffset;
        ls->tokenStartOffset = savedTokenStartOffset;
        ls->tokenStartLineStart = savedTokenStartLineStart;
        ls->tokenStartLine = savedTokenStartLine;
        ls->t = savedToken;
        ls->hasLookahead = ZR_TRUE;
    }
    return ls->lookahead.token;
}

TZrBool ZrParser_Lexer_SplitCurrentPunct(SZrLexState *ls, TZrChar firstPunct) {
    if (ls == ZR_NULL || ls->t.rawLength < 2U || ls->t.rawText == ZR_NULL ||
        ls->t.rawText[0] != firstPunct) {
        return ZR_FALSE;
    }
    if (ZrParser_TokenSource_IsStream(ls)) {
        return ZrParser_TokenSource_SplitCurrentPunct(ls, firstPunct);
    }
    {
        SZrToken original = ls->t;
        SZrToken first = original;
        SZrToken second = {0};
        EZrToken remainder;
        if (original.rawLength > 3u) {
            return ZR_FALSE;
        }
        if (!ZrParser_TokenSpan_Subspan(original.span, original.span.startOffset,
                                        original.span.startOffset + 1U,
                                        &first.span) ||
            !ZrParser_TokenSpan_Subspan(original.span, original.span.startOffset + 1U,
                                        original.span.endOffset,
                                        &second.span)) {
            return ZR_FALSE;
        }
        remainder = original.rawLength == 2u
                            ? ZrParser_Token_SinglePunct((TZrUInt8)original.rawText[1])
                            : ZrParser_Token_CompoundPunct(
                                      (TZrUInt8)original.rawText[1],
                                      (TZrUInt8)original.rawText[2], 0u, ZR_FALSE);
        if (remainder == ZR_TK_EOS) {
            return ZR_FALSE;
        }
        second.token = remainder;
        second.rawText = original.rawText + 1;
        second.rawLength = original.rawLength - 1u;
        second.syntaxContext = original.syntaxContext;
        first.token = ZrParser_Token_SinglePunct((TZrUInt8)firstPunct);
        first.rawText = original.rawText;
        first.rawLength = 1U;
        ls->t = first;
        ls->splitPending = second;
    }
    if (ls->splitPending.rawText == ZR_NULL) {
        return ZR_FALSE;
    }
    ls->hasSplitPending = ZR_TRUE;
    return ZR_TRUE;
}

// 诊断位置的列号回溯；与主扫描器的换行标准必须一致。
