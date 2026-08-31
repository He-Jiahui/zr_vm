#include "ownership_transfer_cross_domain_internal.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

typedef enum EZrDomainTransportValueKind {
    ZR_DOMAIN_TRANSPORT_VALUE_NULL = 0,
    ZR_DOMAIN_TRANSPORT_VALUE_BOOL,
    ZR_DOMAIN_TRANSPORT_VALUE_SIGNED,
    ZR_DOMAIN_TRANSPORT_VALUE_UNSIGNED,
    ZR_DOMAIN_TRANSPORT_VALUE_FLOAT,
    ZR_DOMAIN_TRANSPORT_VALUE_TEXT,
    ZR_DOMAIN_TRANSPORT_VALUE_OBJECT_REF
} EZrDomainTransportValueKind;

typedef struct SZrDomainTransportValue {
    EZrDomainTransportValueKind kind;
    TZrUInt32 valueType;
    TZrUInt32 objectIndex;
    TZrUInt32 textLength;
    TZrUInt64 unsignedValue;
    TZrInt64 signedValue;
    TZrFloat64 floatValue;
    TZrChar *text;
} SZrDomainTransportValue;

typedef struct SZrDomainTransportField {
    SZrDomainTransportValue key;
    SZrDomainTransportValue value;
} SZrDomainTransportField;

typedef struct SZrDomainTransportNode {
    EZrObjectInternalType internalType;
    TZrUInt32 fieldCount;
    SZrDomainTransportField *fields;
} SZrDomainTransportNode;

struct SZrDomainTransferGraph {
    SZrDomainTransportValue root;
    SZrDomainTransportNode *nodes;
    const SZrObject **sourceObjects;
    SZrState *sourceState;
    TZrUInt32 nodeCount;
    TZrUInt32 nodeCapacity;
    TZrUInt64 byteCount;
    SZrDomainTransferQuota quota;
};

static void domain_transfer_diagnostic_set(
        SZrDomainTransferDiagnostic *diagnostic,
        EZrDomainTransferStatus status,
        TZrUInt32 objects,
        TZrUInt64 bytes,
        TZrUInt32 depth) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->objectCount = objects;
        diagnostic->byteCount = bytes;
        diagnostic->depth = depth;
    }
}

static TZrBool domain_transfer_graph_charge_bytes(
        SZrDomainTransferGraph *graph,
        TZrUInt64 bytes,
        SZrDomainTransferDiagnostic *diagnostic,
        TZrUInt32 depth) {
    if (graph == ZR_NULL || bytes > UINT64_MAX - graph->byteCount ||
        graph->byteCount + bytes > graph->quota.maxBytes) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_BYTE_QUOTA,
                graph != ZR_NULL ? graph->nodeCount : 0u,
                graph != ZR_NULL ? graph->byteCount : 0u,
                depth);
        return ZR_FALSE;
    }
    graph->byteCount += bytes;
    return ZR_TRUE;
}

static void domain_transport_value_free(SZrDomainTransportValue *value) {
    if (value != ZR_NULL && value->text != ZR_NULL) {
        free(value->text);
        value->text = ZR_NULL;
    }
}

void ZrCore_DomainTransferGraph_Free(SZrDomainTransferGraph *graph) {
    if (graph == ZR_NULL) {
        return;
    }
    domain_transport_value_free(&graph->root);
    for (TZrUInt32 nodeIndex = 0u; nodeIndex < graph->nodeCount; nodeIndex++) {
        SZrDomainTransportNode *node = &graph->nodes[nodeIndex];
        for (TZrUInt32 fieldIndex = 0u;
             fieldIndex < node->fieldCount;
             fieldIndex++) {
            domain_transport_value_free(&node->fields[fieldIndex].key);
            domain_transport_value_free(&node->fields[fieldIndex].value);
        }
        free(node->fields);
    }
    free(graph->nodes);
    free(graph->sourceObjects);
    free(graph);
}

