//
// Artifact golden regression test for .zrs/.zri/.zro outputs.
//

#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "test_utils.h"

void setUp(void) {}

void tearDown(void) {}

void test_basic_operations_artifacts_match_golden(void);

static TZrBool is_text_artifact_extension(const TZrChar* extension) {
    if (extension == ZR_NULL) {
        return ZR_FALSE;
    }

    return strcmp(extension, ".zrs") == 0 || strcmp(extension, ".zri") == 0;
}

static TZrBool normalize_text_newlines(const TZrBytePtr buffer,
                                     TZrSize length,
                                     TZrBytePtr* outBuffer,
                                     TZrSize* outLength) {
    TZrSize sourceIndex = 0;
    TZrSize targetIndex = 0;
    TZrBytePtr normalized;

    if (buffer == ZR_NULL || outBuffer == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    normalized = (TZrBytePtr)malloc(length + 1);
    if (normalized == ZR_NULL) {
        return ZR_FALSE;
    }

    while (sourceIndex < length) {
        if (buffer[sourceIndex] == '\r') {
            sourceIndex++;
            continue;
        }

        normalized[targetIndex++] = buffer[sourceIndex++];
    }

    normalized[targetIndex] = '\0';
    *outBuffer = normalized;
    *outLength = targetIndex;
    return ZR_TRUE;
}

static TZrBool read_file_bytes(const TZrChar* path, TZrBytePtr* outBuffer, TZrSize* outLength) {
    if (path == ZR_NULL || outBuffer == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    FILE* file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    long fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    TZrBytePtr buffer = (TZrBytePtr)malloc((TZrSize)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }

    TZrSize readSize = (TZrSize)fread(buffer, 1, (TZrSize)fileSize, file);
    fclose(file);
    if (readSize != (TZrSize)fileSize) {
        free(buffer);
        return ZR_FALSE;
    }

    buffer[readSize] = '\0';
    *outBuffer = buffer;
    *outLength = readSize;
    return ZR_TRUE;
}

static void assert_file_matches_golden(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension) {
    TZrChar generatedPath[1024];
    TZrChar goldenPath[1024];
    TZrBytePtr generatedBuffer = ZR_NULL;
    TZrBytePtr goldenBuffer = ZR_NULL;
    TZrSize generatedLength = 0;
    TZrSize goldenLength = 0;

    get_output_path(baseName, subDir, extension, generatedPath, sizeof(generatedPath));
    get_golden_output_path(baseName, subDir, extension, goldenPath, sizeof(goldenPath));

    TEST_ASSERT_TRUE_MESSAGE(read_file_bytes(generatedPath, &generatedBuffer, &generatedLength), generatedPath);
    TEST_ASSERT_TRUE_MESSAGE(read_file_bytes(goldenPath, &goldenBuffer, &goldenLength), goldenPath);

    if (is_text_artifact_extension(extension)) {
        TZrBytePtr normalizedGenerated = ZR_NULL;
        TZrBytePtr normalizedGolden = ZR_NULL;
        TZrSize normalizedGeneratedLength = 0;
        TZrSize normalizedGoldenLength = 0;

        TEST_ASSERT_TRUE_MESSAGE(
            normalize_text_newlines(generatedBuffer, generatedLength, &normalizedGenerated, &normalizedGeneratedLength),
            generatedPath);
        TEST_ASSERT_TRUE_MESSAGE(
            normalize_text_newlines(goldenBuffer, goldenLength, &normalizedGolden, &normalizedGoldenLength),
            goldenPath);

        free(generatedBuffer);
        free(goldenBuffer);
        generatedBuffer = normalizedGenerated;
        goldenBuffer = normalizedGolden;
        generatedLength = normalizedGeneratedLength;
        goldenLength = normalizedGoldenLength;
    }

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(goldenLength, generatedLength, goldenPath);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(goldenBuffer, generatedBuffer, goldenLength, goldenPath);

    free(generatedBuffer);
    free(goldenBuffer);
}

void test_basic_operations_artifacts_match_golden(void) {
    const TZrChar* caseFileName = "artifact_baseline.zr";
    const TZrChar* baseName = "artifact_baseline";
    TZrChar sourcePath[1024];
    TZrSize sourceLength = 0;
    TZrChar* source = ZR_NULL;
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    get_test_case_path(caseFileName, sourcePath, sizeof(sourcePath));
    source = load_zr_file(sourcePath, &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    SZrTestResult* result = parse_and_compile(state, source, sourceLength, sourcePath);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE_MESSAGE(result->success, result->errorMessage ? result->errorMessage : "parse_and_compile failed");

    TEST_ASSERT_TRUE(dump_ast_to_file(state, result->ast, baseName));
    TEST_ASSERT_TRUE(dump_intermediate_to_file(state, result->function, baseName));
    TEST_ASSERT_TRUE(dump_binary_to_file(state, result->function, baseName));

    assert_file_matches_golden(baseName, "ast", ".zrs");
    assert_file_matches_golden(baseName, "intermediate", ".zri");
    assert_file_matches_golden(baseName, "binary", ".zro");

    free_test_result(result);
    free(source);
    destroy_test_state(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basic_operations_artifacts_match_golden);
    return UNITY_END();
}
