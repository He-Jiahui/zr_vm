#ifndef ZR_VM_TEST_LSP_SEMANTIC_TOKEN_CONTEXTUAL_WORD_CASES_H
#define ZR_VM_TEST_LSP_SEMANTIC_TOKEN_CONTEXTUAL_WORD_CASES_H

#include "semantic/query/lsp_semantic_token_canonical.h"

/* 由 test_lsp_interface.c 末尾包含（依赖其 semantic_tokens_contain 系列助手）：
 * 词法器按标识符返回的上下文词（move/init/where/native…）若已解析为符号就按符号着色；
 * 与导入别名同名的参数按参数着色；保留字仍是 keyword。 */

/* 打开单文档并取全量语义 token；失败时返回 ZR_FALSE 并清理。 */
static TZrBool contextual_word_tokens_open(SZrState *state,
                                           const TZrChar *uriText,
                                           const TZrChar *content,
                                           SZrLspContext **outContext,
                                           SZrArray *tokens) {
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri;

    *outContext = context;
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }
    uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    ZrCore_Array_Init(state, tokens, sizeof(TZrUInt32), 32);
    return uri != ZR_NULL &&
           ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, content, strlen(content), 1) &&
           ZrLanguageServer_Lsp_GetSemanticTokens(state, context, uri, tokens);
}

/* 断言 content 中第 occurrence 个 needle（偏移 delta 处，长度 length）是 typeName。 */
static TZrBool contextual_word_token_is(SZrArray *tokens,
                                        const TZrChar *content,
                                        const TZrChar *needle,
                                        TZrSize occurrence,
                                        TZrInt32 delta,
                                        TZrInt32 length,
                                        const TZrChar *typeName,
                                        TZrUInt32 modifierMask) {
    SZrLspPosition position;

    if (!lsp_find_position_for_substring(content, needle, occurrence, delta, &position)) {
        printf("  fixture lookup failed: %s#%u\n", needle, (unsigned)occurrence);
        return ZR_FALSE;
    }
    if (modifierMask != 0U
            ? semantic_tokens_contain_modifier(tokens, position.line, position.character, length,
                                               typeName, modifierMask)
            : semantic_tokens_contain(tokens, position.line, position.character, length, typeName)) {
        return ZR_TRUE;
    }
    printf("  expected %s at %d:%d len %d for '%s'#%u\n",
           typeName, position.line, position.character, length, needle, (unsigned)occurrence);
    return ZR_FALSE;
}

