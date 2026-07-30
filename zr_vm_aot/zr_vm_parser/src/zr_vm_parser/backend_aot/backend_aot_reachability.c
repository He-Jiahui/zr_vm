#include "backend_aot_reachability.h"

static TZrBool backend_aot_reachability_reason_is_root(EZrAotReachabilityReason reason) {
    switch (reason) {
        case ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY:
        case ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT:
        case ZR_AOT_REACHABILITY_REASON_MANIFEST:
        case ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION:
        case ZR_AOT_REACHABILITY_REASON_PROPERTY_ACCESSOR:
        case ZR_AOT_REACHABILITY_REASON_RESOURCE_DROP:
        case ZR_AOT_REACHABILITY_REASON_GENERIC_METHODSPEC:
        case ZR_AOT_REACHABILITY_REASON_REFLECTION_CONSTRUCTOR:
        case ZR_AOT_REACHABILITY_REASON_PACKAGE_EXPORT:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_reachability_reason_is_edge(EZrAotReachabilityReason reason) {
    switch (reason) {
        case ZR_AOT_REACHABILITY_REASON_DIRECT_CALL:
        case ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS:
        case ZR_AOT_REACHABILITY_REASON_VIRTUAL_CALL:
        case ZR_AOT_REACHABILITY_REASON_REFLECTION:
        case ZR_AOT_REACHABILITY_REASON_GENERIC_INSTANCE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static const TZrChar *backend_aot_reachability_reason_name(EZrAotReachabilityReason reason) {
    switch (reason) {
        case ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY:
            return "root.entry";
        case ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT:
            return "root.export";
        case ZR_AOT_REACHABILITY_REASON_MANIFEST:
            return "root.manifest";
        case ZR_AOT_REACHABILITY_REASON_DIRECT_CALL:
            return "edge.direct_call";
        case ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS:
            return "edge.field_access";
        case ZR_AOT_REACHABILITY_REASON_VIRTUAL_CALL:
            return "edge.virtual_call";
        case ZR_AOT_REACHABILITY_REASON_REFLECTION:
            return "edge.reflection";
        case ZR_AOT_REACHABILITY_REASON_GENERIC_INSTANCE:
            return "edge.generic_instance";
        case ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION:
            return "root.reflection_annotation";
        case ZR_AOT_REACHABILITY_REASON_PROPERTY_ACCESSOR:
            return "root.property_accessor";
        case ZR_AOT_REACHABILITY_REASON_RESOURCE_DROP:
            return "root.resource_drop";
        case ZR_AOT_REACHABILITY_REASON_GENERIC_METHODSPEC:
            return "root.generic_methodspec";
        case ZR_AOT_REACHABILITY_REASON_REFLECTION_CONSTRUCTOR:
            return "root.reflection_constructor";
        case ZR_AOT_REACHABILITY_REASON_PACKAGE_EXPORT:
            return "root.package_export";
        default:
            return ZR_NULL;
    }
}

static TZrBool backend_aot_reachability_validate_function_manifest(
        const SZrAotReachabilityMark *marks,
        TZrUInt32 markCount,
        TZrUInt32 *outRetainedCount) {
    TZrUInt32 retainedCount = 0u;

    if (outRetainedCount != ZR_NULL) {
        *outRetainedCount = 0u;
    }
    if (marks == ZR_NULL || markCount == 0u) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < markCount; index++) {
        const SZrAotReachabilityMark *mark = &marks[index];
        TZrUInt32 cursor = index;
        TZrBool reachedRoot = ZR_FALSE;

        if (mark->state == ZR_AOT_REACHABILITY_STATE_UNMARKED) {
            if (mark->reason != ZR_AOT_REACHABILITY_REASON_NONE ||
                mark->predecessor != ZR_AOT_REACHABILITY_NO_NODE) {
                return ZR_FALSE;
            }
            continue;
        }
        if (mark->state != ZR_AOT_REACHABILITY_STATE_PROCESSED) {
            return ZR_FALSE;
        }

        retainedCount++;
        for (TZrUInt32 depth = 0u; depth < markCount; depth++) {
            const SZrAotReachabilityMark *chainMark = &marks[cursor];

            if (chainMark->state != ZR_AOT_REACHABILITY_STATE_PROCESSED) {
                return ZR_FALSE;
            }
            if (chainMark->predecessor == ZR_AOT_REACHABILITY_NO_NODE) {
                if (!backend_aot_reachability_reason_is_root(chainMark->reason)) {
                    return ZR_FALSE;
                }
                reachedRoot = ZR_TRUE;
                break;
            }
            if (!backend_aot_reachability_reason_is_edge(chainMark->reason) ||
                chainMark->predecessor >= markCount) {
                return ZR_FALSE;
            }
            cursor = chainMark->predecessor;
        }
        if (!reachedRoot) {
            return ZR_FALSE;
        }
    }

    if (outRetainedCount != ZR_NULL) {
        *outRetainedCount = retainedCount;
    }
    return ZR_TRUE;
}

static void backend_aot_reachability_init_marks(SZrAotReachabilityMark *marks, TZrUInt32 markCount) {
    for (TZrUInt32 index = 0u; index < markCount; index++) {
        marks[index].state = ZR_AOT_REACHABILITY_STATE_UNMARKED;
        marks[index].reason = ZR_AOT_REACHABILITY_REASON_NONE;
        marks[index].predecessor = ZR_AOT_REACHABILITY_NO_NODE;
    }
}

static TZrBool backend_aot_reachability_validate_inputs(TZrUInt32 markCount,
                                                        const TZrUInt32 *roots,
                                                        const EZrAotReachabilityReason *rootReasons,
                                                        TZrUInt32 rootCount,
                                                        const SZrAotReachabilityEdge *edges,
                                                        TZrUInt32 edgeCount,
                                                        TZrUInt32 queueCapacity) {
    if (queueCapacity < markCount) {
        return ZR_FALSE;
    }
    if (rootCount > 0u && (roots == ZR_NULL || rootReasons == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (edgeCount > 0u && edges == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 rootIndex = 0u; rootIndex < rootCount; rootIndex++) {
        if (roots[rootIndex] >= markCount ||
            !backend_aot_reachability_reason_is_root(rootReasons[rootIndex])) {
            return ZR_FALSE;
        }
    }
    for (TZrUInt32 edgeIndex = 0u; edgeIndex < edgeCount; edgeIndex++) {
        if (edges[edgeIndex].source >= markCount ||
            edges[edgeIndex].target >= markCount ||
            !backend_aot_reachability_reason_is_edge(edges[edgeIndex].reason)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_reachability_enqueue(TZrUInt32 nodeIndex,
                                                EZrAotReachabilityReason reason,
                                                TZrUInt32 predecessor,
                                                SZrAotReachabilityMark *marks,
                                                TZrUInt32 *queue,
                                                TZrUInt32 *tail,
                                                TZrUInt32 *markedCount) {
    if (marks[nodeIndex].state != ZR_AOT_REACHABILITY_STATE_UNMARKED) {
        return ZR_TRUE;
    }

    marks[nodeIndex].state = ZR_AOT_REACHABILITY_STATE_MARKED_PENDING;
    marks[nodeIndex].reason = reason;
    marks[nodeIndex].predecessor = predecessor;
    queue[*tail] = nodeIndex;
    (*tail)++;
    (*markedCount)++;
    return ZR_TRUE;
}

TZrBool backend_aot_reachability_compute(SZrAotReachabilityMark *marks,
                                          TZrUInt32 markCount,
                                          const TZrUInt32 *roots,
                                          const EZrAotReachabilityReason *rootReasons,
                                          TZrUInt32 rootCount,
                                          const SZrAotReachabilityEdge *edges,
                                          TZrUInt32 edgeCount,
                                          TZrUInt32 *queue,
                                          TZrUInt32 queueCapacity,
                                          TZrUInt32 *outMarkedCount) {
    TZrUInt32 head = 0u;
    TZrUInt32 tail = 0u;
    TZrUInt32 markedCount = 0u;

    if (outMarkedCount != ZR_NULL) {
        *outMarkedCount = 0u;
    }
    if (marks == ZR_NULL || queue == ZR_NULL ||
        !backend_aot_reachability_validate_inputs(markCount,
                                                  roots,
                                                  rootReasons,
                                                  rootCount,
                                                  edges,
                                                  edgeCount,
                                                  queueCapacity)) {
        return ZR_FALSE;
    }

    backend_aot_reachability_init_marks(marks, markCount);
    for (TZrUInt32 rootIndex = 0u; rootIndex < rootCount; rootIndex++) {
        backend_aot_reachability_enqueue(roots[rootIndex],
                                         rootReasons[rootIndex],
                                         ZR_AOT_REACHABILITY_NO_NODE,
                                         marks,
                                         queue,
                                         &tail,
                                         &markedCount);
    }

    while (head < tail) {
        TZrUInt32 source = queue[head];
        head++;
        marks[source].state = ZR_AOT_REACHABILITY_STATE_PROCESSED;

        for (TZrUInt32 edgeIndex = 0u; edgeIndex < edgeCount; edgeIndex++) {
            const SZrAotReachabilityEdge *edge = &edges[edgeIndex];
            if (edge->source == source) {
                backend_aot_reachability_enqueue(edge->target,
                                                 edge->reason,
                                                 source,
                                                 marks,
                                                 queue,
                                                 &tail,
                                                 &markedCount);
            }
        }
    }

    if (outMarkedCount != ZR_NULL) {
        *outMarkedCount = markedCount;
    }
    return ZR_TRUE;
}

TZrBool backend_aot_reachability_write_function_manifest(FILE *file,
                                                          const SZrAotReachabilityMark *marks,
                                                          TZrUInt32 markCount) {
    TZrUInt32 retainedCount = 0u;

    if (file == ZR_NULL ||
        !backend_aot_reachability_validate_function_manifest(marks, markCount, &retainedCount)) {
        return ZR_FALSE;
    }
    if (fprintf(file, "/* reachability.functionManifest.version = 1 */\n") < 0 ||
        fprintf(file,
                "/* reachability.functionManifest.count = %u */\n",
                (unsigned)retainedCount) < 0) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < markCount; index++) {
        const SZrAotReachabilityMark *mark = &marks[index];
        const TZrChar *reasonName;

        if (mark->state != ZR_AOT_REACHABILITY_STATE_PROCESSED) {
            continue;
        }
        reasonName = backend_aot_reachability_reason_name(mark->reason);
        if (reasonName == ZR_NULL) {
            return ZR_FALSE;
        }
        if (mark->predecessor == ZR_AOT_REACHABILITY_NO_NODE) {
            if (fprintf(file,
                        "/* reachability.functionManifest.node[%u] = reason=%s predecessor=none */\n",
                        (unsigned)index,
                        reasonName) < 0) {
                return ZR_FALSE;
            }
        } else if (fprintf(file,
                           "/* reachability.functionManifest.node[%u] = reason=%s predecessor=%u */\n",
                           (unsigned)index,
                           reasonName,
                           (unsigned)mark->predecessor) < 0) {
            return ZR_FALSE;
        }
    }

    return (TZrBool)(ferror(file) == 0);
}
