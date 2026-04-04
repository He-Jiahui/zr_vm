//
// Created by Auto on 2025/01/XX.
//

#include "lsp_interface_internal.h"

#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"

static TZrBool lsp_string_ends_with_native(SZrString *value, const TZrChar *suffix);

static TZrBool lsp_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source) &&
        range.source != ZR_NULL &&
        position.source != ZR_NULL) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset &&
               position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line &&
             range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line &&
             position.end.column <= range.end.column));
}

static SZrFileRange lsp_super_call_context_range(SZrAstNode *metaFunctionNode) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    if (metaFunctionNode != ZR_NULL) {
        range = metaFunctionNode->location;
    }
    if (metaFunctionNode == ZR_NULL || metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return range;
    }

    if (metaFunctionNode->data.classMetaFunction.superArgs != ZR_NULL &&
        metaFunctionNode->data.classMetaFunction.superArgs->count > 0 &&
        metaFunctionNode->data.classMetaFunction.superArgs->nodes != ZR_NULL &&
        metaFunctionNode->data.classMetaFunction.superArgs->nodes[0] != ZR_NULL) {
        SZrAstNode *lastArgNode =
            metaFunctionNode->data.classMetaFunction.superArgs
                ->nodes[metaFunctionNode->data.classMetaFunction.superArgs->count - 1];
        range.start = metaFunctionNode->data.classMetaFunction.superArgs->nodes[0]->location.start;
        if (range.start.offset >= 6) {
            range.start.offset -= 6;
        }
        if (range.start.column >= 6) {
            range.start.column -= 6;
        }
        range.end = lastArgNode != ZR_NULL ? lastArgNode->location.end : range.end;
    } else if (metaFunctionNode->data.classMetaFunction.body != ZR_NULL) {
        range.end = metaFunctionNode->data.classMetaFunction.body->location.start;
    }

    return range;
}

