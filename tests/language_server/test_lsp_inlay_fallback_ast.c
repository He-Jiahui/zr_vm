/* LSP-U4 (t727u, decision B) inlay half: while a syntax error keeps the last-good AST
 * (usesFallbackAst), inlay hints must not project that tree's hints or textEdits onto
 * the current text; after recovery they come back at the new positions. */
#include <stdio.h>
#include <string.h>

#include "interface/lsp_interface_internal.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"

static void print_hints(const char *stage, const SZrArray *hints) {
    printf("  %s: %u hint(s)\n", stage, (unsigned)(hints->isValid ? hints->length : 0U));
    for (TZrSize i = 0U; hints->isValid && i < hints->length; ++i) {
        const SZrLspInlayHint *hint = *(SZrLspInlayHint **)ZrCore_Array_Get((SZrArray *)hints, i);
        printf("    %d:%d kind=%d textEdits=%u\n", (int)hint->position.line, (int)hint->position.character,
               (int)hint->kind, (unsigned)(hint->textEdits.isValid ? hint->textEdits.length : 0U));
    }
}

static TZrBool get_hints(SZrState *state, SZrLspContext *context, SZrString *uri, SZrArray *hints) {
    SZrLspRange range;
    memset(&range, 0, sizeof(range));
    range.end.line = 100;
    memset(hints, 0, sizeof(*hints));
    return ZrLanguageServer_Lsp_GetInlayHints(state, context, uri, range, hints);
}

static TZrBool has_hint_at(const SZrArray *hints, TZrInt32 line, TZrInt32 character) {
    for (TZrSize i = 0U; hints->isValid && i < hints->length; ++i) {
        const SZrLspInlayHint *hint = *(SZrLspInlayHint **)ZrCore_Array_Get((SZrArray *)hints, i);
        if (hint->position.line == line && hint->position.character == character) return ZR_TRUE;
    }
    return ZR_FALSE;
}

/* `let total` type hint and the `left:` / `right:` parameter hints of `add(1, 2)`. */
static TZrBool has_call_line_hints(const SZrArray *hints, TZrInt32 line) {
    return has_hint_at(hints, line, 9) && has_hint_at(hints, line, 16) && has_hint_at(hints, line, 19);
}

int main(void) {
    static const char *valid =
            "fn add(left: int, right: int): int { return left + right; }\n"
            "let total = add(1, 2);\n";
    /* One inserted line above the call plus a syntax error: the last-good AST still says
     * the call is on line 1, but in the current text line 1 is the broken statement. */
    static const char *broken =
            "fn add(left: int, right: int): int { return left + right; }\n"
            "let broken = ;\n"
            "let total = add(1, 2);\n";
    static const char *recovered =
            "fn add(left: int, right: int): int { return left + right; }\n"
            "let fixed = 0;\n"
            "let total = add(1, 2);\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLspContext *context;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrArray hints;
    TZrBool baseline = ZR_FALSE, fallback = ZR_FALSE, recovery = ZR_FALSE, sawFallback = ZR_FALSE;

    if (state == ZR_NULL) return 1;
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZR_STRING_LITERAL(state, "file:///inlay-fallback.zr");
    if (context != ZR_NULL && uri != ZR_NULL &&
        ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, valid, strlen(valid), 1)) {
        baseline = get_hints(state, context, uri, &hints) && hints.isValid && has_call_line_hints(&hints, 1);
        print_hints("valid v1", &hints);
        ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);

        if (ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, broken, strlen(broken), 2)) {
            fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
            sawFallback = fileVersion != ZR_NULL && fileVersion->usesFallbackAst;
            (void)get_hints(state, context, uri, &hints);
            print_hints("fallback v2", &hints);
            fallback = sawFallback && (!hints.isValid || hints.length == 0U);
            ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
        }
        if (ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, recovered, strlen(recovered), 3)) {
            fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
            recovery = fileVersion != ZR_NULL && !fileVersion->usesFallbackAst &&
                       get_hints(state, context, uri, &hints) && hints.isValid && has_call_line_hints(&hints, 2) &&
                       !has_hint_at(&hints, 2, 1) && !has_hint_at(&hints, 2, 4);
            print_hints("recovered v3", &hints);
            ZrLanguageServer_Lsp_FreeInlayHints(state, &hints);
        }
    }
    printf("%s: valid document publishes inlay hints on the call line\n", baseline ? "PASS" : "FAIL");
    printf("%s: fallback AST (usesFallbackAst=%d) publishes no inlay hints or textEdits\n",
           fallback ? "PASS" : "FAIL", (int)sawFallback);
    printf("%s: recovered document publishes hints at the new positions\n", recovery ? "PASS" : "FAIL");
    ZrLanguageServer_LspContext_Free(state, context);
    ZrTests_Runtime_State_Destroy(state);
    return baseline && fallback && recovery ? 0 : 1;
}
