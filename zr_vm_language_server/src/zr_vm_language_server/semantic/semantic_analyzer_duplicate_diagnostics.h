#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_DUPLICATE_DIAGNOSTICS_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_DUPLICATE_DIAGNOSTICS_H

#include "semantic/semantic_analyzer_internal.h"

TZrBool ZrLanguageServer_SemanticAnalyzer_ReportDuplicateType(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrString *name,
        SZrFileRange location,
        const SZrFileRange *previousLocation);

#endif // ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_DUPLICATE_DIAGNOSTICS_H
