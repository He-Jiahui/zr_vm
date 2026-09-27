#include "interface/lsp_interface_internal.h"
#include "project/lsp_project_internal.h"
#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_language_server/lsp_semantic_snapshot.h"
#include "zr_vm_language_server/lsp_uri.h"
#include "lsp_virtual_documents.h"
#include "metadata/lsp_metadata_provider.h"
#include "metadata/lsp_virtual_document_identity.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/project_imports.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ZR_VM_PLATFORM_IS_WIN
#include <windows.h>
#else
#include <dirent.h>
#endif

#define ZR_LSP_PROJECT_HEX_DIGIT_INVALID ((TZrInt32)-1)

/* 同步 Analyze 期间临时替换全局 sourceLoader；保存原回调及其 userData 以便项目解析失败时回退。
 * 此结构在调用栈上，不能被异步加载器留存。 */
typedef struct SZrLspProjectSourceLoaderContext {
    SZrLspProjectIndex *projectIndex;
    FZrIoLoadSource fallbackSourceLoader;
    TZrPtr fallbackSourceLoaderUserData;
    TZrPtr fallbackUserData;
} SZrLspProjectSourceLoaderContext;

/* 语义分析器回调借用本次请求的项目作用域，分析结束后必须清空回调以免悬垂。 */
typedef struct SZrLspProjectVirtualDeclarationUriResolverContext {
    SZrState *state;
    SZrLspContext *context;
    SZrLspProjectIndex *projectIndex;
} SZrLspProjectVirtualDeclarationUriResolverContext;

/* 语义分析器只需要可导航的虚拟声明 URI；在本次同步分析期间借用解析上下文，
 * 由元数据提供者把外部来源映射回当前项目的声明视图。 */
static SZrString *project_resolve_virtual_declaration_uri(
        SZrSemanticContext *semanticContext,
        SZrString *externalOriginUri,
        TZrPtr userData) {
    SZrLspProjectVirtualDeclarationUriResolverContext *resolverContext =
            (SZrLspProjectVirtualDeclarationUriResolverContext *)userData;
    SZrLspMetadataProvider provider;
    SZrLspResolvedImportedModuleEntry resolved;

    ZR_UNUSED_PARAMETER(semanticContext);
    if (resolverContext == ZR_NULL || resolverContext->state == ZR_NULL ||
        resolverContext->context == ZR_NULL || externalOriginUri == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLanguageServer_LspMetadataProvider_Init(
            &provider, resolverContext->state, resolverContext->context);
    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(
                &provider,
                ZR_NULL,
                resolverContext->projectIndex,
                externalOriginUri,
                &resolved) ||
        !resolved.hasDeclaration) {
        return ZR_NULL;
    }

    if (resolved.virtualDeclarationUri != ZR_NULL &&
        ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(resolved.virtualDeclarationUri)) {
        return resolved.virtualDeclarationUri;
    }
    if (resolved.declarationUri == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(resolved.declarationUri)) {
        return ZR_NULL;
    }
    return resolved.declarationUri;
}

static TZrBool project_refresh_for_updated_document_internal(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrString *uri,
                                                             const TZrChar *content,
                                                             TZrSize contentLength,
                                                             TZrBool rescanAllLoadedSources,
                                                             TZrBool advanceProviderGeneration);

/* 为项目路径和模块键提供保留长度的只读视图，返回指针由 VM 字符串持有。 */
static void get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

/* 仅供需要 NUL 结尾文本的项目路径接口使用；调用方不得保留返回指针越过字符串生命周期。 */
static const TZrChar *get_string_text(SZrString *value) {
    TZrNativeString text;
    TZrSize length;

    get_string_view(value, &text, &length);
    return text;
}

/* 把文件系统返回的暂存文本复制进 VM 字符串，以便索引长期引用。 */
static SZrString *create_string_from_const_text(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

/* 导入规范化依赖当前项目作为解析器上下文；调用期间临时安装项目并在返回前恢复。
 * 调用方须持有同步分析范围内的 state 和 AST。 */
static TZrBool project_canonicalize_ast_for_path(SZrState *state,
                                                 SZrLspProjectIndex *projectIndex,
                                                 SZrAstNode *ast,
                                                 const TZrChar *path,
                                                 SZrString **outCurrentModuleKey,
                                                 TZrChar *errorBuffer,
                                                 TZrSize errorBufferSize,
                                                 SZrFileRange *outErrorLocation) {
    SZrString *pathString;
    TZrPtr previousUserData;
    TZrBool success;

    if (outCurrentModuleKey != ZR_NULL) {
        *outCurrentModuleKey = ZR_NULL;
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (outErrorLocation != ZR_NULL) {
        memset(outErrorLocation, 0, sizeof(*outErrorLocation));
    }

    if (state == ZR_NULL || state->global == ZR_NULL || projectIndex == ZR_NULL || projectIndex->project == ZR_NULL ||
        ast == ZR_NULL || path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    pathString = create_string_from_const_text(state, path);
    if (pathString == ZR_NULL) {
        return ZR_FALSE;
    }

    previousUserData = state->global->userData;
    state->global->userData = projectIndex->project;
    success = ZrParser_ProjectImports_CanonicalizeAst(state,
                                                      ast,
                                                      pathString,
                                                      outCurrentModuleKey,
                                                      errorBuffer,
                                                      errorBufferSize,
                                                      outErrorLocation);
    state->global->userData = previousUserData;
    return success;
}

/* 文件记录优先采用 AST 规范化后的模块身份，磁盘扫描无法取得 AST 时再由路径推导。 */
static TZrBool project_determine_source_module_key(SZrState *state,
                                                   SZrLspProjectIndex *projectIndex,
                                                   const TZrChar *path,
                                                   SZrAstNode *ast,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize) {
    SZrString *currentModuleKey = ZR_NULL;
    const TZrChar *moduleKeyText;
    TZrChar importError[ZR_PARSER_ERROR_BUFFER_LENGTH];
    SZrFileRange importErrorLocation;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }

    if (projectIndex == ZR_NULL || projectIndex->project == ZR_NULL || path == ZR_NULL || buffer == ZR_NULL ||
        bufferSize == 0) {
        return ZR_FALSE;
    }

    if (ast != ZR_NULL &&
        project_canonicalize_ast_for_path(state,
                                          projectIndex,
                                          ast,
                                          path,
                                          &currentModuleKey,
                                          importError,
                                          sizeof(importError),
                                          &importErrorLocation)) {
        moduleKeyText = get_string_text(currentModuleKey);
        if (moduleKeyText != ZR_NULL &&
            ZrLibrary_Project_NormalizeModuleKey(moduleKeyText, buffer, bufferSize)) {
            return ZR_TRUE;
        }
    }

    return ZrLibrary_Project_DeriveCurrentModuleKey(projectIndex->project,
                                                    path,
                                                    ZR_NULL,
                                                    buffer,
                                                    bufferSize,
                                                    ZR_NULL,
                                                    0);
}

/* 按进程首次查询固定追踪开关，供本模块的热路径避免重复读取环境。 */
static TZrBool lsp_project_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_LSP_PROJECT_TRACE");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

/* 将项目装载和增量刷新决策写到 stderr，帮助诊断跨文件失效链路。 */
static void lsp_project_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!lsp_project_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fflush(stderr);
}

static void project_index_free(SZrState *state, SZrLspProjectIndex *projectIndex);
static TZrBool discover_project_path_for_uri(SZrString *uri,
                                             TZrChar *projectPath,
                                             TZrSize projectPathSize,
                                             TZrBool *outAmbiguous);
static TZrBool discover_project_path_with_context(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  TZrChar *projectPath,
                                                  TZrSize projectPathSize,
                                                  TZrBool *outAmbiguous);
static TZrBool project_resolve_source_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize);
static TZrBool project_resolve_binary_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize);
static TZrBool project_scan_source_module_graph(SZrState *state,
                                                SZrLspContext *context,
                                                SZrLspProjectIndex *projectIndex,
                                                SZrString *moduleName);

