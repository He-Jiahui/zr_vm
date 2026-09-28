//
// Created by Auto on 2025/01/XX.
//

#include "interface/lsp_interface_internal.h"
#include "lsp_canonical_signature_help.h"
#include "lsp_virtual_documents.h"
#include "metadata/lsp_native_declaration_projection.h"
#include "metadata/lsp_virtual_document_identity.h"
#include "project/lsp_project_internal.h"
#include "project/lsp_workspace.h"
#include "semantic/lsp_local_semantic_query.h"
#include "semantic/lsp_semantic_reference_query.h"
#include "semantic/lsp_semantic_query.h"
#include "semantic/semantic_analyzer_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/task_runtime.h"
#include "zr_vm_parser/compiler.h"

#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/native_registry.h"
#if defined(ZR_VM_LSP_HAS_NETWORK_LIB)
#include "zr_vm_lib_network/module.h"
#endif
#if defined(ZR_VM_LSP_HAS_THREAD_LIB)
#include "zr_vm_lib_thread/module.h"
#endif

static TZrBool lsp_string_ends_with_native(SZrString *value, const TZrChar *suffix);
static const TZrChar *lsp_string_text_native(SZrString *value);
static SZrString *lsp_create_const_string(SZrState *state, const TZrChar *text);
static TZrBool lsp_append_location_result(SZrState *state, SZrArray *result, SZrString *uri, SZrLspRange range);
static TZrBool lsp_resolve_virtual_descriptor(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              const ZrLibModuleDescriptor **outDescriptor,
                                              EZrLspImportedModuleSourceKind *outSourceKind,
                                              TZrChar *moduleNameBuffer,
                                              TZrSize bufferSize);
static TZrBool lsp_project_modules_append_summary(SZrState *state,
                                                  SZrArray *result,
                                                  TZrInt32 sourceKind,
                                                  TZrBool isEntry,
                                                  SZrString *moduleName,
                                                  SZrString *displayName,
                                                  SZrString *description,
                                                  SZrString *navigationUri,
                                                  SZrLspRange range);
static SZrLspRange lsp_project_modules_source_entry_range(
        SZrLspContext *context,
        const SZrLspProjectFileRecord *record);
static TZrBool lsp_should_include_document_symbol(SZrSymbolTable *table,
                                                  SZrSymbolScope *scope,
                                                  SZrSymbol *symbol,
                                                  SZrString *uri);
static TZrBool lsp_project_has_indexed_record_for_uri(SZrLspContext *context, SZrString *uri);
static TZrBool lsp_document_is_open_overlay(SZrLspContext *context, SZrString *uri);
static TZrBool lsp_is_identifier_char(TZrChar value);
static TZrBool lsp_position_is_identifier_char(SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position);
static SZrFilePosition lsp_file_position_from_offset(const TZrChar *content,
                                                     TZrSize contentLength,
                                                     TZrSize offset);
static TZrBool lsp_try_get_identifier_range_at_position(SZrLspContext *context,
                                                        SZrString *uri,
                                                        SZrLspPosition position,
                                                        SZrFileRange *outRange);
static void lsp_normalize_rename_location_ranges(SZrLspContext *context, SZrArray *locations);
static TZrBool lsp_semantic_query_is_project_member_rename_target(SZrLspSemanticQuery *query);
static TZrBool lsp_semantic_query_append_rename_locations(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrLspSemanticQuery *query,
                                                          SZrArray *result);
static SZrString *lsp_semantic_query_rename_placeholder(SZrLspSemanticQuery *query);
static void lsp_hover_free_internal(SZrState *state, SZrLspHover *hover);
static const TZrChar *lsp_rich_hover_role_for_label(const TZrChar *label);
static TZrBool lsp_rich_hover_append_section(SZrState *state,
                                             SZrLspRichHover *hover,
                                             const TZrChar *role,
                                             const TZrChar *label,
                                             const TZrChar *value);
static TZrBool lsp_rich_hover_flush_docs(SZrState *state,
                                         SZrLspRichHover *hover,
                                         TZrChar *docsBuffer,
                                         TZrSize *docsLength);
static TZrBool lsp_build_rich_hover_from_markdown(SZrState *state,
                                                  const TZrChar *markdown,
                                                  SZrLspRange range,
                                                  SZrLspRichHover **result);
static TZrBool lsp_refresh_local_hover_query(SZrState *state,
                                             SZrLspContext *context,
                                             SZrString *uri,
                                             SZrLspPosition position,
                                             SZrLspLocalSemanticQueryResult *localQuery);

/**
 * @brief 为一个 LSP 上下文预备内建模块描述符，使导入、补全和虚拟声明页共用注册表。
 * @details 由 LspContext_New 调用；调用方须提供可用的 VM 全局状态，编译开关决定可见的可选库。
 */
static void lsp_register_builtin_native_libraries(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    if (!ZrLibrary_NativeRegistry_Attach(state->global)) {
        return;
    }

    ZrVmLibMath_Register(state->global);
    ZrVmLibSystem_Register(state->global);
    ZrVmLibContainer_Register(state->global);
    ZrVmLibFfi_Register(state->global);
    ZrCore_TaskRuntime_RegisterBuiltins(state->global);
#if defined(ZR_VM_LSP_HAS_NETWORK_LIB)
    ZrVmLibNetwork_Register(state->global);
#endif
#if defined(ZR_VM_LSP_HAS_THREAD_LIB)
    ZrVmThread_Register(state->global);
#endif
}
static const TZrChar *lsp_signature_label_text(SZrLspSignatureHelp *help);

/** @brief 在文档更新入口辨认工程文件后缀；这里只按字节比较，不承担 URI 规范化。 */
static TZrBool lsp_string_ends_with_native(SZrString *value, const TZrChar *suffix) {
    TZrNativeString text;
    TZrSize length;
    TZrSize suffixLength;

    if (suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        text = ZrCore_String_GetNativeStringShort(value);
        length = value->shortStringLength;
    } else {
        text = ZrCore_String_GetNativeString(value);
        length = value->longStringLength;
    }

    suffixLength = strlen(suffix);
    return text != ZR_NULL && length >= suffixLength &&
           memcmp(text + length - suffixLength, suffix, suffixLength) == 0;
}

