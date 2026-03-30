//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/lsp_interface.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define ZR_LSP_MAX_PATH 4096
#define ZR_LSP_MAX_IDENT 128
#define ZR_LSP_MAX_CHAIN_PARTS 16

typedef struct SZrLspImportAlias {
    SZrString *alias;
    SZrString *modulePath;
    SZrFileRange aliasRange;
} SZrLspImportAlias;

typedef struct SZrLspProjectExport {
    SZrString *name;
    SZrString *kind;
    SZrString *detail;
    SZrString *documentation;
    SZrFileRange location;
} SZrLspProjectExport;

typedef struct SZrLspProjectFile {
    SZrString *uri;
    SZrString *path;
    SZrString *moduleName;
    SZrArray imports;     // SZrLspImportAlias*
    SZrArray exports;     // SZrLspProjectExport*
} SZrLspProjectFile;

struct SZrLspProjectIndex {
    SZrString *configUri;
    SZrString *configPath;
    SZrString *projectRootPath;
    SZrString *sourceRootPath;
    SZrArray files;       // SZrLspProjectFile*
};

typedef struct SZrLspBuiltinSymbol {
    SZrString *name;
    SZrString *kind;
    SZrString *detail;
    SZrString *documentation;
    SZrString *targetModule;
    SZrFileRange location;
    TZrBool isModuleLink;
} SZrLspBuiltinSymbol;

struct SZrLspBuiltinDocument {
    const ZrLibModuleDescriptor *descriptor;
    SZrString *moduleName;
    SZrString *uri;
    SZrString *content;
    SZrArray symbols;     // SZrLspBuiltinSymbol*
};

typedef enum EZrLspResolvedKind {
    ZR_LSP_RESOLVED_NONE = 0,
    ZR_LSP_RESOLVED_PROJECT_EXPORT = 1,
    ZR_LSP_RESOLVED_BUILTIN_SYMBOL = 2,
    ZR_LSP_RESOLVED_BUILTIN_MODULE = 3
} EZrLspResolvedKind;

typedef struct SZrLspResolvedSymbol {
    EZrLspResolvedKind kind;
    SZrLspProjectIndex *project;
    SZrLspProjectFile *projectFile;
    SZrLspProjectExport *projectExport;
    SZrLspBuiltinDocument *builtinDocument;
    SZrLspBuiltinSymbol *builtinSymbol;
    char moduleName[ZR_LSP_MAX_PATH];
    char exportName[ZR_LSP_MAX_IDENT];
} SZrLspResolvedSymbol;

typedef struct SZrLspChainQuery {
    TZrBool isValid;
    TZrSize partCount;
    TZrSize focusedPartIndex;
    TZrSize tokenStart;
    TZrSize tokenEnd;
    TZrSize chainStart;
    TZrSize chainEnd;
    char parts[ZR_LSP_MAX_CHAIN_PARTS][ZR_LSP_MAX_IDENT];
} SZrLspChainQuery;

static TZrBool lsp_find_import_module_in_content(const char *content,
                                                 TZrSize contentLength,
                                                 const char *alias,
                                                 char *moduleBuffer,
                                                 size_t moduleBufferSize);

static const char *lsp_string_or_null(SZrString *string) {
    return string != ZR_NULL ? ZrCore_String_GetNativeString(string) : ZR_NULL;
}

static TZrBool lsp_string_equals(SZrString *string, const char *value) {
    const char *nativeString = lsp_string_or_null(string);
    return nativeString != ZR_NULL && value != ZR_NULL && strcmp(nativeString, value) == 0;
}

static void lsp_array_reset(SZrArray *array) {
    if (array == ZR_NULL) {
        return;
    }
    array->head = ZR_NULL;
    array->elementSize = 0;
    array->length = 0;
    array->capacity = 0;
    array->isValid = ZR_FALSE;
}

static TZrBool lsp_is_identifier_start(TZrInt32 c) {
    return isalpha(c) || c == '_';
}

static TZrBool lsp_is_identifier_char(TZrInt32 c) {
    return isalnum(c) || c == '_';
}

// 转换 FileRange 到 LspRange
SZrLspRange ZrLanguageServer_LspRange_FromFileRange(SZrFileRange fileRange) {
    SZrLspRange lspRange;
    lspRange.start.line = fileRange.start.line;
    lspRange.start.character = fileRange.start.column;
    lspRange.end.line = fileRange.end.line;
    lspRange.end.character = fileRange.end.column;
    return lspRange;
}

static TZrSize calculate_offset_from_line_column(const TZrChar *content,
                                                 TZrSize contentLength,
                                                 TZrInt32 line,
                                                 TZrInt32 column) {
    TZrSize offset = 0;
    TZrInt32 currentLine = 0;
    TZrInt32 currentColumn = 0;
    TZrSize index;

    if (content == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < contentLength && currentLine < line; index++) {
        if (content[index] == '\n') {
            currentLine++;
            currentColumn = 0;
            offset = index + 1;
        } else {
            currentColumn++;
        }
    }

    if (currentLine == line) {
        for (index = offset; index < contentLength && currentColumn < column; index++) {
            if (content[index] == '\n') {
                break;
            }
            currentColumn++;
            offset = index + 1;
        }
    }

    return offset;
}

// 转换 LspRange 到 FileRange
SZrFileRange ZrLanguageServer_LspRange_ToFileRange(SZrLspRange lspRange, SZrString *uri) {
    SZrFileRange fileRange;
    fileRange.start.line = lspRange.start.line;
    fileRange.start.column = lspRange.start.character;
    fileRange.start.offset = 0;
    fileRange.end.line = lspRange.end.line;
    fileRange.end.column = lspRange.end.character;
    fileRange.end.offset = 0;
    fileRange.source = uri;
    return fileRange;
}

// 转换 LspRange 到 FileRange（带文件内容）
SZrFileRange ZrLanguageServer_LspRange_ToFileRangeWithContent(SZrLspRange lspRange,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength) {
    SZrFileRange fileRange;
    fileRange.start.line = lspRange.start.line;
    fileRange.start.column = lspRange.start.character;
    fileRange.start.offset = calculate_offset_from_line_column(content,
                                                               contentLength,
                                                               lspRange.start.line,
                                                               lspRange.start.character);
    fileRange.end.line = lspRange.end.line;
    fileRange.end.column = lspRange.end.character;
    fileRange.end.offset = calculate_offset_from_line_column(content,
                                                             contentLength,
                                                             lspRange.end.line,
                                                             lspRange.end.character);
    fileRange.source = uri;
    return fileRange;
}

// 转换 FilePosition 到 LspPosition
SZrLspPosition ZrLanguageServer_LspPosition_FromFilePosition(SZrFilePosition filePosition) {
    SZrLspPosition lspPosition;
    lspPosition.line = filePosition.line;
    lspPosition.character = filePosition.column;
    return lspPosition;
}

// 转换 LspPosition 到 FilePosition
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePosition(SZrLspPosition lspPosition) {
    SZrFilePosition filePosition;
    filePosition.line = lspPosition.line;
    filePosition.column = lspPosition.character;
    filePosition.offset = 0;
    return filePosition;
}

// 转换 LspPosition 到 FilePosition（带文件内容）
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePositionWithContent(SZrLspPosition lspPosition,
                                                                       const TZrChar *content,
                                                                       TZrSize contentLength) {
    SZrFilePosition filePosition;
    filePosition.line = lspPosition.line;
    filePosition.column = lspPosition.character;
    filePosition.offset = calculate_offset_from_line_column(content,
                                                            contentLength,
                                                            lspPosition.line,
                                                            lspPosition.character);
    return filePosition;
}

static void lsp_free_project_file(SZrState *state, SZrLspProjectFile *file) {
    TZrSize index;

    if (state == ZR_NULL || file == ZR_NULL) {
        return;
    }

    if (file->imports.isValid) {
        for (index = 0; index < file->imports.length; index++) {
            SZrLspImportAlias **aliasPtr = (SZrLspImportAlias **)ZrCore_Array_Get(&file->imports, index);
            if (aliasPtr != ZR_NULL && *aliasPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, *aliasPtr, sizeof(SZrLspImportAlias));
            }
        }
        ZrCore_Array_Free(state, &file->imports);
    }

    if (file->exports.isValid) {
        for (index = 0; index < file->exports.length; index++) {
            SZrLspProjectExport **exportPtr = (SZrLspProjectExport **)ZrCore_Array_Get(&file->exports, index);
            if (exportPtr != ZR_NULL && *exportPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, *exportPtr, sizeof(SZrLspProjectExport));
            }
        }
        ZrCore_Array_Free(state, &file->exports);
    }

    ZrCore_Memory_RawFree(state->global, file, sizeof(SZrLspProjectFile));
}

static void lsp_free_project_index(SZrState *state, SZrLspProjectIndex *projectIndex) {
    TZrSize index;

    if (state == ZR_NULL || projectIndex == ZR_NULL) {
        return;
    }

    if (projectIndex->files.isValid) {
        for (index = 0; index < projectIndex->files.length; index++) {
            SZrLspProjectFile **filePtr = (SZrLspProjectFile **)ZrCore_Array_Get(&projectIndex->files, index);
            if (filePtr != ZR_NULL && *filePtr != ZR_NULL) {
                lsp_free_project_file(state, *filePtr);
            }
        }
        ZrCore_Array_Free(state, &projectIndex->files);
    }

    ZrCore_Memory_RawFree(state->global, projectIndex, sizeof(SZrLspProjectIndex));
}

static void lsp_free_builtin_document(SZrState *state, SZrLspBuiltinDocument *document) {
    TZrSize index;

    if (state == ZR_NULL || document == ZR_NULL) {
        return;
    }

    if (document->symbols.isValid) {
        for (index = 0; index < document->symbols.length; index++) {
            SZrLspBuiltinSymbol **symbolPtr = (SZrLspBuiltinSymbol **)ZrCore_Array_Get(&document->symbols, index);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, *symbolPtr, sizeof(SZrLspBuiltinSymbol));
            }
        }
        ZrCore_Array_Free(state, &document->symbols);
    }

    ZrCore_Memory_RawFree(state->global, document, sizeof(SZrLspBuiltinDocument));
}

static SZrSemanticAnalyzer *get_or_create_analyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;
    SZrSemanticAnalyzer *analyzer;
    SZrTypeValue value;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        return (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
    }

    analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    pair = ZrCore_HashSet_Add(state, &context->uriToAnalyzerMap, &key);
    if (pair == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
        return ZR_NULL;
    }

    ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)analyzer);
    ZrCore_Value_Copy(state, &pair->value, &value);
    return analyzer;
}

// 创建 LSP 上下文
SZrLspContext *ZrLanguageServer_LspContext_New(SZrState *state) {
    SZrLspContext *context;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    context = (SZrLspContext *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspContext));
    if (context == ZR_NULL) {
        return ZR_NULL;
    }
    memset(context, 0, sizeof(SZrLspContext));

    context->state = state;
    context->parser = ZrLanguageServer_IncrementalParser_New(state);
    context->analyzer = ZR_NULL;
    ZrCore_HashSet_Init(state, &context->uriToAnalyzerMap, 4);
    ZrCore_Array_Init(state, &context->projectIndexes, sizeof(SZrLspProjectIndex *), 2);
    ZrCore_Array_Init(state, &context->builtinDocuments, sizeof(SZrLspBuiltinDocument *), 8);

    if (context->parser == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_NULL;
    }

    return context;
}

