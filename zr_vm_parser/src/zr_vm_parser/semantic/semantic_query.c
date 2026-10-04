#include "zr_vm_parser/semantic_query.h"

#include "semantic_query_ownership_diagnostics.h"
#include "semantic_query_published_diagnostics.h"
#include "semantic_query_unresolved_diagnostics.h"

#include <string.h>

static TZrBool semantic_query_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool semantic_query_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool semantic_query_range_contains_position(const SZrFileRange *range,
                                                      const SZrFileRange *position) {
    TZrSize queryOffset;
    TZrInt32 queryLine;
    TZrInt32 queryColumn;

    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_query_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }

    if ((semantic_query_has_offset(&range->start) ||
         semantic_query_has_offset(&range->end)) &&
        (semantic_query_has_offset(&position->start) ||
         semantic_query_has_offset(&position->end))) {
        queryOffset = position->start.offset;
        return queryOffset >= range->start.offset && queryOffset <= range->end.offset;
    }

    queryLine = position->start.line;
    queryColumn = position->start.column;
    if (queryLine < range->start.line || queryLine > range->end.line) {
        return ZR_FALSE;
    }
    if (queryLine == range->start.line && queryColumn < range->start.column) {
        return ZR_FALSE;
    }
    if (queryLine == range->end.line && queryColumn > range->end.column) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool semantic_query_range_contains_range(const SZrFileRange *outer,
                                                   const SZrFileRange *inner) {
    SZrFileRange endPosition;

    if (outer == ZR_NULL || inner == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!semantic_query_range_contains_position(outer, inner)) {
        return ZR_FALSE;
    }

    endPosition = *inner;
    endPosition.start = inner->end;
    return semantic_query_range_contains_position(outer, &endPosition);
}

static TZrBool semantic_query_ranges_equal(const SZrFileRange *left,
                                           const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_query_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }

    if ((semantic_query_has_offset(&left->start) ||
         semantic_query_has_offset(&left->end) ||
         semantic_query_has_offset(&right->start) ||
         semantic_query_has_offset(&right->end))) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }

    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

/* 模块查询允许全快照；节点查询只接受完全落在 root 内的事实，避免局部查询泄漏相邻节点。 */
static TZrBool semantic_query_scope_allows_range(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *range) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    if (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE || scope->root == ZR_NULL) {
        return ZR_FALSE;
    }
    return semantic_query_range_contains_range(&scope->root->location, range);
}

/* 先限制查询位置，再限制命中的事实范围；root 借用当前 AST，调用期须保持有效。 */
static TZrBool semantic_query_scope_allows_position(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *position) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    if (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE || scope->root == ZR_NULL) {
        return ZR_FALSE;
    }
    return semantic_query_range_contains_position(&scope->root->location, position);
}

static const SZrSemanticNumericFact *semantic_query_find_numeric_at_position(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope) {
    TZrSize i;

    if (context == ZR_NULL || !context->numericFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->numericFacts.length; i++) {
        const SZrSemanticNumericFact *fact =
            (const SZrSemanticNumericFact *)ZrCore_Array_Get((SZrArray *)&context->numericFacts, i);
        if (fact != ZR_NULL &&
            semantic_query_range_contains_position(&fact->range, &position) &&
            semantic_query_scope_allows_range(scope, &fact->range)) {
            return fact;
        }
    }
    return ZR_NULL;
}

/* 单目标定义查询的回退：声明引用直接使用自身，其余已解析引用按身份或声明范围找同 scope 声明。 */
static const SZrSemanticReferenceFact *semantic_query_find_declaration_for_reference(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        const SZrParserSemanticQueryScope *scope) {
    TZrSize i;

    if (context == ZR_NULL || reference == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    if (reference->kind == ZR_SEMANTIC_REFERENCE_DECLARATION) {
        return reference;
    }
    if (!reference->isResolved) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !semantic_query_scope_allows_range(scope, &fact->range)) {
            continue;
        }
        if (reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
            fact->symbolId == reference->symbolId) {
            return fact;
        }
        if (semantic_query_ranges_equal(&fact->range, &reference->declarationRange)) {
            return fact;
        }
    }

    return ZR_NULL;
}

