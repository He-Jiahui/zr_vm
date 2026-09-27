#include "module/lsp_module_metadata.h"
#include "project/lsp_project_internal.h"
#include "semantic/lsp_semantic_import_chain.h"
#include "lsp_virtual_documents.h"
#include "metadata/lsp_virtual_document_identity.h"
#include "zr_vm_language_server/lsp_uri.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifdef ZR_VM_PLATFORM_IS_WIN
#include <windows.h>
#else
#include <dirent.h>
#endif

/* 为项目路径、模块键和诊断文字取得 VM 字符串视图；借用值不可在状态释放后使用。 */
static const TZrChar *project_navigation_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/* 将 URI 解码后的原生路径收敛为平台相关的比较形式，供插件目标去重。 */
/* TODO: 输出超出 buffer 时静默截断；核查超长路径是否可能使两个不同插件路径被认作同一个。 */
static void project_navigation_normalize_path_for_compare(const TZrChar *path,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *source = path;
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL) {
        return;
    }

    if (ZrLibrary_File_NormalizePath((TZrNativeString)path, normalizedPath, sizeof(normalizedPath))) {
        source = normalizedPath;
    }

    for (TZrSize index = 0; source[index] != '\0' && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = source[index];
        if (current == '\\') {
            current = '/';
        }
#ifdef ZR_VM_PLATFORM_IS_WIN
        current = (TZrChar)tolower((unsigned char)current);
#endif
        buffer[writeIndex++] = current;
    }
    buffer[writeIndex] = '\0';
}

