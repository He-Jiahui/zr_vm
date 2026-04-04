#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zr_vm_language_server.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_library/file.h"
#include "path_support.h"

#ifndef ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH
#define ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH ""
#endif

#ifndef ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH
#define ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH ""
#endif

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while (0)

#define TEST_PASS(timer, summary) do { \
    timer.endTime = clock(); \
    printf("Pass - Cost Time:%.3fms - %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL(timer, summary, reason) do { \
    timer.endTime = clock(); \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary, \
           reason); \
    fflush(stdout); \
} while (0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while (0)

/* Added ahead of the implementation for TDD. */
extern TZrBool ZrLanguageServer_Lsp_GetSemanticTokens(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrArray *result);
extern TZrSize ZrLanguageServer_Lsp_SemanticTokenTypeCount(void);
extern const TZrChar *ZrLanguageServer_Lsp_SemanticTokenTypeName(TZrSize index);
extern TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                             SZrLspContext *context,
                                                                             SZrString *uri);
extern TZrBool ZrLanguageServer_Lsp_ProjectContainsUri(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri);

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL &&
            (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 &&
            originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

static const TZrChar *test_string_ptr(SZrString *value) {
    if (value == ZR_NULL) {
        return "<null>";
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrBool build_fixture_native_path(const TZrChar *relativePath,
                                         TZrChar *buffer,
                                         TZrSize bufferSize) {
    TZrInt32 written;

    if (relativePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    written = snprintf(buffer,
                       bufferSize,
                       "%s%c%s",
                       ZR_VM_SOURCE_ROOT,
                       ZR_SEPARATOR,
                       relativePath);
    return written > 0 && (TZrSize)written < bufferSize;
}

static TZrChar *read_fixture_text_file(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    TZrChar *buffer;
    long size;
    size_t readSize;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrChar *)malloc((size_t)size + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    readSize = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (readSize != (size_t)size) {
        free(buffer);
        return ZR_NULL;
    }

    buffer[readSize] = '\0';
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)readSize;
    }
    return buffer;
}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *content, TZrSize length) {
    FILE *file;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    written = fwrite(content, 1, (size_t)length, file);
    fclose(file);
    return written == (size_t)length;
}

static TZrBool write_binary_file(const TZrChar *path, const TZrByte *content, TZrSize length) {
    FILE *file;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    written = fwrite(content, 1, (size_t)length, file);
    fclose(file);
    return written == (size_t)length;
}

static TZrBool copy_fixture_file(const TZrChar *sourcePath, const TZrChar *targetPath) {
    TZrSize contentLength = 0;
    TZrChar *content = read_fixture_text_file(sourcePath, &contentLength);
    TZrBool success;

    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    success = write_text_file(targetPath, content, contentLength);
    free(content);
    return success;
}

static TZrBool copy_fixture_binary_file(const TZrChar *sourcePath, const TZrChar *targetPath) {
    TZrBytePtr bytes = ZR_NULL;
    TZrSize length = 0;
    TZrBool success;

    if (!ZrTests_ReadFileBytes(sourcePath, &bytes, &length) || bytes == ZR_NULL) {
        return ZR_FALSE;
    }

    success = write_binary_file(targetPath, bytes, length);
    free(bytes);
    return success;
}

static TZrBool replace_text_in_file(const TZrChar *path,
                                    const TZrChar *needle,
                                    const TZrChar *replacement) {
    TZrSize contentLength = 0;
    TZrChar *content;
    TZrChar *match;
    TZrSize prefixLength;
    TZrSize needleLength;
    TZrSize replacementLength;
    TZrSize suffixLength;
    TZrSize newLength;
    TZrChar *updated;
    TZrBool success;

    if (path == ZR_NULL || needle == ZR_NULL || replacement == ZR_NULL) {
        return ZR_FALSE;
    }

    content = read_fixture_text_file(path, &contentLength);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    if (match == ZR_NULL) {
        free(content);
        return ZR_FALSE;
    }

    prefixLength = (TZrSize)(match - content);
    needleLength = strlen(needle);
    replacementLength = strlen(replacement);
    suffixLength = contentLength - prefixLength - needleLength;
    newLength = prefixLength + replacementLength + suffixLength;
    updated = (TZrChar *)malloc(newLength + 1);
    if (updated == ZR_NULL) {
        free(content);
        return ZR_FALSE;
    }

    memcpy(updated, content, prefixLength);
    memcpy(updated + prefixLength, replacement, replacementLength);
    memcpy(updated + prefixLength + replacementLength, match + needleLength, suffixLength);
    updated[newLength] = '\0';

    success = write_text_file(path, updated, newLength);
    free(updated);
    free(content);
    return success;
}

static SZrString *create_file_uri_from_native_path(SZrState *state, const TZrChar *path) {
    TZrChar buffer[ZR_LIBRARY_MAX_PATH_LENGTH * 2];
    TZrSize pathLength;
    TZrSize writeIndex = 0;

    if (state == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    pathLength = strlen(path);
    if (pathLength + 16 >= sizeof(buffer)) {
        return ZR_NULL;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    memcpy(buffer, "file:///", 8);
    writeIndex = 8;
#else
    memcpy(buffer, "file://", 7);
    writeIndex = 7;
#endif

    for (TZrSize index = 0; index < pathLength && writeIndex + 1 < sizeof(buffer); index++) {
        buffer[writeIndex++] = path[index] == '\\' ? '/' : path[index];
    }

    buffer[writeIndex] = '\0';
    return ZrCore_String_Create(state, buffer, writeIndex);
}

typedef struct SZrGeneratedBinaryMetadataFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar metadataPath[ZR_TESTS_PATH_MAX];
} SZrGeneratedBinaryMetadataFixture;

typedef struct SZrGeneratedFfiWrapperFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar wrapperPath[ZR_TESTS_PATH_MAX];
} SZrGeneratedFfiWrapperFixture;

typedef struct SZrGeneratedDescriptorPluginFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar pluginPath[ZR_TESTS_PATH_MAX];
} SZrGeneratedDescriptorPluginFixture;

