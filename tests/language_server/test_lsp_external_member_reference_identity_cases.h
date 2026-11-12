/* 导入成员名相同仍不足以合并引用；声明精确范围改变时外部引用投影应失败关闭。 */
static void test_external_member_references_reject_mismatched_declaration_identity(
        SZrState *state) {
    static const TZrChar *content =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n";
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspSemanticQuery query;
    SZrFileRange originalDeclarationRange;
    SZrArray locations = {0};
    SZrParityTimer timer;
    TZrChar failureBuffer[256] = {0};
    const TZrChar *failure = "fixture initialization";
    TZrBool valid = ZR_FALSE;
    TZrBool resolved = ZR_FALSE;
    TZrBool appended = ZR_FALSE;

    TEST_START("LSP External Member References Reject Mismatched Declaration Identity");
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///external_member_identity.zr",
            strlen("file:///external_member_identity.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "list.addLast", 0U, 5, &memberPosition)) {
        goto cleanup;
    }

    failure = "exact external member resolution";
    resolved = ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
            state, context, uri, memberPosition, &query);
    if (resolved) {
        appended = ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_FALSE, &locations);
    }
    if (!resolved ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        !query.resolvedMember.hasDeclaration ||
        query.resolvedMember.declarationUri == ZR_NULL ||
        !appended ||
        locations.length != 2U) {
        snprintf(
                failureBuffer,
                sizeof(failureBuffer),
                "resolved=%d kind=%d declaration=%d uri=%p project=%p appended=%d count=%zu",
                resolved,
                query.kind,
                query.resolvedMember.hasDeclaration,
                (void *)query.resolvedMember.declarationUri,
                (void *)query.projectIndex,
                appended,
                (size_t)locations.length);
        failure = failureBuffer;
        goto cleanup;
    }

    free_local_reference_projection_results(state, &locations, ZR_NULL);
    /* 只扰动查询目标的声明身份，保持原始文档和引用事实不变以隔离匹配条件。 */
    originalDeclarationRange = query.resolvedMember.declarationRange;
    query.resolvedMember.declarationRange.start.line++;
    query.resolvedMember.declarationRange.start.offset++;
    query.resolvedMember.declarationRange.end.line++;
    query.resolvedMember.declarationRange.end.offset++;

    failure = "mismatched exact declaration must fail closed";
    if (ZrLanguageServer_LspSemanticQuery_AppendReferences(
                state, context, &query, ZR_FALSE, &locations) ||
        locations.length != 0U) {
        query.resolvedMember.declarationRange = originalDeclarationRange;
        goto cleanup;
    }
    query.resolvedMember.declarationRange = originalDeclarationRange;
    valid = ZR_TRUE;

cleanup:
    /* 引用结果逐项原生分配，须在释放语义查询及其 context 前清理。 */
    free_local_reference_projection_results(state, &locations, ZR_NULL);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (valid) {
        TEST_PASS(
                timer,
                "LSP External Member References Reject Mismatched Declaration Identity");
    } else {
        TEST_FAIL(
                timer,
                "LSP External Member References Reject Mismatched Declaration Identity",
                failure);
    }
}

