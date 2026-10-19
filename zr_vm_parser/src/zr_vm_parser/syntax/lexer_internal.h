#ifndef ZR_VM_PARSER_LEXER_INTERNAL_H
#define ZR_VM_PARSER_LEXER_INTERNAL_H

#include "zr_vm_parser/lexer.h"

void ZrParser_Lexer_InitBudget(
        SZrLexState *ls, SZrState *state, const TZrChar *source,
        TZrSize sourceLength, SZrString *sourceName,
        SZrParserComptimeBudget *budget);
void ZrParser_Lexer_InitOwnedSource(
        SZrLexState *ls, SZrState *state, const TZrChar *source,
        TZrSize sourceLength, SZrString *sourceName,
        SZrAstOwnership *owner, SZrParserComptimeBudget *budget,
        SZrSyntaxContext context, SZrTokenSpan origin,
        const SZrFileRange *originLocation);
void ZrParser_Lexer_InitTokenStreamBudget(
        SZrLexState *ls, SZrState *state, const SZrTokenStream *stream,
        SZrString *sourceName, SZrParserComptimeBudget *budget);

TZrInt32 ZrParser_Lexer_ReadChar(SZrLexState *ls);
EZrToken ZrParser_Lexer_Scan(SZrLexState *ls, TZrSemInfo *seminfo);
void ZrParser_Lexer_SetTextTokenMetadata(SZrLexState *ls);
EZrToken ZrParser_Lexer_FindKeyword(const TZrChar *name, TZrSize length);

#endif
