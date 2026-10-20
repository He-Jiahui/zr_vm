#ifndef ZR_VM_PARSER_OWNERSHIP_INTERNAL_H
#define ZR_VM_PARSER_OWNERSHIP_INTERNAL_H

#include "parser/parser_ownership.h"
#include "parser/parser_ownership_strings.h"
#include "zr_vm_core/gc_domain.h"

typedef struct SZrParserOwnedString {
    SZrString *string;
    SZrGcRootHandle root;
} SZrParserOwnedString;

struct SZrAstOwnership {
    SZrState *state;
    TZrSize refCount;
    const SZrTokenStream *stream;
    TZrUInt64 ownerKey;
    TZrUInt64 sourceOrigin;
    SZrParserRangeEvidence *evidence;
    /* Parser entry points may lend a stack budget for the duration of the
     * cursor.  AST nodes outlive that cursor, so Lexer_Free snapshots it into
     * fallbackBudget before releasing the external pointer. */
    SZrParserComptimeBudget *externalBudget;
    SZrParserComptimeBudget fallbackBudget;
    SZrParserOwnedString *strings;
    TZrSize stringCount;
    TZrSize stringCapacity;
    TZrSize *slots;
    TZrSize slotCapacity;
    SZrParserOwnershipPinBlock *pinBlocks;
};

#endif
