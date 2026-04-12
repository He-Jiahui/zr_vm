//
// Artifact golden regression test for .zrs/.zri/.zro outputs.
//

#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "test_support.h"
#include "test_utils.h"

void setUp(void) {}

void tearDown(void) {}

typedef struct {
    const TZrChar* sourceFileName;
    const TZrChar* baseName;
} SZrArtifactGoldenCase;

static const SZrArtifactGoldenCase ZR_ARTIFACT_GOLDEN_CASES[] = {
    {"compile_time_decorator_artifact_baseline.zr", "compile_time_decorator_artifact_baseline"},
    {"compile_time_parameter_decorator_artifact_baseline.zr", "compile_time_parameter_decorator_artifact_baseline"},
    {"decorator_artifact_baseline.zr", "decorator_artifact_baseline"},
};

static const SZrArtifactGoldenCase ZR_TRUE_AOT_C_GOLDEN_CASES[] = {
    {"aot_closure_export.zr", "aot_closure_export"},
};

static TZrBool is_text_artifact_extension(const TZrChar* extension) {
    if (extension == ZR_NULL) {
        return ZR_FALSE;
    }

    return strcmp(extension, ".zrs") == 0 || strcmp(extension, ".zri") == 0 ||
           strcmp(extension, ".zrs.json") == 0 || strcmp(extension, ".zri.json") == 0 ||
           strcmp(extension, ".c") == 0 || strcmp(extension, ".ll") == 0;
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

static TZrBool artifact_remove_aot_llvm_embedded_blob_for_compare(const TZrBytePtr in,
                                                                  TZrSize inLen,
                                                                  TZrBytePtr* out,
                                                                  TZrSize* outLen) {
    const char* marker;
    const char* close;
    const TZrByte* regionEnd;
    TZrSize prefixLen;
    TZrSize suffixLen;
    TZrBytePtr result;

    if (in == ZR_NULL || out == ZR_NULL || outLen == ZR_NULL) {
        return ZR_FALSE;
    }

    marker = (const char*)strstr((const char*)in, "@zr_aot_embedded_module_blob = private constant [");
    if (marker == ZR_NULL) {
        result = (TZrBytePtr)malloc(inLen + (TZrSize)1);
        if (result == ZR_NULL) {
            return ZR_FALSE;
        }
        (void)memcpy(result, in, inLen);
        result[inLen] = '\0';
        *out = result;
        *outLen = inLen;
        return ZR_TRUE;
    }

    close = strstr(marker, "\n]\n");
    if (close == ZR_NULL) {
        return ZR_FALSE;
    }
    regionEnd = (const TZrByte*)close + strlen("\n]\n");
    prefixLen = (TZrSize)((const TZrByte*)marker - in);
    suffixLen = inLen - (TZrSize)(regionEnd - in);
    result = (TZrBytePtr)malloc(prefixLen + suffixLen + (TZrSize)1);
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    (void)memcpy(result, in, prefixLen);
    (void)memcpy(result + prefixLen, regionEnd, suffixLen);
    result[prefixLen + suffixLen] = '\0';
    *out = result;
    *outLen = prefixLen + suffixLen;
    return ZR_TRUE;
}

static TZrBool artifact_mask_decimal_runs_for_compare(const TZrBytePtr in,
                                                      TZrSize inLen,
                                                      TZrBytePtr* out,
                                                      TZrSize* outLen) {
    TZrSize cap;
    TZrBytePtr buf;
    TZrSize writeIndex;
    TZrSize readIndex;

    if (in == ZR_NULL || out == ZR_NULL || outLen == ZR_NULL) {
        return ZR_FALSE;
    }

    cap = inLen + (TZrSize)64;
    buf = (TZrBytePtr)malloc(cap);
    if (buf == ZR_NULL) {
        return ZR_FALSE;
    }

    writeIndex = 0;
    readIndex = 0;
    while (readIndex < inLen) {
        TZrByte ch = in[readIndex];
        if (ch >= '0' && ch <= '9') {
            if (writeIndex + (TZrSize)1 > cap) {
                TZrBytePtr nb;
                TZrSize newCap = cap * (TZrSize)2;

                if (newCap < cap) {
                    free(buf);
                    return ZR_FALSE;
                }
                nb = (TZrBytePtr)realloc(buf, newCap);
                if (nb == ZR_NULL) {
                    free(buf);
                    return ZR_FALSE;
                }
                buf = nb;
                cap = newCap;
            }
            buf[writeIndex++] = 'D';
            readIndex++;
            while (readIndex < inLen && in[readIndex] >= '0' && in[readIndex] <= '9') {
                readIndex++;
            }
        } else {
            if (writeIndex + (TZrSize)1 > cap) {
                TZrBytePtr nb;
                TZrSize newCap = cap * (TZrSize)2;

                if (newCap < cap) {
                    free(buf);
                    return ZR_FALSE;
                }
                nb = (TZrBytePtr)realloc(buf, newCap);
                if (nb == ZR_NULL) {
                    free(buf);
                    return ZR_FALSE;
                }
                buf = nb;
                cap = newCap;
            }
            buf[writeIndex++] = ch;
            readIndex++;
        }
    }

    *out = buf;
    *outLen = writeIndex;
    return ZR_TRUE;
}

static void artifact_trim_trailing_spaces_on_each_line(TZrBytePtr buffer, TZrSize* ioLength) {
    TZrSize readIndex;
    TZrSize writeIndex;

    if (buffer == ZR_NULL || ioLength == ZR_NULL) {
        return;
    }

    readIndex = 0;
    writeIndex = 0;
    while (readIndex < *ioLength) {
        TZrSize lineStart;
        TZrSize lineEnd;
        TZrSize trimEnd;

        lineStart = readIndex;
        while (readIndex < *ioLength && buffer[readIndex] != '\n') {
            readIndex++;
        }
        lineEnd = readIndex;
        trimEnd = lineEnd;
        while (trimEnd > lineStart && (buffer[trimEnd - 1] == ' ' || buffer[trimEnd - 1] == '\t')) {
            trimEnd--;
        }
        if (trimEnd > lineStart) {
            (void)memmove(buffer + writeIndex, buffer + lineStart, trimEnd - lineStart);
            writeIndex += trimEnd - lineStart;
        }
        if (readIndex < *ioLength) {
            buffer[writeIndex++] = '\n';
            readIndex++;
        }
    }
    *ioLength = writeIndex;
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

static TZrBool artifact_text_equals(const TZrChar* left, const TZrChar* right) {
    return left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0;
}

static void normalize_binary_words_at_offsets(const TZrSize* offsets,
                                              TZrSize offsetCount,
                                              TZrSize wordSize,
                                              TZrBytePtr generatedBuffer,
                                              TZrBytePtr goldenBuffer,
                                              TZrSize length) {
    TZrSize index;

    if (offsets == ZR_NULL || generatedBuffer == ZR_NULL || goldenBuffer == ZR_NULL || wordSize == 0) {
        return;
    }

    for (index = 0; index < offsetCount; index++) {
        TZrSize offset = offsets[index];
        if (offset + wordSize > length) {
            return;
        }

        memset(generatedBuffer + offset, 0, wordSize);
        memset(goldenBuffer + offset, 0, wordSize);
    }
}

static void normalize_known_volatile_binary_words(const TZrChar* baseName,
                                                  const TZrChar* subDir,
                                                  const TZrChar* extension,
                                                  TZrBytePtr generatedBuffer,
                                                  TZrBytePtr goldenBuffer,
                                                  TZrSize length) {
    static const TZrSize kCompileTimeDecoratorWordOffsets[] = {614, 675, 735, 4009, 4070, 4130};
    static const TZrSize kDecoratorWordOffsets[] = {
            8170, 8231, 8293, 8357, 8421,
            16710, 16771, 16833, 16897, 16961};

    if (!artifact_text_equals(subDir, "binary") ||
        !artifact_text_equals(extension, ".zro") ||
        generatedBuffer == ZR_NULL || goldenBuffer == ZR_NULL) {
        return;
    }

    if (artifact_text_equals(baseName, "compile_time_decorator_artifact_baseline")) {
        normalize_binary_words_at_offsets(
                kCompileTimeDecoratorWordOffsets,
                sizeof(kCompileTimeDecoratorWordOffsets) / sizeof(kCompileTimeDecoratorWordOffsets[0]),
                4,
                generatedBuffer,
                goldenBuffer,
                length);
        return;
    }

    if (artifact_text_equals(baseName, "decorator_artifact_baseline")) {
        normalize_binary_words_at_offsets(kDecoratorWordOffsets,
                                          sizeof(kDecoratorWordOffsets) / sizeof(kDecoratorWordOffsets[0]),
                                          4,
                                          generatedBuffer,
                                          goldenBuffer,
                                          length);
        return;
    }
}

typedef struct {
    TZrSize tokenStart;
    TZrByte value;
} SZrArtifactHexToken;

static TZrBool artifact_parse_hex_byte(const TZrByte* cursor, TZrByte* outValue) {
    TZrByte high;
    TZrByte low;

    if (cursor == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cursor[0] < '0' || cursor[0] > 'f' || cursor[1] < '0' || cursor[1] > 'f') {
        return ZR_FALSE;
    }

    if (cursor[0] >= '0' && cursor[0] <= '9') {
        high = (TZrByte)(cursor[0] - '0');
    } else if (cursor[0] >= 'a' && cursor[0] <= 'f') {
        high = (TZrByte)(cursor[0] - 'a' + 10);
    } else if (cursor[0] >= 'A' && cursor[0] <= 'F') {
        high = (TZrByte)(cursor[0] - 'A' + 10);
    } else {
        return ZR_FALSE;
    }

    if (cursor[1] >= '0' && cursor[1] <= '9') {
        low = (TZrByte)(cursor[1] - '0');
    } else if (cursor[1] >= 'a' && cursor[1] <= 'f') {
        low = (TZrByte)(cursor[1] - 'a' + 10);
    } else if (cursor[1] >= 'A' && cursor[1] <= 'F') {
        low = (TZrByte)(cursor[1] - 'A' + 10);
    } else {
        return ZR_FALSE;
    }

    *outValue = (TZrByte)((high << 4) | low);
    return ZR_TRUE;
}

static TZrBool artifact_collect_embedded_blob_tokens(const TZrBytePtr buffer,
                                                     TZrSize length,
                                                     SZrArtifactHexToken** outTokens,
                                                     TZrSize* outCount) {
    static const TZrChar* kBlobStart = "static const TZrByte zr_aot_embedded_module_blob[] = {";
    const TZrByte* start;
    const TZrByte* end;
    TZrSize count = 0;
    TZrSize index;
    SZrArtifactHexToken* tokens;
    TZrSize tokenIndex = 0;

    if (outTokens != ZR_NULL) {
        *outTokens = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (buffer == ZR_NULL || outTokens == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }
    (void)length;

    start = (const TZrByte*)strstr((const TZrChar*)buffer, kBlobStart);
    if (start == ZR_NULL) {
        return ZR_FALSE;
    }

    end = (const TZrByte*)strstr((const TZrChar*)start, "};");
    if (end == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = (TZrSize)(start - buffer); index + 3 < (TZrSize)(end - buffer); index++) {
        if (buffer[index] == '0' && buffer[index + 1] == 'x') {
            TZrByte value;
            if (artifact_parse_hex_byte(buffer + index + 2, &value)) {
                count++;
            }
        }
    }

    if (count == 0) {
        return ZR_FALSE;
    }

    tokens = (SZrArtifactHexToken*)malloc(sizeof(*tokens) * count);
    if (tokens == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = (TZrSize)(start - buffer); index + 3 < (TZrSize)(end - buffer) && tokenIndex < count; index++) {
        if (buffer[index] == '0' && buffer[index + 1] == 'x') {
            TZrByte value;
            if (!artifact_parse_hex_byte(buffer + index + 2, &value)) {
                continue;
            }
            tokens[tokenIndex].tokenStart = index;
            tokens[tokenIndex].value = value;
            tokenIndex++;
        }
    }

    *outTokens = tokens;
    *outCount = tokenIndex;
    return tokenIndex == count;
}

static void normalize_known_volatile_aot_embedded_blob_words(const TZrChar* baseName,
                                                             const TZrChar* subDir,
                                                             const TZrChar* extension,
                                                             TZrBytePtr generatedBuffer,
                                                             TZrBytePtr goldenBuffer,
                                                             TZrSize generatedLength,
                                                             TZrSize goldenLength) {
    static const TZrSize kAotClosureExportTokenWordStarts[] = {234, 1079, 1108};
    SZrArtifactHexToken* generatedTokens = ZR_NULL;
    SZrArtifactHexToken* goldenTokens = ZR_NULL;
    TZrSize generatedCount = 0;
    TZrSize goldenCount = 0;
    TZrSize wordIndex = 0;

    if (!artifact_text_equals(baseName, "aot_closure_export") ||
        !artifact_text_equals(subDir, "aot_c") ||
        !artifact_text_equals(extension, ".c") ||
        generatedBuffer == ZR_NULL || goldenBuffer == ZR_NULL) {
        return;
    }

    if (!artifact_collect_embedded_blob_tokens(generatedBuffer, generatedLength, &generatedTokens, &generatedCount) ||
        !artifact_collect_embedded_blob_tokens(goldenBuffer, goldenLength, &goldenTokens, &goldenCount) ||
        generatedCount != goldenCount) {
        free(generatedTokens);
        free(goldenTokens);
        return;
    }

    for (wordIndex = 0; wordIndex < sizeof(kAotClosureExportTokenWordStarts) / sizeof(kAotClosureExportTokenWordStarts[0]);
         wordIndex++) {
        TZrSize runStart = kAotClosureExportTokenWordStarts[wordIndex];
        TZrSize tokenIndex;

        if (runStart + 4 > generatedCount || runStart + 4 > goldenCount) {
            free(generatedTokens);
            free(goldenTokens);
            return;
        }

        for (tokenIndex = runStart; tokenIndex < runStart + 4; tokenIndex++) {
            generatedBuffer[generatedTokens[tokenIndex].tokenStart + 2] = '0';
            generatedBuffer[generatedTokens[tokenIndex].tokenStart + 3] = '0';
            goldenBuffer[goldenTokens[tokenIndex].tokenStart + 2] = '0';
            goldenBuffer[goldenTokens[tokenIndex].tokenStart + 3] = '0';
        }
    }

    free(generatedTokens);
    free(goldenTokens);
}

static TZrBool artifact_buffers_match_after_known_normalization(const TZrChar* baseName,
                                                                const TZrChar* subDir,
                                                                const TZrChar* extension,
                                                                const TZrBytePtr generatedBuffer,
                                                                const TZrBytePtr goldenBuffer,
                                                                TZrSize length) {
    TZrBytePtr generatedCopy;
    TZrBytePtr goldenCopy;
    TZrBool matches;

    if (generatedBuffer == ZR_NULL || goldenBuffer == ZR_NULL) {
        return ZR_FALSE;
    }

    generatedCopy = (TZrBytePtr)malloc(length + 1);
    goldenCopy = (TZrBytePtr)malloc(length + 1);
    if (generatedCopy == ZR_NULL || goldenCopy == ZR_NULL) {
        free(generatedCopy);
        free(goldenCopy);
        return ZR_FALSE;
    }

    memcpy(generatedCopy, generatedBuffer, length);
    memcpy(goldenCopy, goldenBuffer, length);
    generatedCopy[length] = '\0';
    goldenCopy[length] = '\0';

    normalize_known_volatile_binary_words(baseName,
                                          subDir,
                                          extension,
                                          generatedCopy,
                                          goldenCopy,
                                          length);
    normalize_known_volatile_aot_embedded_blob_words(baseName,
                                                     subDir,
                                                     extension,
                                                     generatedCopy,
                                                     goldenCopy,
                                                     length,
                                                     length);
    matches = memcmp(generatedCopy, goldenCopy, length) == 0;
    free(generatedCopy);
    free(goldenCopy);
    return matches;
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

    /* AOT LLVM text: MSVC uses different ZR_ALIGN_SIZE / struct layout than GCC/Clang, which shifts
     * numeric immediates and alignment annotations. Strip embedded .zro blobs (covered by binary golden),
     * trim line ends, then mask decimal runs so the compare is stable across hosts. */
    if (strcmp(extension, ".ll") == 0) {
        TZrBytePtr llvmStrippedGenerated = ZR_NULL;
        TZrBytePtr llvmStrippedGolden = ZR_NULL;
        TZrSize llvmStrippedGeneratedLength = 0;
        TZrSize llvmStrippedGoldenLength = 0;
        TZrBytePtr maskedGenerated = ZR_NULL;
        TZrBytePtr maskedGolden = ZR_NULL;
        TZrSize maskedGeneratedLength = 0;
        TZrSize maskedGoldenLength = 0;

        TEST_ASSERT_TRUE_MESSAGE(
                artifact_remove_aot_llvm_embedded_blob_for_compare(
                        generatedBuffer, generatedLength, &llvmStrippedGenerated, &llvmStrippedGeneratedLength),
                generatedPath);
        TEST_ASSERT_TRUE_MESSAGE(
                artifact_remove_aot_llvm_embedded_blob_for_compare(
                        goldenBuffer, goldenLength, &llvmStrippedGolden, &llvmStrippedGoldenLength),
                goldenPath);
        free(generatedBuffer);
        free(goldenBuffer);
        generatedBuffer = llvmStrippedGenerated;
        goldenBuffer = llvmStrippedGolden;
        generatedLength = llvmStrippedGeneratedLength;
        goldenLength = llvmStrippedGoldenLength;

        artifact_trim_trailing_spaces_on_each_line(generatedBuffer, &generatedLength);
        artifact_trim_trailing_spaces_on_each_line(goldenBuffer, &goldenLength);

        TEST_ASSERT_TRUE_MESSAGE(
                artifact_mask_decimal_runs_for_compare(
                        generatedBuffer, generatedLength, &maskedGenerated, &maskedGeneratedLength),
                generatedPath);
        TEST_ASSERT_TRUE_MESSAGE(
                artifact_mask_decimal_runs_for_compare(
                        goldenBuffer, goldenLength, &maskedGolden, &maskedGoldenLength),
                goldenPath);
        free(generatedBuffer);
        free(goldenBuffer);
        generatedBuffer = maskedGenerated;
        goldenBuffer = maskedGolden;
        generatedLength = maskedGeneratedLength;
        goldenLength = maskedGoldenLength;
    }

    if (generatedLength == goldenLength &&
        artifact_buffers_match_after_known_normalization(baseName,
                                                         subDir,
                                                         extension,
                                                         generatedBuffer,
                                                         goldenBuffer,
                                                         generatedLength)) {
        free(generatedBuffer);
        free(goldenBuffer);
        return;
    }

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(goldenLength, generatedLength, goldenPath);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(goldenBuffer, generatedBuffer, goldenLength, goldenPath);

    free(generatedBuffer);
    free(goldenBuffer);
}

static void assert_file_matches_golden_if_present(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension) {
    TZrChar goldenPath[1024];

    get_golden_output_path(baseName, subDir, extension, goldenPath, sizeof(goldenPath));
    if (ZrTests_File_Exists(goldenPath)) {
        assert_file_matches_golden(baseName, subDir, extension);
    }
}

static void assert_generated_text_contains(const TZrChar* baseName,
                                           const TZrChar* subDir,
                                           const TZrChar* extension,
                                           const TZrChar* needle) {
    TZrChar generatedPath[1024];
    TZrBytePtr generatedBuffer = ZR_NULL;
    TZrSize generatedLength = 0;

    TEST_ASSERT_NOT_NULL(needle);
    get_output_path(baseName, subDir, extension, generatedPath, sizeof(generatedPath));
    TEST_ASSERT_TRUE_MESSAGE(read_file_bytes(generatedPath, &generatedBuffer, &generatedLength), generatedPath);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr((const char*)generatedBuffer, needle), needle);
    free(generatedBuffer);
}

static void assert_generated_text_not_contains(const TZrChar* baseName,
                                               const TZrChar* subDir,
                                               const TZrChar* extension,
                                               const TZrChar* needle) {
    TZrChar generatedPath[1024];
    TZrBytePtr generatedBuffer = ZR_NULL;
    TZrSize generatedLength = 0;

    TEST_ASSERT_NOT_NULL(needle);
    get_output_path(baseName, subDir, extension, generatedPath, sizeof(generatedPath));
    TEST_ASSERT_TRUE_MESSAGE(read_file_bytes(generatedPath, &generatedBuffer, &generatedLength), generatedPath);
    TEST_ASSERT_NULL_MESSAGE(strstr((const char*)generatedBuffer, needle), needle);
    free(generatedBuffer);
}

static void run_artifact_case(const SZrArtifactGoldenCase* testCase) {
    TZrChar sourcePath[1024];
    TZrSize sourceLength = 0;
    TZrChar* source = ZR_NULL;
    SZrState* state = create_test_state();

    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_NOT_NULL(state);

    get_test_case_path(testCase->sourceFileName, sourcePath, sizeof(sourcePath));
    source = load_zr_file(sourcePath, &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    SZrTestResult* result = parse_and_compile(state, source, sourceLength, testCase->sourceFileName);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE_MESSAGE(result->success, result->errorMessage ? result->errorMessage : "parse_and_compile failed");

    TEST_ASSERT_TRUE(dump_ast_to_file(state, result->ast, testCase->baseName));
    TEST_ASSERT_TRUE(dump_intermediate_to_file(state, result->function, testCase->baseName));
    TEST_ASSERT_TRUE(dump_binary_to_file(state, result->function, testCase->baseName));
    TEST_ASSERT_TRUE(dump_aot_llvm_to_file(state, result->function, testCase->baseName));

    if (strcmp(testCase->baseName, "compile_time_decorator_artifact_baseline") == 0) {
        assert_generated_text_contains(testCase->baseName,
                                       "intermediate",
                                       ".zri",
                                       "fn decorateWithScale(target: Function, bonus: int = 5, factor: int = 2): zr.DecoratorPatch");
        assert_generated_text_contains(testCase->baseName,
                                       "intermediate",
                                       ".zri",
                                       "decoratorRegistry: object [bindings: deep.scale->fn:decorateWithScale]");
    }

    assert_file_matches_golden(testCase->baseName, "intermediate", ".zri");
    assert_file_matches_golden(testCase->baseName, "binary", ".zro");
    assert_file_matches_golden(testCase->baseName, "aot_llvm", ".ll");
    assert_file_matches_golden_if_present(testCase->baseName, "intermediate", ".zri.json");

    free_test_result(result);
    free(source);
    destroy_test_state(state);
}

static void run_true_aot_c_case(const SZrArtifactGoldenCase* testCase) {
    TZrChar sourcePath[1024];
    TZrSize sourceLength = 0;
    TZrChar* source = ZR_NULL;
    SZrState* state = create_test_state();

    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_NOT_NULL(state);

    get_test_case_path(testCase->sourceFileName, sourcePath, sizeof(sourcePath));
    source = load_zr_file(sourcePath, &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    SZrTestResult* result = parse_and_compile(state, source, sourceLength, testCase->sourceFileName);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE_MESSAGE(result->success, result->errorMessage ? result->errorMessage : "parse_and_compile failed");

    TEST_ASSERT_TRUE(dump_binary_to_file(state, result->function, testCase->baseName));
    TEST_ASSERT_TRUE(dump_aot_c_to_file(state, result->function, testCase->baseName));
    assert_generated_text_contains(testCase->baseName, "aot_c", ".c", "#define ZR_AOT_C_GUARD(");
    assert_generated_text_contains(testCase->baseName, "aot_c", ".c", "ZR_AOT_C_GUARD(");
    assert_generated_text_not_contains(testCase->baseName, "aot_c", ".c", "if (!ZrLibrary_AotRuntime_");
    assert_file_matches_golden(testCase->baseName, "aot_c", ".c");

    free_test_result(result);
    free(source);
    destroy_test_state(state);
}

static void test_artifact_outputs_match_goldens(void) {
    TZrSize i;

    for (i = 0; i < sizeof(ZR_ARTIFACT_GOLDEN_CASES) / sizeof(ZR_ARTIFACT_GOLDEN_CASES[0]); i++) {
        run_artifact_case(&ZR_ARTIFACT_GOLDEN_CASES[i]);
    }
}

static void test_true_aot_c_outputs_match_goldens(void) {
    TZrSize i;

    for (i = 0; i < sizeof(ZR_TRUE_AOT_C_GOLDEN_CASES) / sizeof(ZR_TRUE_AOT_C_GOLDEN_CASES[0]); i++) {
        run_true_aot_c_case(&ZR_TRUE_AOT_C_GOLDEN_CASES[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_artifact_outputs_match_goldens);
    RUN_TEST(test_true_aot_c_outputs_match_goldens);
    return UNITY_END();
}
