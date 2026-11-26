/* LSP-U4 (t727u, decision B) token half: while a syntax error keeps the last-good AST
 * (usesFallbackAst), semantic tokens must not project that tree's symbols onto the
 * current text. The fallback result uses only the text candidates that need no AST
 * (keywords, decorators, meta-methods), and symbol tokens return after recovery. */
#include <stdio.h>
#include <string.h>

#include "interface/lsp_interface_internal.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"

typedef struct Token { TZrUInt32 line, character, length, type, modifiers; } Token;

static TZrSize decode(const SZrArray *data, Token *out, TZrSize capacity) {
    TZrUInt32 line = 0U, character = 0U;
    TZrSize count = 0U;
    for (TZrSize i = 0U; data->isValid && i + 4U < data->length + 0U && count < capacity; i += 5U) {
        TZrUInt32 v[5];
        for (TZrSize k = 0U; k < 5U; ++k) v[k] = *(TZrUInt32 *)ZrCore_Array_Get((SZrArray *)data, i + k);
        if (v[0] != 0U) { line += v[0]; character = v[1]; } else { character += v[1]; }
        out[count].line = line; out[count].character = character; out[count].length = v[2];
        out[count].type = v[3]; out[count].modifiers = v[4]; ++count;
    }
    return count;
}

static TZrSize tokens_of(SZrState *state, SZrLspContext *context, SZrString *uri, const char *stage,
                         Token *out, TZrSize capacity) {
    SZrArray data;
    TZrSize count;
    memset(&data, 0, sizeof(data));
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, &data)) return (TZrSize)-1;
    count = decode(&data, out, capacity);
    printf("  %s: %u token(s)\n", stage, (unsigned)count);
    for (TZrSize i = 0U; i < count; ++i) {
        const TZrChar *name = ZrLanguageServer_Lsp_SemanticTokenTypeName(out[i].type);
        printf("    %u:%u len=%u %s mod=%u\n", out[i].line, out[i].character, out[i].length,
               name != ZR_NULL ? name : "?", out[i].modifiers);
    }
    ZrCore_Array_Free(state, &data);
    return count;
}

static TZrBool only_text_candidates(const Token *tokens, TZrSize count) {
    for (TZrSize i = 0U; i < count; ++i) {
        const TZrChar *name = ZrLanguageServer_Lsp_SemanticTokenTypeName(tokens[i].type);
        if (name == ZR_NULL || (strcmp(name, "keyword") != 0 && strcmp(name, "decorator") != 0 &&
                                strcmp(name, "metaMethod") != 0)) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool has_token(const Token *tokens, TZrSize count, TZrUInt32 line, TZrUInt32 character,
                         const char *typeName) {
    for (TZrSize i = 0U; i < count; ++i) {
        const TZrChar *name = ZrLanguageServer_Lsp_SemanticTokenTypeName(tokens[i].type);
        if (tokens[i].line == line && tokens[i].character == character &&
            name != ZR_NULL && strcmp(name, typeName) == 0) return ZR_TRUE;
    }
    return ZR_FALSE;
}

int main(void) {
    static const char *valid =
            "fn add(left: int, right: int): int { return left + right; }\n"
            "let total = add(1, 2);\n";
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
    Token v1[64], v2[64], v3[64];
    TZrSize n1 = 0, n2 = 0, n3 = 0;
    TZrBool sawFallback = ZR_FALSE, baseline = ZR_FALSE, fallback = ZR_FALSE, recovery = ZR_FALSE;

    if (state == ZR_NULL) return 1;
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZR_STRING_LITERAL(state, "file:///tokens-fallback.zr");
    if (context != ZR_NULL && uri != ZR_NULL &&
        ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, valid, strlen(valid), 1)) {
        n1 = tokens_of(state, context, uri, "valid v1", v1, 64);
        /* `add` call on line 1 is a resolved function symbol. */
        baseline = n1 != (TZrSize)-1 && has_token(v1, n1, 1, 12, "function");
        if (ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, broken, strlen(broken), 2)) {
            fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
            sawFallback = fileVersion != ZR_NULL && fileVersion->usesFallbackAst;
            n2 = tokens_of(state, context, uri, "fallback v2", v2, 64);
        }
        /* Before the fix the old tree's `total` variable lands on `broken` (1:4) and the
         * real call at 2:12 loses its function token. */
        fallback = sawFallback && n2 != (TZrSize)-1 && only_text_candidates(v2, n2) &&
                   has_token(v2, n2, 0, 0, "keyword") && has_token(v2, n2, 1, 0, "keyword") &&
                   has_token(v2, n2, 2, 0, "keyword");
        if (ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, recovered, strlen(recovered), 3)) {
            n3 = tokens_of(state, context, uri, "recovered v3", v3, 64);
            recovery = n3 != (TZrSize)-1 && has_token(v3, n3, 2, 12, "function") &&
                       !has_token(v3, n3, 1, 12, "function");
        }
    }
    printf("%s: valid document colours the resolved call\n", baseline ? "PASS" : "FAIL");
    printf("%s: fallback AST (usesFallbackAst=%d) publishes only text-candidate tokens\n",
           fallback ? "PASS" : "FAIL", (int)sawFallback);
    printf("%s: recovered document colours the call at its new position\n", recovery ? "PASS" : "FAIL");
    ZrLanguageServer_LspContext_Free(state, context);
    ZrTests_Runtime_State_Destroy(state);
    return baseline && fallback && recovery ? 0 : 1;
}