/** @brief 把 VM 字符串借给签名与富悬停适配器；返回值的生命周期仍由 VM 管理。 */
static const TZrChar *lsp_string_text_native(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/** @brief 将模块摘要或富悬停中的固定标签交给 VM 字符串管理。 */
static SZrString *lsp_create_const_string(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

/**
 * @brief 为虚拟模块链接的定义跳转提供与普通语义查询一致的位置结果。
 * @details 调用方负责按位置数组的约定释放结构体；URI 仍是 VM 管理的字符串。
 */
static TZrBool lsp_append_location_result(SZrState *state, SZrArray *result, SZrString *uri, SZrLspRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = uri;
    location->range = range;
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

/**
 * @brief 将虚拟声明 URI 还原为工程可见的原生模块描述符。
 * @details 定义跳转和虚拟文档读取均先尝试各工程索引，最后尝试全局注册表；不可将任意 URI 当作原生声明页。
 */
static TZrBool lsp_resolve_virtual_descriptor(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              const ZrLibModuleDescriptor **outDescriptor,
                                              EZrLspImportedModuleSourceKind *outSourceKind,
                                              TZrChar *moduleNameBuffer,
                                              TZrSize bufferSize) {
    if (outDescriptor != ZR_NULL) {
        *outDescriptor = ZR_NULL;
    }
    if (outSourceKind != ZR_NULL) {
        *outSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    }
    if (moduleNameBuffer != ZR_NULL && bufferSize > 0) {
        moduleNameBuffer[0] = '\0';
    }
    if (state == ZR_NULL || uri == ZR_NULL || outDescriptor == ZR_NULL || moduleNameBuffer == ZR_NULL ||
        bufferSize == 0 || !ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
        return ZR_FALSE;
    }

    if (context != ZR_NULL) {
        for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
            SZrLspProjectIndex **projectPtr =
                (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
            if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL &&
                ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(state,
                                                                             context,
                                                                             *projectPtr,
                                                                             uri,
                                                                             outDescriptor,
                                                                             outSourceKind,
                                                                             moduleNameBuffer,
                                                                             bufferSize) &&
                *outDescriptor != ZR_NULL) {
                return ZR_TRUE;
            }
        }
    }

    return ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(state,
                                                                        context,
                                                                        ZR_NULL,
                                                                        uri,
                                                                        outDescriptor,
                                                                        outSourceKind,
                                                                        moduleNameBuffer,
                                                                        bufferSize) &&
           *outDescriptor != ZR_NULL;
}

/**
 * @brief 合并同来源类别、同模块名的工程浏览条目，并保留可导航的首个入口。
 * @details GetProjectModules 将源文件和导入模块都送入此处；摘要结构体由 FreeProjectModules 释放。
 */
static TZrBool lsp_project_modules_append_summary(SZrState *state,
                                                  SZrArray *result,
                                                  TZrInt32 sourceKind,
                                                  TZrBool isEntry,
                                                  SZrString *moduleName,
                                                  SZrString *displayName,
                                                  SZrString *description,
                                                  SZrString *navigationUri,
                                                  SZrLspRange range) {
    SZrLspProjectModuleSummary *summary;

    if (state == ZR_NULL || result == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspProjectModuleSummary *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspProjectModuleSummary **summaryPtr =
            (SZrLspProjectModuleSummary **)ZrCore_Array_Get(result, index);
        if (summaryPtr == ZR_NULL || *summaryPtr == ZR_NULL) {
            continue;
        }

        if ((*summaryPtr)->sourceKind == sourceKind &&
            ZrLanguageServer_Lsp_StringsEqual((*summaryPtr)->moduleName, moduleName)) {
            if (isEntry) {
                (*summaryPtr)->isEntry = ZR_TRUE;
            }
            if ((*summaryPtr)->navigationUri == ZR_NULL && navigationUri != ZR_NULL) {
                (*summaryPtr)->navigationUri = navigationUri;
                (*summaryPtr)->range = range;
            }
            return ZR_TRUE;
        }
    }

    summary = (SZrLspProjectModuleSummary *)ZrCore_Memory_RawMalloc(state->global,
                                                                    sizeof(SZrLspProjectModuleSummary));
    if (summary == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(summary, 0, sizeof(*summary));
    summary->sourceKind = sourceKind;
    summary->isEntry = isEntry;
    summary->moduleName = moduleName;
    summary->displayName = displayName != ZR_NULL ? displayName : moduleName;
    summary->description = description;
    summary->navigationUri = navigationUri;
    summary->range = range;
    ZrCore_Array_Push(state, result, &summary);
    return ZR_TRUE;
}

/** @brief 把项目索引中的源模块入口映射为客户端可导航的文档坐标。 */
static SZrLspRange lsp_project_modules_source_entry_range(
        SZrLspContext *context,
        const SZrLspProjectFileRecord *record) {
    SZrFileRange range =
            ZrLanguageServer_LspProject_GetSourceModuleEntryRange(
                    context, record);
    return ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context,
            record != ZR_NULL ? record->uri : ZR_NULL,
            range);
}

/**
 * @brief 限制文档大纲为本文件声明及其成员，避免把局部变量、参数和导入符号混入大纲。
 * @details GetDocumentSymbols 扫描所有作用域时使用；同一路径的 URI 别名按原生路径归并。
 */
static TZrBool lsp_should_include_document_symbol(SZrSymbolTable *table,
                                                  SZrSymbolScope *scope,
                                                  SZrSymbol *symbol,
                                                  SZrString *uri) {
    if (table == ZR_NULL || scope == ZR_NULL || symbol == ZR_NULL || symbol->location.source == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UrisResolveToSameNativePath(symbol->location.source, uri)) {
        return ZR_FALSE;
    }

    if (scope == table->globalScope) {
        return symbol->type != ZR_SYMBOL_PARAMETER;
    }

    switch (symbol->type) {
        case ZR_SYMBOL_CLASS:
        case ZR_SYMBOL_STRUCT:
        case ZR_SYMBOL_INTERFACE:
        case ZR_SYMBOL_ENUM:
        case ZR_SYMBOL_ENUM_MEMBER:
        case ZR_SYMBOL_FIELD:
        case ZR_SYMBOL_METHOD:
        case ZR_SYMBOL_PROPERTY:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

/** @brief 工作区符号查询用此判断文件是否已由项目索引提供，避免叠加缓存分析器造成重复。 */
static TZrBool lsp_project_has_indexed_record_for_uri(SZrLspContext *context, SZrString *uri) {
    if (context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize projectIndex = 0; projectIndex < context->projectIndexes.length; projectIndex++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndex);
        if (projectPtr != ZR_NULL &&
            *projectPtr != ZR_NULL &&
            ZrLanguageServer_LspProject_FindRecordByUri(*projectPtr, uri) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/** @brief 工作区符号仅从未入项目索引的已打开缓冲区补充临时符号。 */
static TZrBool lsp_document_is_open_overlay(SZrLspContext *context, SZrString *uri) {
    SZrFileVersion *fileVersion;

    if (context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    return fileVersion != ZR_NULL && fileVersion->isOpenDocument;
}

/** @brief 重命名范围与局部悬停共用的字节级标识符判定。 */
static TZrBool lsp_is_identifier_char(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

/**
 * @brief 悬停在标识符内时优先给出符号说明，避免局部表达式事实遮盖声明信息。
 * @details 使用当前文档快照进行 LSP 坐标转换；回退 AST 的位置不可视为当前文本的可靠语义位置。
 */
static TZrBool lsp_position_is_identifier_char(SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;
    SZrFilePosition filePosition;
    TZrSize offset;
    TZrBool isIdentifier;

    if (context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(context->state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    if (snapshot.contentLength == 0) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                          snapshot.content,
                                                                          snapshot.contentLength);
    if (snapshot.usesFallbackAst) {
        filePosition.offset = 0;
    }
    offset = filePosition.offset;
    if (offset >= snapshot.contentLength) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
        return ZR_FALSE;
    }

    isIdentifier = lsp_is_identifier_char(snapshot.content[offset]);
    ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
    return isIdentifier;
}

/** @brief 将重命名识别出的字节边界还原为解析器内部的行列坐标。 */
static SZrFilePosition lsp_file_position_from_offset(const TZrChar *content,
                                                     TZrSize contentLength,
                                                     TZrSize offset) {
    TZrInt32 line = 1;
    TZrInt32 column = 1;

    if (content == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, line, column);
    }
    if (offset > contentLength) {
        offset = contentLength;
    }

    for (TZrSize index = 0; index < offset; index++) {
        if (content[index] == '\n') {
            line++;
            column = 1;
        } else if (content[index] != '\r') {
            column++;
        }
    }

    return ZrParser_FilePosition_Create(offset, line, column);
}

/**
 * @brief 为 prepareRename 和最终编辑范围定位当前快照中的完整标识符。
 * @details 允许光标落在标识符紧邻的右侧边界；调用方再将内部范围转换为客户端编码坐标。
 */
static TZrBool lsp_try_get_identifier_range_at_position(SZrLspContext *context,
                                                        SZrString *uri,
                                                        SZrLspPosition position,
                                                        SZrFileRange *outRange) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;
    SZrFilePosition filePosition;
    TZrSize offset;
    TZrSize start;
    TZrSize end;

    if (context == ZR_NULL || uri == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(context->state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    if (snapshot.contentLength == 0) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                          snapshot.content,
                                                                          snapshot.contentLength);
    if (snapshot.usesFallbackAst) {
        filePosition.offset = 0;
    }
    offset = filePosition.offset;
    if (offset >= snapshot.contentLength) {
        offset = snapshot.contentLength - 1;
    }
    if (!lsp_is_identifier_char(snapshot.content[offset]) &&
        offset > 0 &&
        lsp_is_identifier_char(snapshot.content[offset - 1])) {
        offset--;
    }
    if (!lsp_is_identifier_char(snapshot.content[offset])) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
        return ZR_FALSE;
    }

    start = offset;
    while (start > 0 && lsp_is_identifier_char(snapshot.content[start - 1])) {
        start--;
    }
    end = offset + 1;
    while (end < snapshot.contentLength && lsp_is_identifier_char(snapshot.content[end])) {
        end++;
    }
    if (end <= start) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
        return ZR_FALSE;
    }

    *outRange = ZrParser_FileRange_Create(
        lsp_file_position_from_offset(snapshot.content, snapshot.contentLength, start),
        lsp_file_position_from_offset(snapshot.content, snapshot.contentLength, end),
        uri);
    ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
    return ZR_TRUE;
}

/**
 * @brief 将语义引用返回的位置统一收敛到实际标识符，供 WorkspaceEdit 生成精确替换范围。
 * @details Rename 在此后再次检查取消状态；部分规范化结果不得作为成功响应发送。
 */
static void lsp_normalize_rename_location_ranges(SZrLspContext *context, SZrArray *locations) {
    if (context == ZR_NULL || locations == ZR_NULL || !locations->isValid) {
        return;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrFileRange identifierRange;

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            return;
        }

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL || (*locationPtr)->uri == ZR_NULL) {
            continue;
        }

        if (lsp_try_get_identifier_range_at_position(context,
                                                     (*locationPtr)->uri,
                                                     (*locationPtr)->range.start,
                                                     &identifierRange)) {
            (*locationPtr)->range =
                ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, (*locationPtr)->uri, identifierRange);
        }
    }
}

/** @brief 仅允许有源声明的工程成员参与跨文件重命名，排除无法编辑的原生和二进制元数据。 */
static TZrBool lsp_semantic_query_is_project_member_rename_target(SZrLspSemanticQuery *query) {
    EZrLspImportedModuleSourceKind sourceKind;

    if (query == ZR_NULL || query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        query->memberName == ZR_NULL || !query->resolvedMember.hasDeclaration ||
        query->resolvedMember.declarationUri == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceKind = query->resolvedMember.module.sourceKind;
    return sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE ||
           sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER;
}

/** @brief 将局部符号或工程成员的语义身份转成可编辑引用集合。 */
static TZrBool lsp_semantic_query_append_rename_locations(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrLspSemanticQuery *query,
                                                          SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL &&
        query->hasCanonicalSymbol &&
        query->canonicalSymbol.symbolId != ZR_SEMANTIC_ID_INVALID) {
        return ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences(
                state, context, query, ZR_TRUE, result) && result->length > 0;
    }

    if (lsp_semantic_query_is_project_member_rename_target(query)) {
        return ZrLanguageServer_LspSemanticQuery_AppendReferences(state, context, query, ZR_TRUE, result);
    }

    return ZR_FALSE;
}

/** @brief prepareRename 只对可由最终 Rename 收集引用的目标给出占位名称。 */
static SZrString *lsp_semantic_query_rename_placeholder(SZrLspSemanticQuery *query) {
    if (query == ZR_NULL) {
        return ZR_NULL;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL &&
        query->hasCanonicalSymbol &&
        query->canonicalSymbol.symbolId != ZR_SEMANTIC_ID_INVALID) {
        return query->canonicalSymbol.displayName;
    }

    if (lsp_semantic_query_is_project_member_rename_target(query)) {
        return query->memberName;
    }

    return ZR_NULL;
}

/**
 * @brief 给调用悬停追加源码注释，同时避免明显重复的 Markdown 段落。
 * @details 仅在固定缓冲区容得下内容时合并；溢出时保持原悬停，调用方不应把返回原值理解为无注释。
 */
static SZrString *lsp_append_markdown_section(SZrState *state, SZrString *base, SZrString *appendix) {
    TZrNativeString baseText;
    TZrNativeString appendixText;
    TZrSize baseLength;
    TZrSize appendixLength;
    TZrChar buffer[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrSize used = 0;

    if (state == ZR_NULL || base == ZR_NULL || appendix == ZR_NULL) {
        return base;
    }

    if (base->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        baseText = ZrCore_String_GetNativeStringShort(base);
        baseLength = base->shortStringLength;
    } else {
        baseText = ZrCore_String_GetNativeString(base);
        baseLength = base->longStringLength;
    }

    if (appendix->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        appendixText = ZrCore_String_GetNativeStringShort(appendix);
        appendixLength = appendix->shortStringLength;
    } else {
        appendixText = ZrCore_String_GetNativeString(appendix);
        appendixLength = appendix->longStringLength;
    }

    if (baseText == ZR_NULL || appendixText == ZR_NULL || appendixLength == 0) {
        return base;
    }

    if (strstr(baseText, appendixText) != ZR_NULL) {
        return base;
    }

    buffer[0] = '\0';
    if (baseLength + appendixLength + 3 >= sizeof(buffer)) {
        return base;
    }

    memcpy(buffer + used, baseText, baseLength);
    used += baseLength;
    memcpy(buffer + used, "\n\n", 2);
    used += 2;
    memcpy(buffer + used, appendixText, appendixLength);
    used += appendixLength;
    buffer[used] = '\0';
    return ZrCore_String_Create(state, buffer, used);
}

/** @brief 富悬停适配器消费普通悬停后释放其容器；内容字符串继续由 VM 管理。 */
static void lsp_hover_free_internal(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

/** @brief 将现有 Markdown 字段映射到扩展端使用的富悬停语义角色。 */
static const TZrChar *lsp_rich_hover_role_for_label(const TZrChar *label) {
    if (label == ZR_NULL || label[0] == '\0') {
        return "detail";
    }

    if (strcmp(label, "Signature") == 0) {
        return "signature";
    }
    if (strcmp(label, "Resolved Type") == 0 || strcmp(label, "Type") == 0) {
        return "resolvedType";
    }
    if (strcmp(label, "Expression") == 0) {
        return "expression";
    }
    if (strcmp(label, "Constant") == 0) {
        return "constant";
    }
    if (strcmp(label, "Numeric range") == 0 ||
        strcmp(label, "Unsigned range") == 0 ||
        strcmp(label, "Numeric warning") == 0) {
        return "numericRange";
    }
    if (strcmp(label, "Logical value") == 0) {
        return "logicalValue";
    }
    if (strcmp(label, "Logical flow") == 0) {
        return "logicalFlow";
    }
    if (strcmp(label, "Reference") == 0) {
        return "reference";
    }
    if (strcmp(label, "Symbol") == 0) {
        return "symbol";
    }
    if (strcmp(label, "Declared at") == 0) {
        return "declaration";
    }
    if (strcmp(label, "Reachability") == 0) {
        return "reachability";
    }
    if (strcmp(label, "Ownership") == 0) {
        return "ownership";
    }
    if (strcmp(label, "Call") == 0) {
        return "call";
    }
    if (strcmp(label, "Member") == 0) {
        return "member";
    }
    if (strcmp(label, "Access") == 0) {
        return "access";
    }
    if (strcmp(label, "Category") == 0) {
        return "category";
    }
    if (strcmp(label, "Applicable To") == 0) {
        return "applicableTo";
    }
    if (strcmp(label, "Source") == 0) {
        return "source";
    }
    if (strcmp(label, "Detail") == 0) {
        return "detail";
    }
    if (strcmp(label, "Meta Method") == 0 || strcmp(label, "Decorator") == 0) {
        return "name";
    }

    return "detail";
}

/** @brief 为富悬停构造一个独立字段；调用方通过 FreeRichHover 回收字段容器。 */
static TZrBool lsp_rich_hover_append_section(SZrState *state,
                                             SZrLspRichHover *hover,
                                             const TZrChar *role,
                                             const TZrChar *label,
                                             const TZrChar *value) {
    SZrLspRichHoverSection *section;

    if (state == ZR_NULL || hover == ZR_NULL || role == ZR_NULL || value == ZR_NULL || value[0] == '\0') {
        return ZR_FALSE;
    }

    section =
        (SZrLspRichHoverSection *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspRichHoverSection));
    if (section == ZR_NULL) {
        return ZR_FALSE;
    }

    section->role = lsp_create_const_string(state, role);
    section->label = label != ZR_NULL ? lsp_create_const_string(state, label) : ZR_NULL;
    section->value = lsp_create_const_string(state, value);
    if (section->role == ZR_NULL || section->value == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, section, sizeof(SZrLspRichHoverSection));
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &hover->sections, &section);
    return ZR_TRUE;
}

/** @brief 在遇到结构化字段前收束普通文档文字，保持扩展端展示顺序。 */
static TZrBool lsp_rich_hover_flush_docs(SZrState *state,
                                         SZrLspRichHover *hover,
                                         TZrChar *docsBuffer,
                                         TZrSize *docsLength) {
    TZrBool appended;

    if (docsBuffer == ZR_NULL || docsLength == ZR_NULL || *docsLength == 0) {
        return ZR_TRUE;
    }

    docsBuffer[*docsLength] = '\0';
    appended = lsp_rich_hover_append_section(state, hover, "docs", "Documentation", docsBuffer);
    docsBuffer[0] = '\0';
    *docsLength = 0;
    return appended;
}

/**
 * @brief 将普通悬停 Markdown 投影为供扩展端分别渲染的角色字段。
 * @details GetRichHover 复用 GetHover 的语义决策；转换受固定长度缓冲区限制，调用方必须使用 FreeRichHover 释放。
 * TODO: 当前把所有含冒号的非标题行当成字段；若普通说明行含冒号且冒号后为空，转换会失败，需确认客户端期望的容错策略。
 */
static TZrBool lsp_build_rich_hover_from_markdown(SZrState *state,
                                                  const TZrChar *markdown,
                                                  SZrLspRange range,
                                                  SZrLspRichHover **result) {
    SZrLspRichHover *richHover;
    TZrChar markdownBuffer[ZR_LSP_HOVER_BUFFER_LENGTH];
    TZrChar docsBuffer[ZR_LSP_HOVER_BUFFER_LENGTH];
    TZrSize docsLength = 0;
    TZrSize markdownLength;
    TZrChar *cursor;

    if (result != ZR_NULL) {
        *result = ZR_NULL;
    }
    if (state == ZR_NULL || markdown == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    richHover = (SZrLspRichHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspRichHover));
    if (richHover == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &richHover->sections, sizeof(SZrLspRichHoverSection *), 8);
    richHover->range = range;

    markdownLength = strlen(markdown);
    if (markdownLength >= sizeof(markdownBuffer)) {
        markdownLength = sizeof(markdownBuffer) - 1;
    }
    memcpy(markdownBuffer, markdown, markdownLength);
    markdownBuffer[markdownLength] = '\0';
    docsBuffer[0] = '\0';

    cursor = markdownBuffer;
    while (cursor != ZR_NULL && *cursor != '\0') {
        TZrChar *line = cursor;
        TZrChar *trimmed = line;
        TZrChar *end = ZR_NULL;
        TZrChar *headingEnd;
        TZrChar *colon;

        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }
        if (*cursor == '\n') {
            *cursor = '\0';
            cursor++;
        }

        while (*trimmed != '\0' && isspace((unsigned char)*trimmed)) {
            trimmed++;
        }

        end = trimmed + strlen(trimmed);
        while (end > trimmed && isspace((unsigned char)end[-1])) {
            end--;
        }
        *end = '\0';

        if (*trimmed == '\0') {
            if (docsLength > 0 && docsLength + 2 < sizeof(docsBuffer)) {
                docsBuffer[docsLength++] = '\n';
                docsBuffer[docsLength++] = '\n';
                docsBuffer[docsLength] = '\0';
            }
            continue;
        }

        headingEnd = ZR_NULL;
        if (trimmed[0] == '*' && trimmed[1] == '*') {
            headingEnd = strstr(trimmed + 2, "**");
        }
        if (headingEnd != ZR_NULL) {
            TZrChar labelBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
            TZrChar valueBuffer[ZR_LSP_HOVER_BUFFER_LENGTH];
            TZrSize labelLength = (TZrSize)(headingEnd - (trimmed + 2));
            TZrChar *valueStart = headingEnd + 2;

            if (!lsp_rich_hover_flush_docs(state, richHover, docsBuffer, &docsLength)) {
                ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
                return ZR_FALSE;
            }

            if (labelLength >= sizeof(labelBuffer)) {
                labelLength = sizeof(labelBuffer) - 1;
            }
            memcpy(labelBuffer, trimmed + 2, labelLength);
            labelBuffer[labelLength] = '\0';

            while (*valueStart == ':' || isspace((unsigned char)*valueStart)) {
                valueStart++;
            }

            if (*valueStart != '\0') {
                snprintf(valueBuffer, sizeof(valueBuffer), "%s", valueStart);
                if (!lsp_rich_hover_append_section(state, richHover, "name", labelBuffer, valueBuffer)) {
                    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
                    return ZR_FALSE;
                }
            } else if (!lsp_rich_hover_append_section(state, richHover, "kind", "Kind", labelBuffer)) {
                ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
                return ZR_FALSE;
            }

            continue;
        }

        colon = strchr(trimmed, ':');
        if (colon != ZR_NULL) {
            TZrChar labelBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
            TZrChar valueBuffer[ZR_LSP_HOVER_BUFFER_LENGTH];
            TZrSize labelLength = (TZrSize)(colon - trimmed);
            TZrChar *valueStart = colon + 1;
            const TZrChar *role;

            if (!lsp_rich_hover_flush_docs(state, richHover, docsBuffer, &docsLength)) {
                ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
                return ZR_FALSE;
            }

            while (labelLength > 0 && isspace((unsigned char)trimmed[labelLength - 1])) {
                labelLength--;
            }
            while (*valueStart != '\0' && isspace((unsigned char)*valueStart)) {
                valueStart++;
            }

            if (labelLength >= sizeof(labelBuffer)) {
                labelLength = sizeof(labelBuffer) - 1;
            }
            memcpy(labelBuffer, trimmed, labelLength);
            labelBuffer[labelLength] = '\0';
            snprintf(valueBuffer, sizeof(valueBuffer), "%s", valueStart);
            role = lsp_rich_hover_role_for_label(labelBuffer);
            if (!lsp_rich_hover_append_section(state, richHover, role, labelBuffer, valueBuffer)) {
                ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
                return ZR_FALSE;
            }

            continue;
        }

        if (docsLength > 0 && docsLength + 1 < sizeof(docsBuffer) && docsBuffer[docsLength - 1] != '\n') {
            docsBuffer[docsLength++] = '\n';
        }
        if (docsLength + strlen(trimmed) >= sizeof(docsBuffer)) {
            trimmed[sizeof(docsBuffer) - docsLength - 1] = '\0';
        }
        snprintf(docsBuffer + docsLength, sizeof(docsBuffer) - docsLength, "%s", trimmed);
        docsLength = strlen(docsBuffer);
    }

    if (!lsp_rich_hover_flush_docs(state, richHover, docsBuffer, &docsLength)) {
        ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
        return ZR_FALSE;
    }

    *result = richHover;
    return ZR_TRUE;
}

/** @brief 在其他悬停分支完成后重取局部表达式事实，以最新语义快照补充悬停。 */
static TZrBool lsp_refresh_local_hover_query(SZrState *state,
                                             SZrLspContext *context,
                                             SZrString *uri,
                                             SZrLspPosition position,
                                             SZrLspLocalSemanticQueryResult *localQuery) {
    if (localQuery == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Clear(localQuery);
    return ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, localQuery);
}

/**
 * @brief 为 stdio_server 等客户端建立解析器、项目索引和语义缓存共用的会话状态。
 * @details 先注册编译与内建库，再延迟创建各文档分析器；必须与 LspContext_Free 配对。
 * BUG: parser 或 workspace 初始化失败时直接释放 context，但已构造的 parser、哈希表和项目索引数组未按正常销毁路径回收。
 */
SZrLspContext *ZrLanguageServer_LspContext_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global != ZR_NULL && state->global->compileSource == ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
    }
    
    SZrLspContext *context = (SZrLspContext *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspContext));
    if (context == ZR_NULL) {
        return ZR_NULL;
    }
    
    context->state = state;
    context->parser = ZrLanguageServer_IncrementalParser_New(state);
    /* TODO: 旧单分析器槽位目前只在此初始化和销毁路径读取；确认公开结构体的 ABI 约束后再判断是否保留。 */
    context->analyzer = ZR_NULL;
    context->semanticSnapshotCache = ZR_NULL;
    context->semanticCacheLru = ZR_NULL;
    ZrCore_HashSet_Construct(&context->uriToAnalyzerMap);
    ZrCore_HashSet_Init(state, &context->uriToAnalyzerMap, ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2);
    ZrCore_Array_Init(state,
                      &context->projectIndexes,
                      sizeof(SZrLspProjectIndex *),
                      ZR_LSP_PROJECT_INDEX_INITIAL_CAPACITY);
    context->workspace = ZrLanguageServer_LspWorkspace_New(state);
    context->semanticSnapshotProviderGeneration = 1U;
    context->activeSemanticSnapshot = ZR_NULL;
    context->clientSelectedZrpNativePath = ZR_NULL;
    context->requestCancellationCheck = ZR_NULL;
    context->requestCancellationUserData = ZR_NULL;

    lsp_register_builtin_native_libraries(state);
    
    if (context->parser == ZR_NULL || context->workspace == ZR_NULL) {
        ZrLanguageServer_LspWorkspace_Free(state, context->workspace);
        ZrCore_Memory_RawFree(state->global, context, sizeof(SZrLspContext));
        return ZR_NULL;
    }
    context->semanticSnapshotCache = ZrLanguageServer_LspSemanticSnapshotCache_New(state);
    if (context->semanticSnapshotCache == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_NULL;
    }
    context->semanticCacheLru = ZrLanguageServer_LspSemanticCacheLru_New(state);
    if (context->semanticCacheLru == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return ZR_NULL;
    }
    
    return context;
}

