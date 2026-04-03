#include "reference_support.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

static TZrBool zr_tests_reference_match_here(const TZrChar *pattern, const TZrChar *text);
static const TZrChar *zr_tests_reference_skip_json_spacing(const TZrChar *cursor);

static TZrBool zr_tests_reference_match_star(TZrChar token, const TZrChar *pattern, const TZrChar *text) {
    do {
        if (zr_tests_reference_match_here(pattern, text)) {
            return ZR_TRUE;
        }
    } while (*text != '\0' && (*text++ == token || token == '.'));

    return ZR_FALSE;
}

static TZrBool zr_tests_reference_match_here(const TZrChar *pattern, const TZrChar *text) {
    if (pattern[0] == '\0') {
        return ZR_TRUE;
    }

    if (pattern[1] == '*') {
        return zr_tests_reference_match_star(pattern[0], pattern + 2, text);
    }

    if (pattern[0] == '$' && pattern[1] == '\0') {
        return text[0] == '\0' ? ZR_TRUE : ZR_FALSE;
    }

    if (text[0] != '\0' && (pattern[0] == '.' || pattern[0] == text[0])) {
        return zr_tests_reference_match_here(pattern + 1, text + 1);
    }

    return ZR_FALSE;
}

static const TZrChar *zr_tests_reference_skip_json_spacing(const TZrChar *cursor) {
    while (cursor != ZR_NULL && *cursor != '\0' && isspace((unsigned char)*cursor)) {
        cursor += 1;
    }

    return cursor;
}

TZrChar *ZrTests_Reference_ReadFixture(const TZrChar *relativePath, TZrSize *outLength) {
    TZrChar path[ZR_TESTS_PATH_MAX];

    if (!ZrTests_Path_GetFixture("reference", relativePath, path, sizeof(path))) {
        return ZR_NULL;
    }

    return ZrTests_ReadTextFile(path, outLength);
}

TZrChar *ZrTests_Reference_ReadDoc(const TZrChar *relativePath, TZrSize *outLength) {
    TZrChar path[ZR_TESTS_PATH_MAX];

    if (!ZrTests_Path_GetRepoDoc(relativePath, path, sizeof(path))) {
        return ZR_NULL;
    }

    return ZrTests_ReadTextFile(path, outLength);
}

TZrSize ZrTests_Reference_CountOccurrences(const TZrChar *text, const TZrChar *needle) {
    TZrSize count = 0;
    const TZrChar *cursor;
    TZrSize needleLength;

    if (text == ZR_NULL || needle == ZR_NULL || needle[0] == '\0') {
        return 0;
    }

    needleLength = strlen(needle);
    cursor = text;
    while ((cursor = strstr(cursor, needle)) != ZR_NULL) {
        count += 1;
        cursor += needleLength;
    }

    return count;
}

TZrSize ZrTests_Reference_CountJsonStringFieldValueOccurrences(const TZrChar *text,
                                                               const TZrChar *fieldName,
                                                               const TZrChar *value) {
    TZrChar fieldPattern[128];
    TZrSize count = 0;
    const TZrChar *cursor;
    TZrSize fieldPatternLength;
    TZrSize valueLength;

    if (text == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL || fieldName[0] == '\0' || value[0] == '\0') {
        return 0;
    }

    if (snprintf(fieldPattern, sizeof(fieldPattern), "\"%s\"", fieldName) < 0) {
        return 0;
    }

    fieldPatternLength = strlen(fieldPattern);
    valueLength = strlen(value);
    cursor = text;

    while ((cursor = strstr(cursor, fieldPattern)) != ZR_NULL) {
        const TZrChar *probe = zr_tests_reference_skip_json_spacing(cursor + fieldPatternLength);

        if (probe != ZR_NULL && *probe == ':') {
            probe = zr_tests_reference_skip_json_spacing(probe + 1);
            if (probe != ZR_NULL && probe[0] == '"' && strncmp(probe + 1, value, valueLength) == 0 &&
                probe[valueLength + 1] == '"') {
                count += 1;
            }
        }

        cursor += fieldPatternLength;
    }

    return count;
}