/* 向 VM 源加载器交付已由项目解析器定位的文件流；成功后流由 SZrIo 的关闭回调接管。 */
static TZrBool project_load_resolved_file_to_io(SZrState *state,
                                                const TZrChar *path,
                                                TZrBool isBinary,
                                                SZrIo *io) {
    SZrLibrary_File_Reader *reader;

    if (state == ZR_NULL || path == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    reader = ZrLibrary_File_OpenRead(state->global, (TZrNativeString)path, isBinary);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io->isBinary = isBinary;
    return ZR_TRUE;
}

/* 项目候选未命中时恢复原加载器的 userData 语境，保持外层注册的源加载行为。 */
static TZrBool project_source_loader_invoke_fallback(SZrState *state,
                                                     SZrLspProjectSourceLoaderContext *context,
                                                     TZrNativeString sourcePath,
                                                     TZrNativeString md5,
                                                     SZrIo *io) {
    TZrPtr previousUserData;
    TZrPtr previousSourceLoaderUserData;
    TZrBool success;

    if (state == ZR_NULL || state->global == ZR_NULL || context == ZR_NULL || context->fallbackSourceLoader == ZR_NULL) {
        return ZR_FALSE;
    }

    previousUserData = state->global->userData;
    previousSourceLoaderUserData = state->global->sourceLoaderUserData;
    state->global->userData = context->fallbackUserData;
    state->global->sourceLoaderUserData = context->fallbackSourceLoaderUserData;
    success = context->fallbackSourceLoader(state, sourcePath, md5, io);
    state->global->userData = previousUserData;
    state->global->sourceLoaderUserData = previousSourceLoaderUserData;
    return success;
}

/* 语义分析期间优先按项目 source/binary 根解析导入；未命中才交还原加载器。
 * 此回调依赖栈上的 loader context，只能在安装它的同步 Analyze 调用内使用。 */
static TZrBool project_source_loader(SZrState *state,
                                     TZrNativeString sourcePath,
                                     TZrNativeString md5,
                                     SZrIo *io) {
    SZrLspProjectSourceLoaderContext *context;
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || state->global == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    context = (SZrLspProjectSourceLoaderContext *)state->global->sourceLoaderUserData;
    if (context == ZR_NULL || context->projectIndex == ZR_NULL || sourcePath == ZR_NULL) {
        return project_source_loader_invoke_fallback(state, context, sourcePath, md5, io);
    }

    if (project_resolve_source_path(context->projectIndex, sourcePath, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return project_load_resolved_file_to_io(state, resolvedPath, ZR_FALSE, io);
    }

    if (project_resolve_binary_path(context->projectIndex, sourcePath, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return project_load_resolved_file_to_io(state, resolvedPath, ZR_TRUE, io);
    }
    return project_source_loader_invoke_fallback(state, context, sourcePath, md5, io);
}

/* 先读取 AST 导入并注册项目 native 描述插件，使后续语义分析能解析插件声明。 */
static void project_preload_descriptor_plugin_imports(SZrState *state,
                                                      SZrLspProjectIndex *projectIndex,
                                                      SZrAstNode *ast) {
    SZrArray bindings;
    const TZrChar *projectDirectory;

    if (state == ZR_NULL || projectIndex == ZR_NULL || ast == ZR_NULL || state->global == ZR_NULL ||
        projectIndex->projectRootPath == ZR_NULL) {
        return;
    }

    projectDirectory = get_string_text(projectIndex->projectRootPath);
    if (projectDirectory == ZR_NULL || projectDirectory[0] == '\0') {
        return;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, ast, &bindings);
    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr =
                (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        const TZrChar *moduleNameText;

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
            continue;
        }

        moduleNameText = get_string_text((*bindingPtr)->moduleName);
        if (moduleNameText == ZR_NULL || moduleNameText[0] == '\0') {
            continue;
        }

        ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(state, projectDirectory, moduleNameText);
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
}

/* 轻量源码图不一定有 AST；用已发现的模块名提前准备相同的 native 插件。 */
static void project_preload_descriptor_plugin_import_names(SZrState *state,
                                                           SZrLspProjectIndex *projectIndex,
                                                           SZrArray *moduleNames) {
    const TZrChar *projectDirectory;

    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleNames == ZR_NULL || state->global == ZR_NULL ||
        projectIndex->projectRootPath == ZR_NULL) {
        return;
    }

    projectDirectory = get_string_text(projectIndex->projectRootPath);
    if (projectDirectory == ZR_NULL || projectDirectory[0] == '\0') {
        return;
    }

    for (TZrSize index = 0; index < moduleNames->length; index++) {
        SZrString **moduleNamePtr = (SZrString **)ZrCore_Array_Get(moduleNames, index);
        const TZrChar *moduleNameText;

        if (moduleNamePtr == ZR_NULL || *moduleNamePtr == ZR_NULL) {
            continue;
        }

        moduleNameText = get_string_text(*moduleNamePtr);
        if (moduleNameText == ZR_NULL || moduleNameText[0] == '\0') {
            continue;
        }

        ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(state, projectDirectory, moduleNameText);
    }
}

/* 统一调用文件库的项目根拼接接口，结果必须由调用方提供最大路径缓冲区。 */
static void path_join_const_inputs(const TZrChar *path1, const TZrChar *path2, TZrChar *result) {
    ZrLibrary_File_PathJoin((TZrNativeString)path1, (TZrNativeString)path2, result);
}

/* URI 的文件类型分流只比较字节后缀，调用方需先排除虚拟 URI。 */
static TZrBool string_ends_with(SZrString *value, const TZrChar *suffix) {
    TZrNativeString text;
    TZrSize length;
    TZrSize suffixLength;

    if (suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    get_string_view(value, &text, &length);
    suffixLength = strlen(suffix);
    return text != ZR_NULL && length >= suffixLength &&
           memcmp(text + length - suffixLength, suffix, suffixLength) == 0;
}

/* 监听事件用此后缀识别需失效的 native 插件文件。 */
static TZrBool native_path_has_dynamic_library_extension(const TZrChar *path) {
    TZrSize length;

    if (path == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(path);
    return (length >= 4 && strcmp(path + length - 4, ".dll") == 0) ||
           (length >= 3 && strcmp(path + length - 3, ".so") == 0) ||
           (length >= 6 && strcmp(path + length - 6, ".dylib") == 0);
}

static void normalize_path_for_compare(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize);

/* 文件监听与项目归属比较使用同一形式：分隔符统一，Windows 上折叠大小写。 */
static void normalize_path_for_compare(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; path[index] != '\0' && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = path[index];
        if (current == '\\') {
            current = '/';
        }
#ifdef ZR_VM_PLATFORM_IS_WIN
        current = (TZrChar)tolower((unsigned char)current);
#endif
        buffer[writeIndex++] = current;
    }

    while (writeIndex > 1 && buffer[writeIndex - 1] == '/') {
        writeIndex--;
    }
    buffer[writeIndex] = '\0';
}

/* 用路径段边界判定项目归属，供多项目选择、监听重载和已装载源枚举共享。 */
static TZrBool native_path_is_within_directory(const TZrChar *path, const TZrChar *directory) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize directoryLength;

    normalize_path_for_compare(path, normalizedPath, sizeof(normalizedPath));
    normalize_path_for_compare(directory, normalizedDirectory, sizeof(normalizedDirectory));
    directoryLength = strlen(normalizedDirectory);

    if (directoryLength == 0 || strncmp(normalizedPath, normalizedDirectory, directoryLength) != 0) {
        return ZR_FALSE;
    }

    return normalizedPath[directoryLength] == '\0' || normalizedPath[directoryLength] == '/';
}

/* 复用项目解析器的模块键规范，避免导航和加载器使用不同模块命名。 */
static TZrBool normalize_module_key(const TZrChar *modulePath, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLibrary_Project_NormalizeModuleKey(modulePath, buffer, bufferSize);
}

/* 仅将顶层 extern 声明视为 FFI 源包装器，供导航选择源码或元数据路径。 */
static TZrBool project_script_contains_top_level_ffi_wrapper(SZrAstNode *ast) {
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL || ast->data.script.statements->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < ast->data.script.statements->count; index++) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement == ZR_NULL) {
            continue;
        }

        if (statement->type == ZR_AST_EXTERN_BLOCK) {
            return ZR_TRUE;
        }

        if (statement->type == ZR_AST_COMPILE_TIME_DECLARATION &&
            statement->data.compileTimeDeclaration.declaration != ZR_NULL &&
            statement->data.compileTimeDeclaration.declaration->type == ZR_AST_EXTERN_BLOCK) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 以源码标记辅助识别 FFI 包装器，轻量扫描在没有可靠 AST 时也能使用。
 * BUG: 普通注释或字符串只要包含 native extern 也会命中，登记为 FFI 源并影响导航来源；
 * 触发链为 project_register_source_record -> lsp_project_navigation.c 的 sourceKind 分流。 */
static TZrBool project_content_contains_ffi_wrapper_marker(const TZrChar *content) {
    return content != ZR_NULL && strstr(content, "native extern") != ZR_NULL;
}

/* 导入图按语义字符串去重，避免同一模块被重复预装载或递归扫描。 */
static TZrBool project_string_array_contains(SZrArray *values, SZrString *needle) {
    if (values == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < values->length; index++) {
        SZrString **valuePtr = (SZrString **)ZrCore_Array_Get(values, index);
        if (valuePtr != ZR_NULL && *valuePtr != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(*valuePtr, needle)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 复制导入名到本轮扫描数组；调用方负责释放数组，字符串由 VM 状态管理。 */
static TZrBool project_append_unique_import_module_name(SZrState *state,
                                                        SZrArray *moduleNames,
                                                        SZrString *moduleName) {
    TZrNativeString text;
    TZrSize length;
    SZrString *copy;

    if (state == ZR_NULL || moduleNames == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!moduleNames->isValid) {
        ZrCore_Array_Init(state, moduleNames, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    if (project_string_array_contains(moduleNames, moduleName)) {
        return ZR_TRUE;
    }

    get_string_view(moduleName, &text, &length);
    if (text == ZR_NULL || length == 0) {
        return ZR_FALSE;
    }

    copy = ZrCore_String_Create(state, text, length);
    if (copy == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, moduleNames, &copy);
    return ZR_TRUE;
}

/* 优先沿真实 AST 导入绑定枚举依赖，避免文本匹配误认非代码内容。 */
static TZrBool project_collect_import_module_names_from_ast(SZrState *state,
                                                            SZrAstNode *ast,
                                                            SZrArray *moduleNames) {
    SZrArray bindings;
    TZrBool success = ZR_TRUE;

    if (state == ZR_NULL || ast == ZR_NULL || moduleNames == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, ast, &bindings);
    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr = (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
            continue;
        }

        if (!project_append_unique_import_module_name(state, moduleNames, (*bindingPtr)->moduleName)) {
            success = ZR_FALSE;
            break;
        }
    }

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return success;
}

/* 文本回退路径只定位 import( 形态，不保证位于代码区；供无 AST 时尽量发现依赖。 */
static const TZrChar *project_find_import_call(const TZrChar *content, const TZrChar *cursor) {
    const TZrChar *match;

    if (content == ZR_NULL || cursor == ZR_NULL) {
        return ZR_NULL;
    }

    while ((match = strstr(cursor, "import")) != ZR_NULL) {
        const TZrChar *scan = match + strlen("import");

        if ((match > content && (isalnum((unsigned char)match[-1]) || match[-1] == '_' || match[-1] == '.')) ||
            isalnum((unsigned char)*scan) || *scan == '_') {
            cursor = scan;
            continue;
        }
        while (*scan != '\0' && isspace((unsigned char)*scan)) {
            scan++;
        }
        if (*scan == '(') {
            return match;
        }
        cursor = scan;
    }

    return ZR_NULL;
}

/* 无 AST 时为轻量引用图保留可发现的字面量导入。
 * TODO: 扫描未跳过注释和字符串，需核对解析失败场景下的误报如何影响诊断与跨文件引用。 */
static TZrBool project_collect_import_module_names_from_text(SZrState *state,
                                                             const TZrChar *content,
                                                             SZrArray *moduleNames) {
    const TZrChar *cursor;

    if (state == ZR_NULL || content == ZR_NULL || moduleNames == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!moduleNames->isValid) {
        ZrCore_Array_Init(state, moduleNames, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    cursor = content;
    while ((cursor = project_find_import_call(content, cursor)) != ZR_NULL) {
        const TZrChar *scan = cursor + strlen("import");
        const TZrChar *start;
        SZrString *moduleName;

        while (*scan != '\0' && isspace((unsigned char)*scan)) {
            scan++;
        }
        if (*scan != '(') {
            cursor = scan;
            continue;
        }

        scan++;
        while (*scan != '\0' && isspace((unsigned char)*scan)) {
            scan++;
        }
        if (*scan != '"') {
            cursor = scan;
            continue;
        }

        scan++;
        start = scan;
        while (*scan != '\0' && *scan != '"') {
            if (*scan == '\\' && scan[1] != '\0') {
                scan += 2;
            } else {
                scan++;
            }
        }

        if (*scan != '"' || scan == start) {
            cursor = start;
            continue;
        }

        moduleName = ZrCore_String_Create(state, (TZrNativeString)start, (TZrSize)(scan - start));
        if (moduleName == ZR_NULL || !project_append_unique_import_module_name(state, moduleNames, moduleName)) {
            return ZR_FALSE;
        }

        cursor = scan + 1;
    }

    return ZR_TRUE;
}

/* 在 AST 和源码文本之间选择依赖发现路径，轻量扫描与引用查询共用此口径。 */
static TZrBool project_collect_import_module_names(SZrState *state,
                                                   SZrAstNode *ast,
                                                   const TZrChar *content,
                                                   SZrArray *moduleNames) {
    if (ast != ZR_NULL) {
        return project_collect_import_module_names_from_ast(state, ast, moduleNames);
    }

    return project_collect_import_module_names_from_text(state, content, moduleNames);
}

/* 监听文件失效时从项目源码路径反推模块键，再清除 VM 中对应的缓存别名。 */
static TZrBool derive_module_name_from_path(SZrLspProjectIndex *projectIndex,
                                            const TZrChar *path,
                                            TZrChar *buffer,
                                            TZrSize bufferSize) {
    if (projectIndex == ZR_NULL || projectIndex->project == ZR_NULL || path == ZR_NULL || buffer == ZR_NULL ||
        bufferSize == 0) {
        return ZR_FALSE;
    }

    return ZrLibrary_Project_DeriveCurrentModuleKey(projectIndex->project,
                                                    path,
                                                    ZR_NULL,
                                                    buffer,
                                                    bufferSize,
                                                    ZR_NULL,
                                                    0);
}

/* 项目 binary 根由描述文件目录和输出配置共同决定，供二进制加载与监听失效使用。 */
static TZrBool project_binary_root_path(SZrLspProjectIndex *projectIndex,
                                        TZrChar *buffer,
                                        TZrSize bufferSize) {
    const TZrChar *projectDirectory;
    const TZrChar *projectBinary;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }

    if (projectIndex == ZR_NULL || projectIndex->project == ZR_NULL || projectIndex->project->directory == ZR_NULL ||
        projectIndex->project->binary == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    projectDirectory = get_string_text(projectIndex->project->directory);
    projectBinary = get_string_text(projectIndex->project->binary);
    if (projectDirectory == ZR_NULL || projectBinary == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(projectDirectory, projectBinary, buffer);
    return buffer[0] != '\0';
}

/* 从 binary 根内的产物路径恢复模块键，供虚拟导航与缓存失效共享。
 * TODO: 当前仅检查字节前缀，/out 与 /outside 会混淆；核对旁路产物能否误删同名 VM 缓存。 */
static TZrBool derive_binary_module_name_from_path(SZrLspProjectIndex *projectIndex,
                                                   const TZrChar *path,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;
    const TZrChar *relative;

    if (projectIndex == ZR_NULL || path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !project_binary_root_path(projectIndex, binaryRoot, sizeof(binaryRoot))) {
        return ZR_FALSE;
    }

    normalize_path_for_compare(path, normalizedPath, sizeof(normalizedPath));
    normalize_path_for_compare(binaryRoot, normalizedRoot, sizeof(normalizedRoot));
    rootLength = strlen(normalizedRoot);
    if (rootLength == 0 || strncmp(normalizedPath, normalizedRoot, rootLength) != 0) {
        return ZR_FALSE;
    }

    relative = normalizedPath + rootLength;
    while (*relative == '/') {
        relative++;
    }

    return normalize_module_key(relative, buffer, bufferSize);
}

/* 对项目导航暴露二进制文件到模块身份的映射，后续还须核对实际产物 URI。 */
TZrBool ZrLanguageServer_LspProject_DeriveBinaryModuleNameFromPath(SZrLspProjectIndex *projectIndex,
                                                                   const TZrChar *path,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize) {
    return derive_binary_module_name_from_path(projectIndex, path, buffer, bufferSize);
}

/* 监听事件按一个模块键清除 VM 缓存，字符串键为本次调用临时创建。 */
static void project_remove_module_cache_key_text(SZrState *state, const TZrChar *cacheKeyText) {
    SZrString *cacheKey;

    if (state == ZR_NULL || cacheKeyText == ZR_NULL || cacheKeyText[0] == '\0') {
        return;
    }

    cacheKey = ZrCore_String_Create(state, (TZrNativeString)cacheKeyText, strlen(cacheKeyText));
    if (cacheKey != ZR_NULL) {
        ZrCore_Module_RemoveFromCache(state, cacheKey);
    }
}

/* VM 缓存可能以原路径、规范路径或异种分隔符为键，监听失效需逐一移除。 */
static void project_remove_path_cache_key_variants(SZrState *state, const TZrChar *path) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar separatorVariant[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool hasSeparatorVariant = ZR_FALSE;

    if (state == ZR_NULL || path == ZR_NULL || path[0] == '\0') {
        return;
    }

    project_remove_module_cache_key_text(state, path);
    if (ZrLibrary_File_NormalizePath((TZrNativeString)path, normalizedPath, sizeof(normalizedPath)) &&
        strcmp(normalizedPath, path) != 0) {
        project_remove_module_cache_key_text(state, normalizedPath);
    }

    for (TZrSize index = 0; path[index] != '\0' && index + 1 < sizeof(separatorVariant); index++) {
        separatorVariant[index] = path[index] == '\\' ? '/' : (path[index] == '/' ? '\\' : path[index]);
        separatorVariant[index + 1] = '\0';
        if (separatorVariant[index] != path[index]) {
            hasSeparatorVariant = ZR_TRUE;
        }
    }

    if (hasSeparatorVariant && strcmp(separatorVariant, path) != 0) {
        project_remove_module_cache_key_text(state, separatorVariant);
        if (ZrLibrary_File_NormalizePath(separatorVariant, normalizedPath, sizeof(normalizedPath)) &&
            strcmp(normalizedPath, separatorVariant) != 0) {
            project_remove_module_cache_key_text(state, normalizedPath);
        }
    }
}

/* 文件变更同时撤销路径键与推导出的模块键，保证重载使用新产物。 */
static void project_invalidate_module_cache_for_watched_path(SZrState *state,
                                                             SZrLspProjectIndex *projectIndex,
                                                             const TZrChar *affectedPath) {
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || affectedPath == ZR_NULL) {
        return;
    }

    project_remove_path_cache_key_variants(state, affectedPath);

    if (projectIndex == ZR_NULL) {
        return;
    }

    if (derive_module_name_from_path(projectIndex, affectedPath, moduleName, sizeof(moduleName))) {
        project_remove_module_cache_key_text(state, moduleName);
        if (project_resolve_source_path(projectIndex, moduleName, resolvedPath, sizeof(resolvedPath))) {
            project_remove_path_cache_key_variants(state, resolvedPath);
        }
    }

    if (derive_binary_module_name_from_path(projectIndex, affectedPath, moduleName, sizeof(moduleName))) {
        project_remove_module_cache_key_text(state, moduleName);
        if (project_resolve_binary_path(projectIndex, moduleName, resolvedPath, sizeof(resolvedPath))) {
            project_remove_path_cache_key_variants(state, resolvedPath);
        }
    }
}

/* 项目源加载器把模块键映射到 .zr；保留 $ 包引用的项目解析器优先级。 */
static TZrBool project_resolve_source_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    if (moduleName != ZR_NULL && moduleName[0] == '$' && projectIndex != ZR_NULL && projectIndex->project != ZR_NULL &&
        ZrLibrary_Project_ResolveSourcePath(projectIndex->project, moduleName, buffer, bufferSize)) {
        return ZR_TRUE;
    }

    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize writeIndex = 0;

    if (projectIndex == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !normalize_module_key(moduleName, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; normalizedModule[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = normalizedModule[index] == '/' ? ZR_SEPARATOR : normalizedModule[index];
    }

    if (writeIndex + 4 >= sizeof(relativePath)) {
        return ZR_FALSE;
    }

    memcpy(relativePath + writeIndex, ".zr", 4);
    writeIndex += 3;
    relativePath[writeIndex] = '\0';
    path_join_const_inputs(get_string_text(projectIndex->sourceRootPath), relativePath, buffer);
    return buffer[0] != '\0';
}

/* 源文件缺席时回退已存在的 .zro 产物，确保编辑器可分析依赖的二进制声明。 */
static TZrBool project_resolve_binary_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    if (moduleName != ZR_NULL && moduleName[0] == '$' && projectIndex != ZR_NULL && projectIndex->project != ZR_NULL &&
        ZrLibrary_Project_ResolveBinaryPath(projectIndex->project, moduleName, buffer, bufferSize)) {
        return ZrLibrary_File_Exist(buffer) == ZR_LIBRARY_FILE_IS_FILE;
    }

    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize writeIndex = 0;
    TZrSize baseLength;

    if (projectIndex == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !normalize_module_key(moduleName, normalizedModule, sizeof(normalizedModule)) ||
        !project_binary_root_path(projectIndex, binaryRoot, sizeof(binaryRoot))) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; normalizedModule[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = normalizedModule[index] == '/' ? ZR_SEPARATOR : normalizedModule[index];
    }

    baseLength = writeIndex;
    if (baseLength + 5 >= sizeof(relativePath)) {
        return ZR_FALSE;
    }

    memcpy(relativePath + baseLength, ".zro", 5);
    path_join_const_inputs(binaryRoot, relativePath, buffer);
    return ZrLibrary_File_Exist(buffer) == ZR_LIBRARY_FILE_IS_FILE;
}

/* 为导航和缓存刷新返回项目拥有的文件记录；返回指针随索引移除而失效。 */
SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByUri(SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri) {
    for (TZrSize index = 0; projectIndex != ZR_NULL && index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL &&
            (ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->uri, uri) ||
             ZrLanguageServer_LspUri_Equivalent((*recordPtr)->uri, uri))) {
            return *recordPtr;
        }
    }

    return ZR_NULL;
}

/* 装载与轻量扫描按模块键查重；返回记录仍由项目索引持有。 */
SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByModuleName(SZrLspProjectIndex *projectIndex,
                                                                            SZrString *moduleName) {
    for (TZrSize index = 0; projectIndex != ZR_NULL && index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->moduleName, moduleName)) {
            return *recordPtr;
        }
    }

    return ZR_NULL;
}

/* 诊断收集按 URI 等价关系去重，避免多个索引共同拥有文件时重复发布。 */
static TZrBool project_uri_array_contains(const SZrArray *uris, SZrString *uri) {
    for (TZrSize index = 0U; uris != ZR_NULL && index < uris->length; index++) {
        SZrString *const *existing = (SZrString *const *)ZrCore_Array_Get((SZrArray *)uris, index);
        if (existing != ZR_NULL && *existing != ZR_NULL &&
            (ZrLanguageServer_Lsp_StringsEqual(*existing, uri) ||
             ZrLanguageServer_LspUri_Equivalent(*existing, uri))) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* stdio/WASM 诊断入口需要项目图和打开文档的并集；先确保每个项目能提供轻量源码图。
 * 返回数组由调用方持有，元素 URI 仍由项目记录或解析器持有。 */
TZrBool ZrLanguageServer_LspProject_CollectDiagnosticDocumentUris(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrArray *outUris) {
    if (state == ZR_NULL || context == ZR_NULL || outUris == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!outUris->isValid) {
        ZrCore_Array_Init(state, outUris, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    if (outUris->elementSize != sizeof(SZrString *)) {
        return ZR_FALSE;
    }

    for (TZrSize projectIndexIndex = 0U;
         projectIndexIndex < context->projectIndexes.length;
         projectIndexIndex++) {
        SZrLspProjectIndex *const *projectIndexPtr = (SZrLspProjectIndex *const *)ZrCore_Array_Get(
                &context->projectIndexes, projectIndexIndex);
        SZrLspProjectIndex *projectIndex = projectIndexPtr != ZR_NULL ? *projectIndexPtr : ZR_NULL;

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            return ZR_FALSE;
        }
        if (projectIndex == ZR_NULL) {
            continue;
        }
        if (!ZrLanguageServer_LspProject_EnsureScannedSourceGraph(state, context, projectIndex)) {
            return ZR_FALSE;
        }
        for (TZrSize fileIndex = 0U; fileIndex < projectIndex->files.length; fileIndex++) {
            SZrLspProjectFileRecord *const *recordPtr = (SZrLspProjectFileRecord *const *)ZrCore_Array_Get(
                    &projectIndex->files, fileIndex);
            SZrLspProjectFileRecord *record = recordPtr != ZR_NULL ? *recordPtr : ZR_NULL;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                return ZR_FALSE;
            }
            if (record == ZR_NULL || record->uri == ZR_NULL || project_uri_array_contains(outUris, record->uri)) {
                continue;
            }
            ZrCore_Array_Push(state, outUris, &record->uri);
        }
    }

    if (context->parser == ZR_NULL || !context->parser->uriToFileMap.isValid ||
        context->parser->uriToFileMap.buckets == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize bucketIndex = 0U; bucketIndex < context->parser->uriToFileMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = context->parser->uriToFileMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            SZrFileVersion *fileVersion = pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER
                                               ? (SZrFileVersion *)pair->value.value.nativeObject.nativePointer
                                               : ZR_NULL;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                return ZR_FALSE;
            }
            if (fileVersion != ZR_NULL && fileVersion->isOpenDocument && fileVersion->uri != ZR_NULL &&
                !project_uri_array_contains(outUris, fileVersion->uri)) {
                ZrCore_Array_Push(state, outUris, &fileVersion->uri);
            }
            pair = pair->next;
        }
    }
    return ZR_TRUE;
}

/* 虚拟 URI 作用域和项目移除按描述文件 URI 精确定位索引，而非按目录推断。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectByProjectUri(SZrLspContext *context,
                                                                        SZrString *uri,
                                                                        TZrSize *outIndex) {
    for (TZrSize index = 0; context != ZR_NULL && index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL &&
            (ZrLanguageServer_Lsp_StringsEqual((*projectPtr)->projectFileUri, uri) ||
             ZrLanguageServer_LspUri_Equivalent((*projectPtr)->projectFileUri, uri))) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return *projectPtr;
        }
    }

    return ZR_NULL;
}

/* 多项目环境优先选最深的 source/project 根，虚拟文档则用其显式作用域。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectForUri(SZrLspContext *context, SZrString *uri) {
    TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrLspProjectIndex *bestProject = ZR_NULL;
    TZrSize bestRootLength = 0;

    if (context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    if (ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(uri)) {
        return ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(context, uri);
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*projectPtr)->projectFileUri, uri)) {
            return *projectPtr;
        }
    }

    if (!ZrLanguageServer_LspUri_FileToNativePath(uri, pathBuffer, sizeof(pathBuffer))) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        const TZrChar *sourceRoot;
        const TZrChar *projectRoot;
        TZrSize sourceRootLength;
        TZrSize projectRootLength;

        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        sourceRoot = get_string_text((*projectPtr)->sourceRootPath);
        sourceRootLength = sourceRoot != ZR_NULL ? strlen(sourceRoot) : 0;
        if (native_path_is_within_directory(pathBuffer, sourceRoot) &&
            (bestProject == ZR_NULL || sourceRootLength > bestRootLength)) {
            bestProject = *projectPtr;
            bestRootLength = sourceRootLength;
        }

        projectRoot = get_string_text((*projectPtr)->projectRootPath);
        projectRootLength = projectRoot != ZR_NULL ? strlen(projectRoot) : 0;
        if (native_path_is_within_directory(pathBuffer, projectRoot) &&
            (bestProject == ZR_NULL || projectRootLength > bestRootLength)) {
            bestProject = *projectPtr;
            bestRootLength = projectRootLength;
        }
    }

    return bestProject;
}

/* 文件删除后仅释放索引记录；记录内字符串仍由 VM 状态管理，数组位置随之压缩。 */
static void project_remove_file_record_at_index(SZrState *state,
                                                SZrLspProjectIndex *projectIndex,
                                                TZrSize recordIndex) {
    SZrLspProjectFileRecord **recordPtr;

    if (state == ZR_NULL || projectIndex == ZR_NULL || recordIndex >= projectIndex->files.length) {
        return;
    }

    recordPtr = (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, recordIndex);
    if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, *recordPtr, sizeof(SZrLspProjectFileRecord));
    }

    if (recordIndex + 1 < projectIndex->files.length) {
        memmove(projectIndex->files.head + recordIndex * projectIndex->files.elementSize,
                projectIndex->files.head + (recordIndex + 1) * projectIndex->files.elementSize,
                (projectIndex->files.length - recordIndex - 1) * projectIndex->files.elementSize);
    }
    projectIndex->files.length--;
}

/* watcher 删除事件先试 URI 等价，再按本地规范路径查找原项目索引。 */
static TZrBool project_find_index_by_project_uri(SZrLspContext *context,
                                                 SZrString *uri,
                                                 TZrSize *outProjectIndexOffset) {
    TZrSize projectIndexOffset;
    SZrLspProjectIndex **projectPtr;
    TZrChar targetPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTargetPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (context == ZR_NULL || uri == ZR_NULL || outProjectIndexOffset == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspProject_FindProjectByProjectUri(context, uri, &projectIndexOffset) == ZR_NULL) {
        if (!ZrLanguageServer_LspUri_FileToNativePath(uri, targetPath, sizeof(targetPath))) {
            return ZR_FALSE;
        }

        normalize_path_for_compare(targetPath, normalizedTargetPath, sizeof(normalizedTargetPath));
        for (projectIndexOffset = 0; projectIndexOffset < context->projectIndexes.length; projectIndexOffset++) {
            TZrChar normalizedProjectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            projectPtr = (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndexOffset);
            if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL || (*projectPtr)->projectFilePath == ZR_NULL) {
                continue;
            }

            normalize_path_for_compare(get_string_text((*projectPtr)->projectFilePath),
                                       normalizedProjectPath,
                                       sizeof(normalizedProjectPath));
            if (strcmp(normalizedTargetPath, normalizedProjectPath) == 0) {
                break;
            }
        }

        if (projectIndexOffset >= context->projectIndexes.length) {
            return ZR_FALSE;
        }
    }

    *outProjectIndexOffset = projectIndexOffset;
    return ZR_TRUE;
}