/**
 * @brief 把 stdio 当前请求的取消探针暂借给同步语义查询。
 * @details stdio_requests 在请求结束后清除回调；userData 的生命周期必须覆盖该请求执行期。
 */
void ZrLanguageServer_LspContext_SetRequestCancellationCheck(
        SZrLspContext *context,
        FZrLspRequestCancellationCheck check,
        void *userData) {
    if (context == ZR_NULL) {
        return;
    }
    context->requestCancellationCheck = check;
    context->requestCancellationUserData = check != ZR_NULL ? userData : ZR_NULL;
}

/** @brief 长时间遍历的引用、符号和重命名查询用此读取当前请求的取消状态。 */
TZrBool ZrLanguageServer_LspContext_IsRequestCancellationRequested(
        const SZrLspContext *context) {
    return context != ZR_NULL &&
           context->requestCancellationCheck != ZR_NULL &&
           context->requestCancellationCheck(context->requestCancellationUserData);
}

/**
 * @brief 结束会话时按缓存、分析器、解析器、工程和工作区的所有权链释放状态。
 * @details stdio_server 在停止时调用；分析器由 URI 映射持有，项目索引和快照在销毁前不得继续使用。
 */
void ZrLanguageServer_LspContext_Free(SZrState *state, SZrLspContext *context) {
    if (state == ZR_NULL || context == ZR_NULL) {
        return;
    }

    ZrLanguageServer_LspSemanticSnapshotCache_Free(state, context);
    ZrLanguageServer_LspSemanticCacheLru_Free(state, context);

    // 释放所有分析器
    if (context->uriToAnalyzerMap.isValid && context->uriToAnalyzerMap.buckets != ZR_NULL) {
        // 遍历哈希表释放所有分析器和节点
        for (TZrSize i = 0; i < context->uriToAnalyzerMap.capacity; i++) {
            SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[i];
            while (pair != ZR_NULL) {
                // 释放节点中存储的数据
                if (pair->key.type != ZR_VALUE_TYPE_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrSemanticAnalyzer *analyzer = 
                            (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                        if (analyzer != ZR_NULL) {
                            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
                        }
                    }
                }
                SZrHashKeyValuePair *next = pair->next;
                /* HashSet pairs are owned by the pair pool released by Deconstruct. */
                pair = next;
            }
        }
        // 释放 buckets 数组
        ZrCore_HashSet_Deconstruct(state, &context->uriToAnalyzerMap);
    }

    if (context->parser != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_Free(state, context->parser);
    }

    if (context->analyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, context->analyzer);
    }

    if (context->clientSelectedZrpNativePath != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      context->clientSelectedZrpNativePath,
                                      strlen(context->clientSelectedZrpNativePath) + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        context->clientSelectedZrpNativePath = ZR_NULL;
    }

    ZrLanguageServer_Lsp_ProjectIndexes_Free(state, context);
    ZrLanguageServer_LspWorkspace_Free(state, context->workspace);
    ZrCore_Memory_RawFree(state->global, context, sizeof(SZrLspContext));
}

