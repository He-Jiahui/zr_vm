//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "zr_vm_language_server.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"

#ifndef ZR_VM_SOURCE_ROOT
#define ZR_VM_SOURCE_ROOT "."
#endif

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while(0)

#define TEST_PASS(timer, summary) do { \
    timer.endTime = clock(); \
    printf("Pass - Cost Time:%.3fms - %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary); \
    fflush(stdout); \
} while(0)

#define TEST_FAIL(timer, summary, reason) do { \
    timer.endTime = clock(); \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", \
           ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0, \
           summary, \
           reason); \
    fflush(stdout); \
} while(0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while(0)

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL) {
            if ((TZrPtr)pointer >= (TZrPtr)0x1000 && originalSize > 0 && originalSize < 1024 * 1024 * 1024) {
                free(pointer);
            }
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000 && originalSize > 0 && originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

static void build_source_path(char *buffer, size_t bufferSize, const char *relativePath) {
    snprintf(buffer, bufferSize, "%s/%s", ZR_VM_SOURCE_ROOT, relativePath);
}

static void path_to_file_uri(const char *path, char *buffer, size_t bufferSize) {
    size_t srcIndex = 0;
    size_t dstIndex = 0;

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    memset(buffer, 0, bufferSize);

#ifdef _WIN32
    if (strlen(path) > 1 && path[1] == ':') {
        snprintf(buffer, bufferSize, "file:///");
        dstIndex = strlen(buffer);
    } else {
        snprintf(buffer, bufferSize, "file://");
        dstIndex = strlen(buffer);
    }
#else
    snprintf(buffer, bufferSize, "file://");
    dstIndex = strlen(buffer);
#endif

    while (path[srcIndex] != '\0' && dstIndex + 1 < bufferSize) {
        buffer[dstIndex++] = path[srcIndex] == '\\' ? '/' : path[srcIndex];
        srcIndex++;
    }
    buffer[dstIndex] = '\0';
}

static char *read_text_file(const char *path, size_t *outLength) {
    FILE *file;
    long fileSize;
    size_t readSize;
    char *buffer;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0) {
        fclose(file);
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    readSize = fread(buffer, 1, (size_t)fileSize, file);
    fclose(file);
    if (readSize != (size_t)fileSize) {
        free(buffer);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    if (outLength != ZR_NULL) {
        *outLength = (size_t)fileSize;
    }

    return buffer;
}

static TZrBool update_document_with_content(SZrState *state,
                                            SZrLspContext *context,
                                            const char *path,
                                            const char *content,
                                            SZrString **outUri) {
    char uriBuffer[2048];

    if (state == ZR_NULL || context == ZR_NULL || path == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    path_to_file_uri(path, uriBuffer, sizeof(uriBuffer));
    if (outUri != ZR_NULL) {
        *outUri = ZrCore_String_Create(state, uriBuffer, strlen(uriBuffer));
    }

    return ZrLanguageServer_Lsp_UpdateDocument(state,
                                               context,
                                               outUri != ZR_NULL ? *outUri : ZrCore_String_Create(state, uriBuffer, strlen(uriBuffer)),
                                               content,
                                               strlen(content),
                                               1);
}

static TZrBool update_document_from_relative_file(SZrState *state,
                                                  SZrLspContext *context,
                                                  const char *relativePath,
                                                  SZrString **outUri,
                                                  char **outContent) {
    char pathBuffer[2048];
    size_t contentLength = 0;
    char *content;

    build_source_path(pathBuffer, sizeof(pathBuffer), relativePath);
    content = read_text_file(pathBuffer, &contentLength);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outContent != ZR_NULL) {
        *outContent = content;
    }

    if (!update_document_with_content(state, context, pathBuffer, content, outUri)) {
        free(content);
        if (outContent != ZR_NULL) {
            *outContent = ZR_NULL;
        }
        return ZR_FALSE;
    }

    ZR_UNUSED_PARAMETER(contentLength);
    return ZR_TRUE;
}

static SZrLspPosition position_for_offset(const char *content, size_t offset) {
    SZrLspPosition position;
    size_t index;

    position.line = 0;
    position.character = 0;
    if (content == ZR_NULL) {
        return position;
    }

    for (index = 0; index < offset && content[index] != '\0'; index++) {
        if (content[index] == '\n') {
            position.line++;
            position.character = 0;
        } else {
            position.character++;
        }
    }

    return position;
}

static SZrLspPosition position_for_nth_substring(const char *content,
                                                 const char *needle,
                                                 TZrSize occurrence,
                                                 TZrBool useEnd) {
    const char *cursor = content;
    const char *found = ZR_NULL;
    TZrSize remaining = occurrence;

    while (cursor != ZR_NULL && *cursor != '\0') {
        found = strstr(cursor, needle);
        if (found == ZR_NULL) {
            break;
        }

        if (remaining == 0) {
            break;
        }

        remaining--;
        cursor = found + 1;
    }

    if (found == ZR_NULL) {
        return position_for_offset(content, 0);
    }

    return position_for_offset(content,
                               (size_t)(found - content) + (useEnd ? strlen(needle) : 0));
}

static const char *zr_string_or_null(SZrString *string) {
    return string != ZR_NULL ? ZrCore_String_GetNativeString(string) : ZR_NULL;
}

static TZrBool locations_contain_uri(SZrArray *locations, const char *uri) {
    TZrSize i;

    if (locations == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    for (i = 0; i < locations->length; i++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, i);
        const char *candidateUri;
        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL || (*locationPtr)->uri == ZR_NULL) {
            continue;
        }

        candidateUri = ZrCore_String_GetNativeString((*locationPtr)->uri);
        if (candidateUri != ZR_NULL && strcmp(candidateUri, uri) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool locations_contain_uri_line(SZrArray *locations, const char *uri, TZrInt32 line) {
    TZrSize i;

    if (locations == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    for (i = 0; i < locations->length; i++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, i);
        const char *candidateUri;
        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL || (*locationPtr)->uri == ZR_NULL) {
            continue;
        }

        candidateUri = ZrCore_String_GetNativeString((*locationPtr)->uri);
        if (candidateUri != ZR_NULL &&
            strcmp(candidateUri, uri) == 0 &&
            (*locationPtr)->range.start.line == line) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void describe_location_lines(SZrArray *locations, char *buffer, size_t bufferSize) {
    TZrSize i;
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (locations == ZR_NULL) {
        return;
    }

    for (i = 0; i < locations->length && offset + 1 < bufferSize; i++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, i);
        int written;
        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL) {
            continue;
        }

        written = snprintf(buffer + offset,
                           bufferSize - offset,
                           "%s%d",
                           offset == 0 ? "" : ",",
                           (*locationPtr)->range.start.line);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            break;
        }
        offset += (size_t)written;
    }
}

static SZrLspCompletionItem *find_completion_item(SZrArray *items, const char *label) {
    TZrSize i;

    if (items == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < items->length; i++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, i);
        const char *candidateLabel;
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        candidateLabel = ZrCore_String_GetNativeString((*itemPtr)->label);
        if (candidateLabel != ZR_NULL && strcmp(candidateLabel, label) == 0) {
            return *itemPtr;
        }
    }

    return ZR_NULL;
}

static const char *lsp_completion_detail_string(SZrLspCompletionItem *item) {
    if (item == ZR_NULL || item->detail == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(item->detail);
}

static const char *lsp_completion_detail_for_label(SZrArray *items, const char *label) {
    SZrLspCompletionItem *item = find_completion_item(items, label);
    return lsp_completion_detail_string(item);
}

static const char *lsp_hover_first_content(SZrLspHover *hover) {
    SZrString **contentPtr;

    if (hover == ZR_NULL || !hover->contents.isValid || hover->contents.length == 0) {
        return ZR_NULL;
    }

    contentPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, 0);
    if (contentPtr == ZR_NULL || *contentPtr == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(*contentPtr);
}

static TZrBool lsp_diagnostics_contain_code(SZrArray *diagnostics, const char *code) {
    TZrSize i;

    if (diagnostics == ZR_NULL || code == ZR_NULL) {
        return ZR_FALSE;
    }

    for (i = 0; i < diagnostics->length; i++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, i);
        const char *candidateCode;
        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL || (*diagnosticPtr)->code == ZR_NULL) {
            continue;
        }

        candidateCode = ZrCore_String_GetNativeString((*diagnosticPtr)->code);
        if (candidateCode != ZR_NULL && strcmp(candidateCode, code) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void describe_lsp_diagnostics(SZrArray *diagnostics, char *buffer, size_t bufferSize) {
    TZrSize i;
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (diagnostics == ZR_NULL) {
        return;
    }

    for (i = 0; i < diagnostics->length && offset + 1 < bufferSize; i++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, i);
        const char *code;
        const char *message;
        int written;

        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL) {
            continue;
        }

        code = (*diagnosticPtr)->code != ZR_NULL
                   ? ZrCore_String_GetNativeString((*diagnosticPtr)->code)
                   : "<null>";
        message = (*diagnosticPtr)->message != ZR_NULL
                      ? ZrCore_String_GetNativeString((*diagnosticPtr)->message)
                      : "<null>";
        written = snprintf(buffer + offset,
                           bufferSize - offset,
                           "%s%s@%d:%d=%s",
                           offset == 0 ? "" : " | ",
                           code != ZR_NULL ? code : "<null>",
                           (*diagnosticPtr)->range.start.line,
                           (*diagnosticPtr)->range.start.character,
                           message != ZR_NULL ? message : "<null>");
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            break;
        }
        offset += (TZrSize)written;
    }
}

