#ifndef ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_EXPRESSION_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_EXPRESSION_TEXT_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_parser/semantic_facts.h"

TZrBool ZrLanguageServer_LspLocalSemanticExpressionText_AppendHover(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticExpressionFact *fact);

#endif
