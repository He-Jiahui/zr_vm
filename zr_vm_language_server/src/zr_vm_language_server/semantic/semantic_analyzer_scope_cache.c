#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_language_server/incremental_parser.h"

static TZrSize scoped_cache_range_length(const SZrFileRange *range) {
    return range != ZR_NULL && range->end.offset >= range->start.offset
                   ? range->end.offset - range->start.offset
                   : 0;
}

static TZrBool scoped_cache_ranges_overlap(const SZrFileRange *left,
                                           const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset < right->end.offset &&
           right->start.offset < left->end.offset;
}

static TZrBool scoped_cache_ranges_equal(const SZrFileRange *left,
                                         const SZrFileRange *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           left->start.offset == right->start.offset &&
           left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.offset == right->end.offset &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static TZrBool scoped_cache_range_contains(const SZrFileRange *container,
                                           const SZrFileRange *range) {
    return container != ZR_NULL && range != ZR_NULL &&
           range->start.offset >= container->start.offset &&
           range->end.offset <= container->end.offset;
}

static TZrBool scoped_cache_is_supported_callable_type(EZrAstNodeType type) {
    return type == ZR_AST_FUNCTION_DECLARATION ||
           type == ZR_AST_CLASS_METHOD ||
           type == ZR_AST_STRUCT_METHOD;
}

static SZrAstNode *scoped_cache_changed_callable(
        SZrAstNode *currentAst,
        const SZrFileChangeInfo *changeInfo) {
    SZrAstNode *changedRoot;

    if (currentAst == ZR_NULL || changeInfo == ZR_NULL ||
        !changeInfo->hasDeclaration ||
        !scoped_cache_is_supported_callable_type(changeInfo->declarationType)) {
        return ZR_NULL;
    }
    changedRoot = ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
            currentAst,
            changeInfo->oldRange);
    return changedRoot != ZR_NULL &&
                   changedRoot->type == changeInfo->declarationType
               ? changedRoot
               : ZR_NULL;
}

static const SZrFileRange *scoped_cache_callable_declaration_range(
        const SZrAstNode *callable) {
    if (callable == ZR_NULL) {
        return ZR_NULL;
    }
    if (callable->type == ZR_AST_FUNCTION_DECLARATION) {
        return &callable->data.functionDeclaration.nameLocation;
    }
    return callable->type == ZR_AST_CLASS_METHOD ||
                           callable->type == ZR_AST_STRUCT_METHOD
                   ? &callable->location
                   : ZR_NULL;
}

static const SZrType *scoped_cache_callable_return_type(
        const SZrAstNode *callable) {
    if (callable == ZR_NULL) {
        return ZR_NULL;
    }
    switch (callable->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return callable->data.functionDeclaration.returnType;
        case ZR_AST_CLASS_METHOD:
            return callable->data.classMethod.returnType;
        case ZR_AST_STRUCT_METHOD:
            return callable->data.structMethod.returnType;
        default:
            return ZR_NULL;
    }
}

typedef enum EZrScopedCacheDependency {
    ZR_SCOPED_CACHE_DEPENDENCY_UNKNOWN = 0,
    ZR_SCOPED_CACHE_DEPENDENCY_NONE,
    ZR_SCOPED_CACHE_DEPENDENCY_DIRECT
} EZrScopedCacheDependency;

typedef enum EZrScopedCacheChangeDecision {
    ZR_SCOPED_CACHE_CHANGE_INVALIDATE_LOCAL = 0,
    ZR_SCOPED_CACHE_CHANGE_PRESERVE,
    ZR_SCOPED_CACHE_CHANGE_INVALIDATE_DIRECT_DEPENDENCY,
    ZR_SCOPED_CACHE_CHANGE_INVALIDATE_CONSERVATIVE
} EZrScopedCacheChangeDecision;