/**
 * @brief 记录 IDE 当前选中的工程文件，供多工程目录下的项目发现消除歧义。
 * @details initialize 与项目请求可更新此选择；传空 URI 清除，仅接受能转为原生 .zrp 路径的 URI。
 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspContext_SetClientSelectedZrpUri(SZrState *state,
                                                                              SZrLspContext *context,
                                                                              SZrString *zrpFileUri) {
    TZrChar buffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize pathLength;
    TZrChar *copy;

    if (state == ZR_NULL || context == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    if (context->clientSelectedZrpNativePath != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      context->clientSelectedZrpNativePath,
                                      strlen(context->clientSelectedZrpNativePath) + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        context->clientSelectedZrpNativePath = ZR_NULL;
    }

    if (zrpFileUri == ZR_NULL) {
        return;
    }

    if (!ZrLanguageServer_Lsp_FileUriToNativePath(zrpFileUri, buffer, sizeof(buffer))) {
        return;
    }

    pathLength = strlen(buffer);
    if (pathLength < 4) {
        return;
    }

    if (tolower((unsigned char)buffer[pathLength - 4]) != '.' ||
        tolower((unsigned char)buffer[pathLength - 3]) != 'z' ||
        tolower((unsigned char)buffer[pathLength - 2]) != 'r' ||
        tolower((unsigned char)buffer[pathLength - 1]) != 'p') {
        return;
    }

    copy = (TZrChar *)ZrCore_Memory_RawMallocWithType(state->global,
                                                      pathLength + 1,
                                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    if (copy == ZR_NULL) {
        return;
    }

    memcpy(copy, buffer, pathLength + 1);
    context->clientSelectedZrpNativePath = copy;
}

/**
 * @brief 在会话 URI 映射中复用或创建语义分析器，供编辑查询和项目重分析共享。
 * @details 支持解析到同一原生路径的 URI 别名；返回对象预期由映射统一管理，并同步快照代次及 LRU 热度。
 * BUG: 哈希表新增节点失败时仍返回新分析器，调用方会继续使用一个未登记、无法由上下文统一回收的对象。
 */
SZrSemanticAnalyzer *ZrLanguageServer_Lsp_GetOrCreateAnalyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从哈希表中查找
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(state, &context->uriToAnalyzerMap, uri);
    }
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        // 从原生指针中获取 SZrSemanticAnalyzer
        SZrSemanticAnalyzer *existing =
                (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
        ZrLanguageServer_SemanticAnalyzer_SetExternalProviderGeneration(
                state,
                existing,
                context->semanticSnapshotProviderGeneration);
        ZrLanguageServer_LspSemanticCacheLru_Touch(context, existing);
        return existing;
    }
    
    // 创建新分析器
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_SetExternalProviderGeneration(
                state,
                analyzer,
                context->semanticSnapshotProviderGeneration);
        // 添加到哈希表
        SZrHashKeyValuePair *newPair = ZrCore_HashSet_Add(state, &context->uriToAnalyzerMap, &key);
        if (newPair != ZR_NULL) {
            // 将 SZrSemanticAnalyzer 指针存储为原生指针
            SZrTypeValue value;
            ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)analyzer);
            ZrCore_Value_Copy(state, &newPair->value, &value);
        }
    }
    
    ZrLanguageServer_LspSemanticCacheLru_Touch(context, analyzer);
    return analyzer;
}

/**
 * @brief 只查找现有文档分析器，并在活动语义快照中登记依赖。
 * @details 诊断、悬停及快照提供者用此避免无意新建分析器；别名 URI 仍按原生路径匹配。
 */
SZrSemanticAnalyzer *ZrLanguageServer_Lsp_FindAnalyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    if (context->activeSemanticSnapshot != ZR_NULL) {
        (void)ZrLanguageServer_LspSemanticSnapshot_TrackDependency(
                state, context, context->activeSemanticSnapshot, uri);
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(state, &context->uriToAnalyzerMap, uri);
    }
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        SZrSemanticAnalyzer *analyzer =
                (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
        ZrLanguageServer_SemanticAnalyzer_SetExternalProviderGeneration(
                state,
                analyzer,
                context->semanticSnapshotProviderGeneration);
        ZrLanguageServer_LspSemanticCacheLru_Touch(context, analyzer);
        return analyzer;
    }

    return ZR_NULL;
}

/** @brief 文档分析失败或关闭后同步移除快照缓存和 URI 映射，避免后续查询读取旧语义。 */
void ZrLanguageServer_Lsp_RemoveAnalyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrSemanticAnalyzer *analyzer;
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair == ZR_NULL) {
        pair = ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(state, &context->uriToAnalyzerMap, uri);
    }
    if (pair == ZR_NULL || pair->value.type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return;
    }

    analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
    ZrLanguageServer_LspSemanticSnapshotCache_RemoveUri(state, context, uri);
    key = pair->key;
    ZrCore_HashSet_Remove(state, &context->uriToAnalyzerMap, &key);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
}

/** @brief 从增量解析器读取指定 URI 的当前版本，供坐标转换、诊断与编辑快照共用。 */
SZrFileVersion *ZrLanguageServer_Lsp_GetDocumentFileVersion(SZrLspContext *context, SZrString *uri) {
    if (context == ZR_NULL || context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLanguageServer_IncrementalParser_GetFileVersion(context->parser, uri);
}

/**
 * @brief 将客户端位置按当前内容转换为解析器位置，导航和语义查询不得直接混用两套坐标。
 * @details 无内容快照时返回零位置；回退 AST 只保留行列，偏移置零以免误用旧 AST 的字节位置。
 */
SZrFilePosition ZrLanguageServer_Lsp_GetDocumentFilePosition(SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position) {
    SZrFileVersion *fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    SZrFileVersionContentSnapshot snapshot;
    SZrFilePosition filePosition;

    if (context != ZR_NULL &&
        ZrLanguageServer_FileVersionContentSnapshot_Acquire(context->state, fileVersion, &snapshot)) {
        filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                              snapshot.content,
                                                                              snapshot.contentLength);
        if (snapshot.usesFallbackAst) {
            filePosition.offset = 0;
        }
        ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
        return filePosition;
    }

    return ZrParser_FilePosition_Create(0, 0, 0);
}

/**
 * @brief 统一处理编辑器缓冲区与项目磁盘文件的解析、索引和语义缓存更新。
 * @details stdio 的 UpdateDocument 允许刷新项目并标记打开文档；项目扫描、元数据和跨文件查询以
 * allowProjectRefresh=false 载入依赖，避免递归触发项目刷新。失败时调用方不得继续信任旧分析器。
 */
