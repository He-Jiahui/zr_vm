#include "backend_aot_reachability_function_graph.h"

#include "backend_aot_internal.h"

static TZrBool backend_aot_static_reachability_has_entry(const SZrAotFunctionTable *table, TZrUInt32 flatIndex) {
    for (TZrUInt32 index = 0u; index < table->count; index++) {
        if (table->entries[index].flatIndex == flatIndex) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const SZrFunction *backend_aot_static_reachability_find_entry_function(const SZrAotFunctionTable *table,
                                                                              TZrUInt32 flatIndex) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        if (table->entries[index].flatIndex == flatIndex) {
            return table->entries[index].function;
        }
    }

    return ZR_NULL;
}

static TZrBool backend_aot_static_reachability_append_root(TZrUInt32 *roots,
                                                           EZrAotReachabilityReason *rootReasons,
                                                           TZrUInt32 rootCapacity,
                                                           TZrUInt32 *rootCount,
                                                           TZrUInt32 root,
                                                           EZrAotReachabilityReason reason,
                                                           TZrUInt32 markCount) {
    if (roots == ZR_NULL || rootReasons == ZR_NULL || rootCount == ZR_NULL || root >= markCount) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < *rootCount; index++) {
        if (roots[index] == root) {
            return ZR_TRUE;
        }
    }

    if (*rootCount >= rootCapacity) {
        return ZR_FALSE;
    }

    roots[*rootCount] = root;
    rootReasons[*rootCount] = reason;
    (*rootCount)++;
    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_collect_export_roots(const SZrAotFunctionTable *table,
                                                                    TZrUInt32 *roots,
                                                                    EZrAotReachabilityReason *rootReasons,
                                                                    TZrUInt32 rootCapacity,
                                                                    TZrUInt32 *rootCount,
                                                                    TZrUInt32 markCount) {
    const SZrFunction *entryFunction =
            backend_aot_static_reachability_find_entry_function(table, ZR_AOT_FUNCTION_TREE_ROOT_INDEX);

    if (entryFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (entryFunction->topLevelCallableBindingLength > 0u &&
        entryFunction->topLevelCallableBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 bindingIndex = 0u;
         bindingIndex < entryFunction->topLevelCallableBindingLength;
         bindingIndex++) {
        const SZrFunctionTopLevelCallableBinding *binding =
                &entryFunction->topLevelCallableBindings[bindingIndex];
        const SZrFunction *childFunction;
        TZrUInt32 targetIndex;

        if (binding->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION ||
            binding->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE) {
            continue;
        }
        if (entryFunction->childFunctionList == ZR_NULL ||
            binding->callableChildIndex >= entryFunction->childFunctionLength) {
            return ZR_FALSE;
        }

        childFunction = &entryFunction->childFunctionList[binding->callableChildIndex];
        targetIndex = backend_aot_find_function_table_index(table, childFunction);
        if (targetIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            return ZR_FALSE;
        }
        if (!backend_aot_static_reachability_append_root(roots,
                                                         rootReasons,
                                                         rootCapacity,
                                                         rootCount,
                                                         targetIndex,
                                                         ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT,
                                                         markCount)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_collect_manifest_roots(const SZrAotFunctionTable *table,
                                                                      const TZrUInt32 *manifestRoots,
                                                                      TZrUInt32 manifestRootCount,
                                                                      TZrUInt32 *roots,
                                                                      EZrAotReachabilityReason *rootReasons,
                                                                      TZrUInt32 rootCapacity,
                                                                      TZrUInt32 *rootCount,
                                                                      TZrUInt32 markCount) {
    if (manifestRootCount == 0u) {
        return ZR_TRUE;
    }
    if (manifestRoots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 manifestIndex = 0u; manifestIndex < manifestRootCount; manifestIndex++) {
        TZrUInt32 root = manifestRoots[manifestIndex];
        if (root >= markCount || !backend_aot_static_reachability_has_entry(table, root)) {
            return ZR_FALSE;
        }
        if (!backend_aot_static_reachability_append_root(roots,
                                                         rootReasons,
                                                         rootCapacity,
                                                         rootCount,
                                                         root,
                                                         ZR_AOT_REACHABILITY_REASON_MANIFEST,
                                                         markCount)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_append_edge(SZrAotReachabilityEdge *edges,
                                                           TZrUInt32 edgeCapacity,
                                                           TZrUInt32 *edgeCount,
                                                           TZrUInt32 source,
                                                           TZrUInt32 target,
                                                           TZrUInt32 markCount) {
    if (edgeCount == ZR_NULL || source >= markCount || target >= markCount) {
        return ZR_FALSE;
    }
    if (*edgeCount >= edgeCapacity || edges == ZR_NULL) {
        return ZR_FALSE;
    }

    edges[*edgeCount].source = source;
    edges[*edgeCount].target = target;
    edges[*edgeCount].reason = ZR_AOT_REACHABILITY_REASON_DIRECT_CALL;
    (*edgeCount)++;
    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_scan_instruction(SZrState *state,
                                                                const SZrAotFunctionTable *table,
                                                                const SZrFunction *function,
                                                                TZrUInt32 sourceIndex,
                                                                const TZrInstruction *instruction,
                                                                SZrAotReachabilityEdge *edges,
                                                                TZrUInt32 edgeCapacity,
                                                                TZrUInt32 *edgeCount,
                                                                TZrUInt32 markCount) {
    TZrUInt32 targetIndex = ZR_AOT_INVALID_FUNCTION_INDEX;

    switch (instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            if (backend_aot_resolve_callable_constant_function_index(table,
                                                                     state,
                                                                     function,
                                                                     instruction->instruction.operand.operand2[0],
                                                                     &targetIndex)) {
                return backend_aot_static_reachability_append_edge(edges,
                                                                   edgeCapacity,
                                                                   edgeCount,
                                                                   sourceIndex,
                                                                   targetIndex,
                                                                   markCount);
            }
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            if (backend_aot_resolve_callable_constant_function_index(
                        table,
                        state,
                        function,
                        (TZrInt32)instruction->instruction.operand.operand1[0],
                        &targetIndex)) {
                return backend_aot_static_reachability_append_edge(edges,
                                                                   edgeCapacity,
                                                                   edgeCount,
                                                                   sourceIndex,
                                                                   targetIndex,
                                                                   markCount);
            }
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            if (function->childFunctionList != ZR_NULL &&
                instruction->instruction.operand.operand1[0] < function->childFunctionLength) {
                const SZrFunction *childFunction =
                        &function->childFunctionList[instruction->instruction.operand.operand1[0]];
                targetIndex = backend_aot_find_function_table_index(table, childFunction);
                if (targetIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
                    return backend_aot_static_reachability_append_edge(edges,
                                                                       edgeCapacity,
                                                                       edgeCount,
                                                                       sourceIndex,
                                                                       targetIndex,
                                                                       markCount);
                }
            }
            return ZR_TRUE;
        default:
            return ZR_TRUE;
    }
}

static TZrBool backend_aot_static_reachability_collect_edges(SZrState *state,
                                                             const SZrAotFunctionTable *table,
                                                             SZrAotReachabilityEdge *edges,
                                                             TZrUInt32 edgeCapacity,
                                                             TZrUInt32 *outEdgeCount,
                                                             TZrUInt32 markCount) {
    TZrUInt32 edgeCount = 0u;

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &table->entries[entryIndex];
        const SZrFunction *function = entry->function;

        if (function == ZR_NULL || entry->flatIndex >= markCount) {
            return ZR_FALSE;
        }
        if (function->instructionsLength > 0u && function->instructionsList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrUInt32 instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
            if (!backend_aot_static_reachability_scan_instruction(state,
                                                                  table,
                                                                  function,
                                                                  entry->flatIndex,
                                                                  &function->instructionsList[instructionIndex],
                                                                  edges,
                                                                  edgeCapacity,
                                                                  &edgeCount,
                                                                  markCount)) {
                return ZR_FALSE;
            }
        }
    }

    if (outEdgeCount != ZR_NULL) {
        *outEdgeCount = edgeCount;
    }
    return ZR_TRUE;
}

TZrBool backend_aot_compute_static_callable_reachability(SZrState *state,
                                                         const SZrAotFunctionTable *table,
                                                         const TZrUInt32 *manifestRoots,
                                                         TZrUInt32 manifestRootCount,
                                                         TZrUInt32 *roots,
                                                         EZrAotReachabilityReason *rootReasons,
                                                         TZrUInt32 rootCapacity,
                                                         SZrAotReachabilityMark *marks,
                                                         TZrUInt32 markCount,
                                                         SZrAotReachabilityEdge *edges,
                                                         TZrUInt32 edgeCapacity,
                                                         TZrUInt32 *queue,
                                                         TZrUInt32 queueCapacity,
                                                         TZrUInt32 *outMarkedCount,
                                                         TZrUInt32 *outEdgeCount) {
    TZrUInt32 rootCount = 0u;
    TZrUInt32 edgeCount = 0u;
    TZrUInt32 indexSpace;

    if (outMarkedCount != ZR_NULL) {
        *outMarkedCount = 0u;
    }
    if (outEdgeCount != ZR_NULL) {
        *outEdgeCount = 0u;
    }
    if (table == ZR_NULL || table->entries == ZR_NULL || table->count == 0u ||
        table->count > table->capacity || roots == ZR_NULL || rootReasons == ZR_NULL ||
        rootCapacity == 0u || marks == ZR_NULL || queue == ZR_NULL) {
        return ZR_FALSE;
    }

    indexSpace = backend_aot_function_table_index_space(table);
    if (indexSpace == ZR_AOT_COUNT_NONE || markCount < indexSpace ||
        !backend_aot_static_reachability_has_entry(table, ZR_AOT_FUNCTION_TREE_ROOT_INDEX)) {
        return ZR_FALSE;
    }

    if (!backend_aot_static_reachability_append_root(roots,
                                                     rootReasons,
                                                     rootCapacity,
                                                     &rootCount,
                                                     ZR_AOT_FUNCTION_TREE_ROOT_INDEX,
                                                     ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY,
                                                     markCount)) {
        return ZR_FALSE;
    }
    if (!backend_aot_static_reachability_collect_export_roots(table,
                                                              roots,
                                                              rootReasons,
                                                              rootCapacity,
                                                              &rootCount,
                                                              markCount)) {
        return ZR_FALSE;
    }
    if (!backend_aot_static_reachability_collect_manifest_roots(table,
                                                                manifestRoots,
                                                                manifestRootCount,
                                                                roots,
                                                                rootReasons,
                                                                rootCapacity,
                                                                &rootCount,
                                                                markCount)) {
        return ZR_FALSE;
    }

    if (!backend_aot_static_reachability_collect_edges(state,
                                                       table,
                                                       edges,
                                                       edgeCapacity,
                                                       &edgeCount,
                                                       markCount)) {
        return ZR_FALSE;
    }

    if (outEdgeCount != ZR_NULL) {
        *outEdgeCount = edgeCount;
    }
    return backend_aot_reachability_compute(marks,
                                            markCount,
                                            roots,
                                            rootReasons,
                                            rootCount,
                                            edges,
                                            edgeCount,
                                            queue,
                                            queueCapacity,
                                            outMarkedCount);
}