/* 导航到 native 插件时按文件路径身份匹配，而不要求 URI 文本完全相同。 */
static TZrBool project_navigation_native_paths_equal(const TZrChar *left, const TZrChar *right) {
    TZrChar normalizedLeft[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedRight[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    project_navigation_normalize_path_for_compare(left, normalizedLeft, sizeof(normalizedLeft));
    project_navigation_normalize_path_for_compare(right, normalizedRight, sizeof(normalizedRight));
    return normalizedLeft[0] != '\0' && strcmp(normalizedLeft, normalizedRight) == 0;
}

/* 插件声明导航遵循当前目标平台的实际动态库后缀。 */
static const TZrChar *project_navigation_dynamic_library_extension(void) {
#if defined(ZR_VM_PLATFORM_IS_WIN) || defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

/* 与项目 native 插件命名规则对应，生成用于反向查找的安全文件名片段。 */
static void project_navigation_sanitize_module_name(const TZrChar *moduleName,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize) {
    TZrSize cursor = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (moduleName == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; moduleName[index] != '\0' && cursor + 1 < bufferSize; index++) {
        TZrChar current = moduleName[index];
        buffer[cursor++] = (TZrChar)(isalnum((unsigned char)current) ? current : '_');
    }
    buffer[cursor] = '\0';
}

/* 根据项目根与模块名推导约定插件路径，供从二进制文件跳回导入模块。 */
static TZrBool project_navigation_build_descriptor_plugin_path(SZrLspProjectIndex *projectIndex,
                                                               SZrString *moduleName,
                                                               TZrChar *buffer,
                                                               TZrSize bufferSize) {
    const TZrChar *projectDirectory;
    const TZrChar *moduleText;
    const TZrChar *extension;
    TZrChar nativeDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sanitizedModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar pluginFileName[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (projectIndex == ZR_NULL || projectIndex->projectRootPath == ZR_NULL || moduleName == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    projectDirectory = project_navigation_string_text(projectIndex->projectRootPath);
    moduleText = project_navigation_string_text(moduleName);
    if (projectDirectory == ZR_NULL || projectDirectory[0] == '\0' || moduleText == ZR_NULL ||
        moduleText[0] == '\0') {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin((TZrNativeString)projectDirectory, "native", nativeDirectory);
    project_navigation_sanitize_module_name(moduleText, sanitizedModuleName, sizeof(sanitizedModuleName));
    extension = project_navigation_dynamic_library_extension();
    if (sanitizedModuleName[0] == '\0' ||
        snprintf(pluginFileName, sizeof(pluginFileName), "zrvm_native_%s%s", sanitizedModuleName, extension) >=
            (int)sizeof(pluginFileName)) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin((TZrNativeString)nativeDirectory, pluginFileName, buffer);
    return buffer[0] != '\0';
}

/* 按源类型选用源码、二进制元数据或描述符坐标协议，结果 Location 归调用方所有。 */
static TZrBool append_lsp_location(SZrState *state,
                                   SZrLspContext *context,
                                   SZrArray *result,
                                   SZrString *uri,
                                   SZrFileRange range,
                                   EZrLspImportedModuleSourceKind sourceKind) {
    SZrLspLocation *location;
    SZrString *locationUri;

    if (state == ZR_NULL || result == ZR_NULL || (uri == ZR_NULL && range.source == ZR_NULL)) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    locationUri = range.source != ZR_NULL ? range.source : uri;
    location->uri = locationUri;
    if (sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA) {
        if (!ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates(context,
                                                                        locationUri,
                                                                        range,
                                                                        &location->range)) {
            ZrCore_Memory_RawFree(state->global, location, sizeof(SZrLspLocation));
            return ZR_FALSE;
        }
    } else if (sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
        if (!ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates(range, &location->range)) {
            ZrCore_Memory_RawFree(state->global, location, sizeof(SZrLspLocation));
            return ZR_FALSE;
        }
    } else {
        location->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, locationUri, range);
    }
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

/* 外部元数据没有源码声明范围时用文档起点代表模块条目。 */
static SZrFileRange project_navigation_metadata_file_entry_range(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

/* 仅文档起点允许按模块整体查找；其他位置须命中具体导出声明。 */
static TZrBool project_navigation_position_is_module_entry(SZrLspPosition position) {
    return position.line == 0 && position.character == 0;
}

/* 匹配二进制导出元数据位置，偏移缺失时采用元数据的行列坐标。 */
static TZrBool project_navigation_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (range.start.offset > 0 && range.end.offset > 0 && position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}

/* 对只有列号的旧二进制导出信息采用首行约定，保持可导航性。 */
static TZrInt32 project_navigation_binary_export_normalize_line(TZrUInt32 line, TZrUInt32 column) {
    if (line > 0) {
        return (TZrInt32)line;
    }

    return column > 0 ? 1 : 0;
}

/* 校验导出符号元数据后建立文件范围；无效坐标由调用方跳过该成员。 */
static TZrBool project_navigation_binary_export_symbol_try_range(
    SZrString *uri,
    const SZrIoFunctionTypedExportSymbol *symbol,
    SZrFileRange *outRange) {
    SZrFilePosition start;
    SZrFilePosition end;
    TZrInt32 startLine;
    TZrInt32 endLine;

    if (outRange != ZR_NULL) {
        *outRange = project_navigation_metadata_file_entry_range(uri);
    }
    if (symbol == ZR_NULL || outRange == ZR_NULL || symbol->columnInSourceStart == 0 ||
        symbol->columnInSourceEnd == 0) {
        return ZR_FALSE;
    }

    startLine = project_navigation_binary_export_normalize_line(symbol->lineInSourceStart,
                                                                symbol->columnInSourceStart);
    endLine = project_navigation_binary_export_normalize_line(symbol->lineInSourceEnd,
                                                              symbol->columnInSourceEnd);
    if (startLine <= 0 || endLine <= 0) {
        return ZR_FALSE;
    }

    start = ZrParser_FilePosition_Create(0, startLine, (TZrInt32)symbol->columnInSourceStart);
    end = ZrParser_FilePosition_Create(0, endLine, (TZrInt32)symbol->columnInSourceEnd);
    if (end.line < start.line || (end.line == start.line && end.column < start.column)) {
        return ZR_FALSE;
    }

    *outRange = ZrParser_FileRange_Create(start, end, uri);
    return ZR_TRUE;
}

/* 项目导航沿用公开查询结果布局，避免在语义查询层复制外部声明状态。 */
typedef SZrLspExternalMetadataDeclaration SZrLspProjectResolvedExternalMetadataDeclaration;

/* 已有 analyzer 时从同一 AST 提取绑定和成员引用，保证临时绑定的生命周期覆盖遍历。 */
static TZrBool append_imported_member_locations_from_analyzer(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrString *moduleName,
                                                              SZrString *memberName,
                                                              SZrArray *result) {
    SZrArray bindings;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL || moduleName == ZR_NULL ||
        memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    appended = ZrLanguageServer_LspProject_AppendMatchingImportedMemberLocations(state,
                                                                                 context,
                                                                                 uri,
                                                                                 analyzer->ast,
                                                                                 &bindings,
                                                                                 moduleName,
                                                                                 memberName,
                                                                                 result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

static TZrBool append_imported_module_locations_from_analyzer(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrString *moduleName,
                                                              SZrArray *result);
static TZrBool append_import_binding_locations_from_analyzer(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrString *uri,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrString *moduleName,
                                                             SZrArray *result);
/* 只在文档快照内寻找目标行，避免磁盘文本与打开的编辑器内容错位。 */
static TZrBool project_navigation_try_find_line_bounds(const TZrChar *content,
                                                       TZrSize contentLength,
                                                       TZrInt32 fileLine,
                                                       TZrSize *outLineStart,
                                                       TZrSize *outLineEnd) {
    TZrInt32 currentLine = 1;
    TZrSize lineStart = 0;

    if (outLineStart != ZR_NULL) {
        *outLineStart = 0;
    }
    if (outLineEnd != ZR_NULL) {
        *outLineEnd = 0;
    }
    if (content == ZR_NULL || fileLine <= 0 || outLineStart == ZR_NULL || outLineEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < contentLength; index++) {
        if (currentLine == fileLine) {
            TZrSize lineEnd = index;
            while (lineEnd < contentLength && content[lineEnd] != '\n') {
                lineEnd++;
            }

            *outLineStart = lineStart;
            *outLineEnd = lineEnd;
            return ZR_TRUE;
        }

        if (content[index] == '\n') {
            currentLine++;
            lineStart = index + 1;
        }
    }

    if (currentLine == fileLine && lineStart <= contentLength) {
        *outLineStart = lineStart;
        *outLineEnd = contentLength;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/* 用打开文档的原文校正不完整的 import 字符串位置，供重命名和引用准确指向目标。 */
/* TODO: moduleName 是规范化模块键；相对路径原文字面量可能不同，需与路径规范化规则核对回退范围是否准确。 */
/* BUG: 同一行有两个相同模块字面量时，每次都选行内首个匹配并覆盖原范围；第二个导入的引用/重命名位置会错到第一个。 */
static SZrFileRange project_navigation_refine_import_module_path_location(const TZrChar *content,
                                                                          TZrSize contentLength,
                                                                          SZrFileRange range,
                                                                          SZrString *moduleName) {
    const TZrChar *moduleText;
    TZrSize moduleLength;
    TZrInt32 targetLine;
    TZrSize lineStart = 0;
    TZrSize lineEnd = 0;

    moduleText = project_navigation_string_text(moduleName);
    moduleLength = moduleText != ZR_NULL ? strlen(moduleText) : 0;
    targetLine = range.start.line > 0 ? range.start.line : range.end.line;
    if (content == ZR_NULL || moduleText == ZR_NULL || moduleLength == 0 || targetLine <= 0 ||
        !project_navigation_try_find_line_bounds(content, contentLength, targetLine, &lineStart, &lineEnd) ||
        lineEnd <= lineStart || lineEnd - lineStart < moduleLength) {
        return range;
    }

    for (TZrSize index = lineStart; index + moduleLength <= lineEnd; index++) {
        if (memcmp(content + index, moduleText, moduleLength) != 0) {
            continue;
        }

        if ((index == lineStart || content[index - 1] != '"') ||
            (index + moduleLength >= lineEnd || content[index + moduleLength] != '"')) {
            continue;
        }

        range.start.offset = index;
        range.end.offset = index + moduleLength;
        range.start.line = targetLine;
        range.end.line = targetLine;
        range.start.column = (TZrInt32)(index - lineStart) + 1;
        range.end.column = range.start.column + (TZrInt32)moduleLength;
        return range;
    }

    return range;
}

/* 批量校正临时绑定的目标范围，不修改持久 AST 或项目索引。 */
static void project_navigation_refine_import_binding_target_locations(const TZrChar *content,
                                                                      TZrSize contentLength,
                                                                      SZrArray *bindings) {
    if (content == ZR_NULL || bindings == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);
        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
            continue;
        }

        (*bindingPtr)->modulePathLocation =
            project_navigation_refine_import_module_path_location(content,
                                                                  contentLength,
                                                                  (*bindingPtr)->modulePathLocation,
                                                                  (*bindingPtr)->moduleName);
    }
}

static TZrBool append_import_target_locations_from_analyzer(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrString *uri,
                                                            SZrString *moduleName,
                                                            SZrArray *result);

/* 项目遍历框架的单文档回调；不同入口选择成员、别名或目标字面量。 */
typedef TZrBool (*TZrLspProjectSourceReferenceAppender)(SZrState *state,
                                                        SZrLspContext *context,
                                                        SZrString *uri,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrString *moduleName,
                                                        SZrString *memberName,
                                                        SZrArray *result);

/* 引用查询可触及未打开源文件：优先当前 analyzer/文档快照，再读磁盘并更新语义缓存。 */
/* 调用约束：成功但 outAnalyzer 为空表示无法取得文本，调用者必须允许跳过该文件。 */
static TZrBool project_navigation_try_get_analyzer_for_uri(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           SZrSemanticAnalyzer **outAnalyzer) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    TZrNativeString sourceBuffer = ZR_NULL;
    TZrSize sourceLength = 0;
    TZrSize sourceVersion = 0;
    TZrBool loadedFromDisk = ZR_FALSE;
    TZrBool hasSnapshot = ZR_FALSE;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    sourceVersion = fileVersion != ZR_NULL ? fileVersion->version : 0;
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        sourceBuffer = snapshot.content;
        sourceLength = snapshot.contentLength;
        sourceVersion = snapshot.version;
        hasSnapshot = ZR_TRUE;
    } else if (state->global != ZR_NULL && ZrLanguageServer_LspUri_FileToNativePath(uri, nativePath, sizeof(nativePath))) {
        sourceBuffer = ZrLibrary_File_ReadAll(state->global, nativePath);
        sourceLength = sourceBuffer != ZR_NULL ? strlen(sourceBuffer) : 0;
        loadedFromDisk = sourceBuffer != ZR_NULL;
    }

    if (sourceBuffer == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state,
                                                 context,
                                                 uri,
                                                 sourceBuffer,
                                                 sourceLength,
                                                 sourceVersion,
                                                 ZR_FALSE)) {
        if (hasSnapshot) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        }
        if (loadedFromDisk) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceBuffer,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        }
        return ZR_FALSE;
    }

    if (loadedFromDisk) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }
    if (hasSnapshot) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
    }

    return ZR_TRUE;
}

/* 纯 AST 成员引用回调；未取得 analyzer 时不让整个项目查询失败。 */
static TZrBool project_navigation_append_imported_member_for_uri(SZrState *state,
                                                                 SZrLspContext *context,
                                                                 SZrString *uri,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrString *moduleName,
                                                                 SZrString *memberName,
                                                                 SZrArray *result) {
    return analyzer != ZR_NULL
               ? append_imported_member_locations_from_analyzer(state,
                                                                context,
                                                                uri,
                                                                analyzer,
                                                                moduleName,
                                                                memberName,
                                                                result)
               : ZR_TRUE;
}

/* native 描述符成员借语义导入链处理别名与再导出，而非仅检查直接 AST 形状。 */
static TZrBool project_navigation_append_semantic_imported_member_for_uri(SZrState *state,
                                                                          SZrLspContext *context,
                                                                          SZrString *uri,
                                                                          SZrSemanticAnalyzer *analyzer,
                                                                          SZrString *moduleName,
                                                                          SZrString *memberName,
                                                                          SZrArray *result) {
    ZR_UNUSED_PARAMETER(analyzer);

    return ZrLanguageServer_LspSemanticImportChain_AppendMatchingLocationsForUri(state,
                                                                                  context,
                                                                                  ZR_NULL,
                                                                                  uri,
                                                                                  moduleName,
                                                                                  memberName,
                                                                                  result);
}

/* 模块级引用回调收集别名使用；无法分析的文件被视为无可见引用。 */
static TZrBool project_navigation_append_imported_module_for_uri(SZrState *state,
                                                                 SZrLspContext *context,
                                                                 SZrString *uri,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrString *moduleName,
                                                                 SZrString *memberName,
                                                                 SZrArray *result) {
    ZR_UNUSED_PARAMETER(memberName);

    return analyzer != ZR_NULL
               ? append_imported_module_locations_from_analyzer(state, context, uri, analyzer, moduleName, result)
               : ZR_TRUE;
}

/* 模块级引用回调收集 import 别名声明，补足成员使用以外的位置。 */
static TZrBool project_navigation_append_import_binding_for_uri(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrString *moduleName,
                                                                SZrString *memberName,
                                                                SZrArray *result) {
    ZR_UNUSED_PARAMETER(memberName);

    return analyzer != ZR_NULL
               ? append_import_binding_locations_from_analyzer(state, context, uri, analyzer, moduleName, result)
               : ZR_TRUE;
}

/* 模块级引用回调收集 import 目标字面量；用于源文件重命名和外部模块引用。 */
static TZrBool project_navigation_append_import_target_for_uri(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrString *uri,
                                                               SZrSemanticAnalyzer *analyzer,
                                                               SZrString *moduleName,
                                                               SZrString *memberName,
                                                               SZrArray *result) {
    ZR_UNUSED_PARAMETER(analyzer);
    ZR_UNUSED_PARAMETER(memberName);

    return append_import_target_locations_from_analyzer(state, context, uri, moduleName, result);
}

/* 单文件适配层：先保证 analyzer 可用，再交给指定类型的引用收集器。 */
static TZrBool project_navigation_append_source_reference_for_uri(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri,
                                                                  SZrString *moduleName,
                                                                  SZrString *memberName,
                                                                  TZrLspProjectSourceReferenceAppender appender,
                                                                  SZrArray *result) {
    SZrSemanticAnalyzer *analyzer = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || moduleName == ZR_NULL ||
        appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!project_navigation_try_get_analyzer_for_uri(state, context, uri, &analyzer)) {
        return ZR_FALSE;
    }

    return appender(state, context, uri, analyzer, moduleName, memberName, result);
}

/* 从文件系统枚举路径转入 LSP URI/文档缓存身份。 */
static TZrBool project_navigation_append_source_reference_for_path(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   const TZrChar *path,
                                                                   SZrString *moduleName,
                                                                   SZrString *memberName,
                                                                   TZrLspProjectSourceReferenceAppender appender,
                                                                   SZrArray *result) {
    SZrString *uri;

    if (state == ZR_NULL || context == ZR_NULL || path == ZR_NULL || moduleName == ZR_NULL ||
        appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    uri = ZrLanguageServer_LspUri_FromNativePath(state, path);
    if (uri == ZR_NULL) {
        return ZR_FALSE;
    }

    return project_navigation_append_source_reference_for_uri(state,
                                                              context,
                                                              uri,
                                                              moduleName,
                                                              memberName,
                                                              appender,
                                                              result);
}

/* 项目范围引用需要覆盖未打开的 .zr 文件，按源根目录逐个交给单文档回调。 */
/* TODO: 两个平台均递归跟随目录且未追踪访问过的真实路径；核查目录链接环与超深目录的行为。 */
static TZrBool project_navigation_append_source_root_references_recursive(
    SZrState *state,
    SZrLspContext *context,
    const TZrChar *directory,
    SZrString *moduleName,
    SZrString *memberName,
    TZrLspProjectSourceReferenceAppender appender,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || directory == ZR_NULL || moduleName == ZR_NULL ||
        appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    {
        TZrChar pattern[ZR_LIBRARY_MAX_PATH_LENGTH];
        WIN32_FIND_DATAA findData;
        HANDLE handle;

        ZrLibrary_File_PathJoin(directory, "*", pattern);
        handle = FindFirstFileA(pattern, &findData);
        if (handle == INVALID_HANDLE_VALUE) {
            return ZR_TRUE;
        }

        do {
            TZrChar childPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            TZrSize nameLength = strlen(findData.cFileName);

            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                continue;
            }

            ZrLibrary_File_PathJoin(directory, findData.cFileName, childPath);
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (!project_navigation_append_source_root_references_recursive(state,
                                                                                context,
                                                                                childPath,
                                                                                moduleName,
                                                                                memberName,
                                                                                appender,
                                                                                result)) {
                    FindClose(handle);
                    return ZR_FALSE;
                }
                continue;
            }

            if (nameLength >= 3 &&
                strcmp(findData.cFileName + nameLength - 3, ".zr") == 0 &&
                !project_navigation_append_source_reference_for_path(state,
                                                                    context,
                                                                    childPath,
                                                                    moduleName,
                                                                    memberName,
                                                                    appender,
                                                                    result)) {
                FindClose(handle);
                return ZR_FALSE;
            }
        } while (FindNextFileA(handle, &findData) != 0);

        FindClose(handle);
        return ZR_TRUE;
    }
#else
    {
        DIR *dir = opendir(directory);
        struct dirent *entry;

        if (dir == ZR_NULL) {
            return ZR_TRUE;
        }

        while ((entry = readdir(dir)) != ZR_NULL) {
            TZrChar childPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            TZrSize nameLength;
            EZrLibrary_File_Exist fileExist;

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            ZrLibrary_File_PathJoin((TZrNativeString)directory, entry->d_name, childPath);
            fileExist = ZrLibrary_File_Exist(childPath);
            if (fileExist == ZR_LIBRARY_FILE_IS_DIRECTORY) {
                if (!project_navigation_append_source_root_references_recursive(state,
                                                                                context,
                                                                                childPath,
                                                                                moduleName,
                                                                                memberName,
                                                                                appender,
                                                                                result)) {
                    closedir(dir);
                    return ZR_FALSE;
                }
                continue;
            }

            nameLength = strlen(entry->d_name);
            if (fileExist == ZR_LIBRARY_FILE_IS_FILE &&
                nameLength >= 3 &&
                strcmp(entry->d_name + nameLength - 3, ".zr") == 0 &&
                !project_navigation_append_source_reference_for_path(state,
                                                                    context,
                                                                    childPath,
                                                                    moduleName,
                                                                    memberName,
                                                                    appender,
                                                                    result)) {
                closedir(dir);
                return ZR_FALSE;
            }
        }

        closedir(dir);
        return ZR_TRUE;
    }
#endif
}