TZrBool ZrLanguageServer_Lsp_UpdateDocumentCore(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize version,
                                                TZrBool allowProjectRefresh) {
    TZrBool analyzeSuccess;
    TZrBool preserveScopedQueryCache = ZR_FALSE;
    TZrBool retainCurrentAst = ZR_FALSE;
    TZrBool retainPreviousSemanticAst = ZR_FALSE;
    SZrSemanticAnalyzer *existingAnalyzer = ZR_NULL;
    SZrAstNode *retainedAst = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
        return ZR_TRUE;
    }

    /* 工程清单属于项目发现输入，不交给普通源码的语义分析器。 */
    if (lsp_string_ends_with_native(uri, ".zrp")) {
        if (!(allowProjectRefresh
                      ? ZrLanguageServer_IncrementalParser_UpdateOpenDocument(
                                state,
                                context->parser,
                                uri,
                                content,
                                contentLength,
                                version)
                      : ZrLanguageServer_IncrementalParser_UpdateFile(
                                state,
                                context->parser,
                                uri,
                                content,
                                contentLength,
                                version))) {
            return ZR_FALSE;
        }
        if (allowProjectRefresh) {
            (void)ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(state,
                                                                        context,
                                                                        uri,
                                                                        content,
                                                                        contentLength,
                                                                        ZR_TRUE);
        }
        return ZR_TRUE;
    }
    
    // 更新文件
    if (!(allowProjectRefresh
                  ? ZrLanguageServer_IncrementalParser_UpdateOpenDocument(
                            state,
                            context->parser,
                            uri,
                            content,
                            contentLength,
                            version)
                  : ZrLanguageServer_IncrementalParser_UpdateFile(
                            state,
                            context->parser,
                            uri,
                            content,
                            contentLength,
                            version))) {
        return ZR_FALSE;
    }

    {
        SZrFileVersion *fileVersion =
                ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
        existingAnalyzer =
                ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
        if (fileVersion != ZR_NULL &&
            fileVersion->hasIncrementalInfo &&
            !fileVersion->usesFallbackAst) {
            ZrLanguageServer_SemanticAnalyzer_ClassifyFileChange(
                    fileVersion->ast,
                    &fileVersion->lastChangeInfo);
        }
        if (fileVersion != ZR_NULL && fileVersion->isDirty &&
            existingAnalyzer != ZR_NULL) {
            preserveScopedQueryCache =
                    ZrLanguageServer_SemanticAnalyzer_PrepareScopedQueryCacheForChange(
                            state,
                            existingAnalyzer,
                            fileVersion->ast,
                            &fileVersion->lastChangeInfo,
                            &retainCurrentAst);
            retainPreviousSemanticAst = !fileVersion->usesFallbackAst &&
                                        fileVersion->ast != ZR_NULL &&
                                        existingAnalyzer->ast == fileVersion->ast &&
                                        existingAnalyzer->ownedAst == ZR_NULL &&
                                        existingAnalyzer->borrowedAst == ZR_NULL;
        }
    }
    
    /* 解析器需要保留上一棵 AST 时，先交出其所有权；后续历史快照或失败路径必须接管并释放。 */
    if (!((retainCurrentAst || retainPreviousSemanticAst)
                  ? ZrLanguageServer_IncrementalParser_ParseRetainingPreviousAst(
                        state,
                        context->parser,
                        uri,
                        &retainedAst)
                  : ZrLanguageServer_IncrementalParser_Parse(
                        state,
                        context->parser,
                        uri))) {
        if (preserveScopedQueryCache && existingAnalyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                    state,
                    existingAnalyzer);
        }
        if (retainedAst != ZR_NULL) {
            ZrParser_Ast_Free(state, retainedAst);
        }
        return ZR_FALSE;
    }
    
    {
        SZrFileVersion *fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrLspProjectIndex *projectIndex = ZR_NULL;

        if (fileVersion == ZR_NULL) {
            if (retainedAst != ZR_NULL) {
                if (existingAnalyzer != ZR_NULL) {
                    ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                            state,
                            existingAnalyzer);
                }
                ZrParser_Ast_Free(state, retainedAst);
            }
            return ZR_FALSE;
        }

        /* 语法错误时保留旧 AST 与分析器供恢复，但诊断入口只发布当前解析错误。 */
        if (fileVersion->parserDiagnostics.length > 0 && fileVersion->usesFallbackAst && existingAnalyzer != ZR_NULL) {
            return ZR_TRUE;
        }

        ast = fileVersion->ast;
        if (ast == ZR_NULL) {
            if (retainedAst != ZR_NULL) {
                ZrParser_Ast_Free(state, retainedAst);
            }
            ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, uri);
            return fileVersion->parserDiagnostics.length > 0;
        }

        if (retainedAst != ZR_NULL) {
            if (existingAnalyzer == ZR_NULL ||
                !ZrLanguageServer_LspSemanticSnapshotCache_Capture(
                        state,
                        context,
                        uri,
                        fileVersion,
                        existingAnalyzer,
                        retainedAst,
                        preserveScopedQueryCache)) {
                if (existingAnalyzer != ZR_NULL) {
                    ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                            state,
                            existingAnalyzer);
                }
                ZrParser_Ast_Free(state, retainedAst);
                preserveScopedQueryCache = ZR_FALSE;
            }
            retainedAst = ZR_NULL;
        }

        if (preserveScopedQueryCache && existingAnalyzer != ZR_NULL) {
            preserveScopedQueryCache =
                    ZrLanguageServer_SemanticAnalyzer_CommitScopedQueryCachePreservation(
                            state,
                            existingAnalyzer,
                            ast,
                            ZR_NULL);
        }

        analyzer = existingAnalyzer != ZR_NULL
                       ? existingAnalyzer
                       : ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
        if (analyzer == ZR_NULL) {
            return ZR_FALSE;
        }

        /* 编辑器修改会重建所属工程；依赖文件载入只沿用已有工程，避免索引刷新递归。 */
        projectIndex = allowProjectRefresh
                           ? ZrLanguageServer_LspProject_GetOrCreateForUri(state, context, uri)
                           : ZrLanguageServer_LspProject_FindProjectForUri(context, uri);
        if (allowProjectRefresh && projectIndex != ZR_NULL) {
            TZrBool refreshed = ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(state,
                                                                                       context,
                                                                                       uri,
                                                                                       content,
                                                                                       contentLength,
                                                                                       ZR_FALSE);
            if (!refreshed) {
                ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, uri);
                return ZR_FALSE;
            }
            analyzeSuccess = ZrLanguageServer_Lsp_ProjectAnalyzeDocument(state, context, uri, analyzer, ast);
            if (!analyzeSuccess) {
                ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, uri);
            } else {
                ZrLanguageServer_LspSemanticCacheLru_Touch(context, analyzer);
                ZrLanguageServer_LspSemanticCacheLru_Enforce(state, context);
            }
            return analyzeSuccess;
        }

        analyzeSuccess = ZrLanguageServer_Lsp_ProjectAnalyzeDocument(
                state, context, uri, analyzer, ast);
        if (!analyzeSuccess) {
            ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, uri);
            return ZR_FALSE;
        }

        ZrLanguageServer_LspSemanticCacheLru_Touch(context, analyzer);
        ZrLanguageServer_LspSemanticCacheLru_Enforce(state, context);

        return ZR_TRUE;
    }
}

/** @brief stdio 文档变更入口；与内部依赖载入不同，此入口会刷新所属工程。 */
TZrBool ZrLanguageServer_Lsp_UpdateDocument(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          const TZrChar *content,
                          TZrSize contentLength,
                          TZrSize version) {
    return ZrLanguageServer_Lsp_UpdateDocumentCore(state, context, uri, content, contentLength, version, ZR_TRUE);
}

/** @brief 为历史语义快照设置会话级存储上限，由缓存管理器负责逐出。 */
TZrBool ZrLanguageServer_Lsp_SetSemanticCacheStorageLimit(
        SZrState *state,
        SZrLspContext *context,
        TZrSize limitBytes) {
    return ZrLanguageServer_LspSemanticCacheLru_SetLimit(
            state,
            context,
            limitBytes);
}

/** @brief 将语义快照缓存的当前占用和逐出统计提供给诊断或测试调用方。 */
TZrBool ZrLanguageServer_Lsp_GetSemanticCacheStorageInfo(
        const SZrLspContext *context,
        SZrLspSemanticCacheStorageInfo *outInfo) {
    return ZrLanguageServer_LspSemanticCacheLru_GetInfo(context, outInfo);
}

/**
 * @brief 汇总当前文档的解析、语义与导入诊断供发布器和代码动作消费。
 * @details 虚拟声明页无诊断；回退 AST 时只发布当前解析错误，避免把旧语义问题投影到新文本。
 * TODO: 语义和导入诊断收集失败时仍返回成功及已有部分结果，需核对 stdio 发布器是否应感知降级。
 */
TZrBool ZrLanguageServer_Lsp_GetDiagnostics(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;
    SZrLspProjectIndex *projectIndex;
    SZrArray diagnostics;
    SZrArray importDiagnostics;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
        if (!result->isValid) {
            ZrCore_Array_Init(state, result, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
        }
        return ZR_TRUE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);

    for (TZrSize i = 0; i < fileVersion->parserDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&fileVersion->parserDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Lsp_AppendDiagnosticForDocument(state, context, uri, result, *diagPtr);
        }
    }

    if (fileVersion->parserDiagnostics.length > 0 && fileVersion->usesFallbackAst) {
        return ZR_TRUE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        if (fileVersion->ast != ZR_NULL && !fileVersion->usesFallbackAst) {
            SZrFileRange hintLocation;
            SZrDiagnostic *hintDiagnostic;

            hintLocation = ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 1, 1),
                                                     ZrParser_FilePosition_Create(0, 1, 1),
                                                     uri);
            hintDiagnostic = ZrLanguageServer_Diagnostic_New(
                state,
                ZR_DIAGNOSTIC_HINT,
                hintLocation,
                "Semantic analysis is unavailable for this document version; types and navigation may be stale. Edit to retry.",
                "zr_semantic_unavailable");
            if (hintDiagnostic != ZR_NULL) {
                ZrLanguageServer_Lsp_AppendDiagnosticForDocument(state, context, uri, result, hintDiagnostic);
                ZrLanguageServer_Diagnostic_Free(state, hintDiagnostic);
            }
        }
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(state, analyzer, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        return ZR_TRUE;
    }

    for (TZrSize i = 0; i < diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Lsp_AppendDiagnosticForDocument(state, context, uri, result, *diagPtr);
        }
    }

    ZrCore_Array_Free(state, &diagnostics);

    ZrCore_Array_Construct(&importDiagnostics);
    if (!ZrLanguageServer_LspProject_CollectImportDiagnostics(state,
                                                              context,
                                                              projectIndex,
                                                              uri,
                                                              &importDiagnostics)) {
        if (importDiagnostics.isValid) {
            for (TZrSize i = 0; i < importDiagnostics.length; i++) {
                SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&importDiagnostics, i);
                if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                    ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
                }
            }
            ZrCore_Array_Free(state, &importDiagnostics);
        }
        return ZR_TRUE;
    }

    if (importDiagnostics.isValid) {
        for (TZrSize i = 0; i < importDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&importDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrLanguageServer_Lsp_AppendDiagnosticForDocument(state, context, uri, result, *diagPtr);
                ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
            }
        }
        ZrCore_Array_Free(state, &importDiagnostics);
    }

    return ZR_TRUE;
}