static TZrBool domain_transfer_graph_grow_nodes(
        SZrDomainTransferGraph *graph) {
    TZrUInt32 newCapacity;
    SZrDomainTransportNode *newNodes;
    const SZrObject **newSources;

    if (graph->nodeCount < graph->nodeCapacity) {
        return ZR_TRUE;
    }
    newCapacity = graph->nodeCapacity == 0u ? 4u : graph->nodeCapacity * 2u;
    if (newCapacity < graph->nodeCapacity) {
        return ZR_FALSE;
    }
    newNodes = (SZrDomainTransportNode *)realloc(
            graph->nodes, newCapacity * sizeof(*newNodes));
    if (newNodes == ZR_NULL) {
        return ZR_FALSE;
    }
    graph->nodes = newNodes;
    newSources = (const SZrObject **)realloc(
            graph->sourceObjects, newCapacity * sizeof(*newSources));
    if (newSources == ZR_NULL) {
        return ZR_FALSE;
    }
    graph->sourceObjects = newSources;
    memset(
            graph->nodes + graph->nodeCapacity,
            0,
            (newCapacity - graph->nodeCapacity) * sizeof(*graph->nodes));
    graph->nodeCapacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool domain_transfer_graph_encode_value(
        SZrDomainTransferGraph *graph,
        const SZrTypeValue *source,
        TZrUInt32 depth,
        SZrDomainTransportValue *outValue,
        SZrDomainTransferDiagnostic *diagnostic);

static TZrBool domain_transfer_graph_add_object(
        SZrDomainTransferGraph *graph,
        const SZrObject *object,
        TZrUInt32 depth,
        TZrUInt32 *outIndex,
        SZrDomainTransferDiagnostic *diagnostic) {
    TZrUInt32 nodeIndex;
    TZrUInt32 fieldIndex = 0u;
    SZrDomainTransportField *fields = ZR_NULL;

    for (nodeIndex = 0u; nodeIndex < graph->nodeCount; nodeIndex++) {
        if (graph->sourceObjects[nodeIndex] == object) {
            *outIndex = nodeIndex;
            return ZR_TRUE;
        }
    }
    if (depth > graph->quota.maxDepth) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_DEPTH_QUOTA,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    if (object == ZR_NULL || graph->sourceState == ZR_NULL ||
        !ZrCore_GcDomain_ObjectBelongsToState(
                graph->sourceState, ZR_CAST_RAW_OBJECT_AS_SUPER(object))) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_SOURCE_GC_EDGE,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    if (object->prototype != ZR_NULL ||
        (object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT &&
         object->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY)) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY &&
        !ZrCore_Object_SuperArrayMaterializeGeneric(
                graph->sourceState, (SZrObject *)object)) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    if (graph->nodeCount >= graph->quota.maxObjects) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_OBJECT_QUOTA,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    if (!domain_transfer_graph_grow_nodes(graph) ||
        !domain_transfer_graph_charge_bytes(
                graph, sizeof(SZrDomainTransportNode), diagnostic, depth)) {
        return ZR_FALSE;
    }
    nodeIndex = graph->nodeCount++;
    graph->sourceObjects[nodeIndex] = object;
    graph->nodes[nodeIndex].internalType = object->internalType;
    graph->nodes[nodeIndex].fieldCount = (TZrUInt32)object->nodeMap.elementCount;
    if (graph->nodes[nodeIndex].fieldCount > 0u) {
        TZrUInt64 fieldBytes = (TZrUInt64)graph->nodes[nodeIndex].fieldCount *
                               sizeof(SZrDomainTransportField);
        if (!domain_transfer_graph_charge_bytes(
                    graph, fieldBytes, diagnostic, depth)) {
            return ZR_FALSE;
        }
        fields = (SZrDomainTransportField *)calloc(
                graph->nodes[nodeIndex].fieldCount, sizeof(*fields));
        if (fields == ZR_NULL) {
            domain_transfer_diagnostic_set(
                    diagnostic,
                    ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
                    graph->nodeCount,
                    graph->byteCount,
                    depth);
            return ZR_FALSE;
        }
        graph->nodes[nodeIndex].fields = fields;
    }
    for (TZrSize bucket = 0u; bucket < object->nodeMap.capacity; bucket++) {
        for (SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucket];
             pair != ZR_NULL;
             pair = pair->next) {
            if (fieldIndex >= graph->nodes[nodeIndex].fieldCount ||
                !domain_transfer_graph_encode_value(
                        graph,
                        &pair->key,
                        depth + 1u,
                        &graph->nodes[nodeIndex].fields[fieldIndex].key,
                        diagnostic) ||
                !domain_transfer_graph_encode_value(
                        graph,
                        &pair->value,
                        depth + 1u,
                        &graph->nodes[nodeIndex].fields[fieldIndex].value,
                        diagnostic)) {
                return ZR_FALSE;
            }
            fieldIndex++;
        }
    }
    *outIndex = nodeIndex;
    return fieldIndex == graph->nodes[nodeIndex].fieldCount;
}

