#ifndef ZR_VM_PARSER_TOKEN_SOURCE_H
#define ZR_VM_PARSER_TOKEN_SOURCE_H

#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/token_stream.h"

/* The text lexer remains the canonical source scanner. This adapter only
 * owns the cursor state needed to walk an already-tokenized native stream. */
typedef struct SZrTokenSourceCheckpoint {
    TZrBool isStream;
    TZrSize streamIndex;
    TZrBool hasPending;
    SZrToken pending;
    TZrBool hasLookahead;
    SZrToken lookahead;
    /* Shared text-lexer state is captured even when isStream is false. */
    TZrBool hasSplitPending;
    SZrToken splitPending;
} SZrTokenSourceCheckpoint;

void ZrParser_TokenSource_InitStream(
        SZrLexState *lexer,
        const SZrTokenStream *stream);
void ZrParser_TokenSource_FreeStream(SZrLexState *lexer);
TZrBool ZrParser_TokenSource_IsStream(const SZrLexState *lexer);
/* Borrow an owner-held name; native ordinals and evidence survive cursor free. */
TZrBool ZrParser_TokenSource_SourceName(
        const SZrLexState *lexer, TZrTokenSourceId sourceId, SZrString **outName);
TZrBool ZrParser_TokenSource_Next(
        SZrLexState *lexer,
        SZrToken *outToken);
TZrBool ZrParser_TokenSource_Lookahead(
        SZrLexState *lexer,
        SZrToken *outToken);
TZrBool ZrParser_TokenSource_SplitCurrentPunct(
        SZrLexState *lexer,
        TZrChar firstPunct);
void ZrParser_TokenSource_Save(
        const SZrLexState *lexer,
        SZrTokenSourceCheckpoint *checkpoint);
void ZrParser_TokenSource_Restore(
        SZrLexState *lexer,
        const SZrTokenSourceCheckpoint *checkpoint);

#endif
