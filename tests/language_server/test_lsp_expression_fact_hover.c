//
// Focused LSP hover regression for expression fact kind/exact constants.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic/lsp_local_semantic_query.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server.h"

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(flag);

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

static const TZrChar *string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

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

static TZrBool string_constant_equals(SZrString *value, const TZrChar *expected, TZrSize expectedLength) {
    const TZrChar *text = string_text(value);

    return text != ZR_NULL &&
           ZrCore_String_GetByteLength(value) == expectedLength &&
           memcmp(text, expected, expectedLength) == 0;
}

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

static void hover_free(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

static TZrBool lsp_range_equals(SZrLspRange range,
                                TZrInt32 startLine,
                                TZrInt32 startCharacter,
                                TZrInt32 endLine,
                                TZrInt32 endCharacter) {
    return range.start.line == startLine &&
           range.start.character == startCharacter &&
           range.end.line == endLine &&
           range.end.character == endCharacter;
}

static TZrBool test_lsp_hover_surfaces_expression_fact_kind_and_constant(SZrState *state) {
    const TZrChar *uriText = "file:///expression_fact_hover.zr";
    const TZrChar *content =
        "func calc(): int {\n"
        "    return 1 + 2;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition plusPosition;
    SZrLspLocalSemanticQueryResult query;
    SZrLspHover *localHover = ZR_NULL;
    SZrLspHover *publicHover = ZR_NULL;
    SZrLspRichHover *richHover = ZR_NULL;
    const TZrChar *localHoverText;
    const TZrChar *publicHoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", 0, &plusPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare expression fact hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                 context,
                                                                 uri,
                                                                 plusPosition,
                                                                 &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_BINARY &&
             query.expressionFact->exactness == ZR_SEMANTIC_FACT_EXACT &&
             query.expressionFact->hasConstant &&
             query.expressionFact->valueKind == ZR_SEMANTIC_VALUE_KIND_INT64 &&
             query.expressionFact->constantValue.int64Value == 3 &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &localHover) &&
             localHover != ZR_NULL &&
             hover_contains_text(localHover, "Expression: binary exact") &&
             hover_contains_text(localHover, "Constant: 3") &&
             ZrLanguageServer_Lsp_GetHover(state, context, uri, plusPosition, &publicHover) &&
             publicHover != ZR_NULL &&
             hover_contains_text(publicHover, "Expression: binary exact") &&
             hover_contains_text(publicHover, "Constant: 3") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, plusPosition, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "expression", "binary exact") &&
             rich_hover_section_contains_text(richHover, "constant", "3");

    if (!passed) {
        localHoverText = hover_first_text(localHover);
        publicHoverText = hover_first_text(publicHover);
        printf("FAIL: expected LSP hover to surface expression fact kind/exactness and constant; "
               "status=%d expr=%p kind=%d exact=%d hasConst=%d valueKind=%d const=%lld "
               "localHover=%p localText=%s publicHover=%p publicText=%s richHover=%p\n",
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->exactness : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasConstant : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->valueKind : -1,
               query.expressionFact != ZR_NULL ? (long long)query.expressionFact->constantValue.int64Value : -1,
               (void *)localHover,
               localHoverText != ZR_NULL ? localHoverText : "<null>",
               (void *)publicHover,
               publicHoverText != ZR_NULL ? publicHoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, localHover);
    hover_free(state, publicHover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspLocalSemanticQuery_Clear(&query);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_lsp_hover_range_after_utf8_prefix_uses_utf16_columns(SZrState *state) {
    const TZrChar *uriText = "file:///expression_fact_utf16_hover.zr";
    const TZrChar *content =
        "func calc(): int {\n"
        "    /* \xCE\xBB */ return 1 + 2;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition plusPosition;
    SZrLspHover *publicHover = ZR_NULL;
    const TZrChar *hoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", 0, &plusPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare UTF-16 expression fact hover fixture\n");
        return ZR_FALSE;
    }

    passed = ZrLanguageServer_Lsp_GetHover(state, context, uri, plusPosition, &publicHover) &&
             publicHover != ZR_NULL &&
             hover_contains_text(publicHover, "Expression: binary exact") &&
             hover_contains_text(publicHover, "Constant: 3") &&
             lsp_range_equals(publicHover->range, 1, 19, 1, 24);

    if (!passed) {
        hoverText = hover_first_text(publicHover);
        printf("FAIL: expected UTF-16 hover range 1:19-1:24 after UTF-8 prefix; "
               "hover=%p range=%d:%d-%d:%d text=%s\n",
               (void *)publicHover,
               publicHover != ZR_NULL ? publicHover->range.start.line : -1,
               publicHover != ZR_NULL ? publicHover->range.start.character : -1,
               publicHover != ZR_NULL ? publicHover->range.end.line : -1,
               publicHover != ZR_NULL ? publicHover->range.end.character : -1,
               hoverText != ZR_NULL ? hoverText : "<null>");
    }

    hover_free(state, publicHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_lsp_hover_escapes_string_constant_payload(SZrState *state) {
    const TZrChar *uriText = "file:///expression_fact_string_hover.zr";
    const TZrChar *content =
        "func text(): string {\n"
        "    return \"a\\\"b\\\\c\\n\\t\";\n"
        "}\n";
    const TZrChar expectedDecoded[] = "a\"b\\c\n\t";
    const TZrChar *expectedHover = "Constant: \"a\\\"b\\\\c\\n\\t\"";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition stringPosition;
    SZrLspLocalSemanticQueryResult query;
    SZrLspHover *localHover = ZR_NULL;
    SZrLspHover *publicHover = ZR_NULL;
    SZrLspRichHover *richHover = ZR_NULL;
    const TZrChar *localHoverText;
    const TZrChar *publicHoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "a\\\"b", 0, &stringPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare escaped string hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                 context,
                                                                 uri,
                                                                 stringPosition,
                                                                 &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.expressionFact != ZR_NULL &&
             query.expressionFact->kind == ZR_SEMANTIC_EXPRESSION_FACT_LITERAL &&
             query.expressionFact->exactness == ZR_SEMANTIC_FACT_EXACT &&
             query.expressionFact->hasConstant &&
             query.expressionFact->valueKind == ZR_SEMANTIC_VALUE_KIND_STRING &&
             string_constant_equals(query.expressionFact->constantValue.stringValue,
                                    expectedDecoded,
                                    sizeof(expectedDecoded) - 1u) &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &localHover) &&
             localHover != ZR_NULL &&
             hover_contains_text(localHover, expectedHover) &&
             !hover_contains_text(localHover, "Constant: \"a\"b") &&
             ZrLanguageServer_Lsp_GetHover(state, context, uri, stringPosition, &publicHover) &&
             publicHover != ZR_NULL &&
             hover_contains_text(publicHover, expectedHover) &&
             !hover_contains_text(publicHover, "Constant: \"a\"b") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, stringPosition, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "constant", "a\\\"b\\\\c\\n\\t") &&
             !rich_hover_section_contains_text(richHover, "constant", "a\"b");

    if (!passed) {
        localHoverText = hover_first_text(localHover);
        publicHoverText = hover_first_text(publicHover);
        printf("FAIL: expected LSP hover to escape string constant payload; "
               "status=%d expr=%p kind=%d exact=%d hasConst=%d valueKind=%d "
               "localHover=%p localText=%s publicHover=%p publicText=%s richHover=%p\n",
               (int)query.status,
               (void *)query.expressionFact,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->kind : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->exactness : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->hasConstant : -1,
               query.expressionFact != ZR_NULL ? (int)query.expressionFact->valueKind : -1,
               (void *)localHover,
               localHoverText != ZR_NULL ? localHoverText : "<null>",
               (void *)publicHover,
               publicHoverText != ZR_NULL ? publicHoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, localHover);
    hover_free(state, publicHover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspLocalSemanticQuery_Clear(&query);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool expressionHoverPassed;
    TZrBool utf16HoverRangePassed;
    TZrBool stringHoverPassed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Expression Fact Hover Tests\n");
    printf("=====================================\n");
    expressionHoverPassed = test_lsp_hover_surfaces_expression_fact_kind_and_constant(state);
    printf("%s: LSP Hover Surfaces Expression Fact Kind And Constant\n",
           expressionHoverPassed ? "PASS" : "FAIL");
    utf16HoverRangePassed = test_lsp_hover_range_after_utf8_prefix_uses_utf16_columns(state);
    printf("%s: LSP Hover Range After UTF-8 Prefix Uses UTF-16 Columns\n",
           utf16HoverRangePassed ? "PASS" : "FAIL");
    stringHoverPassed = test_lsp_hover_escapes_string_constant_payload(state);
    printf("%s: LSP Hover Escapes String Constant Payload\n",
           stringHoverPassed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return expressionHoverPassed && utf16HoverRangePassed && stringHoverPassed ? 0 : 1;
}
