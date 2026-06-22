#include "semantic/lsp_numeric_range_text.h"

#include "zr_vm_language_server/conf.h"

#include <stdarg.h>
#include <stdio.h>

static TZrBool lsp_numeric_range_text_append_format(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *used,
                                                    const TZrChar *format,
                                                    ...) {
    va_list args;
    int written;

    if (buffer == ZR_NULL || used == ZR_NULL || format == ZR_NULL || *used >= bufferSize) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, args);
    va_end(args);

    if (written < 0 || (TZrSize)written >= bufferSize - *used) {
        buffer[bufferSize - 1u] = '\0';
        return ZR_FALSE;
    }

    *used += (TZrSize)written;
    return ZR_TRUE;
}

static TZrBool lsp_numeric_range_text_is_float_fact(const SZrSemanticNumericFact *fact) {
    return fact != ZR_NULL &&
           (fact->sourceType == ZR_VALUE_TYPE_FLOAT ||
            fact->sourceType == ZR_VALUE_TYPE_DOUBLE ||
            fact->targetType == ZR_VALUE_TYPE_FLOAT ||
            fact->targetType == ZR_VALUE_TYPE_DOUBLE);
}

static TZrBool lsp_numeric_range_text_append_segments(TZrChar *buffer,
                                                      TZrSize bufferSize,
                                                      TZrSize *used,
                                                      const SZrSemanticNumericFact *fact) {
    TZrSize segmentCount;
    TZrSize displayCount;

    if (fact == ZR_NULL || fact->rangeSegmentCount == 0) {
        return ZR_TRUE;
    }

    segmentCount = fact->rangeSegmentCount;
    displayCount = segmentCount > ZR_LSP_NUMERIC_RANGE_SEGMENT_DISPLAY_LIMIT
                       ? ZR_LSP_NUMERIC_RANGE_SEGMENT_DISPLAY_LIMIT
                       : segmentCount;
    if (!lsp_numeric_range_text_append_format(buffer, bufferSize, used, " (segments")) {
        return ZR_FALSE;
    }

    for (TZrSize segmentIndex = 0; segmentIndex < displayCount; segmentIndex++) {
        const SZrNumericRangeSegment *segment =
            ZrParser_SemanticNumericFact_RangeSegmentAt(fact, segmentIndex);
        if (segment == ZR_NULL ||
            !lsp_numeric_range_text_append_format(buffer,
                                                  bufferSize,
                                                  used,
                                                  "%s %lld..%lld",
                                                  segmentIndex == 0 ? "" : ",",
                                                  (long long)segment->minValue,
                                                  (long long)segment->maxValue)) {
            return ZR_FALSE;
        }
    }

    if (displayCount < segmentCount &&
        !lsp_numeric_range_text_append_format(buffer,
                                              bufferSize,
                                              used,
                                              ", ... +%llu more",
                                              (unsigned long long)(segmentCount - displayCount))) {
        return ZR_FALSE;
    }

    return lsp_numeric_range_text_append_format(buffer, bufferSize, used, ")");
}

TZrBool ZrLanguageServer_LspNumericRangeText_AppendRange(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticNumericFact *fact,
    TZrBool includeRangeLabel) {
    if (fact == ZR_NULL || !fact->hasRange) {
        return ZR_TRUE;
    }

    if (includeRangeLabel &&
        !lsp_numeric_range_text_append_format(buffer, bufferSize, used, "range ")) {
        return ZR_FALSE;
    }

    if (lsp_numeric_range_text_is_float_fact(fact)) {
        return lsp_numeric_range_text_append_format(buffer,
                                                    bufferSize,
                                                    used,
                                                    "%.17g..%.17g",
                                                    fact->minDoubleValue,
                                                    fact->maxDoubleValue);
    }

    if (!lsp_numeric_range_text_append_format(buffer,
                                              bufferSize,
                                              used,
                                              "%lld..%lld",
                                              (long long)fact->minValue,
                                              (long long)fact->maxValue)) {
        return ZR_FALSE;
    }

    return lsp_numeric_range_text_append_segments(buffer, bufferSize, used, fact);
}
