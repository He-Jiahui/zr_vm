//
// Created by Auto on 2025/01/XX.
//

#include "lsp_interface_internal.h"
#include "lsp_virtual_documents.h"
#include "lsp_semantic_query.h"

#include "zr_vm_parser/compiler.h"

#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#if defined(ZR_VM_LSP_HAS_NETWORK_LIB)
#include "zr_vm_lib_network/module.h"
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
static TZrBool lsp_should_include_document_symbol(SZrSymbolTable *table,
                                                  SZrSymbolScope *scope,
                                                  SZrSymbol *symbol,
                                                  SZrString *uri);

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

static const TZrChar *lsp_string_text_native(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static SZrString *lsp_create_const_string(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

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
                                                                        ZR_NULL,
                                                                        uri,
                                                                        outDescriptor,
                                                                        outSourceKind,
                                                                        moduleNameBuffer,
                                                                        bufferSize) &&
           *outDescriptor != ZR_NULL;
}

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

static TZrBool lsp_should_include_document_symbol(SZrSymbolTable *table,
                                                  SZrSymbolScope *scope,
                                                  SZrSymbol *symbol,
                                                  SZrString *uri) {
    if (table == ZR_NULL || scope == ZR_NULL || symbol == ZR_NULL || symbol->location.source == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(symbol->location.source, uri)) {
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
    context->analyzer = ZR_NULL; // 延迟创建，每个文件一个分析器
    context->uriToAnalyzerMap.buckets = ZR_NULL;
    context->uriToAnalyzerMap.bucketSize = 0;
    context->uriToAnalyzerMap.elementCount = 0;
    context->uriToAnalyzerMap.capacity = 0;
    context->uriToAnalyzerMap.resizeThreshold = 0;
    context->uriToAnalyzerMap.isValid = ZR_FALSE;
    ZrCore_HashSet_Init(state, &context->uriToAnalyzerMap, ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2);
    ZrCore_Array_Init(state,
                      &context->projectIndexes,
                      sizeof(SZrLspProjectIndex *),
                      ZR_LSP_PROJECT_INDEX_INITIAL_CAPACITY);

    ZrVmLibMath_Register(state->global);
    ZrVmLibSystem_Register(state->global);
    ZrVmLibContainer_Register(state->global);
    ZrVmLibFfi_Register(state->global);
#if defined(ZR_VM_LSP_HAS_NETWORK_LIB)
    ZrVmLibNetwork_Register(state->global);
#endif
    
    if (context->parser == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, context, sizeof(SZrLspContext));
        return ZR_NULL;
    }
    
    return context;
}

// 释放 LSP 上下文
void ZrLanguageServer_LspContext_Free(SZrState *state, SZrLspContext *context) {
    if (state == ZR_NULL || context == ZR_NULL) {
        return;
    }

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
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrCore_Memory_RawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            context->uriToAnalyzerMap.buckets[i] = ZR_NULL;
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

    ZrLanguageServer_Lsp_ProjectIndexes_Free(state, context);
    ZrCore_Memory_RawFree(state->global, context, sizeof(SZrLspContext));
}

// 获取或创建分析器
SZrSemanticAnalyzer *ZrLanguageServer_Lsp_GetOrCreateAnalyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从哈希表中查找
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        // 从原生指针中获取 SZrSemanticAnalyzer
        return (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
    }
    
    // 创建新分析器
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer != ZR_NULL) {
        // 添加到哈希表
        SZrHashKeyValuePair *newPair = ZrCore_HashSet_Add(state, &context->uriToAnalyzerMap, &key);
        if (newPair != ZR_NULL) {
            // 将 SZrSemanticAnalyzer 指针存储为原生指针
            SZrTypeValue value;
            ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)analyzer);
            ZrCore_Value_Copy(state, &newPair->value, &value);
        }
    }
    
    return analyzer;
}

SZrSemanticAnalyzer *ZrLanguageServer_Lsp_FindAnalyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        return (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
    }

    return ZR_NULL;
}

void ZrLanguageServer_Lsp_RemoveAnalyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrSemanticAnalyzer *analyzer;
    SZrTypeValue key;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    ZrCore_HashSet_Remove(state, &context->uriToAnalyzerMap, &key);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
}

SZrFileVersion *ZrLanguageServer_Lsp_GetDocumentFileVersion(SZrLspContext *context, SZrString *uri) {
    if (context == ZR_NULL || context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLanguageServer_IncrementalParser_GetFileVersion(context->parser, uri);
}

SZrFilePosition ZrLanguageServer_Lsp_GetDocumentFilePosition(SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position) {
    SZrFileVersion *fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);

    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        SZrFilePosition filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                                              fileVersion->content,
                                                                                              fileVersion->contentLength);
        if (fileVersion->usesFallbackAst) {
            filePosition.offset = 0;
        }
        return filePosition;
    }

    return ZrLanguageServer_LspPosition_ToFilePosition(position);
}

