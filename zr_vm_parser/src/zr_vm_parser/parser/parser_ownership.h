#ifndef ZR_VM_PARSER_OWNERSHIP_H
#define ZR_VM_PARSER_OWNERSHIP_H

#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_core/memory.h"

typedef struct SZrAstOwnership SZrAstOwnership;
typedef struct SZrParserRangeEvidence SZrParserRangeEvidence;

SZrAstOwnership *ZrParser_Ownership_Create(
        SZrState *state,
        const SZrTokenStream *stream,
        SZrParserComptimeBudget *budget);
void ZrParser_Ownership_Retain(SZrAstOwnership *owner);
void ZrParser_Ownership_Release(SZrAstOwnership *owner);
TZrUInt64 ZrParser_Ownership_Key(const SZrAstOwnership *owner);
TZrUInt64 ZrParser_Ownership_SourceOrigin(const SZrAstOwnership *owner);
SZrParserRangeEvidence *ZrParser_Ownership_Evidence(const SZrAstOwnership *owner);
TZrBool ZrParser_Ownership_PublishEvidence(
        SZrAstOwnership *owner, SZrParserRangeEvidence *evidence);
/* Return the live budget while the parser is active, or the owner's detached
 * snapshot after Lexer_Free.  The returned pointer is borrowed. */
SZrParserComptimeBudget *ZrParser_Ownership_Budget(
        SZrAstOwnership *owner);
/* Detach parser-owned allocations from a caller's stack budget.  The current
 * value, including usage and exceeded metadata, is copied before detaching. */
void ZrParser_Ownership_DetachBudget(SZrAstOwnership *owner);
TZrBool ZrParser_Ownership_HoldString(SZrAstOwnership *owner, SZrString *string);
void *ZrParser_Ownership_Allocate(
        SZrAstOwnership *owner, TZrSize bytes, EZrMemoryNativeType type,
        TZrBool astNode);
SZrString *ZrParser_Lexer_CreateString(
        SZrLexState *lexer, const TZrChar *bytes, TZrSize length);
TZrBool ZrParser_Lexer_HoldString(SZrLexState *lexer, SZrString *string);

#endif