static TZrBool domain_transfer_graph_encode_text(
        SZrDomainTransferGraph *graph,
        const SZrString *string,
        TZrUInt32 depth,
        SZrDomainTransportValue *outValue,
        SZrDomainTransferDiagnostic *diagnostic) {
    const TZrChar *text;
    TZrSize length;

    text = string != ZR_NULL ? ZrCore_String_GetNativeString(string) : ZR_NULL;
    if (text == ZR_NULL || graph->sourceState == ZR_NULL ||
        !ZrCore_GcDomain_ObjectBelongsToState(
                graph->sourceState,
                ZR_CAST_RAW_OBJECT_AS_SUPER((SZrString *)string))) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_SOURCE_GC_EDGE,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    length = strlen(text);
    if (length > UINT32_MAX ||
        !domain_transfer_graph_charge_bytes(
                graph, (TZrUInt64)length + 1u, diagnostic, depth)) {
        return ZR_FALSE;
    }
    outValue->text = (TZrChar *)malloc(length + 1u);
    if (outValue->text == ZR_NULL) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    memcpy(outValue->text, text, length + 1u);
    outValue->kind = ZR_DOMAIN_TRANSPORT_VALUE_TEXT;
    outValue->valueType = ZR_VALUE_TYPE_STRING;
    outValue->textLength = (TZrUInt32)length;
    return ZR_TRUE;
}

static TZrBool domain_transfer_graph_encode_value(
        SZrDomainTransferGraph *graph,
        const SZrTypeValue *source,
        TZrUInt32 depth,
        SZrDomainTransportValue *outValue,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrRawObject *rawObject;

    if (source == ZR_NULL || outValue == ZR_NULL ||
        source->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        source->ownershipControl != ZR_NULL || source->ownershipWeakRef != ZR_NULL) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
                graph->nodeCount,
                graph->byteCount,
                depth);
        return ZR_FALSE;
    }
    memset(outValue, 0, sizeof(*outValue));
    outValue->valueType = (TZrUInt32)source->type;
    if (ZR_VALUE_IS_TYPE_NULL(source->type)) {
        outValue->kind = ZR_DOMAIN_TRANSPORT_VALUE_NULL;
        return ZR_TRUE;
    }
    if (source->type == ZR_VALUE_TYPE_BOOL) {
        outValue->kind = ZR_DOMAIN_TRANSPORT_VALUE_BOOL;
        outValue->unsignedValue = source->value.nativeObject.nativeBool;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(source->type)) {
        outValue->kind = ZR_DOMAIN_TRANSPORT_VALUE_SIGNED;
        outValue->signedValue = source->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(source->type)) {
        outValue->kind = ZR_DOMAIN_TRANSPORT_VALUE_UNSIGNED;
        outValue->unsignedValue = source->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(source->type)) {
        outValue->kind = ZR_DOMAIN_TRANSPORT_VALUE_FLOAT;
        outValue->floatValue = source->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }
    rawObject = ZrCore_Value_GetRawObject(source);
    if (source->type == ZR_VALUE_TYPE_STRING && rawObject != ZR_NULL) {
        return domain_transfer_graph_encode_text(
                graph,
                ZR_CAST(SZrString *, rawObject),
                depth,
                outValue,
                diagnostic);
    }
    if ((source->type == ZR_VALUE_TYPE_OBJECT ||
         source->type == ZR_VALUE_TYPE_ARRAY) &&
        rawObject != ZR_NULL) {
        outValue->kind = ZR_DOMAIN_TRANSPORT_VALUE_OBJECT_REF;
        return domain_transfer_graph_add_object(
                graph,
                ZR_CAST(SZrObject *, rawObject),
                depth,
                &outValue->objectIndex,
                diagnostic);
    }
    domain_transfer_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
            graph->nodeCount,
            graph->byteCount,
            depth);
    return ZR_FALSE;
}