/* 有项目时遍历完整源根；无项目时只查请求文档，统一各类引用入口的范围。 */
static TZrBool project_navigation_append_project_source_references(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrString *fallbackUri,
    SZrString *moduleName,
    SZrString *memberName,
    TZrLspProjectSourceReferenceAppender appender,
    SZrArray *result) {
    const TZrChar *sourceRootPath;

    if (state == ZR_NULL || context == ZR_NULL || moduleName == ZR_NULL || appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (projectIndex == ZR_NULL) {
        return fallbackUri != ZR_NULL
                   ? project_navigation_append_source_reference_for_uri(state,
                                                                       context,
                                                                       fallbackUri,
                                                                       moduleName,
                                                                       memberName,
                                                                       appender,
                                                                       result)
                   : ZR_FALSE;
    }

    sourceRootPath = project_navigation_string_text(projectIndex->sourceRootPath);
    if (sourceRootPath == ZR_NULL || sourceRootPath[0] == '\0') {
        return ZR_TRUE;
    }

    return project_navigation_append_source_root_references_recursive(state,
                                                                      context,
                                                                      sourceRootPath,
                                                                      moduleName,
                                                                      memberName,
                                                                      appender,
                                                                      result);
}

/* 从元数据声明反向追踪项目源码中的成员引用。 */
static TZrBool append_project_imported_references(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrLspProjectIndex *projectIndex,
                                                  SZrString *fallbackUri,
                                                  SZrString *moduleName,
                                                  SZrString *memberName,
                                                  SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               memberName,
                                                               project_navigation_append_imported_member_for_uri,
                                                               result);
}

