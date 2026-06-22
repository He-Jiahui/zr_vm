#ifndef ZR_VM_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_TEXT_H

#include "zr_vm_parser/semantic_facts.h"

TZrBool ZrLanguageServer_LspNumericRangeText_AppendRange(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticNumericFact *fact,
    TZrBool includeRangeLabel);

#endif