static SZrLspCodeAction *find_code_action_with_title_fragment(SZrArray *items, const char *fragment) {
    TZrSize i;

    if (items == ZR_NULL || fragment == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < items->length; i++) {
        SZrLspCodeAction **itemPtr = (SZrLspCodeAction **)ZrCore_Array_Get(items, i);
        const char *title;
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->title == ZR_NULL) {
            continue;
        }

        title = ZrCore_String_GetNativeString((*itemPtr)->title);
        if (title != ZR_NULL && strstr(title, fragment) != ZR_NULL) {
            return *itemPtr;
        }
    }

    return ZR_NULL;
}

static void test_lsp_context_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;

    TEST_START("LSP Context Creation and Free");
    TEST_INFO("LSP Context Creation", "Creating and freeing LSP context");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL || context->parser == ZR_NULL) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        TEST_FAIL(timer, "LSP Context Creation and Free", "Failed to create LSP context");
        return;
    }

    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Context Creation and Free");
}

static void test_lsp_project_cross_file_definition(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *mainUri = ZR_NULL;
    char *mainContent = ZR_NULL;
    char callbacksPath[2048];
    char callbacksUri[2048];
    SZrLspPosition position;
    SZrArray definitions;

    TEST_START("LSP Project Cross File Definition");
    TEST_INFO("Cross file definition", "Getting definition from an imported project module member should land in the sibling project script");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Project Cross File Definition", "Failed to create LSP context");
        return;
    }

    if (!update_document_from_relative_file(state,
                                            context,
                                            "tests/fixtures/projects/native_numeric_pipeline/src/main.zr",
                                            &mainUri,
                                            &mainContent)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Cross File Definition", "Failed to load fixture main.zr");
        return;
    }

    build_source_path(callbacksPath,
                      sizeof(callbacksPath),
                      "tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr");
    path_to_file_uri(callbacksPath, callbacksUri, sizeof(callbacksUri));

    position = position_for_nth_substring(mainContent, "runCallbacks", 0, ZR_FALSE);
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, mainUri, position, &definitions) ||
        !locations_contain_uri(&definitions, callbacksUri)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Project Cross File Definition",
                  "Expected imported project member definition to resolve to callbacks.zr");
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    free(mainContent);
    TEST_PASS(timer, "LSP Project Cross File Definition");
}

