#include "zr_vm_language_server/semantic_analyzer.h"

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

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->scopedQueryAnalyzer == ZR_NULL) {
        return;
    }

    scopedQueryAnalyzer = analyzer->scopedQueryAnalyzer;
    analyzer->scopedQueryAnalyzer = ZR_NULL;
    ZrLanguageServer_SemanticAnalyzer_Free(state, scopedQueryAnalyzer);
}