/** @brief 普通悬停的最后兜底分支借用首个签名文本；借用只在 SignatureHelp 释放前有效。 */
static const TZrChar *lsp_signature_label_text(SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation **signaturePtr;

    if (help == ZR_NULL || help->signatures.length == 0) {
        return ZR_NULL;
    }

    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(&help->signatures, 0);
    if (signaturePtr == ZR_NULL || *signaturePtr == ZR_NULL || (*signaturePtr)->label == ZR_NULL) {
        return ZR_NULL;
    }

    return lsp_string_text_native((*signaturePtr)->label);
}

/**
 * @brief 将语义补全转换为协议补全，供 stdio_completion 的标准和完成项请求共用。
 * @details 在字符串、注释等非代码区域直接返回空列表；语义分析失败时 stdio_completion 也返回空列表。
 * 返回数组中的结构体由 stdio 层释放，字符串仍由 VM 管理。
 */
TZrBool ZrLanguageServer_Lsp_GetCompletion(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    SZrArray completions;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;
    SZrFilePosition filePosition;
    TZrBool isCodeSpan;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                              snapshot.content,
                                                                              snapshot.contentLength);
        if (snapshot.usesFallbackAst) {
            filePosition.offset = 0;
        }
        isCodeSpan = ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan(snapshot.content,
                                                                   snapshot.contentLength,
                                                                   filePosition.offset);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        if (!isCodeSpan) {
            return ZR_TRUE;
        }
    }
    
    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(state, context, uri, position, &completions)) {
        ZrCore_Array_Free(state, &completions);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 补全项
    for (TZrSize i = 0; i < completions.length; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(&completions, i);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            SZrCompletionItem *item = *itemPtr;
            
            SZrLspCompletionItem *lspItem = (SZrLspCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCompletionItem));
            if (lspItem != ZR_NULL) {
                lspItem->label = item->label;
                
                // 转换 kind 字符串到整数
                TZrInt32 kindValue = ZR_LSP_COMPLETION_ITEM_KIND_TEXT;
                if (item->kind != ZR_NULL) {
                    TZrNativeString kindStr;
                    TZrSize kindLen;
                    if (item->kind->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                        kindStr = ZrCore_String_GetNativeStringShort(item->kind);
                        kindLen = item->kind->shortStringLength;
                    } else {
                        kindStr = ZrCore_String_GetNativeString(item->kind);
                        kindLen = item->kind->longStringLength;
                    }
                    
                    // LSP CompletionItemKind 映射
                    if (kindLen == 8 && memcmp(kindStr, "function", 8) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_FUNCTION;
                    } else if (kindLen == 5 && memcmp(kindStr, "class", 5) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_CLASS;
                    } else if (kindLen == 8 && memcmp(kindStr, "variable", 8) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_VARIABLE;
                    } else if (kindLen == 6 && memcmp(kindStr, "struct", 6) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_STRUCT;
                    } else if (kindLen == 6 && memcmp(kindStr, "method", 6) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_METHOD;
                    } else if (kindLen == 9 && memcmp(kindStr, "interface", 9) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_INTERFACE;
                    } else if (kindLen == 8 && memcmp(kindStr, "property", 8) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_PROPERTY;
                    } else if (kindLen == 5 && memcmp(kindStr, "field", 5) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_FIELD;
                    } else if (kindLen == 6 && memcmp(kindStr, "module", 6) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_MODULE;
                    } else if (kindLen == 4 && memcmp(kindStr, "enum", 4) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_ENUM;
                    } else if (kindLen == 8 && memcmp(kindStr, "constant", 8) == 0) {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_CONSTANT;
                    } else {
                        kindValue = ZR_LSP_COMPLETION_ITEM_KIND_TEXT;
                    }
                }
                lspItem->kind = kindValue;
                lspItem->detail = item->detail;
                lspItem->documentation = item->documentation;
                lspItem->insertText = item->label;
                lspItem->insertTextFormat = ZrCore_String_Create(
                    state,
                    ZR_LSP_INSERT_TEXT_FORMAT_KIND_PLAINTEXT,
                    sizeof(ZR_LSP_INSERT_TEXT_FORMAT_KIND_PLAINTEXT) - 1
                );
                
                ZrCore_Array_Push(state, result, &lspItem);
            }
            ZrLanguageServer_CompletionItem_Free(state, item);
        }
    }
    
    ZrCore_Array_Free(state, &completions);
    return ZR_TRUE;
}

/**
 * @brief 按装饰器、元方法、属性、调用签名、语义目标和局部表达式的顺序生成普通悬停。
 * @details stdio_navigation 直接发布普通悬停，GetRichHover 又复用此入口；局部事实用于补充而非替换已解析的符号说明。
 * 回退 AST 的文本快照需由各分支释放，返回悬停容器由调用方释放。
 */
TZrBool ZrLanguageServer_Lsp_GetHover(SZrState *state,
                    SZrLspContext *context,
                    SZrString *uri,
                    SZrLspPosition position,
                    SZrLspHover **result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrFileRange signatureHoverRange;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot contentSnapshot = {0};
    SZrSymbol *symbol;
    SZrString *content;
    SZrLspHover *lspHover;
    SZrLspSignatureHelp *signatureHelp = ZR_NULL;
    const TZrChar *signatureLabel = ZR_NULL;
    TZrChar signatureHoverBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    SZrLspLocalSemanticQueryResult localQuery;
    TZrBool hasLocalQuery;
    TZrBool hasContentSnapshot = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    if (ZrLanguageServer_Lsp_TryGetDecoratorHover(state, context, uri, position, result)) {
        return ZR_TRUE;
    }

    if (ZrLanguageServer_Lsp_TryGetMetaMethodHover(state, context, uri, position, result)) {
        return ZR_TRUE;
    }
    
    // 获取分析器
    if (!ZrLanguageServer_LspSemanticQuery_TryGetAnalyzerForUri(
                state, context, uri, &analyzer)) {
        return ZR_FALSE;
    }
    
    // 转换位置
    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, fileRange);

    ZrLanguageServer_LspLocalSemanticQuery_Init(&localQuery);
    hasLocalQuery = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                        context,
                                                                        uri,
                                                                        position,
                                                                        &localQuery);
    if (hasLocalQuery &&
        localQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_DIAGNOSTIC_FAILURE) {
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_FALSE;
    }

    if (symbol != ZR_NULL && symbol->hasPropertyContract &&
        ZrLanguageServer_FileVersionContentSnapshot_Acquire(
                state,
                fileVersion,
                &contentSnapshot)) {
        content = ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(
                state,
                analyzer,
                symbol,
                contentSnapshot.content,
                contentSnapshot.contentLength);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &contentSnapshot);
        if (content != ZR_NULL) {
            lspHover = (SZrLspHover *)ZrCore_Memory_RawMalloc(
                    state->global,
                    sizeof(SZrLspHover));
            if (lspHover == ZR_NULL) {
                ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
                return ZR_FALSE;
            }
            ZrCore_Array_Init(state, &lspHover->contents, sizeof(SZrString *), 1U);
            ZrCore_Array_Push(state, &lspHover->contents, &content);
            lspHover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
                    context,
                    uri,
                    fileRange);
            *result = lspHover;
            if (hasLocalQuery &&
                localQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
                ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover(
                        state,
                        &localQuery,
                        lspHover);
            }
            ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
            return ZR_TRUE;
        }
    }

    if (ZrLanguageServer_LspCanonicalSignatureHelp_ResolveReceiverHover(
                state,
                context,
                analyzer,
                uri,
                fileRange,
                result) ||
        ZrLanguageServer_LspCanonicalSignatureHelp_ResolveExternalCallableHover(
                state,
                context,
                analyzer,
                uri,
                fileRange,
                result)) {
        if (analyzer->semanticContext != ZR_NULL && analyzer->symbolTable != ZR_NULL &&
            result != ZR_NULL && *result != ZR_NULL) {
            SZrParserSemanticCallQuery callQuery;
            if (ZrParser_SemanticQuery_CallAt(
                    analyzer->semanticContext, fileRange, ZR_NULL, &callQuery) &&
                callQuery.hasResolvedTarget &&
                callQuery.targetSymbolId != ZR_SEMANTIC_ID_INVALID) {
                SZrSymbol *target = ZrLanguageServer_SymbolTable_FindBySemanticId(
                    analyzer->symbolTable, callQuery.targetSymbolId);
                if (target != ZR_NULL &&
                    ZrLanguageServer_FileVersionContentSnapshot_Acquire(
                        state, fileVersion, &contentSnapshot)) {
                    SZrString **hoverContent = (SZrString **)ZrCore_Array_Get(
                        &(*result)->contents, 0);
                    if (hoverContent != ZR_NULL && *hoverContent != ZR_NULL) {
                        SZrString *comment = ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(
                            state, target, contentSnapshot.content, contentSnapshot.contentLength);
                        *hoverContent = lsp_append_markdown_section(state, *hoverContent, comment);
                    }
                    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &contentSnapshot);
                }
            }
        }
        if (hasLocalQuery &&
            localQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
            ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover(
                    state,
                    &localQuery,
                    *result);
        }
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_TRUE;
    }

    if (ZrLanguageServer_LspCanonicalSignatureHelp_HasUnavailableLocalCall(
                analyzer,
                fileRange)) {
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_FALSE;
    }

    {
        SZrLspSemanticQuery semanticQuery;

        ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
        if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery)) {
            if (ZrLanguageServer_LspSemanticQuery_BuildHover(state, context, &semanticQuery, result)) {
                if (result != ZR_NULL && *result != ZR_NULL) {
                    hasLocalQuery = lsp_refresh_local_hover_query(state, context, uri, position, &localQuery);
                    if (hasLocalQuery &&
                        localQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
                        ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover(state,
                                                                                   &localQuery,
                                                                                   *result);
                    }
                }
                ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
                ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
                return ZR_TRUE;
            }
        }
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    }

    if (symbol == ZR_NULL &&
        ZrLanguageServer_Lsp_GetSignatureHelp(state, context, uri, position, &signatureHelp) &&
        (signatureLabel = lsp_signature_label_text(signatureHelp)) != ZR_NULL) {
        snprintf(signatureHoverBuffer, sizeof(signatureHoverBuffer), "**call**\n\nSignature: %s", signatureLabel);
        content = ZrCore_String_Create(state, signatureHoverBuffer, strlen(signatureHoverBuffer));
        ZrLanguageServer_LspSignatureHelp_Free(state, signatureHelp);
        signatureHelp = ZR_NULL;
        if (content != ZR_NULL) {
            lspHover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
            if (lspHover == ZR_NULL) {
                ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
                return ZR_FALSE;
            }

            ZrCore_Array_Init(state, &lspHover->contents, sizeof(SZrString *), 1);
            ZrCore_Array_Push(state, &lspHover->contents, &content);
            signatureHoverRange = fileRange;
            (void)ZrLanguageServer_LspCanonicalSignatureHelp_TryGetResolvedCallReferenceRange(
                    analyzer,
                    fileRange,
                    &signatureHoverRange);
            lspHover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
                    context,
                    uri,
                    signatureHoverRange);
            *result = lspHover;
            ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
            return ZR_TRUE;
        }
    }
    if (signatureHelp != ZR_NULL) {
        ZrLanguageServer_LspSignatureHelp_Free(state, signatureHelp);
        signatureHelp = ZR_NULL;
    }

    hasLocalQuery = lsp_refresh_local_hover_query(state, context, uri, position, &localQuery);
    if (hasLocalQuery &&
        localQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_DIAGNOSTIC_FAILURE) {
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_FALSE;
    }

    if (hasLocalQuery &&
        !lsp_position_is_identifier_char(context, uri, position) &&
        ZrLanguageServer_LspLocalSemanticQuery_BuildHoverForDocument(
            state,
            context,
            uri,
            &localQuery,
            result)) {
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_TRUE;
    }

    if (symbol == ZR_NULL &&
        hasLocalQuery &&
        ZrLanguageServer_LspLocalSemanticQuery_BuildHoverForDocument(
            state,
            context,
            uri,
            &localQuery,
            result)) {
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_TRUE;
    }

    if (symbol != ZR_NULL &&
               ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &contentSnapshot)) {
        hasContentSnapshot = ZR_TRUE;
        content = ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(state,
                                                                        analyzer,
                                                                        symbol,
                                                                        contentSnapshot.content,
                                                                        contentSnapshot.contentLength);
        if (content == ZR_NULL) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &contentSnapshot);
            ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
            return ZR_FALSE;
        }
    } else {
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_FALSE;
    }

    if (symbol != ZR_NULL &&
        (hasContentSnapshot ||
         ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &contentSnapshot))) {
        SZrString *comment = ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(state,
                                                                                symbol,
                                                                                contentSnapshot.content,
                                                                                contentSnapshot.contentLength);
        content = lsp_append_markdown_section(state, content, comment);
        if (!hasContentSnapshot) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &contentSnapshot);
        }
    }
    if (hasContentSnapshot) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &contentSnapshot);
    }
    
    // 转换为 LSP 悬停
    lspHover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (lspHover == ZR_NULL) {
        ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
        return ZR_FALSE;
    }
    
    ZrCore_Array_Init(state, &lspHover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &lspHover->contents, &content);
    lspHover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
        context,
        uri,
        symbol != ZR_NULL ? ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol)
                          : fileRange);
    
    *result = lspHover;
    hasLocalQuery = lsp_refresh_local_hover_query(state, context, uri, position, &localQuery);
    if (hasLocalQuery &&
        localQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
        ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover(state, &localQuery, *result);
    }
    ZrLanguageServer_LspLocalSemanticQuery_Clear(&localQuery);
    return ZR_TRUE;
}

