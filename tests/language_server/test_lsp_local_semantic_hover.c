//
// Focused LSP local semantic hover regression tests.
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

static const TZrChar *string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
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

static void hover_free(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

static TZrBool test_local_expression_hover_surfaces_reference_fact(SZrState *state) {
    const TZrChar *uriText = "file:///local_expression_reference_hover.zr";
    const TZrChar *content =
        "var total = 2;\n"
        "read(): int {\n"
        "    return total;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *hoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "total", 1, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local expression reference hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.referenceFact != ZR_NULL &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Reference: read") &&
             hover_contains_text(hover, "Symbol: total") &&
             hover_contains_text(hover, "Declared at: 1:5");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected hover to surface resolved reference fact; status=%d reference=%p hover=%p text=%s\n",
               (int)query.status,
               (void *)query.referenceFact,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>");
    }

    hover_free(state, hover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_expression_hover_surfaces_assignment_write_reference_fact(SZrState *state) {
    const TZrChar *uriText = "file:///local_assignment_write_reference_hover.zr";
    const TZrChar *content =
        "var seed = 1;\n"
        "mutate(): int {\n"
        "    seed = 3;\n"
        "    return seed;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
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
        !find_position_for_substring(content, "seed", 1, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local assignment write reference hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.referenceFact != ZR_NULL &&
             query.referenceFact->kind == ZR_SEMANTIC_REFERENCE_WRITE &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Reference: write") &&
             hover_contains_text(hover, "Symbol: seed") &&
             hover_contains_text(hover, "Declared at: 1:5") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, position, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "reference", "write") &&
             rich_hover_section_contains_text(richHover, "symbol", "seed") &&
             rich_hover_section_contains_text(richHover, "declaration", "1:5");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected assignment target hover to surface write reference fact; status=%d reference=%p kind=%d hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.referenceFact,
               query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_expression_hover_surfaces_member_write_reference_fact(SZrState *state) {
    const TZrChar *uriText = "file:///local_member_write_reference_hover.zr";
    const TZrChar *content =
        "var seed = { value: 1 };\n"
        "mutate(): int {\n"
        "    seed.value = 3;\n"
        "    return seed.value;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
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
        !find_position_for_substring(content, "value", 1, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local member write reference hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.referenceFact != ZR_NULL &&
             query.referenceFact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE &&
             !query.referenceFact->isResolved &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Reference: member write") &&
             hover_contains_text(hover, "Symbol: value") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, position, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "reference", "member write") &&
             rich_hover_section_contains_text(richHover, "symbol", "value");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected member assignment hover to surface member write reference fact; status=%d reference=%p kind=%d resolved=%d hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.referenceFact,
               query.referenceFact != ZR_NULL ? (int)query.referenceFact->kind : -1,
               query.referenceFact != ZR_NULL ? (int)query.referenceFact->isResolved : -1,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_rich_hover_structures_shared_fact_sections(SZrState *state) {
    const TZrChar *uriText = "file:///local_rich_hover_fact_sections.zr";
    const TZrChar *content =
        "var total = 2;\n"
        "read(): int {\n"
        "    var numeric = 1 + 2;\n"
        "    var logical = true || false;\n"
        "    return total;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition numericPosition;
    SZrLspPosition logicalPosition;
    SZrLspPosition referencePosition;
    SZrLspRichHover *numericHover = ZR_NULL;
    SZrLspRichHover *logicalHover = ZR_NULL;
    SZrLspRichHover *referenceHover = ZR_NULL;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "+", 0, &numericPosition) ||
        !find_position_for_substring(content, "||", 0, &logicalPosition) ||
        !find_position_for_substring(content, "total", 1, &referencePosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local rich-hover fact section fixture\n");
        return ZR_FALSE;
    }

    passed = ZrLanguageServer_Lsp_GetRichHover(state, context, uri, numericPosition, &numericHover) &&
             numericHover != ZR_NULL &&
             rich_hover_section_contains_text(numericHover, "numericRange", "3..3") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, logicalPosition, &logicalHover) &&
             logicalHover != ZR_NULL &&
             rich_hover_section_contains_text(logicalHover, "logicalValue", "true") &&
             rich_hover_section_contains_text(logicalHover, "logicalFlow", "short-circuits right operand") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, referencePosition, &referenceHover) &&
             referenceHover != ZR_NULL &&
             rich_hover_section_contains_text(referenceHover, "reference", "read") &&
             rich_hover_section_contains_text(referenceHover, "symbol", "total") &&
             rich_hover_section_contains_text(referenceHover, "declaration", "1:5");

    if (!passed) {
        printf("FAIL: expected rich hover to expose numeric/logical/reference fact roles; numeric=%p logical=%p reference=%p\n",
               (void *)numericHover,
               (void *)logicalHover,
               (void *)referenceHover);
    }

    ZrLanguageServer_Lsp_FreeRichHover(state, numericHover);
    ZrLanguageServer_Lsp_FreeRichHover(state, logicalHover);
    ZrLanguageServer_Lsp_FreeRichHover(state, referenceHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_call_member_payloads(SZrState *state) {
    const TZrChar *uriText = "file:///local_call_member_payload_hover.zr";
    const TZrChar *content =
        "func pick(value: int): int {\n"
        "    return value;\n"
        "}\n"
        "func read(): int {\n"
        "    var seed = 2;\n"
        "    var chosen = pick(42);\n"
        "    return seed.value;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition callPosition;
    SZrLspPosition memberPosition;
    SZrLspLocalSemanticQueryResult callQuery;
    SZrLspLocalSemanticQueryResult memberQuery;
    SZrLspHover *callHover = ZR_NULL;
    SZrLspHover *memberHover = ZR_NULL;
    SZrLspRichHover *callRichHover = ZR_NULL;
    SZrLspRichHover *memberRichHover = ZR_NULL;
    const TZrChar *callHoverText;
    const TZrChar *memberHoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "pick", 1, &callPosition) ||
        !find_position_for_substring(content, "value", 2, &memberPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local call/member payload hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&callQuery);
    ZrLanguageServer_LspLocalSemanticQuery_Init(&memberQuery);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                 context,
                                                                 uri,
                                                                 callPosition,
                                                                 &callQuery) &&
             callQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             callQuery.expressionFact != ZR_NULL &&
             callQuery.expressionFact->hasCallInfo &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &callQuery, &callHover) &&
             callHover != ZR_NULL &&
             hover_contains_text(callHover, "Call: pick args=1") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, callPosition, &callRichHover) &&
             callRichHover != ZR_NULL &&
             rich_hover_section_contains_text(callRichHover, "call", "pick args=1") &&
             ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                 context,
                                                                 uri,
                                                                 memberPosition,
                                                                 &memberQuery) &&
             memberQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             memberQuery.expressionFact != ZR_NULL &&
             memberQuery.expressionFact->hasMemberInfo &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &memberQuery, &memberHover) &&
             memberHover != ZR_NULL &&
             hover_contains_text(memberHover, "Member: value") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, memberPosition, &memberRichHover) &&
             memberRichHover != ZR_NULL &&
             rich_hover_section_contains_text(memberRichHover, "member", "value");

    if (!passed) {
        callHoverText = hover_first_text(callHover);
        memberHoverText = hover_first_text(memberHover);
        printf("FAIL: expected local hover/rich-hover to surface call/member payloads; call status=%d expr=%p hasCall=%d hover=%p text=%s rich=%p; member status=%d expr=%p hasMember=%d hover=%p text=%s rich=%p\n",
               (int)callQuery.status,
               (void *)callQuery.expressionFact,
               callQuery.expressionFact != ZR_NULL ? (int)callQuery.expressionFact->hasCallInfo : -1,
               (void *)callHover,
               callHoverText != ZR_NULL ? callHoverText : "<null>",
               (void *)callRichHover,
               (int)memberQuery.status,
               (void *)memberQuery.expressionFact,
               memberQuery.expressionFact != ZR_NULL ? (int)memberQuery.expressionFact->hasMemberInfo : -1,
               (void *)memberHover,
               memberHoverText != ZR_NULL ? memberHoverText : "<null>",
               (void *)memberRichHover);
    }

    hover_free(state, callHover);
    hover_free(state, memberHover);
    ZrLanguageServer_Lsp_FreeRichHover(state, callRichHover);
    ZrLanguageServer_Lsp_FreeRichHover(state, memberRichHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_reachability_cause(SZrState *state) {
    const TZrChar *uriText = "file:///local_reachability_hover.zr";
    const TZrChar *content =
        "flow(): int {\n"
        "    return 1;\n"
        "    var dead = 2;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
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
        !find_position_for_substring(content, "dead", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local reachability hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.reachabilityFact != ZR_NULL &&
             query.reachabilityFact->cause == ZR_SEMANTIC_REACHABILITY_AFTER_RETURN &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Reachability: unreachable after return") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, position, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "reachability", "unreachable after return");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected reachability hover to include concrete cause; status=%d reachability=%p cause=%d hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.reachabilityFact,
               query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_constant_boolean_branch_cause(SZrState *state) {
    const TZrChar *uriText = "file:///local_constant_boolean_branch_hover.zr";
    const TZrChar *content =
        "flow(): int {\n"
        "    var const flag = false;\n"
        "    if (flag) {\n"
        "        var deadThen = 2;\n"
        "    } else {\n"
        "        return 1;\n"
        "    }\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
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
        !find_position_for_substring(content, "deadThen", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local constant-boolean branch hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.reachabilityFact != ZR_NULL &&
             query.reachabilityFact->cause == ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH &&
             query.logicalFact != ZR_NULL &&
             query.logicalFact->hasKnownValue &&
             !query.logicalFact->knownValue &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Logical value: false") &&
             hover_contains_text(hover, "Reachability: unreachable because a constant branch excludes it") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, position, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "logicalValue", "false") &&
             rich_hover_section_contains_text(richHover,
                                              "reachability",
                                              "unreachable because a constant branch excludes it");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected constant boolean local to surface branch reachability; status=%d reachability=%p cause=%d logical=%p hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.reachabilityFact,
               query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
               (void *)query.logicalFact,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_constant_false_loop_body_cause(SZrState *state) {
    const TZrChar *uriText = "file:///local_constant_false_loop_hover.zr";
    const TZrChar *content =
        "flow(): int {\n"
        "    var const keepGoing = false;\n"
        "    while (keepGoing) {\n"
        "        var deadLoop = 2;\n"
        "    }\n"
        "    return 1;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
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
        !find_position_for_substring(content, "deadLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local constant-false loop hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.reachabilityFact != ZR_NULL &&
             query.reachabilityFact->cause == ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE &&
             query.logicalFact != ZR_NULL &&
             query.logicalFact->hasKnownValue &&
             !query.logicalFact->knownValue &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Logical value: false") &&
             hover_contains_text(hover, "Reachability: unreachable because the condition is false") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, position, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "logicalValue", "false") &&
             rich_hover_section_contains_text(richHover,
                                              "reachability",
                                              "unreachable because the condition is false");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected constant false loop body to surface reachability; status=%d reachability=%p cause=%d logical=%p hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.reachabilityFact,
               query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
               (void *)query.logicalFact,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_constant_true_branch_exit_cause(SZrState *state) {
    const TZrChar *uriText = "file:///local_constant_true_branch_exit_hover.zr";
    const TZrChar *content =
        "flow(): int {\n"
        "    var const flag = true;\n"
        "    if (flag) {\n"
        "        return 1;\n"
        "    }\n"
        "    var afterBranch = 2;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
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
        !find_position_for_substring(content, "afterBranch", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local constant-true branch exit hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.reachabilityFact != ZR_NULL &&
             query.reachabilityFact->cause == ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH &&
             query.logicalFact != ZR_NULL &&
             query.logicalFact->hasKnownValue &&
             query.logicalFact->knownValue &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Logical value: true") &&
             hover_contains_text(hover, "Reachability: unreachable after exhaustive branch") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, position, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "logicalValue", "true") &&
             rich_hover_section_contains_text(richHover,
                                              "reachability",
                                              "unreachable after exhaustive branch");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected constant true branch exit to mark following statement unreachable; status=%d reachability=%p cause=%d logical=%p hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.reachabilityFact,
               query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
               (void *)query.logicalFact,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_loop_jump_exit_causes(SZrState *state) {
    const TZrChar *uriText = "file:///local_loop_jump_exit_hover.zr";
    const TZrChar *content =
        "flow(): int {\n"
        "    var sum = 0;\n"
        "    for (var i = 0; i < 3; i = i + 1) {\n"
        "        if (i == 0) {\n"
        "            continue;\n"
        "            var afterContinue = i;\n"
        "        }\n"
        "        if (i == 1) {\n"
        "            break;\n"
        "            var afterBreak = i;\n"
        "        }\n"
        "        sum = sum + i;\n"
        "    }\n"
        "    return sum;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition continuePosition;
    SZrLspPosition breakPosition;
    SZrLspLocalSemanticQueryResult continueQuery;
    SZrLspLocalSemanticQueryResult breakQuery;
    SZrLspHover *continueHover = ZR_NULL;
    SZrLspHover *breakHover = ZR_NULL;
    const TZrChar *continueHoverText;
    const TZrChar *breakHoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "afterContinue", 0, &continuePosition) ||
        !find_position_for_substring(content, "afterBreak", 0, &breakPosition)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local loop jump exit hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&continueQuery);
    ZrLanguageServer_LspLocalSemanticQuery_Init(&breakQuery);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                 context,
                                                                 uri,
                                                                 continuePosition,
                                                                 &continueQuery) &&
             continueQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             continueQuery.reachabilityFact != ZR_NULL &&
             continueQuery.reachabilityFact->cause == ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &continueQuery, &continueHover) &&
             continueHover != ZR_NULL &&
             hover_contains_text(continueHover, "Reachability: unreachable after continue") &&
             ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state,
                                                                 context,
                                                                 uri,
                                                                 breakPosition,
                                                                 &breakQuery) &&
             breakQuery.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             breakQuery.reachabilityFact != ZR_NULL &&
             breakQuery.reachabilityFact->cause == ZR_SEMANTIC_REACHABILITY_AFTER_BREAK &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &breakQuery, &breakHover) &&
             breakHover != ZR_NULL &&
             hover_contains_text(breakHover, "Reachability: unreachable after break");

    if (!passed) {
        continueHoverText = hover_first_text(continueHover);
        breakHoverText = hover_first_text(breakHover);
        printf("FAIL: expected loop jump exits to mark following statements unreachable; continue status=%d reachability=%p cause=%d hover=%p text=%s; break status=%d reachability=%p cause=%d hover=%p text=%s\n",
               (int)continueQuery.status,
               (void *)continueQuery.reachabilityFact,
               continueQuery.reachabilityFact != ZR_NULL ? (int)continueQuery.reachabilityFact->cause : -1,
               (void *)continueHover,
               continueHoverText != ZR_NULL ? continueHoverText : "<null>",
               (int)breakQuery.status,
               (void *)breakQuery.reachabilityFact,
               breakQuery.reachabilityFact != ZR_NULL ? (int)breakQuery.reachabilityFact->cause : -1,
               (void *)breakHover,
               breakHoverText != ZR_NULL ? breakHoverText : "<null>");
    }

    hover_free(state, continueHover);
    hover_free(state, breakHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_constant_true_loop_exit_cause(SZrState *state) {
    const TZrChar *uriText = "file:///local_constant_true_loop_exit_hover.zr";
    const TZrChar *content =
        "flow(): int {\n"
        "    while (true) {\n"
        "        return 1;\n"
        "    }\n"
        "    var afterLoop = 2;\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspLocalSemanticQueryResult query;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *hoverText;
    TZrBool passed;

    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    if (context == ZR_NULL ||
        uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) ||
        !find_position_for_substring(content, "afterLoop", 0, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local constant-true loop exit hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.reachabilityFact != ZR_NULL &&
             query.reachabilityFact->cause == ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP &&
             query.logicalFact != ZR_NULL &&
             query.logicalFact->hasKnownValue &&
             query.logicalFact->knownValue &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Logical value: true") &&
             hover_contains_text(hover, "Reachability: unreachable after non-fallthrough loop");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected constant true loop exit to mark following statement unreachable; status=%d reachability=%p cause=%d logical=%p hover=%p text=%s\n",
               (int)query.status,
               (void *)query.reachabilityFact,
               query.reachabilityFact != ZR_NULL ? (int)query.reachabilityFact->cause : -1,
               (void *)query.logicalFact,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>");
    }

    hover_free(state, hover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

static TZrBool test_local_hover_surfaces_ownership_violation_message(SZrState *state) {
    const TZrChar *uriText = "file:///local_ownership_violation_hover.zr";
    const TZrChar *content =
        "class Resource {\n"
        "}\n"
        "leak(resource: %unique Resource): %loaned Resource {\n"
        "    return %loan(resource);\n"
        "}\n";
    SZrLspContext *context;
    SZrString *uri;
    SZrLspPosition position;
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
        !find_position_for_substring(content, "%loan", 1, &position)) {
        if (context != ZR_NULL) {
            ZrLanguageServer_LspContext_Free(state, context);
        }
        printf("FAIL: unable to prepare local ownership violation hover fixture\n");
        return ZR_FALSE;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    passed = ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(state, context, uri, position, &query) &&
             query.status == ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT &&
             query.ownershipFact != ZR_NULL &&
             query.ownershipFact->isViolation &&
             ZrLanguageServer_LspLocalSemanticQuery_BuildHover(state, &query, &hover) &&
             hover != ZR_NULL &&
             hover_contains_text(hover, "Ownership: violation") &&
             hover_contains_text(hover, "Loaned value cannot escape") &&
             ZrLanguageServer_Lsp_GetRichHover(state, context, uri, position, &richHover) &&
             richHover != ZR_NULL &&
             rich_hover_section_contains_text(richHover, "ownership", "violation") &&
             rich_hover_section_contains_text(richHover, "ownership", "Loaned value cannot escape");

    if (!passed) {
        hoverText = hover_first_text(hover);
        printf("FAIL: expected ownership hover to keep violation message in ownership role; status=%d ownership=%p violation=%d hover=%p text=%s rich=%p\n",
               (int)query.status,
               (void *)query.ownershipFact,
               query.ownershipFact != ZR_NULL ? (int)query.ownershipFact->isViolation : -1,
               (void *)hover,
               hoverText != ZR_NULL ? hoverText : "<null>",
               (void *)richHover);
    }

    hover_free(state, hover);
    ZrLanguageServer_Lsp_FreeRichHover(state, richHover);
    ZrLanguageServer_LspContext_Free(state, context);
    return passed;
}

int main(void) {
    SZrCallbackGlobal callbacks;
    SZrGlobalState *global;
    SZrState *state;
    TZrBool referenceHoverPassed;
    TZrBool assignmentWriteReferenceHoverPassed;
    TZrBool memberWriteReferenceHoverPassed;
    TZrBool richHoverPassed;
    TZrBool callMemberHoverPassed;
    TZrBool reachabilityHoverPassed;
    TZrBool constantBooleanBranchHoverPassed;
    TZrBool constantFalseLoopHoverPassed;
    TZrBool constantTrueBranchExitHoverPassed;
    TZrBool loopJumpExitHoverPassed;
    TZrBool constantTrueLoopExitHoverPassed;
    TZrBool ownershipHoverPassed;

    memset(&callbacks, 0, sizeof(callbacks));
    global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        printf("FAIL: unable to create test state\n");
        return 1;
    }

    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    printf("ZR VM LSP Local Semantic Hover Tests\n");
    printf("====================================\n");
    referenceHoverPassed = test_local_expression_hover_surfaces_reference_fact(state);
    printf("%s: LSP Local Expression Hover Surfaces Reference Fact\n",
           referenceHoverPassed ? "PASS" : "FAIL");
    assignmentWriteReferenceHoverPassed =
        test_local_expression_hover_surfaces_assignment_write_reference_fact(state);
    printf("%s: LSP Local Expression Hover Surfaces Assignment Write Reference Fact\n",
           assignmentWriteReferenceHoverPassed ? "PASS" : "FAIL");
    memberWriteReferenceHoverPassed =
        test_local_expression_hover_surfaces_member_write_reference_fact(state);
    printf("%s: LSP Local Expression Hover Surfaces Member Write Reference Fact\n",
           memberWriteReferenceHoverPassed ? "PASS" : "FAIL");
    richHoverPassed = test_local_rich_hover_structures_shared_fact_sections(state);
    printf("%s: LSP Local Rich Hover Structures Shared Fact Sections\n",
           richHoverPassed ? "PASS" : "FAIL");
    callMemberHoverPassed = test_local_hover_surfaces_call_member_payloads(state);
    printf("%s: LSP Local Hover Surfaces Call/Member Payloads\n",
           callMemberHoverPassed ? "PASS" : "FAIL");
    reachabilityHoverPassed = test_local_hover_surfaces_reachability_cause(state);
    printf("%s: LSP Local Hover Surfaces Reachability Cause\n",
           reachabilityHoverPassed ? "PASS" : "FAIL");
    constantBooleanBranchHoverPassed = test_local_hover_surfaces_constant_boolean_branch_cause(state);
    printf("%s: LSP Local Hover Surfaces Constant Boolean Branch Cause\n",
           constantBooleanBranchHoverPassed ? "PASS" : "FAIL");
    constantFalseLoopHoverPassed = test_local_hover_surfaces_constant_false_loop_body_cause(state);
    printf("%s: LSP Local Hover Surfaces Constant False Loop Body Cause\n",
           constantFalseLoopHoverPassed ? "PASS" : "FAIL");
    constantTrueBranchExitHoverPassed = test_local_hover_surfaces_constant_true_branch_exit_cause(state);
    printf("%s: LSP Local Hover Surfaces Constant True Branch Exit Cause\n",
           constantTrueBranchExitHoverPassed ? "PASS" : "FAIL");
    loopJumpExitHoverPassed = test_local_hover_surfaces_loop_jump_exit_causes(state);
    printf("%s: LSP Local Hover Surfaces Loop Jump Exit Causes\n",
           loopJumpExitHoverPassed ? "PASS" : "FAIL");
    constantTrueLoopExitHoverPassed = test_local_hover_surfaces_constant_true_loop_exit_cause(state);
    printf("%s: LSP Local Hover Surfaces Constant True Loop Exit Cause\n",
           constantTrueLoopExitHoverPassed ? "PASS" : "FAIL");
    ownershipHoverPassed = test_local_hover_surfaces_ownership_violation_message(state);
    printf("%s: LSP Local Hover Surfaces Ownership Violation Message\n",
           ownershipHoverPassed ? "PASS" : "FAIL");

    ZrCore_GlobalState_Free(global);
    return referenceHoverPassed && assignmentWriteReferenceHoverPassed && memberWriteReferenceHoverPassed &&
                   richHoverPassed && callMemberHoverPassed &&
                   reachabilityHoverPassed &&
                   constantBooleanBranchHoverPassed && constantFalseLoopHoverPassed &&
                   constantTrueBranchExitHoverPassed && loopJumpExitHoverPassed &&
                   constantTrueLoopExitHoverPassed && ownershipHoverPassed
               ? 0
               : 1;
}
