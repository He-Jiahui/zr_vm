/** @file
 * @brief 保存与查询同一语义快照的事实投影，连接分析发布者与 compiler、LSP、debug/REPL 消费者。
 * @note 本层管理原生复制载荷；借用输入、内部 VM 文本的 GC 根及完整 OOM 保证分别受对应契约约束。
 */
#include "zr_vm_parser/semantic.h"

static TZrBool semantic_facts_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool semantic_facts_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool semantic_facts_same_string(SZrString *left, SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

/** @brief 为诊断去重比较同一来源中的同一范围，避免把不同源文件的坐标当作同一位置。 */
static TZrBool semantic_facts_same_range(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_facts_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }
    if (semantic_facts_has_offset(&left->start) ||
        semantic_facts_has_offset(&left->end) ||
        semantic_facts_has_offset(&right->start) ||
        semantic_facts_has_offset(&right->end)) {
        return (TZrBool)(left->start.offset == right->start.offset &&
                         left->end.offset == right->end.offset);
    }
    return (TZrBool)(left->start.line == right->start.line &&
                     left->start.column == right->start.column &&
                     left->end.line == right->end.line &&
                     left->end.column == right->end.column);
}

/** @brief 为位置查询统一以 position.start 作为光标，检查同源闭边界命中。
 * @note position.end 仅参与 offset 坐标模式的资格判断，不扩大查询区域；双方具备 offset 信息时使用 offset，否则回退到行列。
 */
static TZrBool semantic_facts_range_contains_position(const SZrFileRange *range,
                                                      const SZrFileRange *position) {
    TZrSize queryOffset;
    TZrInt32 queryLine;
    TZrInt32 queryColumn;

    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_facts_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }

    if ((semantic_facts_has_offset(&range->start) ||
         semantic_facts_has_offset(&range->end)) &&
        (semantic_facts_has_offset(&position->start) ||
         semantic_facts_has_offset(&position->end))) {
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

static TZrBool semantic_facts_range_starts_at_position(
        const SZrFileRange *range,
        const SZrFileRange *position) {
    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_facts_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }

    if ((semantic_facts_has_offset(&range->start) ||
         semantic_facts_has_offset(&range->end)) &&
        (semantic_facts_has_offset(&position->start) ||
         semantic_facts_has_offset(&position->end))) {
        return range->start.offset == position->start.offset;
    }

    return (TZrBool)(range->start.line == position->start.line &&
                     range->start.column == position->start.column);
}

/* 供多个位置候选选择较局部的投影；评分依赖 offset 跨度。
 * 命中检查虽可回退到行列，缺少 offset 的候选仍没有按实际文本跨度选取最局部投影的保障。
 */
static TZrSize semantic_facts_range_width(const SZrFileRange *range) {
    if (range == ZR_NULL) {
        return 0;
    }
    if (range->end.offset >= range->start.offset) {
        return range->end.offset - range->start.offset;
    }
    return 0;
}

static TZrBool semantic_facts_range_is_known(const SZrFileRange *range) {
    if (range == ZR_NULL) {
        return ZR_FALSE;
    }

    return range->source != ZR_NULL ||
           range->start.line != 0 ||
           range->start.column != 0 ||
           range->start.offset != 0 ||
           range->end.line != 0 ||
           range->end.column != 0 ||
           range->end.offset != 0;
}

/** @brief 在同位置、同跨度的多个引用角色重叠时，偏好写入、调用等更强的语义投影。
 * @note 此偏好只用于位置筛选后的角色取舍，不覆盖精确位置与局部范围的选择。
 */
static TZrInt32 semantic_facts_reference_priority(EZrSemanticReferenceKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_REFERENCE_WRITE:
        case ZR_SEMANTIC_REFERENCE_MEMBER_WRITE:
            return 4;
        case ZR_SEMANTIC_REFERENCE_CALL:
            return 3;
        case ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS:
            return 2;
        case ZR_SEMANTIC_REFERENCE_READ:
            return 1;
        case ZR_SEMANTIC_REFERENCE_DECLARATION:
        case ZR_SEMANTIC_REFERENCE_UNKNOWN:
        default:
            return 0;
    }
}

static TZrBool semantic_facts_reference_is_symbol_definition(const SZrSemanticReferenceFact *fact) {
    if (fact == ZR_NULL ||
        !fact->isResolved ||
        fact->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    return fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
           fact->kind == ZR_SEMANTIC_REFERENCE_WRITE;
}

/** @brief 为线性 reaching-definition 与 definite-assignment 解析建立声明/写入事实的自身定义种子。
 * @pre 引用已解析到非零 SymbolId，且角色是 DECLARATION 或 WRITE；借用的 source 与当前快照保持有效。
 */
static void semantic_facts_reference_set_own_definition(SZrSemanticReferenceFact *fact) {
    if (!semantic_facts_reference_is_symbol_definition(fact)) {
        return;
    }

    if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
        semantic_facts_range_is_known(&fact->declarationRange)) {
        fact->definitionRange = fact->declarationRange;
    } else {
        fact->definitionRange = fact->range;
    }
    fact->hasDefinitionRange = semantic_facts_range_is_known(&fact->definitionRange);
}