// 释放 LSP 上下文
void ZrLanguageServer_LspContext_Free(SZrState *state, SZrLspContext *context) {
    TZrSize bucketIndex;
    TZrSize arrayIndex;

    if (state == ZR_NULL || context == ZR_NULL) {
        return;
    }

    if (context->projectIndexes.isValid) {
        for (arrayIndex = 0; arrayIndex < context->projectIndexes.length; arrayIndex++) {
            SZrLspProjectIndex **projectPtr = (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes,
                                                                                      arrayIndex);
            if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL) {
                lsp_free_project_index(state, *projectPtr);
            }
        }
        ZrCore_Array_Free(state, &context->projectIndexes);
    }

    if (context->builtinDocuments.isValid) {
        for (arrayIndex = 0; arrayIndex < context->builtinDocuments.length; arrayIndex++) {
            SZrLspBuiltinDocument **documentPtr =
                    (SZrLspBuiltinDocument **)ZrCore_Array_Get(&context->builtinDocuments, arrayIndex);
            if (documentPtr != ZR_NULL && *documentPtr != ZR_NULL) {
                lsp_free_builtin_document(state, *documentPtr);
            }
        }
        ZrCore_Array_Free(state, &context->builtinDocuments);
    }

    if (context->uriToAnalyzerMap.isValid && context->uriToAnalyzerMap.buckets != ZR_NULL) {
        for (bucketIndex = 0; bucketIndex < context->uriToAnalyzerMap.capacity; bucketIndex++) {
            SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[bucketIndex];
            while (pair != ZR_NULL) {
                SZrHashKeyValuePair *next = pair->next;
                if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                    SZrSemanticAnalyzer *analyzer =
                            (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                    if (analyzer != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
                    }
                }
                ZrCore_Memory_RawFreeWithType(state->global,
                                              pair,
                                              sizeof(SZrHashKeyValuePair),
                                              ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            context->uriToAnalyzerMap.buckets[bucketIndex] = ZR_NULL;
        }
        ZrCore_HashSet_Deconstruct(state, &context->uriToAnalyzerMap);
    }

    if (context->parser != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_Free(state, context->parser);
    }
    if (context->analyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, context->analyzer);
    }

    ZrCore_Memory_RawFree(state->global, context, sizeof(SZrLspContext));
}

static SZrFilePosition lsp_offset_to_file_position(const char *content, TZrSize contentLength, TZrSize offset) {
    SZrFilePosition position;
    TZrSize index;

    position.offset = offset;
    position.line = 0;
    position.column = 0;

    if (content == ZR_NULL) {
        return position;
    }

    if (offset > contentLength) {
        offset = contentLength;
    }

    for (index = 0; index < offset; index++) {
        if (content[index] == '\n') {
            position.line++;
            position.column = 0;
        } else {
            position.column++;
        }
    }

    position.offset = offset;
    return position;
}

static SZrFileRange lsp_range_from_offsets(SZrString *uri,
                                           const char *content,
                                           TZrSize contentLength,
                                           TZrSize startOffset,
                                           TZrSize endOffset) {
    SZrFileRange range;
    range.start = lsp_offset_to_file_position(content, contentLength, startOffset);
    range.end = lsp_offset_to_file_position(content, contentLength, endOffset);
    range.source = uri;
    return range;
}