/* 单目标导航只消费 definitionRange；多分支定义集合由 DefinitionsOf 展开，结果借用当前事实快照。 */
static const SZrSemanticReferenceFact *semantic_query_find_reaching_definition_for_reference(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        const SZrParserSemanticQueryScope *scope) {
    TZrSize i;

    if (context == ZR_NULL ||
        reference == ZR_NULL ||
        reference->kind != ZR_SEMANTIC_REFERENCE_READ ||
        !reference->isResolved ||
        !reference->hasDefinitionRange ||
        reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact == ZR_NULL ||
            fact->symbolId != reference->symbolId ||
            (fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION &&
             fact->kind != ZR_SEMANTIC_REFERENCE_WRITE) ||
            !semantic_query_scope_allows_range(scope, &fact->range)) {
            continue;
        }
        if (semantic_query_ranges_equal(&fact->range, &reference->definitionRange)) {
            return fact;
        }
    }

    return ZR_NULL;
}

static TZrBool semantic_query_reference_output_contains(SZrArray *outDefinitions,
                                                        const SZrSemanticReferenceFact *candidate) {
    TZrSize index;

    if (outDefinitions == ZR_NULL || candidate == ZR_NULL || !outDefinitions->isValid) {
        return ZR_FALSE;
    }

    for (index = 0; index < outDefinitions->length; index++) {
        const SZrSemanticReferenceFact **slot =
                (const SZrSemanticReferenceFact **)ZrCore_Array_Get(outDefinitions, index);
        if (slot != ZR_NULL && *slot == candidate) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* DefinitionsOf 输出存放 fact 借用指针；复用数组必须已按该元素类型初始化，函数只清长度。 */
static TZrBool semantic_query_prepare_reference_output(const SZrSemanticContext *context,
                                                       SZrArray *outDefinitions) {
    if (context == ZR_NULL || context->state == ZR_NULL || outDefinitions == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!outDefinitions->isValid) {
        ZrCore_Array_Init(context->state,
                          outDefinitions,
                          sizeof(const SZrSemanticReferenceFact *),
                          ZR_PARSER_INITIAL_CAPACITY_SMALL);
    } else {
        outDefinitions->length = 0;
    }

    return ZR_TRUE;
}

/* 追加借用指针而非复制 fact；以指针去重，调用方持有数组但不持有事实本体。 */
static TZrBool semantic_query_append_definition_fact(const SZrSemanticContext *context,
                                                     SZrArray *outDefinitions,
                                                     const SZrSemanticReferenceFact *definition,
                                                     const SZrParserSemanticQueryScope *scope) {
    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        outDefinitions == ZR_NULL ||
        definition == ZR_NULL ||
        !semantic_query_scope_allows_range(scope, &definition->range) ||
        semantic_query_reference_output_contains(outDefinitions, definition)) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(context->state, outDefinitions, &definition);
    return ZR_TRUE;
}

/* 把可达定义范围恢复为声明/写入事实；fact 自身和它保存的 definitionRange 都可指向该定义。 */
static TZrBool semantic_query_append_definition_for_range(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        const SZrFileRange *range,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outDefinitions) {
    TZrSize i;

    if (context == ZR_NULL ||
        reference == ZR_NULL ||
        range == ZR_NULL ||
        reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->referenceFacts.isValid ||
        !semantic_query_scope_allows_range(scope, range)) {
        return ZR_FALSE;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact == ZR_NULL ||
            fact->symbolId != reference->symbolId ||
            (fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION &&
             fact->kind != ZR_SEMANTIC_REFERENCE_WRITE)) {
            continue;
        }
        if (semantic_query_ranges_equal(&fact->range, range) ||
            (fact->hasDefinitionRange &&
             semantic_query_ranges_equal(&fact->definitionRange, range))) {
            return semantic_query_append_definition_fact(context, outDefinitions, fact, scope);
        }
    }

    return ZR_FALSE;
}

