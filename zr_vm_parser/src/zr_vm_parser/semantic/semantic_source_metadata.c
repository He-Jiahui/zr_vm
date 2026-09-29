#include "zr_vm_parser/semantic_source_metadata.h"

#include <string.h>

#include "zr_vm_core/string.h"
#include "zr_vm_parser/type_inference.h"

/** @brief 取得 using 资源在当前编译或语义分析上下文中的推断类型。
 * @pre outType 已由调用方初始化；资源 AST 与类型环境在查询期间有效。
 * @return 标识符优先使用变量绑定中已知的所有权限定，其他情况回退到表达式推断；失败返回 ZR_FALSE。
 */
TZrBool ZrParser_SemanticMetadata_InferUsingResourceType(
        SZrCompilerState *cs, SZrAstNode *resource, SZrInferredType *outType) {
    if (cs == ZR_NULL || resource == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 变量绑定可能保留表达式重推无法恢复的借用限定，因此先查环境再走通用推断。 */
    if (resource->type == ZR_AST_IDENTIFIER_LITERAL && cs->typeEnv != ZR_NULL &&
        resource->data.identifier.name != ZR_NULL &&
        ZrParser_TypeEnvironment_LookupVariable(
                cs->state, cs->typeEnv, resource->data.identifier.name, outType)) {
        return ZR_TRUE;
    }
    return ZrParser_ExpressionType_Infer(cs, resource, outType);
}

/** @brief 将 using 资源的 ownership qualifier 映射为清理计划使用的 builtin 标签。
 * @return UNIQUE、SHARED、BORROWED 对应 DROP，LOANED 对应 RETURN_LOAN，其他限定对应 NONE。
 */
EZrOwnershipBuiltinKind ZrParser_SemanticMetadata_UsingCleanupBuiltin(
        EZrOwnershipQualifier ownershipQualifier) {
    switch (ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return ZR_OWNERSHIP_BUILTIN_KIND_DROP;
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return ZR_OWNERSHIP_BUILTIN_KIND_RETURN_LOAN;
        default:
            return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
}

/** @brief 为 DROP 守卫在语义上下文中记录确定性清理元数据。
 * @return 非 DROP 或输入无效时返回 ZR_FALSE；适用输入会追加计划项并返回 ZR_TRUE。
 * @note 该计划是语义快照；实际作用域清理由编译器后续单独注册，区域 ID 来自同一语义上下文。
 */
TZrBool ZrParser_SemanticMetadata_RecordUsingCleanup(
        SZrCompilerState *cs, SZrAstNode *usingNode, const SZrInferredType *resourceType) {
    SZrDeterministicCleanupStep step;
    SZrAstNode *resource;
    const SZrTypeBinding *binding = ZR_NULL;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || usingNode == ZR_NULL ||
        usingNode->type != ZR_AST_USING_STATEMENT ||
        usingNode->data.usingStatement.guardKind != ZR_USING_GUARD_DROP) {
        return ZR_FALSE;
    }
    resource = usingNode->data.usingStatement.resource;
    if (resource == ZR_NULL) {
        return ZR_FALSE;
    }
    if (resource->type == ZR_AST_IDENTIFIER_LITERAL && cs->typeEnv != ZR_NULL) {
        binding = ZrParser_TypeEnvironment_FindVariableBinding(
                cs->typeEnv, resource->data.identifier.name);
    }
    /* 只复制符号、区域和所有权身份；可执行清理由编译器另行注册到 scope 栈，不从此快照执行。 */
    memset(&step, 0, sizeof(step));
    step.kind = ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(cs->semanticContext);
    step.ownerRegionId = step.regionId;
    step.symbolId = binding != ZR_NULL ? binding->symbolId : ZR_SEMANTIC_ID_INVALID;
    step.declarationOrder = (TZrInt32)cs->semanticContext->cleanupPlan.length;
    step.ownershipQualifier = resourceType != ZR_NULL
                                      ? resourceType->ownershipQualifier
                                      : ZR_OWNERSHIP_QUALIFIER_NONE;
    step.ownershipBuiltinKind =
            ZrParser_SemanticMetadata_UsingCleanupBuiltin(step.ownershipQualifier);
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    return ZrParser_Semantic_AppendCleanupStep(cs->semanticContext, &step);
}

/** @brief 在语义上下文中按模板 AST 顺序保留静态文本与插值表达式的分段视图。
 * @return 输入无效时返回 ZR_FALSE；缺少段数组或段节点缓冲区时仍返回 ZR_TRUE。
 * @note expression 与静态文本指针由 AST/GC 对象持有；语义上下文 reset/free 会清空或释放此数组。
 */
TZrBool ZrParser_SemanticMetadata_RecordTemplateSegments(
        SZrSemanticContext *context, SZrAstNode *templateNode) {
    SZrAstNodeArray *segments;

    if (context == ZR_NULL || templateNode == ZR_NULL ||
        templateNode->type != ZR_AST_TEMPLATE_STRING_LITERAL) {
        return ZR_FALSE;
    }
    segments = templateNode->data.templateStringLiteral.segments;
    if (segments == ZR_NULL || segments->nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    /* 保存借用 AST/GC 指针的浅层视图；编译器运行时字符串拼接仍按 AST 段独立生成。 */
    for (TZrSize index = 0; index < segments->count; index++) {
        SZrAstNode *node = segments->nodes[index];
        SZrTemplateSegment segment;
        if (node == ZR_NULL) {
            continue;
        }
        memset(&segment, 0, sizeof(segment));
        if (node->type == ZR_AST_STRING_LITERAL) {
            segment.staticText = node->data.stringLiteral.value;
            if (segment.staticText == ZR_NULL) {
                segment.staticText = ZrCore_String_Create(context->state, "", 0);
            }
        } else if (node->type == ZR_AST_INTERPOLATED_SEGMENT) {
            segment.isInterpolation = ZR_TRUE;
            segment.expression = node->data.interpolatedSegment.expression;
        } else {
            continue;
        }
        if (!ZrParser_Semantic_AppendTemplateSegment(context, &segment)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