/* 在一个 analyzer 上重建短期绑定，以模块键收集别名接收者及成员位置。 */
static TZrBool append_imported_module_locations_from_analyzer(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrString *moduleName,
                                                              SZrArray *result) {
    SZrArray bindings;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL || moduleName == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    appended = ZrLanguageServer_LspProject_AppendMatchingImportedModuleLocations(state,
                                                                                 context,
                                                                                 uri,
                                                                                 analyzer->ast,
                                                                                 &bindings,
                                                                                 moduleName,
                                                                                 result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

/* 在一个 analyzer 上为模块键收集别名声明位置；收集后立即释放原生绑定。 */
static TZrBool append_import_binding_locations_from_analyzer(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrString *uri,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrString *moduleName,
                                                             SZrArray *result) {
    SZrArray bindings;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL || moduleName == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    appended = ZrLanguageServer_LspProject_AppendMatchingImportBindingLocations(state,
                                                                                context,
                                                                                uri,
                                                                                &bindings,
                                                                                moduleName,
                                                                                result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

/* 从当前 AST 与文档快照定位 import 字面量，避免无关文件创建结果或读取过期磁盘文本。 */
static TZrBool append_import_target_locations_from_analyzer(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrString *uri,
                                                            SZrString *moduleName,
                                                            SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrArray bindings;
    TZrBool appended;
    TZrBool hasMatchingImport = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || moduleName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        project_navigation_refine_import_binding_target_locations(snapshot.content,
                                                                  snapshot.contentLength,
                                                                  &bindings);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    }
    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->moduleName, moduleName)) {
            hasMatchingImport = ZR_TRUE;
            break;
        }
    }

    if (!hasMatchingImport) {
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        return ZR_TRUE;
    }

    appended = ZrLanguageServer_LspProject_AppendMatchingImportTargetLocations(state,
                                                                               context,
                                                                               uri,
                                                                               &bindings,
                                                                               moduleName,
                                                                               result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

/* 项目级模块引用中的别名使用部分。 */
static TZrBool append_project_imported_module_references(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrLspProjectIndex *projectIndex,
                                                         SZrString *fallbackUri,
                                                         SZrString *moduleName,
                                                         SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               ZR_NULL,
                                                               project_navigation_append_imported_module_for_uri,
                                                               result);
}

/* 项目级模块引用中的别名声明部分。 */
static TZrBool append_project_import_binding_references(SZrState *state,
                                                        SZrLspContext *context,
                                                        SZrLspProjectIndex *projectIndex,
                                                        SZrString *fallbackUri,
                                                        SZrString *moduleName,
                                                        SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               ZR_NULL,
                                                               project_navigation_append_import_binding_for_uri,
                                                               result);
}