static void semantic_facts_reference_free_definition_ranges(SZrSemanticContext *context,
                                                            SZrSemanticReferenceFact *fact) {
    if (context == ZR_NULL || context->state == ZR_NULL || fact == ZR_NULL) {
        return;
    }

    if (fact->definitionRanges.isValid) {
        ZrCore_Array_Free(context->state, &fact->definitionRanges);
    }
    ZrCore_Array_Construct(&fact->definitionRanges);
}

/** @brief 为追加事实复制独立的多定义 native 容器；各 range.source 仍借用原来源。
 * @pre dst 尚未持有旧容器，src 容器与元素在复制期间有效。
 * @note 未初始化或空 src 视为空成功；TRUE 不构成底层分配失败已处理的保证。
 */
static TZrBool semantic_facts_reference_copy_definition_ranges(SZrSemanticContext *context,
                                                               SZrSemanticReferenceFact *dst,
                                                               const SZrSemanticReferenceFact *src) {
    TZrSize index;

    if (context == ZR_NULL || context->state == ZR_NULL || dst == ZR_NULL || src == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&dst->definitionRanges);
    if (!src->definitionRanges.isValid || src->definitionRanges.length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(context->state,
                      &dst->definitionRanges,
                      sizeof(SZrFileRange),
                      src->definitionRanges.length);
    for (index = 0; index < src->definitionRanges.length; index++) {
        const SZrFileRange *range =
                (const SZrFileRange *)ZrCore_Array_Get((SZrArray *)&src->definitionRanges, index);
        if (range != ZR_NULL) {
            SZrFileRange rangeCopy = *range;
            ZrCore_Array_Push(context->state, &dst->definitionRanges, &rangeCopy);
        }
    }

    return ZR_TRUE;
}

static void semantic_facts_reference_free_argument_mappings(
        SZrSemanticContext *context,
        SZrSemanticReferenceFact *fact) {
    if (context == ZR_NULL || context->state == ZR_NULL || fact == ZR_NULL) {
        return;
    }
    if (fact->argumentMappings.isValid) {
        ZrCore_Array_Free(context->state, &fact->argumentMappings);
    }
    ZrCore_Array_Construct(&fact->argumentMappings);
}

/** @brief 将 producer 的暂存调用参数映射复制到事实自有 native 容器。
 * @note 映射中的 ID/索引/tag 按值复制，range.source 借用；空源成功，非空仅以最终长度相等判断复制完成，不校验参数兼容性。
 */
static TZrBool semantic_facts_reference_copy_argument_mappings(
        SZrSemanticContext *context,
        SZrSemanticReferenceFact *dst,
        const SZrSemanticReferenceFact *src) {
    TZrSize index;

    if (context == ZR_NULL || context->state == ZR_NULL || dst == ZR_NULL ||
        src == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Construct(&dst->argumentMappings);
    if (!src->argumentMappings.isValid || src->argumentMappings.length == 0U) {
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      &dst->argumentMappings,
                      sizeof(SZrSemanticCallArgumentFact),
                      src->argumentMappings.length);
    for (index = 0U; index < src->argumentMappings.length; index++) {
        const SZrSemanticCallArgumentFact *mapping =
                (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
                        (SZrArray *)&src->argumentMappings, index);
        if (mapping != ZR_NULL) {
            SZrSemanticCallArgumentFact mappingCopy = *mapping;
            ZrCore_Array_Push(
                    context->state, &dst->argumentMappings, &mappingCopy);
        }
    }
    return dst->argumentMappings.length == src->argumentMappings.length;
}

/** @brief 为线性 reaching-definition 解析查找同符号的已发布前驱定义。
 * @note 这是发布顺序上的回退，不证明 CFG 支配关系；分支合流结果由后续 CFG 分析发布。
 */
static const SZrSemanticReferenceFact *semantic_facts_find_previous_definition(
        SZrSemanticContext *context,
        TZrSize beforeIndex,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL ||
        !context->referenceFacts.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }

    for (index = beforeIndex; index > 0; index--) {
        SZrSemanticReferenceFact *candidate =
            (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index - 1);
        if (candidate != ZR_NULL &&
            candidate->symbolId == symbolId &&
            semantic_facts_reference_is_symbol_definition(candidate) &&
            candidate->hasDefinitionRange) {
            return candidate;
        }
    }

    return ZR_NULL;
}

static TZrBool semantic_facts_reference_definite_assignment_source_state(
        const SZrSemanticReferenceFact *fact,
        EZrSemanticDefiniteAssignmentState *outState) {
    if (outState != ZR_NULL) {
        *outState = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN;
    }
    if (!semantic_facts_reference_is_symbol_definition(fact)) {
        return ZR_FALSE;
    }

    if (fact->kind == ZR_SEMANTIC_REFERENCE_WRITE) {
        if (outState != ZR_NULL) {
            *outState = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT;
        }
        return ZR_TRUE;
    }

    if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
        fact->hasDefiniteAssignmentState &&
        fact->definiteAssignmentState != ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN) {
        if (outState != ZR_NULL) {
            *outState = fact->definiteAssignmentState;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/** @brief 为线性 definite-assignment 解析读取同符号最近发布的赋值状态。
 * @note 最近状态即使 UNKNOWN 也遮蔽更早的已初始化状态；不跨分支推导 CFG 合流结果。
 */
static TZrBool semantic_facts_find_previous_definite_assignment_state(
        SZrSemanticContext *context,
        TZrSize beforeIndex,
        TZrSymbolId symbolId,
        EZrSemanticDefiniteAssignmentState *outState) {
    TZrSize index;

    if (outState != ZR_NULL) {
        *outState = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN;
    }
    if (context == ZR_NULL ||
        !context->referenceFacts.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (index = beforeIndex; index > 0; index--) {
        SZrSemanticReferenceFact *candidate =
            (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index - 1);
        if (candidate == ZR_NULL ||
            candidate->symbolId != symbolId ||
            !semantic_facts_reference_is_symbol_definition(candidate)) {
            continue;
        }

        return semantic_facts_reference_definite_assignment_source_state(candidate, outState);
    }

    return ZR_FALSE;
}

static void semantic_facts_free_expression_facts(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL || !context->expressionFacts.isValid) {
        return;
    }

    for (i = 0; i < context->expressionFacts.length; i++) {
        SZrSemanticExpressionFact *fact =
            (SZrSemanticExpressionFact *)ZrCore_Array_Get(&context->expressionFacts, i);
        if (fact != ZR_NULL) {
            ZrParser_InferredType_Free(context->state, &fact->inferredType);
        }
    }
}

static void semantic_facts_free_numeric_facts(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL || !context->numericFacts.isValid) {
        return;
    }

    for (i = 0; i < context->numericFacts.length; i++) {
        SZrSemanticNumericFact *fact =
            (SZrSemanticNumericFact *)ZrCore_Array_Get(&context->numericFacts, i);
        if (fact != ZR_NULL) {
            ZrParser_NumericRangeSegments_Free(context->state,
                                               &fact->rangeSegmentCount,
                                               fact->rangeSegments,
                                               &fact->rangeExtraSegments);
        }
    }
}

static void semantic_facts_free_ownership_intrinsic_facts(
        SZrSemanticContext *context) {
    if (context == ZR_NULL || !context->ownershipIntrinsicFacts.isValid) {
        return;
    }
    for (TZrSize index = 0u;
         index < context->ownershipIntrinsicFacts.length;
         index++) {
        SZrOwnershipIntrinsicFact *fact =
                (SZrOwnershipIntrinsicFact *)ZrCore_Array_Get(
                        &context->ownershipIntrinsicFacts, index);
        if (fact != ZR_NULL) {
            ZrParser_InferredType_Free(context->state, &fact->inputType);
            ZrParser_InferredType_Free(context->state, &fact->resultType);
        }
    }
}

static void semantic_facts_free_receiver_guard_facts(SZrSemanticContext *context) {
    if (context == ZR_NULL || !context->receiverGuardFacts.isValid) {
        return;
    }
    for (TZrSize index = 0u; index < context->receiverGuardFacts.length; index++) {
        SZrReceiverGuardFact *fact =
                (SZrReceiverGuardFact *)ZrCore_Array_Get(
                        &context->receiverGuardFacts, index);
        if (fact != ZR_NULL) {
            ZrParser_InferredType_Free(context->state, &fact->receiverType);
            ZrParser_InferredType_Free(context->state, &fact->guardedType);
        }
    }
}

/** @brief 以 context 的 VM state 重建事实使用的可选展示文本。
 * @note 原生事实保存返回地址不提供 GC 根；原输入有根也不能保住独立长串副本。
 */
/* BUG: [NATIVE_CONTEXT_INTERNAL_VM_CLONE_UNROOTED] 内部新建长串没有交接 GC 根。
 * 合法 AppendOwnership 在原输入已有根时仍可保存无根副本；默认 incremental GC、未安装补充 host trace 时，完整同线程 FullGC 可回收它，随后 REPL 所有权展示读取悬空对象。
 * 与原生 context URI 克隆共用根持有及 Reset/Free 释放问题；此链尚未动态复现。
 */
static SZrString *semantic_facts_clone_string(SZrSemanticContext *context, SZrString *value) {
    TZrNativeString text;

    if (context == ZR_NULL || context->state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(context->state, text, ZrCore_String_GetByteLength(value));
}

/** @brief 为事实发布准备统一的原生数组存储，供 context 的 Init/Reset/Free 生命周期管理。
 * @note void 初始化不向调用者传播分配失败，不能把数组有效标志当作缓冲申请成功证明。
 */
static void semantic_facts_init_array(SZrSemanticContext *context,
                                      SZrArray *array,
                                      TZrSize elementSize) {
    ZrCore_Array_Init(context->state,
                      array,
                      elementSize,
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

void ZrParser_SemanticFacts_Init(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    semantic_facts_init_array(context, &context->expressionFacts, sizeof(SZrSemanticExpressionFact));
    semantic_facts_init_array(context, &context->referenceFacts, sizeof(SZrSemanticReferenceFact));
    semantic_facts_init_array(context, &context->numericFacts, sizeof(SZrSemanticNumericFact));
    semantic_facts_init_array(context, &context->reachabilityFacts, sizeof(SZrSemanticReachabilityFact));
    semantic_facts_init_array(context, &context->logicalFacts, sizeof(SZrSemanticLogicalFact));
    semantic_facts_init_array(context, &context->ownershipFacts, sizeof(SZrSemanticOwnershipFact));
    semantic_facts_init_array(
            context,
            &context->ownershipIntrinsicFacts,
            sizeof(SZrOwnershipIntrinsicFact));
    semantic_facts_init_array(
            context, &context->receiverGuardFacts, sizeof(SZrReceiverGuardFact));
    semantic_facts_init_array(context, &context->diagnosticFacts, sizeof(SZrSemanticDiagnosticFact));
    ZrParser_SemanticRelations_Init(context);
}

void ZrParser_SemanticFacts_Reset(SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL) {
        return;
    }

    semantic_facts_free_expression_facts(context);
    if (context->expressionFacts.isValid) {
        context->expressionFacts.length = 0;
    }
    if (context->referenceFacts.isValid) {
        for (index = 0; index < context->referenceFacts.length; index++) {
            SZrSemanticReferenceFact *fact =
                    (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index);
            semantic_facts_reference_free_definition_ranges(context, fact);
            semantic_facts_reference_free_argument_mappings(context, fact);
        }
        context->referenceFacts.length = 0;
    }
    if (context->numericFacts.isValid) {
        semantic_facts_free_numeric_facts(context);
        context->numericFacts.length = 0;
    }
    if (context->reachabilityFacts.isValid) {
        context->reachabilityFacts.length = 0;
    }
    if (context->logicalFacts.isValid) {
        context->logicalFacts.length = 0;
    }
    if (context->ownershipFacts.isValid) {
        context->ownershipFacts.length = 0;
    }
    if (context->ownershipIntrinsicFacts.isValid) {
        semantic_facts_free_ownership_intrinsic_facts(context);
        context->ownershipIntrinsicFacts.length = 0;
    }
    if (context->receiverGuardFacts.isValid) {
        semantic_facts_free_receiver_guard_facts(context);
        context->receiverGuardFacts.length = 0;
    }
    if (context->diagnosticFacts.isValid) {
        for (index = 0U; index < context->diagnosticFacts.length; index++) {
            SZrSemanticDiagnosticFact *fact =
                    (SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                            &context->diagnosticFacts, index);
            if (fact != ZR_NULL) {
                ZrParser_StructuredDiagnostic_Free(
                        context->state, &fact->diagnostic);
            }
        }
        context->diagnosticFacts.length = 0;
    }
    ZrParser_SemanticRelations_Reset(context);
}

void ZrParser_SemanticFacts_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    ZrParser_SemanticFacts_Reset(context);
    ZrCore_Array_Free(context->state, &context->expressionFacts);
    ZrCore_Array_Free(context->state, &context->referenceFacts);
    ZrCore_Array_Free(context->state, &context->numericFacts);
    ZrCore_Array_Free(context->state, &context->reachabilityFacts);
    ZrCore_Array_Free(context->state, &context->logicalFacts);
    ZrCore_Array_Free(context->state, &context->ownershipFacts);
    ZrCore_Array_Free(context->state, &context->ownershipIntrinsicFacts);
    ZrCore_Array_Free(context->state, &context->receiverGuardFacts);
    ZrCore_Array_Free(context->state, &context->diagnosticFacts);
    ZrParser_SemanticRelations_Free(context);
}

TZrBool ZrParser_SemanticFacts_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticDiagnosticFact *fact) {
    SZrSemanticDiagnosticFact copy;
    TZrSize index;

    if (context == ZR_NULL || fact == ZR_NULL ||
        !context->diagnosticFacts.isValid ||
        fact->diagnostic.code == ZR_NULL ||
        fact->diagnostic.message == ZR_NULL ||
        ((!fact->diagnostic.fixes.isValid ||
          fact->diagnostic.fixes.length == 0U) &&
         fact->diagnostic.noFixReason ==
                 ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->diagnosticFacts.length; index++) {
        const SZrSemanticDiagnosticFact *existing =
                (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                        &context->diagnosticFacts, index);
        if (existing != ZR_NULL &&
            semantic_facts_same_range(
                    &existing->diagnostic.location,
                    &fact->diagnostic.location) &&
            semantic_facts_same_string(
                    existing->diagnostic.code, fact->diagnostic.code) &&
            semantic_facts_same_string(
                    existing->diagnostic.message, fact->diagnostic.message)) {
            return ZR_TRUE;
        }
    }

    memset(&copy, 0, sizeof(copy));
    copy.node = fact->node;
    if (!ZrParser_StructuredDiagnostic_Copy(
                context->state, &copy.diagnostic, &fact->diagnostic)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->diagnosticFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendExpression(SZrSemanticContext *context,
                                                const SZrSemanticExpressionFact *fact) {
    SZrSemanticExpressionFact copy;
    TZrSize index;

    if (context == ZR_NULL || fact == ZR_NULL || !context->expressionFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrParser_InferredType_Copy(context->state, &copy.inferredType, &fact->inferredType);
    if (copy.typeId == ZR_SEMANTIC_ID_INVALID) {
        copy.typeId = ZrParser_CanonicalType_FromInferred(context, &copy.inferredType);
    }
    copy.callTargetName = semantic_facts_clone_string(context, fact->callTargetName);
    copy.memberName = semantic_facts_clone_string(context, fact->memberName);
    copy.diagnosticMessage = semantic_facts_clone_string(context, fact->diagnosticMessage);
    copy.diagnosticCode = semantic_facts_clone_string(context, fact->diagnosticCode);

    if (fact->node != ZR_NULL) {
        for (index = 0; index < context->expressionFacts.length; index++) {
            SZrSemanticExpressionFact *existing =
                (SZrSemanticExpressionFact *)ZrCore_Array_Get(
                    &context->expressionFacts, index);
            if (existing != ZR_NULL && existing->node == fact->node) {
                ZrParser_InferredType_Free(context->state, &existing->inferredType);
                *existing = copy;
                return ZR_TRUE;
            }
        }
    }

    ZrCore_Array_Push(context->state, &context->expressionFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendReference(SZrSemanticContext *context,
                                               const SZrSemanticReferenceFact *fact) {
    SZrSemanticReferenceFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    copy.signatureDisplay = semantic_facts_clone_string(context, fact->signatureDisplay);
    copy.externalOwnerIdentity =
            semantic_facts_clone_string(context, fact->externalOwnerIdentity);
    if (!semantic_facts_reference_copy_definition_ranges(context, &copy, fact)) {
        return ZR_FALSE;
    }
    if (!semantic_facts_reference_copy_argument_mappings(context, &copy, fact)) {
        semantic_facts_reference_free_definition_ranges(context, &copy);
        semantic_facts_reference_free_argument_mappings(context, &copy);
        return ZR_FALSE;
    }
    semantic_facts_reference_set_own_definition(&copy);
    ZrCore_Array_Push(context->state, &context->referenceFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    for (index = 0; index < context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
            (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index);
        if (fact == ZR_NULL) {
            continue;
        }

        semantic_facts_reference_free_definition_ranges(context, fact);
        semantic_facts_reference_set_own_definition(fact);

        if (fact->kind == ZR_SEMANTIC_REFERENCE_READ &&
            fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
            const SZrSemanticReferenceFact *definition =
                semantic_facts_find_previous_definition(context, index, fact->symbolId);
            if (definition != ZR_NULL && definition->hasDefinitionRange) {
                fact->definitionRange = definition->definitionRange;
                fact->hasDefinitionRange = ZR_TRUE;
            }
        }
    }

    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    for (index = 0; index < context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
            (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index);
        EZrSemanticDefiniteAssignmentState state = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN;

        if (fact == ZR_NULL) {
            continue;
        }

        if (fact->kind == ZR_SEMANTIC_REFERENCE_WRITE &&
            semantic_facts_reference_definite_assignment_source_state(fact, &state)) {
            fact->definiteAssignmentState = state;
            fact->hasDefiniteAssignmentState = ZR_TRUE;
            continue;
        }

        if (fact->kind == ZR_SEMANTIC_REFERENCE_READ &&
            fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
            if (semantic_facts_find_previous_definite_assignment_state(context,
                                                                       index,
                                                                       fact->symbolId,
                                                                       &state)) {
                fact->definiteAssignmentState = state;
                fact->hasDefiniteAssignmentState = ZR_TRUE;
            } else {
                fact->definiteAssignmentState = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN;
                fact->hasDefiniteAssignmentState = ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendNumeric(SZrSemanticContext *context,
                                             const SZrSemanticNumericFact *fact) {
    SZrSemanticNumericFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->numericFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrCore_Array_Construct(&copy.rangeExtraSegments);
    if (!ZrParser_NumericRangeSegments_Copy(context->state,
                                            &copy.rangeSegmentCount,
                                            copy.rangeSegments,
                                            &copy.rangeExtraSegments,
                                            fact->rangeSegmentCount,
                                            fact->rangeSegments,
                                            &fact->rangeExtraSegments)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->numericFacts, &copy);
    return ZR_TRUE;
}

const SZrNumericRangeSegment *ZrParser_SemanticNumericFact_RangeSegmentAt(
        const SZrSemanticNumericFact *fact,
        TZrSize index) {
    if (fact == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_NumericRangeSegments_At(fact->rangeSegmentCount,
                                            fact->rangeSegments,
                                            &fact->rangeExtraSegments,
                                            index);
}

TZrBool ZrParser_SemanticFacts_AppendReachability(SZrSemanticContext *context,
                                                  const SZrSemanticReachabilityFact *fact) {
    SZrSemanticReachabilityFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->reachabilityFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrCore_Array_Push(context->state, &context->reachabilityFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendLogical(SZrSemanticContext *context,
                                             const SZrSemanticLogicalFact *fact) {
    SZrSemanticLogicalFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->logicalFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    ZrCore_Array_Push(context->state, &context->logicalFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendOwnership(SZrSemanticContext *context,
                                               const SZrSemanticOwnershipFact *fact) {
    SZrSemanticOwnershipFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_FALSE;
    }

    copy = *fact;
    if (fact->diagnosticMessage != ZR_NULL) {
        copy.diagnosticMessage = semantic_facts_clone_string(context, fact->diagnosticMessage);
        if (copy.diagnosticMessage == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    ZrCore_Array_Push(context->state, &context->ownershipFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendOwnershipIntrinsic(
        SZrSemanticContext *context,
        const SZrOwnershipIntrinsicFact *fact) {
    SZrOwnershipIntrinsicFact copy;

    if (context == ZR_NULL || fact == ZR_NULL ||
        !context->ownershipIntrinsicFacts.isValid) {
        return ZR_FALSE;
    }
    copy = *fact;
    ZrParser_InferredType_Copy(context->state, &copy.inputType, &fact->inputType);
    ZrParser_InferredType_Copy(context->state, &copy.resultType, &fact->resultType);
    ZrCore_Array_Push(context->state, &context->ownershipIntrinsicFacts, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFacts_AppendReceiverGuard(
        SZrSemanticContext *context,
        const SZrReceiverGuardFact *fact) {
    SZrReceiverGuardFact copy;

    if (context == ZR_NULL || fact == ZR_NULL ||
        !context->receiverGuardFacts.isValid) {
        return ZR_FALSE;
    }
    copy = *fact;
    ZrParser_InferredType_Copy(context->state, &copy.receiverType, &fact->receiverType);
    ZrParser_InferredType_Copy(context->state, &copy.guardedType, &fact->guardedType);
    ZrCore_Array_Push(context->state, &context->receiverGuardFacts, &copy);
    return ZR_TRUE;
}

const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->expressionFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->expressionFacts.length; i++) {
        const SZrSemanticExpressionFact *fact =
            (const SZrSemanticExpressionFact *)ZrCore_Array_Get((SZrArray *)&context->expressionFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticExpressionFact *ZrParser_SemanticFacts_FindExpressionAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    const SZrSemanticExpressionFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (context == ZR_NULL || !context->expressionFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->expressionFacts.length; i++) {
        const SZrSemanticExpressionFact *fact =
            (const SZrSemanticExpressionFact *)ZrCore_Array_Get((SZrArray *)&context->expressionFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            if (best == ZR_NULL || width <= bestWidth) {
                best = fact;
                bestWidth = width;
            }
        }
    }
    return best;
}

const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    const SZrSemanticReferenceFact *best = ZR_NULL;
    TZrSize bestWidth = 0;
    TZrInt32 bestPriority = 0;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            TZrInt32 priority = semantic_facts_reference_priority(fact->kind);
            TZrBool startsAtPosition =
                    semantic_facts_range_starts_at_position(&fact->range, &position);
            TZrBool bestStartsAtPosition =
                    best != ZR_NULL &&
                    semantic_facts_range_starts_at_position(&best->range, &position);
            if (best == ZR_NULL ||
                (startsAtPosition && !bestStartsAtPosition) ||
                (startsAtPosition == bestStartsAtPosition &&
                 (width < bestWidth ||
                  (width == bestWidth && priority > bestPriority)))) {
                best = fact;
                bestWidth = width;
                bestPriority = priority;
            }
        }
    }
    return best;
}

const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
        const SZrSemanticContext *context,
        SZrFileRange position,
        EZrSemanticReferenceKind kind) {
    TZrSize i;
    const SZrSemanticReferenceFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL &&
            fact->kind == kind &&
            semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            if (best == ZR_NULL || width < bestWidth) {
                best = fact;
                bestWidth = width;
            }
        }
    }
    return best;
}

const SZrSemanticReferenceFact *ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        EZrSemanticReferenceKind kind) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->referenceFacts.length; i++) {
        const SZrSemanticReferenceFact *fact =
            (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, i);
        if (fact != ZR_NULL && fact->node == node && fact->kind == kind) {
            return fact;
        }
    }
    return ZR_NULL;
}

/** @brief 为同一节点的既有数值投影比较范围信息量，供查询选择展示候选。
 * @note 仅度量 signed 范围，unsigned 信息由候选选择另行解释；无有效范围用最大宽度表示，评分覆盖完整 Int64 边界。
 */
static TZrUInt64 semantic_facts_numeric_range_width(const SZrSemanticNumericFact *fact) {
    TZrUInt64 minMagnitude;
    TZrUInt64 maxMagnitude;

    if (fact == ZR_NULL || !fact->hasRange) {
        return UINT64_MAX;
    }
    if (fact->maxValue < fact->minValue) {
        return UINT64_MAX;
    }
    if (fact->minValue >= 0) {
        return (TZrUInt64)fact->maxValue - (TZrUInt64)fact->minValue;
    }

    minMagnitude = (TZrUInt64)(-(fact->minValue + 1)) + 1u;
    if (fact->maxValue < 0) {
        maxMagnitude = (TZrUInt64)(-(fact->maxValue + 1)) + 1u;
        return minMagnitude - maxMagnitude;
    }
    return minMagnitude + (TZrUInt64)fact->maxValue;
}

/** @brief 按投影的信息量为节点查询选取一条已经发布的数值事实。
 * @note 不合并候选范围或汇总风险；只有其它信息评分均相同时才优先 mayOverflow 为真的候选，完全平局保留前项。具体信息评分门槛见公共数值查询契约。
 */
static TZrBool semantic_facts_numeric_candidate_is_better(
        const SZrSemanticNumericFact *candidate,
        const SZrSemanticNumericFact *best) {
    TZrUInt64 candidateWidth;
    TZrUInt64 bestWidth;

    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    if (best == ZR_NULL) {
        return ZR_TRUE;
    }
    if (candidate->exactness != best->exactness) {
        return candidate->exactness > best->exactness;
    }
    if ((candidate->rangeSegmentCount > 0) != (best->rangeSegmentCount > 0)) {
        return candidate->rangeSegmentCount > 0;
    }
    if (candidate->rangeSegmentCount != best->rangeSegmentCount) {
        return candidate->rangeSegmentCount > best->rangeSegmentCount;
    }
    if (candidate->hasRange != best->hasRange) {
        return candidate->hasRange;
    }
    candidateWidth = semantic_facts_numeric_range_width(candidate);
    bestWidth = semantic_facts_numeric_range_width(best);
    if (candidateWidth != bestWidth) {
        return candidateWidth < bestWidth;
    }
    if (candidate->hasUnsignedRange != best->hasUnsignedRange) {
        return candidate->hasUnsignedRange;
    }
    if (candidate->mayOverflow != best->mayOverflow) {
        return candidate->mayOverflow;
    }
    return ZR_FALSE;
}

const SZrSemanticNumericFact *ZrParser_SemanticFacts_FindNumericByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;
    const SZrSemanticNumericFact *best = ZR_NULL;

    if (context == ZR_NULL || node == ZR_NULL || !context->numericFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->numericFacts.length; i++) {
        const SZrSemanticNumericFact *fact =
            (const SZrSemanticNumericFact *)ZrCore_Array_Get((SZrArray *)&context->numericFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            if (semantic_facts_numeric_candidate_is_better(fact, best)) {
                best = fact;
            }
        }
    }
    return best;
}

/** @brief 优先采用控制转移之后死代码的原因，帮助诊断定位使后续代码不可达的转移。
 * @note 这类原因描述转移后的不可达代码，不表示转移语句本身不可达；此取舍不负责选择最局部范围。
 */
static TZrInt32 semantic_facts_reachability_priority(EZrSemanticReachabilityCause cause) {
    switch (cause) {
        case ZR_SEMANTIC_REACHABILITY_AFTER_RETURN:
        case ZR_SEMANTIC_REACHABILITY_AFTER_THROW:
        case ZR_SEMANTIC_REACHABILITY_AFTER_BREAK:
        case ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE:
            return 1;
        default:
            return 0;
    }
}

const SZrSemanticReachabilityFact *ZrParser_SemanticFacts_FindReachabilityAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    const SZrSemanticReachabilityFact *best = ZR_NULL;
    TZrInt32 bestPriority = 0;

    if (context == ZR_NULL || !context->reachabilityFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->reachabilityFacts.length; i++) {
        const SZrSemanticReachabilityFact *fact =
            (const SZrSemanticReachabilityFact *)ZrCore_Array_Get((SZrArray *)&context->reachabilityFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrInt32 priority = semantic_facts_reachability_priority(fact->cause);
            if (best == ZR_NULL || priority > bestPriority) {
                best = fact;
                bestPriority = priority;
            }
        }
    }
    return best;
}

const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->logicalFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->logicalFacts.length; i++) {
        const SZrSemanticLogicalFact *fact =
            (const SZrSemanticLogicalFact *)ZrCore_Array_Get((SZrArray *)&context->logicalFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticLogicalFact *ZrParser_SemanticFacts_FindLogicalAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    const SZrSemanticLogicalFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (context == ZR_NULL || !context->logicalFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->logicalFacts.length; i++) {
        const SZrSemanticLogicalFact *fact =
            (const SZrSemanticLogicalFact *)ZrCore_Array_Get((SZrArray *)&context->logicalFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            if (best == ZR_NULL || width <= bestWidth) {
                best = fact;
                bestWidth = width;
            }
        }
    }
    return best;
}

const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize i;

    if (context == ZR_NULL || node == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->ownershipFacts.length; i++) {
        const SZrSemanticOwnershipFact *fact =
            (const SZrSemanticOwnershipFact *)ZrCore_Array_Get((SZrArray *)&context->ownershipFacts, i);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrSemanticOwnershipFact *ZrParser_SemanticFacts_FindOwnershipAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    TZrSize i;
    TZrSize bestWidth = 0U;
    const SZrSemanticOwnershipFact *best = ZR_NULL;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }

    for (i = 0; i < context->ownershipFacts.length; i++) {
        const SZrSemanticOwnershipFact *fact =
            (const SZrSemanticOwnershipFact *)ZrCore_Array_Get((SZrArray *)&context->ownershipFacts, i);
        if (fact != ZR_NULL && semantic_facts_range_contains_position(&fact->range, &position)) {
            TZrSize width = semantic_facts_range_width(&fact->range);
            if (best == ZR_NULL || width < bestWidth ||
                (width == bestWidth && fact->isViolation && !best->isViolation)) {
                best = fact;
                bestWidth = width;
            }
        }
    }
    return best;
}

const SZrOwnershipIntrinsicFact *ZrParser_SemanticFacts_FindOwnershipIntrinsicByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL ||
        !context->ownershipIntrinsicFacts.isValid) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u;
         index < context->ownershipIntrinsicFacts.length;
         index++) {
        const SZrOwnershipIntrinsicFact *fact =
                (const SZrOwnershipIntrinsicFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipIntrinsicFacts, index);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrOwnershipIntrinsicFact *ZrParser_SemanticFacts_FindOwnershipIntrinsicAtPosition(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    const SZrOwnershipIntrinsicFact *best = ZR_NULL;
    TZrSize bestWidth = 0u;

    if (context == ZR_NULL || !context->ownershipIntrinsicFacts.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0u;
         index < context->ownershipIntrinsicFacts.length;
         index++) {
        const SZrOwnershipIntrinsicFact *fact =
                (const SZrOwnershipIntrinsicFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipIntrinsicFacts, index);
        const SZrFileRange *candidateRange;
        TZrSize width;

        if (fact == ZR_NULL ||
            !semantic_facts_range_contains_position(&fact->range, &position)) {
            continue;
        }

        candidateRange = semantic_facts_range_contains_position(
                                 &fact->nameRange, &position)
                             ? &fact->nameRange
                             : &fact->range;
        width = semantic_facts_range_width(candidateRange);
        if (best == ZR_NULL || width < bestWidth) {
            best = fact;
            bestWidth = width;
        }
    }

    return best;
}

const SZrReceiverGuardFact *ZrParser_SemanticFacts_FindReceiverGuardByNode(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    if (context == ZR_NULL || node == ZR_NULL ||
        !context->receiverGuardFacts.isValid) {
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < context->receiverGuardFacts.length; index++) {
        const SZrReceiverGuardFact *fact =
                (const SZrReceiverGuardFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->receiverGuardFacts, index);
        if (fact != ZR_NULL && fact->node == node) {
            return fact;
        }
    }
    return ZR_NULL;
}