/* 读取可能有多个 reaching ranges；优先展开集合，仅无集合时使用单一 definitionRange。 */
static TZrBool semantic_query_append_reaching_definitions_for_reference(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outDefinitions) {
    TZrSize index;

    if (context == ZR_NULL ||
        reference == ZR_NULL ||
        reference->kind != ZR_SEMANTIC_REFERENCE_READ ||
        !reference->isResolved) {
        return ZR_FALSE;
    }

    if (reference->definitionRanges.isValid && reference->definitionRanges.length > 0) {
        for (index = 0; index < reference->definitionRanges.length; index++) {
            const SZrFileRange *range =
                    (const SZrFileRange *)ZrCore_Array_Get((SZrArray *)&reference->definitionRanges, index);
            (void)semantic_query_append_definition_for_range(context,
                                                             reference,
                                                             range,
                                                             scope,
                                                             outDefinitions);
        }
    } else if (reference->hasDefinitionRange) {
        (void)semantic_query_append_definition_for_range(context,
                                                         reference,
                                                         &reference->definitionRange,
                                                         scope,
                                                         outDefinitions);
    }

    return outDefinitions != ZR_NULL && outDefinitions->length > 0;
}

static TZrInt32 semantic_query_compare_file_ranges(const SZrFileRange *left,
                                                   const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return 0;
    }

    if (semantic_query_has_offset(&left->start) ||
        semantic_query_has_offset(&left->end) ||
        semantic_query_has_offset(&right->start) ||
        semantic_query_has_offset(&right->end)) {
        if (left->start.offset < right->start.offset) {
            return -1;
        }
        if (left->start.offset > right->start.offset) {
            return 1;
        }
        if (left->end.offset < right->end.offset) {
            return -1;
        }
        if (left->end.offset > right->end.offset) {
            return 1;
        }
        return 0;
    }

    if (left->start.line < right->start.line) {
        return -1;
    }
    if (left->start.line > right->start.line) {
        return 1;
    }
    if (left->start.column < right->start.column) {
        return -1;
    }
    if (left->start.column > right->start.column) {
        return 1;
    }
    if (left->end.line < right->end.line) {
        return -1;
    }
    if (left->end.line > right->end.line) {
        return 1;
    }
    if (left->end.column < right->end.column) {
        return -1;
    }
    if (left->end.column > right->end.column) {
        return 1;
    }
    return 0;
}

static TZrInt32 semantic_query_compare_definition_facts(
        const SZrSemanticReferenceFact *left,
        const SZrSemanticReferenceFact *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return 0;
    }
    if (!semantic_query_same_source(left->range.source, right->range.source)) {
        return 0;
    }
    return semantic_query_compare_file_ranges(&left->range, &right->range);
}

/* 插入排序仅进行相邻候选比较；不同 source 比较为零并停止移动，不保证跨 source 分隔的同源组整体有序。 */
static void semantic_query_sort_definition_output(SZrArray *definitions) {
    TZrSize i;

    if (definitions == ZR_NULL || !definitions->isValid || definitions->length < 2) {
        return;
    }

    for (i = 1; i < definitions->length; i++) {
        const SZrSemanticReferenceFact **slot =
                (const SZrSemanticReferenceFact **)ZrCore_Array_Get(definitions, i);
        const SZrSemanticReferenceFact *key = slot != ZR_NULL ? *slot : ZR_NULL;
        TZrSize j = i;

        while (j > 0) {
            const SZrSemanticReferenceFact **previousSlot =
                    (const SZrSemanticReferenceFact **)ZrCore_Array_Get(definitions, j - 1);
            const SZrSemanticReferenceFact *previous =
                    previousSlot != ZR_NULL ? *previousSlot : ZR_NULL;
            if (semantic_query_compare_definition_facts(previous, key) <= 0) {
                break;
            }
            ZrCore_Array_Set(definitions, j, &previous);
            j--;
        }
        ZrCore_Array_Set(definitions, j, &key);
    }
}

static const TZrChar *semantic_query_reachability_cause_text(
        EZrSemanticReachabilityCause cause) {
    switch (cause) {
        case ZR_SEMANTIC_REACHABILITY_AFTER_RETURN:
            return "A previous return statement exits this control-flow path before the statement can execute.";
        case ZR_SEMANTIC_REACHABILITY_AFTER_THROW:
            return "A previous throw statement exits this control-flow path before the statement can execute.";
        case ZR_SEMANTIC_REACHABILITY_AFTER_BREAK:
            return "A previous break statement exits the current loop path before the statement can execute.";
        case ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE:
            return "A previous continue statement skips the rest of the current loop body.";
        case ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE:
            return "The controlling condition is known to be false on this path.";
        case ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH:
            return "A constant condition or switch selector prevents this branch from executing.";
        case ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT:
            return "Short-circuit evaluation prevents this expression from executing.";
        case ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH:
            return "All control-flow alternatives are exhausted before this statement.";
        case ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP:
            return "The preceding loop has no fallthrough path to this statement.";
        case ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN:
        default:
            return "Control-flow analysis found no reachable path to this statement.";
    }
}