/* 移除一个项目时，共享源的解析器/分析器状态仍由其他项目使用，不能一并撤销。 */
static TZrBool project_uri_is_referenced_by_other_index(SZrLspContext *context,
                                                         TZrSize excludedProjectIndexOffset,
                                                         SZrString *uri) {
    if (context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize projectIndexOffset = 0; projectIndexOffset < context->projectIndexes.length;
         projectIndexOffset++) {
        SZrLspProjectIndex **projectPtr;

        if (projectIndexOffset == excludedProjectIndexOffset) {
            continue;
        }

        projectPtr = (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndexOffset);
        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        if (ZrLanguageServer_LspProject_FindRecordByUri(*projectPtr, uri) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 工作区移除项目时同步撤销其文件缓存；保留打开的编辑覆盖层供其他入口继续分析。 */
static TZrBool project_remove_index_at(SZrState *state,
                                       SZrLspContext *context,
                                       TZrSize projectIndexOffset,
                                       TZrBool preserveOpenDocuments) {
    SZrLspProjectIndex **projectPtr;

    if (state == ZR_NULL || context == ZR_NULL || projectIndexOffset >= context->projectIndexes.length) {
        return ZR_FALSE;
    }

    projectPtr = (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndexOffset);
    if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL) {
        for (TZrSize recordIndex = 0; recordIndex < (*projectPtr)->files.length; recordIndex++) {
            SZrLspProjectFileRecord **recordPtr =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(&(*projectPtr)->files, recordIndex);
            if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL && (*recordPtr)->uri != ZR_NULL) {
                SZrFileVersion *fileVersion =
                    ZrLanguageServer_Lsp_GetDocumentFileVersion(context, (*recordPtr)->uri);
                TZrBool retainOpenOverlay = preserveOpenDocuments && fileVersion != ZR_NULL &&
                                             fileVersion->isOpenDocument;

                if (!retainOpenOverlay &&
                    !project_uri_is_referenced_by_other_index(
                            context, projectIndexOffset, (*recordPtr)->uri)) {
                    ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, (*recordPtr)->uri);
                    if (context->parser != ZR_NULL) {
                        ZrLanguageServer_IncrementalParser_RemoveFile(state, context->parser, (*recordPtr)->uri);
                    }
                }
            }
        }
        project_index_free(state, *projectPtr);
    }

    if (projectIndexOffset + 1 < context->projectIndexes.length) {
        memmove(context->projectIndexes.head + projectIndexOffset * context->projectIndexes.elementSize,
                context->projectIndexes.head + (projectIndexOffset + 1) * context->projectIndexes.elementSize,
                (context->projectIndexes.length - projectIndexOffset - 1) * context->projectIndexes.elementSize);
    }
    context->projectIndexes.length--;
    return ZR_TRUE;
}