static void test_lsp_project_cross_file_references(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *mainUri = ZR_NULL;
    char *mainContent = ZR_NULL;
    char callbacksPath[2048];
    char callbacksUri[2048];
    SZrLspPosition position;
    SZrArray references;

    TEST_START("LSP Project Cross File References");
    TEST_INFO("Cross file references", "Finding references for an imported project member should include declaration and external use sites");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Project Cross File References", "Failed to create LSP context");
        return;
    }

    if (!update_document_from_relative_file(state,
                                            context,
                                            "tests/fixtures/projects/native_numeric_pipeline/src/main.zr",
                                            &mainUri,
                                            &mainContent)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Cross File References", "Failed to load fixture main.zr");
        return;
    }

    build_source_path(callbacksPath,
                      sizeof(callbacksPath),
                      "tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr");
    path_to_file_uri(callbacksPath, callbacksUri, sizeof(callbacksUri));

    position = position_for_nth_substring(mainContent, "runCallbacks", 0, ZR_FALSE);
    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, mainUri, position, ZR_TRUE, &references) ||
        !locations_contain_uri(&references, callbacksUri) ||
        !locations_contain_uri(&references, zr_string_or_null(mainUri))) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        free(mainContent);
        TEST_FAIL(timer,
                  "LSP Project Cross File References",
                  "Expected cross-file references to include both declaration and imported call site");
        return;
    }

    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    free(mainContent);
    TEST_PASS(timer, "LSP Project Cross File References");
}