static const TZrChar *semantic_query_reachability_suggestion_text(
        EZrSemanticReachabilityCause cause) {
    switch (cause) {
        case ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE:
        case ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH:
        case ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT:
        case ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH:
            return "Remove the unreachable branch or change the condition so this path can execute.";
        case ZR_SEMANTIC_REACHABILITY_AFTER_RETURN:
        case ZR_SEMANTIC_REACHABILITY_AFTER_THROW:
        case ZR_SEMANTIC_REACHABILITY_AFTER_BREAK:
        case ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE:
        case ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP:
        case ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN:
        default:
            return "Remove the unreachable statement or move it before the control-flow exit.";
    }
}

/* 物化替换旧缓存：先释放每条诊断拥有的资源，再复用数组容量；旧 Diagnostics 视图随之失效。 */
static void semantic_query_reset_diagnostics(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL || !context->queryDiagnostics.isValid) {
        return;
    }

    for (i = 0; i < context->queryDiagnostics.length; i++) {
        SZrStructuredDiagnostic *diagnostic =
            (SZrStructuredDiagnostic *)ZrCore_Array_Get(&context->queryDiagnostics, i);
        if (diagnostic != ZR_NULL) {
            ZrParser_StructuredDiagnostic_Free(context->state, diagnostic);
        }
    }
    context->queryDiagnostics.length = 0;
}

/* 把不可达事实投影为 warning；自动删除可能不安全，因此明确发布 no-fix 原因。 */
static TZrBool semantic_query_append_unreachable_diagnostic(
        SZrSemanticContext *context,
        const SZrSemanticReachabilityFact *fact) {
    SZrStructuredDiagnostic diagnostic;

    if (context == ZR_NULL ||
        fact == ZR_NULL ||
        fact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        !context->queryDiagnostics.isValid) {
        return ZR_FALSE;
    }

    if (!ZrParser_DiagnosticBuilder_Build(
            context->state,
            &diagnostic,
            ZR_STRUCTURED_DIAGNOSTIC_WARNING,
            fact->range,
            "unreachable_code",
            "Unreachable code",
            semantic_query_reachability_cause_text(fact->cause),
            semantic_query_reachability_suggestion_text(fact->cause))) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic, ZR_DIAGNOSTIC_NO_FIX_REASON_UNSAFE_EDIT)) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }

    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}

static TZrBool semantic_query_reference_is_uninitialized_read_diagnostic(
        const SZrSemanticReferenceFact *fact) {
    return fact != ZR_NULL &&
           fact->kind == ZR_SEMANTIC_REFERENCE_READ &&
           fact->hasDefiniteAssignmentState &&
           (fact->definiteAssignmentState == ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT ||
            fact->definiteAssignmentState == ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT);
}

/* UNINIT 是 error，MAYBE_INIT 是 warning；占位修复只提供用户编辑起点，不自动承诺安全初始化。 */
static TZrBool semantic_query_append_definite_assignment_diagnostic(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact) {
    SZrStructuredDiagnostic diagnostic;
    EZrStructuredDiagnosticSeverity severity;
    const TZrChar *code;
    const TZrChar *message;
    const TZrChar *cause;

    if (context == ZR_NULL ||
        !semantic_query_reference_is_uninitialized_read_diagnostic(fact) ||
        !context->queryDiagnostics.isValid) {
        return ZR_FALSE;
    }

    if (fact->definiteAssignmentState == ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT) {
        severity = ZR_STRUCTURED_DIAGNOSTIC_ERROR;
        code = "uninitialized_read";
        message = "Variable is read before assignment";
        cause = "Definite-assignment analysis found no assignment reaching this read.";
    } else {
        severity = ZR_STRUCTURED_DIAGNOSTIC_WARNING;
        code = "possibly_uninitialized_read";
        message = "Variable may be read before assignment";
        cause = "Definite-assignment analysis found at least one path where this variable is not assigned.";
    }

    if (!ZrParser_DiagnosticBuilder_Build(
            context->state,
            &diagnostic,
            severity,
            fact->range,
            code,
            message,
            cause,
            "Assign the variable on every path before reading it.")) {
        return ZR_FALSE;
    }

    if (!ZrParser_StructuredDiagnostic_AddRelatedInformation(
            context->state,
            &diagnostic,
            fact->declarationRange,
            "Variable declaration is here")) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_AddFix(
            context->state,
            &diagnostic,
            "Replace with an initialized value",
            fact->range,
            "<value>",
            ZR_DIAGNOSTIC_FIX_HAS_PLACEHOLDERS)) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }

    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}

