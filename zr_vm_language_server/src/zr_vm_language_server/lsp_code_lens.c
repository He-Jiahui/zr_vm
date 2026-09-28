#include "lsp_editor_features_internal.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/test_contract.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 统一装配服务端完整 CodeLens：stdio 与 WASM 的首次响应携带完整命令供客户端点击，
 * URI、标题和命令字符串由请求上下文/GC 持有，结果数组只持有 lens 原生结构。 */
static TZrBool lsp_code_lens_append(SZrState *state,
                                    SZrArray *result,
                                    SZrLspRange range,
                                    const TZrChar *title,
                                    const TZrChar *command,
                                    SZrString *argument,
                                    const SZrLspPosition *positionArgument) {
    SZrLspCodeLens *lens;

    if (state == ZR_NULL || result == ZR_NULL || title == ZR_NULL ||
        command == ZR_NULL) {
        return ZR_FALSE;
    }

    lens = (SZrLspCodeLens *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspCodeLens));
    if (lens == ZR_NULL) {
        return ZR_FALSE;
    }
    lens->range = range;
    lens->commandTitle = lsp_editor_create_string(state, title, strlen(title));
    lens->command = lsp_editor_create_string(state, command, strlen(command));
    lens->argument = argument;
    lens->hasPositionArgument = positionArgument != ZR_NULL;
    if (positionArgument != ZR_NULL) {
        lens->positionArgument = *positionArgument;
    } else {
        lens->positionArgument.line = 0;
        lens->positionArgument.character = 0;
    }
    ZrCore_Array_Push(state, result, &lens);
    return ZR_TRUE;
}