/* 文件监听的项目删除事件释放对应索引以及不再共享的分析器与解析器状态。 */
TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri) {
    TZrSize projectIndexOffset;

    if (state == ZR_NULL || !project_find_index_by_project_uri(context, uri, &projectIndexOffset)) {
        return ZR_FALSE;
    }

    return project_remove_index_at(state, context, projectIndexOffset, ZR_FALSE);
}

/* 工作区配置移除项目时保留编辑器打开的文档覆盖层，避免未保存内容被磁盘状态替代。 */
TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUriPreservingOpenDocuments(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri) {
    TZrSize projectIndexOffset;

    if (state == ZR_NULL || !project_find_index_by_project_uri(context, uri, &projectIndexOffset)) {
        return ZR_FALSE;
    }

    return project_remove_index_at(state, context, projectIndexOffset, ZR_TRUE);
}

/* 文件删除或关闭时按 URI/规范路径摘除一份项目记录，并撤销该文件的分析缓存。
 * BUG: 同一个 URI 若同时登记在多个项目，stdio 的 watcher 删除事件只调用一次，
 * 其余项目仍保留旧记录；关闭文档入口则用循环重复调用。见 stdio_workspace_files.c 和 stdio_documents.c。 */
TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri) {
    TZrChar targetPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTargetPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_LspUri_FileToNativePath(uri, targetPath, sizeof(targetPath))) {
        targetPath[0] = '\0';
    } else {
        normalize_path_for_compare(targetPath, normalizedTargetPath, sizeof(normalizedTargetPath));
    }

    for (TZrSize projectIndexOffset = 0; projectIndexOffset < context->projectIndexes.length; projectIndexOffset++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndexOffset);
        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        for (TZrSize recordIndex = 0; recordIndex < (*projectPtr)->files.length; recordIndex++) {
            SZrLspProjectFileRecord **recordPtr =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(&(*projectPtr)->files, recordIndex);
            TZrBool pathMatched = ZR_FALSE;

            if (targetPath[0] != '\0' && recordPtr != ZR_NULL && *recordPtr != ZR_NULL && (*recordPtr)->path != ZR_NULL) {
                TZrChar normalizedRecordPath[ZR_LIBRARY_MAX_PATH_LENGTH];
                normalize_path_for_compare(get_string_text((*recordPtr)->path),
                                           normalizedRecordPath,
                                           sizeof(normalizedRecordPath));
                pathMatched = strcmp(normalizedTargetPath, normalizedRecordPath) == 0 ? ZR_TRUE : ZR_FALSE;
            }

            if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL &&
                (ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->uri, uri) || pathMatched)) {
                if ((*recordPtr)->uri != ZR_NULL) {
                    ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, (*recordPtr)->uri);
                    if (context->parser != ZR_NULL) {
                        ZrLanguageServer_IncrementalParser_RemoveFile(state, context->parser, (*recordPtr)->uri);
                    }
                }
                project_remove_file_record_at_index(state, *projectPtr, recordIndex);
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

/* 二进制或 native 插件监听事件选择最深归属项目，失效 VM 缓存并以磁盘 .zrp 重建分析图。
 * 适用于无编辑覆盖层的元数据变更；项目文件内容在返回前释放。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                                            SZrLspContext *context,
                                                                                            SZrString *uri) {
    TZrChar affectedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar discoveredProjectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrLspProjectIndex *bestProject = ZR_NULL;
    TZrSize bestRootLength = 0;
    const TZrChar *projectFilePath;
    SZrString *projectFileUri;
    TZrNativeString projectContent;
    TZrSize contentLength;
    TZrBool ambiguous = ZR_FALSE;
    TZrBool refreshed;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_LspUri_FileToNativePath(uri, affectedPath, sizeof(affectedPath))) {
        return ZR_FALSE;
    }

    if (state->global != ZR_NULL && native_path_has_dynamic_library_extension(affectedPath)) {
        ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(state->global, affectedPath);
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        const TZrChar *projectRoot;
        TZrSize projectRootLength;

        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL || (*projectPtr)->projectRootPath == ZR_NULL) {
            continue;
        }

        projectRoot = get_string_text((*projectPtr)->projectRootPath);
        if (!native_path_is_within_directory(affectedPath, projectRoot)) {
            continue;
        }

        projectRootLength = projectRoot != ZR_NULL ? strlen(projectRoot) : 0;
        if (bestProject == ZR_NULL || projectRootLength > bestRootLength) {
            bestProject = *projectPtr;
            bestRootLength = projectRootLength;
        }
    }

    project_invalidate_module_cache_for_watched_path(state, bestProject, affectedPath);

    if (bestProject != ZR_NULL && bestProject->projectFilePath != ZR_NULL && bestProject->projectFileUri != ZR_NULL) {
        projectFilePath = get_string_text(bestProject->projectFilePath);
        projectFileUri = bestProject->projectFileUri;
    } else {
        if (!discover_project_path_with_context(state,
                                                context,
                                                uri,
                                                discoveredProjectPath,
                                                sizeof(discoveredProjectPath),
                                                &ambiguous) ||
            ambiguous) {
            return ZR_FALSE;
        }

        projectFilePath = discoveredProjectPath;
        projectFileUri = ZrLanguageServer_LspUri_FromNativePath(state, discoveredProjectPath);
        if (projectFileUri == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    projectContent = ZrLibrary_File_ReadAll(state->global, (TZrNativeString)projectFilePath);
    if (projectContent == ZR_NULL) {
        return ZR_FALSE;
    }

    contentLength = strlen(projectContent);
    refreshed = project_refresh_for_updated_document_internal(state,
                                                              context,
                                                              projectFileUri,
                                                              projectContent,
                                                              contentLength,
                                                              ZR_TRUE,
                                                              ZR_TRUE);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  projectContent,
                                  contentLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return refreshed;
}

/* 索引独占文件记录、数组及 Project 对象；VM 字符串不在此逐个释放。 */
static void project_index_free(SZrState *state, SZrLspProjectIndex *projectIndex) {
    if (state == ZR_NULL || projectIndex == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *recordPtr, sizeof(SZrLspProjectFileRecord));
        }
    }

    ZrCore_Array_Free(state, &projectIndex->files);
    ZrCore_Array_Free(state, &projectIndex->activeModuleLoads);
    if (projectIndex->project != ZR_NULL) {
        ZrLibrary_Project_Free(state, projectIndex->project);
    }
    ZrCore_Memory_RawFree(state->global, projectIndex, sizeof(SZrLspProjectIndex));
}

/* 编辑覆盖层或 watcher 提供 .zrp 内容时建立项目索引，保留描述文件 URI 作为索引身份。
 * 返回索引在加入 context 前由调用方持有，失败时不得留下半成品。 */