static TZrBool semantic_query_numeric_is_overflow_diagnostic(
        const SZrSemanticNumericFact *fact) {
    return fact != ZR_NULL && fact->mayOverflow;
}

/* mayOverflow 只表示可能溢出，发布 warning 并要求用户选择扩宽类型或加守卫。 */
static TZrBool semantic_query_append_numeric_overflow_diagnostic(
        SZrSemanticContext *context,
        const SZrSemanticNumericFact *fact) {
    SZrStructuredDiagnostic diagnostic;

    if (context == ZR_NULL ||
        !semantic_query_numeric_is_overflow_diagnostic(fact) ||
        !context->queryDiagnostics.isValid) {
        return ZR_FALSE;
    }

    if (!ZrParser_DiagnosticBuilder_Build(
            context->state,
            &diagnostic,
            ZR_STRUCTURED_DIAGNOSTIC_WARNING,
            fact->range,
            "numeric_overflow",
            "Numeric expression may overflow",
            "Numeric range analysis found an arithmetic result that may overflow the target integer type.",
            "Use a wider type or guard the expression before evaluating it.")) {
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

static TZrBool semantic_query_expression_is_array_bounds_diagnostic(
        const SZrSemanticExpressionFact *fact) {
    return fact != ZR_NULL &&
           fact->diagnosticMessage != ZR_NULL &&
           fact->kind == ZR_SEMANTIC_EXPRESSION_FACT_MEMBER &&
           fact->hasMemberInfo &&
           fact->memberIsComputed;
}

static EZrStructuredDiagnosticSeverity semantic_query_expression_diagnostic_severity(
        EZrSemanticDiagnosticSeverity severity) {
    switch (severity) {
        case ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_WARNING:
            return ZR_STRUCTURED_DIAGNOSTIC_WARNING;
        case ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_INFO:
            return ZR_STRUCTURED_DIAGNOSTIC_INFO;
        case ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_HINT:
            return ZR_STRUCTURED_DIAGNOSTIC_HINT;
        case ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_ERROR:
        case ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_DEFAULT:
        default:
            return ZR_STRUCTURED_DIAGNOSTIC_ERROR;
    }
}

/* 消费已有 computed member 诊断；code 决定类型不匹配、可能越界或确定越界的解释，不在查询层重算索引。 */
static TZrBool semantic_query_append_array_bounds_diagnostic(
        SZrSemanticContext *context,
        const SZrSemanticExpressionFact *fact) {
    SZrStructuredDiagnostic diagnostic;
    EZrStructuredDiagnosticSeverity severity;
    const TZrChar *code;
    const TZrChar *cause;
    const TZrChar *message;
    const TZrChar *suggestion;
    TZrBool isPossibleBounds;
    TZrBool isTypeMismatch;

    if (context == ZR_NULL ||
        !semantic_query_expression_is_array_bounds_diagnostic(fact) ||
        !context->queryDiagnostics.isValid) {
        return ZR_FALSE;
    }

    code = fact->diagnosticCode != ZR_NULL ? ZrCore_String_GetNativeString(fact->diagnosticCode) : ZR_NULL;
    if (code == ZR_NULL) {
        code = "array_index_out_of_bounds";
    }
    message = ZrCore_String_GetNativeString(fact->diagnosticMessage);
    if (message == ZR_NULL) {
        message = "Array index is out of bounds";
    }
    severity = semantic_query_expression_diagnostic_severity(fact->diagnosticSeverity);
    isPossibleBounds = (TZrBool)(strcmp(code, "array_index_may_be_out_of_bounds") == 0);
    isTypeMismatch = (TZrBool)(strcmp(code, "array_index_type_mismatch") == 0);
    if (isTypeMismatch) {
        cause = "Type inference found that the computed array index is not an integer expression.";
        suggestion = "Use an integer index or narrow the expression to an integer type before indexing.";
    } else if (isPossibleBounds) {
        cause = "Numeric range analysis found at least one possible index outside the array's fixed bounds.";
        suggestion = "Guard the access or narrow the index before indexing.";
    } else {
        cause = "Numeric range analysis proved the computed array index cannot reference an element.";
        suggestion = "Use an index in the array's fixed bounds or guard the access before indexing.";
    }

    if (!ZrParser_DiagnosticBuilder_Build(
            context->state,
            &diagnostic,
            severity,
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

/* 填充模块查询范围；scope 不拥有语义快照，NULL 输出按无操作处理。 */
void ZrParser_SemanticQueryScope_Module(SZrParserSemanticQueryScope *scope) {
    if (scope == ZR_NULL) {
        return;
    }

    scope->kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE;
    scope->root = ZR_NULL;
}

/* root 是当前 AST 的借用指针；NULL root 虽可存入，随后查询门禁会拒绝该节点 scope。 */
void ZrParser_SemanticQueryScope_Node(SZrParserSemanticQueryScope *scope,
                                      const SZrAstNode *root) {
    if (scope == ZR_NULL) {
        return;
    }

    scope->kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE;
    scope->root = root;
}

/* 只把 exactness 允许 projection 的表达式类型复制给消费者，避免把不完整推断结果当作确定类型。 */
TZrBool ZrParser_SemanticQuery_TypeAt(const SZrSemanticContext *context,
                                      SZrFileRange position,
                                      const SZrParserSemanticQueryScope *scope,
                                      SZrInferredType *outType) {
    const SZrSemanticExpressionFact *fact;

    if (context == ZR_NULL || outType == ZR_NULL ||
        !semantic_query_scope_allows_position(scope, &position)) {
        return ZR_FALSE;
    }

    fact = ZrParser_SemanticFacts_FindExpressionAtPosition(context, position);
    if (fact == ZR_NULL ||
        !ZrParser_SemanticQuery_ExactnessAllowsProjection(fact->exactness) ||
        !semantic_query_scope_allows_range(scope, &fact->range)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(context->state, outType, &fact->inferredType);
    return ZR_TRUE;
}

/* 单目标导航优先取读取的可达定义，否则退回声明；返回值借用当前 context 的事实存储。 */
const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DefinitionOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope) {
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticReferenceFact *definition;

    if (context == ZR_NULL || !semantic_query_scope_allows_position(scope, &position)) {
        return ZR_NULL;
    }

    reference = ZrParser_SemanticFacts_FindReferenceAtPosition(context, position);
    if (reference == ZR_NULL || !semantic_query_scope_allows_range(scope, &reference->range)) {
        return ZR_NULL;
    }

    definition = semantic_query_find_reaching_definition_for_reference(context, reference, scope);
    if (definition != ZR_NULL) {
        return definition;
    }

    return semantic_query_find_declaration_for_reference(context, reference, scope);
}

/* 多目标导航返回 fact 借用指针数组；调用方释放数组容量，context 保持事实所有权。 */
TZrBool ZrParser_SemanticQuery_DefinitionsOf(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outDefinitions) {
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticReferenceFact *declaration;

    if (context == ZR_NULL ||
        outDefinitions == ZR_NULL ||
        !context->referenceFacts.isValid ||
        !semantic_query_scope_allows_position(scope, &position) ||
        !semantic_query_prepare_reference_output(context, outDefinitions)) {
        return ZR_FALSE;
    }

    reference = ZrParser_SemanticFacts_FindReferenceAtPosition(context, position);
    if (reference == ZR_NULL || !semantic_query_scope_allows_range(scope, &reference->range)) {
        return ZR_FALSE;
    }

    if (semantic_query_append_reaching_definitions_for_reference(context,
                                                                reference,
                                                                scope,
                                                                outDefinitions)) {
        semantic_query_sort_definition_output(outDefinitions);
        return ZR_TRUE;
    }

    declaration = semantic_query_find_declaration_for_reference(context, reference, scope);
    if (declaration == ZR_NULL) {
        return ZR_FALSE;
    }

    (void)semantic_query_append_definition_fact(context, outDefinitions, declaration, scope);
    return outDefinitions->length > 0;
}

/* 输出包含同身份的全部 reference kinds，用于引用查找/code lens；数组只拥有借用指针槽位。 */
TZrBool ZrParser_SemanticQuery_ReferencesOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences) {
    TZrSize i;

    if (context == ZR_NULL || outReferences == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    if (!outReferences->isValid) {
        ZrCore_Array_Init(context->state,
                          outReferences,
                          sizeof(const SZrSemanticReferenceFact *),
                          ZR_PARSER_INITIAL_CAPACITY_SMALL);
    } else {
        outReferences->length = 0;
    }

    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL &&
            fact->symbolId == symbolId &&
            semantic_query_scope_allows_range(scope, &fact->range)) {
            ZrCore_Array_Push(context->state, outReferences, &fact);
        }
    }

    return outReferences->length > 0;
}

/* 按位置聚合各类借用事实供 LSP 复用；结果生命周期不超过当前语义快照。 */
TZrBool ZrParser_SemanticQuery_FactsAt(const SZrSemanticContext *context,
                                       SZrFileRange position,
                                       const SZrParserSemanticQueryScope *scope,
                                       SZrParserSemanticQueryFacts *outFacts) {
    if (outFacts == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outFacts, 0, sizeof(*outFacts));
    if (context == ZR_NULL || !semantic_query_scope_allows_position(scope, &position)) {
        return ZR_FALSE;
    }

    outFacts->expression = ZrParser_SemanticFacts_FindExpressionAtPosition(context, position);
    if (outFacts->expression != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->expression->range)) {
        outFacts->expression = ZR_NULL;
    }

    outFacts->reference = ZrParser_SemanticFacts_FindReferenceAtPosition(context, position);
    if (outFacts->reference != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->reference->range)) {
        outFacts->reference = ZR_NULL;
    }

    outFacts->numeric = semantic_query_find_numeric_at_position(context, position, scope);

    outFacts->reachability = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, position);
    if (outFacts->reachability != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->reachability->range)) {
        outFacts->reachability = ZR_NULL;
    }

    outFacts->logical = ZrParser_SemanticFacts_FindLogicalAtPosition(context, position);
    if (outFacts->logical != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->logical->range)) {
        outFacts->logical = ZR_NULL;
    }

    outFacts->ownership = ZrParser_SemanticFacts_FindOwnershipAtPosition(context, position);
    if (outFacts->ownership != ZR_NULL &&
        !semantic_query_scope_allows_range(scope, &outFacts->ownership->range)) {
        outFacts->ownership = ZR_NULL;
    }

    return outFacts->expression != ZR_NULL ||
           outFacts->reference != ZR_NULL ||
           outFacts->numeric != ZR_NULL ||
           outFacts->reachability != ZR_NULL ||
           outFacts->logical != ZR_NULL ||
           outFacts->ownership != ZR_NULL;
}