static TZrBool lsp_has_suffix(const char *value, const char *suffix) {
    size_t valueLength;
    size_t suffixLength;

    if (value == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    valueLength = strlen(value);
    suffixLength = strlen(suffix);
    return valueLength >= suffixLength && strcmp(value + valueLength - suffixLength, suffix) == 0;
}

static void lsp_normalize_slashes(char *text) {
    size_t index;

    if (text == ZR_NULL) {
        return;
    }

    for (index = 0; text[index] != '\0'; index++) {
#if defined(_WIN32)
        if (text[index] == '/') {
            text[index] = '\\';
        }
#else
        if (text[index] == '\\') {
            text[index] = '/';
        }
#endif
    }
}

static TZrBool lsp_path_get_directory(const char *path, char *buffer, size_t bufferSize) {
    size_t length;
    size_t index;

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = strlen(path);
    if (length == 0 || length >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, path, length + 1);
    lsp_normalize_slashes(buffer);

    for (index = length; index > 0; index--) {
        if (buffer[index - 1] == '/' || buffer[index - 1] == '\\') {
            if (index == 1) {
                buffer[1] = '\0';
            } else {
                buffer[index - 1] = '\0';
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool lsp_path_join(const char *base, const char *leaf, char *buffer, size_t bufferSize) {
    int written;
    char separator = '/';

    if (base == ZR_NULL || leaf == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

#if defined(_WIN32)
    separator = '\\';
#endif

    if (base[0] == '\0') {
        written = snprintf(buffer, bufferSize, "%s", leaf);
    } else {
        size_t baseLength = strlen(base);
        TZrBool needsSeparator = base[baseLength - 1] != '/' && base[baseLength - 1] != '\\';
        if (needsSeparator) {
            written = snprintf(buffer, bufferSize, "%s%c%s", base, separator, leaf);
        } else {
            written = snprintf(buffer, bufferSize, "%s%s", base, leaf);
        }
    }

    if (written < 0 || (size_t)written >= bufferSize) {
        return ZR_FALSE;
    }

    lsp_normalize_slashes(buffer);
    return ZR_TRUE;
}

static TZrBool lsp_path_to_file_uri(const char *path, char *buffer, size_t bufferSize) {
    size_t srcIndex = 0;
    size_t dstIndex = 0;
    const char *prefix;

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

#if defined(_WIN32)
    prefix = (strlen(path) > 1 && path[1] == ':') ? "file:///" : "file://";
#else
    prefix = "file://";
#endif

    if (strlen(prefix) + strlen(path) + 1 >= bufferSize) {
        return ZR_FALSE;
    }

    strcpy(buffer, prefix);
    dstIndex = strlen(prefix);
    while (path[srcIndex] != '\0' && dstIndex + 1 < bufferSize) {
        buffer[dstIndex++] = path[srcIndex] == '\\' ? '/' : path[srcIndex];
        srcIndex++;
    }
    buffer[dstIndex] = '\0';
    return ZR_TRUE;
}

static TZrBool lsp_file_uri_to_path(const char *uri, char *buffer, size_t bufferSize) {
    const char *cursor;
    size_t index = 0;

    if (uri == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (strncmp(uri, "file://", 7) != 0) {
        return ZR_FALSE;
    }

    cursor = uri + 7;
#if defined(_WIN32)
    if (cursor[0] == '/' && isalpha((unsigned char)cursor[1]) && cursor[2] == ':') {
        cursor++;
    }
#endif

    while (*cursor != '\0' && index + 1 < bufferSize) {
        char current = *cursor++;
#if defined(_WIN32)
        buffer[index++] = current == '/' ? '\\' : current;
#else
        buffer[index++] = current == '\\' ? '/' : current;
#endif
    }
    buffer[index] = '\0';
    return index > 0;
}

static char *lsp_read_text_file(const char *path, size_t *outLength) {
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

static TZrBool lsp_parse_quoted_or_identifier(const char *line,
                                              TZrSize startOffset,
                                              char *buffer,
                                              size_t bufferSize,
                                              TZrSize *outTokenStart,
                                              TZrSize *outTokenEnd) {
    TZrSize cursor = startOffset;
    TZrSize outLength = 0;

    if (line == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    while (line[cursor] == ' ' || line[cursor] == '\t') {
        cursor++;
    }

    if (line[cursor] == '"') {
        TZrSize tokenStart = cursor + 1;
        cursor++;
        while (line[cursor] != '\0' && line[cursor] != '"' && outLength + 1 < bufferSize) {
            buffer[outLength++] = line[cursor++];
        }
        if (line[cursor] != '"') {
            return ZR_FALSE;
        }
        buffer[outLength] = '\0';
        if (outTokenStart != ZR_NULL) {
            *outTokenStart = tokenStart;
        }
        if (outTokenEnd != ZR_NULL) {
            *outTokenEnd = cursor;
        }
        return ZR_TRUE;
    }

    if (!lsp_is_identifier_start((unsigned char)line[cursor])) {
        return ZR_FALSE;
    }

    if (outTokenStart != ZR_NULL) {
        *outTokenStart = cursor;
    }
    while (lsp_is_identifier_char((unsigned char)line[cursor]) && outLength + 1 < bufferSize) {
        buffer[outLength++] = line[cursor++];
    }
    buffer[outLength] = '\0';
    if (outTokenEnd != ZR_NULL) {
        *outTokenEnd = cursor;
    }
    return outLength > 0;
}

static TZrBool lsp_parse_line_import(const char *line,
                                     TZrSize lineOffset,
                                     char *aliasBuffer,
                                     size_t aliasBufferSize,
                                     char *moduleBuffer,
                                     size_t moduleBufferSize,
                                     TZrSize *outAliasStart,
                                     TZrSize *outAliasEnd) {
    const char *cursor = line;
    const char *nameCursor;
    TZrSize aliasStart = 0;
    TZrSize aliasEnd = 0;

    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }

    if (strncmp(cursor, "pub ", 4) == 0) {
        cursor += 4;
    }
    if (strncmp(cursor, "var ", 4) != 0) {
        return ZR_FALSE;
    }
    cursor += 4;
    nameCursor = cursor;

    if (!lsp_parse_quoted_or_identifier(nameCursor, 0, aliasBuffer, aliasBufferSize, &aliasStart, &aliasEnd)) {
        return ZR_FALSE;
    }

    cursor = nameCursor + aliasEnd;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor != '=') {
        return ZR_FALSE;
    }
    cursor++;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (strncmp(cursor, "%import", 7) != 0) {
        return ZR_FALSE;
    }
    cursor += 7;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (*cursor != '(') {
        return ZR_FALSE;
    }
    cursor++;
    if (!lsp_parse_quoted_or_identifier(cursor, 0, moduleBuffer, moduleBufferSize, ZR_NULL, ZR_NULL)) {
        return ZR_FALSE;
    }

    if (outAliasStart != ZR_NULL) {
        *outAliasStart = lineOffset + (TZrSize)(nameCursor - line) + aliasStart;
    }
    if (outAliasEnd != ZR_NULL) {
        *outAliasEnd = lineOffset + (TZrSize)(nameCursor - line) + aliasEnd;
    }
    return ZR_TRUE;
}

static TZrBool lsp_parse_line_module(const char *line, char *moduleBuffer, size_t moduleBufferSize) {
    const char *cursor = line;

    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (strncmp(cursor, "%module", 7) != 0) {
        return ZR_FALSE;
    }
    cursor += 7;
    return lsp_parse_quoted_or_identifier(cursor, 0, moduleBuffer, moduleBufferSize, ZR_NULL, ZR_NULL);
}

static TZrBool lsp_parse_line_export(const char *line,
                                     TZrSize lineOffset,
                                     char *nameBuffer,
                                     size_t nameBufferSize,
                                     char *kindBuffer,
                                     size_t kindBufferSize,
                                     TZrSize *outStart,
                                     TZrSize *outEnd) {
    const char *cursor = line;
    TZrSize nameStart = 0;
    TZrSize nameEnd = 0;

    while (*cursor == ' ' || *cursor == '\t') {
        cursor++;
    }
    if (strncmp(cursor, "pub ", 4) != 0) {
        return ZR_FALSE;
    }
    cursor += 4;

    if (strncmp(cursor, "var ", 4) == 0) {
        strcpy(kindBuffer, "variable");
        cursor += 4;
    } else if (strncmp(cursor, "function ", 9) == 0) {
        strcpy(kindBuffer, "function");
        cursor += 9;
    } else {
        return ZR_FALSE;
    }

    if (!lsp_parse_quoted_or_identifier(cursor, 0, nameBuffer, nameBufferSize, &nameStart, &nameEnd)) {
        return ZR_FALSE;
    }

    if (outStart != ZR_NULL) {
        *outStart = lineOffset + (TZrSize)(cursor - line) + nameStart;
    }
    if (outEnd != ZR_NULL) {
        *outEnd = lineOffset + (TZrSize)(cursor - line) + nameEnd;
    }

    ZR_UNUSED_PARAMETER(kindBufferSize);
    return ZR_TRUE;
}

static const SZrFileVersion *lsp_get_file_version(SZrLspContext *context, SZrString *uri) {
    if (context == ZR_NULL || context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLanguageServer_IncrementalParser_GetFileVersion(context->parser, uri);
}

static const char *lsp_get_content_for_uri(SZrLspContext *context, SZrString *uri, TZrSize *outLength) {
    const SZrFileVersion *fileVersion = lsp_get_file_version(context, uri);
    if (outLength != ZR_NULL) {
        *outLength = fileVersion != ZR_NULL ? fileVersion->contentLength : 0;
    }
    return fileVersion != ZR_NULL ? fileVersion->content : ZR_NULL;
}

static TZrBool lsp_update_parser_and_analyzer(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              const TZrChar *content,
                                              TZrSize contentLength,
                                              TZrSize version) {
    TZrBool parseSuccess;
    SZrAstNode *ast;
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_IncrementalParser_UpdateFile(state,
                                                       context->parser,
                                                       uri,
                                                       content,
                                                       contentLength,
                                                       version)) {
        return ZR_FALSE;
    }

    parseSuccess = ZrLanguageServer_IncrementalParser_Parse(state, context->parser, uri);
    ast = ZrLanguageServer_IncrementalParser_GetAST(context->parser, uri);
    if (!parseSuccess || ast == ZR_NULL) {
        return ZR_TRUE;
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
    return ZR_TRUE;
}

static TZrBool lsp_collect_source_files_recursive(SZrState *state, const char *directory, SZrArray *result) {
#if defined(_WIN32)
    WIN32_FIND_DATAA findData;
    HANDLE handle;
    char pattern[ZR_LSP_MAX_PATH];
    char fullPath[ZR_LSP_MAX_PATH];

    ZR_UNUSED_PARAMETER(state);

    if (!lsp_path_join(directory, "*", pattern, sizeof(pattern))) {
        return ZR_FALSE;
    }
    handle = FindFirstFileA(pattern, &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return ZR_TRUE;
    }

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        if (!lsp_path_join(directory, findData.cFileName, fullPath, sizeof(fullPath))) {
            continue;
        }
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            lsp_collect_source_files_recursive(state, fullPath, result);
        } else if (lsp_has_suffix(findData.cFileName, ".zr")) {
            SZrString *pathString = ZrCore_String_Create(state, fullPath, strlen(fullPath));
            ZrCore_Array_Push(state, result, &pathString);
        }
    } while (FindNextFileA(handle, &findData));

    FindClose(handle);
    return ZR_TRUE;
#else
    DIR *dir;
    struct dirent *entry;
    char fullPath[ZR_LSP_MAX_PATH];
    struct stat statBuffer;

    dir = opendir(directory);
    if (dir == ZR_NULL) {
        return ZR_FALSE;
    }

    while ((entry = readdir(dir)) != ZR_NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!lsp_path_join(directory, entry->d_name, fullPath, sizeof(fullPath))) {
            continue;
        }
        if (stat(fullPath, &statBuffer) != 0) {
            continue;
        }
        if (S_ISDIR(statBuffer.st_mode)) {
            lsp_collect_source_files_recursive(state, fullPath, result);
        } else if (S_ISREG(statBuffer.st_mode) && lsp_has_suffix(entry->d_name, ".zr")) {
            SZrString *pathString = ZrCore_String_Create(state, fullPath, strlen(fullPath));
            ZrCore_Array_Push(state, result, &pathString);
        }
    }

    closedir(dir);
    return ZR_TRUE;
#endif
}

static TZrBool lsp_find_project_config_in_directory(const char *directory, char *buffer, size_t bufferSize) {
#if defined(_WIN32)
    WIN32_FIND_DATAA findData;
    HANDLE handle;
    char pattern[ZR_LSP_MAX_PATH];

    if (!lsp_path_join(directory, "*.zrp", pattern, sizeof(pattern))) {
        return ZR_FALSE;
    }
    handle = FindFirstFileA(pattern, &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return ZR_FALSE;
    }
    FindClose(handle);
    return lsp_path_join(directory, findData.cFileName, buffer, bufferSize);
#else
    DIR *dir = opendir(directory);
    struct dirent *entry;
    TZrBool found = ZR_FALSE;

    if (dir == ZR_NULL) {
        return ZR_FALSE;
    }

    while ((entry = readdir(dir)) != ZR_NULL) {
        if (lsp_has_suffix(entry->d_name, ".zrp")) {
            found = lsp_path_join(directory, entry->d_name, buffer, bufferSize);
            break;
        }
    }

    closedir(dir);
    return found;
#endif
}

static TZrBool lsp_find_nearest_project_config(const char *path, char *buffer, size_t bufferSize) {
    char currentDirectory[ZR_LSP_MAX_PATH];
    char parentDirectory[ZR_LSP_MAX_PATH];

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (lsp_has_suffix(path, ".zrp")) {
        if (strlen(path) + 1 >= bufferSize) {
            return ZR_FALSE;
        }
        strcpy(buffer, path);
        return ZR_TRUE;
    }

    if (!lsp_path_get_directory(path, currentDirectory, sizeof(currentDirectory))) {
        return ZR_FALSE;
    }

    while (currentDirectory[0] != '\0') {
        if (lsp_find_project_config_in_directory(currentDirectory, buffer, bufferSize)) {
            return ZR_TRUE;
        }
        if (!lsp_path_get_directory(currentDirectory, parentDirectory, sizeof(parentDirectory)) ||
            strcmp(parentDirectory, currentDirectory) == 0) {
            break;
        }
        strcpy(currentDirectory, parentDirectory);
    }

    return ZR_FALSE;
}

static TZrBool lsp_module_name_from_source_path(const char *sourceRoot,
                                                const char *filePath,
                                                char *buffer,
                                                size_t bufferSize) {
    size_t sourceLength;
    const char *relativePath;
    size_t index;
    size_t outLength = 0;

    if (sourceRoot == ZR_NULL || filePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    sourceLength = strlen(sourceRoot);
    if (strncmp(sourceRoot, filePath, sourceLength) != 0) {
        return ZR_FALSE;
    }

    relativePath = filePath + sourceLength;
    while (*relativePath == '/' || *relativePath == '\\') {
        relativePath++;
    }

    for (index = 0; relativePath[index] != '\0' && outLength + 1 < bufferSize; index++) {
        if (relativePath[index] == '.' && strcmp(relativePath + index, ".zr") == 0) {
            break;
        }
        buffer[outLength++] = relativePath[index] == '\\' ? '/' : relativePath[index];
    }

    buffer[outLength] = '\0';
    return outLength > 0;
}

static TZrInt32 lsp_completion_kind_from_text(const char *kind) {
    if (kind == ZR_NULL) {
        return 1;
    }
    if (strcmp(kind, "function") == 0 || strcmp(kind, "method") == 0) {
        return 3;
    }
    if (strcmp(kind, "variable") == 0) {
        return 6;
    }
    if (strcmp(kind, "module") == 0) {
        return 9;
    }
    if (strcmp(kind, "constant") == 0) {
        return 21;
    }
    if (strcmp(kind, "type") == 0 || strcmp(kind, "struct") == 0 || strcmp(kind, "class") == 0) {
        return 22;
    }
    return 1;
}

static SZrLspProjectIndex *lsp_find_project_index_by_root(SZrLspContext *context, const char *rootPath, TZrSize *outSlot) {
    TZrSize index;

    if (context == ZR_NULL || rootPath == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr = (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        if (projectPtr != ZR_NULL &&
            *projectPtr != ZR_NULL &&
            lsp_string_equals((*projectPtr)->projectRootPath, rootPath)) {
            if (outSlot != ZR_NULL) {
                *outSlot = index;
            }
            return *projectPtr;
        }
    }

    return ZR_NULL;
}

static SZrLspProjectFile *lsp_find_project_file_by_uri(SZrLspProjectIndex *projectIndex, SZrString *uri) {
    TZrSize index;
    const char *targetUri = lsp_string_or_null(uri);

    if (projectIndex == ZR_NULL || uri == ZR_NULL || targetUri == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFile **filePtr = (SZrLspProjectFile **)ZrCore_Array_Get(&projectIndex->files, index);
        if (filePtr != ZR_NULL && *filePtr != ZR_NULL && lsp_string_equals((*filePtr)->uri, targetUri)) {
            return *filePtr;
        }
    }

    return ZR_NULL;
}

static SZrLspProjectFile *lsp_find_project_file_by_module(SZrLspProjectIndex *projectIndex, const char *moduleName) {
    TZrSize index;

    if (projectIndex == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFile **filePtr = (SZrLspProjectFile **)ZrCore_Array_Get(&projectIndex->files, index);
        if (filePtr != ZR_NULL &&
            *filePtr != ZR_NULL &&
            lsp_string_equals((*filePtr)->moduleName, moduleName)) {
            return *filePtr;
        }
    }

    return ZR_NULL;
}

static SZrLspProjectIndex *lsp_find_project_for_uri(SZrLspContext *context, SZrString *uri, SZrLspProjectFile **outFile) {
    TZrSize projectIndex = 0;

    if (outFile != ZR_NULL) {
        *outFile = ZR_NULL;
    }

    if (context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    for (projectIndex = 0; projectIndex < context->projectIndexes.length; projectIndex++) {
        SZrLspProjectIndex **projectPtr =
                (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndex);
        SZrLspProjectFile *projectFile;
        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }
        projectFile = lsp_find_project_file_by_uri(*projectPtr, uri);
        if (projectFile != ZR_NULL) {
            if (outFile != ZR_NULL) {
                *outFile = projectFile;
            }
            return *projectPtr;
        }
    }

    return ZR_NULL;
}

static SZrLspImportAlias *lsp_find_import_alias(SZrLspProjectFile *projectFile, const char *alias) {
    TZrSize index;

    if (projectFile == ZR_NULL || alias == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < projectFile->imports.length; index++) {
        SZrLspImportAlias **aliasPtr = (SZrLspImportAlias **)ZrCore_Array_Get(&projectFile->imports, index);
        if (aliasPtr != ZR_NULL && *aliasPtr != ZR_NULL && lsp_string_equals((*aliasPtr)->alias, alias)) {
            return *aliasPtr;
        }
    }

    return ZR_NULL;
}

static SZrLspProjectExport *lsp_find_project_export(SZrLspProjectFile *projectFile, const char *symbolName) {
    TZrSize index;

    if (projectFile == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < projectFile->exports.length; index++) {
        SZrLspProjectExport **exportPtr = (SZrLspProjectExport **)ZrCore_Array_Get(&projectFile->exports, index);
        if (exportPtr != ZR_NULL && *exportPtr != ZR_NULL && lsp_string_equals((*exportPtr)->name, symbolName)) {
            return *exportPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool lsp_scan_project_file_text(SZrState *state,
                                          SZrString *uri,
                                          SZrString *path,
                                          const char *moduleNameHint,
                                          const char *content,
                                          TZrSize contentLength,
                                          SZrLspProjectFile **outFile) {
    SZrLspProjectFile *projectFile;
    TZrSize offset = 0;
    char parsedModuleName[ZR_LSP_MAX_PATH];
    TZrBool hasParsedModule = ZR_FALSE;

    if (state == ZR_NULL || uri == ZR_NULL || path == ZR_NULL || content == ZR_NULL || outFile == ZR_NULL) {
        return ZR_FALSE;
    }

    projectFile = (SZrLspProjectFile *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspProjectFile));
    if (projectFile == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(projectFile, 0, sizeof(SZrLspProjectFile));
    projectFile->uri = uri;
    projectFile->path = path;
    ZrCore_Array_Init(state, &projectFile->imports, sizeof(SZrLspImportAlias *), 4);
    ZrCore_Array_Init(state, &projectFile->exports, sizeof(SZrLspProjectExport *), 4);

    while (offset < contentLength) {
        TZrSize lineEnd = offset;
        char lineBuffer[2048];
        TZrSize lineLength;
        char aliasBuffer[ZR_LSP_MAX_IDENT];
        char moduleBuffer[ZR_LSP_MAX_PATH];
        char exportName[ZR_LSP_MAX_IDENT];
        char kindBuffer[ZR_LSP_MAX_IDENT];
        TZrSize startOffset = 0;
        TZrSize endOffset = 0;

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }
        lineLength = lineEnd - offset;
        if (lineLength >= sizeof(lineBuffer)) {
            lineLength = sizeof(lineBuffer) - 1;
        }
        memcpy(lineBuffer, content + offset, lineLength);
        lineBuffer[lineLength] = '\0';

        if (!hasParsedModule && lsp_parse_line_module(lineBuffer, parsedModuleName, sizeof(parsedModuleName))) {
            hasParsedModule = ZR_TRUE;
        }

        if (lsp_parse_line_import(lineBuffer,
                                  offset,
                                  aliasBuffer,
                                  sizeof(aliasBuffer),
                                  moduleBuffer,
                                  sizeof(moduleBuffer),
                                  &startOffset,
                                  &endOffset)) {
            SZrLspImportAlias *importAlias =
                    (SZrLspImportAlias *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspImportAlias));
            if (importAlias != ZR_NULL) {
                memset(importAlias, 0, sizeof(SZrLspImportAlias));
                importAlias->alias = ZrCore_String_Create(state, aliasBuffer, strlen(aliasBuffer));
                importAlias->modulePath = ZrCore_String_Create(state, moduleBuffer, strlen(moduleBuffer));
                importAlias->aliasRange = lsp_range_from_offsets(uri, content, contentLength, startOffset, endOffset);
                ZrCore_Array_Push(state, &projectFile->imports, &importAlias);
            }
        }

        if (lsp_parse_line_export(lineBuffer,
                                  offset,
                                  exportName,
                                  sizeof(exportName),
                                  kindBuffer,
                                  sizeof(kindBuffer),
                                  &startOffset,
                                  &endOffset)) {
            SZrLspProjectExport *projectExport =
                    (SZrLspProjectExport *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspProjectExport));
            if (projectExport != ZR_NULL) {
                memset(projectExport, 0, sizeof(SZrLspProjectExport));
                projectExport->name = ZrCore_String_Create(state, exportName, strlen(exportName));
                projectExport->kind = ZrCore_String_Create(state, kindBuffer, strlen(kindBuffer));
                projectExport->detail = ZrCore_String_Create(state, exportName, strlen(exportName));
                projectExport->documentation = ZR_NULL;
                projectExport->location = lsp_range_from_offsets(uri, content, contentLength, startOffset, endOffset);
                ZrCore_Array_Push(state, &projectFile->exports, &projectExport);
            }
        }

        offset = lineEnd + (lineEnd < contentLength ? 1 : 0);
    }

    if (hasParsedModule) {
        projectFile->moduleName = ZrCore_String_Create(state, parsedModuleName, strlen(parsedModuleName));
    } else if (moduleNameHint != ZR_NULL) {
        projectFile->moduleName = ZrCore_String_Create(state, moduleNameHint, strlen(moduleNameHint));
    }

    *outFile = projectFile;
    return ZR_TRUE;
}

static TZrBool lsp_rebuild_project_index(SZrState *state,
                                         SZrLspContext *context,
                                         const char *projectConfigPath,
                                         SZrString *updatedUri) {
    char projectConfigUri[ZR_LSP_MAX_PATH];
    char projectRootPath[ZR_LSP_MAX_PATH];
    char sourceRootPath[ZR_LSP_MAX_PATH];
    char *projectRaw;
    size_t projectRawLength = 0;
    SZrLibrary_Project *project = ZR_NULL;
    SZrLspProjectIndex *projectIndex = ZR_NULL;
    SZrArray sourceFiles;
    TZrSize fileIndex;
    TZrSize existingSlot = 0;
    SZrLspProjectIndex *existingProject;

    if (state == ZR_NULL || context == ZR_NULL || projectConfigPath == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!lsp_path_to_file_uri(projectConfigPath, projectConfigUri, sizeof(projectConfigUri))) {
        return ZR_FALSE;
    }

    projectRaw = lsp_read_text_file(projectConfigPath, &projectRawLength);
    if (projectRaw == ZR_NULL) {
        return ZR_FALSE;
    }

    project = ZrLibrary_Project_New(state, projectRaw, projectConfigPath);
    free(projectRaw);
    if (project == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!lsp_path_get_directory(projectConfigPath, projectRootPath, sizeof(projectRootPath)) ||
        !lsp_path_join(projectRootPath,
                       lsp_string_or_null(project->source),
                       sourceRootPath,
                       sizeof(sourceRootPath))) {
        ZrLibrary_Project_Free(state, project);
        return ZR_FALSE;
    }

    projectIndex = (SZrLspProjectIndex *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspProjectIndex));
    if (projectIndex == ZR_NULL) {
        ZrLibrary_Project_Free(state, project);
        return ZR_FALSE;
    }
    memset(projectIndex, 0, sizeof(SZrLspProjectIndex));
    projectIndex->configUri = ZrCore_String_Create(state, projectConfigUri, strlen(projectConfigUri));
    projectIndex->configPath = ZrCore_String_Create(state, projectConfigPath, strlen(projectConfigPath));
    projectIndex->projectRootPath = ZrCore_String_Create(state, projectRootPath, strlen(projectRootPath));
    projectIndex->sourceRootPath = ZrCore_String_Create(state, sourceRootPath, strlen(sourceRootPath));
    ZrCore_Array_Init(state, &projectIndex->files, sizeof(SZrLspProjectFile *), 8);

    ZrCore_Array_Init(state, &sourceFiles, sizeof(SZrString *), 8);
    lsp_collect_source_files_recursive(state, sourceRootPath, &sourceFiles);

    for (fileIndex = 0; fileIndex < sourceFiles.length; fileIndex++) {
        SZrString **pathPtr = (SZrString **)ZrCore_Array_Get(&sourceFiles, fileIndex);
        const char *filePath;
        char moduleName[ZR_LSP_MAX_PATH];
        char fileUriBuffer[ZR_LSP_MAX_PATH];
        SZrString *fileUri;
        SZrString *filePathString;
        const SZrFileVersion *fileVersion;
        const char *content;
        TZrSize contentLength;
        char *loadedContent = ZR_NULL;
        SZrLspProjectFile *projectFile = ZR_NULL;

        if (pathPtr == ZR_NULL || *pathPtr == ZR_NULL) {
            continue;
        }

        filePath = lsp_string_or_null(*pathPtr);
        if (filePath == ZR_NULL || !lsp_path_to_file_uri(filePath, fileUriBuffer, sizeof(fileUriBuffer))) {
            continue;
        }

        fileUri = ZrCore_String_Create(state, fileUriBuffer, strlen(fileUriBuffer));
        filePathString = ZrCore_String_Create(state, filePath, strlen(filePath));
        lsp_module_name_from_source_path(sourceRootPath, filePath, moduleName, sizeof(moduleName));

        fileVersion = lsp_get_file_version(context, fileUri);
        if (fileVersion != ZR_NULL) {
            content = fileVersion->content;
            contentLength = fileVersion->contentLength;
        } else {
            loadedContent = lsp_read_text_file(filePath, &contentLength);
            content = loadedContent;
            if (loadedContent != ZR_NULL) {
                lsp_update_parser_and_analyzer(state, context, fileUri, loadedContent, contentLength, 1);
            }
        }

        if (content != ZR_NULL &&
            lsp_scan_project_file_text(state,
                                       fileUri,
                                       filePathString,
                                       moduleName,
                                       content,
                                       contentLength,
                                       &projectFile)) {
            ZrCore_Array_Push(state, &projectIndex->files, &projectFile);
        }

        if (loadedContent != ZR_NULL) {
            free(loadedContent);
        }
    }

    ZrCore_Array_Free(state, &sourceFiles);
    ZrLibrary_Project_Free(state, project);

    existingProject = lsp_find_project_index_by_root(context, projectRootPath, &existingSlot);
    if (existingProject != ZR_NULL) {
        lsp_free_project_index(state, existingProject);
        ZrCore_Array_Set(&context->projectIndexes, existingSlot, &projectIndex);
    } else {
        ZrCore_Array_Push(state, &context->projectIndexes, &projectIndex);
    }

    ZR_UNUSED_PARAMETER(updatedUri);
    return ZR_TRUE;
}

static SZrLspBuiltinDocument *lsp_find_builtin_document(SZrLspContext *context, const char *moduleName) {
    TZrSize index;

    if (context == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < context->builtinDocuments.length; index++) {
        SZrLspBuiltinDocument **documentPtr =
                (SZrLspBuiltinDocument **)ZrCore_Array_Get(&context->builtinDocuments, index);
        if (documentPtr != ZR_NULL &&
            *documentPtr != ZR_NULL &&
            lsp_string_equals((*documentPtr)->moduleName, moduleName)) {
            return *documentPtr;
        }
    }

    return ZR_NULL;
}

static SZrLspBuiltinSymbol *lsp_find_builtin_symbol(SZrLspBuiltinDocument *document, const char *symbolName) {
    TZrSize index;

    if (document == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < document->symbols.length; index++) {
        SZrLspBuiltinSymbol **symbolPtr =
                (SZrLspBuiltinSymbol **)ZrCore_Array_Get(&document->symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL && lsp_string_equals((*symbolPtr)->name, symbolName)) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static const ZrLibTypeHintDescriptor *lsp_find_type_hint(const ZrLibModuleDescriptor *descriptor, const char *symbolName) {
    TZrSize index;

    if (descriptor == ZR_NULL || symbolName == ZR_NULL || descriptor->typeHints == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *hint = &descriptor->typeHints[index];
        if (hint->symbolName != ZR_NULL && strcmp(hint->symbolName, symbolName) == 0) {
            return hint;
        }
    }

    return ZR_NULL;
}

static void lsp_builder_append(char *buffer,
                               size_t bufferSize,
                               size_t *inOutLength,
                               TZrInt32 *inOutLine,
                               const char *format,
                               ...) {
    va_list arguments;
    int written;
    size_t index;

    if (buffer == ZR_NULL || inOutLength == ZR_NULL || *inOutLength >= bufferSize) {
        return;
    }

    va_start(arguments, format);
    written = vsnprintf(buffer + *inOutLength, bufferSize - *inOutLength, format, arguments);
    va_end(arguments);
    if (written <= 0) {
        return;
    }

    if (*inOutLength + (size_t)written >= bufferSize) {
        written = (int)(bufferSize - *inOutLength - 1);
    }

    for (index = *inOutLength; index < *inOutLength + (size_t)written; index++) {
        if (buffer[index] == '\n') {
            (*inOutLine)++;
        }
    }

    *inOutLength += (size_t)written;
}

static TZrBool lsp_builtin_symbol_exists(SZrLspBuiltinDocument *document, const char *symbolName) {
    return lsp_find_builtin_symbol(document, symbolName) != ZR_NULL;
}

static void lsp_builtin_add_symbol(SZrState *state,
                                   SZrLspBuiltinDocument *document,
                                   const char *symbolName,
                                   const char *kind,
                                   const char *detail,
                                   const char *documentation,
                                   const char *targetModule,
                                   TZrBool isModuleLink,
                                   TZrInt32 lineNumber) {
    SZrLspBuiltinSymbol *symbol;
    SZrFilePosition start;
    SZrFilePosition end;

    if (state == ZR_NULL ||
        document == ZR_NULL ||
        symbolName == ZR_NULL ||
        lsp_builtin_symbol_exists(document, symbolName)) {
        return;
    }

    symbol = (SZrLspBuiltinSymbol *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspBuiltinSymbol));
    if (symbol == ZR_NULL) {
        return;
    }
    memset(symbol, 0, sizeof(SZrLspBuiltinSymbol));

    start.line = lineNumber;
    start.column = 0;
    start.offset = 0;
    end.line = lineNumber;
    end.column = (TZrInt32)strlen(symbolName);
    end.offset = 0;

    symbol->name = ZrCore_String_Create(state, symbolName, strlen(symbolName));
    symbol->kind = ZrCore_String_Create(state, kind != ZR_NULL ? kind : "text", strlen(kind != ZR_NULL ? kind : "text"));
    symbol->detail = detail != ZR_NULL ? ZrCore_String_Create(state, detail, strlen(detail)) : ZR_NULL;
    symbol->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, documentation, strlen(documentation))
                                                     : ZR_NULL;
    symbol->targetModule = targetModule != ZR_NULL ? ZrCore_String_Create(state, targetModule, strlen(targetModule))
                                                   : ZR_NULL;
    symbol->location = ZrParser_FileRange_Create(start, end, document->uri);
    symbol->isModuleLink = isModuleLink;
    ZrCore_Array_Push(state, &document->symbols, &symbol);
}

static TZrBool lsp_build_builtin_document(SZrState *state,
                                          SZrLspContext *context,
                                          const ZrLibModuleDescriptor *descriptor) {
    SZrLspBuiltinDocument *document;
    char uriBuffer[ZR_LSP_MAX_PATH];
    char contentBuffer[32768];
    size_t contentLength = 0;
    TZrInt32 lineNumber = 0;
    TZrSize index;

    if (state == ZR_NULL || context == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (snprintf(uriBuffer, sizeof(uriBuffer), "zr://builtin/%s.zr", descriptor->moduleName) >= (int)sizeof(uriBuffer)) {
        return ZR_FALSE;
    }

    document = (SZrLspBuiltinDocument *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspBuiltinDocument));
    if (document == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(document, 0, sizeof(SZrLspBuiltinDocument));
    document->descriptor = descriptor;
    document->moduleName = ZrCore_String_Create(state, descriptor->moduleName, strlen(descriptor->moduleName));
    document->uri = ZrCore_String_Create(state, uriBuffer, strlen(uriBuffer));
    ZrCore_Array_Init(state, &document->symbols, sizeof(SZrLspBuiltinSymbol *), 8);

    contentBuffer[0] = '\0';
    lsp_builder_append(contentBuffer, sizeof(contentBuffer), &contentLength, &lineNumber,
                       "%%module \"%s\";\n", descriptor->moduleName);
    if (descriptor->documentation != ZR_NULL) {
        lsp_builder_append(contentBuffer, sizeof(contentBuffer), &contentLength, &lineNumber,
                           "// %s\n", descriptor->documentation);
    }

    for (index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *link = &descriptor->moduleLinks[index];
        TZrInt32 symbolLine = lineNumber;
        lsp_builtin_add_symbol(state,
                               document,
                               link->name,
                               "module",
                               link->moduleName,
                               link->documentation,
                               link->moduleName,
                               ZR_TRUE,
                               symbolLine);
        lsp_builder_append(contentBuffer, sizeof(contentBuffer), &contentLength, &lineNumber,
                           "pub var %s = %%import(\"%s\");\n",
                           link->name != ZR_NULL ? link->name : "module",
                           link->moduleName != ZR_NULL ? link->moduleName : "");
    }

    for (index = 0; index < descriptor->constantCount; index++) {
        const ZrLibConstantDescriptor *constant = &descriptor->constants[index];
        TZrInt32 symbolLine = lineNumber;
        lsp_builtin_add_symbol(state,
                               document,
                               constant->name,
                               "constant",
                               constant->typeName,
                               constant->documentation,
                               ZR_NULL,
                               ZR_FALSE,
                               symbolLine);
        lsp_builder_append(contentBuffer, sizeof(contentBuffer), &contentLength, &lineNumber,
                           "pub const %s;\n", constant->name != ZR_NULL ? constant->name : "constant");
    }

    for (index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *function = &descriptor->functions[index];
        const ZrLibTypeHintDescriptor *hint = lsp_find_type_hint(descriptor, function->name);
        const char *detail = hint != ZR_NULL && hint->signature != ZR_NULL
                             ? hint->signature
                             : function->returnTypeName;
        TZrInt32 symbolLine = lineNumber;
        lsp_builtin_add_symbol(state,
                               document,
                               function->name,
                               "function",
                               detail,
                               function->documentation != ZR_NULL ? function->documentation
                                                                   : (hint != ZR_NULL ? hint->documentation : ZR_NULL),
                               ZR_NULL,
                               ZR_FALSE,
                               symbolLine);
        lsp_builder_append(contentBuffer, sizeof(contentBuffer), &contentLength, &lineNumber,
                           "pub function %s;\n", function->name != ZR_NULL ? function->name : "fn");
    }

    for (index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        const ZrLibTypeHintDescriptor *hint = lsp_find_type_hint(descriptor, typeDescriptor->name);
        const char *detail = hint != ZR_NULL && hint->signature != ZR_NULL
                             ? hint->signature
                             : typeDescriptor->constructorSignature;
        TZrInt32 symbolLine = lineNumber;
        lsp_builtin_add_symbol(state,
                               document,
                               typeDescriptor->name,
                               typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS ? "class" : "type",
                               detail,
                               typeDescriptor->documentation != ZR_NULL ? typeDescriptor->documentation
                                                                        : (hint != ZR_NULL ? hint->documentation : ZR_NULL),
                               ZR_NULL,
                               ZR_FALSE,
                               symbolLine);
        lsp_builder_append(contentBuffer, sizeof(contentBuffer), &contentLength, &lineNumber,
                           "pub type %s;\n", typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "Type");
    }

    for (index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *hint = &descriptor->typeHints[index];
        TZrInt32 symbolLine = lineNumber;
        if (hint->symbolName == ZR_NULL || lsp_builtin_symbol_exists(document, hint->symbolName)) {
            continue;
        }
        lsp_builtin_add_symbol(state,
                               document,
                               hint->symbolName,
                               hint->symbolKind != ZR_NULL ? hint->symbolKind : "text",
                               hint->signature,
                               hint->documentation,
                               ZR_NULL,
                               ZR_FALSE,
                               symbolLine);
        lsp_builder_append(contentBuffer, sizeof(contentBuffer), &contentLength, &lineNumber,
                           "pub var %s;\n", hint->symbolName);
    }

    document->content = ZrCore_String_Create(state, contentBuffer, strlen(contentBuffer));
    ZrCore_Array_Push(state, &context->builtinDocuments, &document);
    return ZR_TRUE;
}

static void lsp_ensure_builtin_documents(SZrState *state, SZrLspContext *context) {
    static const char *kBuiltinModules[] = {
            "zr.math",
            "zr.system",
            "zr.system.console",
            "zr.system.env",
            "zr.system.exception",
            "zr.system.fs",
            "zr.system.gc",
            "zr.system.process",
            "zr.system.vm"
    };
    TZrSize index;

    if (state == ZR_NULL || context == ZR_NULL || context->builtinDocuments.length > 0) {
        return;
    }

    for (index = 0; index < sizeof(kBuiltinModules) / sizeof(kBuiltinModules[0]); index++) {
        const ZrLibModuleDescriptor *descriptor =
                ZrLibrary_NativeRegistry_FindModule(state->global, kBuiltinModules[index]);
        if (descriptor != ZR_NULL) {
            lsp_build_builtin_document(state, context, descriptor);
        }
    }
}

static TZrBool lsp_parse_chain_at_offset(const char *content,
                                         TZrSize contentLength,
                                         TZrSize cursorOffset,
                                         SZrLspChainQuery *query) {
    TZrSize tokenStart;
    TZrSize tokenEnd;
    TZrSize chainStart;
    TZrSize chainEnd;
    TZrSize cursor;
    TZrSize partIndex = 0;
    TZrSize scan;

    if (query == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(query, 0, sizeof(SZrLspChainQuery));

    if (content == ZR_NULL || contentLength == 0) {
        return ZR_FALSE;
    }

    cursor = cursorOffset;
    if (cursor >= contentLength) {
        cursor = contentLength - 1;
    }
    if (!lsp_is_identifier_char((unsigned char)content[cursor])) {
        if (cursor > 0 && lsp_is_identifier_char((unsigned char)content[cursor - 1])) {
            cursor--;
        } else {
            return ZR_FALSE;
        }
    }

    tokenStart = cursor;
    while (tokenStart > 0 && lsp_is_identifier_char((unsigned char)content[tokenStart - 1])) {
        tokenStart--;
    }
    tokenEnd = cursor + 1;
    while (tokenEnd < contentLength && lsp_is_identifier_char((unsigned char)content[tokenEnd])) {
        tokenEnd++;
    }

    chainStart = tokenStart;
    while (chainStart > 0) {
        TZrSize dotIndex = chainStart - 1;
        TZrSize previousStart;
        if (content[dotIndex] != '.') {
            break;
        }
        previousStart = dotIndex;
        while (previousStart > 0 && lsp_is_identifier_char((unsigned char)content[previousStart - 1])) {
            previousStart--;
        }
        if (previousStart == dotIndex) {
            break;
        }
        chainStart = previousStart;
    }

    chainEnd = tokenEnd;
    while (chainEnd < contentLength && content[chainEnd] == '.') {
        TZrSize nextEnd = chainEnd + 1;
        if (nextEnd >= contentLength || !lsp_is_identifier_char((unsigned char)content[nextEnd])) {
            break;
        }
        while (nextEnd < contentLength && lsp_is_identifier_char((unsigned char)content[nextEnd])) {
            nextEnd++;
        }
        chainEnd = nextEnd;
    }

    scan = chainStart;
    while (scan < chainEnd && partIndex < ZR_LSP_MAX_CHAIN_PARTS) {
        TZrSize identifierStart = scan;
        TZrSize identifierEnd;
        size_t outLength = 0;

        if (!lsp_is_identifier_char((unsigned char)content[identifierStart])) {
            break;
        }
        identifierEnd = identifierStart;
        while (identifierEnd < chainEnd && lsp_is_identifier_char((unsigned char)content[identifierEnd])) {
            if (outLength + 1 < ZR_LSP_MAX_IDENT) {
                query->parts[partIndex][outLength++] = content[identifierEnd];
            }
            identifierEnd++;
        }
        query->parts[partIndex][outLength] = '\0';
        if (tokenStart >= identifierStart && tokenEnd <= identifierEnd) {
            query->focusedPartIndex = partIndex;
        }
        partIndex++;
        scan = identifierEnd;
        if (scan < chainEnd && content[scan] == '.') {
            scan++;
        }
    }

    query->isValid = partIndex > 0;
    query->partCount = partIndex;
    query->tokenStart = tokenStart;
    query->tokenEnd = tokenEnd;
    query->chainStart = chainStart;
    query->chainEnd = chainEnd;
    return query->isValid;
}

static TZrBool lsp_parse_chain_before_dot(const char *content,
                                          TZrSize contentLength,
                                          TZrSize cursorOffset,
                                          SZrLspChainQuery *query) {
    TZrSize look = cursorOffset;

    while (look > 0 && isspace((unsigned char)content[look - 1])) {
        look--;
    }
    if (look == 0 || content[look - 1] != '.') {
        return ZR_FALSE;
    }
    if (look < 2) {
        return ZR_FALSE;
    }

    look--;
    while (look > 0 && isspace((unsigned char)content[look - 1])) {
        look--;
    }
    if (look == 0) {
        return ZR_FALSE;
    }

    return lsp_parse_chain_at_offset(content, contentLength, look - 1, query);
}

static TZrBool lsp_resolve_builtin_chain(SZrLspContext *context,
                                         const char *moduleName,
                                         const SZrLspChainQuery *query,
                                         TZrSize startPart,
                                         SZrLspResolvedSymbol *resolved) {
    SZrLspBuiltinDocument *currentDocument;
    TZrSize index;
    SZrLspBuiltinSymbol *symbol = ZR_NULL;

    if (context == ZR_NULL || moduleName == ZR_NULL || query == ZR_NULL || resolved == ZR_NULL) {
        return ZR_FALSE;
    }

    currentDocument = lsp_find_builtin_document(context, moduleName);
    if (currentDocument == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = startPart; index + 1 < query->partCount; index++) {
        symbol = lsp_find_builtin_symbol(currentDocument, query->parts[index]);
        if (symbol == ZR_NULL || !symbol->isModuleLink || symbol->targetModule == ZR_NULL) {
            return ZR_FALSE;
        }
        currentDocument = lsp_find_builtin_document(context, lsp_string_or_null(symbol->targetModule));
        if (currentDocument == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    if (query->partCount <= startPart) {
        return ZR_FALSE;
    }

    symbol = lsp_find_builtin_symbol(currentDocument, query->parts[query->partCount - 1]);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(resolved, 0, sizeof(SZrLspResolvedSymbol));
    resolved->kind = symbol->isModuleLink ? ZR_LSP_RESOLVED_BUILTIN_MODULE : ZR_LSP_RESOLVED_BUILTIN_SYMBOL;
    resolved->builtinDocument = currentDocument;
    resolved->builtinSymbol = symbol;
    if (symbol->targetModule != ZR_NULL) {
        strncpy(resolved->moduleName, lsp_string_or_null(symbol->targetModule), sizeof(resolved->moduleName) - 1);
    } else {
        strncpy(resolved->moduleName, lsp_string_or_null(currentDocument->moduleName), sizeof(resolved->moduleName) - 1);
    }
    strncpy(resolved->exportName, query->parts[query->partCount - 1], sizeof(resolved->exportName) - 1);
    return ZR_TRUE;
}

static TZrBool lsp_resolve_symbol_for_uri(SZrState *state,
                                          SZrLspContext *context,
                                          SZrString *uri,
                                          const SZrLspChainQuery *query,
                                          SZrLspResolvedSymbol *resolved) {
    SZrLspProjectFile *currentFile = ZR_NULL;
    SZrLspProjectIndex *project = lsp_find_project_for_uri(context, uri, &currentFile);
    SZrLspImportAlias *importAlias;
    TZrSize contentLength = 0;
    const char *content;
    char moduleBuffer[ZR_LSP_MAX_PATH];

    ZR_UNUSED_PARAMETER(state);

    if (context == ZR_NULL || uri == ZR_NULL || query == ZR_NULL || resolved == ZR_NULL || !query->isValid) {
        return ZR_FALSE;
    }

    memset(resolved, 0, sizeof(SZrLspResolvedSymbol));

    if (project == ZR_NULL || currentFile == ZR_NULL) {
        content = lsp_get_content_for_uri(context, uri, &contentLength);
        if (content != ZR_NULL &&
            lsp_find_import_module_in_content(content, contentLength, query->parts[0], moduleBuffer, sizeof(moduleBuffer)) &&
            strncmp(moduleBuffer, "zr.", 3) == 0) {
            return lsp_resolve_builtin_chain(context, moduleBuffer, query, 1, resolved);
        }
        return ZR_FALSE;
    }

    importAlias = lsp_find_import_alias(currentFile, query->parts[0]);
    if (importAlias == ZR_NULL || importAlias->modulePath == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strncmp(lsp_string_or_null(importAlias->modulePath), "zr.", 3) == 0) {
        return lsp_resolve_builtin_chain(context,
                                         lsp_string_or_null(importAlias->modulePath),
                                         query,
                                         1,
                                         resolved);
    }

    if (query->partCount < 2) {
        return ZR_FALSE;
    }

    resolved->project = project;
    resolved->projectFile = lsp_find_project_file_by_module(project, lsp_string_or_null(importAlias->modulePath));
    if (resolved->projectFile == ZR_NULL) {
        return ZR_FALSE;
    }
    resolved->projectExport = lsp_find_project_export(resolved->projectFile, query->parts[1]);
    if (resolved->projectExport == ZR_NULL) {
        return ZR_FALSE;
    }

    resolved->kind = ZR_LSP_RESOLVED_PROJECT_EXPORT;
    strncpy(resolved->moduleName,
            lsp_string_or_null(importAlias->modulePath),
            sizeof(resolved->moduleName) - 1);
    strncpy(resolved->exportName, query->parts[1], sizeof(resolved->exportName) - 1);
    return ZR_TRUE;
}

static const char *lsp_read_content_for_project_file(SZrLspContext *context,
                                                     SZrLspProjectFile *projectFile,
                                                     TZrSize *outLength,
                                                     char **ownedBuffer) {
    const SZrFileVersion *fileVersion;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (ownedBuffer != ZR_NULL) {
        *ownedBuffer = ZR_NULL;
    }

    if (context == ZR_NULL || projectFile == ZR_NULL) {
        return ZR_NULL;
    }

    fileVersion = lsp_get_file_version(context, projectFile->uri);
    if (fileVersion != ZR_NULL) {
        if (outLength != ZR_NULL) {
            *outLength = fileVersion->contentLength;
        }
        return fileVersion->content;
    }

    if (projectFile->path != ZR_NULL) {
        char *buffer = lsp_read_text_file(lsp_string_or_null(projectFile->path), outLength);
        if (ownedBuffer != ZR_NULL) {
            *ownedBuffer = buffer;
        }
        return buffer;
    }

    return ZR_NULL;
}

static TZrBool lsp_alias_is_imported_in_content(const char *content, TZrSize contentLength, const char *alias) {
    TZrSize offset = 0;

    if (content == ZR_NULL || alias == ZR_NULL) {
        return ZR_FALSE;
    }

    while (offset < contentLength) {
        TZrSize lineEnd = offset;
        char lineBuffer[2048];
        TZrSize lineLength;
        char aliasBuffer[ZR_LSP_MAX_IDENT];
        char moduleBuffer[ZR_LSP_MAX_PATH];

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }
        lineLength = lineEnd - offset;
        if (lineLength >= sizeof(lineBuffer)) {
            lineLength = sizeof(lineBuffer) - 1;
        }
        memcpy(lineBuffer, content + offset, lineLength);
        lineBuffer[lineLength] = '\0';

        if (lsp_parse_line_import(lineBuffer,
                                  offset,
                                  aliasBuffer,
                                  sizeof(aliasBuffer),
                                  moduleBuffer,
                                  sizeof(moduleBuffer),
                                  ZR_NULL,
                                  ZR_NULL) &&
            strcmp(aliasBuffer, alias) == 0) {
            return ZR_TRUE;
        }

        offset = lineEnd + (lineEnd < contentLength ? 1 : 0);
    }

    return ZR_FALSE;
}

static TZrBool lsp_find_import_module_in_content(const char *content,
                                                 TZrSize contentLength,
                                                 const char *alias,
                                                 char *moduleBuffer,
                                                 size_t moduleBufferSize) {
    TZrSize offset = 0;

    if (content == ZR_NULL || alias == ZR_NULL || moduleBuffer == ZR_NULL || moduleBufferSize == 0) {
        return ZR_FALSE;
    }

    while (offset < contentLength) {
        TZrSize lineEnd = offset;
        char lineBuffer[2048];
        TZrSize lineLength;
        char aliasBuffer[ZR_LSP_MAX_IDENT];
        char parsedModuleBuffer[ZR_LSP_MAX_PATH];

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }
        lineLength = lineEnd - offset;
        if (lineLength >= sizeof(lineBuffer)) {
            lineLength = sizeof(lineBuffer) - 1;
        }
        memcpy(lineBuffer, content + offset, lineLength);
        lineBuffer[lineLength] = '\0';

        if (lsp_parse_line_import(lineBuffer,
                                  offset,
                                  aliasBuffer,
                                  sizeof(aliasBuffer),
                                  parsedModuleBuffer,
                                  sizeof(parsedModuleBuffer),
                                  ZR_NULL,
                                  ZR_NULL) &&
            strcmp(aliasBuffer, alias) == 0) {
            strncpy(moduleBuffer, parsedModuleBuffer, moduleBufferSize - 1);
            moduleBuffer[moduleBufferSize - 1] = '\0';
            return ZR_TRUE;
        }

        offset = lineEnd + (lineEnd < contentLength ? 1 : 0);
    }

    return ZR_FALSE;
}

static TZrBool lsp_find_import_insertion_offset(const char *content, TZrSize contentLength, TZrSize *outOffset) {
    TZrSize offset = 0;
    TZrSize insertionOffset = 0;

    if (outOffset == ZR_NULL) {
        return ZR_FALSE;
    }
    *outOffset = 0;

    if (content == ZR_NULL) {
        return ZR_TRUE;
    }

    while (offset < contentLength) {
        TZrSize lineEnd = offset;
        char lineBuffer[2048];
        TZrSize lineLength;
        char aliasBuffer[ZR_LSP_MAX_IDENT];
        char moduleBuffer[ZR_LSP_MAX_PATH];
        char moduleName[ZR_LSP_MAX_PATH];
        TZrSize trimIndex = 0;

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }
        lineLength = lineEnd - offset;
        if (lineLength >= sizeof(lineBuffer)) {
            lineLength = sizeof(lineBuffer) - 1;
        }
        memcpy(lineBuffer, content + offset, lineLength);
        lineBuffer[lineLength] = '\0';

        while (lineBuffer[trimIndex] == ' ' || lineBuffer[trimIndex] == '\t') {
            trimIndex++;
        }

        if (lineBuffer[trimIndex] == '\0') {
            offset = lineEnd + (lineEnd < contentLength ? 1 : 0);
            continue;
        }

        if (lsp_parse_line_module(lineBuffer, moduleName, sizeof(moduleName)) ||
            lsp_parse_line_import(lineBuffer,
                                  offset,
                                  aliasBuffer,
                                  sizeof(aliasBuffer),
                                  moduleBuffer,
                                  sizeof(moduleBuffer),
                                  ZR_NULL,
                                  ZR_NULL)) {
            insertionOffset = lineEnd + (lineEnd < contentLength ? 1 : 0);
            offset = insertionOffset;
            continue;
        }

        break;
    }

    *outOffset = insertionOffset;
    return ZR_TRUE;
}

static SZrLspTextEdit *lsp_create_text_edit(SZrState *state,
                                            SZrString *uri,
                                            const char *content,
                                            TZrSize contentLength,
                                            TZrSize offset,
                                            const char *newText) {
    SZrLspTextEdit *edit;
    SZrFilePosition position;

    if (state == ZR_NULL || uri == ZR_NULL || newText == ZR_NULL) {
        return ZR_NULL;
    }

    edit = (SZrLspTextEdit *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspTextEdit));
    if (edit == ZR_NULL) {
        return ZR_NULL;
    }
    memset(edit, 0, sizeof(SZrLspTextEdit));

    position = lsp_offset_to_file_position(content, contentLength, offset);
    edit->range = ZrLanguageServer_LspRange_FromFileRange(ZrParser_FileRange_Create(position, position, uri));
    edit->newText = ZrCore_String_Create(state, newText, strlen(newText));
    return edit;
}

static TZrBool lsp_auto_import_module_for_query(SZrLspContext *context,
                                                const char *content,
                                                TZrSize contentLength,
                                                const SZrLspChainQuery *query,
                                                char *moduleBuffer,
                                                size_t moduleBufferSize) {
    TZrSize index;

    if (context == ZR_NULL || query == ZR_NULL || !query->isValid || query->partCount < 2) {
        return ZR_FALSE;
    }

    lsp_ensure_builtin_documents(context->state, context);

    for (index = 0; index < context->builtinDocuments.length; index++) {
        SZrLspBuiltinDocument **documentPtr =
                (SZrLspBuiltinDocument **)ZrCore_Array_Get(&context->builtinDocuments, index);
        const char *moduleName;
        const char *rootAlias;
        if (documentPtr == ZR_NULL || *documentPtr == ZR_NULL) {
            continue;
        }
        moduleName = lsp_string_or_null((*documentPtr)->moduleName);
        if (moduleName == ZR_NULL || strncmp(moduleName, "zr.", 3) != 0 || strchr(moduleName + 3, '.') != ZR_NULL) {
            continue;
        }
        rootAlias = moduleName + 3;
        if (strcmp(rootAlias, query->parts[0]) != 0) {
            continue;
        }
        if (lsp_find_builtin_symbol(*documentPtr, query->parts[1]) == ZR_NULL) {
            continue;
        }
        if (lsp_alias_is_imported_in_content(content, contentLength, query->parts[0])) {
            return ZR_FALSE;
        }
        strncpy(moduleBuffer, moduleName, moduleBufferSize - 1);
        moduleBuffer[moduleBufferSize - 1] = '\0';
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool lsp_push_location(SZrState *state, SZrArray *result, SZrString *uri, SZrFileRange range) {
    SZrLspLocation *location;
    const char *targetUri;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    targetUri = ZrCore_String_GetNativeString(uri);
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspLocation **existingPtr = (SZrLspLocation **)ZrCore_Array_Get(result, index);
        const char *existingUri;
        if (existingPtr == ZR_NULL || *existingPtr == ZR_NULL || (*existingPtr)->uri == ZR_NULL) {
            continue;
        }

        existingUri = ZrCore_String_GetNativeString((*existingPtr)->uri);
        if (existingUri != ZR_NULL &&
            targetUri != ZR_NULL &&
            strcmp(existingUri, targetUri) == 0 &&
            (*existingPtr)->range.start.line == range.start.line &&
            (*existingPtr)->range.start.character == range.start.column &&
            (*existingPtr)->range.end.line == range.end.line &&
            (*existingPtr)->range.end.character == range.end.column) {
            return ZR_TRUE;
        }
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }
    location->uri = uri;
    location->range = ZrLanguageServer_LspRange_FromFileRange(range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static void lsp_push_completion_item(SZrState *state,
                                     SZrArray *result,
                                     const char *label,
                                     const char *kind,
                                     const char *detail,
                                     const char *documentation,
                                     const char *insertText,
                                     const char *insertTextFormat,
                                     const char *sourceType,
                                     SZrLspTextEdit *additionalEdit) {
    SZrLspCompletionItem *item;

    if (state == ZR_NULL || result == ZR_NULL || label == ZR_NULL) {
        return;
    }

    item = (SZrLspCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCompletionItem));
    if (item == ZR_NULL) {
        return;
    }
    memset(item, 0, sizeof(SZrLspCompletionItem));

    item->label = ZrCore_String_Create(state, label, strlen(label));
    item->kind = lsp_completion_kind_from_text(kind);
    item->detail = detail != ZR_NULL ? ZrCore_String_Create(state, detail, strlen(detail)) : ZR_NULL;
    item->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, documentation, strlen(documentation))
                                                   : ZR_NULL;
    item->insertText = ZrCore_String_Create(state,
                                            insertText != ZR_NULL ? insertText : label,
                                            strlen(insertText != ZR_NULL ? insertText : label));
    item->insertTextFormat = ZrCore_String_Create(state,
                                                  insertTextFormat != ZR_NULL ? insertTextFormat : "plaintext",
                                                  strlen(insertTextFormat != ZR_NULL ? insertTextFormat : "plaintext"));
    item->sourceType = sourceType != ZR_NULL ? ZrCore_String_Create(state, sourceType, strlen(sourceType)) : ZR_NULL;
    if (additionalEdit != ZR_NULL) {
        ZrCore_Array_Init(state, &item->additionalTextEdits, sizeof(SZrLspTextEdit *), 1);
        ZrCore_Array_Push(state, &item->additionalTextEdits, &additionalEdit);
    }

    ZrCore_Array_Push(state, result, &item);
}

static void lsp_push_code_action(SZrState *state,
                                 SZrArray *result,
                                 const char *title,
                                 const char *kind,
                                 SZrLspTextEdit *edit,
                                 const char *documentation) {
    SZrLspCodeAction *action;

    if (state == ZR_NULL || result == ZR_NULL || title == ZR_NULL || edit == ZR_NULL) {
        return;
    }

    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        return;
    }
    memset(action, 0, sizeof(SZrLspCodeAction));

    action->title = ZrCore_String_Create(state, title, strlen(title));
    action->kind = ZrCore_String_Create(state, kind != ZR_NULL ? kind : "quickfix",
                                        strlen(kind != ZR_NULL ? kind : "quickfix"));
    action->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, documentation, strlen(documentation))
                                                     : ZR_NULL;
    ZrCore_Array_Init(state, &action->edits, sizeof(SZrLspTextEdit *), 1);
    ZrCore_Array_Push(state, &action->edits, &edit);

    ZrCore_Array_Push(state, result, &action);
}

static TZrBool lsp_push_auto_import_completion_if_needed(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrString *uri,
                                                         const char *content,
                                                         TZrSize contentLength,
                                                         const SZrLspChainQuery *query,
                                                         SZrArray *result) {
    char moduleName[ZR_LSP_MAX_PATH];
    char newText[ZR_LSP_MAX_PATH + 64];
    TZrSize insertOffset = 0;
    SZrLspTextEdit *edit;

    if (!lsp_auto_import_module_for_query(context, content, contentLength, query, moduleName, sizeof(moduleName))) {
        return ZR_FALSE;
    }

    if (!lsp_find_import_insertion_offset(content, contentLength, &insertOffset)) {
        return ZR_FALSE;
    }

    snprintf(newText, sizeof(newText), "var %s = %%import(\"%s\");\n", query->parts[0], moduleName);
    edit = lsp_create_text_edit(state, uri, content, contentLength, insertOffset, newText);
    if (edit == ZR_NULL) {
        return ZR_FALSE;
    }

    lsp_push_completion_item(state,
                             result,
                             query->parts[0],
                             "module",
                             moduleName,
                             "Auto import builtin module",
                             query->parts[0],
                             "plaintext",
                             "auto-import",
                             edit);
    return ZR_TRUE;
}

static TZrBool lsp_push_project_member_completions(SZrState *state,
                                                   SZrLspContext *context,
                                                   SZrString *uri,
                                                   const SZrLspChainQuery *query,
                                                   SZrArray *result) {
    SZrLspProjectFile *currentFile = ZR_NULL;
    SZrLspProjectIndex *project = lsp_find_project_for_uri(context, uri, &currentFile);
    SZrLspImportAlias *importAlias;
    TZrSize index;

    if (project == ZR_NULL || currentFile == ZR_NULL || query == ZR_NULL || query->partCount == 0) {
        return ZR_FALSE;
    }

    importAlias = lsp_find_import_alias(currentFile, query->parts[0]);
    if (importAlias == ZR_NULL || importAlias->modulePath == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strncmp(lsp_string_or_null(importAlias->modulePath), "zr.", 3) == 0) {
        SZrLspResolvedSymbol resolved;
        SZrLspBuiltinDocument *document;
        if (query->partCount == 1) {
            document = lsp_find_builtin_document(context, lsp_string_or_null(importAlias->modulePath));
        } else if (!lsp_resolve_builtin_chain(context,
                                              lsp_string_or_null(importAlias->modulePath),
                                              query,
                                              1,
                                              &resolved)) {
            return ZR_FALSE;
        } else {
            document = resolved.kind == ZR_LSP_RESOLVED_BUILTIN_MODULE
                       ? lsp_find_builtin_document(context, resolved.moduleName)
                       : resolved.builtinDocument;
        }
        if (document == ZR_NULL) {
            return ZR_FALSE;
        }
        for (index = 0; index < document->symbols.length; index++) {
            SZrLspBuiltinSymbol **symbolPtr =
                    (SZrLspBuiltinSymbol **)ZrCore_Array_Get(&document->symbols, index);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                lsp_push_completion_item(state,
                                         result,
                                         lsp_string_or_null((*symbolPtr)->name),
                                         lsp_string_or_null((*symbolPtr)->kind),
                                         lsp_string_or_null((*symbolPtr)->detail),
                                         lsp_string_or_null((*symbolPtr)->documentation),
                                         ZR_NULL,
                                         "plaintext",
                                         "builtin",
                                         ZR_NULL);
            }
        }
        return ZR_TRUE;
    }

    currentFile = lsp_find_project_file_by_module(project, lsp_string_or_null(importAlias->modulePath));
    if (currentFile == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < currentFile->exports.length; index++) {
        SZrLspProjectExport **exportPtr =
                (SZrLspProjectExport **)ZrCore_Array_Get(&currentFile->exports, index);
        if (exportPtr != ZR_NULL && *exportPtr != ZR_NULL) {
            lsp_push_completion_item(state,
                                     result,
                                     lsp_string_or_null((*exportPtr)->name),
                                     lsp_string_or_null((*exportPtr)->kind),
                                     lsp_string_or_null((*exportPtr)->detail),
                                     lsp_string_or_null((*exportPtr)->documentation),
                                     ZR_NULL,
                                     "plaintext",
                                     "project",
                                     ZR_NULL);
        }
    }

    return currentFile->exports.length > 0;
}

static void lsp_push_directive_completions(SZrState *state, SZrArray *result) {
    lsp_push_completion_item(state, result, "%module", "keyword", "Declare module namespace",
                             "Declare the current script module.", "%module \"${1:name}\";", "snippet", "directive",
                             ZR_NULL);
    lsp_push_completion_item(state, result, "%import", "keyword", "Import module",
                             "Import a project or builtin module.", "%import(\"${1:module}\")", "snippet", "directive",
                             ZR_NULL);
    lsp_push_completion_item(state, result, "%compileTime", "keyword", "Compile-time block",
                             "Run a declaration or block during compilation.", "%compileTime {\n    ${1}\n}", "snippet",
                             "directive", ZR_NULL);
    lsp_push_completion_item(state, result, "%extern", "keyword", "Extern block",
                             "Declare native extern symbols.", "%extern \"${1:library}\" {\n    ${2}\n}", "snippet",
                             "directive", ZR_NULL);
    lsp_push_completion_item(state, result, "%test", "keyword", "Test declaration",
                             "Declare a test block.", "%test ${1:name}() {\n    ${2}\n}", "snippet", "directive",
                             ZR_NULL);
}

static TZrBool lsp_convert_analyzer_completions(SZrState *state,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrString *uri,
                                                SZrLspPosition position,
                                                const char *content,
                                                TZrSize contentLength,
                                                SZrArray *result) {
    SZrArray completions;
    TZrSize index;
    SZrFilePosition filePosition = content != ZR_NULL
                                   ? ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                                           content,
                                                                                           contentLength)
                                   : ZrLanguageServer_LspPosition_ToFilePosition(position);
    SZrFileRange fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);

    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
    if (!ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, fileRange, &completions)) {
        ZrCore_Array_Free(state, &completions);
        return ZR_FALSE;
    }

    for (index = 0; index < completions.length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(&completions, index);
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL) {
            continue;
        }
        lsp_push_completion_item(state,
                                 result,
                                 lsp_string_or_null((*itemPtr)->label),
                                 lsp_string_or_null((*itemPtr)->kind),
                                 lsp_string_or_null((*itemPtr)->detail),
                                 lsp_string_or_null((*itemPtr)->documentation),
                                 ZR_NULL,
                                 "plaintext",
                                 "symbol",
                                 ZR_NULL);
    }

    ZrCore_Array_Free(state, &completions);
    return ZR_TRUE;
}