static TZrBool prepare_generated_binary_metadata_fixture(const TZrChar *artifactName,
                                                         SZrGeneratedBinaryMetadataFixture *fixture) {
    TZrChar generatedProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar fixtureProjectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureMainPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureStageAPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureStageBPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fixtureBinaryMetadataPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar binaryRootPath[ZR_TESTS_PATH_MAX];
    TZrChar targetStageAPath[ZR_TESTS_PATH_MAX];
    TZrChar targetStageBPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (artifactName == ZR_NULL || fixture == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(fixture, 0, sizeof(*fixture));
    if (!ZrTests_Path_GetGeneratedArtifact("language_server",
                                           artifactName,
                                           "aot_module_graph_pipeline",
                                           ".zrp",
                                           generatedProjectPath,
                                           sizeof(generatedProjectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/aot_module_graph_pipeline.zrp",
                                   fixtureProjectPath,
                                   sizeof(fixtureProjectPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/src/main.zr",
                                   fixtureMainPath,
                                   sizeof(fixtureMainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/src/graph_stage_a.zr",
                                   fixtureStageAPath,
                                   sizeof(fixtureStageAPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/src/graph_stage_b.zr",
                                   fixtureStageBPath,
                                   sizeof(fixtureStageBPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/bin/graph_binary_stage.zri",
                                   fixtureBinaryMetadataPath,
                                   sizeof(fixtureBinaryMetadataPath))) {
        return ZR_FALSE;
    }

    snprintf(rootPath, sizeof(rootPath), "%s", generatedProjectPath);
    lastSeparator = strrchr(rootPath, ZR_SEPARATOR);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    snprintf(fixture->projectPath, sizeof(fixture->projectPath), "%s", generatedProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "src", sourceRootPath);
    ZrLibrary_File_PathJoin(rootPath, "bin", binaryRootPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "main.zr", fixture->mainPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "graph_stage_a.zr", targetStageAPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "graph_stage_b.zr", targetStageBPath);
    ZrLibrary_File_PathJoin(binaryRootPath, "graph_binary_stage.zri", fixture->metadataPath);

    return copy_fixture_file(fixtureProjectPath, fixture->projectPath) &&
           copy_fixture_file(fixtureMainPath, fixture->mainPath) &&
           copy_fixture_file(fixtureStageAPath, targetStageAPath) &&
           copy_fixture_file(fixtureStageBPath, targetStageBPath) &&
           copy_fixture_file(fixtureBinaryMetadataPath, fixture->metadataPath);
}

static TZrBool prepare_generated_ffi_wrapper_fixture(const TZrChar *artifactName,
                                                     SZrGeneratedFfiWrapperFixture *fixture) {
    static const TZrChar *projectContent =
        "{\n"
        "  \"name\": \"ffi_wrapper_source_kind\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"main\"\n"
        "}\n";
    static const TZrChar *mainContent =
        "var nativeApi = %import(\"native_api\");\n"
        "return nativeApi;\n";
    static const TZrChar *wrapperContent =
        "%extern(\"fixture\") {\n"
        "    Add(lhs: i32, rhs: i32): i32;\n"
        "}\n";
    TZrChar generatedProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (artifactName == ZR_NULL || fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("language_server",
                                           artifactName,
                                           "ffi_wrapper_source_kind",
                                           ".zrp",
                                           generatedProjectPath,
                                           sizeof(generatedProjectPath))) {
        return ZR_FALSE;
    }

    memset(fixture, 0, sizeof(*fixture));
    snprintf(fixture->projectPath, sizeof(fixture->projectPath), "%s", generatedProjectPath);
    snprintf(rootPath, sizeof(rootPath), "%s", generatedProjectPath);
    lastSeparator = strrchr(rootPath, ZR_SEPARATOR);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRootPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "main.zr", fixture->mainPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "native_api.zr", fixture->wrapperPath);

    return write_text_file(fixture->projectPath, projectContent, strlen(projectContent)) &&
           write_text_file(fixture->mainPath, mainContent, strlen(mainContent)) &&
           write_text_file(fixture->wrapperPath, wrapperContent, strlen(wrapperContent));
}

static const TZrChar *plugin_fixture_path_extension(const TZrChar *path) {
    const TZrChar *cursor;
    const TZrChar *lastDot = ZR_NULL;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    for (cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            lastDot = ZR_NULL;
            continue;
        }
        if (*cursor == '.') {
            lastDot = cursor;
        }
    }

    return lastDot;
}

static TZrBool prepare_generated_descriptor_plugin_fixture(const TZrChar *artifactName,
                                                           const TZrChar *pluginSourcePath,
                                                           SZrGeneratedDescriptorPluginFixture *fixture) {
    static const TZrChar *projectContent =
        "{\n"
        "  \"name\": \"descriptor_plugin_source_kind\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"main\"\n"
        "}\n";
    static const TZrChar *mainContent =
        "var plugin = %import(\"zr.pluginprobe\");\n"
        "plugin.answer;\n"
        "var capture = plugin.answer;\n"
        "pub func usePlugin() {\n"
        "    return plugin.answer;\n"
        "}\n";
    TZrChar generatedProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar nativeRootPath[ZR_TESTS_PATH_MAX];
    TZrChar pluginFileName[ZR_TESTS_PATH_MAX];
    const TZrChar *pluginExtension;
    TZrChar *lastSeparator;

    if (artifactName == ZR_NULL || pluginSourcePath == ZR_NULL || fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("language_server",
                                           artifactName,
                                           "descriptor_plugin_source_kind",
                                           ".zrp",
                                           generatedProjectPath,
                                           sizeof(generatedProjectPath))) {
        return ZR_FALSE;
    }

    pluginExtension = plugin_fixture_path_extension(pluginSourcePath);
    if (pluginExtension == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(fixture, 0, sizeof(*fixture));
    snprintf(fixture->projectPath, sizeof(fixture->projectPath), "%s", generatedProjectPath);
    snprintf(rootPath, sizeof(rootPath), "%s", generatedProjectPath);
    lastSeparator = strrchr(rootPath, ZR_SEPARATOR);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRootPath);
    ZrLibrary_File_PathJoin(rootPath, "native", nativeRootPath);
    ZrLibrary_File_PathJoin(sourceRootPath, "main.zr", fixture->mainPath);
    snprintf(pluginFileName,
             sizeof(pluginFileName),
             "zrvm_native_zr_pluginprobe%s",
             pluginExtension);
    ZrLibrary_File_PathJoin(nativeRootPath, pluginFileName, fixture->pluginPath);

    return write_text_file(fixture->projectPath, projectContent, strlen(projectContent)) &&
           write_text_file(fixture->mainPath, mainContent, strlen(mainContent)) &&
           copy_fixture_binary_file(pluginSourcePath, fixture->pluginPath);
}

static TZrBool lsp_find_position_for_substring(const TZrChar *content,
                                               const TZrChar *needle,
                                               TZrSize occurrence,
                                               TZrInt32 extraCharacterOffset,
                                               SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }

    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
        cursor++;
    }

    outPosition->line = line;
    outPosition->character = character + extraCharacterOffset;
    return ZR_TRUE;
}

static TZrBool location_array_contains_uri_and_range(SZrArray *locations,
                                                     SZrString *uri,
                                                     TZrInt32 startLine,
                                                     TZrInt32 startCharacter,
                                                     TZrInt32 endLine,
                                                     TZrInt32 endCharacter) {
    if (locations == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrLspLocation *location;

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL) {
            continue;
        }

        location = *locationPtr;
        if (location->uri != ZR_NULL &&
            strcmp(test_string_ptr(location->uri), test_string_ptr(uri)) == 0 &&
            location->range.start.line == startLine &&
            location->range.start.character == startCharacter &&
            location->range.end.line == endLine &&
            location->range.end.character == endCharacter) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool highlight_array_contains_range(SZrArray *highlights,
                                              TZrInt32 startLine,
                                              TZrInt32 startCharacter,
                                              TZrInt32 endLine,
                                              TZrInt32 endCharacter) {
    if (highlights == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        SZrLspDocumentHighlight *highlight;

        if (highlightPtr == ZR_NULL || *highlightPtr == ZR_NULL) {
            continue;
        }

        highlight = *highlightPtr;
        if (highlight->range.start.line == startLine &&
            highlight->range.start.character == startCharacter &&
            highlight->range.end.line == endLine &&
            highlight->range.end.character == endCharacter) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool completion_array_contains_label(SZrArray *completions, const TZrChar *label) {
    if (completions == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < completions->length; index++) {
        SZrLspCompletionItem **itemPtr =
            (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL &&
            (*itemPtr)->label != ZR_NULL &&
            strcmp(test_string_ptr((*itemPtr)->label), label) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const TZrChar *completion_detail_for_label(SZrArray *completions, const TZrChar *label) {
    if (completions == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < completions->length; index++) {
        SZrLspCompletionItem **itemPtr =
            (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL &&
            (*itemPtr)->label != ZR_NULL &&
            (*itemPtr)->detail != ZR_NULL &&
            strcmp(test_string_ptr((*itemPtr)->label), label) == 0) {
            return test_string_ptr((*itemPtr)->detail);
        }
    }

    return ZR_NULL;
}

static TZrBool completion_detail_contains_fragment(SZrArray *completions,
                                                   const TZrChar *label,
                                                   const TZrChar *fragment) {
    const TZrChar *detail = completion_detail_for_label(completions, label);

    return detail != ZR_NULL && fragment != ZR_NULL && strstr(detail, fragment) != ZR_NULL;
}

static void describe_completion_labels(SZrArray *completions, TZrChar *buffer, size_t bufferSize) {
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (completions == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < completions->length && offset + 1 < bufferSize; index++) {
        SZrLspCompletionItem **itemPtr =
            (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        const TZrChar *label;
        int written;

        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        label = test_string_ptr((*itemPtr)->label);
        written = snprintf(buffer + offset,
                           bufferSize - offset,
                           "%s%s",
                           offset == 0 ? "" : ", ",
                           label != ZR_NULL ? label : "<null>");
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            buffer[bufferSize - 1] = '\0';
            return;
        }
        offset += (TZrSize)written;
    }
}

static TZrBool hover_contains_text(SZrLspHover *hover, const TZrChar *needle) {
    if (hover == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < hover->contents.length; index++) {
        SZrString **contentPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, index);
        if (contentPtr != ZR_NULL && *contentPtr != ZR_NULL &&
            strstr(test_string_ptr(*contentPtr), needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool symbol_array_contains_name(SZrArray *symbols, const TZrChar *needle) {
    if (symbols == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
            (*symbolPtr)->name != ZR_NULL &&
            strcmp(test_string_ptr((*symbolPtr)->name), needle) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrInt32 semantic_token_type_index(const TZrChar *typeName) {
    if (typeName == ZR_NULL) {
        return -1;
    }

    for (TZrSize index = 0; index < ZrLanguageServer_Lsp_SemanticTokenTypeCount(); index++) {
        const TZrChar *candidate = ZrLanguageServer_Lsp_SemanticTokenTypeName(index);
        if (candidate != ZR_NULL && strcmp(candidate, typeName) == 0) {
            return (TZrInt32)index;
        }
    }

    return -1;
}

static TZrBool semantic_tokens_contain(SZrArray *data,
                                       TZrInt32 line,
                                       TZrInt32 character,
                                       TZrInt32 length,
                                       const TZrChar *typeName) {
    TZrUInt32 currentLine = 0;
    TZrUInt32 currentCharacter = 0;
    TZrInt32 typeIndex = semantic_token_type_index(typeName);

    if (data == ZR_NULL || typeIndex < 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index + 4 < data->length; index += 5) {
        TZrUInt32 *deltaLinePtr = (TZrUInt32 *)ZrCore_Array_Get(data, index);
        TZrUInt32 *deltaStartPtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 1);
        TZrUInt32 *lengthPtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 2);
        TZrUInt32 *typePtr = (TZrUInt32 *)ZrCore_Array_Get(data, index + 3);

        if (deltaLinePtr == ZR_NULL || deltaStartPtr == ZR_NULL || lengthPtr == ZR_NULL || typePtr == ZR_NULL) {
            continue;
        }

        currentLine += *deltaLinePtr;
        if (*deltaLinePtr == 0) {
            currentCharacter += *deltaStartPtr;
        } else {
            currentCharacter = *deltaStartPtr;
        }

        if ((TZrInt32)currentLine == line &&
            (TZrInt32)currentCharacter == character &&
            (TZrInt32)(*lengthPtr) == length &&
            (TZrInt32)(*typePtr) == typeIndex) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_lsp_auto_discovers_project_from_source_file(SZrState *state);
static void test_lsp_uses_nearest_ancestor_project(SZrState *state);
static void test_lsp_ambiguous_project_directory_stays_standalone(SZrState *state);
static void test_lsp_native_imports_and_ownership_display(SZrState *state);
static void test_lsp_import_literal_navigation_and_hover(SZrState *state);
static void test_lsp_import_literal_hover_identifies_native_descriptor_plugin(SZrState *state);
static void test_lsp_import_literal_definition_targets_native_descriptor_plugin(SZrState *state);
static void test_lsp_binary_import_literal_definition_targets_metadata(SZrState *state);
static void test_lsp_descriptor_plugin_member_completion_definition_and_references(SZrState *state);
static void test_lsp_descriptor_plugin_project_local_definition_overrides_stale_registry(SZrState *state);
static void test_lsp_binary_import_references_surface_metadata_and_usages(SZrState *state);
static void test_lsp_binary_import_document_highlights_cover_all_local_usages(SZrState *state);
static void test_lsp_native_import_member_references_and_highlights(SZrState *state);
static void test_lsp_watched_binary_metadata_refresh_bootstraps_unopened_projects(SZrState *state);
static void test_lsp_watched_descriptor_plugin_refresh_bootstraps_unopened_projects(SZrState *state);
static void test_lsp_watched_binary_metadata_refresh_invalidates_module_cache_keys(SZrState *state);
static void test_lsp_watched_binary_metadata_refresh_reanalyzes_open_documents(SZrState *state);
static void test_lsp_watched_descriptor_plugin_refresh_reanalyzes_open_documents(SZrState *state);
static void test_lsp_semantic_tokens_cover_keywords_and_symbols(SZrState *state);

static void test_lsp_auto_discovers_project_from_source_file(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    TZrChar mainPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar greetPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *greetUri = ZR_NULL;
    SZrLspPosition memberUsage;
    SZrLspPosition aliasUsage;
    SZrArray definitions;
    SZrArray completions;
    SZrArray workspaceSymbols;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Auto Discovers Project From Source File");
    TEST_INFO("Project Discovery", "Opening only a .zr source file should bind the nearest ancestor .zrp");

    if (!build_fixture_native_path("tests/fixtures/projects/import_basic/src/main.zr", mainPath, sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_basic/src/greet.zr", greetPath, sizeof(greetPath))) {
        TEST_FAIL(timer, "LSP Auto Discovers Project From Source File", "Failed to build fixture paths");
        return;
    }

    mainContent = read_fixture_text_file(mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer, "LSP Auto Discovers Project From Source File", "Failed to prepare test state");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, mainPath);
    greetUri = create_file_uri_from_native_path(state, greetPath);
    if (mainUri == ZR_NULL || greetUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "greet()", 0, 0, &memberUsage) ||
        !lsp_find_position_for_substring(mainContent, "greetModule.greet", 0, 0, &aliasUsage)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Discovers Project From Source File", "Failed to open main source or compute positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, memberUsage, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, greetUri, 0, 8, 0, 13)) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Discovers Project From Source File", "Imported member definition should resolve without explicitly opening the project file");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, aliasUsage, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <greet>")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Discovers Project From Source File", "Import alias hover should display a module type");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state,
                                            context,
                                            mainUri,
                                            (SZrLspPosition){0, 23},
                                            &completions) ||
        !completion_array_contains_label(&completions, "greet")) {
        free(mainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Discovers Project From Source File", "Module alias completion should list exported members from the imported module");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &workspaceSymbols, sizeof(SZrLspSymbolInformation *), 8);
    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(state,
                                                  context,
                                                  ZrCore_String_Create(state, "greet", 5),
                                                  &workspaceSymbols) ||
        !symbol_array_contains_name(&workspaceSymbols, "greet")) {
        free(mainContent);
        ZrCore_Array_Free(state, &workspaceSymbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Discovers Project From Source File", "Workspace symbols should include imported exports after auto-discovery");
        return;
    }

    free(mainContent);
    ZrCore_Array_Free(state, &workspaceSymbols);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Auto Discovers Project From Source File");
}

static void test_lsp_uses_nearest_ancestor_project(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    TZrChar mainPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar helperPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *helperUri = ZR_NULL;
    SZrLspPosition memberUsage;
    SZrArray definitions;

    TEST_START("LSP Uses Nearest Ancestor Project");
    TEST_INFO("Nearest Ancestor", "Nested source files should bind to the closest ancestor .zrp");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_discovery_nested/nested/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/lsp_discovery_nested/nested/src/helper.zr",
                                   helperPath,
                                   sizeof(helperPath))) {
        TEST_FAIL(timer, "LSP Uses Nearest Ancestor Project", "Failed to build fixture paths");
        return;
    }

    mainContent = read_fixture_text_file(mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer, "LSP Uses Nearest Ancestor Project", "Failed to prepare test state");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, mainPath);
    helperUri = create_file_uri_from_native_path(state, helperPath);
    if (mainUri == ZR_NULL || helperUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "value;", 0, 0, &memberUsage)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Uses Nearest Ancestor Project", "Failed to open nested main source");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, memberUsage, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, helperUri, 0, 8, 0, 13)) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Uses Nearest Ancestor Project", "Definition should resolve against the nested project instead of an outer ancestor project");
        return;
    }

    free(mainContent);
    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Uses Nearest Ancestor Project");
}

static void test_lsp_ambiguous_project_directory_stays_standalone(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    TZrChar mainPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrLspPosition memberUsage;
    SZrArray definitions;

    TEST_START("LSP Ambiguous Project Directory Stays Standalone");
    TEST_INFO("Ambiguous Discovery", "Multiple .zrp files in the same directory should not auto-bind a project");

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_discovery_ambiguous/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath))) {
        TEST_FAIL(timer, "LSP Ambiguous Project Directory Stays Standalone", "Failed to build fixture path");
        return;
    }

    mainContent = read_fixture_text_file(mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer, "LSP Ambiguous Project Directory Stays Standalone", "Failed to prepare test state");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, mainPath);
    if (mainUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "value;", 0, 0, &memberUsage)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Ambiguous Project Directory Stays Standalone", "Failed to open ambiguous source");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, memberUsage, &definitions) &&
        definitions.length > 0) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Ambiguous Project Directory Stays Standalone", "Ambiguous project discovery should not pick a definition target");
        return;
    }

    free(mainContent);
    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Ambiguous Project Directory Stays Standalone");
}

