#include "type_inference_semantic_facts.h"

#include <stdio.h>
#include <string.h>

static EZrSemanticValueKind type_inference_array_diagnostic_value_kind(EZrValueType baseType) {
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

static const TZrChar *type_inference_array_index_type_label(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        ZR_VALUE_CASES_SIGNED_INT
            return "signed integer";
        ZR_VALUE_CASES_UNSIGNED_INT
            return "unsigned integer";
        ZR_VALUE_CASES_FLOAT
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_BUFFER:
            return "buffer";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
        case ZR_VALUE_TYPE_CLOSURE_VALUE:
            return "function";
        case ZR_VALUE_TYPE_OBJECT:
            return "object";
        case ZR_VALUE_TYPE_THREAD:
            return "thread";
        case ZR_VALUE_TYPE_NATIVE_POINTER:
        case ZR_VALUE_TYPE_NATIVE_DATA:
        case ZR_VALUE_TYPE_VM_MEMORY:
            return "native";
        case ZR_VALUE_TYPE_UNKNOWN:
        default:
            return "unknown";
    }
}

static void type_inference_record_array_index_bounds_diagnostic_message(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        const SZrInferredType *elementType,
        const TZrChar *message,
        const TZrChar *code,
        EZrSemanticDiagnosticSeverity severity) {
    SZrSemanticExpressionFact fact;
    SZrAstNode *property;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        !memberNode->data.memberExpression.computed ||
        elementType == ZR_NULL ||
        message == ZR_NULL) {
        return;
    }

    property = memberNode->data.memberExpression.property;
    memset(&fact, 0, sizeof(fact));
    fact.node = memberNode;
    fact.range = property != ZR_NULL ? property->location : memberNode->location;
    fact.kind = ZR_SEMANTIC_EXPRESSION_FACT_MEMBER;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.valueKind = type_inference_array_diagnostic_value_kind(elementType->baseType);
    ZrParser_InferredType_Copy(cs->state, &fact.inferredType, elementType);
    fact.hasMemberInfo = ZR_TRUE;
    fact.memberRange = fact.range;
    fact.memberIsComputed = ZR_TRUE;
    fact.diagnosticMessage = ZrCore_String_Create(cs->state, (TZrNativeString)message, strlen(message));
    if (code != ZR_NULL) {
        fact.diagnosticCode = ZrCore_String_Create(cs->state, (TZrNativeString)code, strlen(code));
    }
    fact.diagnosticSeverity = severity;
    (void)ZrParser_SemanticFacts_AppendExpression(cs->semanticContext, &fact);
    ZrParser_InferredType_Free(cs->state, &fact.inferredType);
}

void type_inference_record_array_index_bounds_diagnostic_fact(SZrCompilerState *cs,
                                                              SZrAstNode *memberNode,
                                                              const SZrInferredType *elementType,
                                                              TZrInt64 indexValue,
                                                              TZrSize arraySize,
                                                              TZrBool hasFixedSize) {
    TZrChar message[128];

    if (indexValue < 0) {
        snprintf(message,
                 sizeof(message),
                 "Array index %lld is negative",
                 (long long)indexValue);
    } else if (hasFixedSize) {
        snprintf(message,
                 sizeof(message),
                 "Array index %lld is out of bounds (array size: %zu)",
                 (long long)indexValue,
                 arraySize);
    } else if (arraySize > 0) {
        snprintf(message,
                 sizeof(message),
                 "Array index %lld is out of bounds (array max size: %zu)",
                 (long long)indexValue,
                 arraySize);
    } else {
        snprintf(message,
                 sizeof(message),
                 "Array index %lld is out of bounds",
                 (long long)indexValue);
    }

    type_inference_record_array_index_bounds_diagnostic_message(
            cs,
            memberNode,
            elementType,
            message,
            "array_index_out_of_bounds",
            ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_ERROR);
}

void type_inference_record_array_index_range_bounds_diagnostic_fact(SZrCompilerState *cs,
                                                                    SZrAstNode *memberNode,
                                                                    const SZrInferredType *elementType,
                                                                    TZrInt64 minValue,
                                                                    TZrInt64 maxValue,
                                                                    TZrSize arraySize,
                                                                    TZrBool hasFixedSize) {
    TZrChar message[160];

    if (minValue == maxValue) {
        type_inference_record_array_index_bounds_diagnostic_fact(
                cs,
                memberNode,
                elementType,
                minValue,
                arraySize,
                hasFixedSize);
        return;
    }

    if (maxValue < 0) {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld is negative",
                 (long long)minValue,
                 (long long)maxValue);
    } else if (hasFixedSize) {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld is out of bounds (array size: %zu)",
                 (long long)minValue,
                 (long long)maxValue,
                 arraySize);
    } else if (arraySize > 0) {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld is out of bounds (array max size: %zu)",
                 (long long)minValue,
                 (long long)maxValue,
                 arraySize);
    } else {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld is out of bounds",
                 (long long)minValue,
                 (long long)maxValue);
    }

    type_inference_record_array_index_bounds_diagnostic_message(
            cs,
            memberNode,
            elementType,
            message,
            "array_index_out_of_bounds",
            ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_ERROR);
}

void type_inference_record_array_index_possible_range_bounds_diagnostic_fact(SZrCompilerState *cs,
                                                                             SZrAstNode *memberNode,
                                                                             const SZrInferredType *elementType,
                                                                             TZrInt64 minValue,
                                                                             TZrInt64 maxValue,
                                                                             TZrSize arraySize,
                                                                             TZrBool hasFixedSize) {
    TZrChar message[184];
    TZrBool hasUpperBound = arraySize > 0;
    const TZrChar *sizeLabel = hasFixedSize ? "array size" : "array max size";

    if (!hasUpperBound && minValue < 0) {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld may be negative",
                 (long long)minValue,
                 (long long)maxValue);
    } else if (minValue < 0 && maxValue >= (TZrInt64)arraySize) {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld may be negative or out of bounds (%s: %zu)",
                 (long long)minValue,
                 (long long)maxValue,
                 sizeLabel,
                 arraySize);
    } else if (minValue < 0) {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld may be negative (%s: %zu)",
                 (long long)minValue,
                 (long long)maxValue,
                 sizeLabel,
                 arraySize);
    } else {
        snprintf(message,
                 sizeof(message),
                 "Array index range %lld..%lld may be out of bounds (%s: %zu)",
                 (long long)minValue,
                 (long long)maxValue,
                 sizeLabel,
                 arraySize);
    }

    type_inference_record_array_index_bounds_diagnostic_message(
            cs,
            memberNode,
            elementType,
            message,
            "array_index_may_be_out_of_bounds",
            ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_WARNING);
}

void type_inference_record_array_index_type_mismatch_diagnostic_fact(
        SZrCompilerState *cs,
        SZrAstNode *memberNode,
        const SZrInferredType *elementType,
        const SZrInferredType *indexType) {
    TZrChar message[128];

    snprintf(message,
             sizeof(message),
             "Array index expression must have an integer type (got %s)",
             type_inference_array_index_type_label(
                     indexType != ZR_NULL ? indexType->baseType : ZR_VALUE_TYPE_UNKNOWN));

    type_inference_record_array_index_bounds_diagnostic_message(
            cs,
            memberNode,
            elementType,
            message,
            "array_index_type_mismatch",
            ZR_SEMANTIC_DIAGNOSTIC_SEVERITY_ERROR);
}