/* 项目级模块引用中的导入字符串部分。 */
static TZrBool append_project_import_target_references(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrLspProjectIndex *projectIndex,
                                                       SZrString *fallbackUri,
                                                       SZrString *moduleName,
                                                       SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               ZR_NULL,
                                                               project_navigation_append_import_target_for_uri,
                                                               result);
}

/* 源文件重命名借此取得所有项目文件中的 import 目标范围；调用方负责结果释放。 */
TZrBool ZrLanguageServer_LspProject_AppendProjectImportTargetReferences(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrString *fallbackUri,
        SZrString *moduleName,
        SZrArray *result) {
    return append_project_import_target_references(state,
                                                   context,
                                                   projectIndex,
                                                   fallbackUri,
                                                   moduleName,
                                                   result);
}

/* 插件文件本身缺少模块声明时，反查项目导入边并验证 native 路径身份。 */
static TZrBool project_navigation_resolve_descriptor_plugin_module_from_project(SZrState *state,
                                                                                SZrLspContext *context,
                                                                                SZrLspProjectIndex *projectIndex,
                                                                                SZrString *targetUri,
                                                                                SZrString **outModuleName) {
    TZrChar targetNativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool hasTargetNativePath;

    if (outModuleName != ZR_NULL) {
        *outModuleName = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || targetUri == ZR_NULL ||
        outModuleName == ZR_NULL) {
        return ZR_FALSE;
    }

    hasTargetNativePath = ZrLanguageServer_LspUri_FileToNativePath(targetUri, targetNativePath, sizeof(targetNativePath));
    for (TZrSize fileIndex = 0; fileIndex < projectIndex->files.length; fileIndex++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fileIndex);
        SZrSemanticAnalyzer *analyzer;
        SZrArray bindings;

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL) {
            continue;
        }

        analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, (*recordPtr)->uri);
        if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
            continue;
        }

        ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
        for (TZrSize bindingIndex = 0; bindingIndex < bindings.length; bindingIndex++) {
            SZrLspImportBinding **bindingPtr =
                (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, bindingIndex);
            SZrLspResolvedImportedModule resolved;
            SZrString *declarationUri = ZR_NULL;
            TZrChar expectedPluginPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            TZrBool expectedPluginMatches;

            if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL) {
                continue;
            }

            memset(&resolved, 0, sizeof(resolved));
            expectedPluginMatches =
                hasTargetNativePath &&
                project_navigation_build_descriptor_plugin_path(projectIndex,
                                                                (*bindingPtr)->moduleName,
                                                                expectedPluginPath,
                                                                sizeof(expectedPluginPath)) &&
                project_navigation_native_paths_equal(expectedPluginPath, targetNativePath);
            if (!ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         (*bindingPtr)->moduleName,
                                                                         &resolved) ||
                resolved.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN ||
                resolved.sourceRecord != ZR_NULL) {
                continue;
            }

            if (!expectedPluginMatches &&
                (!ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(state,
                                                                           projectIndex,
                                                                           (*bindingPtr)->moduleName,
                                                                           &declarationUri) ||
                 declarationUri == ZR_NULL ||
                 !ZrLanguageServer_Lsp_UrisResolveToSameNativePath(declarationUri, targetUri))) {
                continue;
            }

            *outModuleName = (*bindingPtr)->moduleName;
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return ZR_TRUE;
        }

        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    }

    return ZR_FALSE;
}

/* 把跨文件引用结果收窄为当前文档高亮；只拷贝坐标，临时 Location 仍由调用方持有。 */
static TZrBool append_locations_as_document_highlights(SZrState *state,
                                                       SZrArray *locations,
                                                       SZrString *uri,
                                                       TZrInt32 kind,
                                                       SZrArray *result) {
    if (state == ZR_NULL || locations == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrLspDocumentHighlight *highlight;

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL || (*locationPtr)->uri == ZR_NULL ||
            !ZrLanguageServer_Lsp_UrisResolveToSameNativePath((*locationPtr)->uri, uri)) {
            continue;
        }

        highlight =
            (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
        if (highlight == ZR_NULL) {
            return ZR_FALSE;
        }

        highlight->range = (*locationPtr)->range;
        highlight->kind = kind;
        ZrCore_Array_Push(state, result, &highlight);
    }

    return result->length > 0;
}