static TZrBool lsp_super_call_matches_position(SZrAstNode *metaFunctionNode, SZrFileRange position) {
    SZrClassMetaFunction *metaFunction;
    SZrFileRange superRange;

    if (metaFunctionNode == ZR_NULL || metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return ZR_FALSE;
    }

    metaFunction = &metaFunctionNode->data.classMetaFunction;
    if (!metaFunction->hasSuperCall) {
        return ZR_FALSE;
    }

    superRange = lsp_super_call_context_range(metaFunctionNode);
    if (lsp_file_range_contains_position(superRange, position)) {
        return ZR_TRUE;
    }

    if (metaFunction->superArgs != ZR_NULL) {
        for (TZrSize index = 0; index < metaFunction->superArgs->count; index++) {
            SZrAstNode *argNode = metaFunction->superArgs->nodes[index];
            if (argNode != ZR_NULL && lsp_file_range_contains_position(argNode->location, position)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool lsp_find_super_constructor_context(SZrAstNode *node,
                                                  SZrFileRange position,
                                                  SZrAstNode **ownerTypeNode,
                                                  SZrAstNode **metaFunctionNode) {
    if (ownerTypeNode != ZR_NULL) {
        *ownerTypeNode = ZR_NULL;
    }
    if (metaFunctionNode != ZR_NULL) {
        *metaFunctionNode = ZR_NULL;
    }
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (lsp_find_super_constructor_context(node->data.script.statements->nodes[index],
                                                           position,
                                                           ownerTypeNode,
                                                           metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (lsp_find_super_constructor_context(node->data.block.body->nodes[index],
                                                           position,
                                                           ownerTypeNode,
                                                           metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.members != ZR_NULL &&
                node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    SZrAstNode *memberNode = node->data.classDeclaration.members->nodes[index];
                    if (memberNode == ZR_NULL) {
                        continue;
                    }

                    if (memberNode->type == ZR_AST_CLASS_META_FUNCTION &&
                        lsp_super_call_matches_position(memberNode, position)) {
                        if (ownerTypeNode != ZR_NULL) {
                            *ownerTypeNode = node;
                        }
                        if (metaFunctionNode != ZR_NULL) {
                            *metaFunctionNode = memberNode;
                        }
                        return ZR_TRUE;
                    }

                    if (lsp_find_super_constructor_context(memberNode, position, ownerTypeNode, metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        default:
            break;
    }

    return ZR_FALSE;
}

static SZrString *lsp_get_direct_base_declaration_name(SZrAstNode *ownerTypeNode) {
    SZrAstNode *inheritNode;

    if (ownerTypeNode == ZR_NULL ||
        ownerTypeNode->type != ZR_AST_CLASS_DECLARATION ||
        ownerTypeNode->data.classDeclaration.inherits == ZR_NULL ||
        ownerTypeNode->data.classDeclaration.inherits->count == 0) {
        return ZR_NULL;
    }

    inheritNode = ownerTypeNode->data.classDeclaration.inherits->nodes[0];
    if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE || inheritNode->data.type.name == ZR_NULL) {
        return ZR_NULL;
    }

    if (inheritNode->data.type.name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return inheritNode->data.type.name->data.identifier.name;
    }

    if (inheritNode->data.type.name->type == ZR_AST_GENERIC_TYPE &&
        inheritNode->data.type.name->data.genericType.name != ZR_NULL) {
        return inheritNode->data.type.name->data.genericType.name->name;
    }

    return ZR_NULL;
}

static SZrAstNode *lsp_find_type_declaration_recursive(SZrAstNode *node, SZrString *typeName) {
    if (node == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    SZrAstNode *resolved =
                        lsp_find_type_declaration_recursive(node->data.script.statements->nodes[index], typeName);
                    if (resolved != ZR_NULL) {
                        return resolved;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    SZrAstNode *resolved =
                        lsp_find_type_declaration_recursive(node->data.block.body->nodes[index], typeName);
                    if (resolved != ZR_NULL) {
                        return resolved;
                    }
                }
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.name != ZR_NULL &&
                node->data.classDeclaration.name->name != ZR_NULL &&
                ZrCore_String_Equal(node->data.classDeclaration.name->name, typeName)) {
                return node;
            }
            break;

        case ZR_AST_STRUCT_DECLARATION:
            if (node->data.structDeclaration.name != ZR_NULL &&
                node->data.structDeclaration.name->name != ZR_NULL &&
                ZrCore_String_Equal(node->data.structDeclaration.name->name, typeName)) {
                return node;
            }
            break;

        default:
            break;
    }

    return ZR_NULL;
}

static SZrAstNode *lsp_find_constructor_declaration_in_type(SZrAstNode *typeDeclarationNode) {
    SZrAstNodeArray *members = ZR_NULL;

    if (typeDeclarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeDeclarationNode->type == ZR_AST_CLASS_DECLARATION) {
        members = typeDeclarationNode->data.classDeclaration.members;
    } else if (typeDeclarationNode->type == ZR_AST_STRUCT_DECLARATION) {
        members = typeDeclarationNode->data.structDeclaration.members;
    }

    if (members == ZR_NULL || members->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *memberNode = members->nodes[index];
        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_CLASS_META_FUNCTION &&
            memberNode->data.classMetaFunction.meta != ZR_NULL &&
            memberNode->data.classMetaFunction.meta->name != ZR_NULL &&
            lsp_string_ends_with_native(memberNode->data.classMetaFunction.meta->name, "constructor")) {
            return memberNode;
        }

        if (memberNode->type == ZR_AST_STRUCT_META_FUNCTION &&
            memberNode->data.structMetaFunction.meta != ZR_NULL &&
            memberNode->data.structMetaFunction.meta->name != ZR_NULL &&
            lsp_string_ends_with_native(memberNode->data.structMetaFunction.meta->name, "constructor")) {
            return memberNode;
        }
    }

    return ZR_NULL;
}

static TZrBool lsp_try_append_super_constructor_definition(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           SZrLspPosition position,
                                                           SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrAstNode *ownerTypeNode = ZR_NULL;
    SZrAstNode *metaFunctionNode = ZR_NULL;
    SZrString *baseTypeName;
    SZrAstNode *baseTypeDeclaration;
    SZrAstNode *baseConstructorDeclaration;
    SZrLspLocation *location;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    if (!lsp_find_super_constructor_context(analyzer->ast, fileRange, &ownerTypeNode, &metaFunctionNode) ||
        ownerTypeNode == ZR_NULL || metaFunctionNode == ZR_NULL) {
        return ZR_FALSE;
    }

    baseTypeName = lsp_get_direct_base_declaration_name(ownerTypeNode);
    if (baseTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    baseTypeDeclaration = lsp_find_type_declaration_recursive(analyzer->ast, baseTypeName);
    baseConstructorDeclaration = lsp_find_constructor_declaration_in_type(baseTypeDeclaration);
    if (baseConstructorDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 1);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = baseConstructorDeclaration->location.source != ZR_NULL
                        ? baseConstructorDeclaration->location.source
                        : uri;
    location->range = ZrLanguageServer_LspRange_FromFileRange(baseConstructorDeclaration->location);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

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
        return ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                      fileVersion->content,
                                                                      fileVersion->contentLength);
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

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
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

        if (fileVersion == ZR_NULL) {
            return ZR_FALSE;
        }

        ast = fileVersion->ast;
        if (ast == ZR_NULL) {
            ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, uri);
            return fileVersion->parserDiagnostics.length > 0;
        }

        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
        if (analyzer == ZR_NULL) {
            return ZR_FALSE;
        }

        analyzeSuccess = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
        if (!analyzeSuccess) {
            return ZR_FALSE;
        }

        return !allowProjectRefresh ||
               ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(state, context, uri, content, contentLength);
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
    SZrArray diagnostics;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < fileVersion->parserDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&fileVersion->parserDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Lsp_AppendDiagnostic(state, result, *diagPtr);
        }
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
    return ZR_TRUE;
}

// 获取补全
TZrBool ZrLanguageServer_Lsp_GetCompletion(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrArray completions;
    SZrFileVersion *fileVersion;
    TZrBool hasReceiverCompletions = ZR_FALSE;
    SZrFileRange hoverRange;
    SZrHoverInfo *hoverInfo = ZR_NULL;
    SZrSymbol *hoveredSymbol = ZR_NULL;
    SZrString *resolvedTypeText = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    
    ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);

    // 获取分析器
    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    hoverRange = fileRange;
    hoveredSymbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, hoverRange);
    if (ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, hoverRange, &hoverInfo) &&
        hoverInfo != ZR_NULL &&
        hoverInfo->contents != ZR_NULL) {
        resolvedTypeText = ZrLanguageServer_Lsp_ParseResolvedTypeFromHoverMarkdown(state, hoverInfo->contents);
    }

    // 获取补全
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL && analyzer->ast != ZR_NULL) {
        hasReceiverCompletions = ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(state,
                                                                                        fileVersion->content,
                                                                                        fileVersion->contentLength,
                                                                                        filePos.offset,
                                                                                        &completions);
        if (!hasReceiverCompletions) {
            hasReceiverCompletions = ZrLanguageServer_Lsp_ProjectTryCollectImportCompletions(state,
                                                                                             context,
                                                                                             uri,
                                                                                             fileVersion->content,
                                                                                             fileVersion->contentLength,
                                                                                             filePos.offset,
                                                                                             fileRange,
                                                                                             &completions);
        }
        if (!hasReceiverCompletions) {
            hasReceiverCompletions = ZrLanguageServer_Lsp_TryCollectReceiverCompletions(state,
                                                                                        analyzer,
                                                                                        analyzer->ast,
                                                                                        fileVersion->content,
                                                                                        fileVersion->contentLength,
                                                                                        filePos.offset,
                                                                                        &completions);
        }
    }
    if (!hasReceiverCompletions &&
        !ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, fileRange, &completions)) {
        ZrCore_Array_Free(state, &completions);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 补全项
    for (TZrSize i = 0; i < completions.length; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(&completions, i);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            SZrCompletionItem *item = *itemPtr;
            if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
                ZrLanguageServer_Lsp_EnrichCompletionItemMetadata(state,
                                                analyzer,
                                                item,
                                                hoveredSymbol != ZR_NULL ? hoveredSymbol->name : ZR_NULL,
                                                resolvedTypeText,
                                                fileVersion->content,
                                                fileVersion->contentLength);
            }
            
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
    if (hoverInfo != ZR_NULL) {
        ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
    }
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
    if (ZrLanguageServer_Lsp_ProjectTryGetHover(state, context, uri, position, result)) {
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

    content = fileVersion != ZR_NULL && fileVersion->content != ZR_NULL
                  ? ZrLanguageServer_Lsp_TryBuildReceiverNativeHoverMarkdown(state,
                                                                             analyzer,
                                                                             analyzer->ast,
                                                                             fileVersion->content,
                                                                             fileVersion->contentLength,
                                                                             filePos.offset)
                  : ZR_NULL;

    if (content != ZR_NULL) {
        /* Receiver-specialized native hover already produced the final markdown. */
    } else if (ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, fileRange, &hoverInfo) &&
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
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (ZrLanguageServer_Lsp_ProjectTryGetDefinition(state, context, uri, position, result)) {
        return ZR_TRUE;
    }

    if (lsp_try_append_super_constructor_definition(state, context, uri, position, result)) {
        return ZR_TRUE;
    }

    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 1);
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
    
    // 创建位置
    SZrLspLocation *location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }
    
    location->uri = symbol->location.source;
    location->range = ZrLanguageServer_LspRange_FromFileRange(ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
    
    ZrCore_Array_Push(state, result, &location);
    
    return ZR_TRUE;
}

