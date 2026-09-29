#include "compiler_internal.h"

#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"
/* 只有来源和起止坐标都未提供时才回填编译错误范围，保留任一显式坐标。 */
static TZrBool compiler_diagnostic_range_is_empty(const SZrFileRange *range) {
    return (TZrBool)(range == ZR_NULL ||
                     (range->source == ZR_NULL &&
                      range->start.line == 0 &&
                      range->start.column == 0 &&
                      range->start.offset == 0U &&
                      range->end.line == 0 &&
                      range->end.column == 0 &&
                      range->end.offset == 0U));
}
/* 只挑出唯一所有权错误中已确认的违规事实，供编译阶段阻止移动后重用。 */
static const SZrSemanticOwnershipFact *compiler_unique_ownership_violation(
        const SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->ownershipFacts.length; index++) {
        const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
            fact->qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
            fact->isViolation) {
            return fact;
        }
    }
    return ZR_NULL;
}
/* 将当前编译错误持久化为语义事实，供后续模块查询和 LSP 投影读取。 */
TZrBool ZrParser_Compiler_PublishCurrentDiagnostic(SZrCompilerState *cs) {
    SZrSemanticDiagnosticFact fact;
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location;
    TZrBool result;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        cs->semanticContext == ZR_NULL || !cs->hasError ||
        cs->errorMessage == ZR_NULL) {
        return ZR_FALSE;
    }
    location = cs->errorLocation;
    memset(&fact, 0, sizeof(fact));
    fact.node = cs->currentAst;
    if (cs->hasStructuredError &&
        cs->structuredError.code != ZR_NULL &&
        cs->structuredError.message != ZR_NULL) {
        fact.diagnostic = cs->structuredError;
        if (compiler_diagnostic_range_is_empty(&fact.diagnostic.location)) {
            fact.diagnostic.location = location;
        }
        return ZrParser_SemanticFacts_AppendDiagnostic(
                cs->semanticContext, &fact);
    }
    /* 上支保留结构化字段并补齐空范围，事实层会深拷贝；下方处理缺少 code/message 的错误。 */
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "compiler_error",
                cs->errorMessage,
                ZR_NULL,
                ZR_NULL)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT)) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        return ZR_FALSE;
    }
    fact.diagnostic = diagnostic;
    result = ZrParser_SemanticFacts_AppendDiagnostic(
            cs->semanticContext, &fact);
    ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
    return result;
}
/* 先解析查询依赖的线性与控制流事实，再物化模块范围的结构化诊断视图。 */
TZrBool ZrParser_Compiler_PublishSemanticQueryDiagnostics(SZrCompilerState *cs) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrSemanticOwnershipFact *ownershipViolation;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(cs->semanticContext)) {
        return ZR_FALSE;
    }
    if (!ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(cs->semanticContext)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowOwnership(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }
    /* 只有各分析阶段均成功后，ownership 违规筛选才代表最终控制流状态。 */
    ownershipViolation = compiler_unique_ownership_violation(cs->semanticContext);
    if (ownershipViolation != ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                               "Unique value is used after it was moved",
                               ownershipViolation->range);
        return ZR_FALSE;
    }
    /* 控制流确认的唯一值移动后重用会成为编译错误，不进入普通诊断列表。 */
    ZrParser_SemanticQueryScope_Module(&scope);
    if (!ZrParser_SemanticQuery_MaterializeDiagnostics(cs->semanticContext, &scope)) {
        return ZR_FALSE;
    }
    /* TODO: compiler.c 忽略此状态；需核实分析或物化失败后继续编译时是否允许查询诊断缺失。 */
    /* 查询结果数组由 semanticContext 持有；这里的 items 只是模块范围的借用视图。 */
    return ZrParser_SemanticQuery_Diagnostics(cs->semanticContext, &scope, &diagnostics);
}