SZrDomainTransferGraph *ZrCore_DomainTransferGraph_Prepare(
        SZrState *sourceState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        SZrDomainTransferDiagnostic *diagnostic,
        TZrUInt32 *outObjectCount,
        TZrUInt64 *outByteCount) {
    SZrDomainTransferGraph *graph;

    if (outObjectCount != ZR_NULL) {
        *outObjectCount = 0u;
    }
    if (outByteCount != ZR_NULL) {
        *outByteCount = 0u;
    }
    if (sourceState == ZR_NULL || source == ZR_NULL || quota == ZR_NULL ||
        quota->maxObjects == 0u ||
        quota->maxBytes == 0u || quota->maxDepth == 0u) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
                0u,
                0u,
                0u);
        return ZR_NULL;
    }
    graph = (SZrDomainTransferGraph *)calloc(1u, sizeof(*graph));
    if (graph == ZR_NULL) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
                0u,
                0u,
                0u);
        return ZR_NULL;
    }
    graph->quota = *quota;
    graph->sourceState = sourceState;
    if (!domain_transfer_graph_charge_bytes(
                graph, sizeof(*graph), diagnostic, 0u) ||
        !domain_transfer_graph_encode_value(
                graph, source, 0u, &graph->root, diagnostic)) {
        ZrCore_DomainTransferGraph_Free(graph);
        return ZR_NULL;
    }
    free(graph->sourceObjects);
    graph->sourceObjects = ZR_NULL;
    graph->sourceState = ZR_NULL;
    graph->nodeCapacity = graph->nodeCount;
    if (outObjectCount != ZR_NULL) {
        *outObjectCount = graph->nodeCount;
    }
    if (outByteCount != ZR_NULL) {
        *outByteCount = graph->byteCount;
    }
    domain_transfer_diagnostic_set(
            diagnostic,
            ZR_DOMAIN_TRANSFER_STATUS_OK,
            graph->nodeCount,
            graph->byteCount,
            0u);
    return graph;
}