/* 声明本身的高亮仍要按元数据来源转换坐标，kind 由上层决定。 */
static TZrBool append_document_highlight(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri,
                                         SZrArray *result,
                                         SZrFileRange range,
                                         TZrInt32 kind,
                                         EZrLspImportedModuleSourceKind sourceKind) {
    SZrLspDocumentHighlight *highlight;

    if (state == ZR_NULL || result == ZR_NULL || range.source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight == ZR_NULL) {
        return ZR_FALSE;
    }

    if (sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA) {
        if (!ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates(context,
                                                                        uri,
                                                                        range,
                                                                        &highlight->range)) {
            ZrCore_Memory_RawFree(state->global, highlight, sizeof(SZrLspDocumentHighlight));
            return ZR_FALSE;
        }
    } else if (sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
        if (!ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates(range, &highlight->range)) {
            ZrCore_Memory_RawFree(state->global, highlight, sizeof(SZrLspDocumentHighlight));
            return ZR_FALSE;
        }
    } else {
        highlight->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, range);
    }
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

/* 从二进制导出表按光标坐标识别成员声明；成员位置不匹配时不提供外部声明。 */
static TZrBool project_navigation_try_find_binary_export_declaration_at(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *uri,
    SZrLspPosition position,
    SZrString **outMemberName,
    SZrFileRange *outRange) {
    SZrIoSource *binarySource = ZR_NULL;
    SZrFilePosition filePosition;
    SZrFileRange positionRange;

    if (outMemberName != ZR_NULL) {
        *outMemberName = ZR_NULL;
    }
    if (outRange != ZR_NULL) {
        *outRange = project_navigation_metadata_file_entry_range(uri);
    }
    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || uri == ZR_NULL ||
        outMemberName == ZR_NULL || outRange == ZR_NULL ||
        !ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(state, projectIndex, moduleName, &binarySource) ||
        binarySource == ZR_NULL || binarySource->modulesLength == 0 || binarySource->modules == ZR_NULL ||
        binarySource->modules[0].entryFunction == ZR_NULL ||
        binarySource->modules[0].entryFunction->typedExportedSymbols == ZR_NULL) {
        if (binarySource != ZR_NULL) {
            ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(state->global, binarySource);
        }
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates(context,
                                                                            uri,
                                                                            position,
                                                                            &filePosition)) {
        ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(state->global, binarySource);
        return ZR_FALSE;
    }
    positionRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    for (TZrSize index = 0; index < binarySource->modules[0].entryFunction->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol =
            &binarySource->modules[0].entryFunction->typedExportedSymbols[index];
        SZrFileRange symbolRange;
        const TZrChar *symbolNameText;

        if (symbol->name == ZR_NULL ||
            !project_navigation_binary_export_symbol_try_range(uri, symbol, &symbolRange)) {
            continue;
        }
        if (!project_navigation_file_range_contains_position(symbolRange, positionRange)) {
            continue;
        }

        symbolNameText = project_navigation_string_text(symbol->name);
        *outMemberName = symbolNameText != ZR_NULL
                             ? ZrCore_String_Create(state, (TZrNativeString)symbolNameText, strlen(symbolNameText))
                             : ZR_NULL;
        *outRange = symbolRange;
        ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(state->global, binarySource);
        return *outMemberName != ZR_NULL;
    }

    ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(state->global, binarySource);
    return ZR_FALSE;
}

/* native 虚拟文档以描述符为声明权威，命中字段或方法后交给引用查询。 */
static TZrBool project_navigation_try_find_descriptor_plugin_member_declaration_at(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *uri,
    SZrLspPosition position,
    SZrString **outMemberName,
    SZrFileRange *outRange) {
    const ZrLibModuleDescriptor *descriptor = ZR_NULL;
    EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    SZrLspVirtualDeclarationMatch match;
    const TZrChar *moduleText;
    TZrChar moduleNameBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outMemberName != ZR_NULL) {
        *outMemberName = ZR_NULL;
    }
    if (outRange != ZR_NULL) {
        *outRange = project_navigation_metadata_file_entry_range(uri);
    }
    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || uri == ZR_NULL ||
        outMemberName == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleText = project_navigation_string_text(moduleName);
    moduleNameBuffer[0] = '\0';
    if (!ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(state,
                                                                      context,
                                                                      projectIndex,
                                                                      uri,
                                                                      &descriptor,
                                                                      &sourceKind,
                                                                      moduleNameBuffer,
                                                                      sizeof(moduleNameBuffer)) ||
        descriptor == ZR_NULL) {
        descriptor = moduleText != ZR_NULL
                         ? ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state,
                                                                                            moduleText,
                                                                                            &sourceKind)
                         : ZR_NULL;
    }
    if (descriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&match, 0, sizeof(match));
    if (!ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(state,
                                                                        descriptor,
                                                                        uri,
                                                                        position,
                                                                        &match) ||
        (match.kind != ZR_LSP_VIRTUAL_DECLARATION_FIELD &&
         match.kind != ZR_LSP_VIRTUAL_DECLARATION_METHOD) ||
        match.name == ZR_NULL) {
        return ZR_FALSE;
    }

    *outMemberName = ZrCore_String_Create(state, (TZrNativeString)match.name, strlen(match.name));
    if (*outMemberName == ZR_NULL) {
        return ZR_FALSE;
    }

    *outRange = match.range;
    return ZR_TRUE;
}

/* 将描述符里的原生名称转为语义引用查询的模块/成员键。 */
static TZrBool project_append_imported_references_for_native_name(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *queryUri,
                                                                  SZrString *moduleName,
                                                                  const TZrChar *memberName,
                                                                  SZrArray *result) {
    SZrString *memberNameString;

    if (state == ZR_NULL || memberName == ZR_NULL || memberName[0] == '\0') {
        return ZR_FALSE;
    }

    memberNameString = ZrCore_String_Create(state, (TZrNativeString)memberName, strlen(memberName));
    if (memberNameString == ZR_NULL) {
        return ZR_FALSE;
    }

    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               queryUri,
                                                               moduleName,
                                                               memberNameString,
                                                               project_navigation_append_semantic_imported_member_for_uri,
                                                               result);
}

