#include "legacy_migration_module_specifier.h"

#include <ctype.h>
#include <string.h>

static TZrBool module_specifier_span_equals(
        const TZrChar *source,
        TZrSize start,
        TZrSize end,
        const TZrChar *expected) {
    TZrSize expectedLength = strlen(expected);

    return end >= start && end - start == expectedLength &&
           memcmp(source + start, expected, expectedLength) == 0;
}

static TZrSize module_specifier_skip_trivia(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize start) {
    for (;;) {
        while (start < sourceLength && isspace((unsigned char)source[start])) {
            start++;
        }
        if (start + 1U >= sourceLength || source[start] != '/') {
            return start;
        }
        if (source[start + 1U] == '/') {
            start += 2U;
            while (start < sourceLength && source[start] != '\n') {
                start++;
            }
            continue;
        }
        if (source[start + 1U] == '*') {
            start += 2U;
            while (start + 1U < sourceLength &&
                   !(source[start] == '*' && source[start + 1U] == '/')) {
                start++;
            }
            if (start + 1U >= sourceLength) {
                return sourceLength;
            }
            start += 2U;
            continue;
        }
        return start;
    }
}

TZrBool ZrParser_LegacyMigrationModuleSpecifier_TryMatchBareDebugImport(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize wordStart,
        TZrSize wordEnd,
        SZrLegacyMigrationModuleSpecifierMatch *outMatch) {
    TZrSize cursor;
    TZrSize literalStart;
    TZrSize literalEnd;

    if (source == ZR_NULL || outMatch == ZR_NULL || wordEnd > sourceLength ||
        !module_specifier_span_equals(source, wordStart, wordEnd, "import") ||
        (wordStart > 0U && source[wordStart - 1U] == '.')) {
        return ZR_FALSE;
    }

    cursor = module_specifier_skip_trivia(source, sourceLength, wordEnd);
    if (cursor >= sourceLength || source[cursor] != '(') {
        return ZR_FALSE;
    }
    cursor = module_specifier_skip_trivia(source, sourceLength, cursor + 1U);
    if (cursor >= sourceLength || source[cursor] != '"') {
        return ZR_FALSE;
    }
    literalStart = cursor + 1U;
    literalEnd = literalStart + strlen("debug");
    if (literalEnd >= sourceLength ||
        !module_specifier_span_equals(source, literalStart, literalEnd, "debug") ||
        source[literalEnd] != '"') {
        return ZR_FALSE;
    }
    cursor = module_specifier_skip_trivia(source, sourceLength, literalEnd + 1U);
    if (cursor >= sourceLength || source[cursor] != ')') {
        return ZR_FALSE;
    }

    outMatch->itemEnd = cursor + 1U;
    outMatch->editStart = literalStart;
    outMatch->editEnd = literalEnd;
    return ZR_TRUE;
}
