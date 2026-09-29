#include "semantic_query_published_diagnostics.h"

#include "zr_vm_parser/diagnostic_builder.h"

/**
 * @brief 将分析阶段已发布的结构化诊断复制到本次查询结果。
 * @note 语义事实持有原诊断；查询数组持有独立副本，调用方不得转移或直接释放原事实。
 * @return context/fact 无效、复制失败或诊断数组不可用时返回 false；失败副本由 Copy 清理。
 */
TZrBool ZrParser_SemanticQueryPublished_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticDiagnosticFact *fact) {
    SZrStructuredDiagnostic diagnostic;

    if (context == ZR_NULL || fact == ZR_NULL ||
        !context->queryDiagnostics.isValid ||
        !ZrParser_StructuredDiagnostic_Copy(
                context->state, &diagnostic, &fact->diagnostic)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}