/* 同一 URI 更新版本后，旧查询不能继续生成悬停、定义、引用或高亮。 */
static void test_external_member_query_rejects_stale_document_snapshot(
        SZrState *state) {
    static const TZrChar *content =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n";
    static const TZrChar *updatedContent =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n"
            "\n";
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspSemanticQuery query;
    SZrLspHover *hover = ZR_NULL;
    SZrArray definitions = {0};
    SZrArray references = {0};
    SZrArray highlights = {0};
    SZrParityTimer timer;
    TZrChar failureBuffer[256] = {0};
    const TZrChar *failure = "fixture initialization";
    TZrBool valid = ZR_FALSE;
    TZrBool hoverBuilt;
    TZrBool definitionAppended;
    TZrBool referencesAppended;
    TZrBool highlightsAppended;

    TEST_START("LSP External Member Query Rejects Stale Document Snapshot");
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///external_member_snapshot.zr",
            strlen("file:///external_member_snapshot.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "list.addLast", 0U, 5, &memberPosition) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        !query.hasSemanticVersion ||
        !query.resolvedMember.hasDeclaration) {
        failure = "fresh query document version must be captured";
        goto cleanup;
    }

    failure = "document update";
    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updatedContent,
                strlen(updatedContent),
                2U)) {
        goto cleanup;
    }

    /* 查询捕获的是旧语义版本；四类消费入口均必须先校验快照而拒绝陈旧投影。 */
    failure = "stale query consumers must fail closed";
    hoverBuilt = ZrLanguageServer_LspSemanticQuery_BuildHover(
            state, context, &query, &hover);
    definitionAppended = ZrLanguageServer_LspSemanticQuery_AppendDefinitions(
            state, context, &query, &definitions);
    referencesAppended = ZrLanguageServer_LspSemanticQuery_AppendReferences(
            state, context, &query, ZR_FALSE, &references);
    highlightsAppended = ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
            state, context, &query, &highlights);
    if (hoverBuilt || hover != ZR_NULL ||
        definitionAppended || definitions.length != 0U ||
        referencesAppended || references.length != 0U ||
        highlightsAppended || highlights.length != 0U) {
        snprintf(
                failureBuffer,
                sizeof(failureBuffer),
                "hover=%d/%p definitions=%d/%zu references=%d/%zu highlights=%d/%zu",
                hoverBuilt,
                (void *)hover,
                definitionAppended,
                (size_t)definitions.length,
                referencesAppended,
                (size_t)references.length,
                highlightsAppended,
                (size_t)highlights.length);
        failure = failureBuffer;
        goto cleanup;
    }
    valid = ZR_TRUE;

cleanup:
    if (hover != ZR_NULL) {
        if (hover->contents.isValid) {
            ZrCore_Array_Free(state, &hover->contents);
        }
        ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
    }
    free_local_reference_projection_results(state, &definitions, ZR_NULL);
    free_local_reference_projection_results(state, &references, &highlights);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (valid) {
        TEST_PASS(timer, "LSP External Member Query Rejects Stale Document Snapshot");
    } else {
        TEST_FAIL(
                timer,
                "LSP External Member Query Rejects Stale Document Snapshot",
                failure);
    }
}

/* 去除外部声明身份后，引用与文档高亮不能仅凭名称或局部相似性继续发布。 */
static void test_external_member_query_rejects_missing_declaration_identity(
        SZrState *state) {
    static const TZrChar *content =
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n";
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition memberPosition;
    SZrLspSemanticQuery query;
    SZrArray references = {0};
    SZrArray highlights = {0};
    SZrParityTimer timer;
    const TZrChar *failure = "fixture initialization";
    TZrBool valid = ZR_FALSE;

    TEST_START("LSP External Member Query Rejects Missing Declaration Identity");
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            "file:///external_member_missing_identity.zr",
            strlen("file:///external_member_missing_identity.zr"));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state, context, uri, content, strlen(content), 1U) ||
        !find_position(content, "list.addLast", 0U, 5, &memberPosition) ||
        !ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, memberPosition, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER ||
        !query.resolvedMember.hasDeclaration) {
        goto cleanup;
    }

    /* 仅改写已解析查询，验证消费层对欠定身份的防护，不依赖重新解析的行为。 */
    query.resolvedMember.hasDeclaration = ZR_FALSE;
    query.resolvedMember.declarationUri = ZR_NULL;
    ZrCore_Array_Init(state, &references, sizeof(SZrLspLocation *), 4U);
    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 4U);
    failure = "missing declaration identity must fail closed";
    valid = !ZrLanguageServer_LspSemanticQuery_AppendReferences(
                    state, context, &query, ZR_FALSE, &references) &&
            references.length == 0U &&
            !ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
                    state, context, &query, &highlights) &&
            highlights.length == 0U;