/* 在插件模块入口查询时覆盖所有公开成员，含链接、常量、函数与类型。 */
static TZrBool project_append_descriptor_plugin_entry_member_references(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        const SZrLspExternalMetadataDeclaration *resolved,
                                                                        SZrString *queryUri,
                                                                        SZrArray *result) {
    const ZrLibModuleDescriptor *descriptor = ZR_NULL;
    EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    TZrChar moduleNameBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || resolved == ZR_NULL || result == ZR_NULL ||
        resolved->projectIndex == ZR_NULL || resolved->moduleName == ZR_NULL ||
        resolved->declarationUri == ZR_NULL ||
        resolved->sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
        return ZR_FALSE;
    }

    moduleNameBuffer[0] = '\0';
    if (!ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(state,
                                                                       context,
                                                                       resolved->projectIndex,
                                                                       resolved->declarationUri,
                                                                       &descriptor,
                                                                       &sourceKind,
                                                                       moduleNameBuffer,
                                                                       sizeof(moduleNameBuffer)) ||
        descriptor == ZR_NULL) {
        const TZrChar *moduleText = project_navigation_string_text(resolved->moduleName);
        descriptor = moduleText != ZR_NULL
                         ? ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state,
                                                                                            moduleText,
                                                                                            &sourceKind)
                         : ZR_NULL;
    }

    if (descriptor == ZR_NULL || sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        appended = project_append_imported_references_for_native_name(state,
                                                                      context,
                                                                      resolved->projectIndex,
                                                                      queryUri,
                                                                      resolved->moduleName,
                                                                      descriptor->moduleLinks[index].name,
                                                                      result) ||
                   appended;
    }
    for (TZrSize index = 0; index < descriptor->constantCount; index++) {
        appended = project_append_imported_references_for_native_name(state,
                                                                      context,
                                                                      resolved->projectIndex,
                                                                      queryUri,
                                                                      resolved->moduleName,
                                                                      descriptor->constants[index].name,
                                                                      result) ||
                   appended;
    }
    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        appended = project_append_imported_references_for_native_name(state,
                                                                      context,
                                                                      resolved->projectIndex,
                                                                      queryUri,
                                                                      resolved->moduleName,
                                                                      descriptor->functions[index].name,
                                                                      result) ||
                   appended;
    }
    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        appended = project_append_imported_references_for_native_name(state,
                                                                      context,
                                                                      resolved->projectIndex,
                                                                      queryUri,
                                                                      resolved->moduleName,
                                                                      descriptor->types[index].name,
                                                                      result) ||
                   appended;
    }

    return appended;
}

/* 语义查询从源码/二进制/插件文档反向解析外部声明，结果供定义、引用和高亮共用。 */
/* 调用约束：仅在返回成功且 hasDeclaration 为真时消费 declarationUri/range；失败可能留下部分结果。 */
TZrBool ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspProjectResolvedExternalMetadataDeclaration *outResolved) {
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *sourceRecord;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar moduleNameBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(uri)) {
        SZrLspVirtualDocumentIdentity identity;
        const ZrLibModuleDescriptor *descriptor;
        SZrLspVirtualDeclarationMatch match;
        if (!ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeDescriptor(
                    state, context, uri, &identity, &projectIndex, &descriptor) ||
            !ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(
                    state, descriptor, uri, position, &match)) {
            return ZR_FALSE;
        }
        if (match.kind != ZR_LSP_VIRTUAL_DECLARATION_MODULE) {
            if (match.name == ZR_NULL) {
                return ZR_FALSE;
            }
            outResolved->memberName = ZrCore_String_Create(state,
                    (TZrNativeString)match.name, strlen(match.name));
            if (outResolved->memberName == ZR_NULL) {
                return ZR_FALSE;
            }
        }
        outResolved->projectIndex = projectIndex;
        outResolved->moduleName = identity.moduleName;
        outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN;
        outResolved->declarationUri = uri;
        outResolved->declarationRange = match.range;
        outResolved->hasDeclaration = ZR_TRUE;
        return ZR_TRUE;
    }

    projectIndex = ZrLanguageServer_LspProject_GetOrCreateForUri(state, context, uri);
    if (projectIndex == ZR_NULL || !ZrLanguageServer_LspUri_FileToNativePath(uri, nativePath, sizeof(nativePath))) {
        return ZR_FALSE;
    }
    if (!projectIndex->hasSemanticProjectLoad && !projectIndex->hasLightweightSourceGraph) {
        ZrLanguageServer_LspProject_EnsureScannedSourceGraph(state, context, projectIndex);
    }

    sourceRecord = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri);
    if (sourceRecord != ZR_NULL &&
        sourceRecord->moduleName != ZR_NULL &&
        project_navigation_position_is_module_entry(position)) {
        outResolved->projectIndex = projectIndex;
        outResolved->moduleName = sourceRecord->moduleName;
        outResolved->sourceKind = sourceRecord->isFfiWrapperSource
                                      ? ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER
                                      : ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
        outResolved->declarationUri = sourceRecord->uri != ZR_NULL ? sourceRecord->uri : uri;
        outResolved->declarationRange =
            project_navigation_metadata_file_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
        return ZR_TRUE;
    }

    if (ZrLanguageServer_LspProject_DeriveBinaryModuleNameFromPath(projectIndex,
                                                                   nativePath,
                                                                   moduleNameBuffer,
                                                                   sizeof(moduleNameBuffer))) {
        SZrString *binaryDeclarationUri = ZR_NULL;

        outResolved->projectIndex = projectIndex;
        outResolved->moduleName = ZrCore_String_CreateFromNative(state, moduleNameBuffer);
        outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA;
        outResolved->hasDeclaration = outResolved->moduleName != ZR_NULL &&
                                      ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(state,
                                                                                                projectIndex,
                                                                                                outResolved->moduleName,
                                                                                                &binaryDeclarationUri) &&
                                      binaryDeclarationUri != ZR_NULL &&
                                      ZrLanguageServer_Lsp_UrisResolveToSameNativePath(binaryDeclarationUri, uri);
        if (!outResolved->hasDeclaration) {
            return ZR_FALSE;
        }

        outResolved->declarationUri = binaryDeclarationUri;
        outResolved->declarationRange = project_navigation_metadata_file_entry_range(outResolved->declarationUri);
        if (!project_navigation_position_is_module_entry(position)) {
            if (!project_navigation_try_find_binary_export_declaration_at(state,
                                                                          context,
                                                                          projectIndex,
                                                                          outResolved->moduleName,
                                                                          outResolved->declarationUri,
                                                                          position,
                                                                          &outResolved->memberName,
                                                                          &outResolved->declarationRange)) {
                return ZR_FALSE;
            }
            outResolved->hasDeclaration = outResolved->memberName != ZR_NULL;
        }
        return ZR_TRUE;
    }

    if (project_navigation_resolve_descriptor_plugin_module_from_project(state,
                                                                         context,
                                                                         projectIndex,
                                                                         uri,
                                                                         &outResolved->moduleName)) {
        outResolved->projectIndex = projectIndex;
        outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN;
        outResolved->declarationUri = uri;
        outResolved->declarationRange = project_navigation_metadata_file_entry_range(uri);
        outResolved->hasDeclaration = outResolved->moduleName != ZR_NULL;
        if (outResolved->hasDeclaration && !project_navigation_position_is_module_entry(position)) {
            if (!project_navigation_try_find_descriptor_plugin_member_declaration_at(state,
                                                                                     context,
                                                                                     projectIndex,
                                                                                     outResolved->moduleName,
                                                                                     uri,
                                                                                     position,
                                                                                     &outResolved->memberName,
                                                                                     &outResolved->declarationRange)) {
                return ZR_FALSE;
            }
            outResolved->hasDeclaration = outResolved->memberName != ZR_NULL;
        }
        return outResolved->hasDeclaration;
    }

    if (state->global != ZR_NULL) {
        ZrLibRegisteredModuleInfo moduleInfo;

        memset(&moduleInfo, 0, sizeof(moduleInfo));
        if (ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(state->global, nativePath, &moduleInfo) &&
            moduleInfo.moduleName != ZR_NULL &&
            (moduleInfo.registrationKind == ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN ||
             moduleInfo.isDescriptorPlugin)) {
            outResolved->projectIndex = projectIndex;
            outResolved->moduleName = ZrCore_String_Create(state,
                                                           (TZrNativeString)moduleInfo.moduleName,
                                                           strlen(moduleInfo.moduleName));
            outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN;
            outResolved->declarationUri = uri;
            outResolved->declarationRange = project_navigation_metadata_file_entry_range(uri);
            outResolved->hasDeclaration = outResolved->moduleName != ZR_NULL;
            if (outResolved->hasDeclaration && !project_navigation_position_is_module_entry(position)) {
                if (!project_navigation_try_find_descriptor_plugin_member_declaration_at(state,
                                                                                         context,
                                                                                         projectIndex,
                                                                                         outResolved->moduleName,
                                                                                         uri,
                                                                                         position,
                                                                                         &outResolved->memberName,
                                                                                         &outResolved->declarationRange)) {
                    return ZR_FALSE;
                }
                outResolved->hasDeclaration = outResolved->memberName != ZR_NULL;
            }
            return outResolved->hasDeclaration;
        }
    }

    return ZR_FALSE;
}