/* compiler 和 LSP 共用此投影缓存；重建释放旧诊断资源，并记录 scope root 供读取时验证。 */
TZrBool ZrParser_SemanticQuery_MaterializeDiagnostics(
        SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope) {
    TZrSize i;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (scope != ZR_NULL &&
        (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE &&
         (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE || scope->root == ZR_NULL))) {
        return ZR_FALSE;
    }

    semantic_query_reset_diagnostics(context);
    /* TODO: AppendDiagnostic 的 false 兼有“不适用”和构建失败；当前忽略失败仍标记物化成功，需区分结果并核实编译器/LSP 可见诊断。 */
    if (context->diagnosticFacts.isValid) {
        for (i = 0U; i < context->diagnosticFacts.length; i++) {
            const SZrSemanticDiagnosticFact *fact =
                    (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->diagnosticFacts, i);
            if (fact != ZR_NULL &&
                semantic_query_scope_allows_range(
                        scope, &fact->diagnostic.location)) {
                (void)ZrParser_SemanticQueryPublished_AppendDiagnostic(
                        context, fact);
            }
        }
    }

    if (context->reachabilityFacts.isValid) {
        for (i = 0; i < context->reachabilityFacts.length; i++) {
            const SZrSemanticReachabilityFact *fact =
                (const SZrSemanticReachabilityFact *)ZrCore_Array_Get((SZrArray *)&context->reachabilityFacts, i);
            if (fact != ZR_NULL &&
                fact->state == ZR_SEMANTIC_REACHABILITY_UNREACHABLE &&
                semantic_query_scope_allows_range(scope, &fact->range)) {
                (void)semantic_query_append_unreachable_diagnostic(context, fact);
            }
        }
    }

    if (context->referenceFacts.isValid) {
        for (i = 0; i < context->referenceFacts.length; i++) {
            const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
            if (fact == ZR_NULL ||
                !semantic_query_scope_allows_range(scope, &fact->range)) {
                continue;
            }
            (void)ZrParser_SemanticQueryUnresolved_AppendDiagnostic(
                    context, fact);
            if (semantic_query_reference_is_uninitialized_read_diagnostic(fact)) {
                (void)semantic_query_append_definite_assignment_diagnostic(context, fact);
            }
        }
    }

    if (context->ownershipFacts.isValid) {
        for (i = 0; i < context->ownershipFacts.length; i++) {
            const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipFacts,
                        i);
            if (fact != ZR_NULL &&
                semantic_query_scope_allows_range(scope, &fact->range)) {
                (void)ZrParser_SemanticQueryOwnership_AppendDiagnostic(context, fact);
            }
        }
    }

    if (context->expressionFacts.isValid) {
        for (i = 0; i < context->expressionFacts.length; i++) {
            const SZrSemanticExpressionFact *fact =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get((SZrArray *)&context->expressionFacts, i);
            if (fact != ZR_NULL &&
                semantic_query_scope_allows_range(scope, &fact->range) &&
                semantic_query_expression_is_array_bounds_diagnostic(fact)) {
                (void)semantic_query_append_array_bounds_diagnostic(context, fact);
            }
        }
    }

    if (context->numericFacts.isValid) {
        for (i = 0; i < context->numericFacts.length; i++) {
            const SZrSemanticNumericFact *fact =
                (const SZrSemanticNumericFact *)ZrCore_Array_Get((SZrArray *)&context->numericFacts, i);
            if (fact != ZR_NULL &&
                semantic_query_scope_allows_range(scope, &fact->range) &&
                semantic_query_numeric_is_overflow_diagnostic(fact)) {
                (void)semantic_query_append_numeric_overflow_diagnostic(context, fact);
            }
        }
    }

    context->queryDiagnosticsMaterialized = ZR_TRUE;
    context->queryDiagnosticsScopeRoot =
            scope != ZR_NULL && scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE
                    ? scope->root
                    : ZR_NULL;
    return ZR_TRUE;
}

/* 未物化返回成功的空视图；已物化只能按同一 root 读取，items 在重建或 context 释放后失效。 */
TZrBool ZrParser_SemanticQuery_Diagnostics(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticQueryDiagnostics *outDiagnostics) {
    const SZrAstNode *requestedScopeRoot;

    if (outDiagnostics == ZR_NULL) {
        return ZR_FALSE;
    }

    outDiagnostics->items = ZR_NULL;
    outDiagnostics->count = 0;
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_FALSE;
    }
    if (scope != ZR_NULL &&
        (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE &&
         (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE || scope->root == ZR_NULL))) {
        return ZR_FALSE;
    }
    if (!context->queryDiagnosticsMaterialized) {
        return ZR_TRUE;
    }

    requestedScopeRoot =
            scope != ZR_NULL && scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE
                    ? scope->root
                    : ZR_NULL;
    if (requestedScopeRoot != context->queryDiagnosticsScopeRoot) {
        return ZR_FALSE;
    }
    if (context->queryDiagnostics.length > 0) {
        outDiagnostics->items =
            (const SZrStructuredDiagnostic *)context->queryDiagnostics.head;
        outDiagnostics->count = context->queryDiagnostics.length;
    }
    return ZR_TRUE;
}
