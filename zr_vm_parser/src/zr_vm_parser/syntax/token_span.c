#include "zr_vm_parser/token_span.h"

static TZrBool token_span_same_source(SZrTokenSpan left, SZrTokenSpan right) {
    return left.sourceId != ZR_TOKEN_SOURCE_INVALID &&
           left.sourceId == right.sourceId;
}

SZrTokenSpan ZrParser_TokenSpan_None(void) {
    SZrTokenSpan span = {0};
    return span;
}

TZrBool ZrParser_TokenSpan_IsValid(SZrTokenSpan span) {
    if (span.sourceId == ZR_TOKEN_SOURCE_INVALID ||
        span.startOffset > span.endOffset ||
        span.startLine <= 0 || span.endLine <= 0 ||
        span.startColumn <= 0 || span.endColumn <= 0) {
        return ZR_FALSE;
    }
    if (span.startLine > span.endLine) {
        return ZR_FALSE;
    }
    if (span.startLine == span.endLine && span.startColumn > span.endColumn) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_TokenSpan_Join(
        SZrTokenSpan left, SZrTokenSpan right, SZrTokenSpan *outSpan) {
    SZrTokenSpan result;

    if (outSpan == ZR_NULL || !ZrParser_TokenSpan_IsValid(left) ||
        !ZrParser_TokenSpan_IsValid(right) ||
        !token_span_same_source(left, right)) {
        return ZR_FALSE;
    }
    result = left;
    if (right.startOffset < result.startOffset) {
        result.startOffset = right.startOffset;
        result.startLine = right.startLine;
        result.startColumn = right.startColumn;
    }
    if (right.endOffset > result.endOffset) {
        result.endOffset = right.endOffset;
        result.endLine = right.endLine;
        result.endColumn = right.endColumn;
    }
    *outSpan = result;
    return ZR_TRUE;
}

TZrBool ZrParser_TokenSpan_Subspan(
        SZrTokenSpan span,
        TZrSize startOffset,
        TZrSize endOffset,
        SZrTokenSpan *outSpan) {
    TZrSize startDelta;
    TZrSize endDelta;

    if (outSpan == ZR_NULL || !ZrParser_TokenSpan_IsValid(span) ||
        span.startLine != span.endLine ||
        startOffset < span.startOffset || endOffset > span.endOffset ||
        startOffset > endOffset) {
        return ZR_FALSE;
    }
    startDelta = startOffset - span.startOffset;
    endDelta = endOffset - span.startOffset;
    if (startDelta > (TZrSize)INT32_MAX || endDelta > (TZrSize)INT32_MAX ||
        span.startColumn > INT32_MAX - (TZrInt32)endDelta) {
        return ZR_FALSE;
    }
    *outSpan = span;
    outSpan->startOffset = startOffset;
    outSpan->endOffset = endOffset;
    outSpan->startColumn = span.startColumn + (TZrInt32)startDelta;
    outSpan->endColumn = span.startColumn + (TZrInt32)endDelta;
    return ZR_TRUE;
}