// 更新文档
TZrBool ZrLanguageServer_Lsp_UpdateDocument(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            const TZrChar *content,
                                            TZrSize contentLength,
                                            TZrSize version) {
    char pathBuffer[ZR_LSP_MAX_PATH];
    char projectConfigPath[ZR_LSP_MAX_PATH];

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    lsp_ensure_builtin_documents(state, context);
    if (!lsp_update_parser_and_analyzer(state, context, uri, content, contentLength, version)) {
        return ZR_FALSE;
    }

    if (lsp_file_uri_to_path(lsp_string_or_null(uri), pathBuffer, sizeof(pathBuffer)) &&
        lsp_find_nearest_project_config(pathBuffer, projectConfigPath, sizeof(projectConfigPath))) {
        lsp_rebuild_project_index(state, context, projectConfigPath, uri);
    }

    return ZR_TRUE;
}

// 获取诊断
TZrBool ZrLanguageServer_Lsp_GetDiagnostics(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrArray diagnostics;
    TZrSize index;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDiagnostic *), 4);
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrDiagnostic *), 8);
    if (!ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(state, analyzer, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        return ZR_FALSE;
    }

    for (index = 0; index < diagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr = (SZrDiagnostic **)ZrCore_Array_Get(&diagnostics, index);
        SZrLspDiagnostic *lspDiagnostic;
        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL) {
            continue;
        }
        lspDiagnostic = (SZrLspDiagnostic *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDiagnostic));
        if (lspDiagnostic == ZR_NULL) {
            continue;
        }
        memset(lspDiagnostic, 0, sizeof(SZrLspDiagnostic));
        lspDiagnostic->range = ZrLanguageServer_LspRange_FromFileRange((*diagnosticPtr)->location);
        lspDiagnostic->severity = (TZrInt32)(*diagnosticPtr)->severity + 1;
        lspDiagnostic->code = (*diagnosticPtr)->code;
        lspDiagnostic->message = (*diagnosticPtr)->message;
        ZrCore_Array_Init(state, &lspDiagnostic->relatedInformation, sizeof(SZrLspLocation *), 1);
        ZrCore_Array_Push(state, result, &lspDiagnostic);
    }

    ZrCore_Array_Free(state, &diagnostics);
    return ZR_TRUE;
}