/* 引用数量只展示在有稳定声明身份且用户可导航的类型/可调用声明上。 */
static TZrBool lsp_code_lens_is_supported_declaration(
        const SZrParserSemanticSymbolQuery *declaration) {
    if (declaration == ZR_NULL || declaration->declarationNode == ZR_NULL ||
        declaration->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    switch (declaration->declarationNode->type) {
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

/* 语义查询可能为同一源码位置产生不同 SZrString 实例；去重按源内容和字节范围。 */
static TZrBool lsp_code_lens_ranges_equal(SZrFileRange left,
                                          SZrFileRange right) {
    TZrBool sameSource = left.source == right.source;

    if (!sameSource && left.source != ZR_NULL && right.source != ZR_NULL) {
        sameSource = ZrCore_String_Equal(left.source, right.source);
    }
    return sameSource && left.start.offset == right.start.offset &&
           left.end.offset == right.end.offset;
}

/* ReferencesOf 可返回多条同位置事实；CodeLens 的数字应对应可见引用位置。 */
static TZrBool lsp_code_lens_reference_was_counted(
        const SZrArray *references,
        TZrSize currentIndex,
        const SZrSemanticReferenceFact *current) {
    if (references == ZR_NULL || current == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < currentIndex; index++) {
        const SZrSemanticReferenceFact *const *factPtr =
                (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
                        (SZrArray *)references, index);
        if (factPtr != ZR_NULL && *factPtr != ZR_NULL &&
            (*factPtr)->isResolved &&
            (*factPtr)->kind != ZR_SEMANTIC_REFERENCE_DECLARATION &&
            lsp_code_lens_ranges_equal((*factPtr)->range, current->range)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 同一 AST 声明可经多个语义查询项出现，避免给一个声明叠加相同透镜。 */
static TZrBool lsp_code_lens_declaration_was_projected(
        const SZrArray *declarations,
        TZrSize currentIndex,
        const SZrParserSemanticSymbolQuery *current) {
    if (declarations == ZR_NULL || current == ZR_NULL ||
        current->declarationNode == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < currentIndex; index++) {
        const SZrParserSemanticSymbolQuery *candidate =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        (SZrArray *)declarations, index);
        if (candidate != ZR_NULL &&
            candidate->declarationNode == current->declarationNode) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 使用 parser 的规范化符号事实，在当前文档 AST 范围内计算已解析、非声明引用；
 * 查询失败时调用者把零视作无透镜，不能把零解释为全工作区没有引用。 */
static TZrSize lsp_code_lens_count_references(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope) {
    SZrArray references;
    TZrSize count = 0U;

    ZrCore_Array_Construct(&references);
    if (ZrParser_SemanticQuery_ReferencesOf(
                semanticContext, symbolId, scope, &references)) {
        for (TZrSize index = 0U; index < references.length; index++) {
            const SZrSemanticReferenceFact *const *factPtr =
                    (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
                            &references, index);
            if (factPtr != ZR_NULL && *factPtr != ZR_NULL &&
                (*factPtr)->isResolved &&
                (*factPtr)->kind != ZR_SEMANTIC_REFERENCE_DECLARATION &&
                !lsp_code_lens_reference_was_counted(
                        &references, index, *factPtr)) {
                count++;
            }
        }
    }
    if (references.isValid) {
        ZrCore_Array_Free(state, &references);
    }
    return count;
}

/* 只为当前文档 AST 对应的声明生成可点击引用计数；点击后扩展重新请求引用，
 * 因而透镜标题不是永久缓存，也不直接承诺全项目引用总数。 */
static TZrBool lsp_code_lens_append_reference_counts(SZrState *state,
                                                     SZrLspContext *context,
                                                     SZrString *uri,
                                                     SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrParserSemanticQueryScope scope;
    SZrArray declarations;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    fileVersion = lsp_editor_get_file_version(context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->ast == ZR_NULL || fileVersion == ZR_NULL ||
        fileVersion->ast == ZR_NULL || analyzer->ast != fileVersion->ast) {
        /* 编辑后语义分析器可能仍指向旧 AST；此时宁可不显示旧引用数字。 */
        return ZR_TRUE;
    }

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE;
    scope.root = analyzer->ast;
    ZrCore_Array_Construct(&declarations);
    if (!ZrParser_SemanticQuery_DeclaredSymbols(
                analyzer->semanticContext, &scope, &declarations)) {
        if (declarations.isValid) {
            ZrCore_Array_Free(state, &declarations);
        }
        return ZR_TRUE;
    }

    for (TZrSize index = 0U; index < declarations.length; index++) {
        const SZrParserSemanticSymbolQuery *declaration =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        &declarations, index);
        SZrFileRange declarationRange;
        SZrLspRange range;
        SZrLspPosition position;
        TZrSize referenceCount;
        TZrChar title[ZR_LSP_SHORT_TEXT_BUFFER_LENGTH];

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            ZrCore_Array_Free(state, &declarations);
            return ZR_FALSE;
        }
        if (!lsp_code_lens_is_supported_declaration(declaration)) {
            continue;
        }
        if (lsp_code_lens_declaration_was_projected(
                    &declarations, index, declaration)) {
            continue;
        }

        declarationRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
                analyzer, declaration->declarationRange);
        if (declarationRange.source == ZR_NULL ||
            (!ZrLanguageServer_Lsp_StringsEqual(declarationRange.source, uri) &&
             !ZrLanguageServer_Lsp_UrisResolveToSameNativePath(
                     declarationRange.source, uri))) {
            continue;
        }

        referenceCount = lsp_code_lens_count_references(
                state,
                analyzer->semanticContext,
                declaration->symbolId,
                &scope);
        if (referenceCount == 0U) {
            continue;
        }

        range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
                context, uri, declarationRange);
        position = range.start;
        snprintf(title,
                 sizeof(title),
                 "%zu reference%s",
                 (size_t)referenceCount,
                 referenceCount == 1U ? "" : "s");
        if (!lsp_code_lens_append(
                    state,
                    result,
                    range,
                    title,
                    "zr.showReferences",
                    uri,
                    &position)) {
            ZrCore_Array_Free(state, &declarations);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(state, &declarations);
    return ZR_TRUE;
}

/* 候选标记通过独立 CompileTest 与 TestManifest 绑定后才展示测试透镜，
 * 不能仅凭词法属性或普通 AST 把无效签名当成可运行测试。 */
static TZrBool lsp_code_lens_append_test_roles(SZrState *state,
                                               const TZrChar *content,
                                               TZrSize contentLength,
                                               SZrString *uri,
                                               SZrArray *result) {
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *sourceName = uri;
    SZrFunction *function;
    SZrParserTestManifest manifest;
    TZrBool success = ZR_TRUE;

    if (state == ZR_NULL || content == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_Lsp_FileUriToNativePath(
                uri, nativePath, sizeof(nativePath))) {
        sourceName = ZrCore_String_CreateFromNative(state, nativePath);
        if (sourceName == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    function = ZrParser_Source_CompileTest(
            state, content, contentLength, sourceName);
    if (function == ZR_NULL || function->testManifestData == ZR_NULL ||
        function->testManifestDataLength == 0U ||
        function->testManifestDataLength > (TZrSize)UINT32_MAX) {
        if (function != ZR_NULL) {
            ZrCore_Function_Free(state, function);
        }
        return ZR_TRUE;
    }
    memset(&manifest, 0, sizeof(manifest));
    if (!ZrParser_TestManifest_Decode(
                state,
                function->testManifestData,
                (TZrUInt32)function->testManifestDataLength,
                &manifest)) {
        ZrCore_Function_Free(state, function);
        return ZR_FALSE;
    }

    for (TZrUInt32 entryIndex = 0U; entryIndex < manifest.entryCount;
         entryIndex++) {
        const SZrParserTestEntry *entry = &manifest.entries[entryIndex];
        TZrSize startOffset = entry->sourceRange.start.offset;
        TZrSize endOffset = entry->sourceRange.end.offset;
        SZrLspRange range;
        SZrLspPosition position;

        if (startOffset > contentLength) {
            continue;
        }
        if (endOffset < startOffset || endOffset > contentLength) {
            endOffset = startOffset;
        }
        range = lsp_editor_range_from_offsets(
                content, contentLength, startOffset, endOffset);
        position = range.start;
        if (!lsp_code_lens_append(
                    state,
                    result,
                    range,
                    "Run Zr test",
                    ZR_LSP_COMMAND_RUN_CURRENT_PROJECT,
                    uri,
                    &position) ||
            !lsp_code_lens_append(
                    state,
                    result,
                    range,
                    "Debug Zr test",
                    "zr.debugCurrentProject",
                    uri,
                    &position)) {
            success = ZR_FALSE;
            break;
        }
        for (TZrUInt32 caseIndex = 0U; caseIndex < entry->caseCount;
             caseIndex++) {
            TZrChar title[ZR_LSP_SHORT_TEXT_BUFFER_LENGTH];
            /* BUG: 每个 Run case 透镜只传同一 URI/位置和 runCurrentProject 命令，
             * 未传 case ordinal；扩展 projectActions.ts 的命令忽略参数并运行所选项目。
             * 点击任何一个 Run case 都不能只运行标题所示用例；需贯通用例身份到执行入口。 */
            snprintf(
                    title, sizeof(title), "Run case %u", entry->cases[caseIndex].ordinal);
            if (!lsp_code_lens_append(
                        state,
                        result,
                        range,
                        title,
                        ZR_LSP_COMMAND_RUN_CURRENT_PROJECT,
                        uri,
                        &position)) {
                success = ZR_FALSE;
                break;
            }
        }
        if (!success) {
            break;
        }
    }
    ZrParser_TestManifest_Free(state, &manifest);
    ZrCore_Function_Free(state, function);
    return success;
}

/* 词法预筛减少每次请求的测试编译成本；真正的角色仍由 manifest 决定。 */
static TZrBool lsp_code_lens_has_test_attribute_candidate(
        const TZrChar *content,
        TZrSize contentLength) {
    static const TZrChar marker[] = "#zr.testing.test#";
    const TZrSize markerLength = sizeof(marker) - 1U;

    if (content == ZR_NULL || contentLength < markerLength) {
        return ZR_FALSE;
    }
    for (TZrSize offset = 0U; offset + markerLength <= contentLength;
         offset++) {
        if (memcmp(content + offset, marker, markerLength) == 0 &&
            lsp_editor_offset_is_code(content, contentLength, offset)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* stdio/WASM 的 textDocument/codeLens 入口：快照锁住本次请求的文本，
 * 引用透镜依赖当前语义 AST，测试透镜仅在非回退 AST 上重新编译并投影。
 * 调用者无论成功与否都须用 FreeCodeLens 归还可能已追加的结果。 */
TZrBool ZrLanguageServer_Lsp_GetCodeLens(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri,
                                         SZrArray *result) {
    SZrFileVersionContentSnapshot snapshot;
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspCodeLens *),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(
                state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }

    if (!lsp_code_lens_append_reference_counts(state, context, uri, result)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    if (!snapshot.usesFallbackAst && fileVersion != ZR_NULL &&
        fileVersion->ast != ZR_NULL &&
        lsp_code_lens_has_test_attribute_candidate(
                snapshot.content, snapshot.contentLength) &&
        !lsp_code_lens_append_test_roles(
                state,
                snapshot.content,
                snapshot.contentLength,
                uri,
                result)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

/* 与 GetCodeLens 配对；数组和原生透镜由这里释放，嵌套 GC 字符串不归此函数单独释放。 */
void ZrLanguageServer_Lsp_FreeCodeLens(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspCodeLens **lensPtr =
                (SZrLspCodeLens **)ZrCore_Array_Get(result, index);
        if (lensPtr != ZR_NULL && *lensPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, *lensPtr, sizeof(SZrLspCodeLens));
        }
    }
    ZrCore_Array_Free(state, result);
}
