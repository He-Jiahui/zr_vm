#include "semantic/semantic_analyzer_query_source.h"

/** 位置查询缺少 URI 时绑定当前 AST 来源，避免单文档请求误查到其他文件的同坐标事实。 */
SZrFileRange ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
        const SZrSemanticAnalyzer *analyzer,
        SZrFileRange position) {
    if (position.source == ZR_NULL && analyzer != ZR_NULL &&
        analyzer->ast != ZR_NULL) {
        position.source = analyzer->ast->location.source;
    }
    return position;
}
