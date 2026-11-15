//
// Focused LSP hover regression for computed member semantic facts.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic/query/lsp_local_semantic_query.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_query.h"

extern SZrSemanticAnalyzer *ZrLanguageServer_Lsp_FindAnalyzer(
        SZrState *state, SZrLspContext *context, SZrString *uri);

/* 独立测试进程通过全局状态回调供给 VM 原生内存；此约定覆盖本文件的 LSP 上下文和查询对象。 */
static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

    /* TODO: 这里用地址和旧大小猜测是否可交给 CRT。若回调可能接收合法的 >=1 GiB 块，释放路径会跳过 free，扩容回退 malloc 又不保留旧内容；需核对 GlobalState 的 allocator 合约。 */
    if (newSize == 0) {
        if (pointer != ZR_NULL &&
            (TZrPtr)pointer >= (TZrPtr)0x1000 &&
            originalSize > 0 &&
            originalSize < 1024 * 1024 * 1024) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    if ((TZrPtr)pointer >= (TZrPtr)0x1000 &&
        originalSize > 0 &&
        originalSize < 1024 * 1024 * 1024) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

/* 两类悬停断言共用字符串视图；返回值借用 VM 字符串，必须在状态仍有效时使用。 */
static const TZrChar *string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

/* 把固定 ASCII 测试源码中的第 N 个片段映射为 LSP 光标，供括号处的三种查询复用。 */
static TZrBool find_position_for_substring(const TZrChar *content,
                                           const TZrChar *needle,
                                           TZrSize occurrence,
                                           SZrLspPosition *outPosition) {
    const TZrChar *match;
    TZrSize currentOccurrence = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    const TZrChar *cursor = content;

    if (content == ZR_NULL || needle == ZR_NULL || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    match = strstr(content, needle);
    while (match != ZR_NULL && currentOccurrence < occurrence) {
        match = strstr(match + 1, needle);
        currentOccurrence++;
    }
    if (match == ZR_NULL) {
        return ZR_FALSE;
    }

    while (cursor < match) {
        if (*cursor == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
        cursor++;
    }

    outPosition->line = line;
    outPosition->character = character;
    return ZR_TRUE;
}

/* 只检查普通悬停任一内容片段是否携带指定语义事实，不依赖展示片段顺序。 */
static TZrBool hover_contains_text(SZrLspHover *hover, const TZrChar *needle) {
    if (hover == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < hover->contents.length; index++) {
        SZrString **contentPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, index);
        if (contentPtr != ZR_NULL &&
            *contentPtr != ZR_NULL &&
            strstr(string_text(*contentPtr), needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 失败报告借用第一条普通悬停文本；调用方须在释放 hover 前完成输出。 */
static const TZrChar *hover_first_text(SZrLspHover *hover) {
    if (hover == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < hover->contents.length; index++) {
        SZrString **contentPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, index);
        if (contentPtr != ZR_NULL && *contentPtr != ZR_NULL) {
            return string_text(*contentPtr);
        }
    }

    return ZR_NULL;
}

/* 丰富悬停以角色区分引用、符号与成员事实，避免仅靠展示顺序判断回归。 */
static TZrBool rich_hover_section_contains_text(SZrLspRichHover *hover,
                                                const TZrChar *role,
                                                const TZrChar *needle) {
    if (hover == ZR_NULL || role == ZR_NULL || needle == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < hover->sections.length; index++) {
        SZrLspRichHoverSection **sectionPtr =
            (SZrLspRichHoverSection **)ZrCore_Array_Get(&hover->sections, index);
        SZrLspRichHoverSection *section =
            sectionPtr != ZR_NULL ? *sectionPtr : ZR_NULL;
        if (section == ZR_NULL ||
            section->role == ZR_NULL ||
            section->value == ZR_NULL ||
            strcmp(string_text(section->role), role) != 0) {
            continue;
        }
        if (strstr(string_text(section->value), needle) != ZR_NULL) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 普通悬停容器由 GetHover 交给调用方；字符串本身仍由 VM 管理。 */
static void hover_free(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

/* 用未解析的 seed[index] 验证括号处仍发布成员访问事实，普通与丰富悬停均消费同一局部查询。 */
static TZrBool test_lsp_hover_surfaces_computed_member_access_at_bracket(SZrState *state) {
    const TZrChar *uriText = "file:///computed_member_hover.zr";
    const TZrChar *content =
        "var seed = { value: 1 };\n"
        "var index = 0;\n"
        "fn inspect() {\n"
        "    seed[index];\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition bracketPosition;
    SZrLspLocalSemanticQueryResult query;
    SZrLspHover *hover = ZR_NULL;
    SZrLspRichHover *richHover = ZR_NULL;
    const TZrChar *hoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "[", 0, &bracketPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare computed member hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    /* 查询事实是悬停预期的来源；后续短路断言只有在事实有效时才继续检查投影。 */
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                 context,
                                                                 uri,
                                                                 bracketPosition,
                                                                 &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.referenceFact != ZR_NULL &&
             query.referenceFact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
             !query.referenceFact->isResolved &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->hasMemberInfo &&
             ZrLanguageServer_Lsp_GetHover(state, context, uri, bracketPosition, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Reference: member access") &&
             hover_contains_text(hover, "Symbol: index") &&
             hover_contains_text(hover, "Member: index") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, bracketPosition, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "reference", "member access") &&
             rich_hover_section_contains_text(richHover, "symbol", "index") &&
             rich_hover_section_contains_text(richHover, "member", "index");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected bracket hover to surface computed member access facts; status=%d reference=%p kind=%d resolved=%d expression=%p hasMember=%d hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.referenceFact,
               query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
               query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasMemberInfo : -1,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspLocalSemanticQuery_Clear(&query);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

typedef struct SZrNativeHoverCase {
    SZrLspPosition position;
    TZrInt32 start;
    TZrInt32 end;
    const TZrChar *type;
    EZrSemanticReferenceKind referenceKind;
} SZrNativeHoverCase;

/* Compare a complete type line, allowing the native container's qualified name. */
static TZrBool native_type_equals(const TZrChar *text, const TZrChar *expected) {
    const TZrChar prefix[] = "zr.container.";
    TZrSize length = strlen(expected);
    if (text == ZR_NULL) return ZR_FALSE;
    if (strncmp(text, prefix, sizeof(prefix) - 1u) == 0) text += sizeof(prefix) - 1u;
    return strcspn(text, "\n") == length && strncmp(text, expected, length) == 0;
}

static TZrBool check_native_hover(SZrState *state, SZrLspContext *context,
                                 SZrString *uri, const SZrNativeHoverCase *test) {
    SZrLspHover *hover = ZR_NULL;
    SZrLspLocalSemanticQueryResult query;
    SZrParserSemanticTypeQuery canonical;
    TZrChar canonicalText[128] = {0};
    const TZrChar *text;
    const TZrChar *typeLine;
    TZrBool passed;

    (void)ZrLanguageServer_Lsp_GetHover(state, context, uri, test->position, &hover);
    text = hover_first_text(hover);
    typeLine = text != ZR_NULL ? strstr(text, "Type: ") : ZR_NULL;
    passed = hover != ZR_NULL && typeLine != ZR_NULL && native_type_equals(typeLine + 6, test->type) &&
             hover->range.start.line == test->position.line && hover->range.start.character == test->start &&
             hover->range.end.line == test->position.line && hover->range.end.character == test->end &&
             test->start <= test->position.character && test->position.character < test->end;

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (test->referenceKind != ZR_SEMANTIC_REFERENCE_UNKNOWN) {
        SZrSemanticAnalyzer *analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
        passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(
                         state, context, uri, test->position, &query) &&
                 query.referenceFact != ZR_NULL && query.referenceFact->kind == test->referenceKind &&
                 query.expressionFact != ZR_NULL && analyzer != ZR_NULL &&
                 query.expressionFact->memberIsComputed == (test->referenceKind != ZR_SEMANTIC_REFERENCE_READ) &&
                 ZrParser_SemanticQuery_CanonicalTypeAt(
                         analyzer->semanticContext, query.queryRange, ZR_NULL, &canonical) &&
                 canonical.typeId == query.expressionFact->typeId &&
                 ZrParser_CanonicalType_Format(analyzer->semanticContext, canonical.typeId,
                                              canonicalText, sizeof(canonicalText)) &&
                 native_type_equals(canonicalText, test->type) && passed;
    }
    if (!passed) {
        printf("FAIL: hover %d:%d expected %s and range %d:%d-%d; ref=%d canonical=%s "
               "range=%d:%d-%d:%d hover=%s\n",
               test->position.line, test->position.character, test->type,
               test->position.line, test->start, test->end,
               query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1, canonicalText,
               hover != ZR_NULL ? hover->range.start.line : -1,
               hover != ZR_NULL ? hover->range.start.character : -1,
               hover != ZR_NULL ? hover->range.end.line : -1,
               hover != ZR_NULL ? hover->range.end.character : -1,
               text != ZR_NULL ? text : "<null>");
    }
    hover_free(state, hover);
    ZrLanguageServer_LspLocalSemanticQuery_Clear(&query);
    return passed;
}

/* Check length inside a loop and both brackets, key, and receiver of typed indexing. */
static TZrBool test_native_computed_hover_token_boundaries(SZrState *state) {
    const TZrChar *content =
        "let container = import(\"zr.container\");\r\n"
        "fn inspect(index: int, label: string): string {\r\n"
        "    let values = new container.Array<string>();\r\n"
        "    let numbers = new container.Array<int>();\r\n"
        "    let mapping = new container.Map<string, bool>();\r\n"
        "    /* \xF0\x9F\x98\x80\xE4\xB8\xAD\xE6\x96\x87 */ while (index < values.length) { break; }\r\n"
        "    /* \xF0\x9F\x98\x80 */ values[index] = \"x\";\r\n"
        "    let number = numbers[index];\r\n"
        "    mapping[label] = true;\r\n"
        "    let present = mapping[label];\r\n"
        "    return values[index];\r\n"
        "}\r\n";
    /* Columns are independent UTF-16 expectations, not derived with the server codec. */
    const SZrNativeHoverCase cases[] = {
        {{5, 38}, 37, 43, "int", ZR_SEMANTIC_REFERENCE_UNKNOWN},
        {{6, 19}, 13, 26, "string", ZR_SEMANTIC_REFERENCE_MEMBER_WRITE},
        {{6, 25}, 13, 26, "string", ZR_SEMANTIC_REFERENCE_MEMBER_WRITE},
        {{6, 22}, 20, 25, "int", ZR_SEMANTIC_REFERENCE_READ},
        {{6, 15}, 13, 19, "Array<string>", ZR_SEMANTIC_REFERENCE_READ},
        {{7, 24}, 17, 31, "int", ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS},
        {{7, 30}, 17, 31, "int", ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS},
        {{8, 11}, 4, 18, "bool", ZR_SEMANTIC_REFERENCE_MEMBER_WRITE},
        {{8, 17}, 4, 18, "bool", ZR_SEMANTIC_REFERENCE_MEMBER_WRITE},
        {{8, 14}, 12, 17, "string", ZR_SEMANTIC_REFERENCE_READ},
        {{9, 25}, 18, 32, "bool", ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS},
        {{9, 31}, 18, 32, "bool", ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS},
        {{10, 17}, 11, 24, "string", ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS},
        {{10, 23}, 11, 24, "string", ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS},
        {{10, 20}, 18, 23, "int", ZR_SEMANTIC_REFERENCE_READ},
        {{10, 13}, 11, 17, "Array<string>", ZR_SEMANTIC_REFERENCE_READ},
    };
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = ZrCore_String_CreateFromNative(state, "file:///native_hover_boundaries.zr");
    TZrBool passed = context != ZR_NULL && uri != ZR_NULL &&
            ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1);

    if (passed) {
        for (TZrSize i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            passed = check_native_hover(state, context, uri, &cases[i]) && passed;
        }
    }
    if (context != ZR_NULL) ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

/* A genuinely absent key stays an unresolved read, not a string-valued member token. */
static TZrBool test_missing_computed_key_is_not_the_element(SZrState *state) {
    const TZrChar *content =
        "let container = import(\"zr.container\");\n"
        "fn inspect(): void {\n"
        "    let values = new container.Array<string>();\n"
        "    values[missing];\n"
        "}\n";
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = ZrCore_String_CreateFromNative(state, "file:///missing_computed_hover_key.zr");
    SZrLspPosition position = {3, 13};
    SZrLspLocalSemanticQueryResult query;
    SZrLspHover *hover = ZR_NULL;
    SZrArray diagnostics = {0};
    TZrBool hasUnresolvedKey = ZR_FALSE;
    TZrBool passed;

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = context != ZR_NULL && uri != ZR_NULL &&
            ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) &&
            ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
            query.referenceFact != ZR_NULL && !query.referenceFact->isResolved &&
            query.referenceFact->kind == ZR_SEMANTIC_REFERENCE_READ;
    if (passed) {
        (void)ZrLanguageServer_Lsp_GetHover(state, context, uri, position, &hover);
        passed = !hover_contains_text(hover, "Type: string") &&
                 !hover_contains_text(hover, "Member: missing") &&
                 ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics);
        for (TZrSize i = 0; i < diagnostics.length; i++) {
            SZrLspDiagnostic **slot = (SZrLspDiagnostic **)ZrCore_Array_Get(&diagnostics, i);
            const SZrLspDiagnostic *diagnostic = slot != ZR_NULL ? *slot : ZR_NULL;
            if (diagnostic != ZR_NULL && diagnostic->severity == 1 && diagnostic->code != ZR_NULL &&
                strcmp(string_text(diagnostic->code), "unresolved_reference") == 0 &&
                diagnostic->range.start.line == 3 && diagnostic->range.start.character == 11 &&
                diagnostic->range.end.line == 3 && diagnostic->range.end.character == 18) {
                hasUnresolvedKey = ZR_TRUE;
            }
        }
    }
    if (!passed || !hasUnresolvedKey) printf("FAIL: missing key must stay an unresolved read\n");
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    hover_free(state, hover);
    ZrLanguageServer_LspLocalSemanticQuery_Clear(&query);
    if (context != ZR_NULL) ZrLanguageServer_LspContext_Free(state, context);
    return passed && hasUnresolvedKey;
}

/* CMake 的 computed_member_hover 测试目标入口：单次构建全局状态并把该回归结果交给 CTest。 */
int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool computedMemberHoverPassed;
    TZrBool tokenBoundariesPassed;
    TZrBool missingKeyPassed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Computed Member Hover Tests\n");
    printf("=====================================\n");
    computedMemberHoverPassed = test_lsp_hover_surfaces_computed_member_access_at_bracket(state);
    printf("%s: LSP Hover Surfaces Computed Member Access At Bracket\n",
           computedMemberHoverPassed ? "PASS" : "FAIL");
    tokenBoundariesPassed = test_native_computed_hover_token_boundaries(state);
    printf("%s: Native Member And Computed Hover Token Boundaries\n",
           tokenBoundariesPassed ? "PASS" : "FAIL");
    missingKeyPassed = test_missing_computed_key_is_not_the_element(state);
    printf("%s: Missing Computed Key Remains Unresolved\n",
           missingKeyPassed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return computedMemberHoverPassed && tokenBoundariesPassed && missingKeyPassed ? 0 : 1;
}