static void test_lsp_native_imports_and_ownership_display(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *nativeContent =
        "var system = %import(\"zr.system\");\n"
        "system.console;\n";
    SZrString *nativeUri;
    SZrLspPosition aliasPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;
    TZrChar mathPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize mathLength = 0;
    TZrChar *mathContent;
    SZrString *mathUri = ZR_NULL;
    SZrLspPosition functionPosition;
    SZrLspHover *ownershipHover = ZR_NULL;

    TEST_START("LSP Native Imports And Ownership Display");
    TEST_INFO("Native Imports / Ownership", "Native module members should expose typed hover/completion and ownership-aware type strings");

    context = ZrLanguageServer_LspContext_New(state);
    nativeUri = ZrCore_String_Create(state,
                                     "file:///native_imports.zr",
                                     strlen("file:///native_imports.zr"));
    if (context == ZR_NULL || nativeUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state,
                                            context,
                                            nativeUri,
                                            nativeContent,
                                            strlen(nativeContent),
                                            1) ||
        !lsp_find_position_for_substring(nativeContent, "system.console", 0, 0, &aliasPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Native Imports And Ownership Display", "Failed to prepare native import source");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, nativeUri, aliasPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <zr.system>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Native Imports And Ownership Display", "Native import alias hover should display the module type");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state,
                                            context,
                                            nativeUri,
                                            (SZrLspPosition){1, 7},
                                            &completions) ||
        !completion_array_contains_label(&completions, "console")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Native Imports And Ownership Display", "Native module completion should list linked module members");
        return;
    }
    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);

    if (!build_fixture_native_path("tests/fixtures/projects/lsp_ownership/src/main.zr", mathPath, sizeof(mathPath))) {
        TEST_FAIL(timer, "LSP Native Imports And Ownership Display", "Failed to build ownership fixture path");
        return;
    }

    mathContent = read_fixture_text_file(mathPath, &mathLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mathContent == ZR_NULL || context == ZR_NULL) {
        free(mathContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Native Imports And Ownership Display", "Failed to prepare ownership fixture");
        return;
    }

    mathUri = create_file_uri_from_native_path(state, mathPath);
    if (mathUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mathUri, mathContent, mathLength, 1) ||
        !lsp_find_position_for_substring(mathContent, "takeFromPoolTest", 0, 0, &functionPosition)) {
        free(mathContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Native Imports And Ownership Display", "Failed to open ownership fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mathUri, functionPosition, &ownershipHover) ||
        ownershipHover == ZR_NULL ||
        !hover_contains_text(ownershipHover, "%unique PointSet")) {
        free(mathContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Native Imports And Ownership Display", "Hover should preserve ownership qualifiers in type display");
        return;
    }

    free(mathContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Imports And Ownership Display");
}

static void test_lsp_import_literal_navigation_and_hover(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    TZrChar mainPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar greetPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *greetUri = ZR_NULL;
    SZrLspPosition sourceImportPosition;
    SZrArray definitions;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *nativeContent = "var system = %import(\"zr.system\");\n";
    SZrString *nativeUri = ZR_NULL;
    SZrLspPosition nativeImportPosition;

    TEST_START("LSP Import Literal Navigation And Hover");
    TEST_INFO("Import target navigation",
              "Hover and definition on %import string literals should resolve the target module and source kind");

    if (!build_fixture_native_path("tests/fixtures/projects/import_basic/src/main.zr", mainPath, sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/import_basic/src/greet.zr", greetPath, sizeof(greetPath))) {
        TEST_FAIL(timer, "LSP Import Literal Navigation And Hover", "Failed to build fixture paths");
        return;
    }

    mainContent = read_fixture_text_file(mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer, "LSP Import Literal Navigation And Hover", "Failed to prepare source import fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, mainPath);
    greetUri = create_file_uri_from_native_path(state, greetPath);
    if (mainUri == ZR_NULL || greetUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "\"greet\"", 0, 1, &sourceImportPosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Import Literal Navigation And Hover", "Failed to open source import fixture or compute import literal position");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, sourceImportPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, greetUri, 0, 0, 0, 0)) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Navigation And Hover",
                  "Definition on %import(\"greet\") should jump to the imported module entry");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, sourceImportPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <greet>") ||
        !hover_contains_text(hover, "Source: project source")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Navigation And Hover",
                  "Hover on %import(\"greet\") should describe the source module target");
        return;
    }

    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);

    context = ZrLanguageServer_LspContext_New(state);
    nativeUri = ZrCore_String_Create(state,
                                     "file:///native_import_literal_hover.zr",
                                     strlen("file:///native_import_literal_hover.zr"));
    if (context == ZR_NULL || nativeUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, nativeUri, nativeContent, strlen(nativeContent), 1) ||
        !lsp_find_position_for_substring(nativeContent, "\"zr.system\"", 0, 1, &nativeImportPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Import Literal Navigation And Hover", "Failed to prepare native import literal fixture");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, nativeUri, nativeImportPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <zr.system>") ||
        !hover_contains_text(hover, "Source: native builtin")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Navigation And Hover",
                  "Hover on %import(\"zr.system\") should describe the native builtin source");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Import Literal Navigation And Hover");
}

static void test_lsp_import_literal_hover_identifies_ffi_source_wrapper(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedFfiWrapperFixture fixture;
    SZrLspContext *context;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *wrapperUri = ZR_NULL;
    SZrLspPosition importPosition;
    SZrArray definitions;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Import Literal Hover Identifies FFI Source Wrapper");
    TEST_INFO("FFI wrapper source kind",
              "Import target hover on a %extern-backed source module should surface ffi source wrapper instead of plain project source");

    if (!prepare_generated_ffi_wrapper_fixture("project_features_ffi_wrapper_source_kind", &fixture)) {
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies FFI Source Wrapper",
                  "Failed to prepare generated ffi wrapper fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies FFI Source Wrapper",
                  "Failed to load generated ffi wrapper fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    wrapperUri = create_file_uri_from_native_path(state, fixture.wrapperPath);
    if (mainUri == ZR_NULL || wrapperUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "\"native_api\"", 0, 1, &importPosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies FFI Source Wrapper",
                  "Failed to open generated ffi wrapper main module or compute import literal position");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, importPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, wrapperUri, 0, 0, 0, 0)) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies FFI Source Wrapper",
                  "Definition on an imported ffi wrapper module should still navigate to the wrapper source entry");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, importPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <native_api>") ||
        !hover_contains_text(hover, "Source: ffi source wrapper")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies FFI Source Wrapper",
                  "Hover on %import(\"native_api\") should describe the imported wrapper as ffi source wrapper");
        return;
    }

    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Import Literal Hover Identifies FFI Source Wrapper");
}