static EZrScopedCacheDependency scoped_cache_dependency_to_callable(
        const SZrSemanticAnalyzer *scopedAnalyzer,
        const SZrAstNode *changedCallable) {
    const SZrFileRange *changedDeclarationRange;
    TZrSize index;

    if (scopedAnalyzer == ZR_NULL || scopedAnalyzer->cache == ZR_NULL ||
        scopedAnalyzer->semanticContext == ZR_NULL ||
        !scopedAnalyzer->semanticContext->referenceFacts.isValid ||
        !scopedAnalyzer->semanticContext->expressionFacts.isValid ||
        !scopedAnalyzer->semanticContext->queryDiagnostics.isValid ||
        !scopedAnalyzer->diagnostics.isValid ||
        scopedAnalyzer->semanticContext->queryDiagnostics.length > 0 ||
        scopedAnalyzer->diagnostics.length > 0 ||
        changedCallable == ZR_NULL ||
        !scoped_cache_is_supported_callable_type(changedCallable->type)) {
        return ZR_SCOPED_CACHE_DEPENDENCY_UNKNOWN;
    }

    changedDeclarationRange =
            scoped_cache_callable_declaration_range(changedCallable);
    if (changedDeclarationRange == ZR_NULL) {
        return ZR_SCOPED_CACHE_DEPENDENCY_UNKNOWN;
    }
    for (index = 0;
         index < scopedAnalyzer->semanticContext->referenceFacts.length;
         index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &scopedAnalyzer->semanticContext->referenceFacts,
                        index);
        if (fact == ZR_NULL) {
            return ZR_SCOPED_CACHE_DEPENDENCY_UNKNOWN;
        }
        if (!scoped_cache_range_contains(
                    &scopedAnalyzer->cache->cacheRange,
                    &fact->range)) {
            continue;
        }
        if (!fact->isResolved && fact->kind == ZR_SEMANTIC_REFERENCE_CALL) {
            return ZR_SCOPED_CACHE_DEPENDENCY_UNKNOWN;
        }
        if (fact->isResolved && scoped_cache_ranges_equal(
                    &fact->declarationRange,
                    changedDeclarationRange)) {
            return ZR_SCOPED_CACHE_DEPENDENCY_DIRECT;
        }
    }

    for (index = 0;
         index < scopedAnalyzer->semanticContext->expressionFacts.length;
         index++) {
        const SZrSemanticExpressionFact *expressionFact =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(
                        &scopedAnalyzer->semanticContext->expressionFacts,
                        index);
        TZrBool hasResolvedReference = ZR_FALSE;
        TZrSize referenceIndex;

        if (expressionFact == ZR_NULL) {
            return ZR_SCOPED_CACHE_DEPENDENCY_UNKNOWN;
        }
        if (!scoped_cache_range_contains(
                    &scopedAnalyzer->cache->cacheRange,
                    &expressionFact->range) ||
            expressionFact->node == ZR_NULL ||
            expressionFact->node->type != ZR_AST_IDENTIFIER_LITERAL ||
            expressionFact->inferredType.baseType != ZR_VALUE_TYPE_CLOSURE) {
            continue;
        }

        for (referenceIndex = 0;
             referenceIndex <
                     scopedAnalyzer->semanticContext->referenceFacts.length;
             referenceIndex++) {
            const SZrSemanticReferenceFact *referenceFact =
                    (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            &scopedAnalyzer->semanticContext->referenceFacts,
                            referenceIndex);
            if (referenceFact != ZR_NULL && referenceFact->isResolved &&
                scoped_cache_ranges_equal(
                        &referenceFact->range,
                        &expressionFact->range)) {
                hasResolvedReference = ZR_TRUE;
                break;
            }
        }
        if (!hasResolvedReference) {
            return ZR_SCOPED_CACHE_DEPENDENCY_UNKNOWN;
        }
    }
    return ZR_SCOPED_CACHE_DEPENDENCY_NONE;
}

static EZrScopedCacheChangeDecision scoped_cache_dependency_decision(
        EZrScopedCacheDependency dependency) {
    if (dependency == ZR_SCOPED_CACHE_DEPENDENCY_NONE) {
        return ZR_SCOPED_CACHE_CHANGE_PRESERVE;
    }
    return dependency == ZR_SCOPED_CACHE_DEPENDENCY_DIRECT
                   ? ZR_SCOPED_CACHE_CHANGE_INVALIDATE_DIRECT_DEPENDENCY
                   : ZR_SCOPED_CACHE_CHANGE_INVALIDATE_CONSERVATIVE;
}

