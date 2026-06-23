#include "type_inference_loop_assignment_self_dependency.h"

#include <string.h>

#include "type_inference_loop_assignment_syntax.h"
#include "zr_vm_core/array.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_parser/compiler.h"

enum {
    ZR_LOOP_ASSIGNMENT_SELF_DEPENDENCY_PLUS_TERM_LIMIT = 8
};

static TZrBool loop_assignment_self_dependency_identifier_is_name(SZrAstNode *node,
                                                                  SZrString *name) {
    return node != ZR_NULL &&
           name != ZR_NULL &&
           node->type == ZR_AST_IDENTIFIER_LITERAL &&
           node->data.identifier.name != ZR_NULL &&
           ZrCore_String_Equal(node->data.identifier.name, name);
}

static TZrBool loop_assignment_self_dependency_delta_expression_is_supported(SZrAstNode *node,
                                                                             SZrString *targetName) {
    SZrAstNodeArray *members;

    if (node == ZR_NULL || targetName == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_TRUE;
        case ZR_AST_IDENTIFIER_LITERAL:
            return node->data.identifier.name != ZR_NULL &&
                   !ZrCore_String_Equal(node->data.identifier.name, targetName);
        case ZR_AST_BINARY_EXPRESSION:
            return loop_assignment_self_dependency_delta_expression_is_supported(
                           node->data.binaryExpression.left,
                           targetName) &&
                   loop_assignment_self_dependency_delta_expression_is_supported(
                           node->data.binaryExpression.right,
                           targetName);
        case ZR_AST_LOGICAL_EXPRESSION:
            return loop_assignment_self_dependency_delta_expression_is_supported(
                           node->data.logicalExpression.left,
                           targetName) &&
                   loop_assignment_self_dependency_delta_expression_is_supported(
                           node->data.logicalExpression.right,
                           targetName);
        case ZR_AST_UNARY_EXPRESSION:
            return loop_assignment_self_dependency_delta_expression_is_supported(
                    node->data.unaryExpression.argument,
                    targetName);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return loop_assignment_self_dependency_delta_expression_is_supported(
                    node->data.typeCastExpression.expression,
                    targetName);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return loop_assignment_self_dependency_delta_expression_is_supported(
                           node->data.conditionalExpression.test,
                           targetName) &&
                   loop_assignment_self_dependency_delta_expression_is_supported(
                           node->data.conditionalExpression.consequent,
                           targetName) &&
                   loop_assignment_self_dependency_delta_expression_is_supported(
                           node->data.conditionalExpression.alternate,
                           targetName);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = node->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_FALSE;
            }
            return loop_assignment_self_dependency_delta_expression_is_supported(
                    node->data.primaryExpression.property,
                    targetName);
        default:
            return ZR_FALSE;
    }
}

static TZrBool loop_assignment_self_dependency_inferred_integer_range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    SZrInferredType inferredType;
    TZrBool success;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        node == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    success = ZrParser_ExpressionType_Infer(cs, node, &inferredType) &&
              inferredType.baseType == ZR_VALUE_TYPE_INT64 &&
              inferredType.hasRangeConstraint;
    if (success) {
        *outMin = inferredType.minValue;
        *outMax = inferredType.maxValue;
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return success;
}

static TZrBool loop_assignment_self_dependency_integer_range(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *targetName,
        TZrInt64 *outMin,
        TZrInt64 *outMax) {
    const SZrTypeBinding *binding;

    if (outMin != ZR_NULL) {
        *outMin = 0;
    }
    if (outMax != ZR_NULL) {
        *outMax = 0;
    }

    if (node == ZR_NULL ||
        outMin == ZR_NULL ||
        outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_INTEGER_LITERAL) {
        *outMin = node->data.integerLiteral.value;
        *outMax = node->data.integerLiteral.value;
        return ZR_TRUE;
    }

    if (cs == ZR_NULL ||
        cs->typeEnv == ZR_NULL ||
        targetName == ZR_NULL ||
        !loop_assignment_self_dependency_delta_expression_is_supported(node, targetName)) {
        return ZR_FALSE;
    }

    if (node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return loop_assignment_self_dependency_inferred_integer_range(cs, node, outMin, outMax);
    }

    if (node->data.identifier.name == ZR_NULL ||
        ZrCore_String_Equal(node->data.identifier.name, targetName)) {
        return ZR_FALSE;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, node->data.identifier.name);
    if (binding == ZR_NULL ||
        binding->type.baseType != ZR_VALUE_TYPE_INT64 ||
        !binding->type.hasRangeConstraint) {
        return ZR_FALSE;
    }

    *outMin = binding->type.minValue;
    *outMax = binding->type.maxValue;
    return ZR_TRUE;
}