static void test_lsp_project_member_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray completions;
    SZrLspPosition position;
    char pathBuffer[2048];
    const char *content =
        "var callbacks = %import(\"callbacks\");\n"
        "callbacks.\n";

    TEST_START("LSP Project Member Completion");
    TEST_INFO("Project member completion", "Completing after an imported project module alias should include exported sibling symbols");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Project Member Completion", "Failed to create LSP context");
        return;
    }

    build_source_path(pathBuffer,
                      sizeof(pathBuffer),
                      "tests/fixtures/projects/native_numeric_pipeline/src/main.zr");
    if (!update_document_with_content(state, context, pathBuffer, content, &uri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Project Member Completion", "Failed to update project-backed probe document");
        return;
    }

    position = position_for_nth_substring(content, "callbacks.", 0, ZR_TRUE);
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, position, &completions) ||
        find_completion_item(&completions, "runCallbacks") == ZR_NULL ||
        find_completion_item(&completions, "summarizeCallback") == ZR_NULL) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Member Completion",
                  "Expected member completion to include callbacks module exports");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Project Member Completion");
}

static void test_lsp_builtin_virtual_definition(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrLspPosition position;
    SZrArray definitions;
    const char *content =
        "var system = %import(\"zr.system\");\n"
        "system.gc.start();\n";
    const char *builtinUri = "zr://builtin/zr.system.gc.zr";

    TEST_START("LSP Builtin Virtual Definition");
    TEST_INFO("Builtin virtual definition", "Getting definition for builtin module members should land on zr:// builtin virtual documents");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Builtin Virtual Definition", "Failed to create LSP context");
        return;
    }

    if (!update_document_with_content(state,
                                      context,
                                      ZR_VM_SOURCE_ROOT "/tests/fixtures/tmp_builtin_probe.zr",
                                      content,
                                      &uri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Builtin Virtual Definition", "Failed to update builtin probe document");
        return;
    }

    position = position_for_nth_substring(content, "start", 0, ZR_FALSE);
    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, &definitions) ||
        !locations_contain_uri(&definitions, builtinUri)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Builtin Virtual Definition",
                  "Expected builtin definition URI zr://builtin/zr.system.gc.zr");
        return;
    }

    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Builtin Virtual Definition");
}

static void test_lsp_directive_completion(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray completions;
    SZrLspPosition position;
    SZrLspCompletionItem *moduleItem;
    const char *content = "%";

    TEST_START("LSP Directive Completion");
    TEST_INFO("Directive completion", "Typing % should surface parser directives with snippet-style insert text");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Directive Completion", "Failed to create LSP context");
        return;
    }

    if (!update_document_with_content(state,
                                      context,
                                      ZR_VM_SOURCE_ROOT "/tests/fixtures/tmp_directive_probe.zr",
                                      content,
                                      &uri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Directive Completion", "Failed to update directive probe document");
        return;
    }

    position = position_for_nth_substring(content, "%", 0, ZR_TRUE);
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, position, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Directive Completion", "Failed to get directive completions");
        return;
    }

    moduleItem = find_completion_item(&completions, "%module");
    if (moduleItem == ZR_NULL ||
        find_completion_item(&completions, "%import") == ZR_NULL ||
        find_completion_item(&completions, "%compileTime") == ZR_NULL ||
        find_completion_item(&completions, "%extern") == ZR_NULL ||
        find_completion_item(&completions, "%test") == ZR_NULL ||
        moduleItem->insertTextFormat == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeString(moduleItem->insertTextFormat), "snippet") != 0) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Directive Completion",
                  "Expected directive completion items with snippet insert text");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Directive Completion");
}