static SZrLspProjectIndex *project_index_new_from_document(SZrState *state,
                                                           SZrString *projectUri,
                                                           const TZrChar *content,
                                                           TZrSize contentLength) {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceRootPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *jsonBuffer;
    SZrLibrary_Project *project;
    SZrLspProjectIndex *projectIndex;

    if (state == ZR_NULL || projectUri == ZR_NULL || content == ZR_NULL ||
        !ZrLanguageServer_LspUri_FileToNativePath(projectUri, projectPath, sizeof(projectPath))) {
        return ZR_NULL;
    }

    jsonBuffer = (TZrChar *)malloc(contentLength + 1);
    if (jsonBuffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(jsonBuffer, content, contentLength);
    jsonBuffer[contentLength] = '\0';
    project = ZrLibrary_Project_New(state, jsonBuffer, projectPath);
    free(jsonBuffer);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }

    path_join_const_inputs(get_string_text(project->directory), get_string_text(project->source), sourceRootPath);
    projectIndex = (SZrLspProjectIndex *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspProjectIndex));
    if (projectIndex == ZR_NULL) {
        ZrLibrary_Project_Free(state, project);
        return ZR_NULL;
    }

    projectIndex->project = project;
    projectIndex->projectFileUri = projectUri;
    projectIndex->projectFilePath = ZrCore_String_Create(state, projectPath, strlen(projectPath));
    projectIndex->projectRootPath = create_string_from_const_text(state, get_string_text(project->directory));
    projectIndex->sourceRootPath = ZrCore_String_Create(state, sourceRootPath, strlen(sourceRootPath));
    projectIndex->hasSemanticProjectLoad = ZR_FALSE;
    projectIndex->hasLightweightSourceGraph = ZR_FALSE;
    projectIndex->reverseDependencyPreservationCount = 0;
    projectIndex->reverseDependencyReanalysisCount = 0;
    projectIndex->lastReverseDependencyReanalysisCount = 0;
    projectIndex->publicContractHashMatchCount = 0;
    projectIndex->publicContractHashChangeCount = 0;
    projectIndex->publicContractHashUnavailableCount = 0;
    ZrCore_Array_Init(state,
                      &projectIndex->files,
                      sizeof(SZrLspProjectFileRecord *),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrCore_Array_Init(state, &projectIndex->activeModuleLoads,
                      sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    return projectIndex;
}

/* 按磁盘描述文件建立索引，供首次发现项目和按项目 URI 打开查询使用。 */
static SZrLspProjectIndex *project_index_new_from_path(SZrState *state, const TZrChar *projectPath) {
    TZrNativeString jsonBuffer;
    SZrString *projectUri;
    SZrLspProjectIndex *projectIndex;
    TZrSize contentLength;

    if (state == ZR_NULL || projectPath == ZR_NULL) {
        return ZR_NULL;
    }

    jsonBuffer = ZrLibrary_File_ReadAll(state->global, (TZrNativeString)projectPath);
    if (jsonBuffer == ZR_NULL) {
        return ZR_NULL;
    }

    contentLength = strlen(jsonBuffer);
    projectUri = ZrLanguageServer_LspUri_FromNativePath(state, projectPath);
    projectIndex = projectUri != ZR_NULL
                   ? project_index_new_from_document(state, projectUri, jsonBuffer, contentLength)
                   : ZR_NULL;
    ZrCore_Memory_RawFreeWithType(state->global,
                                  jsonBuffer,
                                  contentLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return projectIndex;
}

/* 项目自动发现沿物理目录向上爬升；到平台根目录即停止。 */
static TZrBool path_get_parent_directory_in_place(TZrChar *path) {
    TZrSize length;

    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    length = strlen(path);
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) {
#ifdef ZR_VM_PLATFORM_IS_WIN
        if (length == 3 && isalpha((unsigned char)path[0]) && path[1] == ':') {
            break;
        }
#endif
        path[--length] = '\0';
    }

    if (length == 1 && (path[0] == '/' || path[0] == '\\')) {
        return ZR_FALSE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (length == 3 && isalpha((unsigned char)path[0]) && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) {
        return ZR_FALSE;
    }
#endif

    while (length > 0 && path[length - 1] != '/' && path[length - 1] != '\\') {
        length--;
    }

    if (length == 0) {
        return ZR_FALSE;
    }

    if (length == 1) {
        path[1] = '\0';
        return ZR_TRUE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (length == 3 && isalpha((unsigned char)path[0]) && path[1] == ':') {
        path[3] = '\0';
        return ZR_TRUE;
    }
#endif

    path[length - 1] = '\0';
    return ZR_TRUE;
}

/* 自动发现只接受单一 .zrp 的目录，多于一个需由客户端显式选择。
 * BUG: POSIX 分支仅检查名称后缀，名为 *.zrp 的目录也计数，可能把唯一真实项目误判为歧义；
 * Windows 分支已有目录属性过滤，需在 POSIX 使用文件类型核验。 */
static TZrSize directory_count_project_files(const TZrChar *directory,
                                             TZrChar *firstProjectPath,
                                             TZrSize bufferSize) {
    TZrSize count = 0;

    if (directory == ZR_NULL) {
        return 0;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    TZrChar pattern[ZR_LIBRARY_MAX_PATH_LENGTH];
    WIN32_FIND_DATAA findData;
    HANDLE handle;

    ZrLibrary_File_PathJoin(directory, "*.zrp", pattern);
    handle = FindFirstFileA(pattern, &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        count++;
        if (count == 1 && firstProjectPath != ZR_NULL && bufferSize > 0) {
            ZrLibrary_File_PathJoin(directory, findData.cFileName, firstProjectPath);
        }
    } while (FindNextFileA(handle, &findData) != 0);

    FindClose(handle);
#else
    DIR *dir = opendir(directory);
    struct dirent *entry;

    if (dir == ZR_NULL) {
        return 0;
    }

    while ((entry = readdir(dir)) != ZR_NULL) {
        TZrSize nameLength;

        if (entry->d_name[0] == '\0') {
            continue;
        }

        nameLength = strlen(entry->d_name);
        if (nameLength < 4 || strcmp(entry->d_name + nameLength - 4, ".zrp") != 0) {
            continue;
        }

        count++;
        if (count == 1 && firstProjectPath != ZR_NULL && bufferSize > 0) {
            ZrLibrary_File_PathJoin((TZrNativeString)directory, entry->d_name, firstProjectPath);
        }
    }

    closedir(dir);
#endif

    return count;
}

/* 从文档目录向上找最近的唯一 .zrp；当前层多项目即报告歧义而不擅自越层选择。 */
static TZrBool discover_project_path_for_uri(SZrString *uri,
                                             TZrChar *projectPath,
                                             TZrSize projectPathSize,
                                             TZrBool *outAmbiguous) {
    TZrChar currentPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar currentDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize projectCount;

    if (projectPath != ZR_NULL && projectPathSize > 0) {
        projectPath[0] = '\0';
    }
    if (outAmbiguous != ZR_NULL) {
        *outAmbiguous = ZR_FALSE;
    }

    if (!ZrLanguageServer_LspUri_FileToNativePath(uri, currentPath, sizeof(currentPath)) ||
        !ZrLibrary_File_GetDirectory(currentPath, currentDirectory)) {
        return ZR_FALSE;
    }

    while (currentDirectory[0] != '\0') {
        projectCount = directory_count_project_files(currentDirectory, projectPath, projectPathSize);
        if (projectCount == 1) {
            return ZR_TRUE;
        }
        if (projectCount > 1) {
            if (outAmbiguous != ZR_NULL) {
                *outAmbiguous = ZR_TRUE;
            }
            return ZR_FALSE;
        }
        if (!path_get_parent_directory_in_place(currentDirectory)) {
            break;
        }
    }

    return ZR_FALSE;
}

/* 自动发现失败或歧义时，客户端选中的 .zrp 仅能接管其目录下文件。 */
static TZrBool discover_project_path_with_context(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  TZrChar *projectPath,
                                                  TZrSize projectPathSize,
                                                  TZrBool *outAmbiguous) {
    TZrBool ambiguous = ZR_FALSE;
    TZrChar fileNative[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar zrpDir[ZR_LIBRARY_MAX_PATH_LENGTH];

    ZR_UNUSED_PARAMETER(state);

    if (discover_project_path_for_uri(uri, projectPath, projectPathSize, &ambiguous)) {
        if (outAmbiguous != ZR_NULL) {
            *outAmbiguous = ambiguous;
        }
        return ZR_TRUE;
    }

    if (context != ZR_NULL && context->clientSelectedZrpNativePath != ZR_NULL &&
        ZrLanguageServer_LspUri_FileToNativePath(uri, fileNative, sizeof(fileNative)) &&
        ZrLibrary_File_GetDirectory(context->clientSelectedZrpNativePath, zrpDir) &&
        native_path_is_within_directory(fileNative, zrpDir)) {
        TZrSize zrpLength = strlen(context->clientSelectedZrpNativePath);

        if (zrpLength + 1 <= projectPathSize) {
            memcpy(projectPath, context->clientSelectedZrpNativePath, zrpLength + 1);
            if (outAmbiguous != ZR_NULL) {
                *outAmbiguous = ZR_FALSE;
            }
            return ZR_TRUE;
        }
    }

    if (outAmbiguous != ZR_NULL) {
        *outAmbiguous = ambiguous;
    }
    return ZR_FALSE;
}

/* .zrp 内容替换前清空旧索引记录关联的语义分析器，避免新配置复用旧项目语境。 */
static void project_invalidate_loaded_analyzers(SZrState *state,
                                                SZrLspContext *context,
                                                SZrLspProjectIndex *projectIndex) {
    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL) {
        return;
    }

    for (TZrSize recordIndex = 0; recordIndex < projectIndex->files.length; recordIndex++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, recordIndex);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL && (*recordPtr)->uri != ZR_NULL) {
            ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, (*recordPtr)->uri);
        }
    }
}

/* 编辑后或反向依赖刷新复用已解析 AST，临时安装项目 loader 与虚拟声明 URI 回调。
 * loader/回调上下文在栈上，Analyze 必须同步完成且恢复全局钩子后方可返回。 */
static TZrBool project_reanalyze_loaded_document(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrLspProjectIndex *projectIndex,
                                                 SZrString *uri,
                                                 TZrBool forceSemanticCacheReset) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;
    FZrIoLoadSource previousSourceLoader = ZR_NULL;
    TZrPtr previousUserData = ZR_NULL;
    TZrPtr previousSourceLoaderUserData = ZR_NULL;
    SZrLspProjectSourceLoaderContext sourceLoaderContext;
    SZrLspProjectVirtualDeclarationUriResolverContext virtualUriResolverContext;
    TZrBool analyzeSuccess;
    TZrSize currentAstHash;
    TZrBool sameAstAsCachedAnalysis;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    lsp_project_trace("[lsp_project] load imports uri=%s\n", get_string_text(uri));

    lsp_project_trace("[lsp_project] reanalyze begin uri=%s\n", get_string_text(uri));

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL) {
        lsp_project_trace("[lsp_project] reanalyze missing-ast uri=%s\n", get_string_text(uri));
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    currentAstHash = ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(fileVersion->ast);
    sameAstAsCachedAnalysis = analyzer->enableCache && analyzer->cache != ZR_NULL && analyzer->cache->isValid &&
                              analyzer->cache->astHash == currentAstHash;
    if (forceSemanticCacheReset || !sameAstAsCachedAnalysis ||
        fileVersion->lastParseMode == ZR_INCREMENTAL_PARSE_MODE_DECLARATION_REPARSE) {
        ZrLanguageServer_SemanticAnalyzer_ClearCache(state, analyzer);
    }
    project_preload_descriptor_plugin_imports(state, projectIndex, fileVersion->ast);
    virtualUriResolverContext.state = state;
    virtualUriResolverContext.context = context;
    virtualUriResolverContext.projectIndex = projectIndex;
    ZrLanguageServer_SemanticAnalyzer_SetVirtualDeclarationUriResolver(
            analyzer, project_resolve_virtual_declaration_uri, &virtualUriResolverContext);
    if (state->global != ZR_NULL) {
        sourceLoaderContext.projectIndex = projectIndex;
        sourceLoaderContext.fallbackSourceLoader = state->global->sourceLoader;
        previousUserData = state->global->userData;
        previousSourceLoaderUserData = state->global->sourceLoaderUserData;
        sourceLoaderContext.fallbackUserData = previousUserData;
        sourceLoaderContext.fallbackSourceLoaderUserData = previousSourceLoaderUserData;
        previousSourceLoader = state->global->sourceLoader;
        state->global->sourceLoaderUserData = &sourceLoaderContext;
        state->global->sourceLoader = project_source_loader;
    }
    analyzeSuccess = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, fileVersion->ast);
    ZrLanguageServer_SemanticAnalyzer_SetVirtualDeclarationUriResolver(
            analyzer, ZR_NULL, ZR_NULL);
    if (state->global != ZR_NULL) {
        state->global->userData = previousUserData;
        state->global->sourceLoaderUserData = previousSourceLoaderUserData;
        state->global->sourceLoader = previousSourceLoader;
    }

    if (!analyzeSuccess) {
        ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, uri);
    }

    lsp_project_trace("[lsp_project] reanalyze end uri=%s success=%d\n",
                      get_string_text(uri),
                      (int)analyzeSuccess);
    return analyzeSuccess;
}