// 获取补全
TZrBool ZrLanguageServer_Lsp_GetCompletion(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrLspPosition position,
                                           SZrArray *result) {
    TZrSize contentLength = 0;
    const char *content;
    SZrFilePosition filePosition;
    SZrLspChainQuery query;
    SZrSemanticAnalyzer *analyzer;
    TZrBool producedAny = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCompletionItem *), 8);
    }

    lsp_ensure_builtin_documents(state, context);
    content = lsp_get_content_for_uri(context, uri, &contentLength);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength);
    if ((filePosition.offset > 0 && content[filePosition.offset - 1] == '%') ||
        (filePosition.offset < contentLength && content[filePosition.offset] == '%')) {
        lsp_push_directive_completions(state, result);
        return ZR_TRUE;
    }

    if (lsp_parse_chain_before_dot(content, contentLength, filePosition.offset, &query)) {
        producedAny = lsp_push_project_member_completions(state, context, uri, &query, result);
        if (producedAny) {
            return ZR_TRUE;
        }
    }

    if (lsp_parse_chain_at_offset(content, contentLength, filePosition.offset, &query)) {
        producedAny = lsp_push_auto_import_completion_if_needed(state,
                                                                context,
                                                                uri,
                                                                content,
                                                                contentLength,
                                                                &query,
                                                                result) || producedAny;
        if (producedAny) {
            return ZR_TRUE;
        }
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_TRUE;
    }
    lsp_convert_analyzer_completions(state, analyzer, uri, position, content, contentLength, result);
    return ZR_TRUE;
}

