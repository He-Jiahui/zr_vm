#include <stdio.h>
#include <string.h>
#include "harness/runtime_support.h"
#include "zr_vm_parser/lexer.h"

/* The lexer can run before a parser installs its diagnostic sink (LSP init and
 * lookahead both do this). Errors must remain on tokens without writing to a
 * process-wide output stream. The CTest wrapper checks both streams. */
void setUp(void) {}
void tearDown(void) {}

int main(void) {
    static const char *const invalid[] = {
        "0x", "1e+", "9223372036854775808", "\"unfinished", "\"\\uqqqq\"", "'\\xgg'"
    };
    /* Input ends before the closing delimiter. A trailing star or slash alone, or
     * a slash that reuses the star of the opener, does not close the comment. */
    static const char *const unclosedComments[] = {
        "/* unfinished", "/*", "/*/", "/* a *", "/* a /", "/* a * /",
        "// line\n/* unfinished", "/* closed */ /* unfinished", "/* line\r\nbreak"
    };
    /* A block comment that closes on the last two characters is complete. */
    static const char *const closedComments[] = {
        "/* closed */", "/**/", "/***/", "/*/*/", "/* a **/", "/* a */ /* b */",
        "/* line\nbreak */", "/* line\r\nbreak */", "/* a */ // tail"
    };
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) return 1;
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        SZrLexState lexer;
        ZrParser_Lexer_Init(&lexer, state, invalid[i], strlen(invalid[i]), ZR_NULL);
        if (!lexer.t.hasLexError || lexer.t.lexErrorMessage == ZR_NULL) {
            fprintf(stderr, "Missing lexical diagnostic for case %u\n", i);
            return 1;
        }
        ZrParser_Lexer_Free(&lexer);
    }
    {
        const char *source = "0 \"unfinished";
        SZrLexState lexer;
        ZrParser_Lexer_Init(&lexer, state, source, strlen(source), ZR_NULL);
        if (lexer.t.hasLexError) return 1;
        ZrParser_Lexer_Lookahead(&lexer);
        if (lexer.t.hasLexError || !lexer.lookahead.hasLexError) return 1;
        ZrParser_Lexer_Next(&lexer);
        if (!lexer.t.hasLexError || lexer.t.lexErrorMessage == ZR_NULL) return 1;
        ZrParser_Lexer_Free(&lexer);
    }
    for (unsigned i = 0; i < sizeof(unclosedComments) / sizeof(unclosedComments[0]); ++i) {
        SZrLexState lexer;
        ZrParser_Lexer_Init(&lexer, state, unclosedComments[i], strlen(unclosedComments[i]), ZR_NULL);
        /* The comment swallows the rest of the input, so the error rides on EOS. */
        if (lexer.t.token != ZR_TK_EOS || !lexer.t.hasLexError || lexer.t.lexErrorMessage == ZR_NULL ||
            strcmp(lexer.t.lexErrorMessage, "unfinished block comment") != 0) {
            fprintf(stderr, "Missing unfinished block comment diagnostic for case %u\n", i);
            return 1;
        }
        ZrParser_Lexer_Free(&lexer);
    }
    for (unsigned i = 0; i < sizeof(closedComments) / sizeof(closedComments[0]); ++i) {
        SZrLexState lexer;
        ZrParser_Lexer_Init(&lexer, state, closedComments[i], strlen(closedComments[i]), ZR_NULL);
        if (lexer.t.token != ZR_TK_EOS || lexer.t.hasLexError || lexer.t.lexErrorMessage != ZR_NULL) {
            fprintf(stderr, "Unexpected lexical diagnostic for closed comment %u\n", i);
            return 1;
        }
        ZrParser_Lexer_Free(&lexer);
    }
    {
        /* The unclosed comment follows a valid token, so the error rides on EOS. */
        const char *source = "0 /* unfinished";
        SZrLexState lexer;
        ZrParser_Lexer_Init(&lexer, state, source, strlen(source), ZR_NULL);
        if (lexer.t.token != ZR_TK_INTEGER || lexer.t.hasLexError) return 1;
        if (ZrParser_Lexer_Lookahead(&lexer) != ZR_TK_EOS) return 1;
        if (lexer.t.hasLexError || !lexer.lookahead.hasLexError) return 1;
        ZrParser_Lexer_Next(&lexer);
        if (lexer.t.token != ZR_TK_EOS || !lexer.t.hasLexError ||
            lexer.t.lexErrorMessage == ZR_NULL ||
            strcmp(lexer.t.lexErrorMessage, "unfinished block comment") != 0) return 1;
        /* Reading past the end must not trade the failed end token for a clean one:
         * parser recovery does exactly that before the entry point inspects EOS. */
        ZrParser_Lexer_Next(&lexer);
        if (lexer.t.token != ZR_TK_EOS || !lexer.t.hasLexError) {
            fprintf(stderr, "Unfinished block comment diagnostic lost after reading past EOS\n");
            return 1;
        }
        if (ZrParser_Lexer_Lookahead(&lexer) != ZR_TK_EOS) return 1;
        ZrParser_Lexer_Next(&lexer);
        if (lexer.t.token != ZR_TK_EOS || !lexer.t.hasLexError ||
            lexer.t.lexErrorMessage == ZR_NULL ||
            strcmp(lexer.t.lexErrorMessage, "unfinished block comment") != 0) {
            fprintf(stderr, "Unfinished block comment diagnostic lost after lookahead at EOS\n");
            return 1;
        }
        ZrParser_Lexer_Free(&lexer);
    }
    {
        const char *source = "0 /* closed */";
        SZrLexState lexer;
        ZrParser_Lexer_Init(&lexer, state, source, strlen(source), ZR_NULL);
        if (lexer.t.token != ZR_TK_INTEGER || lexer.t.hasLexError) return 1;
        ZrParser_Lexer_Next(&lexer);
        if (lexer.t.token != ZR_TK_EOS || lexer.t.hasLexError) return 1;
        ZrParser_Lexer_Next(&lexer);
        if (lexer.t.token != ZR_TK_EOS || lexer.t.hasLexError) return 1;
        ZrParser_Lexer_Free(&lexer);
    }
    ZrTests_Runtime_State_Destroy(state);
    return 0;
}
