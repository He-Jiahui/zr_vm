#include "zr_vm_parser/type_inference.h"

#include <string.h>

#include "type_inference_constant_eval.h"
#include "type_inference_semantic_facts.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

typedef enum EZrTypeInferenceRangeBoundKind {
    ZR_TYPE_INFERENCE_RANGE_BOUND_LT = 0,
    ZR_TYPE_INFERENCE_RANGE_BOUND_LE,
    ZR_TYPE_INFERENCE_RANGE_BOUND_GT,
    ZR_TYPE_INFERENCE_RANGE_BOUND_GE,
    ZR_TYPE_INFERENCE_RANGE_BOUND_EQ,
    ZR_TYPE_INFERENCE_RANGE_BOUND_NE
} EZrTypeInferenceRangeBoundKind;

static void type_inference_branch_scope_init(SZrTypeInferenceBranchScope *scope,
                                             SZrTypeEnvironment *savedEnv) {
    if (scope == ZR_NULL) {
        return;
    }

    scope->savedEnv = savedEnv;
    scope->isActive = ZR_FALSE;
}

static TZrBool type_inference_branch_identifier_bound(SZrAstNode *identifier,
                                                      SZrAstNode *boundNode,
                                                      const TZrChar *op,
                                                      SZrString **outName,
                                                      TZrInt64 *outBound,
                                                      EZrTypeInferenceRangeBoundKind *outKind) {
    TZrInt64 boundValue;

    if (identifier == ZR_NULL ||
        identifier->type != ZR_AST_IDENTIFIER_LITERAL ||
        identifier->data.identifier.name == ZR_NULL ||
        boundNode == ZR_NULL ||
        op == ZR_NULL ||
        outName == ZR_NULL ||
        outBound == ZR_NULL ||
        outKind == ZR_NULL ||
        !type_inference_node_integer_value(boundNode, &boundValue)) {
        return ZR_FALSE;
    }

    if (strcmp(op, "<") == 0) {
        *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_LT;
    } else if (strcmp(op, "<=") == 0) {
        *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_LE;
    } else if (strcmp(op, ">") == 0) {
        *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_GT;
    } else if (strcmp(op, ">=") == 0) {
        *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_GE;
    } else if (strcmp(op, "==") == 0) {
        *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_EQ;
    } else if (strcmp(op, "!=") == 0) {
        *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_NE;
    } else {
        return ZR_FALSE;
    }

    *outName = identifier->data.identifier.name;
    *outBound = boundValue;
    return ZR_TRUE;
}