TZrBool ZrLanguageServer_Lsp_UpdateDocumentCore(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize version,
                                                TZrBool allowProjectRefresh) {
    TZrBool analyzeSuccess;
    SZrSemanticAnalyzer *existingAnalyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
        return ZR_TRUE;
    }

    if (lsp_string_ends_with_native(uri, ".zrp")) {
        return allowProjectRefresh
                   ? ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(state, context, uri, content, contentLength)
                   : ZR_TRUE;
    }
    
    // 更新文件
    if (!ZrLanguageServer_IncrementalParser_UpdateFile(state, context->parser, uri, content, contentLength, version)) {
        return ZR_FALSE;
    }
    
    // 重新解析
    if (!ZrLanguageServer_IncrementalParser_Parse(state, context->parser, uri)) {
        return ZR_FALSE;
    }
    
    {
        SZrFileVersion *fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;
        SZrLspProjectIndex *projectIndex = ZR_NULL;

        if (fileVersion == ZR_NULL) {
            return ZR_FALSE;
        }

        existingAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
        if (fileVersion->parserDiagnostics.length > 0 && fileVersion->usesFallbackAst && existingAnalyzer != ZR_NULL) {
            return ZR_TRUE;
        }

        ast = fileVersion->ast;
        if (ast == ZR_NULL) {
            ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, uri);
            return fileVersion->parserDiagnostics.length > 0;
        }

        analyzer = existingAnalyzer != ZR_NULL
                       ? existingAnalyzer
                       : ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
        if (analyzer == ZR_NULL) {
            return ZR_FALSE;
        }

        projectIndex = allowProjectRefresh
                           ? ZrLanguageServer_LspProject_GetOrCreateForUri(state, context, uri)
                           : ZrLanguageServer_LspProject_FindProjectForUri(context, uri);
        if (allowProjectRefresh && projectIndex != ZR_NULL) {
            return ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(state,
                                                                         context,
                                                                         uri,
                                                                         content,
                                                                         contentLength);
        }

        analyzeSuccess = projectIndex != ZR_NULL
                             ? ZrLanguageServer_Lsp_ProjectAnalyzeDocument(state, context, uri, analyzer, ast)
                             : ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
        if (!analyzeSuccess) {
            return ZR_FALSE;
        }

        return ZR_TRUE;
    }
}

TZrBool ZrLanguageServer_Lsp_UpdateDocument(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          const TZrChar *content,
                          TZrSize contentLength,
                          TZrSize version) {
    return ZrLanguageServer_Lsp_UpdateDocumentCore(state, context, uri, content, contentLength, version, ZR_TRUE);
}

// 获取诊断
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
            ZrLanguageServer_Lsp_AppendDiagnostic(state, result, *diagPtr);
        }
    }

    if (fileVersion->parserDiagnostics.length > 0 && fileVersion->usesFallbackAst) {
        return ZR_TRUE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(state, analyzer, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Lsp_AppendDiagnostic(state, result, *diagPtr);
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
        return ZR_FALSE;
    }

    if (importDiagnostics.isValid) {
        for (TZrSize i = 0; i < importDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&importDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrLanguageServer_Lsp_AppendDiagnostic(state, result, *diagPtr);
                ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
            }
        }
        ZrCore_Array_Free(state, &importDiagnostics);
    }

    return ZR_TRUE;
}

// 获取补全
TZrBool ZrLanguageServer_Lsp_GetCompletion(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    SZrArray completions;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
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
        }
    }
    
    ZrCore_Array_Free(state, &completions);
    return ZR_TRUE;
}

// 获取悬停信息
TZrBool ZrLanguageServer_Lsp_GetHover(SZrState *state,
                    SZrLspContext *context,
                    SZrString *uri,
                    SZrLspPosition position,
                    SZrLspHover **result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrFileVersion *fileVersion;
    SZrSymbol *symbol;
    SZrString *content;
    SZrHoverInfo *hoverInfo = ZR_NULL;
    SZrLspHover *lspHover;

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
    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, fileRange);

    {
        SZrLspSemanticQuery semanticQuery;

        ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
        if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery) &&
            ZrLanguageServer_LspSemanticQuery_BuildHover(state, context, &semanticQuery, result)) {
            ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
            return ZR_TRUE;
        }
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    }

    if (ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, fileRange, &hoverInfo) &&
        hoverInfo != ZR_NULL &&
        hoverInfo->contents != ZR_NULL) {
        content = hoverInfo->contents;
    } else if (symbol != ZR_NULL && fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        content = ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(state,
                                                                        symbol,
                                                                        fileVersion->content,
                                                                        fileVersion->contentLength);
        if (content == ZR_NULL) {
            return ZR_FALSE;
        }
    } else {
        return ZR_FALSE;
    }

    if (symbol != ZR_NULL && fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        SZrString *comment = ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(state,
                                                                                symbol,
                                                                                fileVersion->content,
                                                                                fileVersion->contentLength);
        content = lsp_append_markdown_section(state, content, comment);
    }
    
    // 转换为 LSP 悬停
    lspHover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (lspHover == ZR_NULL) {
        if (hoverInfo != ZR_NULL) {
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        }
        return ZR_FALSE;
    }
    
    ZrCore_Array_Init(state, &lspHover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &lspHover->contents, &content);
    lspHover->range = ZrLanguageServer_LspRange_FromFileRange(
        symbol != ZR_NULL ? ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol)
                          : (hoverInfo != ZR_NULL ? hoverInfo->range : fileRange));
    
    *result = lspHover;
    return ZR_TRUE;
}

