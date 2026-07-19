#include "zr_vm_language_server/semantic_analyzer.h"

#include "zr_vm_parser/canonical_type.h"

TZrBool ZrLanguageServer_SemanticAnalyzer_FormatTypeId(
    const SZrSemanticContext *semanticContext,
    TZrTypeId typeId,
    TZrChar *buffer,
    TZrSize bufferSize) {
    return ZrParser_CanonicalType_Format(semanticContext, typeId, buffer, bufferSize);
}