static void test_lsp_auto_import_completion_and_code_action(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray completions;
    SZrArray actions;
    SZrLspPosition position;
    SZrLspRange range;
    SZrLspCompletionItem *systemItem;
    SZrLspCodeAction *codeAction;
    const char *content = "system.gc.start();\n";

    TEST_START("LSP Auto Import Completion And Code Action");
    TEST_INFO("Auto import", "Unresolved builtin chains should offer completion and code action that insert var system = %import(\"zr.system\");");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Auto Import Completion And Code Action", "Failed to create LSP context");
        return;
    }

    if (!update_document_with_content(state,
                                      context,
                                      ZR_VM_SOURCE_ROOT "/tests/fixtures/tmp_auto_import_probe.zr",
                                      content,
                                      &uri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Import Completion And Code Action", "Failed to update auto-import probe document");
        return;
    }

    position = position_for_nth_substring(content, "system", 0, ZR_TRUE);
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, position, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Import Completion And Code Action", "Failed to get auto-import completions");
        return;
    }

    systemItem = find_completion_item(&completions, "system");
    if (systemItem == ZR_NULL ||
        systemItem->sourceType == ZR_NULL ||
        strcmp(ZrCore_String_GetNativeString(systemItem->sourceType), "auto-import") != 0 ||
        !systemItem->additionalTextEdits.isValid ||
        systemItem->additionalTextEdits.length == 0) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Import Completion And Code Action",
                  "Expected auto-import completion with sourceType=auto-import and additional edits");
        return;
    }

    range.start = position_for_nth_substring(content, "system", 0, ZR_FALSE);
    range.end = position_for_nth_substring(content, "system", 0, ZR_TRUE);
    ZrCore_Array_Init(state, &actions, sizeof(SZrLspCodeAction *), 4);
    if (!ZrLanguageServer_Lsp_GetCodeActions(state, context, uri, range, &actions)) {
        ZrCore_Array_Free(state, &actions);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer, "LSP Auto Import Completion And Code Action", "Failed to get code actions");
        return;
    }

    codeAction = find_code_action_with_title_fragment(&actions, "zr.system");
    if (codeAction == ZR_NULL ||
        !codeAction->edits.isValid ||
        codeAction->edits.length == 0) {
        ZrCore_Array_Free(state, &actions);
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Auto Import Completion And Code Action",
                  "Expected auto-import code action for zr.system");
        return;
    }

    ZrCore_Array_Free(state, &actions);
    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Auto Import Completion And Code Action");
}

