#ifndef ZR_VM_PARSER_SYNTAX_CONTEXT_INTERNAL_H
#define ZR_VM_PARSER_SYNTAX_CONTEXT_INTERNAL_H

#include "zr_vm_parser/syntax_context.h"

#include <stdint.h>

#include "zr_vm_parser/comptime_contract.h"

typedef struct SZrSyntaxMarkRecord {
    TZrUInt32 parentMarkId;
    TZrUInt32 markerId;
    EZrSyntaxTransparency transparency;
    /* Representatives only refer to this record or an earlier publication. */
    TZrUInt32 representativeMarkId;
    TZrUInt64 rollingHashPrefix;
} SZrSyntaxMarkRecord;

typedef struct SZrSyntaxExpansionRecord {
    SZrSyntaxExpansionInfo info;
    TZrChar *definitionModule;
    TZrChar *callSiteSource;
    TZrChar *definitionSiteSource;
} SZrSyntaxExpansionRecord;

struct SZrSyntaxContextTable {
    SZrGlobalState *global;
    TZrUInt32 refCount;
    SZrParserComptimeBudget *budget;
    SZrParserComptimeBudget fallbackBudget;
    SZrSyntaxMarkRecord *marks;
    TZrSize markCount;
    TZrSize markCapacity;
    SZrSyntaxExpansionRecord *expansions;
    TZrSize expansionCount;
    TZrSize expansionCapacity;
    /* Import/normalization operations publish one budget transaction.  A
     * growth performed while it is active defers the old buffer so failure
     * can restore the exact pre-operation arrays. */
    SZrParserComptimeBudgetTransaction *activeTransaction;
    SZrSyntaxMarkRecord *deferredMarks;
    TZrSize deferredMarkCapacity;
    SZrSyntaxExpansionRecord *deferredExpansions;
    TZrSize deferredExpansionCapacity;
};

/* Private guard used by token-tree batch writers.  Public callers continue to
 * use the individual operations; a batch lets several context imports and
 * mark publications commit or abort as one unit. */
typedef struct SZrSyntaxContextBatch {
    SZrSyntaxContextTable *table;
    SZrParserComptimeBudgetTransaction transaction;
    SZrSyntaxMarkRecord *marks;
    TZrSize markCapacity;
    SZrSyntaxExpansionRecord *expansions;
    TZrSize expansionCapacity;
    TZrSize markCount;
    TZrSize expansionCount;
    TZrBool active;
    TZrBool prepared;
} SZrSyntaxContextBatch;

void ZrParser_SyntaxContext_DefaultBudget(
        SZrParserComptimeBudget *budget);
SZrParserComptimeBudget *ZrParser_SyntaxContext_Budget(
        SZrSyntaxContextTable *table);
TZrBool ZrParser_SyntaxContext_SizeAdd(
        TZrSize left, TZrSize right, TZrSize *out);
TZrBool ZrParser_SyntaxContext_SizeMul(
        TZrSize left, TZrSize right, TZrSize *out);
TZrBool ZrParser_SyntaxContext_TransparencyIsValid(
        EZrSyntaxTransparency transparency);
TZrBool ZrParser_SyntaxContext_NamespaceIsValid(
        EZrSyntaxNamespace namespaceKind);
SZrSyntaxContext ZrParser_SyntaxContext_Invalid(
        SZrSyntaxContextTable *table);
TZrBool ZrParser_SyntaxContext_ValidateMarkChain(
        const SZrSyntaxContextTable *table, TZrUInt32 markId);
TZrBool ZrParser_SyntaxContext_FindMark(
        const SZrSyntaxContextTable *table,
        TZrUInt32 parentMarkId,
        TZrUInt32 markerId,
        EZrSyntaxTransparency transparency,
        TZrUInt32 *outMarkId);
TZrBool ZrParser_SyntaxContext_ExpansionInfoEqual(
        const SZrSyntaxExpansionInfo *left,
        const SZrSyntaxExpansionInfo *right);
/* Initialize the next unpublished slot after its parent has been published. */
void ZrParser_SyntaxContext_InitializeMark(
        SZrSyntaxContextTable *table,
        SZrSyntaxMarkRecord *record,
        TZrUInt32 parentMarkId,
        TZrUInt32 markerId,
        EZrSyntaxTransparency transparency);
TZrBool ZrParser_SyntaxContext_EnsureMarks(
        SZrSyntaxContextTable *table,
        TZrSize required,
        TZrSize *outCapacityBytes);
TZrBool ZrParser_SyntaxContext_EnsureExpansions(
        SZrSyntaxContextTable *table,
        TZrSize required,
        TZrSize *outCapacityBytes);
TZrChar *ZrParser_SyntaxContext_CopyBytes(
        SZrSyntaxContextTable *table,
        const TZrChar *bytes,
        TZrSize length);
void ZrParser_SyntaxContext_FreeExpansionRecord(
        SZrSyntaxContextTable *table,
        SZrSyntaxExpansionRecord *record);
void ZrParser_SyntaxContext_CommitDeferredBuffers(
        SZrSyntaxContextTable *table);
void ZrParser_SyntaxContext_RollbackDeferredBuffers(
        SZrSyntaxContextTable *table,
        SZrSyntaxMarkRecord *originalMarks,
        TZrSize originalMarkCapacity,
        SZrSyntaxExpansionRecord *originalExpansions,
        TZrSize originalExpansionCapacity);
TZrBool ZrParser_SyntaxContext_BeginBatch(
        SZrSyntaxContextTable *table,
        SZrSyntaxContextBatch *batch);
TZrBool ZrParser_SyntaxContext_CommitBatch(
        SZrSyntaxContextBatch *batch);
TZrBool ZrParser_SyntaxContext_PrepareBatch(
        SZrSyntaxContextBatch *batch);
TZrBool ZrParser_SyntaxContext_PublishBatch(
        SZrSyntaxContextBatch *batch);
void ZrParser_SyntaxContext_AbortBatch(
        SZrSyntaxContextBatch *batch);

#endif
