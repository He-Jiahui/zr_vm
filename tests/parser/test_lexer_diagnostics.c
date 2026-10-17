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
    ZrTests_Runtime_State_Destroy(state);
    return 0;
}