static EZrScopedCacheChangeDecision scoped_cache_change_decision(
        const SZrSemanticAnalyzer *scopedAnalyzer,
        SZrAstNode *currentAst,
        const SZrFileChangeInfo *changeInfo) {
    SZrAstNode *changedCallable;

    if (scopedAnalyzer == ZR_NULL || changeInfo == ZR_NULL ||
        !scopedAnalyzer->enableCache || scopedAnalyzer->cache == ZR_NULL ||
        !scopedAnalyzer->cache->isValid ||
        scoped_cache_range_length(&changeInfo->oldRange) !=
                scoped_cache_range_length(&changeInfo->newRange) ||
        scoped_cache_ranges_overlap(
                &scopedAnalyzer->cache->cacheRange,
                &changeInfo->declarationRange)) {
        return ZR_SCOPED_CACHE_CHANGE_INVALIDATE_LOCAL;
    }
    if (!changeInfo->hasDeclaration || currentAst == ZR_NULL) {
        return ZR_SCOPED_CACHE_CHANGE_INVALIDATE_CONSERVATIVE;
    }

    if (changeInfo->impact == ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY) {
        changedCallable = scoped_cache_changed_callable(currentAst, changeInfo);
        if (changedCallable == ZR_NULL) {
            return ZR_SCOPED_CACHE_CHANGE_INVALIDATE_CONSERVATIVE;
        }
        if (scoped_cache_callable_return_type(changedCallable) != ZR_NULL) {
            return ZR_SCOPED_CACHE_CHANGE_PRESERVE;
        }
        return scoped_cache_dependency_decision(
                scoped_cache_dependency_to_callable(
                        scopedAnalyzer,
                        changedCallable));
    }

    if (changeInfo->impact != ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE) {
        return ZR_SCOPED_CACHE_CHANGE_INVALIDATE_CONSERVATIVE;
    }
    changedCallable = scoped_cache_changed_callable(currentAst, changeInfo);
    if (changedCallable == ZR_NULL) {
        return ZR_SCOPED_CACHE_CHANGE_INVALIDATE_CONSERVATIVE;
    }
    return scoped_cache_dependency_decision(
            scoped_cache_dependency_to_callable(
                    scopedAnalyzer,
                    changedCallable));
}

SZrSemanticAnalyzer *
ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    if (analyzer->scopedQueryAnalyzer == ZR_NULL) {
        analyzer->scopedQueryAnalyzer =
                ZrLanguageServer_SemanticAnalyzer_New(state);
        if (analyzer->scopedQueryAnalyzer != ZR_NULL) {
            analyzer->scopedQueryAnalyzer->externalProviderGeneration =
                    analyzer->externalProviderGeneration;
            ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(
                    analyzer->scopedQueryAnalyzer,
                    analyzer->enableCache);
        }
    }

    return analyzer->scopedQueryAnalyzer;
}

void ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    SZrSemanticAnalyzer *scopedQueryAnalyzer;

    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    analyzer->preserveScopedQueryAnalyzerOnNextAstChange = ZR_FALSE;
    if (analyzer->scopedQueryAnalyzer == ZR_NULL) {
        return;
    }
    scopedQueryAnalyzer = analyzer->scopedQueryAnalyzer;
    analyzer->scopedQueryAnalyzer = ZR_NULL;
    ZrLanguageServer_SemanticAnalyzer_Free(state, scopedQueryAnalyzer);
}

