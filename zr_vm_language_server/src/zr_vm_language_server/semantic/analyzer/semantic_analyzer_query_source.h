#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_QUERY_SOURCE_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_QUERY_SOURCE_H

#include "zr_vm_language_server/semantic_analyzer.h"

/** 仅补全缺失的 source；显式请求的来源保持原样。 */
SZrFileRange ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
        const SZrSemanticAnalyzer *analyzer,
        SZrFileRange position);

#endif