// 获取 Code Actions
TZrBool ZrLanguageServer_Lsp_GetCodeActions(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            SZrLspRange range,
                                            SZrArray *result) {
    TZrSize contentLength = 0;
    const char *content;
    SZrFileRange fileRange;
    SZrLspChainQuery query;
    char moduleName[ZR_LSP_MAX_PATH];
    char newText[ZR_LSP_MAX_PATH + 64];
    TZrSize insertOffset = 0;
    SZrLspTextEdit *edit;
    char title[ZR_LSP_MAX_PATH + 64];

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCodeAction *), 4);
    }

    content = lsp_get_content_for_uri(context, uri, &contentLength);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    fileRange = ZrLanguageServer_LspRange_ToFileRangeWithContent(range, uri, content, contentLength);
    if (!lsp_parse_chain_at_offset(content, contentLength, fileRange.start.offset, &query) ||
        !lsp_auto_import_module_for_query(context, content, contentLength, &query, moduleName, sizeof(moduleName)) ||
        !lsp_find_import_insertion_offset(content, contentLength, &insertOffset)) {
        return ZR_TRUE;
    }

    snprintf(newText, sizeof(newText), "var %s = %%import(\"%s\");\n", query.parts[0], moduleName);
    edit = lsp_create_text_edit(state, uri, content, contentLength, insertOffset, newText);
    if (edit == ZR_NULL) {
        return ZR_FALSE;
    }
    snprintf(title, sizeof(title), "Import %s as %s", moduleName, query.parts[0]);
    lsp_push_code_action(state, result, title, "quickfix", edit, "Insert missing %import.");
    return ZR_TRUE;
}

