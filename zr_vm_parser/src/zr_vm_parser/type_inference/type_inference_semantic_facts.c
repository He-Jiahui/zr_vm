#include "type_inference_semantic_facts.h"

#include <limits.h>
#include <string.h>

#include "type_inference_constant_eval.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

static TZrBool type_inference_value_type_is_unsigned(EZrValueType baseType) {
    return ZR_VALUE_IS_TYPE_UNSIGNED_INT(baseType);
}

static TZrBool type_inference_numeric_type_has_signed_range(EZrValueType baseType,
                                                            TZrInt64 *outMinValue,
                                                            TZrInt64 *outMaxValue) {
    if (outMinValue == ZR_NULL || outMaxValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (baseType) {
        case ZR_VALUE_TYPE_INT8:
            *outMinValue = ZR_TYPE_RANGE_INT8_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT8_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT16:
            *outMinValue = ZR_TYPE_RANGE_INT16_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT16_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT32:
            *outMinValue = ZR_TYPE_RANGE_INT32_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT32_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT64:
            *outMinValue = ZR_TYPE_RANGE_INT64_MIN;
            *outMaxValue = ZR_TYPE_RANGE_INT64_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT8_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT16:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT16_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT32:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT32_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT64:
            *outMinValue = 0;
            *outMaxValue = ZR_TYPE_RANGE_UINT64_MAX;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_numeric_type_unsigned_max(EZrValueType baseType,
                                                        TZrUInt64 *outMaxValue) {
    if (outMaxValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (baseType) {
        case ZR_VALUE_TYPE_UINT8:
            *outMaxValue = (TZrUInt64)UINT8_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT16:
            *outMaxValue = (TZrUInt64)UINT16_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT32:
            *outMaxValue = (TZrUInt64)UINT32_MAX;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT64:
            *outMaxValue = (TZrUInt64)UINT64_MAX;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_int64_add(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
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

static TZrBool type_inference_int64_sub(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((right > 0 && left < ZR_TYPE_RANGE_INT64_MIN + right) ||
        (right < 0 && left > ZR_TYPE_RANGE_INT64_MAX + right)) {
        return ZR_FALSE;
    }

    *outValue = left - right;
    return ZR_TRUE;
}

static TZrBool type_inference_int64_mul(TZrInt64 left, TZrInt64 right, TZrInt64 *outValue) {
    long double product;

    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    product = (long double)left * (long double)right;
    if (product < (long double)ZR_TYPE_RANGE_INT64_MIN ||
        product > (long double)ZR_TYPE_RANGE_INT64_MAX) {
        return ZR_FALSE;
    }

    *outValue = (TZrInt64)product;
    return ZR_TRUE;
}

static TZrBool type_inference_int64_mul_range(TZrInt64 leftMin,
                                              TZrInt64 leftMax,
                                              TZrInt64 rightMin,
                                              TZrInt64 rightMax,
                                              TZrInt64 *outMinValue,
                                              TZrInt64 *outMaxValue) {
    TZrInt64 products[4];
    TZrSize index;

    if (outMinValue == ZR_NULL || outMaxValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!type_inference_int64_mul(leftMin, rightMin, &products[0]) ||
        !type_inference_int64_mul(leftMin, rightMax, &products[1]) ||
        !type_inference_int64_mul(leftMax, rightMin, &products[2]) ||
        !type_inference_int64_mul(leftMax, rightMax, &products[3])) {
        return ZR_FALSE;
    }

    *outMinValue = products[0];
    *outMaxValue = products[0];
    for (index = 1; index < 4; index++) {
        if (products[index] < *outMinValue) {
            *outMinValue = products[index];
        }
        if (products[index] > *outMaxValue) {
            *outMaxValue = products[index];
        }
    }

    return ZR_TRUE;
}

static TZrBool type_inference_node_integer_binary_result(const TZrChar *op,
                                                         TZrInt64 left,
                                                         TZrInt64 right,
                                                         TZrInt64 *outValue) {
    if (op == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(op, "+") == 0) {
        return type_inference_int64_add(left, right, outValue);
    }
    if (strcmp(op, "-") == 0) {
        return type_inference_int64_sub(left, right, outValue);
    }
    if (strcmp(op, "*") == 0) {
        return type_inference_int64_mul(left, right, outValue);
    }
    if (strcmp(op, "/") == 0 && right != 0) {
        if (left == ZR_TYPE_RANGE_INT64_MIN && right == -1) {
            return ZR_FALSE;
        }
        *outValue = left / right;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static EZrSemanticExpressionFactKind type_inference_expression_fact_kind(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN;
    }

    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
        case ZR_AST_IDENTIFIER_LITERAL:
            return ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER;
        case ZR_AST_PRIMARY_EXPRESSION:
        case ZR_AST_MEMBER_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_MEMBER;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return node->data.constructExpression.builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE ||
                   node->data.constructExpression.ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE
                           ? ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN
                           : ZR_SEMANTIC_EXPRESSION_FACT_CALL;
        case ZR_AST_BINARY_EXPRESSION:
        case ZR_AST_LOGICAL_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_BINARY;
        case ZR_AST_UNARY_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_UNARY;
        case ZR_AST_FUNCTION_CALL:
            return ZR_SEMANTIC_EXPRESSION_FACT_CALL;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_CONDITIONAL;
        case ZR_AST_ARRAY_LITERAL:
            return ZR_SEMANTIC_EXPRESSION_FACT_ARRAY;
        case ZR_AST_OBJECT_LITERAL:
            return ZR_SEMANTIC_EXPRESSION_FACT_OBJECT;
        case ZR_AST_LAMBDA_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ZR_SEMANTIC_EXPRESSION_FACT_CONVERSION;
        default:
            return ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN;
    }
}

static EZrSemanticValueKind type_inference_semantic_value_kind(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_SEMANTIC_VALUE_KIND_NULL;
        case ZR_VALUE_TYPE_BOOL:
            return ZR_SEMANTIC_VALUE_KIND_BOOL;
        ZR_VALUE_CASES_SIGNED_INT
            return ZR_SEMANTIC_VALUE_KIND_INT64;
        ZR_VALUE_CASES_UNSIGNED_INT
            return ZR_SEMANTIC_VALUE_KIND_UINT64;
        ZR_VALUE_CASES_FLOAT
            return ZR_SEMANTIC_VALUE_KIND_DOUBLE;
        case ZR_VALUE_TYPE_STRING:
            return ZR_SEMANTIC_VALUE_KIND_STRING;
        default:
            return ZR_SEMANTIC_VALUE_KIND_UNKNOWN;
    }
}

static SZrString *type_inference_identifier_name(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return node->data.identifier.name;
}

static void type_inference_record_expression_constant(SZrAstNode *node,
                                                      const SZrInferredType *type,
                                                      SZrSemanticExpressionFact *fact) {
    TZrInt64 intValue;
    TZrDouble doubleValue;
    TZrBool boolValue;

    if (node == ZR_NULL || type == ZR_NULL || fact == ZR_NULL) {
        return;
    }

    if (type->baseType == ZR_VALUE_TYPE_BOOL && type_inference_node_bool_value(node, &boolValue)) {
        fact->hasConstant = ZR_TRUE;
        fact->constantValue.boolValue = boolValue;
        return;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(type->baseType) && type_inference_node_integer_value(node, &intValue) &&
        intValue >= 0) {
        fact->hasConstant = ZR_TRUE;
        fact->constantValue.uint64Value = (TZrUInt64)intValue;
        return;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(type->baseType) && type_inference_node_integer_value(node, &intValue)) {
        fact->hasConstant = ZR_TRUE;
        fact->constantValue.int64Value = intValue;
        return;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(type->baseType) && type_inference_node_double_value(node, &doubleValue)) {
        fact->hasConstant = ZR_TRUE;
        fact->constantValue.doubleValue = doubleValue;
        return;
    }

    if (type->baseType == ZR_VALUE_TYPE_STRING && node->type == ZR_AST_STRING_LITERAL) {
        fact->hasConstant = ZR_TRUE;
        fact->constantValue.stringValue = node->data.stringLiteral.value;
    }
}

static SZrAstNode *type_inference_last_primary_member_of_type(const SZrPrimaryExpression *primary,
                                                              EZrAstNodeType type) {
    TZrSize index;

    if (primary == ZR_NULL || primary->members == ZR_NULL || primary->members->count == 0) {
        return ZR_NULL;
    }

    for (index = primary->members->count; index > 0; index--) {
        SZrAstNode *member = primary->members->nodes[index - 1];
        if (member != ZR_NULL && member->type == type) {
            return member;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *type_inference_primary_call_target(const SZrPrimaryExpression *primary,
                                                      SZrAstNode *callNode,
                                                      TZrBool *outIsMemberCall) {
    SZrAstNode *candidate = ZR_NULL;

    if (outIsMemberCall != ZR_NULL) {
        *outIsMemberCall = ZR_FALSE;
    }
    if (primary == ZR_NULL || callNode == ZR_NULL || primary->members == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < primary->members->count; index++) {
        SZrAstNode *member = primary->members->nodes[index];
        if (member == callNode) {
            break;
        }
        if (member != ZR_NULL && member->type == ZR_AST_MEMBER_EXPRESSION) {
            candidate = member->data.memberExpression.property;
            if (outIsMemberCall != ZR_NULL) {
                *outIsMemberCall = ZR_TRUE;
            }
        }
    }

    if (candidate != ZR_NULL) {
        return candidate;
    }

    return primary->property;
}

static void type_inference_record_primary_call_info(SZrCompilerState *cs,
                                                   const SZrPrimaryExpression *primary,
                                                   SZrSemanticExpressionFact *fact) {
    SZrAstNode *callNode;
    SZrAstNode *target;
    SZrFunctionCall *call;
    TZrBool isMemberCall = ZR_FALSE;

    if (cs == ZR_NULL || primary == ZR_NULL || fact == ZR_NULL) {
        return;
    }

    callNode = type_inference_last_primary_member_of_type(primary, ZR_AST_FUNCTION_CALL);
    if (callNode == ZR_NULL) {
        return;
    }

    call = &callNode->data.functionCall;
    target = type_inference_primary_call_target(primary, callNode, &isMemberCall);

    fact->hasCallInfo = ZR_TRUE;
    fact->callTargetName = type_inference_identifier_name(target);
    fact->callTargetRange = target != ZR_NULL ? target->location : callNode->location;
    fact->argumentCount = call->args != ZR_NULL ? call->args->count : 0;
    fact->hasNamedArguments = call->hasNamedArgs;
    fact->isMemberCall = isMemberCall;
}

static void type_inference_record_primary_member_info(const SZrPrimaryExpression *primary,
                                                     SZrSemanticExpressionFact *fact) {
    SZrAstNode *memberNode;
    SZrAstNode *property;

    if (primary == ZR_NULL || fact == ZR_NULL) {
        return;
    }

    memberNode = type_inference_last_primary_member_of_type(primary, ZR_AST_MEMBER_EXPRESSION);
    if (memberNode == ZR_NULL) {
        return;
    }

    property = memberNode->data.memberExpression.property;
    fact->hasMemberInfo = ZR_TRUE;
    fact->memberName = type_inference_identifier_name(property);
    fact->memberRange = property != ZR_NULL ? property->location : memberNode->location;
    fact->memberIsComputed = memberNode->data.memberExpression.computed;
}

static void type_inference_record_expression_payload(SZrCompilerState *cs,
                                                     SZrAstNode *node,
                                                     SZrSemanticExpressionFact *fact) {
    if (cs == ZR_NULL || node == ZR_NULL || fact == ZR_NULL) {
        return;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION) {
        const SZrPrimaryExpression *primary = &node->data.primaryExpression;
        type_inference_record_primary_member_info(primary, fact);
        type_inference_record_primary_call_info(cs, primary, fact);
    } else if (node->type == ZR_AST_FUNCTION_CALL) {
        const SZrFunctionCall *call = &node->data.functionCall;
        fact->hasCallInfo = ZR_TRUE;
        fact->callTargetRange = node->location;
        fact->argumentCount = call->args != ZR_NULL ? call->args->count : 0;
        fact->hasNamedArguments = call->hasNamedArgs;
    } else if (node->type == ZR_AST_MEMBER_EXPRESSION) {
        SZrAstNode *property = node->data.memberExpression.property;
        fact->hasMemberInfo = ZR_TRUE;
        fact->memberName = type_inference_identifier_name(property);
        fact->memberRange = property != ZR_NULL ? property->location : node->location;
        fact->memberIsComputed = node->data.memberExpression.computed;
    }
}

void type_inference_record_expression_fact(SZrCompilerState *cs,
                                           SZrAstNode *node,
                                           const SZrInferredType *type) {
    SZrSemanticExpressionFact fact;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || node == ZR_NULL || type == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = type_inference_expression_fact_kind(node);
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.valueKind = type_inference_semantic_value_kind(type->baseType);
    ZrParser_InferredType_Copy(cs->state, &fact.inferredType, type);
    type_inference_record_expression_constant(node, type, &fact);
    type_inference_record_expression_payload(cs, node, &fact);
    ZrParser_SemanticFacts_AppendExpression(cs->semanticContext, &fact);
    ZrParser_InferredType_Free(cs->state, &fact.inferredType);
}

void type_inference_record_primary_call_reference_fact(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrAstNode *callNode,
                                                       const SZrFunctionTypeInfo *funcTypeInfo) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *target;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION ||
        callNode == ZR_NULL ||
        callNode->type != ZR_AST_FUNCTION_CALL ||
        funcTypeInfo == ZR_NULL ||
        funcTypeInfo->name == ZR_NULL) {
        return;
    }

    target = type_inference_primary_call_target(&node->data.primaryExpression, callNode, ZR_NULL);

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = target != ZR_NULL ? target->location : callNode->location;
    fact.declarationRange = funcTypeInfo->hasDeclarationRange ? funcTypeInfo->declarationRange : fact.range;
    fact.kind = ZR_SEMANTIC_REFERENCE_CALL;
    fact.symbolId = funcTypeInfo->symbolId;
    fact.typeId = funcTypeInfo->typeId;
    fact.name = funcTypeInfo->name;
    fact.isResolved = ZR_TRUE;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

void type_inference_record_identifier_write_reference_fact(SZrCompilerState *cs,
                                                           SZrAstNode *node,
                                                           const SZrTypeBinding *binding) {
    SZrSemanticReferenceFact fact;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_IDENTIFIER_LITERAL ||
        binding == ZR_NULL ||
        binding->name == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.declarationRange = binding->hasDeclarationRange ? binding->declarationRange : node->location;
    fact.kind = ZR_SEMANTIC_REFERENCE_WRITE;
    fact.symbolId = binding->symbolId;
    fact.typeId = binding->typeId;
    fact.name = binding->name;
    fact.isResolved = ZR_TRUE;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

static void type_inference_record_member_reference_fact(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        EZrSemanticReferenceKind kind) {
    SZrSemanticReferenceFact fact;
    SZrAstNode *memberNode;
    SZrAstNode *property;
    TZrBool computedAccess;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION ||
        (kind != ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
         kind != ZR_SEMANTIC_REFERENCE_MEMBER_WRITE)) {
        return;
    }

    memberNode = type_inference_last_primary_member_of_type(&node->data.primaryExpression,
                                                            ZR_AST_MEMBER_EXPRESSION);
    if (memberNode == ZR_NULL) {
        return;
    }

    property = memberNode->data.memberExpression.property;
    computedAccess = kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
                     memberNode->data.memberExpression.computed;

    if (computedAccess && property != ZR_NULL) {
        SZrInferredType propertyType;
        ZrParser_InferredType_Init(cs->state, &propertyType, ZR_VALUE_TYPE_OBJECT);
        (void)ZrParser_ExpressionType_Infer(cs, property, &propertyType);
        ZrParser_InferredType_Free(cs->state, &propertyType);
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = computedAccess || property == ZR_NULL ? memberNode : property;
    fact.range = computedAccess || property == ZR_NULL ? memberNode->location : property->location;
    fact.declarationRange = fact.range;
    fact.kind = kind;
    fact.symbolId = ZR_SEMANTIC_ID_INVALID;
    fact.typeId = ZR_SEMANTIC_ID_INVALID;
    fact.name = type_inference_identifier_name(property);
    fact.isResolved = ZR_FALSE;
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

void type_inference_record_member_access_reference_fact(SZrCompilerState *cs, SZrAstNode *node) {
    type_inference_record_member_reference_fact(cs, node, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS);
}

void type_inference_record_member_write_reference_fact(SZrCompilerState *cs, SZrAstNode *node) {
    type_inference_record_member_reference_fact(cs, node, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE);
}

static void type_inference_numeric_fact_set_range(SZrSemanticNumericFact *fact,
                                                  TZrInt64 minValue,
                                                  TZrInt64 maxValue) {
    if (fact == ZR_NULL) {
        return;
    }

    fact->hasRange = ZR_TRUE;
    fact->minValue = minValue;
    fact->maxValue = maxValue;
    fact->exactness = ZR_SEMANTIC_FACT_EXACT;
    if (minValue >= 0 && maxValue >= 0) {
        fact->hasUnsignedRange = ZR_TRUE;
        fact->minUnsignedValue = (TZrUInt64)minValue;
        fact->maxUnsignedValue = (TZrUInt64)maxValue;
    }
}

static void type_inference_numeric_fact_set_double_range(SZrSemanticNumericFact *fact,
                                                         TZrDouble minValue,
                                                         TZrDouble maxValue) {
    if (fact == ZR_NULL) {
        return;
    }

    fact->hasRange = ZR_TRUE;
    fact->minDoubleValue = minValue;
    fact->maxDoubleValue = maxValue;
    fact->exactness = ZR_SEMANTIC_FACT_EXACT;
}

static TZrBool type_inference_binary_integer_would_overflow(SZrAstNode *node) {
    TZrInt64 left;
    TZrInt64 right;
    const TZrChar *op;
    TZrInt64 value;

    if (node == ZR_NULL || node->type != ZR_AST_BINARY_EXPRESSION) {
        return ZR_FALSE;
    }

    if (node->data.binaryExpression.left == ZR_NULL ||
        node->data.binaryExpression.right == ZR_NULL ||
        node->data.binaryExpression.left->type != ZR_AST_INTEGER_LITERAL ||
        node->data.binaryExpression.right->type != ZR_AST_INTEGER_LITERAL) {
        return ZR_FALSE;
    }

    op = node->data.binaryExpression.op.op;
    if (op == ZR_NULL) {
        return ZR_FALSE;
    }

    left = node->data.binaryExpression.left->data.integerLiteral.value;
    right = node->data.binaryExpression.right->data.integerLiteral.value;

    if (strcmp(op, "+") == 0 ||
        strcmp(op, "-") == 0 ||
        strcmp(op, "*") == 0) {
        return !type_inference_node_integer_binary_result(op,
                                                          left,
                                                          right,
                                                          &value);
    }

    if (strcmp(op, "/") == 0) {
        return left == ZR_TYPE_RANGE_INT64_MIN && right == -1;
    }

    return ZR_FALSE;
}

void type_inference_record_numeric_fact(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        const SZrInferredType *type,
                                        EZrSemanticNumericFactKind kind) {
    SZrSemanticNumericFact fact;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        type == ZR_NULL ||
        kind == ZR_SEMANTIC_NUMERIC_FACT_UNKNOWN ||
        !ZR_VALUE_IS_TYPE_NUMBER(type->baseType)) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = kind;
    fact.exactness = ZR_SEMANTIC_FACT_APPROXIMATE;
    fact.sourceType = type->baseType;
    fact.targetType = type->baseType;

    if (ZR_VALUE_IS_TYPE_FLOAT(type->baseType)) {
        TZrDouble doubleValue;

        if (type_inference_node_double_value(node, &doubleValue)) {
            type_inference_numeric_fact_set_double_range(&fact, doubleValue, doubleValue);
        }
    } else {
        TZrUInt64 unsignedMaxValue;

        if (type->hasRangeConstraint) {
            type_inference_numeric_fact_set_range(&fact, type->minValue, type->maxValue);
        } else {
            TZrInt64 literalValue;

            if (type_inference_node_integer_value(node, &literalValue)) {
                type_inference_numeric_fact_set_range(&fact, literalValue, literalValue);
            }
        }

        if (type_inference_value_type_is_unsigned(type->baseType)) {
            fact.hasUnsignedRange = ZR_TRUE;
            if (fact.hasRange) {
                fact.minUnsignedValue = fact.minValue >= 0 ? (TZrUInt64)fact.minValue : 0;
                if (type->baseType == ZR_VALUE_TYPE_UINT64 &&
                    type->hasRangeConstraint &&
                    type->minValue == 0 &&
                    type->maxValue == ZR_TYPE_RANGE_UINT64_MAX) {
                    fact.maxUnsignedValue = (TZrUInt64)UINT64_MAX;
                } else {
                    fact.maxUnsignedValue = fact.maxValue >= 0 ? (TZrUInt64)fact.maxValue : 0;
                }
            } else if (type_inference_numeric_type_unsigned_max(type->baseType, &unsignedMaxValue)) {
                fact.minUnsignedValue = 0;
                fact.maxUnsignedValue = unsignedMaxValue;
            }
        } else if (fact.hasRange && fact.minValue >= 0 && fact.maxValue >= 0) {
            fact.hasUnsignedRange = ZR_TRUE;
            fact.minUnsignedValue = (TZrUInt64)fact.minValue;
            fact.maxUnsignedValue = (TZrUInt64)fact.maxValue;
        }
    }

    if (!fact.hasRange &&
        kind == ZR_SEMANTIC_NUMERIC_FACT_PROMOTION &&
        type_inference_binary_integer_would_overflow(node)) {
        fact.mayOverflow = ZR_TRUE;
        fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    }

    ZrParser_SemanticFacts_AppendNumeric(cs->semanticContext, &fact);
}

static void type_inference_record_logical_short_circuit_fact(SZrCompilerState *cs,
                                                             SZrAstNode *node) {
    SZrSemanticLogicalFact fact;
    SZrSemanticReachabilityFact reachabilityFact;
    TZrBool leftValue;
    const TZrChar *op;
    SZrAstNode *right;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_LOGICAL_EXPRESSION ||
        !type_inference_node_bool_value(node->data.logicalExpression.left, &leftValue)) {
        return;
    }

    op = node->data.logicalExpression.op;
    if ((op == ZR_NULL) ||
        ((strcmp(op, "||") == 0 && !leftValue) ||
         (strcmp(op, "&&") == 0 && leftValue))) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.knownValue = leftValue;
    fact.hasKnownValue = ZR_TRUE;
    fact.relatedNode = node->data.logicalExpression.right;
    ZrParser_SemanticFacts_AppendLogical(cs->semanticContext, &fact);

    right = node->data.logicalExpression.right;
    if (right != ZR_NULL) {
        memset(&reachabilityFact, 0, sizeof(reachabilityFact));
        reachabilityFact.node = right;
        reachabilityFact.range = right->location;
        reachabilityFact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
        reachabilityFact.cause = ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT;
        reachabilityFact.causeNode = node->data.logicalExpression.left;
        ZrParser_SemanticFacts_AppendReachability(cs->semanticContext, &reachabilityFact);
    }
}

static void type_inference_record_constant_comparison_logical_fact(SZrCompilerState *cs,
                                                                   SZrAstNode *node) {
    SZrSemanticLogicalFact fact;
    TZrBool knownValue;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_BINARY_EXPRESSION ||
        !type_inference_node_bool_value(node, &knownValue)) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = knownValue
                    ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE
                    : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = ZR_TRUE;
    fact.knownValue = knownValue;
    ZrParser_SemanticFacts_AppendLogical(cs->semanticContext, &fact);
}

static void type_inference_record_logical_expression_constant_fact(SZrCompilerState *cs,
                                                                   SZrAstNode *node) {
    SZrSemanticLogicalFact fact;
    TZrBool leftValue;
    TZrBool knownValue;
    const TZrChar *op;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_LOGICAL_EXPRESSION ||
        !type_inference_node_bool_value(node->data.logicalExpression.left, &leftValue)) {
        return;
    }

    op = node->data.logicalExpression.op;
    if (op == ZR_NULL ||
        (strcmp(op, "||") == 0 && leftValue) ||
        (strcmp(op, "&&") == 0 && !leftValue) ||
        !type_inference_node_bool_value(node, &knownValue)) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = knownValue
                    ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE
                    : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = ZR_TRUE;
    fact.knownValue = knownValue;
    fact.relatedNode = node->data.logicalExpression.right;
    ZrParser_SemanticFacts_AppendLogical(cs->semanticContext, &fact);
}

static void type_inference_record_unary_logical_fact(SZrCompilerState *cs,
                                                     SZrAstNode *node) {
    SZrSemanticLogicalFact fact;
    TZrBool operandValue;
    const TZrChar *op;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_UNARY_EXPRESSION ||
        !type_inference_node_bool_value(node->data.unaryExpression.argument, &operandValue)) {
        return;
    }

    op = node->data.unaryExpression.op.op;
    if (op == ZR_NULL || strcmp(op, "!") != 0) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = operandValue
                    ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE
                    : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.knownValue = operandValue ? ZR_FALSE : ZR_TRUE;
    fact.hasKnownValue = ZR_TRUE;
    fact.relatedNode = node->data.unaryExpression.argument;
    ZrParser_SemanticFacts_AppendLogical(cs->semanticContext, &fact);
}

void type_inference_record_expression_and_numeric_facts(SZrCompilerState *cs,
                                                        SZrAstNode *node,
                                                        const SZrInferredType *type,
                                                        EZrSemanticNumericFactKind numericKind) {
    type_inference_record_expression_fact(cs, node, type);
    type_inference_record_numeric_fact(cs, node, type, numericKind);
    type_inference_record_constant_comparison_logical_fact(cs, node);
    type_inference_record_logical_expression_constant_fact(cs, node);
    type_inference_record_unary_logical_fact(cs, node);
    type_inference_record_logical_short_circuit_fact(cs, node);
}

void type_inference_record_constant_conditional_branch_facts(SZrCompilerState *cs,
                                                             SZrAstNode *node,
                                                             TZrBool conditionValue) {
    SZrSemanticLogicalFact logicalFact;
    SZrSemanticReachabilityFact reachabilityFact;
    SZrAstNode *testNode;
    SZrAstNode *skippedNode;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        return;
    }

    testNode = node->data.conditionalExpression.test;
    skippedNode = conditionValue ? node->data.conditionalExpression.alternate
                                 : node->data.conditionalExpression.consequent;

    memset(&logicalFact, 0, sizeof(logicalFact));
    logicalFact.node = testNode;
    logicalFact.range = testNode != ZR_NULL ? testNode->location : node->location;
    logicalFact.kind = conditionValue ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE;
    logicalFact.exactness = ZR_SEMANTIC_FACT_EXACT;
    logicalFact.knownValue = conditionValue;
    logicalFact.hasKnownValue = ZR_TRUE;
    logicalFact.relatedNode = skippedNode;
    ZrParser_SemanticFacts_AppendLogical(cs->semanticContext, &logicalFact);

    if (skippedNode != ZR_NULL) {
        memset(&reachabilityFact, 0, sizeof(reachabilityFact));
        reachabilityFact.node = skippedNode;
        reachabilityFact.range = skippedNode->location;
        reachabilityFact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
        reachabilityFact.cause = ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH;
        reachabilityFact.causeNode = testNode;
        ZrParser_SemanticFacts_AppendReachability(cs->semanticContext, &reachabilityFact);
    }
}

void type_inference_apply_literal_numeric_range(SZrAstNode *node,
                                                SZrInferredType *type) {
    TZrInt64 literalValue;

    if (node == ZR_NULL || type == ZR_NULL || !ZR_VALUE_IS_TYPE_NUMBER(type->baseType)) {
        return;
    }

    if (type_inference_node_integer_value(node, &literalValue)) {
        type->minValue = literalValue;
        type->maxValue = literalValue;
        type->hasRangeConstraint = ZR_TRUE;
        return;
    }

    if (node->type == ZR_AST_FLOAT_LITERAL) {
        type->hasRangeConstraint = ZR_FALSE;
    }
}

void type_inference_apply_primitive_numeric_range(SZrInferredType *type) {
    if (type == ZR_NULL || !ZR_VALUE_IS_TYPE_NUMBER(type->baseType)) {
        return;
    }

    if (type_inference_numeric_type_has_signed_range(type->baseType, &type->minValue, &type->maxValue)) {
        type->hasRangeConstraint = ZR_TRUE;
    }
}

void type_inference_apply_binary_numeric_range(const TZrChar *op,
                                               const SZrInferredType *leftType,
                                               const SZrInferredType *rightType,
                                               SZrInferredType *result) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (op == ZR_NULL ||
        leftType == ZR_NULL ||
        rightType == ZR_NULL ||
        result == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_NUMBER(result->baseType) ||
        !leftType->hasRangeConstraint ||
        !rightType->hasRangeConstraint) {
        return;
    }

    if (strcmp(op, "+") == 0) {
        if (!type_inference_int64_add(leftType->minValue, rightType->minValue, &minValue) ||
            !type_inference_int64_add(leftType->maxValue, rightType->maxValue, &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
    } else if (strcmp(op, "-") == 0) {
        if (!type_inference_int64_sub(leftType->minValue, rightType->maxValue, &minValue) ||
            !type_inference_int64_sub(leftType->maxValue, rightType->minValue, &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
    } else if (strcmp(op, "*") == 0) {
        if (!type_inference_int64_mul_range(leftType->minValue,
                                            leftType->maxValue,
                                            rightType->minValue,
                                            rightType->maxValue,
                                            &minValue,
                                            &maxValue)) {
            result->hasRangeConstraint = ZR_FALSE;
            return;
        }
        result->minValue = minValue;
        result->maxValue = maxValue;
        result->hasRangeConstraint = ZR_TRUE;
    }
}

void type_inference_apply_unary_numeric_range(const TZrChar *op,
                                              const SZrInferredType *operandType,
                                              SZrInferredType *result) {
    if (op == ZR_NULL || operandType == ZR_NULL || result == ZR_NULL || !operandType->hasRangeConstraint) {
        return;
    }

    if (strcmp(op, "+") == 0) {
        result->minValue = operandType->minValue;
        result->maxValue = operandType->maxValue;
        result->hasRangeConstraint = ZR_TRUE;
    } else if (strcmp(op, "-") == 0 &&
               operandType->minValue == operandType->maxValue) {
        result->minValue = -operandType->minValue;
        result->maxValue = result->minValue;
        result->hasRangeConstraint = ZR_TRUE;
    }
}

static EZrSemanticOwnershipFactKind ownership_fact_kind_for_builtin(EZrOwnershipBuiltinKind builtinKind) {
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
        case ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE:
            return ZR_SEMANTIC_OWNERSHIP_FACT_BORROW;
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            return ZR_SEMANTIC_OWNERSHIP_FACT_MOVE;
        case ZR_OWNERSHIP_BUILTIN_KIND_DETACH:
        case ZR_OWNERSHIP_BUILTIN_KIND_RELEASE:
            return ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE;
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARED:
        case ZR_OWNERSHIP_BUILTIN_KIND_WEAK:
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
            return ZR_SEMANTIC_OWNERSHIP_FACT_COPY;
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        default:
            return ZR_SEMANTIC_OWNERSHIP_FACT_UNKNOWN;
    }
}

void type_inference_record_ownership_builtin_fact(SZrCompilerState *cs,
                                                  SZrAstNode *node,
                                                  EZrOwnershipBuiltinKind builtinKind,
                                                  EZrOwnershipQualifier qualifier) {
    SZrSemanticOwnershipFact fact;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_CONSTRUCT_EXPRESSION ||
        builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_NONE) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = ownership_fact_kind_for_builtin(builtinKind);
    fact.qualifier = qualifier;
    fact.symbolId = ZR_SEMANTIC_ID_INVALID;
    fact.lifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
    fact.ownerLifetimeRegionId = ZR_SEMANTIC_ID_INVALID;
    fact.relatedNode = node->data.constructExpression.target;
    fact.isViolation = ZR_FALSE;
    ZrParser_SemanticFacts_AppendOwnership(cs->semanticContext, &fact);
}