/* 把外部声明映射到项目 import 目标、别名和成员使用；includeDeclaration 控制声明是否入结果。 */
/* TODO: 多个 append 分支用 || 合并“有结果”和“成功”语义；核查分配失败时是否会被已有结果掩盖。 */
TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationReferences(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    TZrBool includeDeclaration,
    SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || resolved == ZR_NULL || result == ZR_NULL ||
        resolved->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (includeDeclaration &&
        resolved->hasDeclaration &&
        resolved->declarationUri != ZR_NULL &&
        !append_lsp_location(state,
                             context,
                             result,
                             resolved->declarationUri,
                             resolved->declarationRange,
                             resolved->sourceKind)) {
        return ZR_FALSE;
    }

    if (resolved->memberName != ZR_NULL) {
        appended = append_project_imported_references(state,
                                                      context,
                                                      resolved->projectIndex,
                                                      queryUri,
                                                      resolved->moduleName,
                                                      resolved->memberName,
                                                      result);
    } else {
        appended = append_project_import_target_references(state,
                                                           context,
                                                           resolved->projectIndex,
                                                           queryUri,
                                                           resolved->moduleName,
                                                           result);
        appended = append_project_import_binding_references(state,
                                                            context,
                                                            resolved->projectIndex,
                                                            queryUri,
                                                            resolved->moduleName,
                                                            result) || appended;
        appended = append_project_imported_module_references(state,
                                                             context,
                                                             resolved->projectIndex,
                                                             queryUri,
                                                             resolved->moduleName,
                                                             result) || appended;
        appended = project_append_descriptor_plugin_entry_member_references(state,
                                                                            context,
                                                                            resolved,
                                                                            queryUri,
                                                                            result) ||
                   appended;
    }

    return appended || result->length > 0;
}

/* 高亮复用引用收集，只保留 queryUri 对应文档并分别标出声明与引用。 */
/* BUG: append_lsp_location 为每个引用分配 SZrLspLocation，结尾只 Array_Free 指针数组；每次高亮查询都会泄漏所收集的 Location。 */
TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationHighlights(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    SZrArray *result) {
    SZrArray locations;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || resolved == ZR_NULL || queryUri == ZR_NULL || result == ZR_NULL ||
        resolved->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (resolved->hasDeclaration &&
        resolved->declarationUri != ZR_NULL &&
        ZrLanguageServer_Lsp_UrisResolveToSameNativePath(resolved->declarationUri, queryUri)) {
        appended = append_document_highlight(state,
                                             context,
                                             queryUri,
                                             result,
                                             resolved->declarationRange,
                                             3,
                                             resolved->sourceKind);
    }

    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (resolved->memberName != ZR_NULL) {
        appended = append_project_imported_references(state,
                                                      context,
                                                      resolved->projectIndex,
                                                      queryUri,
                                                      resolved->moduleName,
                                                      resolved->memberName,
                                                      &locations) || appended;
    } else {
        appended = append_project_import_target_references(state,
                                                           context,
                                                           resolved->projectIndex,
                                                           queryUri,
                                                           resolved->moduleName,
                                                           &locations) || appended;
        appended = append_project_import_binding_references(state,
                                                            context,
                                                            resolved->projectIndex,
                                                            queryUri,
                                                            resolved->moduleName,
                                                            &locations) || appended;
        appended = append_project_imported_module_references(state,
                                                             context,
                                                             resolved->projectIndex,
                                                             queryUri,
                                                             resolved->moduleName,
                                                             &locations) || appended;
    }

    if (locations.length > 0) {
        appended = append_locations_as_document_highlights(state, &locations, queryUri, 2, result) || appended;
    }

    ZrCore_Array_Free(state, &locations);
    return appended;
}