static TZrBool type_inference_branch_reversed_identifier_bound(
        SZrAstNode *boundNode,
        SZrAstNode *identifier,
        const TZrChar *op,
        SZrString **outName,
        TZrInt64 *outBound,
        EZrTypeInferenceRangeBoundKind *outKind) {
    EZrTypeInferenceRangeBoundKind kind;

    if (!type_inference_branch_identifier_bound(identifier, boundNode, op, outName, outBound, &kind)) {
        return ZR_FALSE;
    }

    switch (kind) {
        case ZR_TYPE_INFERENCE_RANGE_BOUND_LT:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_GT;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_LE:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_GE;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_GT:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_LT;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_GE:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_LE;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_EQ:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_EQ;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_NE:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_NE;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_branch_condition_bound(SZrAstNode *condition,
                                                     SZrString **outName,
                                                     TZrInt64 *outBound,
                                                     EZrTypeInferenceRangeBoundKind *outKind) {
    SZrBinaryExpression *binary;
    const TZrChar *op;

    if (condition == ZR_NULL ||
        condition->type != ZR_AST_BINARY_EXPRESSION ||
        outName == ZR_NULL ||
        outBound == ZR_NULL ||
        outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    binary = &condition->data.binaryExpression;
    op = binary->op.op;
    return type_inference_branch_identifier_bound(binary->left,
                                                  binary->right,
                                                  op,
                                                  outName,
                                                  outBound,
                                                  outKind) ||
           type_inference_branch_reversed_identifier_bound(binary->left,
                                                           binary->right,
                                                           op,
                                                           outName,
                                                           outBound,
                                                           outKind);
}

static TZrBool type_inference_branch_int64_add(TZrInt64 value,
                                               TZrInt64 delta,
                                               TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((delta > 0 && value > ZR_TYPE_RANGE_INT64_MAX - delta) ||
        (delta < 0 && value < ZR_TYPE_RANGE_INT64_MIN - delta)) {
        return ZR_FALSE;
    }

    *outValue = value + delta;
    return ZR_TRUE;
}

static void type_inference_branch_clear_range_segments(SZrInferredType *type) {
    if (type == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_ResetRangeSegments(type);
}

static TZrBool type_inference_branch_type_segment_at(const SZrInferredType *type,
                                                     TZrSize index,
                                                     SZrNumericRangeSegment *outSegment) {
    const SZrNumericRangeSegment *segment;

    if (type == ZR_NULL ||
        outSegment == ZR_NULL ||
        !type->hasRangeConstraint) {
        return ZR_FALSE;
    }

    if (type->rangeSegmentCount == 0) {
        if (index != 0) {
            return ZR_FALSE;
        }
        outSegment->minValue = type->minValue;
        outSegment->maxValue = type->maxValue;
        return outSegment->minValue <= outSegment->maxValue;
    }

    segment = ZrParser_InferredType_RangeSegmentAt(type, index);
    if (segment == ZR_NULL) {
        return ZR_FALSE;
    }
    *outSegment = *segment;
    return outSegment->minValue <= outSegment->maxValue;
}

static TZrSize type_inference_branch_type_segment_count(const SZrInferredType *type) {
    if (type == ZR_NULL || !type->hasRangeConstraint) {
        return 0;
    }
    return type->rangeSegmentCount > 0 ? type->rangeSegmentCount : 1;
}

static TZrBool type_inference_branch_segments_touch_or_overlap(
        const SZrNumericRangeSegment *left,
        const SZrNumericRangeSegment *right) {
    TZrInt64 adjacentBound;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left->maxValue < right->minValue) {
        return (TZrBool)(type_inference_branch_int64_add(left->maxValue, 1, &adjacentBound) &&
                         adjacentBound >= right->minValue);
    }
    if (right->maxValue < left->minValue) {
        return (TZrBool)(type_inference_branch_int64_add(right->maxValue, 1, &adjacentBound) &&
                         adjacentBound >= left->minValue);
    }
    return ZR_TRUE;
}

static void type_inference_branch_segment_array_normalize(SZrArray *segments) {
    TZrSize readIndex;
    TZrSize writeIndex;

    if (segments == ZR_NULL || !segments->isValid || segments->length <= 1) {
        return;
    }

    for (readIndex = 1; readIndex < segments->length; readIndex++) {
        TZrSize cursor = readIndex;
        while (cursor > 0) {
            SZrNumericRangeSegment *current =
                (SZrNumericRangeSegment *)ZrCore_Array_Get(segments, cursor);
            SZrNumericRangeSegment *previous =
                (SZrNumericRangeSegment *)ZrCore_Array_Get(segments, cursor - 1);
            if (current->minValue >= previous->minValue) {
                break;
            }
            {
                SZrNumericRangeSegment swap = *previous;
                *previous = *current;
                *current = swap;
            }
            cursor--;
        }
    }

    writeIndex = 0;
    for (readIndex = 1; readIndex < segments->length; readIndex++) {
        SZrNumericRangeSegment *current =
            (SZrNumericRangeSegment *)ZrCore_Array_Get(segments, writeIndex);
        SZrNumericRangeSegment *candidate =
            (SZrNumericRangeSegment *)ZrCore_Array_Get(segments, readIndex);
        if (type_inference_branch_segments_touch_or_overlap(current, candidate)) {
            if (candidate->maxValue > current->maxValue) {
                current->maxValue = candidate->maxValue;
            }
        } else {
            writeIndex++;
            if (writeIndex != readIndex) {
                SZrNumericRangeSegment candidateCopy = *candidate;
                ZrCore_Array_Set(segments, writeIndex, &candidateCopy);
            }
        }
    }
    segments->length = writeIndex + 1;
}

static TZrBool type_inference_branch_segment_array_append_normalized(
        SZrState *state,
        SZrArray *segments,
        TZrInt64 minValue,
        TZrInt64 maxValue) {
    SZrNumericRangeSegment segment;

    if (state == ZR_NULL || segments == ZR_NULL || minValue > maxValue) {
        return ZR_FALSE;
    }
    if (!segments->isValid) {
        ZrCore_Array_Init(state,
                          segments,
                          sizeof(SZrNumericRangeSegment),
                          ZR_PARSER_NUMERIC_RANGE_SEGMENT_CAPACITY);
    }

    segment.minValue = minValue;
    segment.maxValue = maxValue;
    ZrCore_Array_Push(state, segments, &segment);
    type_inference_branch_segment_array_normalize(segments);
    return ZR_TRUE;
}

static TZrBool type_inference_branch_set_type_from_segment_array(
        SZrState *state,
        SZrInferredType *type,
        const SZrArray *segments) {
    TZrSize index;
    const SZrNumericRangeSegment *firstSegment;
    const SZrNumericRangeSegment *lastSegment;

    if (state == ZR_NULL ||
        type == ZR_NULL ||
        segments == ZR_NULL ||
        !segments->isValid ||
        segments->length == 0) {
        return ZR_FALSE;
    }

    firstSegment = (const SZrNumericRangeSegment *)ZrCore_Array_Get((SZrArray *)segments, 0);
    lastSegment = (const SZrNumericRangeSegment *)ZrCore_Array_Get((SZrArray *)segments, segments->length - 1);
    if (firstSegment == ZR_NULL || lastSegment == ZR_NULL) {
        return ZR_FALSE;
    }

    type->hasRangeConstraint = ZR_TRUE;
    type->minValue = firstSegment->minValue;
    type->maxValue = lastSegment->maxValue;
    type_inference_branch_clear_range_segments(type);

    if (segments->length == 1) {
        return ZR_TRUE;
    }

    for (index = 0; index < segments->length; index++) {
        const SZrNumericRangeSegment *segment =
            (const SZrNumericRangeSegment *)ZrCore_Array_Get((SZrArray *)segments, index);
        if (segment == ZR_NULL ||
            !ZrParser_InferredType_AppendRangeSegment(state,
                                                      type,
                                                      segment->minValue,
                                                      segment->maxValue)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool type_inference_branch_apply_bound(SZrState *state,
                                                 SZrInferredType *type,
                                                 TZrInt64 bound,
                                                 EZrTypeInferenceRangeBoundKind kind) {
    TZrInt64 inclusiveBound;
    SZrArray refinedSegments;
    TZrSize segmentIndex;
    TZrSize segmentCount;
    TZrBool result = ZR_FALSE;

    if (state == ZR_NULL || type == ZR_NULL || !type->hasRangeConstraint) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&refinedSegments);
    segmentCount = type_inference_branch_type_segment_count(type);
    for (segmentIndex = 0; segmentIndex < segmentCount; segmentIndex++) {
        SZrNumericRangeSegment segment;
        TZrInt64 minValue;
        TZrInt64 maxValue;

        if (!type_inference_branch_type_segment_at(type, segmentIndex, &segment)) {
            goto cleanup;
        }

        minValue = segment.minValue;
        maxValue = segment.maxValue;
        switch (kind) {
            case ZR_TYPE_INFERENCE_RANGE_BOUND_LT:
                if (!type_inference_branch_int64_add(bound, -1, &inclusiveBound)) {
                    goto cleanup;
                }
                if (inclusiveBound < maxValue) {
                    maxValue = inclusiveBound;
                }
                if (minValue <= maxValue &&
                    !type_inference_branch_segment_array_append_normalized(state,
                                                                           &refinedSegments,
                                                                           minValue,
                                                                           maxValue)) {
                    goto cleanup;
                }
                break;
            case ZR_TYPE_INFERENCE_RANGE_BOUND_LE:
                if (bound < maxValue) {
                    maxValue = bound;
                }
                if (minValue <= maxValue &&
                    !type_inference_branch_segment_array_append_normalized(state,
                                                                           &refinedSegments,
                                                                           minValue,
                                                                           maxValue)) {
                    goto cleanup;
                }
                break;
            case ZR_TYPE_INFERENCE_RANGE_BOUND_GT:
                if (!type_inference_branch_int64_add(bound, 1, &inclusiveBound)) {
                    goto cleanup;
                }
                if (inclusiveBound > minValue) {
                    minValue = inclusiveBound;
                }
                if (minValue <= maxValue &&
                    !type_inference_branch_segment_array_append_normalized(state,
                                                                           &refinedSegments,
                                                                           minValue,
                                                                           maxValue)) {
                    goto cleanup;
                }
                break;
            case ZR_TYPE_INFERENCE_RANGE_BOUND_GE:
                if (bound > minValue) {
                    minValue = bound;
                }
                if (minValue <= maxValue &&
                    !type_inference_branch_segment_array_append_normalized(state,
                                                                           &refinedSegments,
                                                                           minValue,
                                                                           maxValue)) {
                    goto cleanup;
                }
                break;
            case ZR_TYPE_INFERENCE_RANGE_BOUND_EQ:
                if (bound >= minValue &&
                    bound <= maxValue &&
                    !type_inference_branch_segment_array_append_normalized(state,
                                                                           &refinedSegments,
                                                                           bound,
                                                                           bound)) {
                    goto cleanup;
                }
                break;
            case ZR_TYPE_INFERENCE_RANGE_BOUND_NE:
                if (bound < minValue || bound > maxValue) {
                    if (!type_inference_branch_segment_array_append_normalized(state,
                                                                               &refinedSegments,
                                                                               minValue,
                                                                               maxValue)) {
                        goto cleanup;
                    }
                    break;
                }
                if (bound > minValue) {
                    TZrInt64 leftMax;
                    if (!type_inference_branch_int64_add(bound, -1, &leftMax)) {
                        goto cleanup;
                    }
                    if (!type_inference_branch_segment_array_append_normalized(state,
                                                                               &refinedSegments,
                                                                               minValue,
                                                                               leftMax)) {
                        goto cleanup;
                    }
                }
                if (bound < maxValue) {
                    TZrInt64 rightMin;
                    if (!type_inference_branch_int64_add(bound, 1, &rightMin)) {
                        goto cleanup;
                    }
                    if (!type_inference_branch_segment_array_append_normalized(state,
                                                                               &refinedSegments,
                                                                               rightMin,
                                                                               maxValue)) {
                        goto cleanup;
                    }
                }
                break;
            default:
                goto cleanup;
        }
    }

    result = type_inference_branch_set_type_from_segment_array(state, type, &refinedSegments);

cleanup:
    if (refinedSegments.isValid) {
        ZrCore_Array_Free(state, &refinedSegments);
    }
    return result;
}

static TZrBool type_inference_branch_invert_bound(EZrTypeInferenceRangeBoundKind kind,
                                                  EZrTypeInferenceRangeBoundKind *outKind) {
    if (outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (kind) {
        case ZR_TYPE_INFERENCE_RANGE_BOUND_LT:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_GE;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_LE:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_GT;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_GT:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_LE;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_GE:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_LT;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_EQ:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_NE;
            return ZR_TRUE;
        case ZR_TYPE_INFERENCE_RANGE_BOUND_NE:
            *outKind = ZR_TYPE_INFERENCE_RANGE_BOUND_EQ;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_branch_append_refined_binding(SZrState *state,
                                                            SZrTypeEnvironment *env,
                                                            const SZrTypeBinding *sourceBinding,
                                                            const SZrInferredType *refinedType) {
    SZrTypeBinding binding;

    if (state == ZR_NULL ||
        env == ZR_NULL ||
        sourceBinding == ZR_NULL ||
        sourceBinding->name == ZR_NULL ||
        refinedType == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&binding, 0, sizeof(binding));
    binding.name = sourceBinding->name;
    binding.declarationRange = sourceBinding->declarationRange;
    binding.hasDeclarationRange = sourceBinding->hasDeclarationRange;
    binding.typeId = sourceBinding->typeId;
    binding.symbolId = sourceBinding->symbolId;
    ZrParser_InferredType_Copy(state, &binding.type, refinedType);
    ZrCore_Array_Push(state, &env->variableTypes, &binding);
    return ZR_TRUE;
}

static TZrBool type_inference_branch_refined_type_from_bound(
        SZrCompilerState *cs,
        SZrString *name,
        TZrInt64 bound,
        EZrTypeInferenceRangeBoundKind boundKind,
        SZrInferredType *outType,
        const SZrTypeBinding **outSourceBinding) {
    const SZrTypeBinding *sourceBinding;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        name == ZR_NULL ||
        outType == ZR_NULL ||
        outSourceBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
    if (sourceBinding == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(sourceBinding->type.baseType)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, outType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, outType, &sourceBinding->type);
    if (!outType->hasRangeConstraint) {
        type_inference_apply_primitive_numeric_range(outType);
    }
    if (!type_inference_branch_apply_bound(cs->state, outType, bound, boundKind)) {
        ZrParser_InferredType_Free(cs->state, outType);
        return ZR_FALSE;
    }

    *outSourceBinding = sourceBinding;
    return ZR_TRUE;
}

static TZrBool type_inference_branch_push_refined_type_scope(
        SZrCompilerState *cs,
        SZrTypeInferenceBranchScope *scope,
        const SZrTypeBinding *sourceBinding,
        const SZrInferredType *refinedType) {
    SZrTypeEnvironment *newEnv;
    TZrBool pushed;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        scope == ZR_NULL ||
        sourceBinding == ZR_NULL ||
        refinedType == ZR_NULL) {
        return ZR_FALSE;
    }

    newEnv = ZrParser_TypeEnvironment_New(cs->state);
    if (newEnv == ZR_NULL) {
        return ZR_FALSE;
    }

    newEnv->parent = cs->typeEnv;
    newEnv->semanticContext = cs->typeEnv->semanticContext != ZR_NULL
                                  ? cs->typeEnv->semanticContext
                                  : cs->semanticContext;
    pushed = type_inference_branch_append_refined_binding(cs->state, newEnv, sourceBinding, refinedType);
    if (!pushed) {
        ZrParser_TypeEnvironment_Free(cs->state, newEnv);
        return ZR_FALSE;
    }

    scope->savedEnv = cs->typeEnv;
    scope->isActive = ZR_TRUE;
    cs->typeEnv = newEnv;
    return ZR_TRUE;
}

static TZrBool type_inference_branch_ranges_touch_or_overlap(const SZrInferredType *leftType,
                                                             const SZrInferredType *rightType) {
    TZrInt64 adjacentBound;

    if (leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        !leftType->hasRangeConstraint ||
        !rightType->hasRangeConstraint) {
        return ZR_FALSE;
    }

    if (leftType->maxValue < rightType->minValue) {
        return (TZrBool)(type_inference_branch_int64_add(leftType->maxValue, 1, &adjacentBound) &&
                         adjacentBound >= rightType->minValue);
    }
    if (rightType->maxValue < leftType->minValue) {
        return (TZrBool)(type_inference_branch_int64_add(rightType->maxValue, 1, &adjacentBound) &&
                         adjacentBound >= leftType->minValue);
    }
    return ZR_TRUE;
}

static void type_inference_branch_set_disjoint_range_segments(SZrState *state,
                                                              SZrInferredType *target,
                                                              const SZrInferredType *leftType,
                                                              const SZrInferredType *rightType) {
    const SZrInferredType *firstType;
    const SZrInferredType *secondType;
    TZrInt64 firstMin;
    TZrInt64 firstMax;
    TZrInt64 secondMin;
    TZrInt64 secondMax;

    if (state == ZR_NULL ||
        target == ZR_NULL ||
        leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        !leftType->hasRangeConstraint ||
        !rightType->hasRangeConstraint) {
        return;
    }

    if (leftType->minValue <= rightType->minValue) {
        firstType = leftType;
        secondType = rightType;
    } else {
        firstType = rightType;
        secondType = leftType;
    }

    firstMin = firstType->minValue;
    firstMax = firstType->maxValue;
    secondMin = secondType->minValue;
    secondMax = secondType->maxValue;

    target->hasRangeConstraint = ZR_TRUE;
    target->minValue = firstMin;
    target->maxValue = secondMax;
    type_inference_branch_clear_range_segments(target);
    ZrParser_InferredType_AppendRangeSegment(state, target, firstMin, firstMax);
    ZrParser_InferredType_AppendRangeSegment(state, target, secondMin, secondMax);
}

static TZrBool type_inference_branch_refined_type_from_condition(
        SZrCompilerState *cs,
        SZrAstNode *condition,
        TZrBool invertCondition,
        SZrInferredType *outType,
        const SZrTypeBinding **outSourceBinding);

static TZrBool type_inference_branch_intersect_refined_types(SZrState *state,
                                                             SZrInferredType *leftType,
                                                             const SZrInferredType *rightType) {
    SZrArray segments;
    TZrSize leftIndex;
    TZrSize rightIndex;
    TZrBool result = ZR_FALSE;

    if (state == ZR_NULL ||
        leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        !leftType->hasRangeConstraint ||
        !rightType->hasRangeConstraint) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&segments);
    for (leftIndex = 0; leftIndex < type_inference_branch_type_segment_count(leftType); leftIndex++) {
        SZrNumericRangeSegment leftSegment;
        if (!type_inference_branch_type_segment_at(leftType, leftIndex, &leftSegment)) {
            goto cleanup;
        }
        for (rightIndex = 0; rightIndex < type_inference_branch_type_segment_count(rightType); rightIndex++) {
            SZrNumericRangeSegment rightSegment;
            TZrInt64 minValue;
            TZrInt64 maxValue;

            if (!type_inference_branch_type_segment_at(rightType, rightIndex, &rightSegment)) {
                goto cleanup;
            }
            minValue = leftSegment.minValue > rightSegment.minValue
                           ? leftSegment.minValue
                           : rightSegment.minValue;
            maxValue = leftSegment.maxValue < rightSegment.maxValue
                           ? leftSegment.maxValue
                           : rightSegment.maxValue;
            if (minValue <= maxValue &&
                !type_inference_branch_segment_array_append_normalized(state,
                                                                       &segments,
                                                                       minValue,
                                                                       maxValue)) {
                goto cleanup;
            }
        }
    }

    result = type_inference_branch_set_type_from_segment_array(state, leftType, &segments);

cleanup:
    if (segments.isValid) {
        ZrCore_Array_Free(state, &segments);
    }
    return result;
}

static TZrBool type_inference_branch_union_refined_types(SZrState *state,
                                                         SZrInferredType *leftType,
                                                         const SZrInferredType *rightType) {
    SZrArray segments;
    TZrSize index;
    TZrBool result = ZR_FALSE;

    if (state == ZR_NULL ||
        leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        !leftType->hasRangeConstraint ||
        !rightType->hasRangeConstraint) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&segments);
    for (index = 0; index < type_inference_branch_type_segment_count(leftType); index++) {
        SZrNumericRangeSegment segment;
        if (!type_inference_branch_type_segment_at(leftType, index, &segment) ||
            !type_inference_branch_segment_array_append_normalized(state,
                                                                   &segments,
                                                                   segment.minValue,
                                                                   segment.maxValue)) {
            goto cleanup;
        }
    }
    for (index = 0; index < type_inference_branch_type_segment_count(rightType); index++) {
        SZrNumericRangeSegment segment;
        if (!type_inference_branch_type_segment_at(rightType, index, &segment) ||
            !type_inference_branch_segment_array_append_normalized(state,
                                                                   &segments,
                                                                   segment.minValue,
                                                                   segment.maxValue)) {
            goto cleanup;
        }
    }

    result = type_inference_branch_set_type_from_segment_array(state, leftType, &segments);

cleanup:
    if (segments.isValid) {
        ZrCore_Array_Free(state, &segments);
    }
    return result;
}

static TZrBool type_inference_branch_refined_type_from_logical_children(
        SZrCompilerState *cs,
        SZrAstNode *leftCondition,
        SZrAstNode *rightCondition,
        TZrBool invertCondition,
        TZrBool mergeAsUnion,
        SZrInferredType *outType,
        const SZrTypeBinding **outSourceBinding) {
    SZrInferredType leftType;
    SZrInferredType rightType;
    const SZrTypeBinding *leftSourceBinding = ZR_NULL;
    const SZrTypeBinding *rightSourceBinding = ZR_NULL;
    TZrBool merged = ZR_FALSE;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        outType == ZR_NULL ||
        outSourceBinding == ZR_NULL ||
        !type_inference_branch_refined_type_from_condition(cs,
                                                           leftCondition,
                                                           invertCondition,
                                                           &leftType,
                                                           &leftSourceBinding)) {
        return ZR_FALSE;
    }

    if (!type_inference_branch_refined_type_from_condition(cs,
                                                           rightCondition,
                                                           invertCondition,
                                                           &rightType,
                                                           &rightSourceBinding)) {
        ZrParser_InferredType_Free(cs->state, &leftType);
        return ZR_FALSE;
    }

    if (leftSourceBinding == rightSourceBinding) {
        merged = mergeAsUnion
                     ? type_inference_branch_union_refined_types(cs->state, &leftType, &rightType)
                     : type_inference_branch_intersect_refined_types(cs->state, &leftType, &rightType);
    }
    if (merged) {
        ZrParser_InferredType_Init(cs->state, outType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, outType, &leftType);
        *outSourceBinding = leftSourceBinding;
    }

    ZrParser_InferredType_Free(cs->state, &rightType);
    ZrParser_InferredType_Free(cs->state, &leftType);
    return merged;
}

static TZrBool type_inference_branch_refined_type_from_condition(
        SZrCompilerState *cs,
        SZrAstNode *condition,
        TZrBool invertCondition,
        SZrInferredType *outType,
        const SZrTypeBinding **outSourceBinding) {
    SZrString *name = ZR_NULL;
    TZrInt64 bound = 0;
    EZrTypeInferenceRangeBoundKind boundKind;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        condition == ZR_NULL ||
        outType == ZR_NULL ||
        outSourceBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (type_inference_branch_condition_bound(condition, &name, &bound, &boundKind)) {
        if (invertCondition && !type_inference_branch_invert_bound(boundKind, &boundKind)) {
            return ZR_FALSE;
        }
        return type_inference_branch_refined_type_from_bound(cs,
                                                             name,
                                                             bound,
                                                             boundKind,
                                                             outType,
                                                             outSourceBinding);
    }

    if (condition->type == ZR_AST_UNARY_EXPRESSION &&
        condition->data.unaryExpression.op.op != ZR_NULL &&
        strcmp(condition->data.unaryExpression.op.op, "!") == 0 &&
        condition->data.unaryExpression.argument != ZR_NULL) {
        return type_inference_branch_refined_type_from_condition(
            cs,
            condition->data.unaryExpression.argument,
            (TZrBool)!invertCondition,
            outType,
            outSourceBinding);
    }

    if (condition->type == ZR_AST_LOGICAL_EXPRESSION &&
        condition->data.logicalExpression.op != ZR_NULL) {
        const TZrChar *op = condition->data.logicalExpression.op;
        if ((!invertCondition && strcmp(op, "&&") == 0) ||
            (invertCondition && strcmp(op, "||") == 0)) {
            return type_inference_branch_refined_type_from_logical_children(
                cs,
                condition->data.logicalExpression.left,
                condition->data.logicalExpression.right,
                invertCondition,
                ZR_FALSE,
                outType,
                outSourceBinding);
        }
        if ((!invertCondition && strcmp(op, "||") == 0) ||
            (invertCondition && strcmp(op, "&&") == 0)) {
            return type_inference_branch_refined_type_from_logical_children(
                cs,
                condition->data.logicalExpression.left,
                condition->data.logicalExpression.right,
                invertCondition,
                ZR_TRUE,
                outType,
                outSourceBinding);
        }
    }

    return ZR_FALSE;
}

static TZrBool type_inference_branch_push_logical_union_range_scope(
        SZrCompilerState *cs,
        SZrAstNode *condition,
        SZrTypeInferenceBranchScope *scope,
        const TZrChar *logicalOp,
        TZrBool invertBounds) {
    SZrString *leftName = ZR_NULL;
    SZrString *rightName = ZR_NULL;
    TZrInt64 leftBound = 0;
    TZrInt64 rightBound = 0;
    EZrTypeInferenceRangeBoundKind leftKind;
    EZrTypeInferenceRangeBoundKind rightKind;
    SZrInferredType leftType;
    SZrInferredType rightType;
    const SZrTypeBinding *sourceBinding = ZR_NULL;
    const SZrTypeBinding *rightSourceBinding = ZR_NULL;
    TZrBool pushed = ZR_FALSE;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        condition == ZR_NULL ||
        condition->type != ZR_AST_LOGICAL_EXPRESSION ||
        logicalOp == ZR_NULL ||
        condition->data.logicalExpression.op == ZR_NULL ||
        strcmp(condition->data.logicalExpression.op, logicalOp) != 0 ||
        !type_inference_branch_condition_bound(condition->data.logicalExpression.left,
                                               &leftName,
                                               &leftBound,
                                               &leftKind) ||
        !type_inference_branch_condition_bound(condition->data.logicalExpression.right,
                                               &rightName,
                                               &rightBound,
                                               &rightKind) ||
        !ZrCore_String_Equal(leftName, rightName)) {
        return ZR_FALSE;
    }

    if (invertBounds &&
        (!type_inference_branch_invert_bound(leftKind, &leftKind) ||
         !type_inference_branch_invert_bound(rightKind, &rightKind))) {
        return ZR_FALSE;
    }

    if (!type_inference_branch_refined_type_from_bound(cs,
                                                       leftName,
                                                       leftBound,
                                                       leftKind,
                                                       &leftType,
                                                       &sourceBinding)) {
        return ZR_FALSE;
    }
    if (!type_inference_branch_refined_type_from_bound(cs,
                                                       rightName,
                                                       rightBound,
                                                       rightKind,
                                                       &rightType,
                                                       &rightSourceBinding)) {
        ZrParser_InferredType_Free(cs->state, &leftType);
        return ZR_FALSE;
    }

    if (sourceBinding == rightSourceBinding &&
        type_inference_branch_ranges_touch_or_overlap(&leftType, &rightType)) {
        if (rightType.minValue < leftType.minValue) {
            leftType.minValue = rightType.minValue;
        }
        if (rightType.maxValue > leftType.maxValue) {
            leftType.maxValue = rightType.maxValue;
        }
        type_inference_branch_clear_range_segments(&leftType);
        pushed = type_inference_branch_push_refined_type_scope(cs, scope, sourceBinding, &leftType);
    } else if (sourceBinding == rightSourceBinding) {
        type_inference_branch_set_disjoint_range_segments(cs->state, &leftType, &leftType, &rightType);
        pushed = type_inference_branch_push_refined_type_scope(cs, scope, sourceBinding, &leftType);
    }

    ZrParser_InferredType_Free(cs->state, &rightType);
    ZrParser_InferredType_Free(cs->state, &leftType);
    return pushed;
}

static TZrBool type_inference_branch_push_numeric_range_scope(
        SZrCompilerState *cs,
        SZrAstNode *condition,
        SZrTypeInferenceBranchScope *scope,
        TZrBool invertCondition) {
    SZrTypeInferenceBranchScope leftScope;
    SZrTypeInferenceBranchScope rightScope;
    SZrString *name = ZR_NULL;
    TZrInt64 bound = 0;
    EZrTypeInferenceRangeBoundKind boundKind;
    SZrInferredType refinedType;
    const SZrTypeBinding *sourceBinding = ZR_NULL;
    SZrTypeEnvironment *newEnv;
    TZrBool pushed;

    if (scope != ZR_NULL) {
        type_inference_branch_scope_init(scope, cs != ZR_NULL ? cs->typeEnv : ZR_NULL);
    }
    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        scope == ZR_NULL ||
        !type_inference_branch_condition_bound(condition, &name, &bound, &boundKind)) {
        if (type_inference_branch_refined_type_from_condition(cs,
                                                              condition,
                                                              invertCondition,
                                                              &refinedType,
                                                              &sourceBinding)) {
            pushed = type_inference_branch_push_refined_type_scope(cs, scope, sourceBinding, &refinedType);
            ZrParser_InferredType_Free(cs->state, &refinedType);
            return pushed;
        }
        if (condition != ZR_NULL &&
            condition->type == ZR_AST_UNARY_EXPRESSION &&
            condition->data.unaryExpression.op.op != ZR_NULL &&
            strcmp(condition->data.unaryExpression.op.op, "!") == 0 &&
            condition->data.unaryExpression.argument != ZR_NULL) {
            return type_inference_branch_push_numeric_range_scope(cs,
                                                                  condition->data.unaryExpression.argument,
                                                                  scope,
                                                                  (TZrBool)!invertCondition);
        }
        if (cs != ZR_NULL &&
            cs->state != ZR_NULL &&
            condition != ZR_NULL &&
            condition->type == ZR_AST_LOGICAL_EXPRESSION &&
            condition->data.logicalExpression.op != ZR_NULL &&
            ((!invertCondition && strcmp(condition->data.logicalExpression.op, "&&") == 0) ||
             (invertCondition && strcmp(condition->data.logicalExpression.op, "||") == 0))) {
            SZrTypeEnvironment *savedEnv = cs->typeEnv;
            if (!type_inference_branch_push_numeric_range_scope(cs,
                                                                condition->data.logicalExpression.left,
                                                                &leftScope,
                                                                invertCondition)) {
                return ZR_FALSE;
            }
            if (!type_inference_branch_push_numeric_range_scope(cs,
                                                                condition->data.logicalExpression.right,
                                                                &rightScope,
                                                                invertCondition)) {
                ZrParser_TypeInference_PopBranchScope(cs, &leftScope);
                return ZR_FALSE;
            }

            scope->savedEnv = savedEnv;
            scope->isActive = ZR_TRUE;
            return ZR_TRUE;
        }
        if ((!invertCondition &&
             type_inference_branch_push_logical_union_range_scope(cs,
                                                                  condition,
                                                                  scope,
                                                                  "||",
                                                                  ZR_FALSE)) ||
            (invertCondition &&
             type_inference_branch_push_logical_union_range_scope(cs,
                                                                  condition,
                                                                  scope,
                                                                  "&&",
                                                                  ZR_TRUE))) {
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (invertCondition && !type_inference_branch_invert_bound(boundKind, &boundKind)) {
        return ZR_FALSE;
    }

    sourceBinding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
    if (sourceBinding == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(sourceBinding->type.baseType)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &refinedType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(cs->state, &refinedType, &sourceBinding->type);
    if (!refinedType.hasRangeConstraint) {
        type_inference_apply_primitive_numeric_range(&refinedType);
    }
    if (!type_inference_branch_apply_bound(cs->state, &refinedType, bound, boundKind)) {
        ZrParser_InferredType_Free(cs->state, &refinedType);
        return ZR_FALSE;
    }

    newEnv = ZrParser_TypeEnvironment_New(cs->state);
    if (newEnv == ZR_NULL) {
        ZrParser_InferredType_Free(cs->state, &refinedType);
        return ZR_FALSE;
    }

    newEnv->parent = cs->typeEnv;
    newEnv->semanticContext = cs->typeEnv->semanticContext != ZR_NULL
                                  ? cs->typeEnv->semanticContext
                                  : cs->semanticContext;
    pushed = type_inference_branch_append_refined_binding(cs->state, newEnv, sourceBinding, &refinedType);
    ZrParser_InferredType_Free(cs->state, &refinedType);
    if (!pushed) {
        ZrParser_TypeEnvironment_Free(cs->state, newEnv);
        return ZR_FALSE;
    }

    scope->savedEnv = cs->typeEnv;
    scope->isActive = ZR_TRUE;
    cs->typeEnv = newEnv;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
        SZrCompilerState *cs,
        SZrAstNode *condition,
        SZrTypeInferenceBranchScope *scope) {
    return type_inference_branch_push_numeric_range_scope(cs, condition, scope, ZR_FALSE);
}

TZrBool ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
        SZrCompilerState *cs,
        SZrAstNode *condition,
        SZrTypeInferenceBranchScope *scope) {
    return type_inference_branch_push_numeric_range_scope(cs, condition, scope, ZR_TRUE);
}

void ZrParser_TypeInference_PopBranchScope(SZrCompilerState *cs,
                                           SZrTypeInferenceBranchScope *scope) {
    SZrTypeEnvironment *currentEnv;
    SZrTypeEnvironment *nextEnv;
    SZrTypeEnvironment *scanEnv;
    TZrBool hasSavedEnvInChain = ZR_FALSE;

    if (cs == ZR_NULL || scope == ZR_NULL || !scope->isActive) {
        return;
    }

    currentEnv = cs->typeEnv;
    scanEnv = currentEnv;
    while (scanEnv != ZR_NULL) {
        if (scanEnv == scope->savedEnv) {
            hasSavedEnvInChain = ZR_TRUE;
            break;
        }
        scanEnv = scanEnv->parent;
    }

    cs->typeEnv = scope->savedEnv;
    if (!hasSavedEnvInChain) {
        scope->savedEnv = ZR_NULL;
        scope->isActive = ZR_FALSE;
        return;
    }

    while (currentEnv != ZR_NULL && currentEnv != scope->savedEnv) {
        nextEnv = currentEnv->parent;
        ZrParser_TypeEnvironment_Free(cs->state, currentEnv);
        currentEnv = nextEnv;
    }

    scope->savedEnv = ZR_NULL;
    scope->isActive = ZR_FALSE;
}