static void test_lsp_import_literal_hover_identifies_native_descriptor_plugin(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedDescriptorPluginFixture fixture;
    SZrLspContext *context;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrLspPosition importPosition;
    SZrLspPosition memberHoverPosition;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Import Literal Hover Identifies Native Descriptor Plugin");
    TEST_INFO("Descriptor plugin source kind",
              "Import target hover and imported member hover should resolve project-local native descriptor plugins as first-class metadata sources");

    if (!prepare_generated_descriptor_plugin_fixture("project_features_descriptor_plugin_source_kind",
                                                     ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                                                     &fixture)) {
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies Native Descriptor Plugin",
                  "Failed to prepare generated descriptor plugin fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies Native Descriptor Plugin",
                  "Failed to load generated descriptor plugin fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    if (mainUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "\"zr.pluginprobe\"", 0, 1, &importPosition) ||
        !lsp_find_position_for_substring(mainContent, "plugin.answer", 0, 8, &memberHoverPosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies Native Descriptor Plugin",
                  "Failed to open generated descriptor plugin module or compute hover positions");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, importPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "module <zr.pluginprobe>") ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies Native Descriptor Plugin",
                  "Hover on %import(\"zr.pluginprobe\") should describe the imported module as native descriptor plugin");
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, memberHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "answer") ||
        !hover_contains_text(hover, "function") ||
        !hover_contains_text(hover, "int") ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Hover Identifies Native Descriptor Plugin",
                  "Hover on an imported descriptor-plugin function should expose its native signature and source kind");
        return;
    }

    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Import Literal Hover Identifies Native Descriptor Plugin");
}

static void test_lsp_import_literal_definition_targets_native_descriptor_plugin(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedDescriptorPluginFixture fixture;
    SZrLspContext *context;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *pluginUri = ZR_NULL;
    SZrLspPosition importPosition;
    SZrArray definitions;

    ZrCore_Array_Construct(&definitions);

    TEST_START("LSP Import Literal Definition Targets Native Descriptor Plugin");
    TEST_INFO("Descriptor plugin import target definition",
              "Definition on %import(\"zr.pluginprobe\") should navigate to the backing native descriptor plugin file when no source or binary metadata module exists");

    if (!prepare_generated_descriptor_plugin_fixture("project_features_descriptor_plugin_import_definition",
                                                     ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                                                     &fixture)) {
        TEST_FAIL(timer,
                  "LSP Import Literal Definition Targets Native Descriptor Plugin",
                  "Failed to prepare generated descriptor plugin fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Definition Targets Native Descriptor Plugin",
                  "Failed to load generated descriptor plugin fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    pluginUri = create_file_uri_from_native_path(state, fixture.pluginPath);
    if (mainUri == ZR_NULL || pluginUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "\"zr.pluginprobe\"", 0, 1, &importPosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Definition Targets Native Descriptor Plugin",
                  "Failed to open generated descriptor plugin main module or compute import literal position");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, importPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, pluginUri, 0, 0, 0, 0)) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Import Literal Definition Targets Native Descriptor Plugin",
                  "Definition on a descriptor-plugin import literal should resolve to the native plugin file entry");
        return;
    }

    free(mainContent);
    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Import Literal Definition Targets Native Descriptor Plugin");
}

static void test_lsp_binary_import_literal_definition_targets_metadata(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedBinaryMetadataFixture fixture;
    SZrLspContext *context;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    TZrSize metadataLength = 0;
    TZrChar *metadataContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *metadataUri = ZR_NULL;
    SZrLspPosition importPosition;
    SZrArray definitions;

    ZrCore_Array_Construct(&definitions);

    TEST_START("LSP Binary Import Literal Definition Targets Metadata");
    TEST_INFO("Binary import target definition",
              "Definition on %import(\"graph_binary_stage\") should navigate to the backing binary metadata module when no source module exists");

    if (!prepare_generated_binary_metadata_fixture("project_features_binary_import_literal_definition", &fixture)) {
        TEST_FAIL(timer,
                  "LSP Binary Import Literal Definition Targets Metadata",
                  "Failed to prepare generated binary metadata fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    metadataContent = read_fixture_text_file(fixture.metadataPath, &metadataLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || metadataContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        free(metadataContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Literal Definition Targets Metadata",
                  "Failed to load main/metadata fixture content or allocate LSP context");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    metadataUri = create_file_uri_from_native_path(state, fixture.metadataPath);
    if (mainUri == ZR_NULL || metadataUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "\"graph_binary_stage\"", 0, 1, &importPosition)) {
        free(mainContent);
        free(metadataContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Literal Definition Targets Metadata",
                  "Failed to open fixture or compute import/metadata definition positions");
        return;
    }

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, importPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions,
                                               metadataUri,
                                               0,
                                               0,
                                               0,
                                               0)) {
        free(mainContent);
        free(metadataContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Literal Definition Targets Metadata",
                  "Definition on binary import literals should resolve to the binary metadata file entry");
        return;
    }

    free(mainContent);
    free(metadataContent);
    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Binary Import Literal Definition Targets Metadata");
}

static void test_lsp_binary_import_metadata_surfaces_hover_and_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    TZrChar mainPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar metadataPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    TZrSize metadataLength = 0;
    TZrChar *metadataContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *metadataUri = ZR_NULL;
    SZrLspPosition completionPosition;
    SZrLspPosition hoverPosition;
    SZrLspPosition definitionPosition;
    SZrLspPosition metadataDeclarationPosition;
    SZrArray completions;
    SZrArray definitions;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Binary Import Metadata Surfaces Hover And Completion");
    TEST_INFO("Binary import metadata",
              "Binary-only imported modules should surface member completion and hover through the same metadata path as source/native imports");

    if (!build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/src/main.zr",
                                   mainPath,
                                   sizeof(mainPath)) ||
        !build_fixture_native_path("tests/fixtures/projects/aot_module_graph_pipeline/bin/graph_binary_stage.zri",
                                   metadataPath,
                                   sizeof(metadataPath))) {
        TEST_FAIL(timer,
                  "LSP Binary Import Metadata Surfaces Hover And Completion",
                  "Failed to build fixture path");
        return;
    }

    mainContent = read_fixture_text_file(mainPath, &mainLength);
    metadataContent = read_fixture_text_file(metadataPath, &metadataLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || metadataContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        free(metadataContent);
        TEST_FAIL(timer,
                  "LSP Binary Import Metadata Surfaces Hover And Completion",
                  "Failed to prepare binary import fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, mainPath);
    metadataUri = create_file_uri_from_native_path(state, metadataPath);
    if (mainUri == ZR_NULL || metadataUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "binaryStage.binarySeed", 0, 12, &completionPosition) ||
        !lsp_find_position_for_substring(mainContent, "binarySeed", 0, 0, &hoverPosition) ||
        !lsp_find_position_for_substring(mainContent, "binarySeed", 0, 0, &definitionPosition) ||
        !lsp_find_position_for_substring(metadataContent, "fn binarySeed(): int", 0, 3, &metadataDeclarationPosition)) {
        free(mainContent);
        free(metadataContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Metadata Surfaces Hover And Completion",
                  "Failed to open binary import fixture or compute member positions");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "binarySeed")) {
        TZrChar labels[256];
        describe_completion_labels(&completions, labels, sizeof(labels));
        free(mainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Metadata Surfaces Hover And Completion",
                  labels);
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, definitionPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions,
                                               metadataUri,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character + 10)) {
        free(mainContent);
        free(metadataContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Metadata Surfaces Hover And Completion",
                  "Definition on binary-only imported members should navigate to the exported symbol in .zri metadata");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, metadataUri, metadataDeclarationPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions,
                                               metadataUri,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character + 10)) {
        free(mainContent);
        free(metadataContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Metadata Surfaces Hover And Completion",
                  "Goto definition on a .zri exported declaration should stay on the same metadata declaration target");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, hoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "binarySeed") ||
        !hover_contains_text(hover, "function") ||
        !hover_contains_text(hover, "int") ||
        !hover_contains_text(hover, "Source: binary metadata")) {
        free(mainContent);
        free(metadataContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Metadata Surfaces Hover And Completion",
                  "Hover on binary-only import members should expose function type and binary metadata source");
        return;
    }

    free(mainContent);
    free(metadataContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Binary Import Metadata Surfaces Hover And Completion");
}

