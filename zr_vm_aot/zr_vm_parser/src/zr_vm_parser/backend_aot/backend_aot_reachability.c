#include "backend_aot_reachability.h"

static void backend_aot_reachability_init_marks(SZrAotReachabilityMark *marks, TZrUInt32 markCount) {
    for (TZrUInt32 index = 0u; index < markCount; index++) {
        marks[index].state = ZR_AOT_REACHABILITY_STATE_UNMARKED;
        marks[index].reason = ZR_AOT_REACHABILITY_REASON_NONE;
        marks[index].predecessor = ZR_AOT_REACHABILITY_NO_NODE;
    }
}

static TZrBool backend_aot_reachability_validate_inputs(TZrUInt32 markCount,
                                                        const TZrUInt32 *roots,
                                                        TZrUInt32 rootCount,
                                                        const SZrAotReachabilityEdge *edges,
                                                        TZrUInt32 edgeCount,
                                                        TZrUInt32 queueCapacity) {
    if (queueCapacity < markCount) {
        return ZR_FALSE;
    }
    if (rootCount > 0u && roots == ZR_NULL) {
        return ZR_FALSE;
    }
    if (edgeCount > 0u && edges == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 rootIndex = 0u; rootIndex < rootCount; rootIndex++) {
        if (roots[rootIndex] >= markCount) {
            return ZR_FALSE;
        }
    }
    for (TZrUInt32 edgeIndex = 0u; edgeIndex < edgeCount; edgeIndex++) {
        if (edges[edgeIndex].source >= markCount || edges[edgeIndex].target >= markCount) {
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
        (rootCount > 0u && rootReasons == ZR_NULL) ||
        !backend_aot_reachability_validate_inputs(markCount, roots, rootCount, edges, edgeCount, queueCapacity)) {
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
