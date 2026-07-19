#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_TYPE_MISMATCH_DIAGNOSTICS_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_TYPE_MISMATCH_DIAGNOSTICS_H

#include "semantic/semantic_analyzer_internal.h"

TZrBool ZrLanguageServer_SemanticAnalyzer_ReportTypeMismatch(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange location,
        const SZrFileRange *expectedTypeLocation,
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType);
SZrFileRange ZrLanguageServer_SemanticAnalyzer_AssignmentExpectedTypeLocation(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *leftExpression);

#endif // ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_TYPE_MISMATCH_DIAGNOSTICS_H
