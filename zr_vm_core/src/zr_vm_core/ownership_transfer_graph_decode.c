#include "ownership_transfer_graph_internal.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#include <stdlib.h>
#include <string.h>

typedef struct SZrDomainGraphDecode {
    const SZrDomainTransferGraph *graph;
    SZrGcRootHandle *objectRoots;
    TZrUInt32 rootedObjectCount;
    SZrGcRootHandle keyRoot;
    SZrGcRootHandle valueRoot;
    TZrBool keyRooted;
    TZrBool valueRooted;
    SZrTypeValue stagedTarget;
    TZrBool result;
    TZrBool callbackReturned;
} SZrDomainGraphDecode;

static void graph_decode_diagnostic(
        SZrDomainTransferDiagnostic *diagnostic, EZrDomainTransferStatus status,
        const SZrDomainTransferGraph *graph) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->objectCount = graph != ZR_NULL ? graph->nodeCount : 0u;
        diagnostic->byteCount = graph != ZR_NULL ? graph->byteCount : 0u;
        diagnostic->depth = 0u;
    }
}

static TZrBool graph_decode_value(
        SZrState *state, const SZrDomainTransportValue *source,
        const SZrDomainGraphDecode *context, SZrTypeValue *target) {
    switch (source->kind) {
        case ZR_DOMAIN_TRANSPORT_VALUE_NULL:
            ZrCore_Value_ResetAsNull(target);
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_BOOL:
            ZrCore_Value_InitAsBool(state, target, source->unsignedValue != 0u);
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_SIGNED:
            ZrCore_Value_InitAsInt(state, target, source->signedValue);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_UNSIGNED:
            ZrCore_Value_InitAsUInt(state, target, source->unsignedValue);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_FLOAT:
            ZrCore_Value_InitAsFloat(state, target, source->floatValue);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        case ZR_DOMAIN_TRANSPORT_VALUE_TEXT: {
            SZrString *string = ZrCore_String_Create(state, source->text, source->textLength);
            if (string == ZR_NULL) return ZR_FALSE;
            ZrCore_Value_InitAsRawObject(state, target, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
            target->type = ZR_VALUE_TYPE_STRING;
            return ZR_TRUE;
        }
        case ZR_DOMAIN_TRANSPORT_VALUE_OBJECT_REF: {
            SZrRawObject *object = ZR_NULL;
            if (source->objectIndex >= context->graph->nodeCount ||
                !ZrCore_GcRootHandle_Resolve(state,
                        &context->objectRoots[source->objectIndex], &object)) return ZR_FALSE;
            ZrCore_Value_InitAsRawObject(state, target, object);
            target->type = (EZrValueType)source->valueType;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool graph_decode_root_value(
        SZrState *state, const SZrTypeValue *value,
        SZrGcRootHandle *root, TZrBool *rooted) {
    *rooted = ZR_FALSE;
    if (!ZrCore_Value_IsGarbageCollectable(value) ||
        ZrCore_Value_GetRawObject(value) == ZR_NULL) return ZR_TRUE;
    if (!ZrCore_GcRootHandle_Create(state, ZrCore_Value_GetRawObject(value), root))
        return ZR_FALSE;
    *rooted = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool graph_decode_refresh(
        SZrState *state, SZrTypeValue *value,
        const SZrGcRootHandle *root, TZrBool rooted) {
    SZrRawObject *object = ZR_NULL;
    if (!rooted) return ZR_TRUE;
    if (!ZrCore_GcRootHandle_Resolve(state, root, &object)) return ZR_FALSE;
    value->value.object = object;
    return ZR_TRUE;
}

static void graph_decode_release_field_roots(SZrState *state, SZrDomainGraphDecode *context) {
    if (context->valueRooted) {
        (void)ZrCore_GcRootHandle_Release(state, &context->valueRoot);
        context->valueRooted = ZR_FALSE;
    }
    if (context->keyRooted) {
        (void)ZrCore_GcRootHandle_Release(state, &context->keyRoot);
        context->keyRooted = ZR_FALSE;
    }
}

static void graph_decode_callback(SZrState *state, TZrPtr arguments) {
    SZrDomainGraphDecode *context = (SZrDomainGraphDecode *)arguments;
    const SZrDomainTransferGraph *graph = context->graph;
    for (TZrUInt32 nodeIndex = 0u; nodeIndex < graph->nodeCount; ++nodeIndex) {
        SZrObject *object = ZrCore_Object_NewCustomized(
                state, sizeof(SZrObject), graph->nodes[nodeIndex].internalType);
        if (object == ZR_NULL) goto finish;
        ZrCore_Object_Init(state, object);
        if (!ZrCore_GcRootHandle_Create(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object),
                                      &context->objectRoots[nodeIndex])) goto finish;
        ++context->rootedObjectCount;
    }
    for (TZrUInt32 nodeIndex = 0u; nodeIndex < graph->nodeCount; ++nodeIndex) {
        const SZrDomainTransportNode *node = &graph->nodes[nodeIndex];
        for (TZrUInt32 fieldIndex = 0u; fieldIndex < node->fieldCount; ++fieldIndex) {
            SZrRawObject *nodeObject = ZR_NULL;
            SZrTypeValue key;
            SZrTypeValue value;
            memset(&context->keyRoot, 0, sizeof(context->keyRoot));
            memset(&context->valueRoot, 0, sizeof(context->valueRoot));
            ZrCore_Value_ResetAsNull(&key);
            ZrCore_Value_ResetAsNull(&value);
            if (!graph_decode_value(state, &node->fields[fieldIndex].key, context, &key) ||
                !graph_decode_root_value(state, &key, &context->keyRoot, &context->keyRooted) ||
                !graph_decode_value(state, &node->fields[fieldIndex].value, context, &value) ||
                !graph_decode_root_value(state, &value, &context->valueRoot, &context->valueRooted) ||
                !graph_decode_refresh(state, &key, &context->keyRoot, context->keyRooted) ||
                !graph_decode_refresh(state, &value, &context->valueRoot, context->valueRooted) ||
                !ZrCore_GcRootHandle_Resolve(state, &context->objectRoots[nodeIndex], &nodeObject))
                goto finish;
            ZrCore_Object_SetValue(state, ZR_CAST(SZrObject *, nodeObject), &key, &value);
            graph_decode_release_field_roots(state, context);
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) goto finish;
        }
    }
    context->result = graph_decode_value(state, &graph->root, context, &context->stagedTarget);
finish:
    context->callbackReturned = ZR_TRUE;
}

TZrBool ZrCore_DomainTransferGraph_Commit(
        SZrState *targetState, const SZrDomainTransferGraph *graph,
        SZrTypeValue *target, SZrDomainTransferDiagnostic *diagnostic,
        SZrDomainTransferGraphCommitFailure *failure) {
    SZrDomainGraphDecode context = {0};
    if (failure != ZR_NULL) {
        failure->status = ZR_THREAD_STATUS_FINE;
        failure->thrown = ZR_FALSE;
    }
    if (targetState == ZR_NULL || graph == ZR_NULL || target == ZR_NULL || failure == ZR_NULL) {
        graph_decode_diagnostic(diagnostic, ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT, ZR_NULL);
        return ZR_FALSE;
    }
    context.graph = graph;
    ZrCore_Value_ResetAsNull(&context.stagedTarget);
    if (graph->nodeCount > 0u) {
        context.objectRoots = (SZrGcRootHandle *)calloc(graph->nodeCount, sizeof(*context.objectRoots));
        if (context.objectRoots == ZR_NULL) {
            graph_decode_diagnostic(diagnostic, ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED, graph);
            return ZR_FALSE;
        }
    }
    /* The context lives in the caller of TryRun, so callback writes survive
     * its longjmp and every acquired handle remains available for cleanup. */
    failure->status = ZrCore_Exception_TryRun(targetState, graph_decode_callback, &context);
    failure->thrown = (TZrBool)!context.callbackReturned;
    if (failure->thrown) context.result = ZR_FALSE;
    if (context.result) *target = context.stagedTarget;
    graph_decode_release_field_roots(targetState, &context);
    for (TZrUInt32 rootIndex = 0u; rootIndex < context.rootedObjectCount; ++rootIndex)
        (void)ZrCore_GcRootHandle_Release(targetState, &context.objectRoots[rootIndex]);
    free(context.objectRoots);
    graph_decode_diagnostic(diagnostic,
            context.result ? ZR_DOMAIN_TRANSFER_STATUS_OK
                    : failure->status == ZR_THREAD_STATUS_MEMORY_ERROR
                            ? ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED
                            : ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
            graph);
    return context.result;
}
