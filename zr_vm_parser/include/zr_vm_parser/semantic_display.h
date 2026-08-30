#ifndef ZR_VM_PARSER_SEMANTIC_DISPLAY_H
#define ZR_VM_PARSER_SEMANTIC_DISPLAY_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/semantic.h"

/*
 * Semantic display is snapshot-scoped. It formats only registered canonical
 * TypeId/SymbolId/property-contract identities and never resolves a spelling.
 */
typedef struct SZrSemanticTypeDisplayAliasFact {
    TZrTypeId typeId;
    SZrFileRange useRange;
    SZrString *alias;
} SZrSemanticTypeDisplayAliasFact;

typedef struct SZrSemanticDocumentationFact {
    TZrSymbolId symbolId;
    SZrString *documentation;
} SZrSemanticDocumentationFact;

/*
 * Type display aliases are copied into the semantic snapshot and keyed by the
 * exact TypeId/use-site range. Canonical formatting remains TypeId-only.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticTypeDisplayAlias_Publish(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrFileRange *useRange,
        SZrString *alias);
ZR_PARSER_API SZrString *ZrParser_SemanticQuery_TypeDisplayAliasAt(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrFileRange *useRange);

/*
 * Documentation is copied into the semantic snapshot. Queries return a
 * borrowed string that remains valid only for that snapshot generation.
 */
ZR_PARSER_API TZrBool ZrParser_SemanticDocumentation_Publish(
        SZrSemanticContext *context,
        TZrSymbolId symbolId,
        SZrString *documentation);
ZR_PARSER_API SZrString *ZrParser_SemanticQuery_DocumentationOfSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId);

ZR_PARSER_API TZrBool ZrParser_SemanticDisplay_FormatType(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize);
ZR_PARSER_API TZrBool ZrParser_SemanticDisplay_FormatSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        TZrChar *buffer,
        TZrSize bufferSize);
/* Creates a snapshot-owned callable label from exact SymbolId/TypeId/AST identity. */
ZR_PARSER_API SZrString *ZrParser_SemanticDisplay_CreateCallableSignature(
        SZrSemanticContext *context,
        TZrSymbolId symbolId);
ZR_PARSER_API TZrBool ZrParser_SemanticDisplay_FormatProperty(
        const SZrSemanticContext *context,
        const SZrSemanticPropertyContract *property,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif // ZR_VM_PARSER_SEMANTIC_DISPLAY_H