/**
 * @brief 为扩展的富悬停协议复用普通悬停决策，再按语义角色拆分 Markdown。
 * @details stdio_navigation 的扩展请求调用此函数；结果需交给 FreeRichHover，而非普通悬停释放器。
 */
TZrBool ZrLanguageServer_Lsp_GetRichHover(SZrState *state,
                                          SZrLspContext *context,
                                          SZrString *uri,
                                          SZrLspPosition position,
                                          SZrLspRichHover **result) {
    SZrLspHover *hover = ZR_NULL;
    SZrString **contentPtr;
    const TZrChar *markdown;

    if (result != ZR_NULL) {
        *result = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_GetHover(state, context, uri, position, &hover) || hover == ZR_NULL) {
        return ZR_FALSE;
    }

    if (hover->contents.length == 0) {
        lsp_hover_free_internal(state, hover);
        return ZR_FALSE;
    }

    contentPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, 0);
    markdown = (contentPtr != ZR_NULL && *contentPtr != ZR_NULL) ? lsp_string_text_native(*contentPtr) : ZR_NULL;
    if (markdown == ZR_NULL || markdown[0] == '\0') {
        lsp_hover_free_internal(state, hover);
        return ZR_FALSE;
    }

    if (!lsp_build_rich_hover_from_markdown(state, markdown, hover->range, result)) {
        lsp_hover_free_internal(state, hover);
        return ZR_FALSE;
    }

    lsp_hover_free_internal(state, hover);
    return ZR_TRUE;
}

/**
 * @brief 为普通源码和原生虚拟声明页提供统一的定义位置。
 * @details 虚拟模块链接先用描述符解析，装饰器与 super 构造调用有专用路径，其余目标交给语义查询。
 * stdio_navigation 消费位置数组；解析失败表示没有可导航的定义。
 */
TZrBool ZrLanguageServer_Lsp_GetDefinition(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    SZrLspSemanticQuery semanticQuery;
    const ZrLibModuleDescriptor *virtualDescriptor = ZR_NULL;
    SZrLspVirtualDeclarationMatch virtualMatch;
    TZrChar virtualModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *targetUri;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri) &&
        lsp_resolve_virtual_descriptor(state,
                                       context,
                                       uri,
                                       &virtualDescriptor,
                                       ZR_NULL,
                                       virtualModuleName,
                                       sizeof(virtualModuleName)) &&
        ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(state,
                                                                       virtualDescriptor,
                                                                       uri,
                                                                       position,
                                                                       &virtualMatch) &&
        virtualMatch.kind == ZR_LSP_VIRTUAL_DECLARATION_MODULE_LINK &&
        virtualMatch.targetModuleName != ZR_NULL) {
        const ZrLibModuleDescriptor *targetDescriptor = ZR_NULL;
        SZrFileRange targetRange = {0};

        SZrLspProjectIndex *owner = ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(context, uri);
        SZrString *targetModule = ZrCore_String_Create(state,
                (TZrNativeString)virtualMatch.targetModuleName, strlen(virtualMatch.targetModuleName));
        if (!ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeUri(
                    state, context, owner, targetModule, &targetUri) || targetUri == ZR_NULL ||
            !lsp_resolve_virtual_descriptor(state, context, targetUri, &targetDescriptor,
                    ZR_NULL, virtualModuleName, sizeof(virtualModuleName)) ||
            !ZrLanguageServer_LspNativeDeclarationProjection_Find(
                    state, targetDescriptor, targetUri, ZR_LSP_VIRTUAL_DECLARATION_MODULE,
                    targetDescriptor, &targetRange)) {
            return ZR_FALSE;
        }

        return lsp_append_location_result(state,
                                          result,
                                          targetUri,
                                          ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
                                              context,
                                              targetUri,
                                              targetRange));
    }
    
    if (ZrLanguageServer_Lsp_TryGetDecoratorDefinition(state, context, uri, position, result)) {
        return ZR_TRUE;
    }

    if (ZrLanguageServer_Lsp_TryGetSuperConstructorDefinition(state, context, uri, position, result)) {
        return ZR_TRUE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery) &&
        ZrLanguageServer_LspSemanticQuery_AppendDefinitions(state, context, &semanticQuery, result)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_TRUE;
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    return ZR_FALSE;
}

/**
 * @brief 按语义身份收集跨文件引用，并允许客户端选择是否包含声明位置。
 * @details stdio_navigation 和 linked_editing 共用；遍历前后均检查取消，不能发布部分结果。
 */
TZrBool ZrLanguageServer_Lsp_FindReferences(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          SZrLspPosition position,
                          TZrBool includeDeclaration,
                          SZrArray *result) {
    SZrLspSemanticQuery semanticQuery;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_Lsp_TryFindSuperConstructorReferences(state,
                                                               context,
                                                               uri,
                                                               position,
                                                               includeDeclaration,
                                                               result)) {
        return ZR_TRUE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery) &&
        !ZrLanguageServer_LspContext_IsRequestCancellationRequested(context) &&
        ZrLanguageServer_LspSemanticQuery_AppendReferences(state,
                                                           context,
                                                           &semanticQuery,
                                                           includeDeclaration,
                                                           result) &&
        !ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_TRUE;
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    return ZR_FALSE;
}

/**
 * @brief 为 stdio_rename 收集可编辑的语义引用范围，再交由其生成带版本校验的 WorkspaceEdit。
 * @details newName 在此只用于接口校验，实际文本替换由 stdio_rename 完成；仅支持稳定身份的本地符号及有源声明的工程成员。
 * TODO: 此层未验证新名称是否为合法标识符或与现有声明冲突；需明确验证职责属于协议层还是语言语义层。
 */
TZrBool ZrLanguageServer_Lsp_Rename(SZrState *state,
                  SZrLspContext *context,
                  SZrString *uri,
                  SZrLspPosition position,
                  SZrString *newName,
                  SZrArray *result) {
    SZrLspSemanticQuery semanticQuery;
    TZrBool resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || newName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    (void)newName;
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    resolved = ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery);
    if (!resolved) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_FALSE;
    }

    if (!lsp_semantic_query_append_rename_locations(state, context, &semanticQuery, result) ||
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_FALSE;
    }

    lsp_normalize_rename_location_ranges(context, result);
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_FALSE;
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    return result->length > 0;
}

/**
 * @brief 从当前分析器的作用域树生成文档大纲，供 stdio_navigation 发布。
 * @details 只包含当前 URI 对应文件的顶级声明和类型成员；取消时返回失败，调用方应丢弃部分数组。
 */
TZrBool ZrLanguageServer_Lsp_GetDocumentSymbols(SZrState *state,
                              SZrLspContext *context,
                              SZrString *uri,
                              SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    TZrSize scopeIndex;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (analyzer->symbolTable->globalScope == ZR_NULL) {
        return ZR_TRUE;
    }

    for (scopeIndex = 0; scopeIndex < analyzer->symbolTable->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr =
            (SZrSymbolScope **)ZrCore_Array_Get(&analyzer->symbolTable->allScopes, scopeIndex);
        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            return ZR_FALSE;
        }
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                return ZR_FALSE;
            }
            if (symbolPtr != ZR_NULL &&
                *symbolPtr != ZR_NULL &&
                lsp_should_include_document_symbol(analyzer->symbolTable, *scopePtr, *symbolPtr, uri)) {
                SZrSymbol *symbol = *symbolPtr;
                SZrLspSymbolInformation *info =
                    ZrLanguageServer_Lsp_CreateSymbolInformationForDocument(state, context, uri, symbol);
                if (info != ZR_NULL) {
                    ZrCore_Array_Push(state, result, &info);
                }
            }
        }
    }

    return ZR_TRUE;
}

/**
 * @brief 合并项目索引符号与未索引的打开文档符号，供全工作区搜索。
 * @details 项目索引负责持久文件，分析器映射只补充打开的游离缓冲区；取消后不得发布部分结果。
 */