static void test_lsp_single_file_local_symbol_completion_and_hover(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray completions;
    SZrLspPosition completionPosition;
    SZrLspPosition hoverPosition;
    SZrLspCompletionItem *localItem;
    SZrLspHover *hover = ZR_NULL;
    const char *hoverText;
    const char *content =
        "helper(seed: float) {\n"
        "    var localValue: float = seed + 1.0;\n"
        "    return localValue;\n"
        "}\n";

    TEST_START("LSP Single File Local Symbol Completion And Hover");
    TEST_INFO("Single-file local symbol detail",
              "Single-file fallback should surface local scope completions and hover text with type/access detail");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Single File Local Symbol Completion And Hover", "Failed to create LSP context");
        return;
    }

    if (!update_document_with_content(state,
                                      context,
                                      ZR_VM_SOURCE_ROOT "/tests/fixtures/tmp_single_file_local_symbol_probe.zr",
                                      content,
                                      &uri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Completion And Hover",
                  "Failed to update single-file local symbol probe document");
        return;
    }

    completionPosition = position_for_nth_substring(content, "return", 0, ZR_FALSE);
    ZrCore_Array_Init(state, &completions, sizeof(SZrLspCompletionItem *), 8);
    if (!ZrLanguageServer_Lsp_GetCompletion(state, context, uri, completionPosition, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Completion And Hover",
                  "Failed to get single-file local completions");
        return;
    }

    localItem = find_completion_item(&completions, "localValue");
    if (localItem == ZR_NULL ||
        lsp_completion_detail_string(localItem) == ZR_NULL ||
        strstr(lsp_completion_detail_string(localItem), "float") == ZR_NULL ||
        strstr(lsp_completion_detail_string(localItem), "private") == ZR_NULL ||
        find_completion_item(&completions, "seed") == ZR_NULL) {
        char detailMessage[512];
        snprintf(detailMessage,
                 sizeof(detailMessage),
                 "seed=%s | localValue=%s",
                 lsp_completion_detail_for_label(&completions, "seed") != ZR_NULL
                    ? lsp_completion_detail_for_label(&completions, "seed")
                    : "<null>",
                 lsp_completion_detail_for_label(&completions, "localValue") != ZR_NULL
                    ? lsp_completion_detail_for_label(&completions, "localValue")
                    : "<null>");
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Completion And Hover",
                  detailMessage);
        return;
    }

    hoverPosition = position_for_nth_substring(content, "localValue", 1, ZR_FALSE);
    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, hoverPosition, &hover) ||
        hover == ZR_NULL) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Completion And Hover",
                  "Failed to get single-file local hover");
        return;
    }

    hoverText = lsp_hover_first_content(hover);
    if (hoverText == ZR_NULL ||
        strstr(hoverText, "localValue") == ZR_NULL ||
        strstr(hoverText, "float") == ZR_NULL ||
        strstr(hoverText, "private") == ZR_NULL) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Completion And Hover",
                  hoverText != ZR_NULL ? hoverText : "<null hover>");
        return;
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Single File Local Symbol Completion And Hover");
}

static void test_lsp_single_file_local_symbol_definition_and_references(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *uri = ZR_NULL;
    SZrArray definitions;
    SZrArray references;
    SZrLspPosition position;
    const char *content =
        "helper(seed: float) {\n"
        "    var localValue: float = seed + 1.0;\n"
        "    return localValue;\n"
        "}\n";
    const char *uriText;

    TEST_START("LSP Single File Local Symbol Definition And References");
    TEST_INFO("Single-file local symbol definition/reference",
              "Single-file fallback should resolve local symbol definitions and references in multi-line documents");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer, "LSP Single File Local Symbol Definition And References", "Failed to create LSP context");
        return;
    }

    if (!update_document_with_content(state,
                                      context,
                                      ZR_VM_SOURCE_ROOT "/tests/fixtures/tmp_single_file_local_symbol_refs_probe.zr",
                                      content,
                                      &uri)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Definition And References",
                  "Failed to update single-file local symbol refs probe document");
        return;
    }

    uriText = zr_string_or_null(uri);
    position = position_for_nth_substring(content, "localValue", 1, ZR_FALSE);

    ZrCore_Array_Init(state, &definitions, sizeof(SZrLspLocation *), 4);
    if (!ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, &definitions) ||
        !locations_contain_uri_line(&definitions, uriText, 2)) {
        ZrCore_Array_Free(state, &definitions);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Definition And References",
                  "Expected local symbol definition to resolve to the declaration line");
        return;
    }
    ZrCore_Array_Free(state, &definitions);

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    if (!ZrLanguageServer_Lsp_FindReferences(state, context, uri, position, ZR_TRUE, &references) ||
        !locations_contain_uri_line(&references, uriText, 2) ||
        !locations_contain_uri_line(&references, uriText, 3)) {
        char lines[256];
        describe_location_lines(&references, lines, sizeof(lines));
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Single File Local Symbol Definition And References",
                  lines[0] != '\0' ? lines : "Expected local symbol references to include both declaration and use lines");
        return;
    }

    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    TEST_PASS(timer, "LSP Single File Local Symbol Definition And References");
}