// 获取定义位置
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
        targetUri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(state, virtualMatch.targetModuleName);
        if (targetUri == ZR_NULL) {
            return ZR_FALSE;
        }

        return lsp_append_location_result(state,
                                          result,
                                          targetUri,
                                          ZrLanguageServer_LspRange_FromFileRange(
                                              ZrLanguageServer_LspVirtualDocuments_ModuleEntryRange(targetUri)));
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

// 查找引用
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
        ZrLanguageServer_LspSemanticQuery_AppendReferences(state,
                                                           context,
                                                           &semanticQuery,
                                                           includeDeclaration,
                                                           result)) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_TRUE;
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    return ZR_FALSE;
}

// 重命名符号
TZrBool ZrLanguageServer_Lsp_Rename(SZrState *state,
                  SZrLspContext *context,
                  SZrString *uri,
                  SZrLspPosition position,
                  SZrString *newName,
                  SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || newName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    SZrFileRange fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    
    // 查找符号
    SZrSymbol *symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取所有引用（包括定义）
    SZrArray references;
    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 位置（所有需要重命名的位置）
    for (TZrSize i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            
            SZrLspLocation *location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
            if (location != ZR_NULL) {
                location->uri = ref->location.source;
                location->range = ZrLanguageServer_LspRange_FromFileRange(ref->location);
                
                ZrCore_Array_Push(state, result, &location);
            }
        }
    }
    
    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

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
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr != ZR_NULL &&
                *symbolPtr != ZR_NULL &&
                lsp_should_include_document_symbol(analyzer->symbolTable, *scopePtr, *symbolPtr, uri)) {
                SZrSymbol *symbol = *symbolPtr;
                SZrLspSymbolInformation *info = ZrLanguageServer_Lsp_CreateSymbolInformation(state, symbol);
                if (info != ZR_NULL) {
                    ZrCore_Array_Push(state, result, &info);
                }
            }
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetWorkspaceSymbols(SZrState *state,
                               SZrLspContext *context,
                               SZrString *query,
                               SZrArray *result) {
    TZrSize bucketIndex;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    ZrLanguageServer_Lsp_ProjectAppendWorkspaceSymbols(state, context, query, result);

    if (!context->uriToAnalyzerMap.isValid || context->uriToAnalyzerMap.buckets == ZR_NULL) {
        return ZR_TRUE;
    }

    for (bucketIndex = 0; bucketIndex < context->uriToAnalyzerMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->key.type != ZR_VALUE_TYPE_NULL &&
                ZrLanguageServer_Lsp_ProjectContainsUri(state,
                                                        context,
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
                        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                            SZrSymbol *symbol = *symbolPtr;
                            if (ZrLanguageServer_Lsp_StringContainsCaseInsensitive(symbol->name, query)) {
                                SZrLspSymbolInformation *info = ZrLanguageServer_Lsp_CreateSymbolInformation(state, symbol);
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

TZrBool ZrLanguageServer_Lsp_PrepareRename(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrLspRange *outRange,
                         SZrString **outPlaceholder) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrSymbol *symbol;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outRange == ZR_NULL ||
        outPlaceholder == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, fileRange);
    if (symbol == ZR_NULL || symbol->name == ZR_NULL) {
        return ZR_FALSE;
    }

    *outRange = ZrLanguageServer_LspRange_FromFileRange(ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
    *outPlaceholder = symbol->name;
    return ZR_TRUE;
}

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

    fileEntryRange = ZrLanguageServer_LspRange_FromFileRange(
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
                fileEntryRange)) {
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
                        fileEntryRange)) {
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
                if (!ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(state,
                                                                               projectIndex,
                                                                               resolved.moduleName,
                                                                               &navigationUri) ||
                    navigationUri == ZR_NULL) {
                    continue;
                }
                navigationRange = ZrLanguageServer_LspRange_FromFileRange(
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
