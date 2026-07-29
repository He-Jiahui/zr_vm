#include "backend_aot_c_type_layout_reachability.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/type_layout.h"

#include <stdint.h>

#define ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_NO_FUNCTION UINT32_MAX

typedef enum EZrAotCTypeLayoutReachabilityReason {
    ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_REASON_ROOT_REFLECTION_ANNOTATION = 1,
    ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_REASON_EDGE_FRAME_LAYOUT = 2
} EZrAotCTypeLayoutReachabilityReason;

typedef struct SZrAotCTypeLayoutReachabilityRow {
    TZrUInt32 typeLayoutId;
    EZrAotCTypeLayoutReachabilityReason reason;
    TZrUInt32 predecessorFunction;
} SZrAotCTypeLayoutReachabilityRow;

static TZrBool backend_aot_c_type_layout_reachability_table_is_valid(const SZrAotFunctionTable *table) {
    if (table == ZR_NULL ||
        table->entries == ZR_NULL ||
        table->count == 0u ||
        table->count > table->capacity) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        const SZrAotFunctionEntry *entry = &table->entries[index];

        if (entry->function == ZR_NULL ||
            entry->flatIndex == ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_NO_FUNCTION) {
            return ZR_FALSE;
        }
        for (TZrUInt32 previousIndex = 0u; previousIndex < index; previousIndex++) {
            if (table->entries[previousIndex].flatIndex == entry->flatIndex) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

static const SZrTypeLayout *backend_aot_c_type_layout_reachability_resolve_from_function(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 typeLayoutId) {
    const SZrTypeLayout *typeLayout;

    if (state == ZR_NULL ||
        function == ZR_NULL ||
        typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE) {
        return ZR_NULL;
    }

    typeLayout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(function, typeLayoutId, state);
    if (typeLayout == ZR_NULL ||
        (typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT &&
         typeLayout->kind != (TZrUInt8)ZR_TYPE_LAYOUT_KIND_UNION)) {
        return ZR_NULL;
    }
    return typeLayout;
}

static TZrBool backend_aot_c_type_layout_reachability_has_resolver(
        SZrState *state,
        const SZrAotFunctionTable *table,
        TZrUInt32 typeLayoutId) {
    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        if (backend_aot_c_type_layout_reachability_resolve_from_function(
                    state, table->entries[entryIndex].function, typeLayoutId) != ZR_NULL) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool backend_aot_c_type_layout_reachability_is_rooted(
        const TZrUInt32 *typeLayoutRoots,
        TZrUInt32 typeLayoutRootCount,
        TZrUInt32 typeLayoutId) {
    for (TZrUInt32 rootIndex = 0u; rootIndex < typeLayoutRootCount; rootIndex++) {
        if (typeLayoutRoots[rootIndex] == typeLayoutId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool backend_aot_c_type_layout_reachability_find_frame_predecessor(
        const SZrAotFunctionTable *table,
        TZrUInt32 typeLayoutId,
        TZrUInt32 *outPredecessorFunction) {
    if (outPredecessorFunction != ZR_NULL) {
        *outPredecessorFunction = ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_NO_FUNCTION;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &table->entries[entryIndex];
        const SZrFunction *function = entry->function;

        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];

            if (slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
                slotLayout->typeLayoutId == typeLayoutId) {
                if (outPredecessorFunction != ZR_NULL) {
                    *outPredecessorFunction = entry->flatIndex;
                }
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_type_layout_reachability_validate_roots(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const TZrUInt32 *typeLayoutRoots,
        TZrUInt32 typeLayoutRootCount,
        TZrUInt32 *ioMaximumTypeLayoutId) {
    if (typeLayoutRootCount > 0u && typeLayoutRoots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 rootIndex = 0u; rootIndex < typeLayoutRootCount; rootIndex++) {
        TZrUInt32 typeLayoutId = typeLayoutRoots[rootIndex];

        if (typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
            typeLayoutId == UINT32_MAX ||
            !backend_aot_c_type_layout_reachability_has_resolver(state, table, typeLayoutId)) {
            return ZR_FALSE;
        }
        for (TZrUInt32 previousRootIndex = 0u; previousRootIndex < rootIndex; previousRootIndex++) {
            if (typeLayoutRoots[previousRootIndex] == typeLayoutId) {
                return ZR_FALSE;
            }
        }
        if (typeLayoutId > *ioMaximumTypeLayoutId) {
            *ioMaximumTypeLayoutId = typeLayoutId;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_reachability_validate_frame_references(
        SZrState *state,
        const SZrAotFunctionTable *table,
        TZrUInt32 *ioMaximumTypeLayoutId) {
    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrFunction *function = table->entries[entryIndex].function;

        if (function->frameSlotLayoutLength > 0u && function->frameSlotLayouts == ZR_NULL) {
            return ZR_FALSE;
        }
        for (TZrUInt32 slotIndex = 0u; slotIndex < function->frameSlotLayoutLength; slotIndex++) {
            const SZrFunctionFrameSlotLayout *slotLayout = &function->frameSlotLayouts[slotIndex];
            TZrUInt32 typeLayoutId;

            if (slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
                continue;
            }
            typeLayoutId = slotLayout->typeLayoutId;
            if (typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE ||
                typeLayoutId == UINT32_MAX ||
                backend_aot_c_type_layout_reachability_resolve_from_function(
                        state, function, typeLayoutId) == ZR_NULL) {
                return ZR_FALSE;
            }
            if (typeLayoutId > *ioMaximumTypeLayoutId) {
                *ioMaximumTypeLayoutId = typeLayoutId;
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_reachability_collect(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const TZrUInt32 *typeLayoutRoots,
        TZrUInt32 typeLayoutRootCount,
        SZrAotCTypeLayoutReachabilityRow *rows,
        TZrUInt32 rowCapacity,
        TZrUInt32 *outRowCount) {
    TZrUInt32 maximumTypeLayoutId = 0u;
    TZrUInt32 rowCount = 0u;

    if (outRowCount != ZR_NULL) {
        *outRowCount = 0u;
    }
    if (state == ZR_NULL ||
        !backend_aot_c_type_layout_reachability_table_is_valid(table) ||
        !backend_aot_c_type_layout_reachability_validate_roots(state,
                                                                table,
                                                                typeLayoutRoots,
                                                                typeLayoutRootCount,
                                                                &maximumTypeLayoutId) ||
        !backend_aot_c_type_layout_reachability_validate_frame_references(
                state, table, &maximumTypeLayoutId)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 typeLayoutId = 0u; typeLayoutId <= maximumTypeLayoutId; typeLayoutId++) {
        TZrUInt32 predecessorFunction = ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_NO_FUNCTION;
        TZrBool rooted = backend_aot_c_type_layout_reachability_is_rooted(
                typeLayoutRoots, typeLayoutRootCount, typeLayoutId);
        TZrBool frameReferenced = backend_aot_c_type_layout_reachability_find_frame_predecessor(
                table, typeLayoutId, &predecessorFunction);

        if (!rooted && !frameReferenced) {
            continue;
        }
        if (rows != ZR_NULL) {
            if (rowCount >= rowCapacity) {
                return ZR_FALSE;
            }
            rows[rowCount].typeLayoutId = typeLayoutId;
            rows[rowCount].reason = rooted
                                            ? ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_REASON_ROOT_REFLECTION_ANNOTATION
                                            : ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_REASON_EDGE_FRAME_LAYOUT;
            rows[rowCount].predecessorFunction = rooted
                                                         ? ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_NO_FUNCTION
                                                         : predecessorFunction;
        }
        rowCount++;
    }

    if (outRowCount != ZR_NULL) {
        *outRowCount = rowCount;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_c_type_layout_reachability_write_rows(
        FILE *file,
        const SZrAotCTypeLayoutReachabilityRow *rows,
        TZrUInt32 rowCount) {
    if (fprintf(file, "/* reachability.typeLayoutManifest.version = 1 */\n") < 0 ||
        fprintf(file,
                "/* reachability.typeLayoutManifest.count = %u */\n",
                (unsigned)rowCount) < 0) {
        return ZR_FALSE;
    }

    for (TZrUInt32 rowIndex = 0u; rowIndex < rowCount; rowIndex++) {
        const SZrAotCTypeLayoutReachabilityRow *row = &rows[rowIndex];

        if (row->reason == ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_REASON_ROOT_REFLECTION_ANNOTATION) {
            if (fprintf(file,
                        "/* reachability.typeLayoutManifest.node[%u] = reason=root.reflection_annotation predecessorFunction=none */\n",
                        (unsigned)row->typeLayoutId) < 0) {
                return ZR_FALSE;
            }
        } else if (row->reason == ZR_AOT_C_TYPE_LAYOUT_REACHABILITY_REASON_EDGE_FRAME_LAYOUT) {
            if (fprintf(file,
                        "/* reachability.typeLayoutManifest.node[%u] = reason=edge.frame_layout predecessorFunction=%u */\n",
                        (unsigned)row->typeLayoutId,
                        (unsigned)row->predecessorFunction) < 0) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    }

    return (TZrBool)(ferror(file) == 0);
}

TZrBool backend_aot_c_type_layout_reachability_write_manifest(
        FILE *file,
        SZrState *state,
        const SZrAotFunctionTable *table,
        const TZrUInt32 *typeLayoutRoots,
        TZrUInt32 typeLayoutRootCount,
        TZrUInt32 expectedRetainedCount) {
    SZrAotCTypeLayoutReachabilityRow *rows = ZR_NULL;
    TZrUInt32 rowCount = 0u;
    TZrUInt32 collectedRowCount = 0u;
    TZrBool success = ZR_FALSE;

    if (file == ZR_NULL || state == ZR_NULL || state->global == ZR_NULL ||
        !backend_aot_c_type_layout_reachability_collect(state,
                                                        table,
                                                        typeLayoutRoots,
                                                        typeLayoutRootCount,
                                                        ZR_NULL,
                                                        0u,
                                                        &rowCount) ||
        rowCount != expectedRetainedCount) {
        return ZR_FALSE;
    }

    if (rowCount > 0u) {
        rows = (SZrAotCTypeLayoutReachabilityRow *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrAotCTypeLayoutReachabilityRow) * rowCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (rows == ZR_NULL ||
            !backend_aot_c_type_layout_reachability_collect(state,
                                                            table,
                                                            typeLayoutRoots,
                                                            typeLayoutRootCount,
                                                            rows,
                                                            rowCount,
                                                            &collectedRowCount) ||
            collectedRowCount != rowCount) {
            if (rows != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              rows,
                                              sizeof(SZrAotCTypeLayoutReachabilityRow) * rowCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            return ZR_FALSE;
        }
    }

    success = backend_aot_c_type_layout_reachability_write_rows(file, rows, rowCount);
    if (rows != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      rows,
                                      sizeof(SZrAotCTypeLayoutReachabilityRow) * rowCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return success;
}
