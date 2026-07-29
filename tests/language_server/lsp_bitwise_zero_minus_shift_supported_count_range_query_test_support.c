#include "lsp_bitwise_zero_minus_shift_supported_count_range_query_test_support.h"

#include <stdio.h>
#include <string.h>

#include "lsp_numeric_range_query_test_support.h"

TZrBool ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
        SZrState *state,
        const TZrChar *caseName,
        const TZrChar *expression,
        TZrInt64 expectedMin,
        TZrInt64 expectedMax) {
    TZrChar content[1024];
    TZrChar label[256];
    TZrChar uri[256];
    int written;

    written = snprintf(
            content,
            sizeof(content),
            "fn calc(seed: u8): int {\n"
            "    var unit: int = (seed %% 2) + 2;\n"
            "    var zero: int = 0;\n"
            "    var allOnes: int = 0 - 1;\n"
            "    var span: int = seed - 128;\n"
            "    var negativeUnit: int = zero - unit;\n"
            "    var negativeFour: int = zero - (unit + 1);\n"
            "    return %s;\n"
            "}\n",
            expression);
    if (written <= 0 || (size_t)written >= sizeof(content)) {
        printf("FAIL: unable to format %s source\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            label,
            sizeof(label),
            "bitwise zero-minus shift supported-count %s range",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(label)) {
        printf("FAIL: unable to format %s label\n", caseName);
        return ZR_FALSE;
    }

    written = snprintf(
            uri,
            sizeof(uri),
            "file:///local_%s_bitwise_zero_minus_shift_supported_count_range_fact.zr",
            caseName);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        printf("FAIL: unable to format %s uri\n", caseName);
        return ZR_FALSE;
    }

    return ZrVmTest_LspRunAssignmentRangeCaseAt(
            state,
            label,
            uri,
            content,
            "|",
            0,
            expectedMin,
            expectedMax);
}