TZrBool ZrTests_Reference_TextContainsAll(const TZrChar *text,
                                          const TZrChar *const *needles,
                                          TZrSize needleCount) {
    TZrSize index;

    if (text == ZR_NULL || needles == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < needleCount; index++) {
        if (needles[index] == ZR_NULL || strstr(text, needles[index]) == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrTests_Reference_TextContainsInOrder(const TZrChar *text,
                                              const TZrChar *const *fragments,
                                              TZrSize fragmentCount) {
    const TZrChar *cursor;
    TZrSize index;

    if (text == ZR_NULL || fragments == ZR_NULL) {
        return ZR_FALSE;
    }

    cursor = text;
    for (index = 0; index < fragmentCount; index++) {
        if (fragments[index] == ZR_NULL) {
            return ZR_FALSE;
        }

        cursor = strstr(cursor, fragments[index]);
        if (cursor == ZR_NULL) {
            return ZR_FALSE;
        }
        cursor += strlen(fragments[index]);
    }

    return ZR_TRUE;
}

TZrBool ZrTests_Reference_TextMatchesRegex(const TZrChar *text, const TZrChar *pattern) {
    if (text == ZR_NULL || pattern == ZR_NULL) {
        return ZR_FALSE;
    }

    if (pattern[0] == '^') {
        return zr_tests_reference_match_here(pattern + 1, text);
    }

    do {
        if (zr_tests_reference_match_here(pattern, text)) {
            return ZR_TRUE;
        }
    } while (*text++ != '\0');

    return ZR_FALSE;
}

void ZrTests_Reference_AssertManifestShape(const TZrChar *manifestText,
                                           const TZrChar *domainSlug,
                                           TZrSize minimumCases,
                                           const TZrChar *const *requiredFields,
                                           TZrSize requiredFieldCount) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(manifestText);
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "\"domain\""));
    TEST_ASSERT_NOT_NULL(strstr(manifestText, domainSlug));
    TEST_ASSERT_TRUE(ZrTests_Reference_CountOccurrences(manifestText, "\"id\"") >= minimumCases);

    for (index = 0; index < requiredFieldCount; index++) {
        TEST_ASSERT_NOT_NULL(strstr(manifestText, requiredFields[index]));
    }
}

void ZrTests_Reference_AssertCaseKindsCovered(const TZrChar *manifestText,
                                              const TZrChar *const *caseKinds,
                                              TZrSize caseKindCount,
                                              TZrSize minimumOccurrencesPerKind) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(manifestText);
    for (index = 0; index < caseKindCount; index++) {
        TEST_ASSERT_TRUE(ZrTests_Reference_CountJsonStringFieldValueOccurrences(manifestText, "case_kind", caseKinds[index]) >=
                         minimumOccurrencesPerKind);
    }
}

void ZrTests_Reference_AssertJsonStringFieldValueCoverage(const TZrChar *text,
                                                          const TZrChar *fieldName,
                                                          const TZrChar *const *values,
                                                          TZrSize valueCount,
                                                          TZrSize minimumOccurrencesPerValue) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(fieldName);
    TEST_ASSERT_NOT_NULL(values);

    for (index = 0; index < valueCount; index++) {
        TEST_ASSERT_TRUE(
            ZrTests_Reference_CountJsonStringFieldValueOccurrences(text, fieldName, values[index]) >=
            minimumOccurrencesPerValue);
    }
}

void ZrTests_Reference_AssertFieldCoverage(const TZrChar *text,
                                           const TZrChar *const *fields,
                                           TZrSize fieldCount,
                                           TZrSize minimumOccurrencesPerField) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(fields);

    for (index = 0; index < fieldCount; index++) {
        TEST_ASSERT_TRUE(ZrTests_Reference_CountOccurrences(text, fields[index]) >= minimumOccurrencesPerField);
    }
}
