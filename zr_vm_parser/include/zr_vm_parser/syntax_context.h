#ifndef ZR_VM_PARSER_SYNTAX_CONTEXT_H
#define ZR_VM_PARSER_SYNTAX_CONTEXT_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/token_span.h"

typedef struct SZrGlobalState SZrGlobalState;
typedef struct SZrParserComptimeBudget SZrParserComptimeBudget;
typedef struct SZrSyntaxContextTable SZrSyntaxContextTable;
typedef TZrUInt32 TZrSyntaxExpansionId;

/* Borrowed context keys require a retained table owner on their enclosing tree. */
typedef struct SZrSyntaxContext {
    SZrSyntaxContextTable *table;
    TZrUInt32 markId;
} SZrSyntaxContext;

/* A failed context operation is table-owned and never aliases a valid root or
 * parent mark.  The value is deliberately outside the append-only mark ID
 * range (which stops before UINT32_MAX). */
#define ZR_SYNTAX_CONTEXT_INVALID_MARK_ID ((TZrUInt32)-1)

typedef enum EZrSyntaxTransparency {
    ZR_SYNTAX_TRANSPARENT = 0,
    ZR_SYNTAX_SEMI_OPAQUE = 1,
    ZR_SYNTAX_OPAQUE = 2
} EZrSyntaxTransparency;

typedef enum EZrSyntaxNamespace {
    ZR_SYNTAX_NAMESPACE_LOCAL = 0,
    ZR_SYNTAX_NAMESPACE_ITEM = 1
} EZrSyntaxNamespace;

/* Registration copies strings. The stable identity includes the locked
 * provider identity, definition and invocation digests, and source ordinal. */
typedef struct SZrSyntaxExpansionInfo {
    TZrSyntaxExpansionId parentExpansionId;
    TZrUInt64 stableIdentity;
    const TZrChar *definitionModule;
    TZrSize definitionModuleLength;
    const TZrChar *callSiteSource;
    TZrSize callSiteSourceLength;
    const TZrChar *definitionSiteSource;
    TZrSize definitionSiteSourceLength;
    SZrTokenSpan callSiteSpan;
    SZrTokenSpan definitionSiteSpan;
    SZrSyntaxContext callSiteContext;
    SZrSyntaxContext definitionSiteContext;
} SZrSyntaxExpansionInfo;

ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_Root(void);
ZR_PARSER_API SZrSyntaxContextTable *ZrParser_SyntaxContextTable_Create(
        SZrGlobalState *global,
        SZrParserComptimeBudget *budget);
ZR_PARSER_API void ZrParser_SyntaxContextTable_Retain(
        SZrSyntaxContextTable *table);
ZR_PARSER_API void ZrParser_SyntaxContextTable_Release(
        SZrSyntaxContextTable *table);
ZR_PARSER_API TZrBool ZrParser_SyntaxContextTable_RegisterExpansion(
        SZrSyntaxContextTable *table,
        const SZrSyntaxExpansionInfo *info,
        TZrSyntaxExpansionId *outExpansionId);
ZR_PARSER_API const SZrSyntaxExpansionInfo *ZrParser_SyntaxContextTable_Expansion(
        const SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId);
ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_TableRoot(
        SZrSyntaxContextTable *table);
ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_CallSite(
        SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId);
ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_DefSite(
        SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId);
ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_MixedSite(
        SZrSyntaxContextTable *table,
        TZrSyntaxExpansionId expansionId);
ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_ApplyMark(
        SZrSyntaxContext parent,
        TZrUInt32 markId,
        EZrSyntaxTransparency transparency);
ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_ResolveAt(
        SZrSyntaxContext context,
        SZrSyntaxContext target);
ZR_PARSER_API SZrSyntaxContext ZrParser_SyntaxContext_Normalize(
        SZrSyntaxContext context,
        EZrSyntaxNamespace namespaceKind);
ZR_PARSER_API TZrUInt64 ZrParser_SyntaxContext_StableHash(SZrSyntaxContext context);
ZR_PARSER_API TZrBool ZrParser_SyntaxContext_IsValid(
        SZrSyntaxContext context);
ZR_PARSER_API TZrBool ZrParser_SyntaxContext_IsSameTable(
        SZrSyntaxContext left,
        SZrSyntaxContext right);
ZR_PARSER_API TZrBool ZrParser_SyntaxContext_Import(
        SZrSyntaxContextTable *destination,
        SZrSyntaxContext source,
        SZrSyntaxContext *outContext);
/* Nonroot equality is table-sensitive. Roots compare across owners;
 * StableHash, unlike local IDs, preserves identity across import. */
ZR_PARSER_API TZrBool ZrParser_SyntaxContext_Equal(
        SZrSyntaxContext left, SZrSyntaxContext right);

#endif
