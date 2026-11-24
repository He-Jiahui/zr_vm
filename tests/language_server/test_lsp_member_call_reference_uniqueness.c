/* D4 (t727u) LSP-level uniqueness: a resolved member call publishes two CALL
 * facts with the same symbol, kind and range (bound member + invocation view).
 * Symbol-enumerating LSP features must report each call site exactly once. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "interface/lsp_interface_internal.h"
#include "lsp_query_result_cleanup.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __func__, __LINE__, #condition); \
        return ZR_FALSE; \
    } \
} while (0)

/* Locates the n-th spelling of a test token only to build request positions;
 * the expected identities are the server's own results, not spelling matches. */
static SZrLspPosition token_position(const char *text, const char *token, TZrSize occurrence) {
    const char *match = strstr(text, token);
    SZrLspPosition position = {0, 0};
    for (TZrSize i = 0U; match != ZR_NULL && i < occurrence; ++i) {
        match = strstr(match + strlen(token), token);
    }
    assert(match != ZR_NULL);
    for (const char *cursor = text; cursor < match; ++cursor) {
        if (*cursor == '\n') {
            ++position.line;
            position.character = 0;
        } else {
            ++position.character;
        }
    }
    return position;
}

static TZrBool ranges_equal(SZrLspRange left, SZrLspRange right) {
    return left.start.line == right.start.line && left.start.character == right.start.character &&
           left.end.line == right.end.line && left.end.character == right.end.character;
}

/* Every expected site must appear exactly once, and nothing else may appear. */
static TZrBool locations_are_exactly(const SZrArray *locations, const char *text,
                                     const char *token, const TZrSize *occurrences,
                                     TZrSize count) {
    REQUIRE(locations->isValid || count == 0U);
    REQUIRE(locations->length == count);
    for (TZrSize i = 0U; i < locations->length; ++i) {
        const SZrLspLocation *location = *(SZrLspLocation **)ZrCore_Array_Get((SZrArray *)locations, i);
        for (TZrSize j = 0U; j < i; ++j) {
            const SZrLspLocation *previous = *(SZrLspLocation **)ZrCore_Array_Get((SZrArray *)locations, j);
            REQUIRE(!ranges_equal(previous->range, location->range));
        }
    }
    for (TZrSize k = 0U; k < count; ++k) {
        SZrLspPosition start = token_position(text, token, occurrences[k]);
        TZrSize hits = 0U;
        for (TZrSize i = 0U; i < locations->length; ++i) {
            const SZrLspLocation *location = *(SZrLspLocation **)ZrCore_Array_Get((SZrArray *)locations, i);
            if (location->range.start.line == start.line &&
                location->range.start.character == start.character &&
                location->range.end.line == start.line &&
                location->range.end.character == start.character + (TZrInt32)strlen(token)) {
                ++hits;
            }
        }
        REQUIRE(hits == 1U);
    }
    return ZR_TRUE;
}

static TZrBool check_member_call_sites(SZrState *state, SZrLspContext *context, SZrString *uri,
                                       const char *text) {
    /* occurrences of "text": 0 = declaration, 1 and 2 = call sites */
    const TZrSize all[] = {0U, 1U, 2U};
    const TZrSize calls[] = {1U, 2U};
    SZrArray locations = {0};
    SZrArray withoutDeclaration = {0};
    SZrArray renamed = {0};
    SZrArray highlights = {0};
    SZrLspPosition callSite = token_position(text, "text", 1U);
    TZrBool passed;

    passed = ZrLanguageServer_Lsp_FindReferences(state, context, uri, callSite, ZR_TRUE, &locations) &&
             locations_are_exactly(&locations, text, "text", all, 3U);
    if (!passed) fprintf(stderr, "references(includeDeclaration) count=%u\n", (unsigned)locations.length);
    if (passed) {
        passed = ZrLanguageServer_Lsp_FindReferences(state, context, uri, callSite, ZR_FALSE,
                                                     &withoutDeclaration) &&
                 locations_are_exactly(&withoutDeclaration, text, "text", calls, 2U);
        if (!passed) fprintf(stderr, "references count=%u\n", (unsigned)withoutDeclaration.length);
    }
    if (passed) {
        passed = ZrLanguageServer_Lsp_Rename(state, context, uri, callSite,
                                             ZR_STRING_LITERAL(state, "label"), &renamed) &&
                 locations_are_exactly(&renamed, text, "text", all, 3U);
        if (!passed) fprintf(stderr, "rename count=%u\n", (unsigned)renamed.length);
    }
    if (passed) {
        passed = ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, callSite, &highlights) &&
                 highlights.length == 3U;
        for (TZrSize i = 0U; passed && i < highlights.length; ++i) {
            const SZrLspDocumentHighlight *highlight =
                    *(SZrLspDocumentHighlight **)ZrCore_Array_Get(&highlights, i);
            for (TZrSize j = 0U; passed && j < i; ++j) {
                const SZrLspDocumentHighlight *previous =
                        *(SZrLspDocumentHighlight **)ZrCore_Array_Get(&highlights, j);
                passed = !ranges_equal(previous->range, highlight->range);
            }
        }
        if (!passed) fprintf(stderr, "highlight count=%u\n", (unsigned)highlights.length);
    }
    free_local_reference_projection_results(state, &locations, ZR_NULL);
    free_local_reference_projection_results(state, &withoutDeclaration, ZR_NULL);
    free_local_reference_projection_results(state, &renamed, ZR_NULL);
    free_local_reference_projection_results(state, ZR_NULL, &highlights);
    return passed;
}

int main(void) {
    static const char *text =
            "class Calculator {\n"
            "    pub fn text(value: string): string { return value; }\n"
            "}\n"
            "fn first(calculator: Calculator): string { return calculator.text(\"a\"); }\n"
            "fn second(calculator: Calculator): string { return calculator.text(\"b\"); }\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLspContext *context;
    SZrString *uri;
    TZrBool passed = ZR_FALSE;

    if (state == ZR_NULL) {
        return 1;
    }
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZR_STRING_LITERAL(state, "file:///member-call-uniqueness.zr");
    if (context != ZR_NULL && uri != ZR_NULL &&
        ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, text, strlen(text), 1U)) {
        passed = check_member_call_sites(state, context, uri, text);
    }
    printf("%s: member call references/rename/highlight report each call site once\n",
           passed ? "PASS" : "FAIL");
    ZrLanguageServer_LspContext_Free(state, context);
    ZrTests_Runtime_State_Destroy(state);
    return passed ? 0 : 1;
}