cleanup:
    free_local_reference_projection_results(state, &references, &highlights);
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (valid) {
        TEST_PASS(timer, "LSP External Member Query Rejects Missing Declaration Identity");
    } else {
        TEST_FAIL(timer,
                  "LSP External Member Query Rejects Missing Declaration Identity",
                  failure);
    }
}

/* 每个 fixture 共用同一原生接收者；正文只改变成员访问所处的 AST 兄弟位置。 */
#define ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE \
    "var {LinkedList} = import(\"zr.container\");\n" \
    "var list: LinkedList<int> = null;\n"

/* 一种多子树 AST 形态；content 中 `list.count` 的每次出现都指向同一外部成员。 */
typedef struct SZrExternalMemberSiblingCase {
    const TZrChar *label;
    const TZrChar *content;
} SZrExternalMemberSiblingCase;

/* 引用结果须与 needle 在本文档的全部文本出现一一对应：不缺失、不重复、不多出。 */
static TZrBool external_member_sibling_locations_cover_occurrences(
        const SZrArray *locations,
        SZrString *uri,
        const TZrChar *content,
        const TZrChar *needle,
        TZrInt32 tokenOffset,
        TZrSize tokenLength,
        TZrSize *outExpectedCount) {
    SZrLspPosition position;
    TZrSize occurrence;
    TZrBool exact = ZR_TRUE;

    for (occurrence = 0U;
         find_position(content, needle, occurrence, tokenOffset, &position);
         occurrence++) {
        TZrSize matches = 0U;

        for (TZrSize index = 0U; index < locations->length; index++) {
            SZrLspLocation **slot = (SZrLspLocation **)ZrCore_Array_Get(
                    (SZrArray *)locations, index);
            const SZrLspLocation *location = slot != ZR_NULL ? *slot : ZR_NULL;

            if (location != ZR_NULL && location->uri != ZR_NULL &&
                ZrCore_String_Equal(location->uri, uri) &&
                location->range.start.line == position.line &&
                location->range.end.line == position.line &&
                location->range.start.character == position.character &&
                location->range.end.character ==
                        position.character + (TZrInt32)tokenLength) {
                matches++;
            }
        }
        if (matches != 1U) {
            exact = ZR_FALSE;
        }
    }
    *outExpectedCount = occurrence;
    return exact && occurrence > 0U && locations->length == occurrence;
}

