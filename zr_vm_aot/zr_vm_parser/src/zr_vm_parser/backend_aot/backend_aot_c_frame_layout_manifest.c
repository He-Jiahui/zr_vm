#include "backend_aot_c_frame_layout_manifest.h"

#include <stdint.h>

static const SZrAotFunctionEntry *backend_aot_c_frame_layout_find_function(
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 flatIndex) {
    if (functionTable == ZR_NULL || functionTable->entries == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0u; index < functionTable->count; index++) {
        if (functionTable->entries[index].flatIndex == flatIndex) {
            return &functionTable->entries[index];
        }
    }
    return ZR_NULL;
}

static const TZrChar *backend_aot_c_frame_layout_slot_kind_name(
        TZrUInt8 slotKind) {
    switch ((EZrFunctionFrameSlotKind)slotKind) {
        case ZR_FUNCTION_FRAME_SLOT_KIND_VALUE:
            return "value";
        case ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT:
            return "inline_struct";
        case ZR_FUNCTION_FRAME_SLOT_KIND_UNKNOWN:
        default:
            return ZR_NULL;
    }
}

TZrBool backend_aot_c_frame_layout_count_slots(
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 *outCount) {
    TZrUInt64 count = 0u;

    if (outCount != ZR_NULL) {
        *outCount = 0u;
    }
    if (module == ZR_NULL || functionTable == ZR_NULL || outCount == ZR_NULL ||
        functionTable->count > functionTable->capacity ||
        functionTable->indexSpace > functionTable->capacity ||
        (functionTable->count > 0u && functionTable->entries == ZR_NULL) ||
        (module->functionCount > 0u && module->functions == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (functionTable->count > 0u &&
        backend_aot_function_table_index_space(functionTable) == 0u) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < functionTable->count; index++) {
        const SZrAotFunctionEntry *entry = &functionTable->entries[index];
        const SZrAotExecIrFunction *functionIr =
                backend_aot_exec_ir_find_function(module, entry->flatIndex);

        if (entry->function == ZR_NULL || functionIr == ZR_NULL ||
            functionIr->function != entry->function ||
            (functionIr->frameLayout.slotLayoutCount > 0u &&
             functionIr->frameLayout.slotLayouts == ZR_NULL)) {
            return ZR_FALSE;
        }
        count += functionIr->frameLayout.slotLayoutCount;
        if (count > UINT32_MAX) {
            return ZR_FALSE;
        }
    }

    *outCount = (TZrUInt32)count;
    return ZR_TRUE;
}

TZrBool backend_aot_c_frame_layout_write_reachability_manifest(
        FILE *file,
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable) {
    TZrUInt32 count = 0u;
    TZrUInt32 nodeIndex = 0u;
    TZrUInt32 functionIndexSpace;

    if (file == ZR_NULL ||
        !backend_aot_c_frame_layout_count_slots(module, functionTable, &count)) {
        return ZR_FALSE;
    }
    if (fprintf(file,
                "/* reachability.frameLayoutManifest.version = 1 */\n") < 0 ||
        fprintf(file,
                "/* reachability.frameLayoutManifest.count = %u */\n",
                (unsigned)count) < 0) {
        return ZR_FALSE;
    }

    functionIndexSpace = backend_aot_function_table_index_space(functionTable);
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < functionIndexSpace;
         functionIndex++) {
        const SZrAotFunctionEntry *entry =
                backend_aot_c_frame_layout_find_function(
                        functionTable, functionIndex);
        const SZrAotExecIrFunction *functionIr = entry != ZR_NULL
                ? backend_aot_exec_ir_find_function(module, entry->flatIndex)
                : ZR_NULL;

        for (TZrUInt32 slotLayoutIndex = 0u;
             functionIr != ZR_NULL &&
             slotLayoutIndex < functionIr->frameLayout.slotLayoutCount;
             slotLayoutIndex++) {
            const SZrAotExecIrFrameSlotLayout *layout =
                    &functionIr->frameLayout.slotLayouts[slotLayoutIndex];
            const TZrChar *slotKindName =
                    backend_aot_c_frame_layout_slot_kind_name(layout->slotKind);

            if (slotKindName == ZR_NULL ||
                fprintf(file,
                        "/* reachability.frameLayoutManifest.node[%u] = "
                        "reason=edge.frame_layout predecessorFunction=%u "
                        "ownerFunction=%u slotLayout=%u stackSlot=%u byteOffset=%u "
                        "byteSize=%u byteAlign=%u typeLayoutId=%u slotKind=%s "
                        "isParameter=%u flags=0x%04x */\n",
                        (unsigned)nodeIndex,
                        (unsigned)entry->flatIndex,
                        (unsigned)entry->flatIndex,
                        (unsigned)slotLayoutIndex,
                        (unsigned)layout->stackSlot,
                        (unsigned)layout->byteOffset,
                        (unsigned)layout->byteSize,
                        (unsigned)layout->byteAlign,
                        (unsigned)layout->typeLayoutId,
                        slotKindName,
                        (unsigned)layout->isParameter,
                        (unsigned)layout->reserved0) < 0) {
                return ZR_FALSE;
            }
            nodeIndex++;
        }
    }

    return (TZrBool)(nodeIndex == count && ferror(file) == 0);
}
