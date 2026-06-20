//
// Focused project/navigation LSP range regressions for UTF-16 columns.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "path_support.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"
#include "zr_vm_language_server.h"
#include "semantic/lsp_semantic_query.h"

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    return realloc(pointer, newSize);
}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *content) {
    FILE *file;
    TZrSize length;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL || !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(content);
    written = fwrite(content, 1, length, file);
    fclose(file);
    return written == length;
}

static TZrChar *find_last_path_separator(TZrChar *path) {
    TZrChar *forwardSlash;
    TZrChar *backSlash;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    forwardSlash = strrchr(path, '/');
    backSlash = strrchr(path, '\\');
    if (forwardSlash == ZR_NULL) {
        return backSlash;
    }
    if (backSlash == ZR_NULL) {
        return forwardSlash;
    }

    return forwardSlash > backSlash ? forwardSlash : backSlash;
}

static SZrString *create_file_uri_from_native_path(SZrState *state, const TZrChar *path) {
    TZrChar buffer[ZR_TESTS_PATH_MAX * 2];
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

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
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
        SZrLspLocation *location = locationPtr != ZR_NULL ? *locationPtr : ZR_NULL;
        if (location != ZR_NULL &&
            location->uri != ZR_NULL &&
            strcmp(test_string_text(location->uri), test_string_text(uri)) == 0 &&
            location->range.start.line == startLine &&
            location->range.start.character == startCharacter &&
            location->range.end.line == endLine &&
            location->range.end.character == endCharacter) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void describe_first_location(SZrArray *locations) {
    if (locations != ZR_NULL && locations->length > 0) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, 0);
        SZrLspLocation *location = locationPtr != ZR_NULL ? *locationPtr : ZR_NULL;
        if (location != ZR_NULL) {
            printf(" first=%d:%d-%d:%d",
                   location->range.start.line,
                   location->range.start.character,
                   location->range.end.line,
                   location->range.end.character);
        }
    }
}

static TZrBool prepare_utf16_project(TZrChar *mainPath,
                                     TZrSize mainPathSize,
                                     TZrChar *modulePath,
                                     TZrSize modulePathSize) {
    static const TZrChar *projectContent =
        "{\n"
        "  \"name\": \"utf16_project_ranges\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"main\"\n"
        "}\n";
    static const TZrChar *mainContent =
        "/* \xCE\xBB */ var greetModule = %import(\"greet\");\n"
        "/* \xCE\xBB */ return greetModule.greet();\n";
    static const TZrChar *moduleContent =
        "pub var greet = () => {\n"
        "    return \"hello\";\n"
        "};\n";
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (mainPath == ZR_NULL || modulePath == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("language_server",
                                           "project_utf16_ranges",
                                           "utf16_project_ranges",
                                           ".zrp",
                                           projectPath,
                                           sizeof(projectPath))) {
        return ZR_FALSE;
    }

    snprintf(rootPath, sizeof(rootPath), "%s", projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    snprintf(sourceRootPath, sizeof(sourceRootPath), "%s/src", rootPath);
    snprintf(mainPath, mainPathSize, "%s/main.zr", sourceRootPath);
    snprintf(modulePath, modulePathSize, "%s/greet.zr", sourceRootPath);

    return write_text_file(projectPath, projectContent) &&
           write_text_file(mainPath, mainContent) &&
           write_text_file(modulePath, moduleContent);
}

static TZrBool write_binary_metadata_file(SZrState *state,
                                          const TZrChar *binaryPath,
                                          const TZrChar *moduleSource) {
    SZrString *sourceName;
    SZrFunction *function;
    SZrBinaryWriterOptions options;
    TZrChar moduleNameBuffer[ZR_TESTS_PATH_MAX];
    const TZrChar *fileName;
    const TZrChar *extension;
    TZrSize moduleNameLength;
    TZrBool success;

    if (state == ZR_NULL || binaryPath == ZR_NULL || moduleSource == ZR_NULL ||
        !ZrTests_Path_EnsureParentDirectory(binaryPath)) {
        return ZR_FALSE;
    }

    fileName = strrchr(binaryPath, '/');
    if (fileName == ZR_NULL) {
        fileName = strrchr(binaryPath, '\\');
    }
    fileName = fileName != ZR_NULL ? fileName + 1 : binaryPath;
    extension = strrchr(fileName, '.');
    moduleNameLength = extension != ZR_NULL ? (TZrSize)(extension - fileName) : strlen(fileName);
    if (moduleNameLength == 0 || moduleNameLength >= sizeof(moduleNameBuffer)) {
        return ZR_FALSE;
    }

    memcpy(moduleNameBuffer, fileName, moduleNameLength);
    moduleNameBuffer[moduleNameLength] = '\0';

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleNameBuffer;

    sourceName = ZrCore_String_Create(state, (TZrNativeString)binaryPath, strlen(binaryPath));
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }

    function = ZrParser_Source_Compile(state, (TZrNativeString)moduleSource, strlen(moduleSource), sourceName);
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    success = ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath, &options);
    ZrCore_Function_Free(state, function);
    return success;
}