/* 前一个兄弟子树已命中时，引用遍历仍须访问其余子树；逐形态核对全部使用点。 */
static void test_external_member_references_visit_every_sibling_subtree(
        SZrState *state) {
    static const SZrExternalMemberSiblingCase cases[] = {
        {"assignment",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "var slots = [0, 0];\n"
         "slots[list.count] = list.count;\n"},
        {"binary",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "var sum = list.count + list.count;\n"},
        {"logical",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "var both = list.count > 0 && list.count < 9;\n"},
        {"conditional",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "var pick = list.count > 0 ? list.count : list.count;\n"},
        {"if",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "if (list.count > 0) {\n"
         "    var taken = list.count;\n"
         "} else {\n"
         "    var skipped = list.count;\n"
         "}\n"},
        {"while",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "while (list.count > 0) {\n"
         "    var seen = list.count;\n"
         "    break;\n"
         "}\n"},
        {"for",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "for (var i = list.count; i < list.count; i = i + list.count) {\n"
         "    var seen = list.count;\n"
         "}\n"},
        {"foreach",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "for (var item in [list.count]) {\n"
         "    var seen = list.count;\n"
         "}\n"},
        {"switch",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "switch (list.count) {\n"
         "    (list.count) {\n"
         "        var matched = list.count;\n"
         "    }\n"
         "    () {\n"
         "        var fallback = list.count;\n"
         "    }\n"
         "}\n"},
        {"using",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "using (list.count) {\n"
         "    var seen = list.count;\n"
         "}\n"},
        {"try",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "try {\n"
         "    var attempted = list.count;\n"
         "} catch (error) {\n"
         "    var caught = list.count;\n"
         "} finally {\n"
         "    var settled = list.count;\n"
         "}\n"},
        {"keyValue",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "var table = {[list.count]: list.count};\n"},
        {"primary",
         ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
         "var picked = [list.count, 0][list.count];\n"},
    };
    static const TZrChar *needle = "list.count";
    static const TZrChar *summary =
            "LSP External Member References Visit Every Sibling Subtree";
    SZrLspContext *context = ZR_NULL;
    SZrParityTimer timer;
    TZrChar failureBuffer[512] = {0};
    TZrSize failureLength = 0U;
    const TZrChar *failure = "fixture initialization";
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    context = ZrLanguageServer_LspContext_New(state);
    if (context == ZR_NULL) {
        goto cleanup;
    }

    valid = ZR_TRUE;
    for (TZrSize caseIndex = 0U;
         caseIndex < sizeof(cases) / sizeof(cases[0]);
         caseIndex++) {
        const SZrExternalMemberSiblingCase *current = &cases[caseIndex];
        TZrChar uriText[128];
        SZrString *uri;
        SZrLspPosition position;
        SZrLspSemanticQuery query;
        SZrArray locations = {0};
        TZrSize expectedCount = 0U;
        TZrBool resolved;
        TZrBool appended = ZR_FALSE;
        TZrBool covered = ZR_FALSE;

        /* 各形态使用独立文档；无工程索引时引用遍历只覆盖被查询的文档。 */
        snprintf(uriText, sizeof(uriText),
                 "file:///external_member_sibling_%s.zr", current->label);
        ZrLanguageServer_LspSemanticQuery_Init(&query);
        uri = ZrCore_String_Create(state, uriText, strlen(uriText));
        resolved = uri != ZR_NULL &&
                   ZrLanguageServer_Lsp_UpdateDocument(
                           state, context, uri, current->content,
                           strlen(current->content), 1U) &&
                   find_position(current->content, needle, 0U, 5, &position) &&
                   ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                           state, context, uri, position, &query) &&
                   query.kind ==
                           ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER &&
                   query.resolvedMember.hasDeclaration;
        if (resolved) {
            appended = ZrLanguageServer_LspSemanticQuery_AppendReferences(
                    state, context, &query, ZR_FALSE, &locations);
            covered = external_member_sibling_locations_cover_occurrences(
                    &locations, uri, current->content, needle, 5,
                    strlen("count"), &expectedCount);
        }
        if (!resolved || !appended || !covered) {
            /* 汇总全部失配形态及其实得/应得引用数，一次运行即可定位遗漏的子树。 */
            valid = ZR_FALSE;
            if (failureLength < sizeof(failureBuffer)) {
                TZrInt32 written = snprintf(
                        failureBuffer + failureLength,
                        sizeof(failureBuffer) - failureLength,
                        "%s%s(resolved=%d,%zu/%zu)",
                        failureLength == 0U ? "incomplete references: " : " ",
                        current->label,
                        resolved,
                        (size_t)locations.length,
                        (size_t)expectedCount);
                if (written > 0) {
                    failureLength += (TZrSize)written;
                }
            }
            failure = failureBuffer;
        }
        /* 引用结果逐项原生分配，须在释放语义查询前清理。 */
        free_local_reference_projection_results(state, &locations, ZR_NULL);
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    }

cleanup:
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }

    if (valid) {
        TEST_PASS(timer, summary);
    } else {
        TEST_FAIL(timer, summary, failure);
    }
}

#undef ZR_EXTERNAL_MEMBER_SIBLING_PRELUDE