static TZrBool domain_transfer_graph_decode_value(
        SZrState *targetState,
        const SZrDomainTransportValue *source,
        const SZrGcRootHandle *objectRoots,
        TZrUInt32 objectCount,
        SZrTypeValue *target) {
    if (source == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (source->kind) {
        case ZR_DOMAIN_TRANSPORT_VALUE_NULL:
            ZrCore_Value_ResetAsNull(target);
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_BOOL:
            ZrCore_Value_InitAsBool(
                    targetState, target, source->unsignedValue != 0u);
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_SIGNED:
            ZrCore_Value_InitAsInt(targetState, target, source->signedValue);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_UNSIGNED:
            ZrCore_Value_InitAsUInt(targetState, target, source->unsignedValue);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_FLOAT:
            ZrCore_Value_InitAsFloat(targetState, target, source->floatValue);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_TEXT: {
            SZrString *string = ZrCore_String_Create(
                    targetState, source->text, source->textLength);
            if (string == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsRawObject(
                    targetState,
                    target,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(string));
            target->type = ZR_VALUE_TYPE_STRING;
            return ZR_TRUE;
        }
        case ZR_DOMAIN_TRANSPORT_VALUE_OBJECT_REF: {
            SZrRawObject *object = ZR_NULL;
            if (source->objectIndex >= objectCount ||
                !ZrCore_GcRootHandle_Resolve(
                        targetState,
                        &objectRoots[source->objectIndex],
                        &object)) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsRawObject(
                    targetState, target, object);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool domain_transfer_graph_root_value(
        SZrState *state,
        const SZrTypeValue *value,
        SZrGcRootHandle *root,
        TZrBool *outRooted) {
    *outRooted = ZR_FALSE;
    if (!ZrCore_Value_IsGarbageCollectable(value) ||
        ZrCore_Value_GetRawObject(value) == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!ZrCore_GcRootHandle_Create(
                state, ZrCore_Value_GetRawObject(value), root)) {
        return ZR_FALSE;
    }
    *outRooted = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool domain_transfer_graph_refresh_rooted_value(
        SZrState *state,
        SZrTypeValue *value,
        const SZrGcRootHandle *root,
        TZrBool rooted) {
    SZrRawObject *object = ZR_NULL;
    if (!rooted) {
        return ZR_TRUE;
    }
    if (!ZrCore_GcRootHandle_Resolve(state, root, &object)) {
        return ZR_FALSE;
    }
    value->value.object = object;
    return ZR_TRUE;
}

TZrBool ZrCore_DomainTransferGraph_Commit(
        SZrState *targetState,
        const SZrDomainTransferGraph *graph,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic) {
    SZrGcRootHandle *objectRoots;
    TZrUInt32 rootedObjectCount = 0u;
    TZrBool result = ZR_FALSE;

    if (targetState == ZR_NULL || graph == ZR_NULL || target == ZR_NULL) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
                0u,
                0u,
                0u);
        return ZR_FALSE;
    }
    objectRoots = graph->nodeCount > 0u
                          ? (SZrGcRootHandle *)calloc(
                                    graph->nodeCount, sizeof(*objectRoots))
                          : ZR_NULL;
    if (graph->nodeCount > 0u && objectRoots == ZR_NULL) {
        domain_transfer_diagnostic_set(
                diagnostic,
                ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
                graph->nodeCount,
                graph->byteCount,
                0u);
        return ZR_FALSE;
    }
    for (TZrUInt32 nodeIndex = 0u; nodeIndex < graph->nodeCount; nodeIndex++) {
        SZrObject *object = ZrCore_Object_NewCustomized(
                targetState,
                sizeof(SZrObject),
                graph->nodes[nodeIndex].internalType);
        if (object == ZR_NULL) {
            goto cleanup;
        }
        ZrCore_Object_Init(targetState, object);
        if (!ZrCore_GcRootHandle_Create(
                    targetState,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                    &objectRoots[nodeIndex])) {
            goto cleanup;
        }
        rootedObjectCount++;
    }
    for (TZrUInt32 nodeIndex = 0u; nodeIndex < graph->nodeCount; nodeIndex++) {
        const SZrDomainTransportNode *node = &graph->nodes[nodeIndex];
        for (TZrUInt32 fieldIndex = 0u;
             fieldIndex < node->fieldCount;
             fieldIndex++) {
            SZrGcRootHandle keyRoot;
            SZrGcRootHandle valueRoot;
            SZrRawObject *nodeObject = ZR_NULL;
            SZrTypeValue key;
            SZrTypeValue value;
            TZrBool keyRooted = ZR_FALSE;
            TZrBool valueRooted = ZR_FALSE;
            TZrBool fieldDecoded;

            memset(&keyRoot, 0, sizeof(keyRoot));
            memset(&valueRoot, 0, sizeof(valueRoot));
            ZrCore_Value_ResetAsNull(&key);
            ZrCore_Value_ResetAsNull(&value);
            if (!domain_transfer_graph_decode_value(
                        targetState,
                        &node->fields[fieldIndex].key,
                        objectRoots,
                        graph->nodeCount,
                        &key) ||
                !domain_transfer_graph_root_value(
                        targetState, &key, &keyRoot, &keyRooted)) {
                goto cleanup;
            }
            fieldDecoded = domain_transfer_graph_decode_value(
                        targetState,
                        &node->fields[fieldIndex].value,
                        objectRoots,
                        graph->nodeCount,
                        &value) &&
                    domain_transfer_graph_root_value(
                        targetState, &value, &valueRoot, &valueRooted) &&
                    domain_transfer_graph_refresh_rooted_value(
                        targetState, &key, &keyRoot, keyRooted) &&
                    domain_transfer_graph_refresh_rooted_value(
                        targetState, &value, &valueRoot, valueRooted) &&
                    ZrCore_GcRootHandle_Resolve(
                        targetState, &objectRoots[nodeIndex], &nodeObject);
            if (!fieldDecoded) {
                if (valueRooted) {
                    ZrCore_GcRootHandle_Release(targetState, &valueRoot);
                }
                if (keyRooted) {
                    ZrCore_GcRootHandle_Release(targetState, &keyRoot);
                }
                goto cleanup;
            }
            ZrCore_Object_SetValue(
                    targetState, ZR_CAST(SZrObject *, nodeObject), &key, &value);
            if (valueRooted) {
                ZrCore_GcRootHandle_Release(targetState, &valueRoot);
            }
            if (keyRooted) {
                ZrCore_GcRootHandle_Release(targetState, &keyRoot);
            }
            if (targetState->threadStatus != ZR_THREAD_STATUS_FINE) {
                goto cleanup;
            }
        }
    }
    result = domain_transfer_graph_decode_value(
            targetState,
            &graph->root,
            objectRoots,
            graph->nodeCount,
            target);

cleanup:
    for (TZrUInt32 rootIndex = 0u;
         rootIndex < rootedObjectCount;
         rootIndex++) {
        ZrCore_GcRootHandle_Release(targetState, &objectRoots[rootIndex]);
    }
    free(objectRoots);
    domain_transfer_diagnostic_set(
            diagnostic,
            result ? ZR_DOMAIN_TRANSFER_STATUS_OK
                   : ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
            graph->nodeCount,
            graph->byteCount,
            0u);
    return result;
}
