#include "zr_vm_language_server/semantic_analyzer.h"

#include "zr_vm_parser/canonical_type.h"

/** LSP 类型展示走 parser 规范类型格式化，调用方提供可写缓冲区及其容量。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_FormatTypeId(
    const SZrSemanticContext *semanticContext,
    TZrTypeId typeId,
    TZrChar *buffer,
    TZrSize bufferSize) {
    return ZrParser_CanonicalType_Format(semanticContext, typeId, buffer, bufferSize);
}