static void test_lsp_binary_import_references_surface_metadata_and_usages(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedBinaryMetadataFixture fixture;
    SZrLspContext *context;
    const TZrChar *customMainContent =
        "var binaryStage = %import(\"graph_binary_stage\");\n"
        "var left = <int> binaryStage.binarySeed();\n"
        "var right = <int> binaryStage.binarySeed();\n"
        "return left + right;\n";
    TZrSize mainLength = strlen(customMainContent);
    TZrSize metadataLength = 0;
    TZrChar *metadataContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *metadataUri = ZR_NULL;
    SZrLspPosition firstUsagePosition;
    SZrLspPosition secondUsagePosition;
    SZrLspPosition metadataDeclarationPosition;
    SZrArray references;

    TEST_START("LSP Binary Import References Surface Metadata And Usages");
    TEST_INFO("Binary import references",
              "Find references on binary-only imported members should include project usages and the .zri export declaration");

    if (!prepare_generated_binary_metadata_fixture("project_features_binary_import_references", &fixture) ||
        !write_text_file(fixture.mainPath, customMainContent, mainLength)) {
        TEST_FAIL(timer,
                  "LSP Binary Import References Surface Metadata And Usages",
                  "Failed to prepare generated binary reference fixture");
        return;
    }

    metadataContent = read_fixture_text_file(fixture.metadataPath, &metadataLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (metadataContent == ZR_NULL || context == ZR_NULL) {
        free(metadataContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import References Surface Metadata And Usages",
                  "Failed to load binary metadata fixture content or allocate LSP context");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    metadataUri = create_file_uri_from_native_path(state, fixture.metadataPath);
    if (mainUri == ZR_NULL || metadataUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, customMainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(customMainContent, "binarySeed", 0, 0, &firstUsagePosition) ||
        !lsp_find_position_for_substring(customMainContent, "binarySeed", 1, 0, &secondUsagePosition) ||
        !lsp_find_position_for_substring(metadataContent, "fn binarySeed(): int", 0, 3, &metadataDeclarationPosition)) {
        free(metadataContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import References Surface Metadata And Usages",
                  "Failed to open binary reference fixture or compute usage/declaration positions");
        return;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, mainUri, firstUsagePosition, ZR_TRUE, &references) ||
        references.length < 3 ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character + 10) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character + 10) ||
        !location_array_contains_uri_and_range(&references,
                                               metadataUri,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character + 10)) {
        free(metadataContent);
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import References Surface Metadata And Usages",
                  "Binary imported member references should include both local usages and the .zri declaration when includeDeclaration=true");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state,
                                             context,
                                             metadataUri,
                                             metadataDeclarationPosition,
                                             ZR_TRUE,
                                             &references) ||
        references.length < 3 ||
        !location_array_contains_uri_and_range(&references,
                                               metadataUri,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character,
                                               metadataDeclarationPosition.line,
                                               metadataDeclarationPosition.character + 10) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character + 10) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character + 10)) {
        free(metadataContent);
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import References Surface Metadata And Usages",
                  "References from the .zri declaration should resolve back to the same project usages as imported member references");
        return;
    }

    free(metadataContent);
    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Binary Import References Surface Metadata And Usages");
}

static void test_lsp_descriptor_plugin_member_completion_definition_and_references(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedDescriptorPluginFixture fixture;
    SZrLspContext *context;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *pluginUri = ZR_NULL;
    SZrLspPosition completionPosition;
    SZrLspPosition firstUsagePosition;
    SZrLspPosition secondUsagePosition;
    SZrLspPosition thirdUsagePosition;
    SZrArray completions;
    SZrArray definitions;
    SZrArray references;

    ZrCore_Array_Construct(&completions);
    ZrCore_Array_Construct(&definitions);
    ZrCore_Array_Construct(&references);

    TEST_START("LSP Descriptor Plugin Member Completion Definition And References");
    TEST_INFO("Descriptor plugin imported members",
              "Imported descriptor-plugin members should share completion, definition, and references behavior with other external metadata-backed modules");

    if (!prepare_generated_descriptor_plugin_fixture("project_features_descriptor_plugin_member_navigation",
                                                     ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                                                     &fixture)) {
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "Failed to prepare generated descriptor plugin fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "Failed to load generated descriptor plugin fixture");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    pluginUri = create_file_uri_from_native_path(state, fixture.pluginPath);
    if (mainUri == ZR_NULL || pluginUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "plugin.answer", 0, 7, &completionPosition) ||
        !lsp_find_position_for_substring(mainContent, "plugin.answer", 0, 7, &firstUsagePosition) ||
        !lsp_find_position_for_substring(mainContent, "plugin.answer", 1, 7, &secondUsagePosition) ||
        !lsp_find_position_for_substring(mainContent, "plugin.answer", 2, 7, &thirdUsagePosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "Failed to open generated descriptor plugin module or compute completion/reference positions");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "answer")) {
        free(mainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "Descriptor-plugin receiver completion should list native plugin functions");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, firstUsagePosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, pluginUri, 0, 0, 0, 0)) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "Goto definition on a descriptor-plugin member should resolve to the plugin metadata source");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, pluginUri, (SZrLspPosition){0, 0}, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, pluginUri, 0, 0, 0, 0)) {
        free(mainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "Goto definition on the plugin metadata entry should stay on the same declaration target");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, mainUri, firstUsagePosition, ZR_TRUE, &references) ||
        references.length < 4 ||
        !location_array_contains_uri_and_range(&references, pluginUri, 0, 0, 0, 0) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character + 6) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character + 6) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               thirdUsagePosition.line,
                                               thirdUsagePosition.character,
                                               thirdUsagePosition.line,
                                               thirdUsagePosition.character + 6)) {
        free(mainContent);
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "Descriptor-plugin references should include the plugin declaration entry and every project usage");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state,
                                             context,
                                             pluginUri,
                                             (SZrLspPosition){0, 0},
                                             ZR_TRUE,
                                             &references) ||
        references.length < 4 ||
        !location_array_contains_uri_and_range(&references, pluginUri, 0, 0, 0, 0) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character + 6) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character + 6) ||
        !location_array_contains_uri_and_range(&references,
                                               mainUri,
                                               thirdUsagePosition.line,
                                               thirdUsagePosition.character,
                                               thirdUsagePosition.line,
                                               thirdUsagePosition.character + 6)) {
        free(mainContent);
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Member Completion Definition And References",
                  "References from the plugin metadata entry should resolve back to every imported project usage");
        return;
    }

    free(mainContent);
    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Descriptor Plugin Member Completion Definition And References");
}

static void test_lsp_descriptor_plugin_project_local_definition_overrides_stale_registry(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedDescriptorPluginFixture intFixture;
    SZrGeneratedDescriptorPluginFixture floatFixture;
    SZrLspContext *context;
    TZrSize intMainLength = 0;
    TZrSize floatMainLength = 0;
    TZrChar *intMainContent;
    TZrChar *floatMainContent;
    SZrString *intMainUri = ZR_NULL;
    SZrString *floatMainUri = ZR_NULL;
    SZrString *intPluginUri = ZR_NULL;
    SZrString *floatPluginUri = ZR_NULL;
    SZrLspPosition intUsagePosition;
    SZrLspPosition floatCompletionPosition;
    SZrLspPosition floatImportPosition;
    SZrLspPosition floatUsagePosition;
    SZrLspHover *hover = ZR_NULL;
    SZrArray completions;
    SZrArray definitions;
    SZrArray references;
    TZrChar completionLabels[256];
    const TZrChar *completionDetail;

    ZrCore_Array_Construct(&completions);
    ZrCore_Array_Construct(&definitions);
    ZrCore_Array_Construct(&references);

    TEST_START("LSP Descriptor Plugin Project Local Definition Overrides Stale Registry");
    TEST_INFO("Descriptor plugin project-local precedence",
              "A later-opened project with the same descriptor-plugin module name should override stale registry source paths for hover, definition, and references");

    if (!prepare_generated_descriptor_plugin_fixture("project_features_descriptor_plugin_project_override_int",
                                                     ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                                                     &intFixture) ||
        !prepare_generated_descriptor_plugin_fixture("project_features_descriptor_plugin_project_override_float",
                                                     ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH,
                                                     &floatFixture)) {
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  "Failed to prepare descriptor-plugin override fixtures");
        return;
    }

    intMainContent = read_fixture_text_file(intFixture.mainPath, &intMainLength);
    floatMainContent = read_fixture_text_file(floatFixture.mainPath, &floatMainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (intMainContent == ZR_NULL || floatMainContent == ZR_NULL || context == ZR_NULL) {
        free(intMainContent);
        free(floatMainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  "Failed to load descriptor-plugin override fixtures");
        return;
    }

    intMainUri = create_file_uri_from_native_path(state, intFixture.mainPath);
    floatMainUri = create_file_uri_from_native_path(state, floatFixture.mainPath);
    intPluginUri = create_file_uri_from_native_path(state, intFixture.pluginPath);
    floatPluginUri = create_file_uri_from_native_path(state, floatFixture.pluginPath);
    if (intMainUri == ZR_NULL || floatMainUri == ZR_NULL || intPluginUri == ZR_NULL || floatPluginUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, intMainUri, intMainContent, intMainLength, 1) ||
        !lsp_find_position_for_substring(intMainContent, "plugin.answer", 0, 7, &intUsagePosition) ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, floatMainUri, floatMainContent, floatMainLength, 1) ||
        !lsp_find_position_for_substring(floatMainContent, "plugin.answer", 0, 7, &floatCompletionPosition) ||
        !lsp_find_position_for_substring(floatMainContent, "\"zr.pluginprobe\"", 0, 1, &floatImportPosition) ||
        !lsp_find_position_for_substring(floatMainContent, "plugin.answer", 0, 7, &floatUsagePosition)) {
        free(intMainContent);
        free(floatMainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  "Failed to open override fixture documents or compute imported-member positions");
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, intMainUri, intUsagePosition, &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, "int") ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        free(intMainContent);
        free(floatMainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  "Initial descriptor-plugin hover should bind the first project's int plugin");
        return;
    }

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, floatMainUri, floatUsagePosition, &hover) ||
        hover == ZR_NULL || !hover_contains_text(hover, "float") ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        free(intMainContent);
        free(floatMainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  "Second project hover should refresh the same module name to the local float plugin");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, floatMainUri, floatCompletionPosition, &completions) ||
        !completion_array_contains_label(&completions, "answer") ||
        !completion_detail_contains_fragment(&completions, "answer", "float") ||
        completion_detail_contains_fragment(&completions, "answer", "int")) {
        completionDetail = completion_detail_for_label(&completions, "answer");
        describe_completion_labels(&completions, completionLabels, sizeof(completionLabels));
        free(intMainContent);
        free(floatMainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  completionDetail != ZR_NULL
                      ? completionDetail
                      : (completionLabels[0] != '\0'
                             ? completionLabels
                             : "Completion should prefer the active project's float plugin descriptor detail over stale int prototype data"));
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, floatMainUri, floatImportPosition, &definitions) ||
        !location_array_contains_uri_and_range(&definitions, floatPluginUri, 0, 0, 0, 0) ||
        location_array_contains_uri_and_range(&definitions, intPluginUri, 0, 0, 0, 0)) {
        free(intMainContent);
        free(floatMainContent);
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  "Definition should prefer the active project's descriptor plugin path over stale registry entries");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, floatMainUri, floatUsagePosition, ZR_TRUE, &references) ||
        !location_array_contains_uri_and_range(&references, floatPluginUri, 0, 0, 0, 0) ||
        location_array_contains_uri_and_range(&references, intPluginUri, 0, 0, 0, 0)) {
        free(intMainContent);
        free(floatMainContent);
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry",
                  "References should include only the active project's descriptor plugin declaration entry");
        return;
    }

    free(intMainContent);
    free(floatMainContent);
    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Descriptor Plugin Project Local Definition Overrides Stale Registry");
}

