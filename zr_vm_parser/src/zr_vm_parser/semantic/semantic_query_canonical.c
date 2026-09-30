#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"

// 源码身份先比较字符串对象，再比较非空内容；两侧均缺省才会因指针相同而相等。
static TZrBool canonical_query_same_source(SZrString *left, SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

// 统一可缺省源码键的精确比较入口，不做路径归一化或 basename 匹配。
static TZrBool canonical_query_same_optional_source_exact(
        SZrString *left,
        SZrString *right) {
    return canonical_query_same_source(left, right);
}

// 范围比较要求来源相同；存在 offset 坐标时用字节区间，否则回退到行列坐标。
static TZrBool canonical_query_contains(const SZrFileRange *range,
                                        const SZrFileRange *position) {
    if (range == ZR_NULL || position == ZR_NULL ||
        !canonical_query_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }
    if ((range->start.offset > 0u || range->end.offset > 0u) &&
        (position->start.offset > 0u || position->end.offset > 0u)) {
        return (TZrBool)(range->start.offset <= position->start.offset &&
                         position->end.offset <= range->end.offset);
    }
    return (TZrBool)((range->start.line < position->start.line ||
                      (range->start.line == position->start.line &&
                       range->start.column <= position->start.column)) &&
                     (position->end.line < range->end.line ||
                      (position->end.line == range->end.line &&
                       position->end.column <= range->end.column)));
}

// 模块范围或空 scope 不限节点；节点范围只接受完全落在 root AST 范围内的事实。
static TZrBool canonical_query_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrFileRange *range) {
    return (TZrBool)(scope == ZR_NULL ||
                     scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE ||
                     (scope->root != ZR_NULL && canonical_query_contains(&scope->root->location, range)));
}

// 用字节跨度选择最窄候选；反向 offset 范围返回最大宽度，排在有效候选之后。
static TZrSize canonical_query_width(const SZrFileRange *range) {
    if (range->end.offset >= range->start.offset) {
        return range->end.offset - range->start.offset;
    }
    return ZR_MAX_SIZE;
}

// 精确范围相等同时约束来源、起止 offset 与起止行列，供候选冲突检测使用。
static TZrBool canonical_query_ranges_equal(const SZrFileRange *left,
                                             const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !canonical_query_same_optional_source_exact(
                left->source, right->source)) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->start.offset == right->start.offset &&
                     left->start.line == right->start.line &&
                     left->start.column == right->start.column &&
                     left->end.offset == right->end.offset &&
                     left->end.line == right->end.line &&
                     left->end.column == right->end.column);
}

