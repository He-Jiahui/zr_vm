#ifndef ZR_VM_PARSER_OWNERSHIP_STRINGS_H
#define ZR_VM_PARSER_OWNERSHIP_STRINGS_H

#include "parser/parser_ownership.h"

typedef struct SZrParserOwnershipPinBlock SZrParserOwnershipPinBlock;

/* Inputs remain live and unchanged throughout Prepare. NULL strings are skipped.
 * The transaction must be active against this live owner's current budget.
 * Prepare never commits or aborts it. FALSE releases its own pins and leaves
 * *outStage NULL; the caller must abort its transaction. TRUE may also return
 * NULL when there are no new strings. outStage must be an independent empty slot. */
TZrBool ZrParser_Ownership_PrepareStrings(
        SZrAstOwnership *owner, SZrString *const *strings, TZrSize count,
        SZrParserComptimeBudgetTransaction *transaction,
        SZrParserOwnershipPinBlock **outStage);
/* After a successful transaction commit, Apply transfers this same-owner stage.
 * It allocates nothing. Do not apply or discard the transferred stage again. */
void ZrParser_Ownership_ApplyStrings(
        SZrAstOwnership *owner, SZrParserOwnershipPinBlock *stage);
void ZrParser_Ownership_DiscardStrings(
        SZrAstOwnership *owner, SZrParserOwnershipPinBlock *stage);
void ZrParser_Ownership_ReleaseStrings(SZrAstOwnership *owner);

#endif
