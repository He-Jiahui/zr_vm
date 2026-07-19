#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_OWNERSHIP_DIAGNOSTICS_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_OWNERSHIP_DIAGNOSTICS_H

#include "semantic/semantic_analyzer_internal.h"

TZrBool ZrLanguageServer_SemanticOwnership_AddEscapeRelatedInformation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrStructuredDiagnostic *diagnostic,
        SZrAstNode *ownershipNode,
        SZrAstNode *enclosingCallable,
        EZrOwnershipQualifier qualifier);

#endif
