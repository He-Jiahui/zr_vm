#include "exec_ir/source/exec_ir_source_callable_query_internal.h"
#include "zr_vm_parser/semantic.h"

#include <stdint.h>
#include <string.h>

/* Three independent headers and the two traversed array allocations. */
enum { SOURCE_CALLABLE_SPAN_LIMIT = 3 + 2 };

typedef struct SSourceCallableSpan {
    uintptr_t start;
    uintptr_t end;
} SSourceCallableSpan;

typedef struct SSourceCallableStorage {
    SSourceCallableSpan spans[SOURCE_CALLABLE_SPAN_LIMIT];
    size_t count;
} SSourceCallableStorage;

static TZrBool callable_fail(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, TZrMetadataToken token,
                             TZrUInt32 expectedVersion, TZrUInt32 actualVersion, TZrUInt64 expectedHash,
                             TZrUInt64 actualHash) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = token;
        diagnostic->expectedVersion = expectedVersion;
        diagnostic->actualVersion = actualVersion;
        diagnostic->expectedHash = expectedHash;
        diagnostic->actualHash = actualHash;
    }
    return ZR_FALSE;
}

/* Numeric spans require genuine typed readable storage from the caller. */
static TZrBool callable_span(SSourceCallableStorage *storage, const void *pointer, size_t capacity, size_t width,
                             TZrMetadataToken token, SZrExecIrDiagnostic *diagnostic) {
    uintptr_t start, end;
    size_t bytes, i;
    if (width == 0u || capacity > SIZE_MAX / width)
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, token, 0u, 0u, 0u, 0u);
    if ((pointer == ZR_NULL) != (capacity == 0u))
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, token, 0u, 0u, 0u, 0u);
    if (capacity == 0u)
        return ZR_TRUE;
    bytes = capacity * width;
    start = (uintptr_t) pointer;
    if ((uintmax_t) bytes > (uintmax_t) UINTPTR_MAX || start > UINTPTR_MAX - (uintptr_t) bytes ||
        storage->count >= SOURCE_CALLABLE_SPAN_LIMIT)
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, token, 0u, 0u, 0u, 0u);
    end = start + (uintptr_t) bytes;
    for (i = 0u; i < storage->count; ++i) {
        if (start < storage->spans[i].end && storage->spans[i].start < end)
            return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, token, 0u, 0u, 0u, 0u);
    }
    storage->spans[storage->count].start = start;
    storage->spans[storage->count++].end = end;
    return ZR_TRUE;
}

static TZrBool callable_array(SSourceCallableStorage *storage, const SZrArray *array, size_t width,
                              TZrMetadataToken token, SZrExecIrDiagnostic *diagnostic) {
    if (!array->isValid || array->elementSize != width || array->length > array->capacity)
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, token, 0u, 0u, 0u, 0u);
    return callable_span(storage, array->head, array->capacity, width, token, diagnostic);
}

static TZrBool callable_node(const SZrCanonicalTypeNode *node, TZrTypeId requested, EZrCanonicalTypeKind kind,
                             TZrMetadataToken token, SZrExecIrDiagnostic *diagnostic) {
    if (node == ZR_NULL)
        return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, token, requested, 0u, 0u, 0u);
    if (node->id != requested)
        return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, token, requested, node->id, 0u,
                             0u);
    if (node->kind != kind)
        return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, token, (TZrUInt32) kind,
                             (TZrUInt32) node->kind, 0u, 0u);
    if (node->structuralHash == 0u)
        return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, token, 0u, 0u, 1u, 0u);
    return ZR_TRUE;
}

TZrBool source_callable_query(const struct SZrSemanticContext *context, TZrTypeId callableTypeId,
                              TZrMetadataToken functionToken, SSourceCallableFacts *output,
                              SZrExecIrDiagnostic *diagnostic) {
    SSourceCallableStorage storage = {0};
    SSourceCallableFacts candidate;
    const SZrCanonicalTypeNode *nodes, *callable, *result;
    const SZrCanonicalFunctionType *function;
    TZrTypeId previous = ZR_SEMANTIC_ID_INVALID;
    size_t i;
    if (diagnostic != ZR_NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
    if (context == ZR_NULL || output == ZR_NULL)
        return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u, 0u, 0u, 0u, 0u);
    if (callableTypeId == ZR_SEMANTIC_ID_INVALID)
        return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, functionToken, 1u, 0u, 0u, 0u);
    if (!callable_span(&storage, context, 1u, sizeof(*context), functionToken, diagnostic) ||
        !callable_span(&storage, output, 1u, sizeof(*output), functionToken, diagnostic) ||
        (diagnostic != ZR_NULL &&
         !callable_span(&storage, diagnostic, 1u, sizeof(*diagnostic), functionToken, diagnostic)) ||
        !callable_array(&storage, &context->canonicalTypes, sizeof(SZrCanonicalTypeNode), functionToken, diagnostic))
        return ZR_FALSE;

    nodes = (const SZrCanonicalTypeNode *) context->canonicalTypes.head;
    for (i = 0u; i < context->canonicalTypes.length; ++i) {
        if (nodes[i].id == ZR_SEMANTIC_ID_INVALID)
            return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, functionToken, 1u, 0u, 0u, 0u);
        if (i != 0u && nodes[i].id <= previous)
            return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, functionToken, previous,
                                 nodes[i].id, 0u, 0u);
        previous = nodes[i].id;
    }

    callable = ZrParser_CanonicalType_Find(context, callableTypeId);
    if (!callable_node(callable, callableTypeId, ZR_CANONICAL_TYPE_FUNCTION, functionToken, diagnostic))
        return ZR_FALSE;
    function = &callable->data.function;
    if (!callable_array(&storage, &function->parameterContracts, sizeof(SZrCanonicalParameterContract), functionToken,
                        diagnostic))
        return ZR_FALSE;
    if (function->parameterContracts.length != 0u)
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, functionToken, 0u, 0u, 0u, 0u);
    if (function->receiverEffect != ZR_CANONICAL_RECEIVER_NONE)
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, functionToken, 0u, 0u, 0u, 0u);
    if (function->effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE)
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, functionToken, 0u, 0u, 0u, 0u);
    if (function->returnTypeId == ZR_SEMANTIC_ID_INVALID)
        return callable_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, functionToken, 1u, 0u, 0u, 0u);
    result = ZrParser_CanonicalType_Find(context, function->returnTypeId);
    if (!callable_node(result, function->returnTypeId, ZR_CANONICAL_TYPE_PRIMITIVE, functionToken, diagnostic))
        return ZR_FALSE;
    if (result->data.primitive.valueType != ZR_VALUE_TYPE_INT64)
        return callable_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, functionToken, 0u, 0u, 0u, 0u);

    memset(&candidate, 0, sizeof(candidate));
    candidate.callableTypeId = callable->id;
    candidate.returnTypeId = result->id;
    candidate.signatureHash = callable->structuralHash;
    candidate.returnStructuralHash = result->structuralHash;
    candidate.returnPrimitive = result->data.primitive.valueType;
    memcpy(output, &candidate, sizeof(candidate));
    return ZR_TRUE;
}