// 查找引用
TZrBool ZrLanguageServer_Lsp_FindReferences(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          SZrLspPosition position,
                          TZrBool includeDeclaration,
                          SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_Lsp_ProjectTryFindReferences(state, context, uri, position, includeDeclaration, result)) {
        return ZR_TRUE;
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
    
    // 获取所有引用
    SZrArray references;
    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 位置
    for (TZrSize i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            
            // 如果是定义引用且不包含定义，跳过
            if (ref->type == ZR_REFERENCE_DEFINITION && !includeDeclaration) {
                continue;
            }
            
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
    SZrSymbolScope *globalScope;
    TZrSize i;

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

    globalScope = analyzer->symbolTable->globalScope;
    if (globalScope == ZR_NULL) {
        return ZR_TRUE;
    }

    for (i = 0; i < globalScope->symbols.length; i++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&globalScope->symbols, i);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            SZrSymbol *symbol = *symbolPtr;
            if (symbol->location.source != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(symbol->location.source, uri)) {
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
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrSymbol *symbol;
    SZrLspDocumentHighlight *highlight;
    SZrArray references;
    TZrSize i;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight != ZR_NULL) {
        highlight->range = ZrLanguageServer_LspRange_FromFileRange(ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
        highlight->kind = 3;
        ZrCore_Array_Push(state, result, &highlight);
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_TRUE;
    }

    for (i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            if (ref->location.source != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(ref->location.source, uri)) {
                SZrLspDocumentHighlight *item =
                    (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
                if (item != ZR_NULL) {
                    item->range = ZrLanguageServer_LspRange_FromFileRange(ref->location);
                    item->kind = ref->type == ZR_REFERENCE_WRITE ? 3 : 2;
                    ZrCore_Array_Push(state, result, &item);
                }
            }
        }
    }

    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
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