static void test_lsp_binary_import_document_highlights_cover_all_local_usages(SZrState *state) {
    SZrTestTimer timer;
    SZrGeneratedBinaryMetadataFixture fixture;
    SZrLspContext *context;
    const TZrChar *customMainContent =
        "var binaryStage = %import(\"graph_binary_stage\");\n"
        "var left = <int> binaryStage.binarySeed();\n"
        "var right = <int> binaryStage.binarySeed();\n"
        "return left + right;\n";
    TZrSize mainLength = strlen(customMainContent);
    SZrString *mainUri = ZR_NULL;
    SZrLspPosition firstUsagePosition;
    SZrLspPosition secondUsagePosition;
    SZrArray highlights;

    TEST_START("LSP Binary Import Document Highlights Cover All Local Usages");
    TEST_INFO("Binary import document highlights",
              "Document highlights on binary-only imported members should mark every same-document usage through the shared external-symbol path");

    if (!prepare_generated_binary_metadata_fixture("project_features_binary_import_highlights", &fixture) ||
        !write_text_file(fixture.mainPath, customMainContent, mainLength)) {
        TEST_FAIL(timer,
                  "LSP Binary Import Document Highlights Cover All Local Usages",
                  "Failed to prepare generated binary highlight fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Binary Import Document Highlights Cover All Local Usages",
                  "Failed to allocate LSP context");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    if (mainUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, customMainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(customMainContent, "binarySeed", 0, 0, &firstUsagePosition) ||
        !lsp_find_position_for_substring(customMainContent, "binarySeed", 1, 0, &secondUsagePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Document Highlights Cover All Local Usages",
                  "Failed to open binary highlight fixture or compute usage positions");
        return;
    }

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, mainUri, firstUsagePosition, &highlights) ||
        highlights.length < 2 ||
        !highlight_array_contains_range(&highlights,
                                        firstUsagePosition.line,
                                        firstUsagePosition.character,
                                        firstUsagePosition.line,
                                        firstUsagePosition.character + 10) ||
        !highlight_array_contains_range(&highlights,
                                        secondUsagePosition.line,
                                        secondUsagePosition.character,
                                        secondUsagePosition.line,
                                        secondUsagePosition.character + 10)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Binary Import Document Highlights Cover All Local Usages",
                  "Document highlights on binary imported members should include every same-document usage");
        return;
    }

    ZrCore_Array_Free(state, &highlights);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Binary Import Document Highlights Cover All Local Usages");
}

static void test_lsp_native_import_member_references_and_highlights(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "system.console;\n"
        "var capture = system.console;\n"
        "return system.console;\n";
    SZrString *uri;
    SZrLspPosition firstUsagePosition;
    SZrLspPosition secondUsagePosition;
    SZrLspPosition thirdUsagePosition;
    SZrArray references;
    SZrArray highlights;

    TEST_START("LSP Native Import Member References And Highlights");
    TEST_INFO("Native import references / highlights",
              "Native imported members should use the same external-symbol reference/highlight path as binary metadata members");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///native_import_member_references.zr",
                               strlen("file:///native_import_member_references.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "console", 0, 0, &firstUsagePosition) ||
        !lsp_find_position_for_substring(content, "console", 1, 0, &secondUsagePosition) ||
        !lsp_find_position_for_substring(content, "console", 2, 0, &thirdUsagePosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Import Member References And Highlights",
                  "Failed to prepare native import member fixture");
        return;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, firstUsagePosition, ZR_TRUE, &references) ||
        references.length < 3 ||
        !location_array_contains_uri_and_range(&references,
                                               uri,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character,
                                               firstUsagePosition.line,
                                               firstUsagePosition.character + 7) ||
        !location_array_contains_uri_and_range(&references,
                                               uri,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character,
                                               secondUsagePosition.line,
                                               secondUsagePosition.character + 7) ||
        !location_array_contains_uri_and_range(&references,
                                               uri,
                                               thirdUsagePosition.line,
                                               thirdUsagePosition.character,
                                               thirdUsagePosition.line,
                                               thirdUsagePosition.character + 7)) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Import Member References And Highlights",
                  "Native imported member references should include every project/document usage even without a writable declaration");
        return;
    }
    ZrCore_Array_Free(state, &references);

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 8);
    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, firstUsagePosition, &highlights) ||
        highlights.length < 3 ||
        !highlight_array_contains_range(&highlights,
                                        firstUsagePosition.line,
                                        firstUsagePosition.character,
                                        firstUsagePosition.line,
                                        firstUsagePosition.character + 7) ||
        !highlight_array_contains_range(&highlights,
                                        secondUsagePosition.line,
                                        secondUsagePosition.character,
                                        secondUsagePosition.line,
                                        secondUsagePosition.character + 7) ||
        !highlight_array_contains_range(&highlights,
                                        thirdUsagePosition.line,
                                        thirdUsagePosition.character,
                                        thirdUsagePosition.line,
                                        thirdUsagePosition.character + 7)) {
        ZrCore_Array_Free(state, &highlights);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Import Member References And Highlights",
                  "Native imported member highlights should include all same-document usages");
        return;
    }

    ZrCore_Array_Free(state, &highlights);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Import Member References And Highlights");
}

static void test_lsp_watched_binary_metadata_refresh_bootstraps_unopened_projects(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrGeneratedBinaryMetadataFixture fixture;
    SZrString *mainUri = ZR_NULL;
    SZrString *metadataUri = ZR_NULL;
    SZrArray workspaceSymbols;

    ZrCore_Array_Construct(&workspaceSymbols);

    TEST_START("LSP Watched Binary Metadata Refresh Bootstraps Unopened Projects");
    TEST_INFO("Watched binary metadata bootstrap",
              "Reloading binary metadata from workspace file events should discover and index the owning project even before any source file was opened");

    if (!prepare_generated_binary_metadata_fixture("project_features_binary_watch_bootstrap", &fixture)) {
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Bootstraps Unopened Projects",
                  "Failed to prepare generated binary metadata fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Bootstraps Unopened Projects",
                  "Failed to allocate LSP context");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    metadataUri = create_file_uri_from_native_path(state, fixture.metadataPath);
    if (mainUri == ZR_NULL || metadataUri == ZR_NULL ||
        ZrLanguageServer_Lsp_ProjectContainsUri(state, context, mainUri) ||
        !ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(state, context, metadataUri) ||
        !ZrLanguageServer_Lsp_ProjectContainsUri(state, context, mainUri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Bootstraps Unopened Projects",
                  "Binary metadata watched refresh should discover and register the owning unopened project");
        return;
    }

    ZrCore_Array_Init(state, &workspaceSymbols, sizeof(SZrLspSymbolInformation *), 8);
    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(state,
                                                  context,
                                                  ZrCore_String_Create(state, "merged", 6),
                                                  &workspaceSymbols) ||
        !symbol_array_contains_name(&workspaceSymbols, "merged")) {
        ZrCore_Array_Free(state, &workspaceSymbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Bootstraps Unopened Projects",
                  "Bootstrapped project refresh should index the entry module into workspace symbols");
        return;
    }

    ZrCore_Array_Free(state, &workspaceSymbols);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Watched Binary Metadata Refresh Bootstraps Unopened Projects");
}

static void test_lsp_watched_descriptor_plugin_refresh_bootstraps_unopened_projects(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrGeneratedDescriptorPluginFixture fixture;
    SZrString *mainUri = ZR_NULL;
    SZrString *pluginUri = ZR_NULL;
    SZrArray workspaceSymbols;

    ZrCore_Array_Construct(&workspaceSymbols);

    TEST_START("LSP Watched Descriptor Plugin Refresh Bootstraps Unopened Projects");
    TEST_INFO("Watched descriptor plugin bootstrap",
              "Reloading a watched native descriptor plugin should discover and index the owning unopened project before any source document is opened");

    if (!prepare_generated_descriptor_plugin_fixture("project_features_descriptor_plugin_watch_bootstrap",
                                                     ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                                                     &fixture)) {
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Bootstraps Unopened Projects",
                  "Failed to prepare generated descriptor plugin bootstrap fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Bootstraps Unopened Projects",
                  "Failed to allocate LSP context");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    pluginUri = create_file_uri_from_native_path(state, fixture.pluginPath);
    if (mainUri == ZR_NULL || pluginUri == ZR_NULL ||
        ZrLanguageServer_Lsp_ProjectContainsUri(state, context, mainUri) ||
        !ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(state, context, pluginUri) ||
        !ZrLanguageServer_Lsp_ProjectContainsUri(state, context, mainUri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Bootstraps Unopened Projects",
                  "Descriptor plugin watched refresh should discover and register the owning unopened project");
        return;
    }

    ZrCore_Array_Init(state, &workspaceSymbols, sizeof(SZrLspSymbolInformation *), 8);
    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(state,
                                                  context,
                                                  ZrCore_String_Create(state, "usePlugin", 9),
                                                  &workspaceSymbols) ||
        !symbol_array_contains_name(&workspaceSymbols, "usePlugin")) {
        ZrCore_Array_Free(state, &workspaceSymbols);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Bootstraps Unopened Projects",
                  "Bootstrapped descriptor plugin refresh should index the entry module into workspace symbols");
        return;
    }

    ZrCore_Array_Free(state, &workspaceSymbols);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Watched Descriptor Plugin Refresh Bootstraps Unopened Projects");
}

