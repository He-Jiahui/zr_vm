#include "interface/lsp_canonical_symbol_display.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_query.h"

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
