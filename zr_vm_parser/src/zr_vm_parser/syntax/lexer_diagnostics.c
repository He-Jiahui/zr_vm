#include "syntax/lexer_internal.h"

/* Record diagnostics on the token; parser and analyzer own output policy. */
void ZrParser_Lexer_SyntaxError(SZrLexState *ls, const TZrChar *msg) {
    if (ls == ZR_NULL || msg == ZR_NULL) {
        return;
    }
    if (!ls->currentTokenHadError) {
        ls->currentTokenHadError = ZR_TRUE;
        ls->currentTokenErrorMessage = msg;
    }
}