static TZrBool loop_assignment_self_dependency_find_delta_expression(SZrAstNode *right,
                                                                     SZrString *targetName,
                                                                     SZrAstNode **outNode,
                                                                     TZrInt32 *outSign) {
    if (outNode != ZR_NULL) {
        *outNode = ZR_NULL;
    }
    if (outSign != ZR_NULL) {
        *outSign = 0;
    }

    if (right == ZR_NULL ||
        targetName == ZR_NULL ||
        outNode == ZR_NULL ||
        outSign == ZR_NULL ||
        right->type != ZR_AST_BINARY_EXPRESSION ||
        right->data.binaryExpression.op.op == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(right->data.binaryExpression.op.op, "+") == 0) {
        if (loop_assignment_self_dependency_identifier_is_name(
                right->data.binaryExpression.left,
                targetName)) {
            *outNode = right->data.binaryExpression.right;
            *outSign = 1;
            return ZR_TRUE;
        }
        if (loop_assignment_self_dependency_identifier_is_name(
                right->data.binaryExpression.right,
                targetName)) {
            *outNode = right->data.binaryExpression.left;
            *outSign = 1;
            return ZR_TRUE;
        }
    }

    if (strcmp(right->data.binaryExpression.op.op, "-") == 0 &&
        loop_assignment_self_dependency_identifier_is_name(
                right->data.binaryExpression.left,
                targetName)) {
        *outNode = right->data.binaryExpression.right;
        *outSign = -1;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool loop_assignment_self_dependency_int64_add(TZrInt64 left,
                                                         TZrInt64 right,
                                                         TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((right > 0 && left > ZR_TYPE_RANGE_INT64_MAX - right) ||
        (right < 0 && left < ZR_TYPE_RANGE_INT64_MIN - right)) {
        return ZR_FALSE;
    }

    *outValue = left + right;
    return ZR_TRUE;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_TrySelfDependentDelta(SZrCompilerState *cs,
                                                                   SZrAstNode *assignmentNode,
                                                                   SZrString *targetName,
                                                                   TZrInt64 *outDeltaMin,
                                                                   TZrInt64 *outDeltaMax) {
    SZrString *name;
    SZrAstNode *right;
    SZrAstNode *deltaNode;
    TZrInt32 deltaSign;
    TZrInt64 deltaMin;
    TZrInt64 deltaMax;

    if (outDeltaMin != ZR_NULL) {
        *outDeltaMin = 0;
    }
    if (outDeltaMax != ZR_NULL) {
        *outDeltaMax = 0;
    }
    if (assignmentNode == ZR_NULL ||
        targetName == ZR_NULL ||
        outDeltaMin == ZR_NULL ||
        outDeltaMax == ZR_NULL ||
        !ZrParser_TypeInferenceLoopAssignment_AssignmentParts(assignmentNode, &name, &right) ||
        name == ZR_NULL ||
        !ZrCore_String_Equal(name, targetName) ||
        right == ZR_NULL ||
        !loop_assignment_self_dependency_find_delta_expression(
                right,
                targetName,
                &deltaNode,
                &deltaSign) ||
        !loop_assignment_self_dependency_integer_range(
                cs,
                deltaNode,
                targetName,
                &deltaMin,
                &deltaMax) ||
        !((deltaMin == 0 && deltaMax == 0) ||
          (deltaMin >= 0 && deltaMax > 0) ||
          (deltaMin < 0 && deltaMax <= 0) ||
          (deltaMin < 0 && deltaMax > 0))) {
        return ZR_FALSE;
    }

    if (deltaSign > 0) {
        *outDeltaMin = deltaMin;
        *outDeltaMax = deltaMax;
    } else {
        *outDeltaMin = -deltaMax;
        *outDeltaMax = -deltaMin;
    }
    return ZR_TRUE;
}

SZrAstNode *ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(SZrAstNode *right,
                                                                              SZrString *targetName) {
    SZrAstNode *deltaNode = ZR_NULL;
    TZrInt32 deltaSign = 0;

    if (!loop_assignment_self_dependency_find_delta_expression(
            right,
            targetName,
            &deltaNode,
            &deltaSign)) {
        return ZR_NULL;
    }
    return deltaNode;
}

SZrString *ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaIdentifier(SZrAstNode *right,
                                                                             SZrString *targetName) {
    SZrAstNode *deltaNode = ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(
            right,
            targetName);

    if (deltaNode == ZR_NULL ||
        deltaNode->type != ZR_AST_IDENTIFIER_LITERAL ||
        deltaNode->data.identifier.name == ZR_NULL ||
        ZrCore_String_Equal(deltaNode->data.identifier.name, targetName)) {
        return ZR_NULL;
    }

    return deltaNode->data.identifier.name;
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaSign(SZrAstNode *right,
                                                                    SZrString *targetName,
                                                                    TZrInt32 *outSign) {
    SZrAstNode *deltaNode = ZR_NULL;
    TZrInt32 deltaSign = 0;

    if (outSign != ZR_NULL) {
        *outSign = 0;
    }
    if (outSign == ZR_NULL ||
        !loop_assignment_self_dependency_find_delta_expression(
                right,
                targetName,
                &deltaNode,
                &deltaSign) ||
        deltaNode == ZR_NULL ||
        deltaSign == 0) {
        return ZR_FALSE;
    }

    *outSign = deltaSign;
    return ZR_TRUE;
}

static TZrBool loop_assignment_self_dependency_string_equal(const TZrChar *left,
                                                            const TZrChar *right) {
    return left != ZR_NULL && right != ZR_NULL && strcmp(left, right) == 0;
}

static TZrBool loop_assignment_self_dependency_delta_expression_equal(SZrAstNode *left,
                                                                      SZrAstNode *right,
                                                                      SZrString *targetName);

static TZrBool loop_assignment_self_dependency_is_binary_plus(SZrAstNode *node) {
    return node != ZR_NULL &&
           node->type == ZR_AST_BINARY_EXPRESSION &&
           loop_assignment_self_dependency_string_equal(node->data.binaryExpression.op.op, "+");
}

static TZrBool loop_assignment_self_dependency_collect_plus_terms(SZrAstNode *node,
                                                                  SZrAstNode **terms,
                                                                  TZrSize *termCount) {
    if (node == ZR_NULL || terms == ZR_NULL || termCount == ZR_NULL) {
        return ZR_FALSE;
    }

    if (loop_assignment_self_dependency_is_binary_plus(node)) {
        return loop_assignment_self_dependency_collect_plus_terms(
                       node->data.binaryExpression.left,
                       terms,
                       termCount) &&
               loop_assignment_self_dependency_collect_plus_terms(
                       node->data.binaryExpression.right,
                       terms,
                       termCount);
    }

    if (*termCount >= ZR_LOOP_ASSIGNMENT_SELF_DEPENDENCY_PLUS_TERM_LIMIT) {
        return ZR_FALSE;
    }

    terms[*termCount] = node;
    *termCount = *termCount + 1;
    return ZR_TRUE;
}

static TZrBool loop_assignment_self_dependency_plus_terms_equal(SZrAstNode *left,
                                                                SZrAstNode *right,
                                                                SZrString *targetName) {
    SZrAstNode *leftTerms[ZR_LOOP_ASSIGNMENT_SELF_DEPENDENCY_PLUS_TERM_LIMIT];
    SZrAstNode *rightTerms[ZR_LOOP_ASSIGNMENT_SELF_DEPENDENCY_PLUS_TERM_LIMIT];
    TZrBool matched[ZR_LOOP_ASSIGNMENT_SELF_DEPENDENCY_PLUS_TERM_LIMIT];
    TZrSize leftCount = 0;
    TZrSize rightCount = 0;
    TZrSize leftIndex;
    TZrSize rightIndex;

    if (!loop_assignment_self_dependency_collect_plus_terms(left, leftTerms, &leftCount) ||
        !loop_assignment_self_dependency_collect_plus_terms(right, rightTerms, &rightCount) ||
        leftCount != rightCount) {
        return ZR_FALSE;
    }

    for (rightIndex = 0; rightIndex < rightCount; rightIndex++) {
        matched[rightIndex] = ZR_FALSE;
    }

    for (leftIndex = 0; leftIndex < leftCount; leftIndex++) {
        TZrBool found = ZR_FALSE;

        for (rightIndex = 0; rightIndex < rightCount; rightIndex++) {
            if (!matched[rightIndex] &&
                loop_assignment_self_dependency_delta_expression_equal(
                        leftTerms[leftIndex],
                        rightTerms[rightIndex],
                        targetName)) {
                matched[rightIndex] = ZR_TRUE;
                found = ZR_TRUE;
                break;
            }
        }

        if (!found) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool loop_assignment_self_dependency_delta_expression_equal(SZrAstNode *left,
                                                                      SZrAstNode *right,
                                                                      SZrString *targetName) {
    SZrAstNodeArray *leftMembers;
    SZrAstNodeArray *rightMembers;

    if (left == ZR_NULL ||
        right == ZR_NULL ||
        targetName == ZR_NULL ||
        left->type != right->type ||
        !loop_assignment_self_dependency_delta_expression_is_supported(left, targetName) ||
        !loop_assignment_self_dependency_delta_expression_is_supported(right, targetName)) {
        return ZR_FALSE;
    }

    switch (left->type) {
        case ZR_AST_INTEGER_LITERAL:
            return left->data.integerLiteral.value == right->data.integerLiteral.value;
        case ZR_AST_FLOAT_LITERAL:
            return left->data.floatLiteral.value == right->data.floatLiteral.value &&
                   left->data.floatLiteral.isSingle == right->data.floatLiteral.isSingle;
        case ZR_AST_BOOLEAN_LITERAL:
            return left->data.booleanLiteral.value == right->data.booleanLiteral.value;
        case ZR_AST_IDENTIFIER_LITERAL:
            return left->data.identifier.name != ZR_NULL &&
                   right->data.identifier.name != ZR_NULL &&
                   ZrCore_String_Equal(left->data.identifier.name, right->data.identifier.name);
        case ZR_AST_BINARY_EXPRESSION:
            if (!loop_assignment_self_dependency_string_equal(
                        left->data.binaryExpression.op.op,
                        right->data.binaryExpression.op.op)) {
                return ZR_FALSE;
            }
            if (loop_assignment_self_dependency_delta_expression_equal(
                        left->data.binaryExpression.left,
                        right->data.binaryExpression.left,
                        targetName) &&
                loop_assignment_self_dependency_delta_expression_equal(
                        left->data.binaryExpression.right,
                        right->data.binaryExpression.right,
                        targetName)) {
                return ZR_TRUE;
            }
            return loop_assignment_self_dependency_string_equal(
                           left->data.binaryExpression.op.op,
                           "+") &&
                   loop_assignment_self_dependency_plus_terms_equal(left, right, targetName);
        case ZR_AST_LOGICAL_EXPRESSION:
            return loop_assignment_self_dependency_string_equal(
                           left->data.logicalExpression.op,
                           right->data.logicalExpression.op) &&
                   loop_assignment_self_dependency_delta_expression_equal(
                           left->data.logicalExpression.left,
                           right->data.logicalExpression.left,
                           targetName) &&
                   loop_assignment_self_dependency_delta_expression_equal(
                           left->data.logicalExpression.right,
                           right->data.logicalExpression.right,
                           targetName);
        case ZR_AST_UNARY_EXPRESSION:
            return loop_assignment_self_dependency_string_equal(
                           left->data.unaryExpression.op.op,
                           right->data.unaryExpression.op.op) &&
                   loop_assignment_self_dependency_delta_expression_equal(
                           left->data.unaryExpression.argument,
                           right->data.unaryExpression.argument,
                           targetName);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return left->data.typeCastExpression.targetType ==
                           right->data.typeCastExpression.targetType &&
                   loop_assignment_self_dependency_delta_expression_equal(
                           left->data.typeCastExpression.expression,
                           right->data.typeCastExpression.expression,
                           targetName);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return loop_assignment_self_dependency_delta_expression_equal(
                           left->data.conditionalExpression.test,
                           right->data.conditionalExpression.test,
                           targetName) &&
                   loop_assignment_self_dependency_delta_expression_equal(
                           left->data.conditionalExpression.consequent,
                           right->data.conditionalExpression.consequent,
                           targetName) &&
                   loop_assignment_self_dependency_delta_expression_equal(
                           left->data.conditionalExpression.alternate,
                           right->data.conditionalExpression.alternate,
                           targetName);
        case ZR_AST_PRIMARY_EXPRESSION:
            leftMembers = left->data.primaryExpression.members;
            rightMembers = right->data.primaryExpression.members;
            if ((leftMembers != ZR_NULL && leftMembers->count > 0) ||
                (rightMembers != ZR_NULL && rightMembers->count > 0)) {
                return ZR_FALSE;
            }
            return loop_assignment_self_dependency_delta_expression_equal(
                    left->data.primaryExpression.property,
                    right->data.primaryExpression.property,
                    targetName);
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpressionsEqual(
        SZrAstNode *left,
        SZrAstNode *right,
        SZrString *targetName) {
    return loop_assignment_self_dependency_delta_expression_equal(left, right, targetName);
}

static TZrBool loop_assignment_self_dependency_target_array_contains(const SZrArray *targetNames,
                                                                     SZrString *name) {
    TZrSize index;

    if (targetNames == ZR_NULL || !targetNames->isValid || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < targetNames->length; index++) {
        SZrString **target = (SZrString **)ZrCore_Array_Get((SZrArray *)targetNames, index);
        if (target != ZR_NULL &&
            *target != ZR_NULL &&
            ZrCore_String_Equal(*target, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool loop_assignment_self_dependency_expression_uses_any_target(SZrAstNode *node,
                                                                          const SZrArray *targetNames) {
    SZrAstNodeArray *members;

    if (node == ZR_NULL || targetNames == ZR_NULL || !targetNames->isValid) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return loop_assignment_self_dependency_target_array_contains(
                    targetNames,
                    node->data.identifier.name);
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_FALSE;
        case ZR_AST_BINARY_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.binaryExpression.left,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.binaryExpression.right,
                           targetNames);
        case ZR_AST_LOGICAL_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.logicalExpression.left,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.logicalExpression.right,
                           targetNames);
        case ZR_AST_UNARY_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                    node->data.unaryExpression.argument,
                    targetNames);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                    node->data.typeCastExpression.expression,
                    targetNames);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.conditionalExpression.test,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.conditionalExpression.consequent,
                           targetNames) ||
                   loop_assignment_self_dependency_expression_uses_any_target(
                           node->data.conditionalExpression.alternate,
                           targetNames);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = node->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_TRUE;
            }
            return loop_assignment_self_dependency_expression_uses_any_target(
                    node->data.primaryExpression.property,
                    targetNames);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaUsesAnyTarget(
        SZrAstNode *right,
        SZrString *targetName,
        const SZrArray *targetNames) {
    SZrAstNode *deltaNode = ZrParser_TypeInferenceLoopAssignment_SelfDependentDeltaExpression(
            right,
            targetName);

    if (deltaNode == ZR_NULL || targetNames == ZR_NULL || !targetNames->isValid) {
        return ZR_FALSE;
    }
    return loop_assignment_self_dependency_expression_uses_any_target(deltaNode, targetNames);
}

TZrBool ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(SZrAstNode *node,
                                                                SZrString *name) {
    SZrAstNodeArray *members;

    if (node == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return node->data.identifier.name != ZR_NULL &&
                   ZrCore_String_Equal(node->data.identifier.name, name);
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_BOOLEAN_LITERAL:
            return ZR_FALSE;
        case ZR_AST_BINARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.binaryExpression.left,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.binaryExpression.right,
                           name);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.logicalExpression.left,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.logicalExpression.right,
                           name);
        case ZR_AST_UNARY_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    node->data.unaryExpression.argument,
                    name);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    node->data.typeCastExpression.expression,
                    name);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.conditionalExpression.test,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.conditionalExpression.consequent,
                           name) ||
                   ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                           node->data.conditionalExpression.alternate,
                           name);
        case ZR_AST_PRIMARY_EXPRESSION:
            members = node->data.primaryExpression.members;
            if (members != ZR_NULL && members->count > 0) {
                return ZR_TRUE;
            }
            return ZrParser_TypeInferenceLoopAssignment_ExpressionUsesName(
                    node->data.primaryExpression.property,
                    name);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_TypeInferenceLoopAssignment_WidenSelfDependentType(SZrCompilerState *cs,
                                                                    const SZrInferredType *sourceType,
                                                                    TZrInt64 deltaMin,
                                                                    TZrInt64 deltaMax,
                                                                    SZrInferredType *outType) {
    TZrInt64 oneIterationMax = 0;
    TZrInt64 oneIterationMin = 0;
    TZrBool widensUpper;
    TZrBool widensLower;
    TZrBool widensBoth;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        sourceType == ZR_NULL ||
        outType == ZR_NULL ||
        sourceType->baseType != ZR_VALUE_TYPE_INT64 ||
        !sourceType->hasRangeConstraint) {
        return ZR_FALSE;
    }

    widensUpper = deltaMin >= 0 && deltaMax > 0;
    widensLower = deltaMin < 0 && deltaMax <= 0;
    widensBoth = deltaMin < 0 && deltaMax > 0;

    if (deltaMin == 0 && deltaMax == 0) {
        ZrParser_InferredType_Copy(cs->state, outType, sourceType);
        return ZR_TRUE;
    }

    if (widensUpper &&
        !loop_assignment_self_dependency_int64_add(sourceType->minValue, deltaMin, &oneIterationMin)) {
        return ZR_FALSE;
    }

    if (widensLower &&
        !loop_assignment_self_dependency_int64_add(sourceType->maxValue, deltaMax, &oneIterationMax)) {
        return ZR_FALSE;
    }

    if (!(widensUpper || widensLower || widensBoth)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(cs->state, outType, sourceType);
    ZrParser_NumericRangeSegments_Free(cs->state,
                                       &outType->rangeSegmentCount,
                                       outType->rangeSegments,
                                       &outType->rangeExtraSegments);
    outType->baseType = ZR_VALUE_TYPE_INT64;
    outType->hasRangeConstraint = ZR_TRUE;
    if (widensBoth) {
        outType->minValue = ZR_TYPE_RANGE_INT64_MIN;
        outType->maxValue = ZR_TYPE_RANGE_INT64_MAX;
    } else if (widensUpper) {
        outType->minValue = oneIterationMin;
        outType->maxValue = ZR_TYPE_RANGE_INT64_MAX;
    } else {
        outType->minValue = ZR_TYPE_RANGE_INT64_MIN;
        outType->maxValue = oneIterationMax;
    }
    return ZR_TRUE;
}