// 获取悬停信息
TZrBool ZrLanguageServer_Lsp_GetHover(SZrState *state,
                                      SZrLspContext *context,
                                      SZrString *uri,
                                      SZrLspPosition position,
                                      SZrLspHover **result) {
    TZrSize contentLength = 0;
    const char *content;
    SZrFilePosition filePosition;
    SZrLspChainQuery query;
    SZrLspResolvedSymbol resolved;
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    content = lsp_get_content_for_uri(context, uri, &contentLength);
    if (content != ZR_NULL) {
        filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength);
        if (lsp_parse_chain_at_offset(content, contentLength, filePosition.offset, &query) &&
            lsp_resolve_symbol_for_uri(state, context, uri, &query, &resolved)) {
            SZrLspHover *hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
            SZrString *contentString;
            const char *detail = ZR_NULL;
            const char *documentation = ZR_NULL;
            char hoverText[1024];

            if (hover == ZR_NULL) {
                return ZR_FALSE;
            }
            memset(hover, 0, sizeof(SZrLspHover));
            ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);

            if (resolved.kind == ZR_LSP_RESOLVED_PROJECT_EXPORT) {
                detail = lsp_string_or_null(resolved.projectExport->detail);
                documentation = lsp_string_or_null(resolved.projectExport->documentation);
                hover->range = ZrLanguageServer_LspRange_FromFileRange(resolved.projectExport->location);
            } else if (resolved.kind == ZR_LSP_RESOLVED_BUILTIN_SYMBOL ||
                       resolved.kind == ZR_LSP_RESOLVED_BUILTIN_MODULE) {
                detail = lsp_string_or_null(resolved.builtinSymbol->detail);
                documentation = lsp_string_or_null(resolved.builtinSymbol->documentation);
                hover->range = ZrLanguageServer_LspRange_FromFileRange(resolved.builtinSymbol->location);
            }

            snprintf(hoverText,
                     sizeof(hoverText),
                     "%s%s%s",
                     detail != ZR_NULL ? detail : resolved.exportName,
                     documentation != ZR_NULL ? "\n\n" : "",
                     documentation != ZR_NULL ? documentation : "");
            contentString = ZrCore_String_Create(state, hoverText, strlen(hoverText));
            ZrCore_Array_Push(state, &hover->contents, &contentString);
            *result = hover;
            return ZR_TRUE;
        }
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    {
        SZrFilePosition filePos = content != ZR_NULL
                                  ? ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                                            content,
                                                                                            contentLength)
                                  : ZrLanguageServer_LspPosition_ToFilePosition(position);
        SZrFileRange fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
        SZrHoverInfo *hoverInfo = ZR_NULL;
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, fileRange, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            return ZR_FALSE;
        }
        *result = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
        if (*result == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(*result, 0, sizeof(SZrLspHover));
        ZrCore_Array_Init(state, &(*result)->contents, sizeof(SZrString *), 1);
        ZrCore_Array_Push(state, &(*result)->contents, &hoverInfo->contents);
        (*result)->range = ZrLanguageServer_LspRange_FromFileRange(hoverInfo->range);
    }
    return ZR_TRUE;
}

