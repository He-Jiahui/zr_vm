#include "semantic/semantic_analyzer_query_source.h"

SZrFileRange ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
        const SZrSemanticAnalyzer *analyzer,
        SZrFileRange position) {
    if (position.source == ZR_NULL && analyzer != ZR_NULL &&
        analyzer->ast != ZR_NULL) {
        position.source = analyzer->ast->location.source;
    }
    return position;
}
