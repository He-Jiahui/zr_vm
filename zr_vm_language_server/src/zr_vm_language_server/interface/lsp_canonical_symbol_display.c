#include "interface/lsp_canonical_symbol_display.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

TZrBool ZrLanguageServer_Lsp_FormatCanonicalDeclarationType(
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *declaration,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrSemanticReferenceFact *resolved;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        declaration == ZR_NULL || buffer == ZR_NULL || bufferSize == 0U ||
        declaration->symbolId == ZR_SEMANTIC_ID_INVALID ||
        declaration->typeId == ZR_SEMANTIC_ID_INVALID ||
        declaration->role != ZR_SEMANTIC_REFERENCE_DECLARATION) {
        return ZR_FALSE;
    }

    resolved = ZrParser_SemanticQuery_DeclarationOf(
            analyzer->semanticContext, declaration->symbolId, ZR_NULL);
    return resolved != ZR_NULL && resolved->isResolved &&
           resolved->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
           resolved->symbolId == declaration->symbolId &&
           resolved->typeId == declaration->typeId &&
           resolved->node == declaration->declarationNode &&
           ZrParser_CanonicalType_Format(
                   analyzer->semanticContext,
                   declaration->typeId,
                   buffer,
                   bufferSize);
}

TZrBool ZrLanguageServer_Lsp_FormatSymbolCanonicalDeclarationType(
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *symbol,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrSemanticReferenceFact *declaration;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        symbol == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u ||
        symbol->semanticId == ZR_SEMANTIC_ID_INVALID ||
        symbol->semanticTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    declaration = ZrParser_SemanticQuery_DeclarationOf(
            analyzer->semanticContext, symbol->semanticId, ZR_NULL);
    return declaration != ZR_NULL &&
           declaration->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
           declaration->isResolved &&
           declaration->symbolId == symbol->semanticId &&
           declaration->typeId == symbol->semanticTypeId &&
           ZrParser_CanonicalType_Format(
                   analyzer->semanticContext, declaration->typeId, buffer, bufferSize);
}

TZrBool ZrLanguageServer_Lsp_FormatExactExpressionType(
        SZrSemanticAnalyzer *analyzer,
        const SZrAstNode *expression,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrSemanticExpressionFact *fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        expression == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u) {
        return ZR_FALSE;
    }

    fact = ZrParser_SemanticFacts_FindExpressionByNode(
            analyzer->semanticContext, expression);
    return fact != ZR_NULL && fact->exactness == ZR_SEMANTIC_FACT_EXACT &&
           fact->typeId != ZR_SEMANTIC_ID_INVALID &&
           ZrParser_CanonicalType_Format(
                   analyzer->semanticContext, fact->typeId, buffer, bufferSize);
}