/* LSP 文档更新入口以所在项目规范化导入，再执行语义分析；无项目时维持普通分析路径。 */
TZrBool ZrLanguageServer_Lsp_ProjectAnalyzeDocument(SZrState *state,
                                                    SZrLspContext *context,
                                                    SZrString *uri,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *ast) {
    SZrLspProjectIndex *projectIndex;
    FZrIoLoadSource previousSourceLoader = ZR_NULL;
    TZrPtr previousUserData = ZR_NULL;
    TZrPtr previousSourceLoaderUserData = ZR_NULL;
    SZrLspProjectSourceLoaderContext sourceLoaderContext;
    SZrLspProjectVirtualDeclarationUriResolverContext virtualUriResolverContext;
    TZrBool analyzeSuccess;
    TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar importError[ZR_PARSER_ERROR_BUFFER_LENGTH];
    SZrFileRange importErrorLocation;
    SZrString *currentModuleKey = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_SemanticAnalyzer_SetExternalProviderGeneration(
            state,
            analyzer,
            context->semanticSnapshotProviderGeneration);

    projectIndex = uri != ZR_NULL ? ZrLanguageServer_LspProject_FindProjectForUri(context, uri) : ZR_NULL;
    if (projectIndex == ZR_NULL || state->global == ZR_NULL) {
        virtualUriResolverContext.state = state;
        virtualUriResolverContext.context = context;
        virtualUriResolverContext.projectIndex = projectIndex;
        ZrLanguageServer_SemanticAnalyzer_SetVirtualDeclarationUriResolver(
                analyzer, project_resolve_virtual_declaration_uri, &virtualUriResolverContext);
        analyzeSuccess = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
        ZrLanguageServer_SemanticAnalyzer_SetVirtualDeclarationUriResolver(
                analyzer, ZR_NULL, ZR_NULL);
        return analyzeSuccess;
    }
    if (uri != ZR_NULL &&
        ZrLanguageServer_LspUri_FileToNativePath(uri, pathBuffer, sizeof(pathBuffer)) &&
        !project_canonicalize_ast_for_path(state,
                                           projectIndex,
                                           ast,
                                           pathBuffer,
                                           &currentModuleKey,
                                           importError,
                                           sizeof(importError),
                                           &importErrorLocation)) {
        lsp_project_trace("[lsp_project] canonicalize failed uri=%s error=%s\n",
                          get_string_text(uri),
                          importError[0] != '\0' ? importError : "unknown");
        return ZR_FALSE;
    }

    project_preload_descriptor_plugin_imports(state, projectIndex, ast);
    virtualUriResolverContext.state = state;
    virtualUriResolverContext.context = context;
    virtualUriResolverContext.projectIndex = projectIndex;
    ZrLanguageServer_SemanticAnalyzer_SetVirtualDeclarationUriResolver(
            analyzer, project_resolve_virtual_declaration_uri, &virtualUriResolverContext);
    sourceLoaderContext.projectIndex = projectIndex;
    sourceLoaderContext.fallbackSourceLoader = state->global->sourceLoader;
    previousUserData = state->global->userData;
    previousSourceLoaderUserData = state->global->sourceLoaderUserData;
    sourceLoaderContext.fallbackUserData = previousUserData;
    sourceLoaderContext.fallbackSourceLoaderUserData = previousSourceLoaderUserData;
    previousSourceLoader = state->global->sourceLoader;
    state->global->sourceLoaderUserData = &sourceLoaderContext;
    state->global->sourceLoader = project_source_loader;

    analyzeSuccess = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);

    ZrLanguageServer_SemanticAnalyzer_SetVirtualDeclarationUriResolver(
            analyzer, ZR_NULL, ZR_NULL);

    state->global->userData = previousUserData;
    state->global->sourceLoaderUserData = previousSourceLoaderUserData;
    state->global->sourceLoader = previousSourceLoader;
    if (analyzeSuccess && analyzer->compilerState != ZR_NULL && currentModuleKey != ZR_NULL) {
        analyzer->compilerState->currentModuleKey = currentModuleKey;
    }
    return analyzeSuccess;
}

/* 全量刷新只重分析当前项目 source 根内已进入增量解析器的 .zr 文档。 */
static TZrBool project_collect_loaded_source_uris(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrLspProjectIndex *projectIndex,
                                                  SZrArray *uris) {
    if (state == ZR_NULL || context == ZR_NULL || context->parser == ZR_NULL || projectIndex == ZR_NULL || uris == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!uris->isValid) {
        ZrCore_Array_Init(state, uris, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize bucketIndex = 0; bucketIndex < context->parser->uriToFileMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = context->parser->uriToFileMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                SZrFileVersion *fileVersion = (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;
                TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

                if (fileVersion != ZR_NULL &&
                    fileVersion->uri != ZR_NULL &&
                    string_ends_with(fileVersion->uri, ".zr") &&
                    ZrLanguageServer_LspUri_FileToNativePath(fileVersion->uri, pathBuffer, sizeof(pathBuffer)) &&
                    native_path_is_within_directory(pathBuffer, get_string_text(projectIndex->sourceRootPath))) {
                    SZrString *uri = fileVersion->uri;
                    ZrCore_Array_Push(state, uris, &uri);
                }
            }

            pair = pair->next;
        }
    }

    return ZR_TRUE;
}

/* 维护源码 URI、模块键和 FFI 分类以供导航及反向依赖查询；已有记录原位更新，
 * 公共契约哈希随后由 project_register_loaded_document 刷新。 */
static TZrBool project_register_source_record(SZrState *state,
                                              SZrLspProjectIndex *projectIndex,
                                              SZrString *uri,
                                              const TZrChar *path,
                                              SZrAstNode *ast,
                                              const TZrChar *content) {
    TZrChar moduleBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *pathString;
    SZrString *moduleString;
    SZrLspProjectFileRecord *record;
    TZrBool isFfiWrapperSource;

    if (state == ZR_NULL || projectIndex == ZR_NULL || uri == ZR_NULL || path == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!project_determine_source_module_key(state, projectIndex, path, ast, moduleBuffer, sizeof(moduleBuffer))) {
        return ZR_FALSE;
    }

    isFfiWrapperSource = project_script_contains_top_level_ffi_wrapper(ast) ||
                         project_content_contains_ffi_wrapper_marker(content);
    pathString = ZrCore_String_Create(state, (TZrNativeString)path, strlen(path));
    moduleString = ZrCore_String_Create(state, moduleBuffer, strlen(moduleBuffer));
    if (pathString == ZR_NULL || moduleString == ZR_NULL) {
        return ZR_FALSE;
    }

    record = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri);
    if (record == ZR_NULL) {
        record = (SZrLspProjectFileRecord *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspProjectFileRecord));
        if (record == ZR_NULL) {
            return ZR_FALSE;
        }

        record->uri = uri;
        record->path = pathString;
        record->moduleName = moduleString;
        record->isFfiWrapperSource = isFfiWrapperSource;
        record->publicContractHash = 0U;
        record->publicContractExportCount = 0U;
        record->hasPublicContractHash = ZR_FALSE;
        ZrCore_Array_Push(state, &projectIndex->files, &record);
        return ZR_TRUE;
    }

    record->uri = uri;
    record->path = pathString;
    record->moduleName = moduleString;
    record->isFfiWrapperSource = isFfiWrapperSource;
    return ZR_TRUE;
}

/* 轻量扫描重试前清空旧图，避免部分失败记录被误当作完整模块。 */
static void project_clear_file_records(SZrState *state, SZrLspProjectIndex *projectIndex) {
    if (state == ZR_NULL || projectIndex == ZR_NULL || !projectIndex->files.isValid) {
        return;
    }

    for (TZrSize index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *recordPtr, sizeof(SZrLspProjectFileRecord));
        }
    }

    projectIndex->files.length = 0;
}

/* 语义分析完成后把 AST 与编辑快照登记为项目记录，并保存当前公开契约摘要。 */
static TZrBool project_register_loaded_document(SZrState *state,
                                                SZrLspContext *context,
                                                SZrLspProjectIndex *projectIndex,
                                                SZrString *uri) {
    SZrSemanticAnalyzer *analyzer;
    TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const TZrChar *content = ZR_NULL;
    TZrBool hasSnapshot = ZR_FALSE;
    TZrBool registered;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_LspUri_FileToNativePath(uri, pathBuffer, sizeof(pathBuffer))) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        content = snapshot.content;
        hasSnapshot = ZR_TRUE;
    }

    registered = project_register_source_record(state, projectIndex, uri, pathBuffer, analyzer->ast, content);
    if (registered) {
        ZrLanguageServer_LspProject_UpdatePublicContractRecord(
                ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri),
                analyzer);
    }
    if (hasSnapshot) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    }
    return registered;
}

static TZrBool project_load_imports_from_uri(SZrState *state,
                                             SZrLspContext *context,
                                             SZrLspProjectIndex *projectIndex,
                                             SZrString *uri);

/* 语义项目图递归装载真实源文件，活动模块栈避免循环导入；优先复用分析器记录。
 * 读取磁盘只发生在解析器尚未持有该 URI 时，避免覆盖已打开文档。 */
static TZrBool project_ensure_module_loaded(SZrState *state,
                                            SZrLspContext *context,
                                            SZrLspProjectIndex *projectIndex,
                                            SZrString *moduleName) {
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *moduleUri;
    SZrFileVersion *fileVersion;
    SZrLspProjectFileRecord *record;
    SZrSemanticAnalyzer *analyzer;
    TZrNativeString sourceCode;
    TZrSize sourceLength;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    lsp_project_trace("[lsp_project] ensure module=%s\n", get_string_text(moduleName));

    record = ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, moduleName);
    if (record != ZR_NULL && record->uri != ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, record->uri);
        if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
            lsp_project_trace("[lsp_project] ensure cached module=%s\n", get_string_text(moduleName));
            return ZR_TRUE;
        }
    }

    if (!project_resolve_source_path(projectIndex, get_string_text(moduleName), resolvedPath, sizeof(resolvedPath)) ||
        ZrLibrary_File_Exist(resolvedPath) != ZR_LIBRARY_FILE_IS_FILE) {
        lsp_project_trace("[lsp_project] ensure no-source module=%s\n", get_string_text(moduleName));
        return ZR_FALSE;
    }

    moduleUri = ZrLanguageServer_LspUri_FromNativePath(state, resolvedPath);
    if (moduleUri == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < projectIndex->activeModuleLoads.length; index++) {
        SZrString **activeName = (SZrString **)ZrCore_Array_Get(
            &projectIndex->activeModuleLoads, index);
        if (activeName != ZR_NULL && *activeName != ZR_NULL &&
            ZrCore_String_Equal(*activeName, moduleName)) {
            return ZR_FALSE;
        }
    }
    ZrCore_Array_Push(state, &projectIndex->activeModuleLoads, &moduleName);

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, moduleUri);
    if (fileVersion == ZR_NULL) {
        sourceCode = ZrLibrary_File_ReadAll(state->global, resolvedPath);
        if (sourceCode == ZR_NULL) {
            goto cleanup;
        }

        sourceLength = strlen(sourceCode);
        if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state, context, moduleUri, sourceCode, sourceLength, 0, ZR_FALSE)) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceCode,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
            goto cleanup;
        }

        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceCode,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }

    if (!project_reanalyze_loaded_document(state, context, projectIndex, moduleUri, ZR_TRUE)) {
        goto cleanup;
    }

    success = project_register_loaded_document(state, context, projectIndex, moduleUri) &&
              project_load_imports_from_uri(state, context, projectIndex, moduleUri);
cleanup:
    projectIndex->activeModuleLoads.length--;
    return success;
}

/* 元数据提供者请求某个项目模块时复用项目入口的递归装载规则。 */
TZrBool ZrLanguageServer_LspProject_EnsureModuleLoadedByName(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrString *moduleName) {
    return project_ensure_module_loaded(state, context, projectIndex, moduleName);
}

/* 引用与诊断的轻量路径先登记模块再递归导入，以便循环依赖能终止。
 * BUG: 登记成功后导入子图失败会留下部分记录；导航入口忽略 EnsureScannedSourceGraph 失败并继续查此记录，
 * 从而可把未完成扫描误报为可导航源码；见 lsp_project_navigation.c 的解析路径。 */
static TZrBool project_scan_source_module_graph(SZrState *state,
                                                SZrLspContext *context,
                                                SZrLspProjectIndex *projectIndex,
                                                SZrString *moduleName) {
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar importError[ZR_PARSER_ERROR_BUFFER_LENGTH];
    SZrString *moduleUri;
    SZrFileVersion *fileVersion = ZR_NULL;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrAstNode *ast = ZR_NULL;
    TZrNativeString diskSource = ZR_NULL;
    const TZrChar *content = ZR_NULL;
    SZrArray importModuleNames;
    TZrBool success = ZR_FALSE;
    TZrBool hasSnapshot = ZR_FALSE;
    SZrFileRange importErrorLocation;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, moduleName) != ZR_NULL) {
        return ZR_TRUE;
    }

    if (!project_resolve_source_path(projectIndex, get_string_text(moduleName), resolvedPath, sizeof(resolvedPath)) ||
        ZrLibrary_File_Exist(resolvedPath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_FALSE;
    }

    moduleUri = ZrLanguageServer_LspUri_FromNativePath(state, resolvedPath);
    if (moduleUri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, moduleUri);
    if (fileVersion == ZR_NULL) {
        diskSource = ZrLibrary_File_ReadAll(state->global, resolvedPath);
        if (diskSource == ZR_NULL) {
            return ZR_FALSE;
        }

        if (context->parser != ZR_NULL) {
            TZrSize sourceLength = strlen(diskSource);
            if (!ZrLanguageServer_IncrementalParser_UpdateFile(state,
                                                               context->parser,
                                                               moduleUri,
                                                               diskSource,
                                                               sourceLength,
                                                               0)) {
                goto cleanup;
            }
            fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, moduleUri);
        }
    }

    if (context->parser != ZR_NULL && fileVersion != ZR_NULL) {
        ast = ZrLanguageServer_IncrementalParser_GetAST(context->parser, moduleUri);
    }
    if (ast != ZR_NULL &&
        !project_canonicalize_ast_for_path(state,
                                           projectIndex,
                                           ast,
                                           resolvedPath,
                                           ZR_NULL,
                                           importError,
                                           sizeof(importError),
                                           &importErrorLocation)) {
        goto cleanup;
    }

    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        content = snapshot.content;
        hasSnapshot = ZR_TRUE;
    } else {
        content = diskSource;
    }
    if (content == ZR_NULL ||
        !project_register_source_record(state, projectIndex, moduleUri, resolvedPath, ast, content)) {
        goto cleanup;
    }

    ZrCore_Array_Init(state, &importModuleNames, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!project_collect_import_module_names(state, ast, content, &importModuleNames)) {
        ZrCore_Array_Free(state, &importModuleNames);
        goto cleanup;
    }

    project_preload_descriptor_plugin_import_names(state, projectIndex, &importModuleNames);
    for (TZrSize index = 0; index < importModuleNames.length; index++) {
        SZrString **importPtr = (SZrString **)ZrCore_Array_Get(&importModuleNames, index);
        TZrChar importResolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

        if (importPtr == ZR_NULL || *importPtr == ZR_NULL) {
            continue;
        }

        if (project_resolve_source_path(projectIndex,
                                        get_string_text(*importPtr),
                                        importResolvedPath,
                                        sizeof(importResolvedPath)) &&
            ZrLibrary_File_Exist(importResolvedPath) == ZR_LIBRARY_FILE_IS_FILE &&
            !project_scan_source_module_graph(state, context, projectIndex, *importPtr)) {
            ZrCore_Array_Free(state, &importModuleNames);
            goto cleanup;
        }
    }

    ZrCore_Array_Free(state, &importModuleNames);
    success = ZR_TRUE;