static void test_lsp_watched_binary_metadata_refresh_invalidates_module_cache_keys(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrGeneratedBinaryMetadataFixture fixture;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *metadataUri = ZR_NULL;
    SZrString *moduleNameKey = ZR_NULL;
    SZrString *metadataPathKey = ZR_NULL;
    SZrObjectModule *dummyModule = ZR_NULL;

    TEST_START("LSP Watched Binary Metadata Refresh Invalidates Module Cache Keys");
    TEST_INFO("Watched binary metadata cache invalidation",
              "Reloading watched binary metadata should evict both the metadata-path cache key and the logical module-name cache key before rebuilding project metadata");

    if (!prepare_generated_binary_metadata_fixture("project_features_binary_watch_cache", &fixture)) {
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Invalidates Module Cache Keys",
                  "Failed to prepare generated binary metadata fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Invalidates Module Cache Keys",
                  "Failed to allocate LSP state for binary metadata cache invalidation");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    metadataUri = create_file_uri_from_native_path(state, fixture.metadataPath);
    moduleNameKey = ZrCore_String_CreateFromNative(state, "graph_binary_stage");
    metadataPathKey = ZrCore_String_CreateFromNative(state, fixture.metadataPath);
    dummyModule = ZrCore_Module_Create(state);
    if (mainUri == ZR_NULL || metadataUri == ZR_NULL || moduleNameKey == ZR_NULL || metadataPathKey == ZR_NULL ||
        dummyModule == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Invalidates Module Cache Keys",
                  "Failed to load the generated project fixture before seeding module cache entries");
        return;
    }

    ZrCore_Module_AddToCache(state, moduleNameKey, dummyModule);
    ZrCore_Module_AddToCache(state, metadataPathKey, dummyModule);
    if (ZrCore_Module_GetFromCache(state, moduleNameKey) == ZR_NULL ||
        ZrCore_Module_GetFromCache(state, metadataPathKey) == ZR_NULL) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Invalidates Module Cache Keys",
                  "Failed to seed stale module cache entries for the watched metadata fixture");
        return;
    }

    if (!ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(state, context, metadataUri) ||
        ZrCore_Module_GetFromCache(state, moduleNameKey) != ZR_NULL ||
        ZrCore_Module_GetFromCache(state, metadataPathKey) != ZR_NULL) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Invalidates Module Cache Keys",
                  "Watched binary metadata refresh should evict stale module cache keys before project rebuild");
        return;
    }

    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Watched Binary Metadata Refresh Invalidates Module Cache Keys");
}

static void test_lsp_watched_binary_metadata_refresh_reanalyzes_open_documents(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrGeneratedBinaryMetadataFixture fixture;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *metadataUri = ZR_NULL;
    SZrLspPosition completionPosition;
    SZrLspPosition hoverPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Watched Binary Metadata Refresh Reanalyzes Open Documents");
    TEST_INFO("Watched binary metadata refresh",
              "Reloading a project's binary metadata should invalidate and reanalyze already-open documents that consume imported binary facts");

    if (!prepare_generated_binary_metadata_fixture("project_features_binary_watch", &fixture)) {
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Failed to prepare generated binary refresh fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Failed to prepare LSP state for binary metadata refresh test");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    metadataUri = create_file_uri_from_native_path(state, fixture.metadataPath);
    if (mainUri == ZR_NULL || metadataUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "binaryStage.binarySeed", 0, 12, &completionPosition) ||
        !lsp_find_position_for_substring(mainContent, "binarySeed", 0, 0, &hoverPosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Failed to open generated project fixture or compute hover position");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "binarySeed") ||
        !completion_detail_contains_fragment(&completions, "binarySeed", "int") ||
        completion_detail_contains_fragment(&completions, "binarySeed", "float")) {
        free(mainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Initial completion should resolve imported binary metadata through the current metadata detail");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, hoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "binarySeed") ||
        !hover_contains_text(hover, "int") ||
        !hover_contains_text(hover, "Source: binary metadata")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Initial hover should resolve imported binary metadata through the binary metadata source path");
        return;
    }

    if (!replace_text_in_file(fixture.metadataPath, "fn binarySeed(): int", "fn binarySeed(): float") ||
        !ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(state, context, metadataUri)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Failed to update watched binary metadata and trigger project reload");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "binarySeed") ||
        !completion_detail_contains_fragment(&completions, "binarySeed", "float") ||
        completion_detail_contains_fragment(&completions, "binarySeed", "int")) {
        free(mainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Watched binary metadata refresh should invalidate stale imported completion detail as well as hover");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, hoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "binarySeed") ||
        !hover_contains_text(hover, "float") ||
        !hover_contains_text(hover, "Source: binary metadata")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents",
                  "Watched binary metadata refresh should invalidate stale analyzers so hover reflects updated imported return types");
        return;
    }

    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Watched Binary Metadata Refresh Reanalyzes Open Documents");
}

static void test_lsp_watched_descriptor_plugin_refresh_reanalyzes_open_documents(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrGeneratedDescriptorPluginFixture fixture;
    TZrSize mainLength = 0;
    TZrChar *mainContent;
    SZrString *mainUri = ZR_NULL;
    SZrString *pluginUri = ZR_NULL;
    SZrLspPosition completionPosition;
    SZrLspPosition hoverPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *completionDetail;

    TEST_START("LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents");
    TEST_INFO("Watched descriptor plugin refresh",
              "Reloading a watched native descriptor plugin should invalidate project metadata and refresh imported plugin hover results in already-open documents");

    if (!prepare_generated_descriptor_plugin_fixture("project_features_descriptor_plugin_watch",
                                                     ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH,
                                                     &fixture)) {
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  "Failed to prepare generated descriptor plugin refresh fixture");
        return;
    }

    mainContent = read_fixture_text_file(fixture.mainPath, &mainLength);
    context = ZrLanguageServer_LspContext_New(state);
    if (mainContent == ZR_NULL || context == ZR_NULL) {
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  "Failed to prepare LSP state for descriptor plugin refresh test");
        return;
    }

    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    pluginUri = create_file_uri_from_native_path(state, fixture.pluginPath);
    if (mainUri == ZR_NULL || pluginUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, mainLength, 1) ||
        !lsp_find_position_for_substring(mainContent, "plugin.answer", 0, 7, &completionPosition) ||
        !lsp_find_position_for_substring(mainContent, "plugin.answer", 0, 8, &hoverPosition)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  "Failed to open generated descriptor plugin project or compute hover position");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "answer") ||
        !completion_detail_contains_fragment(&completions, "answer", "int") ||
        completion_detail_contains_fragment(&completions, "answer", "float")) {
        free(mainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  "Initial completion should resolve imported descriptor-plugin members through the current plugin metadata detail");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, hoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "answer") ||
        !hover_contains_text(hover, "int") ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  "Initial hover should resolve imported descriptor-plugin members through the native descriptor plugin path");
        return;
    }

    if (!ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(state, context, pluginUri) ||
        !copy_fixture_binary_file(ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_FLOAT_PATH, fixture.pluginPath) ||
        !ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(state, context, pluginUri)) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  "Failed to replace the watched descriptor plugin fixture and trigger project reload");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, mainUri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "answer") ||
        !completion_detail_contains_fragment(&completions, "answer", "float") ||
        completion_detail_contains_fragment(&completions, "answer", "int")) {
        completionDetail = completion_detail_for_label(&completions, "answer");
        free(mainContent);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  completionDetail != ZR_NULL
                      ? completionDetail
                      : "Watched descriptor plugin refresh should invalidate stale imported completion detail as well as hover");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    hover = ZR_NULL;
    if (!ZrLanguageServer_Lsp_GetHover(state, context, mainUri, hoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "answer") ||
        !hover_contains_text(hover, "float") ||
        !hover_contains_text(hover, "Source: native descriptor plugin")) {
        free(mainContent);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents",
                  "Watched descriptor plugin refresh should invalidate stale plugin descriptors so hover reflects updated imported return types");
        return;
    }

    free(mainContent);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents");
}

static void test_lsp_container_native_members_surface_closed_types_and_completions(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *content =
        "var container = %import(\"zr.container\");\n"
        "var xs: Array<int> = null;\n"
        "var map: Map<string, int> = null;\n"
        "var list: LinkedList<int> = null;\n"
        "var node: LinkedNode<int> = null;\n"
        "list.addLast;\n"
        "container.Array;\n"
        "xs.length;\n"
        "node.value;\n";
    SZrString *uri;
    SZrLspPosition moduleCompletion;
    SZrLspPosition arrayCompletion;
    SZrLspPosition nodeCompletion;
    SZrLspPosition nodeHoverPosition;
    SZrLspPosition addLastHoverPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;
    TZrChar completionLabels[512];

    TEST_START("LSP Container Native Members Surface Closed Types And Completions");
    TEST_INFO("Container completions / hover",
              "Native container modules and closed generic instances should expose member completions and resolved hover text");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///container_native_members.zr",
                               strlen("file:///container_native_members.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "container.Array", 0, 10, &moduleCompletion) ||
        !lsp_find_position_for_substring(content, "xs.length", 0, 3, &arrayCompletion) ||
        !lsp_find_position_for_substring(content, "node.value", 0, 5, &nodeCompletion) ||
        !lsp_find_position_for_substring(content, "node.value", 0, 0, &nodeHoverPosition) ||
        !lsp_find_position_for_substring(content, "addLast", 0, 0, &addLastHoverPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Native Members Surface Closed Types And Completions",
                  "Failed to prepare container-native LSP fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, moduleCompletion, &completions) ||
        !completion_array_contains_label(&completions, "Array") ||
        !completion_array_contains_label(&completions, "Map") ||
        !completion_array_contains_label(&completions, "Set") ||
        !completion_array_contains_label(&completions, "LinkedList") ||
        !completion_array_contains_label(&completions, "LinkedNode")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Native Members Surface Closed Types And Completions",
                  "Container module completion should list native container interfaces and types");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, arrayCompletion, &completions) ||
        !completion_array_contains_label(&completions, "add") ||
        !completion_array_contains_label(&completions, "insert") ||
        !completion_array_contains_label(&completions, "length") ||
        !completion_array_contains_label(&completions, "capacity")) {
        describe_completion_labels(&completions, completionLabels, sizeof(completionLabels));
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Native Members Surface Closed Types And Completions",
                  completionLabels[0] != '\0'
                      ? completionLabels
                      : "Array<int> completion should list sequence members");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, nodeCompletion, &completions) ||
        !completion_array_contains_label(&completions, "value") ||
        !completion_array_contains_label(&completions, "next") ||
        !completion_array_contains_label(&completions, "previous")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Native Members Surface Closed Types And Completions",
                  "LinkedNode<int> completion should list value/next/previous");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, nodeHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "LinkedNode<int>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Native Members Surface Closed Types And Completions",
                  "Hover on node reference should surface the resolved closed type");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, addLastHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "addLast") ||
        !hover_contains_text(hover, "LinkedNode<int>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Container Native Members Surface Closed Types And Completions",
                  "Hover on addLast should expose the closed generic method signature");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Container Native Members Surface Closed Types And Completions");
}