static void test_lsp_semantic_tokens_contextual_words_resolve_as_symbols(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Semantic Tokens Classify Resolved Contextual Words As Symbols";
    const TZrChar *content =
        "class Box {\n"
        "    pub fn move(): int { return 3; }\n"
        "}\n"
        "pub fn move(): int {\n"
        "    return 1;\n"
        "}\n"
        "var first = move();\n"
        "var box = new Box();\n"
        "var second = box.move();\n";
    SZrLspContext *context = ZR_NULL;
    SZrArray tokens = {0};
    TZrBool ok;

    TEST_START(summary);
    ok = contextual_word_tokens_open(state, "file:///semantic_tokens_contextual_move.zr", content,
                                     &context, &tokens);
    ok = ok &&
         contextual_word_token_is(&tokens, content, "pub fn move()", 1, 7, 4, "function",
                                  ZR_LSP_SEMANTIC_TOKEN_MODIFIER_DECLARATION) &&
         contextual_word_token_is(&tokens, content, "first = move()", 0, 8, 4, "function", 0U) &&
         contextual_word_token_is(&tokens, content, "box.move()", 0, 4, 4, "method", 0U);
    if (!ok) {
        TEST_FAIL(timer, summary, "a resolved contextual word was not classified as its symbol");
    } else {
        TEST_PASS(timer, summary);
    }
    if (tokens.isValid) {
        ZrCore_Array_Free(state, &tokens);
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_semantic_tokens_contextual_import_alias_is_namespace(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Semantic Tokens Classify A Contextual-Word Import Alias As Namespace";
    const TZrChar *content =
        "let native = import(\"zr.system\");\n"
        "fn run() {\n"
        "    native.console.printLine(\"x\");\n"
        "}\n";
    SZrLspContext *context = ZR_NULL;
    SZrArray tokens = {0};
    TZrBool ok;

    TEST_START(summary);
    ok = contextual_word_tokens_open(state, "file:///semantic_tokens_contextual_native_alias.zr", content,
                                     &context, &tokens);
    ok = ok &&
         contextual_word_token_is(&tokens, content, "native = import", 0, 0, 6, "namespace", 0U) &&
         contextual_word_token_is(&tokens, content, "native.console", 0, 0, 6, "namespace", 0U);
    if (!ok) {
        TEST_FAIL(timer, summary, "an import alias spelled like a contextual word was not a namespace");
    } else {
        TEST_PASS(timer, summary);
    }
    if (tokens.isValid) {
        ZrCore_Array_Free(state, &tokens);
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_semantic_tokens_parameter_shadows_import_alias(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Semantic Tokens Classify A Parameter Shadowing An Import Alias As Parameter";
    const TZrChar *content =
        "let system = import(\"zr.system\");\n"
        "fn scale(system: int): int {\n"
        "    return system;\n"
        "}\n"
        "fn run() {\n"
        "    system.console.printLine(\"x\");\n"
        "}\n";
    SZrLspContext *context = ZR_NULL;
    SZrArray tokens = {0};
    TZrBool ok;

    TEST_START(summary);
    ok = contextual_word_tokens_open(state, "file:///semantic_tokens_shadowed_alias.zr", content,
                                     &context, &tokens);
    ok = ok &&
         contextual_word_token_is(&tokens, content, "system: int", 0, 0, 6, "parameter", 0U) &&
         contextual_word_token_is(&tokens, content, "return system", 0, 7, 6, "parameter", 0U) &&
         contextual_word_token_is(&tokens, content, "system = import", 0, 0, 6, "namespace", 0U) &&
         contextual_word_token_is(&tokens, content, "system.console", 0, 0, 6, "namespace", 0U);
    if (!ok) {
        TEST_FAIL(timer, summary, "a parameter shadowing an import alias was colored as the alias");
    } else {
        TEST_PASS(timer, summary);
    }
    if (tokens.isValid) {
        ZrCore_Array_Free(state, &tokens);
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_semantic_tokens_keep_keyword_positions(SZrState *state) {
    SZrTestTimer timer;
    const TZrChar *summary = "LSP Semantic Tokens Keep Contextual And Reserved Keywords";
    const TZrChar *content =
        "pub abstract class Base {\n"
        "    pub fn ping(): int { return 1; }\n"
        "}\n"
        "class Child: Base {\n"
        "    pub override fn ping(): int { return 2; }\n"
        "}\n"
        "class Holder<T> where T: Base { var value: T; }\n"
        "struct Vec2 {\n"
        "    pub var x: float;\n"
        "    pub @constructor(x: float) { this.x = x; }\n"
        "}\n"
        "var v = init Vec2(1.0);\n";
    SZrLspContext *context = ZR_NULL;
    SZrArray tokens = {0};
    TZrBool ok;

    TEST_START(summary);
    ok = contextual_word_tokens_open(state, "file:///semantic_tokens_keyword_positions.zr", content,
                                     &context, &tokens);
    ok = ok &&
         contextual_word_token_is(&tokens, content, "abstract class", 0, 0, 8, "keyword", 0U) &&
         contextual_word_token_is(&tokens, content, "override fn", 0, 0, 8, "keyword", 0U) &&
         contextual_word_token_is(&tokens, content, "where T", 0, 0, 5, "keyword", 0U) &&
         contextual_word_token_is(&tokens, content, "init Vec2", 0, 0, 4, "keyword", 0U);
    if (!ok) {
        TEST_FAIL(timer, summary, "a keyword position lost its keyword token");
    } else {
        TEST_PASS(timer, summary);
    }
    if (tokens.isValid) {
        ZrCore_Array_Free(state, &tokens);
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