cleanup:
    if (hasSnapshot) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    }
    if (diskSource != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      diskSource,
                                      strlen(diskSource) + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }
    return success;
}

/* 完成一个分析器的导入预装载；单个导入未找到由独立导入诊断处理，不中止整个文档刷新。 */
static TZrBool project_load_imports_from_uri(SZrState *state,
                                             SZrLspContext *context,
                                             SZrLspProjectIndex *projectIndex,
                                             SZrString *uri) {
    SZrSemanticAnalyzer *analyzer;
    SZrArray bindings;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL) {
            lsp_project_trace("[lsp_project] load import module=%s uri=%s\n",
                              get_string_text((*bindingPtr)->moduleName),
                              get_string_text(uri));
            project_ensure_module_loaded(state, context, projectIndex, (*bindingPtr)->moduleName);
        }
    }

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return ZR_TRUE;
}

/* 语义查询与代码操作入口确保项目入口模块已加载，才将索引标为语义可用。 */
SZrLspProjectIndex *ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(SZrState *state,
                                                                    SZrLspContext *context,
                                                                    SZrString *uri) {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool ambiguous = ZR_FALSE;
    SZrLspProjectIndex *projectIndex;
    TZrSize existingIndex;
    SZrString *projectUri;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    projectIndex = ZrLanguageServer_LspProject_FindProjectForUri(context, uri);
    if (projectIndex != ZR_NULL) {
        if (!projectIndex->hasSemanticProjectLoad &&
            (projectIndex->project == ZR_NULL || projectIndex->project->entry == ZR_NULL ||
             !project_ensure_module_loaded(state, context, projectIndex, projectIndex->project->entry))) {
            return ZR_NULL;
        }
        projectIndex->hasSemanticProjectLoad = ZR_TRUE;
        return projectIndex;
    }

    if (!discover_project_path_with_context(state, context, uri, projectPath, sizeof(projectPath), &ambiguous) ||
        ambiguous) {
        return ZR_NULL;
    }

    projectUri = ZrLanguageServer_LspUri_FromNativePath(state, projectPath);
    if (projectUri != ZR_NULL) {
        projectIndex = ZrLanguageServer_LspProject_FindProjectByProjectUri(context,
                                                                           projectUri,
                                                                           &existingIndex);
        if (projectIndex != ZR_NULL) {
            return projectIndex;
        }
    }

    projectIndex = project_index_new_from_path(state, projectPath);
    if (projectIndex == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Array_Push(state, &context->projectIndexes, &projectIndex);
    if (projectIndex->project == ZR_NULL || projectIndex->project->entry == ZR_NULL ||
        !project_ensure_module_loaded(state, context, projectIndex, projectIndex->project->entry)) {
        return ZR_NULL;
    }
    projectIndex->hasSemanticProjectLoad = ZR_TRUE;
    return projectIndex;
}

/* 文档更新和源码导航只建立项目索引，语义图按需引导加载。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_GetOrCreateForUri(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri) {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool ambiguous = ZR_FALSE;
    SZrLspProjectIndex *projectIndex;
    TZrSize existingIndex;
    SZrString *projectUri;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    projectIndex = ZrLanguageServer_LspProject_FindProjectForUri(context, uri);
    if (projectIndex != ZR_NULL) {
        return projectIndex;
    }

    if (!discover_project_path_with_context(state, context, uri, projectPath, sizeof(projectPath), &ambiguous) ||
        ambiguous) {
        return ZR_NULL;
    }

    projectUri = ZrLanguageServer_LspUri_FromNativePath(state, projectPath);
    if (projectUri != ZR_NULL) {
        projectIndex = ZrLanguageServer_LspProject_FindProjectByProjectUri(context,
                                                                           projectUri,
                                                                           &existingIndex);
        if (projectIndex != ZR_NULL) {
            return projectIndex;
        }
    }

    projectIndex = project_index_new_from_path(state, projectPath);
    if (projectIndex == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Array_Push(state, &context->projectIndexes, &projectIndex);
    return projectIndex;
}

/* 按明确 .zrp URI 打开项目索引，供项目级引用查询避免依赖任一源文件已打开。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_GetOrCreateByProjectUri(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        SZrString *projectUri) {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize existingIndex;
    SZrLspProjectIndex *projectIndex;

    if (state == ZR_NULL || context == ZR_NULL || projectUri == ZR_NULL) {
        return ZR_NULL;
    }

    projectIndex = ZrLanguageServer_LspProject_FindProjectByProjectUri(context, projectUri, &existingIndex);
    if (projectIndex != ZR_NULL) {
        return projectIndex;
    }

    if (!ZrLanguageServer_LspUri_FileToNativePath(projectUri, projectPath, sizeof(projectPath))) {
        return ZR_NULL;
    }

    projectIndex = project_index_new_from_path(state, projectPath);
    if (projectIndex == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Array_Push(state, &context->projectIndexes, &projectIndex);
    return projectIndex;
}

/* 诊断/跨快照引用需要源码图时从项目入口做轻量扫描；语义图已加载则直接复用。
 * 失败返回前保留的部分文件记录不可作为完整图使用。 */
TZrBool ZrLanguageServer_LspProject_EnsureScannedSourceGraph(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrLspProjectIndex *projectIndex) {
    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL ||
        projectIndex->project == ZR_NULL || projectIndex->project->entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (projectIndex->hasSemanticProjectLoad) {
        return ZR_TRUE;
    }

    project_clear_file_records(state, projectIndex);
    if (!project_scan_source_module_graph(state, context, projectIndex, projectIndex->project->entry)) {
        return ZR_FALSE;
    }

    projectIndex->hasLightweightSourceGraph = ZR_TRUE;
    return ZR_TRUE;
}

/* 跨文件引用查询按解析器覆盖层优先、磁盘次之收集导入模块，并在有项目时规范化 AST。 */
TZrBool ZrLanguageServer_LspProject_CollectImportModuleNamesForUri(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrArray *moduleNames) {
    SZrFileVersion *fileVersion = ZR_NULL;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrAstNode *ast = ZR_NULL;
    TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar importError[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrNativeString diskSource = ZR_NULL;
    const TZrChar *content = ZR_NULL;
    TZrBool success = ZR_FALSE;
    TZrBool hasSnapshot = ZR_FALSE;
    SZrLspProjectIndex *projectIndex = ZR_NULL;
    SZrFileRange importErrorLocation;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || moduleNames == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!moduleNames->isValid) {
        ZrCore_Array_Init(state, moduleNames, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    projectIndex = ZrLanguageServer_LspProject_FindProjectForUri(context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL &&
        ZrLanguageServer_LspUri_FileToNativePath(uri, pathBuffer, sizeof(pathBuffer)) &&
        ZrLibrary_File_Exist(pathBuffer) == ZR_LIBRARY_FILE_IS_FILE) {
        diskSource = ZrLibrary_File_ReadAll(state->global, pathBuffer);
        if (diskSource != ZR_NULL && context->parser != ZR_NULL) {
            TZrSize sourceLength = strlen(diskSource);
            if (!ZrLanguageServer_IncrementalParser_UpdateFile(state,
                                                               context->parser,
                                                               uri,
                                                               diskSource,
                                                               sourceLength,
                                                               0)) {
                goto cleanup;
            }
            fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
        }
    }

    if (context->parser != ZR_NULL && fileVersion != ZR_NULL) {
        ast = ZrLanguageServer_IncrementalParser_GetAST(context->parser, uri);
    }
    if (ast != ZR_NULL &&
        projectIndex != ZR_NULL &&
        ZrLanguageServer_LspUri_FileToNativePath(uri, pathBuffer, sizeof(pathBuffer)) &&
        !project_canonicalize_ast_for_path(state,
                                           projectIndex,
                                           ast,
                                           pathBuffer,
                                           ZR_NULL,
                                           importError,
                                           sizeof(importError),
                                           &importErrorLocation)) {
        goto cleanup;
    }

    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        content = snapshot.content;
        hasSnapshot = ZR_TRUE;
    } else {
        content = diskSource;
    }
    if (content != ZR_NULL) {
        success = project_collect_import_module_names(state, ast, content, moduleNames);
    }

cleanup:
    if (hasSnapshot) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    }
    if (diskSource != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      diskSource,
                                      strlen(diskSource) + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }
    return success;
}

/* LSP context 析构时释放所有项目索引；必须在 VM state 仍有效时调用。 */
void ZrLanguageServer_Lsp_ProjectIndexes_Free(SZrState *state, SZrLspContext *context) {
    if (state == ZR_NULL || context == ZR_NULL || !context->projectIndexes.isValid) {
        return;
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL) {
            project_index_free(state, *projectPtr);
        }
    }

    ZrCore_Array_Free(state, &context->projectIndexes);
}

/* 反向依赖广度遍历的已发现集合以 URI 避免循环重分析。 */
static TZrBool project_uri_contained_in_string_array(SZrArray *uris, SZrString *needle) {
    TZrSize index;

    if (uris == ZR_NULL || !uris->isValid || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < uris->length; index++) {
        SZrString **uriPtr = (SZrString **)ZrCore_Array_Get(uris, index);
        if (uriPtr != ZR_NULL && *uriPtr != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(*uriPtr, needle)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 反向依赖刷新从当前分析器 AST 读取真实 import 绑定，避免依赖旧文本快照。 */
static TZrBool project_file_imports_module_name(SZrState *state,
                                               SZrLspContext *context,
                                               SZrString *fileUri,
                                               SZrString *moduleName) {
    SZrSemanticAnalyzer *analyzer;
    SZrArray bindings;
    TZrSize index;

    if (state == ZR_NULL || context == ZR_NULL || fileUri == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, fileUri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    for (index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr = (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);

        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL && (*bindingPtr)->moduleName != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->moduleName, moduleName)) {
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return ZR_TRUE;
        }
    }

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return ZR_FALSE;
}

/* 在项目文件记录中寻找直接导入者，放入广度遍历队列并去重。 */
static void project_enqueue_importers_of_module(SZrState *state,
                                                SZrLspContext *context,
                                                SZrLspProjectIndex *projectIndex,
                                                SZrString *importedModuleName,
                                                SZrString *excludeUri,
                                                SZrArray *queue,
                                                SZrArray *discovered) {
    TZrSize recordIndex;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || importedModuleName == ZR_NULL ||
        queue == ZR_NULL) {
        return;
    }

    for (recordIndex = 0; recordIndex < projectIndex->files.length; recordIndex++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, recordIndex);
        SZrString *candidateUri;

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL || (*recordPtr)->uri == ZR_NULL) {
            continue;
        }

        candidateUri = (*recordPtr)->uri;
        if (excludeUri != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(candidateUri, excludeUri)) {
            continue;
        }

        if (discovered != ZR_NULL && project_uri_contained_in_string_array(discovered, candidateUri)) {
            continue;
        }

        if (project_file_imports_module_name(state, context, candidateUri, importedModuleName)) {
            ZrCore_Array_Push(state, queue, &candidateUri);
            if (discovered != ZR_NULL) {
                ZrCore_Array_Push(state, discovered, &candidateUri);
            }
        }
    }
}