static void test_lsp_ffi_pointer_types_surface_hover_and_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *content =
        "var ffi = %import(\"zr.ffi\");\n"
        "var buffer = ffi.BufferHandle.allocate(8);\n"
        "var bytePtr = buffer.pin();\n"
        "var typedPtr = bytePtr.as({ kind: \"pointer\", to: \"i32\", direction: \"inout\" });\n"
        "var direct: ffi.Ptr<u8> = bytePtr;\n"
        "ffi.Ptr;\n"
        "typedPtr.read;\n"
        "typedPtr;\n"
        "direct;\n";
    SZrString *uri;
    SZrLspPosition moduleCompletion;
    SZrLspPosition methodCompletion;
    SZrLspPosition typedHoverPosition;
    SZrLspPosition directHoverPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP FFI Pointer Types Surface Hover And Completion");
    TEST_INFO("FFI native pointer types",
              "zr.ffi should surface Ptr/Ptr32/Ptr64/Char/WChar and propagate pointer helper types through hover and member completion");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///ffi_pointer_types.zr",
                               strlen("file:///ffi_pointer_types.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !lsp_find_position_for_substring(content, "ffi.Ptr;", 0, 4, &moduleCompletion) ||
        !lsp_find_position_for_substring(content, "typedPtr.read", 0, 9, &methodCompletion) ||
        !lsp_find_position_for_substring(content, "typedPtr;", 0, 0, &typedHoverPosition) ||
        !lsp_find_position_for_substring(content, "direct;", 0, 0, &directHoverPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Types Surface Hover And Completion",
                  "Failed to prepare ffi pointer type fixture");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, moduleCompletion, &completions) ||
        !completion_array_contains_label(&completions, "Ptr") ||
        !completion_array_contains_label(&completions, "Ptr32") ||
        !completion_array_contains_label(&completions, "Ptr64") ||
        !completion_array_contains_label(&completions, "Char") ||
        !completion_array_contains_label(&completions, "WChar")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Types Surface Hover And Completion",
                  "ffi module completion should list semantic pointer and ABI wrapper types");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, methodCompletion, &completions) ||
        !completion_array_contains_label(&completions, "as") ||
        !completion_array_contains_label(&completions, "read") ||
        !completion_array_contains_label(&completions, "close")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Types Surface Hover And Completion",
                  "Ptr<i32> completion should surface pointer helper members");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, typedHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Ptr<i32>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Types Surface Hover And Completion",
                  "Hover on typedPtr should expose the refined Ptr<i32> type");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, directHoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "Ptr<u8>")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP FFI Pointer Types Surface Hover And Completion",
                  "Hover on direct ffi.Ptr<u8> annotation should resolve the closed pointer type");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP FFI Pointer Types Surface Hover And Completion");
}

static void test_lsp_semantic_tokens_cover_keywords_and_symbols(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *content =
        "var system = %import(\"zr.system\");\n"
        "class Foo {\n"
        "    %borrowed pub work(arg: int) {\n"
        "        system.console.print(\"x\");\n"
        "    }\n"
        "}\n";
    SZrString *uri;
    SZrArray tokens;

    TEST_START("LSP Semantic Tokens Cover Keywords And Symbols");
    TEST_INFO("Semantic Tokens", "Semantic tokens should classify keywords, namespaces, types, and methods");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///semantic_tokens.zr",
                               strlen("file:///semantic_tokens.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Semantic Tokens Cover Keywords And Symbols", "Failed to prepare semantic token source");
        return;
    }

    ZrCore_Array_Init(state, &tokens, sizeof(TZrUInt32), 32);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, &tokens) ||
        !semantic_tokens_contain(&tokens, 0, 13, 7, "keyword") ||
        !semantic_tokens_contain(&tokens, 1, 6, 3, "class") ||
        !semantic_tokens_contain(&tokens, 2, 4, 9, "keyword") ||
        !semantic_tokens_contain(&tokens, 3, 8, 6, "namespace") ||
        !semantic_tokens_contain(&tokens, 3, 23, 5, "method")) {
        ZrCore_Array_Free(state, &tokens);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Semantic Tokens Cover Keywords And Symbols", "Expected semantic token coverage for %import, ownership, class names, module aliases, and methods");
        return;
    }

    ZrCore_Array_Free(state, &tokens);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Tokens Cover Keywords And Symbols");
}

static void test_lsp_semantic_tokens_cover_decorators_and_meta_methods(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *content =
        "#singleton#\n"
        "class Foo {\n"
        "    pub @constructor() { }\n"
        "}\n"
        "%compileTime var MAX_SIZE = 1;\n";
    SZrString *uri;
    SZrArray tokens;

    TEST_START("LSP Semantic Tokens Cover Decorators And Meta Methods");
    TEST_INFO("Semantic Tokens", "Semantic tokens should classify decorators and @meta-method declarations as first-class language tokens");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///semantic_tokens_meta.zr",
                               strlen("file:///semantic_tokens_meta.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Tokens Cover Decorators And Meta Methods",
                  "Failed to prepare semantic token source");
        return;
    }

    ZrCore_Array_Init(state, &tokens, sizeof(TZrUInt32), 32);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, &tokens) ||
        !semantic_tokens_contain(&tokens, 0, 0, 11, "decorator") ||
        !semantic_tokens_contain(&tokens, 1, 6, 3, "class") ||
        !semantic_tokens_contain(&tokens, 2, 8, 12, "metaMethod") ||
        !semantic_tokens_contain(&tokens, 4, 0, 12, "keyword")) {
        ZrCore_Array_Free(state, &tokens);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Semantic Tokens Cover Decorators And Meta Methods",
                  "Expected semantic token coverage for #decorator#, class names, @meta-methods, and %compileTime");
        return;
    }

    ZrCore_Array_Free(state, &tokens);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Semantic Tokens Cover Decorators And Meta Methods");
}

static void test_lsp_native_value_constructor_members_surface_hover_and_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    const TZrChar *content =
        "var math = %import(\"zr.math\");\n"
        "runImpl() {\n"
        "    return $math.Vector3(4.0, 5.0, 6.0).y;\n"
        "}\n";
    SZrString *uri;
    SZrLspPosition completionPosition;
    SZrLspPosition hoverPosition;
    SZrArray completions;
    SZrLspHover *hover = ZR_NULL;

    TEST_START("LSP Native Value Constructor Members Surface Hover And Completion");
    TEST_INFO("Native value constructors",
              "Direct $module.Type(...) receivers should expose native member completion and field hover");

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state,
                               "file:///native_value_constructor_members.zr",
                               strlen("file:///native_value_constructor_members.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Value Constructor Members Surface Hover And Completion",
                  "Failed to prepare native value constructor source");
        return;
    }

    if (!lsp_find_position_for_substring(content, ").y", 0, 2, &completionPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Value Constructor Members Surface Hover And Completion",
                  "Failed to compute completion position for native value member");
        return;
    }

    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 16);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, completionPosition, &completions) ||
        !completion_array_contains_label(&completions, "x") ||
        !completion_array_contains_label(&completions, "y") ||
        !completion_array_contains_label(&completions, "z")) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Value Constructor Members Surface Hover And Completion",
                  "Expected $math.Vector3(...). completion to list Vector3 fields");
        return;
    }
    ZrCore_Array_Free(state, &completions);

    if (!lsp_find_position_for_substring(content, ").y", 0, 2, &hoverPosition)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Value Constructor Members Surface Hover And Completion",
                  "Failed to compute hover position for native value field");
        return;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, hoverPosition, &hover) ||
        hover == ZR_NULL ||
        !hover_contains_text(hover, "field") ||
        !hover_contains_text(hover, "y") ||
        !hover_contains_text(hover, "float") ||
        !hover_contains_text(hover, "Vector3")) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Native Value Constructor Members Surface Hover And Completion",
                  "Hover on $math.Vector3(...).y should expose the native field type and receiver");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Native Value Constructor Members Surface Hover And Completion");
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

    printf("==========\n");
    printf("Language Server - Project Feature Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Fail - Failed to create global state\n");
        return 1;
    }

    state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Fail - Failed to get main thread state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(state, global);

    test_lsp_auto_discovers_project_from_source_file(state);
    TEST_DIVIDER();

    test_lsp_uses_nearest_ancestor_project(state);
    TEST_DIVIDER();

    test_lsp_ambiguous_project_directory_stays_standalone(state);
    TEST_DIVIDER();

    test_lsp_native_imports_and_ownership_display(state);
    TEST_DIVIDER();

    test_lsp_import_literal_navigation_and_hover(state);
    TEST_DIVIDER();

    test_lsp_import_literal_hover_identifies_ffi_source_wrapper(state);
    TEST_DIVIDER();

    test_lsp_import_literal_hover_identifies_native_descriptor_plugin(state);
    TEST_DIVIDER();

    test_lsp_import_literal_definition_targets_native_descriptor_plugin(state);
    TEST_DIVIDER();

    test_lsp_binary_import_literal_definition_targets_metadata(state);
    TEST_DIVIDER();

    test_lsp_binary_import_metadata_surfaces_hover_and_completion(state);
    TEST_DIVIDER();

    test_lsp_descriptor_plugin_member_completion_definition_and_references(state);
    TEST_DIVIDER();

    test_lsp_descriptor_plugin_project_local_definition_overrides_stale_registry(state);
    TEST_DIVIDER();

    test_lsp_binary_import_references_surface_metadata_and_usages(state);
    TEST_DIVIDER();

    test_lsp_binary_import_document_highlights_cover_all_local_usages(state);
    TEST_DIVIDER();

    test_lsp_native_import_member_references_and_highlights(state);
    TEST_DIVIDER();

    test_lsp_watched_binary_metadata_refresh_bootstraps_unopened_projects(state);
    TEST_DIVIDER();

    test_lsp_watched_descriptor_plugin_refresh_bootstraps_unopened_projects(state);
    TEST_DIVIDER();

    test_lsp_watched_binary_metadata_refresh_invalidates_module_cache_keys(state);
    TEST_DIVIDER();

    test_lsp_watched_binary_metadata_refresh_reanalyzes_open_documents(state);
    TEST_DIVIDER();

    test_lsp_watched_descriptor_plugin_refresh_reanalyzes_open_documents(state);
    TEST_DIVIDER();

    test_lsp_container_native_members_surface_closed_types_and_completions(state);
    TEST_DIVIDER();

    test_lsp_ffi_pointer_types_surface_hover_and_completion(state);
    TEST_DIVIDER();

    test_lsp_semantic_tokens_cover_keywords_and_symbols(state);
    TEST_DIVIDER();

    test_lsp_semantic_tokens_cover_decorators_and_meta_methods(state);
    TEST_DIVIDER();

    test_lsp_native_value_constructor_members_surface_hover_and_completion(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All Project Feature Tests Completed\n");
    printf("==========\n");
    return 0;
}