// 可选文本按对象或内容比较；两个空值相等，单侧缺失不相等。
static TZrBool canonical_query_optional_strings_equal(
        SZrString *left,
        SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

// 比较调用事实的全部身份字段；同宽但字段不同的事实不能任意择一。
static TZrBool canonical_query_call_expressions_equal(
        const SZrSemanticExpressionFact *left,
        const SZrSemanticExpressionFact *right) {
    return (TZrBool)(left != ZR_NULL && right != ZR_NULL &&
                     left->kind == right->kind &&
                     left->exactness == right->exactness &&
                     canonical_query_ranges_equal(&left->range, &right->range) &&
                     canonical_query_ranges_equal(
                             &left->callTargetRange, &right->callTargetRange) &&
                     canonical_query_optional_strings_equal(
                             left->callTargetName, right->callTargetName) &&
                     left->typeId == right->typeId &&
                     left->argumentCount == right->argumentCount &&
                     left->hasNamedArguments == right->hasNamedArguments &&
                     left->isMemberCall == right->isMemberCall);
}

// 只有解析标志与稳定 symbol id 同时有效，调用引用才算已解析到目标。
static TZrBool canonical_query_call_reference_has_resolved_target(
        const SZrSemanticReferenceFact *reference) {
    return (TZrBool)(reference->isResolved &&
                     reference->symbolId != ZR_SEMANTIC_ID_INVALID);
}

// 给调用引用排序：解析目标权重最高，其余分数表示映射、声明、签名和接收者信息。
static TZrSize canonical_query_call_reference_completeness(
        const SZrSemanticReferenceFact *reference) {
    TZrSize completeness = 0u;

    if (canonical_query_call_reference_has_resolved_target(reference)) {
        completeness += 8u;
    }
    if (reference->argumentMappings.isValid &&
        reference->argumentMappings.length > 0U) {
        completeness += 1u;
    }
    if (reference->declarationRange.source != ZR_NULL ||
        reference->declarationRange.start.offset > 0u ||
        reference->declarationRange.end.offset > 0u ||
        reference->declarationRange.start.line > 0 ||
        reference->declarationRange.end.line > 0 ||
        reference->declarationRange.start.column > 0 ||
        reference->declarationRange.end.column > 0) {
        completeness += 4u;
    }
    if (reference->signatureDisplay != ZR_NULL) {
        completeness += 2u;
    }
    if (reference->receiverTypeId != ZR_SEMANTIC_ID_INVALID) {
        completeness += 1u;
    }
    return completeness;
}

// 参数类型必须精确相同；非值传递合同另允许引用参数的 pointee 与推断类型相同。
static TZrBool canonical_query_call_argument_parameter_type_matches(
        const SZrSemanticContext *context,
        const SZrCanonicalParameterContract *contract,
        TZrTypeId parameterTypeId) {
    const SZrCanonicalTypeNode *contractType;

    if (context == ZR_NULL || contract == ZR_NULL ||
        parameterTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (parameterTypeId == contract->typeId) {
        return ZR_TRUE;
    }
    if (contract->passingForm == ZR_CANONICAL_PASSING_VALUE) {
        return ZR_FALSE;
    }
    contractType = ZrParser_CanonicalType_Find(context, contract->typeId);
    return (TZrBool)(contractType != ZR_NULL &&
                     contractType->kind == ZR_CANONICAL_TYPE_REF &&
                     contractType->data.refType.pointeeTypeId == parameterTypeId);
}

// 将规范参数的 passingForm 映射到推断阶段的 passing mode，未知形式拒绝投影。
static TZrBool canonical_query_call_argument_passing_matches(
        const SZrCanonicalParameterContract *contract,
        EZrParameterPassingMode passingMode) {
    if (contract == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (contract->passingForm) {
        case ZR_CANONICAL_PASSING_VALUE:
            return passingMode == ZR_PARAMETER_PASSING_MODE_VALUE;
        case ZR_CANONICAL_PASSING_IN:
            return passingMode == ZR_PARAMETER_PASSING_MODE_IN;
        case ZR_CANONICAL_PASSING_REF:
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return passingMode == ZR_PARAMETER_PASSING_MODE_REF;
        case ZR_CANONICAL_PASSING_OUT:
            return passingMode == ZR_PARAMETER_PASSING_MODE_OUT;
        default:
            return ZR_FALSE;
    }
}

// 每个实参映射必须占用唯一形参；扫描当前映射之前的条目即可发现重复。
static TZrBool canonical_query_call_argument_parameter_is_unique(
        const SZrArray *mappings,
        TZrSize mappingIndex,
        TZrSize parameterIndex) {
    for (TZrSize priorIndex = 0U; priorIndex < mappingIndex; priorIndex++) {
        const SZrSemanticCallArgumentFact *prior =
                (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
                        (SZrArray *)mappings, priorIndex);
        if (prior == ZR_NULL || prior->parameterIndex == parameterIndex) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

// 校验调用映射的数组形状、规范函数、索引/转换、形参类型与传递方式及实参范围。
static TZrBool canonical_query_call_argument_mappings_valid(
        const SZrSemanticContext *context,
        const SZrSemanticExpressionFact *expression,
        const SZrSemanticReferenceFact *reference) {
    const SZrCanonicalTypeNode *callableType;
    TZrSize index;

    if (context == ZR_NULL || expression == ZR_NULL || reference == ZR_NULL ||
        !reference->argumentMappings.isValid ||
        reference->argumentMappings.length == 0U) {
        return ZR_TRUE;
    }
    if (reference->argumentMappings.length != expression->argumentCount) {
        return ZR_FALSE;
    }
    callableType = ZrParser_CanonicalType_Find(context, reference->typeId);
    if (callableType == ZR_NULL ||
        callableType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZR_FALSE;
    }
    for (index = 0U; index < reference->argumentMappings.length; index++) {
        const SZrSemanticCallArgumentFact *mapping =
                (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
                        (SZrArray *)&reference->argumentMappings, index);
        const SZrCanonicalParameterContract *parameterContract =
                mapping != ZR_NULL &&
                                mapping->parameterIndex <
                                        callableType->data.function.parameterContracts.length
                        ? (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                                  (SZrArray *)&callableType->data.function.parameterContracts,
                                  mapping->parameterIndex)
                        : ZR_NULL;
        if (mapping == ZR_NULL || mapping->argumentIndex != index ||
            mapping->argumentTypeId == ZR_SEMANTIC_ID_INVALID ||
            mapping->parameterTypeId == ZR_SEMANTIC_ID_INVALID ||
            (mapping->conversion != ZR_SEMANTIC_CALL_CONVERSION_EXACT &&
             mapping->conversion != ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT) ||
            !canonical_query_call_argument_parameter_is_unique(
                    &reference->argumentMappings,
                    index,
                    mapping->parameterIndex) ||
            ZrParser_CanonicalType_Find(context, mapping->argumentTypeId) == ZR_NULL ||
            !canonical_query_call_argument_parameter_type_matches(
                    context, parameterContract, mapping->parameterTypeId) ||
            !canonical_query_call_argument_passing_matches(
                    parameterContract, mapping->passingMode) ||
            ((mapping->argumentTypeId == mapping->parameterTypeId) !=
             (mapping->conversion == ZR_SEMANTIC_CALL_CONVERSION_EXACT)) ||
            !canonical_query_same_optional_source_exact(
                    expression->range.source, mapping->argumentRange.source) ||
            !canonical_query_contains(
                    &expression->range, &mapping->argumentRange)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

// 只接纳目标范围内、来源一致且具有规范函数类型的 CALL 引用事实。
static TZrBool canonical_query_call_reference_is_candidate(
        const SZrSemanticContext *context,
        const SZrFileRange *callTargetRange,
        const SZrSemanticReferenceFact *reference) {
    const SZrCanonicalTypeNode *callableType;

    if (reference == ZR_NULL || reference->kind != ZR_SEMANTIC_REFERENCE_CALL ||
        reference->typeId == ZR_SEMANTIC_ID_INVALID ||
        !canonical_query_same_optional_source_exact(
                callTargetRange->source, reference->range.source) ||
        !canonical_query_contains(callTargetRange, &reference->range)) {
        return ZR_FALSE;
    }
    callableType = ZrParser_CanonicalType_Find(context, reference->typeId);
    return (TZrBool)(callableType != ZR_NULL &&
                     callableType->kind == ZR_CANONICAL_TYPE_FUNCTION);
}

/**
 * @brief 按位置优先投影引用事实的规范 TypeId，必要时回退到允许投影的表达式事实。
 * @pre context 与 scope 来自同一未变更语义快照。
 * @note 节点 scope 必须提供覆盖查询位置及候选事实的 root 范围。
 * @post outQuery 先清零；返回的 reference 与 expression 是快照借用指针。
 * @return 找到有效引用类型或可投影表达式类型时返回 true；失败时仍可能保留观察到的事实指针。
 */
TZrBool ZrParser_SemanticQuery_CanonicalTypeAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticTypeQuery *outQuery) {
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticExpressionFact *expression;

    if (outQuery != ZR_NULL) memset(outQuery, 0, sizeof(*outQuery));
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        !canonical_query_scope_allows(scope, &position)) {
        return ZR_FALSE;
    }
    reference = ZrParser_SemanticFacts_FindReferenceAtPosition(context, position);
    expression = ZrParser_SemanticFacts_FindExpressionAtPosition(context, position);
    outQuery->reference = reference;
    outQuery->expression = expression;
    if (reference != ZR_NULL && reference->typeId != ZR_SEMANTIC_ID_INVALID &&
        canonical_query_scope_allows(scope, &reference->range)) {
        outQuery->typeId = reference->typeId;
        return ZR_TRUE;
    }
    if (expression != ZR_NULL && expression->typeId != ZR_SEMANTIC_ID_INVALID &&
        ZrParser_SemanticQuery_ExactnessAllowsProjection(expression->exactness) &&
        canonical_query_scope_allows(scope, &expression->range)) {
        outQuery->typeId = expression->typeId;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

/**
 * @brief 在给定 scope 中查找符号的已解析声明引用，并选择最窄声明范围。
 * @pre context 中的 referenceFacts 与 scope 必须属于同一未变更语义快照。
 * @return 返回快照借用的声明事实；缺少有效 symbol id、事实数组或匹配声明时返回 NULL。
 */
const SZrSemanticReferenceFact *ZrParser_SemanticQuery_DeclarationOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope) {
    const SZrSemanticReferenceFact *best = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->referenceFacts.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        TZrSize width;

        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !fact->isResolved || fact->symbolId != symbolId ||
            !canonical_query_scope_allows(scope, &fact->range)) {
            continue;
        }

        width = canonical_query_width(&fact->range);
        if (best == ZR_NULL || width < bestWidth) {
            best = fact;
            bestWidth = width;
        }
    }

    return best;
}

/**
 * @brief 在位置与 scope 中挑选唯一最窄调用表达式，再关联完整度最高的目标引用。
 * @pre context、scope 和 AST/semantic facts 属于同一未变更快照。
 * @post outQuery 先清零；expression、reference 与 argumentMappings 都借用该快照。
 * @return 找到无冲突且映射自洽的调用时返回 true；目标可未解析，调用方须检查 hasResolvedTarget。
 */
TZrBool ZrParser_SemanticQuery_CallAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticCallQuery *outQuery) {
    const SZrSemanticExpressionFact *best = ZR_NULL;
    const SZrSemanticReferenceFact *bestReference = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;
    TZrSize bestReferenceCompleteness = 0u;
    TZrBool bestIsConflicting = ZR_FALSE;
    TZrSize index;

    if (outQuery != ZR_NULL) memset(outQuery, 0, sizeof(*outQuery));
    if (context == ZR_NULL || outQuery == ZR_NULL ||
        !context->expressionFacts.isValid || !context->referenceFacts.isValid ||
        !canonical_query_scope_allows(scope, &position)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < context->expressionFacts.length; ++index) {
        const SZrSemanticExpressionFact *fact =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->expressionFacts, index);
        TZrBool factIsExact;
        TZrBool bestIsExact;
        TZrSize width;
        if (fact == ZR_NULL || !fact->hasCallInfo ||
            !canonical_query_same_optional_source_exact(
                    fact->range.source, position.source) ||
            !canonical_query_contains(&fact->range, &position) ||
            !canonical_query_scope_allows(scope, &fact->range)) {
            continue;
        }
        width = canonical_query_width(&fact->range);
        factIsExact = ZrParser_SemanticQuery_ExactnessAllowsProjection(
                fact->exactness);
        bestIsExact = best != ZR_NULL &&
                      ZrParser_SemanticQuery_ExactnessAllowsProjection(
                              best->exactness);
        if (best == ZR_NULL || width < bestWidth ||
            (width == bestWidth && factIsExact && !bestIsExact)) {
            best = fact;
            bestWidth = width;
            bestIsConflicting = ZR_FALSE;
        } else if (width == bestWidth && factIsExact == bestIsExact &&
                   !canonical_query_call_expressions_equal(best, fact)) {
            bestIsConflicting = ZR_TRUE;
        }
    }
    if (best == ZR_NULL || bestIsConflicting) return ZR_FALSE;
    if (!canonical_query_same_optional_source_exact(
                best->range.source, best->callTargetRange.source)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        TZrSize completeness;

        if (!canonical_query_call_reference_is_candidate(
                    context, &best->callTargetRange, reference)) {
            continue;
        }
        completeness = canonical_query_call_reference_completeness(reference);
        if (bestReference == ZR_NULL ||
            completeness > bestReferenceCompleteness) {
            bestReference = reference;
            bestReferenceCompleteness = completeness;
        }
    }
    if (bestReference == ZR_NULL) return ZR_FALSE;
    if ((best->isMemberCall &&
         bestReference->receiverTypeId == ZR_SEMANTIC_ID_INVALID) ||
        (!best->isMemberCall &&
         bestReference->receiverTypeId != ZR_SEMANTIC_ID_INVALID)) {
        return ZR_FALSE;
    }
    if (canonical_query_call_reference_has_resolved_target(bestReference)) {
        for (index = 0u; index < context->referenceFacts.length; ++index) {
            const SZrSemanticReferenceFact *reference =
                    (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->referenceFacts, index);

            if (reference == bestReference ||
                !canonical_query_call_reference_is_candidate(
                        context, &best->callTargetRange, reference) ||
                !canonical_query_ranges_equal(
                        &bestReference->range, &reference->range) ||
                !canonical_query_call_reference_has_resolved_target(reference)) {
                continue;
            }
            if (bestReference->symbolId != reference->symbolId) {
                return ZR_FALSE;
            }
            if (bestReference->receiverTypeId != ZR_SEMANTIC_ID_INVALID &&
                reference->receiverTypeId != ZR_SEMANTIC_ID_INVALID &&
                bestReference->receiverTypeId != reference->receiverTypeId) {
                return ZR_FALSE;
            }
        }
    }
    if (!canonical_query_call_argument_mappings_valid(
                context, best, bestReference)) {
        return ZR_FALSE;
    }
    outQuery->callableTypeId = bestReference->typeId;
    outQuery->receiverTypeId = bestReference->receiverTypeId;
    outQuery->expression = best;
    outQuery->reference = bestReference;
    outQuery->callSiteRange = best->range;
    outQuery->callTargetRange = best->callTargetRange;
    outQuery->argumentCount = best->argumentCount;
    outQuery->hasNamedArguments = best->hasNamedArguments;
    outQuery->isMemberCall = best->isMemberCall;
    if (canonical_query_call_reference_has_resolved_target(bestReference)) {
        outQuery->hasResolvedTarget = ZR_TRUE;
        outQuery->targetSymbolId = bestReference->symbolId;
        outQuery->targetDeclarationRange = bestReference->declarationRange;
    }
    if (bestReference->argumentMappings.isValid &&
        bestReference->argumentMappings.length > 0U) {
        outQuery->argumentMappings = &bestReference->argumentMappings;
    }
    return ZR_TRUE;
}

/**
 * @brief 优先复制引用事实中的签名文本，否则按规范 callable TypeId 格式化调用签名。
 * @pre query 必须来自同一存续且未改变的 context 快照；buffer 须为可写且容量非零。
 * @post 若提供非空缓冲区，函数先写入空串；缓冲区由调用方持有，失败时不保证完整签名。
 * @return 输入不完整、签名不可表示或输出容量不足时返回 false。
 */
TZrBool ZrParser_SemanticQuery_FormatCall(
        const SZrSemanticContext *context,
        const SZrParserSemanticCallQuery *query,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrChar typeBuffer[512];
    const TZrChar *name = ZR_NULL;
    int written;

    if (buffer != ZR_NULL && bufferSize > 0u) {
        buffer[0] = '\0';
    }
    if (context == ZR_NULL || query == ZR_NULL || query->expression == ZR_NULL ||
        query->reference == ZR_NULL ||
        !query->expression->hasCallInfo ||
        !ZrParser_SemanticQuery_ExactnessAllowsProjection(
                query->expression->exactness) ||
        buffer == ZR_NULL || bufferSize == 0u ||
        query->callableTypeId == ZR_SEMANTIC_ID_INVALID ||
        query->reference->kind != ZR_SEMANTIC_REFERENCE_CALL ||
        query->reference->typeId != query->callableTypeId ||
        !canonical_query_contains(
                &query->expression->callTargetRange, &query->reference->range)) {
        return ZR_FALSE;
    }
    if (query->reference != ZR_NULL && query->reference->signatureDisplay != ZR_NULL) {
        const TZrChar *display = ZrCore_String_GetNativeString(query->reference->signatureDisplay);
        TZrSize length = ZrCore_String_GetByteLength(query->reference->signatureDisplay);
        if (display == ZR_NULL || length + 1u > bufferSize) return ZR_FALSE;
        memcpy(buffer, display, length);
        buffer[length] = '\0';
        return ZR_TRUE;
    }
    if (!ZrParser_CanonicalType_Format(
                context, query->callableTypeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }
    if (query->reference != ZR_NULL && query->reference->name != ZR_NULL) {
        name = ZrCore_String_GetNativeString(query->reference->name);
    } else if (query->expression != ZR_NULL && query->expression->callTargetName != ZR_NULL) {
        name = ZrCore_String_GetNativeString(query->expression->callTargetName);
    }
    written = name != ZR_NULL && name[0] != '\0'
                      ? snprintf(buffer, bufferSize, "%s: %s", name, typeBuffer)
                      : snprintf(buffer, bufferSize, "%s", typeBuffer);
    return (TZrBool)(written >= 0 && (TZrSize)written < bufferSize);
}
