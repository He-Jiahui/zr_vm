#include "compiler_extern_decorator_diagnostics.h"

/**
 * @brief 集中处理 extern/FFI 装饰器校验器的失败出口，避免各规则产生不同诊断身份或范围。
 * @note 规则调用方提供具体消息、原因与修正建议；此处统一选择 error、invalid_decorator 和用户决策型 no-fix。
 * @return 无论结构化诊断发布还是普通错误回退，均返回 false 让调用链停止当前声明校验。
 */
TZrBool compiler_extern_report_invalid_decorator(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        const TZrChar *message,
        const TZrChar *cause,
        const TZrChar *suggestion) {
    SZrStructuredDiagnostic diagnostic;

    if (cs == ZR_NULL || cs->state == ZR_NULL || decoratorNode == ZR_NULL ||
        message == ZR_NULL || cause == ZR_NULL || suggestion == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    /* 结构化载荷分配失败时仍须拒绝声明；退回普通错误，避免资源压力使规则错误静默通过。 */
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                decoratorNode->location,
                "invalid_decorator",
                message,
                cause,
                suggestion) ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        ZrParser_Compiler_Error(cs, message, decoratorNode->location);
        return ZR_FALSE;
    }
    /* StructuredError 将载荷浅拷贝进 compiler state；字符串所有权随交接保留在该状态中。 */
    ZrParser_Compiler_StructuredError(cs, &diagnostic);
    return ZR_FALSE;
}