static TZrBool prepare_binary_metadata_utf16_project(SZrState *state,
                                                     TZrChar *projectPath,
                                                     TZrSize projectPathSize,
                                                     TZrChar *binaryPath,
                                                     TZrSize binaryPathSize) {
    static const TZrChar *projectContent =
        "{\n"
        "  \"name\": \"binary_utf16_ranges\",\n"
        "  \"source\": \"src\",\n"
        "  \"binary\": \"bin\",\n"
        "  \"entry\": \"main\"\n"
        "}\n";
    static const TZrChar *mainContent =
        "var binaryStage = %import(\"binary_utf16_stage\");\n"
        "return binaryStage.binarySeed();\n";
    static const TZrChar *binarySource =
        "/* \xCE\xBB */ pub var binarySeed = () => {\n"
        "    return 40;\n"
        "};\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRootPath[ZR_TESTS_PATH_MAX];
    TZrChar binaryRootPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (state == ZR_NULL || projectPath == ZR_NULL || binaryPath == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("language_server",
                                           "project_binary_utf16_ranges",
                                           "binary_utf16_ranges",
                                           ".zrp",
                                           projectPath,
                                           projectPathSize)) {
        return ZR_FALSE;
    }

    snprintf(rootPath, sizeof(rootPath), "%s", projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    snprintf(sourceRootPath, sizeof(sourceRootPath), "%s/src", rootPath);
    snprintf(binaryRootPath, sizeof(binaryRootPath), "%s/bin", rootPath);
    snprintf(mainPath, sizeof(mainPath), "%s/main.zr", sourceRootPath);
    snprintf(binaryPath, binaryPathSize, "%s/binary_utf16_stage.zro", binaryRootPath);

    return write_text_file(projectPath, projectContent) &&
           write_text_file(mainPath, mainContent) &&
           write_binary_metadata_file(state, binaryPath, binarySource);
}

static TZrBool test_module_entry_references_after_utf8_prefix_use_utf16_columns(SZrState *state) {
    static const TZrChar *mainContent =
        "/* \xCE\xBB */ var greetModule = %import(\"greet\");\n"
        "/* \xCE\xBB */ return greetModule.greet();\n";
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar modulePath[ZR_TESTS_PATH_MAX];
    SZrLspContext *context;
    SZrString *mainUri;
    SZrString *moduleUri;
    SZrArray references = {0};
    TZrBool resolved;

    if (!prepare_utf16_project(mainPath, sizeof(mainPath), modulePath, sizeof(modulePath))) {
        printf("FAIL: Project UTF-16 module entry references could not prepare fixture\n");
        return ZR_FALSE;
    }

    context = ZrLanguageServer_LspContext_New(state);
    mainUri = create_file_uri_from_native_path(state, mainPath);
    moduleUri = create_file_uri_from_native_path(state, modulePath);
    if (context == ZR_NULL || mainUri == ZR_NULL || moduleUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, mainUri, mainContent, strlen(mainContent), 1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: Project UTF-16 module entry references could not open fixture\n");
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 8);
    resolved = ZrLanguageServer_Lsp_FindReferences(state,
                                                   context,
                                                   moduleUri,
                                                   (SZrLspPosition){0, 0},
                                                   ZR_TRUE,
                                                   &references);
    if (!resolved ||
        !location_array_contains_uri_and_range(&references, moduleUri, 0, 0, 0, 0) ||
        !location_array_contains_uri_and_range(&references, mainUri, 0, 35, 0, 40) ||
        !location_array_contains_uri_and_range(&references, mainUri, 0, 12, 0, 23) ||
        !location_array_contains_uri_and_range(&references, mainUri, 1, 15, 1, 26) ||
        !location_array_contains_uri_and_range(&references, mainUri, 1, 27, 1, 32)) {
        printf("FAIL: Project UTF-16 module entry references expected import/member ranges 0:35-0:40, 0:12-0:23, 1:15-1:26, 1:27-1:32 but got resolved=%d count=%llu",
               (int)resolved,
               (unsigned long long)references.length);
        describe_first_location(&references);
        printf("\n");
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_FALSE;
    }

    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_LspContext_Free(state, context);
    printf("PASS: Project UTF-16 module entry references use UTF-16 columns\n");
    return ZR_TRUE;
}

static TZrBool test_binary_metadata_declaration_after_utf8_prefix_uses_utf16_columns(SZrState *state) {
    static const TZrChar *binarySource =
        "/* \xCE\xBB */ pub var binarySeed = () => {\n"
        "    return 40;\n"
        "};\n";
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar binaryPath[ZR_TESTS_PATH_MAX];
    SZrLspContext *context;
    SZrString *projectUri;
    SZrString *binaryUri;
    SZrLspSemanticQuery query;
    TZrBool resolved;

    if (!prepare_binary_metadata_utf16_project(state,
                                               projectPath,
                                               sizeof(projectPath),
                                               binaryPath,
                                               sizeof(binaryPath))) {
        printf("FAIL: Project UTF-16 binary metadata declaration could not prepare fixture\n");
        return ZR_FALSE;
    }

    context = ZrLanguageServer_LspContext_New(state);
    projectUri = create_file_uri_from_native_path(state, projectPath);
    binaryUri = create_file_uri_from_native_path(state, binaryPath);
    if (context == ZR_NULL || projectUri == ZR_NULL || binaryUri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: Project UTF-16 binary metadata declaration could not open fixture\n");
        return ZR_FALSE;
    }
    ZrLanguageServer_LspContext_SetClientSelectedZrpUri(state, context, projectUri);
    if (!ZrLanguageServer_IncrementalParser_UpdateFile(state,
                                                       context->parser,
                                                       binaryUri,
                                                       binarySource,
                                                       strlen(binarySource),
                                                       1)) {
        ZrLanguageServer_LspContext_Free(state, context);
        printf("FAIL: Project UTF-16 binary metadata declaration could not open fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&query);
    resolved = ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state,
                                                                   context,
                                                                   binaryUri,
                                                                   (SZrLspPosition){0, 16},
                                                                   &query);
    if (!resolved ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION ||
        query.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA ||
        query.memberName == ZR_NULL ||
        strcmp(test_string_text(query.memberName), "binarySeed") != 0 ||
        !query.resolvedMember.hasDeclaration ||
        query.resolvedMember.declarationUri == ZR_NULL ||
        strcmp(test_string_text(query.resolvedMember.declarationUri), test_string_text(binaryUri)) != 0) {
        printf("FAIL: Project UTF-16 binary metadata declaration expected binarySeed member but got resolved=%d kind=%d member=%s",
               (int)resolved,
               (int)query.kind,
               query.memberName != ZR_NULL ? test_string_text(query.memberName) : "<null>");
        printf("\n");
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_FALSE;
    }

    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    ZrLanguageServer_LspContext_Free(state, context);
    printf("PASS: Project UTF-16 binary metadata declaration uses UTF-16 columns\n");
    return ZR_TRUE;
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    TZrBool passed;

    printf("==========\n");
    printf("Language Server - Project UTF-16 Range Tests\n");
    printf("==========\n\n");

    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: Project UTF-16 range tests could not create VM state\n");
        return 1;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    passed = test_module_entry_references_after_utf8_prefix_use_utf16_columns(global->mainThreadState);
    passed = test_binary_metadata_declaration_after_utf8_prefix_uses_utf16_columns(global->mainThreadState) && passed;
    ZrCore_GlobalState_Free(global);

    printf("\n==========\n");
    if (passed) {
        printf("All Project UTF-16 Range Tests Completed\n");
        printf("==========\n");
        return 0;
    }

    printf("Project UTF-16 Range Test Failed\n");
    printf("==========\n");
    return 1;
}