/* 公开契约改变时按旧/新模块键传播失效到所有传递导入者；逐层重分析并登记新记录。 */
static TZrBool project_refresh_transitive_importers(SZrState *state,
                                                    SZrLspContext *context,
                                                    SZrLspProjectIndex *projectIndex,
                                                    SZrString *changedUri,
                                                    SZrString *previousModuleName) {
    SZrArray queue;
    SZrArray discovered;
    SZrLspProjectFileRecord *record;
    TZrSize head;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || changedUri == ZR_NULL) {
        return ZR_FALSE;
    }

    record = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, changedUri);
    if (previousModuleName == ZR_NULL &&
        (record == ZR_NULL || record->moduleName == ZR_NULL)) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, &queue, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrCore_Array_Init(state, &discovered, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrCore_Array_Push(state, &discovered, &changedUri);

    if (previousModuleName != ZR_NULL) {
        project_enqueue_importers_of_module(state,
                                           context,
                                           projectIndex,
                                           previousModuleName,
                                           changedUri,
                                           &queue,
                                           &discovered);
    }
    if (record != ZR_NULL && record->moduleName != ZR_NULL &&
        (previousModuleName == ZR_NULL ||
         !ZrLanguageServer_Lsp_StringsEqual(
                 previousModuleName, record->moduleName))) {
        project_enqueue_importers_of_module(state,
                                           context,
                                           projectIndex,
                                           record->moduleName,
                                           changedUri,
                                           &queue,
                                           &discovered);
    }

    head = 0;
    while (head < queue.length) {
        SZrString **nextPtr = (SZrString **)ZrCore_Array_Get(&queue, head);
        SZrString *nextUri;

        head++;
        if (nextPtr == ZR_NULL || *nextPtr == ZR_NULL) {
            continue;
        }

        nextUri = *nextPtr;
        if (!project_reanalyze_loaded_document(state, context, projectIndex, nextUri, ZR_TRUE) ||
            !project_register_loaded_document(state, context, projectIndex, nextUri) ||
            !project_load_imports_from_uri(state, context, projectIndex, nextUri)) {
            ZrCore_Array_Free(state, &queue);
            ZrCore_Array_Free(state, &discovered);
            return ZR_FALSE;
        }
        projectIndex->reverseDependencyReanalysisCount++;
        projectIndex->lastReverseDependencyReanalysisCount++;

        record = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, nextUri);
        if (record == ZR_NULL || record->moduleName == ZR_NULL) {
            continue;
        }

        project_enqueue_importers_of_module(state,
                                           context,
                                           projectIndex,
                                           record->moduleName,
                                           ZR_NULL,
                                           &queue,
                                           &discovered);
    }

    ZrCore_Array_Free(state, &queue);
    ZrCore_Array_Free(state, &discovered);
    return ZR_TRUE;
}

/* 更新 .zrp 时重建索引并全量重分析；更新 .zr 时比较公开契约，只在变化时传播反向依赖。
 * watcher 请求可额外递增外部提供者代数并强制扫描已加载源码。 */
static TZrBool project_refresh_for_updated_document_internal(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrBool rescanAllLoadedSources,
                                                              TZrBool advanceProviderGeneration) {
    SZrLspProjectIndex *projectIndex;
    TZrSize existingIndex;
    SZrArray loadedUris;
    TZrBool projectBootstrap;
    SZrFileVersion *updatedFileVersion;
    EZrFileChangeImpact updatedChangeImpact;
    SZrLspProjectPublicContractSnapshot previousPublicContract;

    ZrCore_Array_Construct(&loadedUris);

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    lsp_project_trace("[lsp_project] refresh begin uri=%s length=%llu incremental=%d\n",
                      get_string_text(uri),
                      (unsigned long long)contentLength,
                      (int)(rescanAllLoadedSources ? 0 : 1)); /* 1 = import-only incremental refresh */

    if (string_ends_with(uri, ".zrp")) {
        projectIndex = project_index_new_from_document(state, uri, content, contentLength);
        if (projectIndex == ZR_NULL) {
            return ZR_FALSE;
        }

        if (ZrLanguageServer_LspProject_FindProjectByProjectUri(context, uri, &existingIndex) != ZR_NULL) {
            SZrLspProjectIndex **projectPtr =
                (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, existingIndex);
            if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL) {
                project_invalidate_loaded_analyzers(state, context, *projectPtr);
                project_index_free(state, *projectPtr);
            }

            memmove(context->projectIndexes.head + existingIndex * context->projectIndexes.elementSize,
                    context->projectIndexes.head + (existingIndex + 1) * context->projectIndexes.elementSize,
                    (context->projectIndexes.length - existingIndex - 1) * context->projectIndexes.elementSize);
            context->projectIndexes.length--;
        }

        ZrCore_Array_Push(state, &context->projectIndexes, &projectIndex);
        if (advanceProviderGeneration) {
            ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(context);
        }
        if (!project_ensure_module_loaded(state, context, projectIndex, projectIndex->project->entry) ||
            !project_collect_loaded_source_uris(state, context, projectIndex, &loadedUris)) {
            ZrCore_Array_Free(state, &loadedUris);
            return ZR_FALSE;
        }

        for (TZrSize uriIndex = 0; uriIndex < loadedUris.length; uriIndex++) {
            SZrString **loadedUriPtr = (SZrString **)ZrCore_Array_Get(&loadedUris, uriIndex);
            if (loadedUriPtr == ZR_NULL || *loadedUriPtr == ZR_NULL) {
                continue;
            }

            if (!project_reanalyze_loaded_document(state, context, projectIndex, *loadedUriPtr, ZR_TRUE) ||
                !project_register_loaded_document(state, context, projectIndex, *loadedUriPtr) ||
                !project_load_imports_from_uri(state, context, projectIndex, *loadedUriPtr)) {
                ZrCore_Array_Free(state, &loadedUris);
                return ZR_FALSE;
            }
        }

        ZrCore_Array_Free(state, &loadedUris);
        return ZR_TRUE;
    }

    projectIndex = ZrLanguageServer_LspProject_GetOrCreateForUri(state, context, uri);
    if (projectIndex == ZR_NULL) {
        lsp_project_trace("[lsp_project] refresh no-project uri=%s\n", get_string_text(uri));
        return ZR_TRUE;
    }

    projectIndex->lastReverseDependencyReanalysisCount = 0;
    ZrLanguageServer_LspProject_CapturePublicContract(
            projectIndex, uri, &previousPublicContract);
    updatedFileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    updatedChangeImpact = updatedFileVersion != ZR_NULL &&
                                  updatedFileVersion->hasIncrementalInfo
                              ? updatedFileVersion->lastChangeInfo.impact
                              : ZR_FILE_CHANGE_IMPACT_MODULE;
    projectBootstrap = ZR_FALSE;
    if (advanceProviderGeneration) {
        ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(context);
    }
    if (!projectIndex->hasSemanticProjectLoad) {
        if (projectIndex->project == ZR_NULL || projectIndex->project->entry == ZR_NULL ||
            !project_ensure_module_loaded(state, context, projectIndex, projectIndex->project->entry)) {
            lsp_project_trace("[lsp_project] refresh semantic-bootstrap-failed uri=%s\n", get_string_text(uri));
            ZrCore_Array_Free(state, &loadedUris);
            return ZR_FALSE;
        }
        projectIndex->hasSemanticProjectLoad = ZR_TRUE;
        projectBootstrap = ZR_TRUE;
    }

    if (!project_reanalyze_loaded_document(state, context, projectIndex, uri, ZR_FALSE) ||
        !project_register_loaded_document(state, context, projectIndex, uri) ||
        !project_load_imports_from_uri(state, context, projectIndex, uri) ||
        !project_collect_loaded_source_uris(state, context, projectIndex, &loadedUris)) {
        lsp_project_trace("[lsp_project] refresh primary-failed uri=%s\n", get_string_text(uri));
        ZrCore_Array_Free(state, &loadedUris);
        return ZR_FALSE;
    }

    if (projectBootstrap || rescanAllLoadedSources) {
        for (TZrSize uriIndex = 0; uriIndex < loadedUris.length; uriIndex++) {
            SZrString **loadedUriPtr = (SZrString **)ZrCore_Array_Get(&loadedUris, uriIndex);

            if (loadedUriPtr == ZR_NULL || *loadedUriPtr == ZR_NULL ||
                ZrLanguageServer_Lsp_StringsEqual(*loadedUriPtr, uri)) {
                continue;
            }

            if (!project_reanalyze_loaded_document(state, context, projectIndex, *loadedUriPtr, ZR_TRUE) ||
                !project_register_loaded_document(state, context, projectIndex, *loadedUriPtr) ||
                !project_load_imports_from_uri(state, context, projectIndex, *loadedUriPtr)) {
                lsp_project_trace("[lsp_project] refresh secondary-failed uri=%s loaded=%s\n",
                                  get_string_text(uri),
                                  get_string_text(*loadedUriPtr));
                ZrCore_Array_Free(state, &loadedUris);
                return ZR_FALSE;
            }
        }
    } else {
        SZrLspProjectFileRecord *currentRecord =
                ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri);
        EZrLspProjectPublicContractChange contractChange =
                ZrLanguageServer_LspProject_ClassifyPublicContractChange(
                        projectIndex, &previousPublicContract, currentRecord);

        if (contractChange == ZR_LSP_PROJECT_PUBLIC_CONTRACT_MATCH) {
            projectIndex->reverseDependencyPreservationCount++;
            lsp_project_trace(
                    "[lsp_project] preserve reverse dependencies uri=%s impact=%d reason=public-contract-match\n",
                    get_string_text(uri),
                    (int)updatedChangeImpact);
        } else if (!project_refresh_transitive_importers(
                           state,
                           context,
                           projectIndex,
                           uri,
                           previousPublicContract.moduleName)) {
            ZrCore_Array_Free(state, &loadedUris);
            return ZR_FALSE;
        } else {
            lsp_project_trace(
                    "[lsp_project] reanalyze reverse dependencies uri=%s impact=%d reason=%s\n",
                    get_string_text(uri),
                    (int)updatedChangeImpact,
                    contractChange == ZR_LSP_PROJECT_PUBLIC_CONTRACT_CHANGE
                            ? "public-contract-change"
                            : "public-contract-unavailable");
        }
    }

    ZrCore_Array_Free(state, &loadedUris);
    lsp_project_trace("[lsp_project] refresh end uri=%s\n", get_string_text(uri));
    return ZR_TRUE;
}

/* 文档编辑入口执行按契约变化选择性的项目刷新；普通更新不递增 watcher 的提供者代数。 */
TZrBool ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrBool rescanAllLoadedSources) {
    return project_refresh_for_updated_document_internal(state,
                                                          context,
                                                          uri,
                                                          content,
                                                          contentLength,
                                                          rescanAllLoadedSources,
                                                          ZR_FALSE);
}

/* 对外仅报告当前 context 中是否已有某 URI 的项目归属，不主动发现或加载项目。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_ProjectContainsUri(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri) {
    ZR_UNUSED_PARAMETER(state);
    return ZrLanguageServer_LspProject_FindProjectForUri(context, uri) != ZR_NULL;
}

/* workspace/symbol 合并所有已分析项目文件的顶层符号，并在长循环中响应请求取消。
 * 结果数组由调用方持有，追加的 SymbolInformation 按 LSP 结果生命周期管理。 */
TZrBool ZrLanguageServer_Lsp_ProjectAppendWorkspaceSymbols(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *query,
                                                           SZrArray *result) {
    TZrBool appendedAny = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize projectIndex = 0; projectIndex < context->projectIndexes.length; projectIndex++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndex);
        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            return ZR_FALSE;
        }
        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        for (TZrSize fileIndex = 0; fileIndex < (*projectPtr)->files.length; fileIndex++) {
            SZrLspProjectFileRecord **recordPtr =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(&(*projectPtr)->files, fileIndex);
            SZrSemanticAnalyzer *analyzer;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                return ZR_FALSE;
            }

            if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL) {
                continue;
            }

            analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, (*recordPtr)->uri);
            if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
                analyzer->symbolTable->globalScope == ZR_NULL) {
                continue;
            }

            for (TZrSize symbolIndex = 0; symbolIndex < analyzer->symbolTable->globalScope->symbols.length; symbolIndex++) {
                SZrSymbol **symbolPtr =
                    (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, symbolIndex);
                if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                    return ZR_FALSE;
                }
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                    ZrLanguageServer_Lsp_StringContainsCaseInsensitive((*symbolPtr)->name, query)) {
                    SZrLspSymbolInformation *info =
                        ZrLanguageServer_Lsp_CreateSymbolInformation(state, *symbolPtr);
                    if (info != ZR_NULL) {
                        ZrCore_Array_Push(state, result, &info);
                        appendedAny = ZR_TRUE;
                    }
                }
            }
        }
    }

    return appendedAny;
}
