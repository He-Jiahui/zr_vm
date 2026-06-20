#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_UNION_PATTERNS_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_UNION_PATTERNS_H

#include "semantic/semantic_analyzer_internal.h"

typedef struct SZrSemanticUnionPatternResolution {
    SZrString *variantName;
    SZrAstNodeArray *bindings;
    SZrAstNode *variant;
    SZrInferredType resourceType;
    TZrBool hasResourceType;
} SZrSemanticUnionPatternResolution;

void ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(
        SZrState *state,
        SZrSemanticUnionPatternResolution *resolution);

void ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionFree(
        SZrState *state,
        SZrSemanticUnionPatternResolution *resolution);

TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveUsingUnionPattern(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrUsingStatement *usingStmt,
        SZrSemanticUnionPatternResolution *resolution);

TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveSwitchUnionPattern(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *caseValue,
        const SZrInferredType *subjectType,
        SZrSemanticUnionPatternResolution *resolution);

void ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticUnionPatternResolution *resolution,
        TZrBool registerSymbols,
        TZrBool registerTypeEnv);

#endif
