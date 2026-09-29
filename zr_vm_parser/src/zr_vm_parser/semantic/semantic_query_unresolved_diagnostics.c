#include "semantic_query_unresolved_diagnostics.h"

#include "zr_vm_parser/diagnostic_builder.h"

#include <stdio.h>

/**
 * @brief 判断两个 fact 的来源是否指向同一份源文本。
 * @note unresolved fact 可能来自不同字符串对象；位置去重必须按内容识别文件名。
 */
static TZrBool semantic_query_unresolved_same_source(SZrString *left,
                                                      SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    return left == right || ZrCore_String_Equal(left, right);
}

/**
 * @brief 判断位置是否携带可用于跨版本比较的偏移坐标。
 * @note 旧式位置仅有行列号，调用方会在其缺失时回退到行列比较。
 */
static TZrBool semantic_query_unresolved_has_offset(
        const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0U;
}

/**
 * @brief 比较两个引用范围，用于压制同一语法位置的未解析投影。
 * @note 有偏移时以偏移为准；旧位置以行列为准，来源也必须相同。
 */
static TZrBool semantic_query_unresolved_ranges_equal(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_query_unresolved_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }
    if (semantic_query_unresolved_has_offset(&left->start) ||
        semantic_query_unresolved_has_offset(&left->end) ||
        semantic_query_unresolved_has_offset(&right->start) ||
        semantic_query_unresolved_has_offset(&right->end)) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }
    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

/**
 * @brief 按名称内容判断两个引用是否相同。
 * @note NULL 名称不参与抑制，避免缺失名称把无关 fact 合并。
 */
static TZrBool semantic_query_unresolved_names_equal(
        SZrString *left,
        SZrString *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           (left == right || ZrCore_String_Equal(left, right));
}

/**
 * @brief 限定可转换为用户可见“未解析引用”的 fact 种类。
 * @note 声明、合成 payload 等不是查询失败，不能由此产生重复错误。
 */
static TZrBool semantic_query_unresolved_kind_is_reportable(
        EZrSemanticReferenceKind kind) {
    return kind == ZR_SEMANTIC_REFERENCE_READ ||
           kind == ZR_SEMANTIC_REFERENCE_WRITE ||
           kind == ZR_SEMANTIC_REFERENCE_CALL ||
           kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
           kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE ||
           kind == ZR_SEMANTIC_REFERENCE_TYPE;
}

/**
 * @brief 判断引用失败属于成员查找，以选择成员专用诊断文案和 code。
 */
static TZrBool semantic_query_unresolved_kind_is_member(
        EZrSemanticReferenceKind kind) {
    return kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
           kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE;
}

/**
 * @brief 识别虽标记未解析、但已经带有 canonical 绑定结果的引用 fact。
 * @note 这些 fact 表示外部/类型系统已提供目标信息，不应被投影成缺失符号。
 */
static TZrBool semantic_query_unresolved_has_canonical_target(
        const SZrSemanticReferenceFact *fact) {
    return fact != ZR_NULL &&
           (fact->symbolId != ZR_SEMANTIC_ID_INVALID ||
            fact->typeId != ZR_SEMANTIC_ID_INVALID ||
            fact->signatureDisplay != ZR_NULL ||
            fact->contractRole != 0U);
}

/**
 * @brief 决定引用 fact 是否仍代表一个应显示的 unresolved 结果。
 * @note 同名同范围的 resolved fact 优先；该规则兼容分析阶段留下的旧 unresolved 记录。
 */
static TZrBool semantic_query_unresolved_is_effective(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact) {
    TZrSize index;

    if (context == ZR_NULL || fact == ZR_NULL || fact->isResolved ||
        fact->name == ZR_NULL ||
        semantic_query_unresolved_has_canonical_target(fact) ||
        !semantic_query_unresolved_kind_is_reportable(fact->kind) ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *candidate =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        if (candidate != ZR_NULL &&
            candidate->isResolved &&
            semantic_query_unresolved_kind_is_reportable(candidate->kind) &&
            semantic_query_unresolved_names_equal(candidate->name, fact->name) &&
            semantic_query_unresolved_ranges_equal(
                    &candidate->range, &fact->range)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 将有效的 unresolved reference fact 转为结构化查询诊断。
 * @note 仅接受 materializer 选出的有效 reference fact；诊断需要用户决策，故不猜测自动修复。
 * @return 成功追加到 queryDiagnostics 时返回 true；构建或 no-fix 元数据失败时清理临时诊断。
 */
TZrBool ZrParser_SemanticQueryUnresolved_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact) {
    const TZrChar *code;
    const TZrChar *name;
    const TZrChar *cause;
    const TZrChar *suggestion;
    TZrChar message[256];
    SZrStructuredDiagnostic diagnostic;

    if (!semantic_query_unresolved_is_effective(context, fact) ||
        !context->queryDiagnostics.isValid) {
        return ZR_FALSE;
    }

    name = fact->name != ZR_NULL
                   ? ZrCore_String_GetNativeString(fact->name)
                   : "reference";
    if (semantic_query_unresolved_kind_is_member(fact->kind)) {
        code = "member_not_found";
        cause = "Canonical member binding found no field, property, or method for this reference.";
        suggestion = "Declare the member or use a member exposed by the receiver's canonical type.";
        (void)snprintf(message,
                       sizeof(message),
                       "Member '%s' could not be resolved",
                       name);
    } else {
        code = "unresolved_reference";
        cause = "Canonical semantic binding found no declaration for this reference.";
        suggestion = "Declare or import the referenced symbol and verify that it is visible in this scope.";
        (void)snprintf(message,
                       sizeof(message),
                       "Reference '%s' could not be resolved",
                       name);
    }

    if (!ZrParser_DiagnosticBuilder_Build(
                context->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                fact->range,
                code,
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}