static void test_lsp_project_diagnostics_avoid_native_numeric_pipeline_false_type_mismatch(SZrState *state) {
    SZrTestTimer timer;
    SZrLspContext *context;
    SZrString *systemUri = ZR_NULL;
    SZrString *linAlgUri = ZR_NULL;
    char *systemContent = ZR_NULL;
    char *linAlgContent = ZR_NULL;
    SZrArray diagnostics;
    char detail[1024];

    TEST_START("LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch");
    TEST_INFO("Project diagnostics regression",
              "Getting diagnostics for native_numeric_pipeline sources should not emit the historical type_mismatch false positives in system_io.zr or lin_alg.zr");

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch",
                  "Failed to create LSP context");
        return;
    }

    if (!update_document_from_relative_file(state,
                                            context,
                                            "tests/fixtures/projects/native_numeric_pipeline/src/system_io.zr",
                                            &systemUri,
                                            &systemContent)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch",
                  "Failed to load native_numeric_pipeline system_io.zr fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 8);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, systemUri, &diagnostics) ||
        lsp_diagnostics_contain_code(&diagnostics, "type_mismatch")) {
        describe_lsp_diagnostics(&diagnostics, detail, sizeof(detail));
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        free(systemContent);
        free(linAlgContent);
        TEST_FAIL(timer,
                  "LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch",
                  detail[0] != '\0' ? detail : "system_io.zr still emitted type_mismatch diagnostics");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);
    ZrLanguageServer_LspContext_Free(state, context);
    free(systemContent);

    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        TEST_FAIL(timer,
                  "LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch",
                  "Failed to recreate LSP context for lin_alg.zr");
        return;
    }

    if (!update_document_from_relative_file(state,
                                            context,
                                            "tests/fixtures/projects/native_numeric_pipeline/src/lin_alg.zr",
                                            &linAlgUri,
                                            &linAlgContent)) {
        ZrLanguageServer_LspContext_Free(state, context);
        TEST_FAIL(timer,
                  "LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch",
                  "Failed to load native_numeric_pipeline lin_alg.zr fixture");
        return;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrLspDiagnostic *), 8);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(state, context, linAlgUri, &diagnostics) ||
        lsp_diagnostics_contain_code(&diagnostics, "type_mismatch")) {
        describe_lsp_diagnostics(&diagnostics, detail, sizeof(detail));
        ZrCore_Array_Free(state, &diagnostics);
        ZrLanguageServer_LspContext_Free(state, context);
        free(linAlgContent);
        TEST_FAIL(timer,
                  "LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch",
                  detail[0] != '\0' ? detail : "lin_alg.zr still emitted type_mismatch diagnostics");
        return;
    }
    ZrCore_Array_Free(state, &diagnostics);

    ZrLanguageServer_LspContext_Free(state, context);
    free(linAlgContent);
    TEST_PASS(timer, "LSP Project Diagnostics Avoid Native Numeric Pipeline False Type Mismatch");
}

int main() {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

    printf("==========\n");
    printf("Language Server - LSP Interface Tests\n");
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
    ZrVmLibMath_Register(global);
    ZrVmLibSystem_Register(global);

    test_lsp_context_create_and_free(state);
    TEST_DIVIDER();
    test_lsp_project_cross_file_definition(state);
    TEST_DIVIDER();
    test_lsp_project_cross_file_references(state);
    TEST_DIVIDER();
    test_lsp_project_member_completion(state);
    TEST_DIVIDER();
    test_lsp_builtin_virtual_definition(state);
    TEST_DIVIDER();
    test_lsp_directive_completion(state);
    TEST_DIVIDER();
    test_lsp_auto_import_completion_and_code_action(state);
    TEST_DIVIDER();
    test_lsp_single_file_local_symbol_completion_and_hover(state);
    TEST_DIVIDER();
    test_lsp_single_file_local_symbol_definition_and_references(state);
    TEST_DIVIDER();
    test_lsp_project_diagnostics_avoid_native_numeric_pipeline_false_type_mismatch(state);
    TEST_DIVIDER();

    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    printf("All LSP Interface Tests Completed\n");
    printf("==========\n");
    return 0;
}