TZrBool ZrLanguageServer_Lsp_GetWorkspaceSymbols(SZrState *state,
                               SZrLspContext *context,
                               SZrString *query,
                               SZrArray *result) {
    TZrSize bucketIndex;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    /* 已索引项目是工作区搜索的主数据源；后面的 URI 映射仅提供打开文件的补集。 */
    ZrLanguageServer_Lsp_ProjectAppendWorkspaceSymbols(state, context, query, result);
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }

    if (!context->uriToAnalyzerMap.isValid || context->uriToAnalyzerMap.buckets == ZR_NULL) {
        return ZR_TRUE;
    }

    for (bucketIndex = 0; bucketIndex < context->uriToAnalyzerMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                return ZR_FALSE;
            }
            if (pair->key.type == ZR_VALUE_TYPE_NULL ||
                lsp_project_has_indexed_record_for_uri(context,
                                                       (SZrString *)ZrCore_Value_GetRawObject(&pair->key)) ||
                !lsp_document_is_open_overlay(context,
                                              (SZrString *)ZrCore_Value_GetRawObject(&pair->key))) {
                pair = pair->next;
                continue;
            }

            if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                if (analyzer != ZR_NULL && analyzer->symbolTable != ZR_NULL &&
                    analyzer->symbolTable->globalScope != ZR_NULL) {
                    TZrSize symbolIndex;
                    for (symbolIndex = 0; symbolIndex < analyzer->symbolTable->globalScope->symbols.length; symbolIndex++) {
                        SZrSymbol **symbolPtr =
                            (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, symbolIndex);
                        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                            return ZR_FALSE;
                        }
                        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                            SZrSymbol *symbol = *symbolPtr;
                            if (ZrLanguageServer_Lsp_StringContainsCaseInsensitive(symbol->name, query)) {
                                SZrString *symbolUri = symbol->location.source != ZR_NULL
                                                       ? symbol->location.source
                                                       : (SZrString *)ZrCore_Value_GetRawObject(&pair->key);
                                SZrLspSymbolInformation *info =
                                    ZrLanguageServer_Lsp_CreateSymbolInformationForDocument(state,
                                                                                            context,
                                                                                            symbolUri,
                                                                                            symbol);
                                if (info != ZR_NULL) {
                                    ZrCore_Array_Push(state, result, &info);
                                }
                            }
                        }
                    }
                }
            }
            pair = pair->next;
        }
    }

    return ZR_TRUE;
}

/** @brief 为同文档高亮查询复用 super 构造调用特例及通用语义引用身份。 */
TZrBool ZrLanguageServer_Lsp_GetDocumentHighlights(SZrState *state,
                                  SZrLspContext *context,
                                  SZrString *uri,
                                  SZrLspPosition position,
                                  SZrArray *result) {
    SZrLspSemanticQuery semanticQuery;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_Lsp_TryGetSuperConstructorDocumentHighlights(state,
                                                                      context,
                                                                      uri,
                                                                      position,
                                                                      result)) {
        return ZR_TRUE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery) &&
        ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(state, context, &semanticQuery, result)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_TRUE;
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    return ZR_FALSE;
}

/**
 * @brief 在编辑前确认目标可由 Rename 处理，并返回当前标识符范围与展示占位名。
 * @details stdio_rename 以此响应客户端预检；实际编辑仍需重新解析目标并捕获文档快照。
 */
TZrBool ZrLanguageServer_Lsp_PrepareRename(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrLspRange *outRange,
                         SZrString **outPlaceholder) {
    SZrLspSemanticQuery semanticQuery;
    SZrFileRange identifierRange;
    SZrString *placeholder;
    TZrBool resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outRange == ZR_NULL ||
        outPlaceholder == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    resolved = ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery);
    placeholder = lsp_semantic_query_rename_placeholder(&semanticQuery);
    if (!resolved || placeholder == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_FALSE;
    }

    if (lsp_try_get_identifier_range_at_position(context, uri, position, &identifierRange)) {
        *outRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, identifierRange);
    } else if (semanticQuery.kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL && semanticQuery.symbol != ZR_NULL) {
        *outRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context,
            uri,
            ZrLanguageServer_Lsp_GetSymbolLookupRange(semanticQuery.symbol));
    } else {
        *outRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context,
                                                                       uri,
                                                                       semanticQuery.resolvedMember.declarationRange);
    }
    *outPlaceholder = placeholder;
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    return ZR_TRUE;
}

/**
 * @brief 将原生模块描述符投影为只读虚拟声明文本，供 stdio_navigation 的虚拟文档请求。
 * @details 仅接受已识别的声明 URI；普通文件由文档同步与项目索引管理。
 */
TZrBool ZrLanguageServer_Lsp_GetNativeDeclarationDocument(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri,
                                                          SZrString **outText) {
    const ZrLibModuleDescriptor *descriptor = ZR_NULL;
    TZrChar moduleNameBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outText != ZR_NULL) {
        *outText = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outText == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri) ||
        !lsp_resolve_virtual_descriptor(state,
                                       context,
                                       uri,
                                       &descriptor,
                                       ZR_NULL,
                                       moduleNameBuffer,
                                       sizeof(moduleNameBuffer)) ||
        descriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText(state, descriptor, uri, outText) &&
           *outText != ZR_NULL;
}

/**
 * @brief 为项目模块浏览视图汇总源模块、FFI 包装、二进制和原生导入模块。
 * @details stdio_project 使用导航 URI 与范围构造客户端树；需先确保源图扫描完成，并用 FreeProjectModules 释放摘要。
 */
TZrBool ZrLanguageServer_Lsp_GetProjectModules(SZrState *state,
                                               SZrLspContext *context,
                                               SZrString *projectUri,
                                               SZrArray *result) {
    SZrLspProjectIndex *projectIndex;
    SZrLspRange fileEntryRange;

    if (state == ZR_NULL || context == ZR_NULL || projectUri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspProjectModuleSummary *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    projectIndex = ZrLanguageServer_LspProject_GetOrCreateByProjectUri(state, context, projectUri);
    if (projectIndex == ZR_NULL ||
        !ZrLanguageServer_LspProject_EnsureScannedSourceGraph(state, context, projectIndex)) {
        return ZR_FALSE;
    }

    fileEntryRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
        context,
        ZR_NULL,
        ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 1, 1),
                                  ZrParser_FilePosition_Create(0, 1, 1),
                                  ZR_NULL));

    for (TZrSize fileIndex = 0; fileIndex < projectIndex->files.length; fileIndex++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fileIndex);

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL || (*recordPtr)->moduleName == ZR_NULL) {
            continue;
        }

        if (!lsp_project_modules_append_summary(
                state,
                result,
                (*recordPtr)->isFfiWrapperSource ? ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER
                                                 : ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE,
                projectIndex->project != ZR_NULL && projectIndex->project->entry != ZR_NULL &&
                    ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->moduleName, projectIndex->project->entry),
                (*recordPtr)->moduleName,
                (*recordPtr)->moduleName,
                (*recordPtr)->path,
                (*recordPtr)->uri,
                lsp_project_modules_source_entry_range(context, *recordPtr))) {
            return ZR_FALSE;
        }
    }

    for (TZrSize fileIndex = 0; fileIndex < projectIndex->files.length; fileIndex++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fileIndex);
        SZrArray moduleNames;

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL || (*recordPtr)->uri == ZR_NULL) {
            continue;
        }

        ZrCore_Array_Init(state, &moduleNames, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        if (!ZrLanguageServer_LspProject_CollectImportModuleNamesForUri(state,
                                                                        context,
                                                                        (*recordPtr)->uri,
                                                                        &moduleNames)) {
            ZrCore_Array_Free(state, &moduleNames);
            continue;
        }

        for (TZrSize bindingIndex = 0; bindingIndex < moduleNames.length; bindingIndex++) {
            SZrString **moduleNamePtr = (SZrString **)ZrCore_Array_Get(&moduleNames, bindingIndex);
            SZrLspResolvedImportedModule resolved;
            SZrString *navigationUri = ZR_NULL;
            SZrString *description;
            SZrLspRange navigationRange = fileEntryRange;

            if (moduleNamePtr == ZR_NULL || *moduleNamePtr == ZR_NULL) {
                continue;
            }

            memset(&resolved, 0, sizeof(resolved));
            if (!ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                         ZR_NULL,
                                                                         projectIndex,
                                                                         *moduleNamePtr,
                                                                         &resolved)) {
                continue;
            }

            if ((resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE ||
                 resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER) &&
                resolved.sourceRecord != ZR_NULL) {
                if (!lsp_project_modules_append_summary(
                        state,
                        result,
                        resolved.sourceKind,
                        projectIndex->project != ZR_NULL && projectIndex->project->entry != ZR_NULL &&
                            ZrLanguageServer_Lsp_StringsEqual(resolved.sourceRecord->moduleName,
                                                              projectIndex->project->entry),
                        resolved.sourceRecord->moduleName,
                        resolved.sourceRecord->moduleName,
                        resolved.sourceRecord->path,
                        resolved.sourceRecord->uri,
                        lsp_project_modules_source_entry_range(
                                context, resolved.sourceRecord))) {
                    ZrCore_Array_Free(state, &moduleNames);
                    return ZR_FALSE;
                }
                continue;
            }

            if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA) {
                if (!ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(state,
                                                                               projectIndex,
                                                                               resolved.moduleName,
                                                                               &navigationUri) ||
                    navigationUri == ZR_NULL) {
                    continue;
                }
            } else if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN ||
                       resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
                if (!ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeUri(
                            state, context, projectIndex, resolved.moduleName, &navigationUri) ||
                    navigationUri == ZR_NULL) {
                    continue;
                }
                navigationRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
                    context,
                    navigationUri,
                    ZrLanguageServer_LspVirtualDocuments_ModuleEntryRange(navigationUri));
            } else {
                continue;
            }

            description = lsp_create_const_string(state,
                                                  ZrLanguageServer_LspModuleMetadata_SourceKindLabel(
                                                      (EZrLspImportedModuleSourceKind)resolved.sourceKind));
            if (!lsp_project_modules_append_summary(state,
                                                    result,
                                                    resolved.sourceKind,
                                                    ZR_FALSE,
                                                    resolved.moduleName,
                                                    resolved.moduleName,
                                                    description,
                                                    navigationUri,
                                                    navigationRange)) {
                ZrCore_Array_Free(state, &moduleNames);
                return ZR_FALSE;
            }
        }
        ZrCore_Array_Free(state, &moduleNames);
    }

    return ZR_TRUE;
}

/** @brief 释放 GetProjectModules 返回的摘要容器；内部 VM 字符串由 GC 管理。 */
void ZrLanguageServer_Lsp_FreeProjectModules(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspProjectModuleSummary **summaryPtr =
            (SZrLspProjectModuleSummary **)ZrCore_Array_Get(result, index);
        if (summaryPtr != ZR_NULL && *summaryPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *summaryPtr, sizeof(SZrLspProjectModuleSummary));
        }
    }

    ZrCore_Array_Free(state, result);
}

/** @brief 释放 GetRichHover 的角色字段及结果容器；字段字符串由 VM 管理。 */
void ZrLanguageServer_Lsp_FreeRichHover(SZrState *state, SZrLspRichHover *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < result->sections.length; index++) {
        SZrLspRichHoverSection **sectionPtr =
            (SZrLspRichHoverSection **)ZrCore_Array_Get(&result->sections, index);
        if (sectionPtr != ZR_NULL && *sectionPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *sectionPtr, sizeof(SZrLspRichHoverSection));
        }
    }

    ZrCore_Array_Free(state, &result->sections);
    ZrCore_Memory_RawFree(state->global, result, sizeof(SZrLspRichHover));
}
