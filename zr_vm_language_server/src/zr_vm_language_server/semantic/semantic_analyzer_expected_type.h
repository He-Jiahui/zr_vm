#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_EXPECTED_TYPE_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_EXPECTED_TYPE_H

#include "semantic/semantic_analyzer_internal.h"

SZrFileRange ZrLanguageServer_SemanticAnalyzer_AssignmentExpectedTypeLocation(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *leftExpression);

#endif // ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_EXPECTED_TYPE_H