TZrBool ZrLanguageServer_SemanticAnalyzer_PrepareScopedQueryCacheForChange(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *currentAst,
        const SZrFileChangeInfo *changeInfo,
        TZrBool *retainCurrentAst) {
    SZrSemanticAnalyzer *scopedAnalyzer;
    EZrScopedCacheChangeDecision decision;
    TZrBool canPreserve;

    if (retainCurrentAst != ZR_NULL) {
        *retainCurrentAst = ZR_FALSE;
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || changeInfo == ZR_NULL ||
        retainCurrentAst == ZR_NULL || analyzer->scopedQueryAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    scopedAnalyzer = analyzer->scopedQueryAnalyzer;
    decision = scoped_cache_change_decision(
            scopedAnalyzer,
            currentAst,
            changeInfo);
    canPreserve = decision == ZR_SCOPED_CACHE_CHANGE_PRESERVE;
    if (!canPreserve) {
        analyzer->metrics.scopedCacheInvalidationCount++;
        if (decision ==
            ZR_SCOPED_CACHE_CHANGE_INVALIDATE_DIRECT_DEPENDENCY) {
            analyzer->metrics.scopedCacheDirectDependencyInvalidationCount++;
        } else if (decision ==
                   ZR_SCOPED_CACHE_CHANGE_INVALIDATE_CONSERVATIVE) {
            analyzer->metrics.scopedCacheConservativeInvalidationCount++;
        }
        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                state,
                analyzer);
        return ZR_FALSE;
    }

    *retainCurrentAst = scopedAnalyzer->ast == currentAst &&
                        scopedAnalyzer->ownedAst == ZR_NULL &&
                        scopedAnalyzer->borrowedAst == ZR_NULL;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_CommitScopedQueryCachePreservation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *newAst,
        SZrAstNode *retainedAst) {
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrAstNode *newScopeRoot;
    TZrBool canCommit;

    if (state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (analyzer == ZR_NULL) {
        if (retainedAst != ZR_NULL) {
            ZrParser_Ast_Free(state, retainedAst);
        }
        return ZR_FALSE;
    }
    if (newAst == ZR_NULL || analyzer->scopedQueryAnalyzer == ZR_NULL) {
        TZrBool cacheOwnsRetainedAst =
                analyzer->scopedQueryAnalyzer != ZR_NULL &&
                analyzer->scopedQueryAnalyzer->ownedAst == retainedAst;

        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                state,
                analyzer);
        if (retainedAst != ZR_NULL && !cacheOwnsRetainedAst) {
            ZrParser_Ast_Free(state, retainedAst);
        }
        return ZR_FALSE;
    }

    scopedAnalyzer = analyzer->scopedQueryAnalyzer;
    newScopeRoot = scopedAnalyzer->cache != ZR_NULL
                           ? ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
                                 newAst,
                                 scopedAnalyzer->cache->cacheRange)
                           : ZR_NULL;
    canCommit = scopedAnalyzer->cache != ZR_NULL &&
                scopedAnalyzer->cache->isValid && newScopeRoot != ZR_NULL &&
                scoped_cache_ranges_equal(
                        &scopedAnalyzer->cache->cacheRange,
                        &newScopeRoot->location) &&
                scopedAnalyzer->cache->scopeAstHash ==
                        ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(
                                newScopeRoot) &&
                (retainedAst == ZR_NULL ||
                 (scopedAnalyzer->ast == retainedAst &&
                  scopedAnalyzer->ownedAst == ZR_NULL &&
                  scopedAnalyzer->borrowedAst == ZR_NULL));
    if (!canCommit) {
        TZrBool cacheOwnsRetainedAst = scopedAnalyzer->ownedAst == retainedAst;

        analyzer->metrics.scopedCacheInvalidationCount++;
        analyzer->metrics.scopedCacheConservativeInvalidationCount++;
        ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
                state,
                analyzer);
        if (retainedAst != ZR_NULL && !cacheOwnsRetainedAst) {
            ZrParser_Ast_Free(state, retainedAst);
        }
        return ZR_FALSE;
    }

    if (retainedAst != ZR_NULL) {
        scopedAnalyzer->ownedAst = retainedAst;
    }
    analyzer->metrics.scopedCachePreservationCount++;
    analyzer->preserveScopedQueryAnalyzerOnNextAstChange = ZR_TRUE;
    return ZR_TRUE;
}