// 获取定义位置
TZrBool ZrLanguageServer_Lsp_GetDefinition(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrLspPosition position,
                                           SZrArray *result) {
    TZrSize contentLength = 0;
    const char *content;
    SZrFilePosition filePosition;
    SZrLspChainQuery query;
    SZrLspResolvedSymbol resolved;
    SZrSemanticAnalyzer *analyzer;
    SZrFileRange fileRange;
    SZrSymbol *symbol;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 4);
    }

    lsp_ensure_builtin_documents(state, context);
    content = lsp_get_content_for_uri(context, uri, &contentLength);
    if (content != ZR_NULL) {
        filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength);
        if (lsp_parse_chain_at_offset(content, contentLength, filePosition.offset, &query) &&
            lsp_resolve_symbol_for_uri(state, context, uri, &query, &resolved)) {
            if (resolved.kind == ZR_LSP_RESOLVED_PROJECT_EXPORT) {
                lsp_push_location(state, result, resolved.projectExport->location.source, resolved.projectExport->location);
                return ZR_TRUE;
            }
            if (resolved.kind == ZR_LSP_RESOLVED_BUILTIN_SYMBOL) {
                lsp_push_location(state, result, resolved.builtinSymbol->location.source, resolved.builtinSymbol->location);
                return ZR_TRUE;
            }
            if (resolved.kind == ZR_LSP_RESOLVED_BUILTIN_MODULE) {
                SZrLspBuiltinDocument *targetDocument = lsp_find_builtin_document(context, resolved.moduleName);
                if (targetDocument != ZR_NULL) {
                    SZrFilePosition start = ZrParser_FilePosition_Create(0, 0, 0);
                    lsp_push_location(state,
                                      result,
                                      targetDocument->uri,
                                      ZrParser_FileRange_Create(start, start, targetDocument->uri));
                    return ZR_TRUE;
                }
            }
        }
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = content != ZR_NULL
                   ? ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength)
                   : ZrLanguageServer_LspPosition_ToFilePosition(position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    lsp_push_location(state, result, symbol->location.source, symbol->location);
    return ZR_TRUE;
}

// 查找引用
TZrBool ZrLanguageServer_Lsp_FindReferences(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            SZrLspPosition position,
                                            TZrBool includeDeclaration,
                                            SZrArray *result) {
    TZrSize contentLength = 0;
    const char *content;
    SZrFilePosition filePosition;
    SZrLspChainQuery query;
    SZrLspResolvedSymbol resolved;
    char searchNeedle[ZR_LSP_MAX_PATH];
    TZrSize fileIndex;
    SZrSemanticAnalyzer *analyzer;
    SZrFileRange fileRange;
    SZrSymbol *symbol;
    SZrArray references;
    TZrSize index;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 8);
    }

    content = lsp_get_content_for_uri(context, uri, &contentLength);
    if (content != ZR_NULL) {
        filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength);
        if (lsp_parse_chain_at_offset(content, contentLength, filePosition.offset, &query) &&
            lsp_resolve_symbol_for_uri(state, context, uri, &query, &resolved) &&
            resolved.kind == ZR_LSP_RESOLVED_PROJECT_EXPORT &&
            resolved.project != ZR_NULL) {
            if (includeDeclaration) {
                lsp_push_location(state, result, resolved.projectExport->location.source, resolved.projectExport->location);
            }
            for (fileIndex = 0; fileIndex < resolved.project->files.length; fileIndex++) {
                SZrLspProjectFile **projectFilePtr =
                        (SZrLspProjectFile **)ZrCore_Array_Get(&resolved.project->files, fileIndex);
                char *ownedBuffer = ZR_NULL;
                const char *projectContent;
                TZrSize projectContentLength = 0;
                TZrSize importIndex;
                if (projectFilePtr == ZR_NULL || *projectFilePtr == ZR_NULL) {
                    continue;
                }
                projectContent = lsp_read_content_for_project_file(context, *projectFilePtr, &projectContentLength,
                                                                   &ownedBuffer);
                if (projectContent == ZR_NULL) {
                    continue;
                }
                for (importIndex = 0; importIndex < (*projectFilePtr)->imports.length; importIndex++) {
                    SZrLspImportAlias **aliasPtr =
                            (SZrLspImportAlias **)ZrCore_Array_Get(&(*projectFilePtr)->imports, importIndex);
                    const char *found;
                    if (aliasPtr == ZR_NULL ||
                        *aliasPtr == ZR_NULL ||
                        !lsp_string_equals((*aliasPtr)->modulePath, resolved.moduleName)) {
                        continue;
                    }
                    snprintf(searchNeedle,
                             sizeof(searchNeedle),
                             "%s.%s",
                             lsp_string_or_null((*aliasPtr)->alias),
                             resolved.exportName);
                    found = projectContent;
                    while ((found = strstr(found, searchNeedle)) != ZR_NULL) {
                        TZrSize occurrenceOffset = (TZrSize)(found - projectContent);
                        TZrSize memberOffset =
                                occurrenceOffset + strlen(lsp_string_or_null((*aliasPtr)->alias)) + 1;
                        char before = occurrenceOffset > 0 ? projectContent[occurrenceOffset - 1] : '\0';
                        char after = projectContent[memberOffset + strlen(resolved.exportName)];
                        if ((occurrenceOffset == 0 || !lsp_is_identifier_char((unsigned char)before)) &&
                            !lsp_is_identifier_char((unsigned char)after)) {
                            SZrFileRange referenceRange =
                                    lsp_range_from_offsets((*projectFilePtr)->uri,
                                                           projectContent,
                                                           projectContentLength,
                                                           memberOffset,
                                                           memberOffset + strlen(resolved.exportName));
                            lsp_push_location(state, result, (*projectFilePtr)->uri, referenceRange);
                        }
                        found += strlen(searchNeedle);
                    }
                }
                if (ownedBuffer != ZR_NULL) {
                    free(ownedBuffer);
                }
            }
            return ZR_TRUE;
        }
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = content != ZR_NULL
                   ? ZrLanguageServer_LspPosition_ToFilePositionWithContent(position, content, contentLength)
                   : ZrLanguageServer_LspPosition_ToFilePosition(position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), 8);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }

    if (includeDeclaration && symbol->location.source != ZR_NULL) {
        lsp_push_location(state, result, symbol->location.source, symbol->location);
    }

    for (index = 0; index < references.length; index++) {
        SZrReference **referencePtr = (SZrReference **)ZrCore_Array_Get(&references, index);
        if (referencePtr == ZR_NULL || *referencePtr == ZR_NULL) {
            continue;
        }
        if ((*referencePtr)->type == ZR_REFERENCE_DEFINITION && !includeDeclaration) {
            continue;
        }
        lsp_push_location(state, result, (*referencePtr)->location.source, (*referencePtr)->location);
    }
    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

// 重命名符号
TZrBool ZrLanguageServer_Lsp_Rename(SZrState *state,
                                    SZrLspContext *context,
                                    SZrString *uri,
                                    SZrLspPosition position,
                                    SZrString *newName,
                                    SZrArray *result) {
    ZR_UNUSED_PARAMETER(newName);
    return ZrLanguageServer_Lsp_FindReferences(state, context, uri, position, ZR_TRUE, result);
}
