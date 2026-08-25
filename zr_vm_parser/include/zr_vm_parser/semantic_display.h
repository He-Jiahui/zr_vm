#ifndef ZR_VM_PARSER_SEMANTIC_DISPLAY_H
#define ZR_VM_PARSER_SEMANTIC_DISPLAY_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/semantic.h"

/*
 * Semantic display is snapshot-scoped. It formats only registered canonical
 * TypeId/SymbolId/property-contract identities and never resolves a spelling.
 */
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
ZR_PARSER_API TZrBool ZrParser_SemanticDisplay_FormatProperty(
        const SZrSemanticContext *context,
        const SZrSemanticPropertyContract *property,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif // ZR_VM_PARSER_SEMANTIC_DISPLAY_H
