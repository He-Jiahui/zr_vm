#include "backend_aot_c_debug_sidecar_manifest.h"

#include "backend_aot_exec_ir_source_location.h"

#include <stdint.h>

static const SZrAotFunctionEntry *backend_aot_c_debug_sidecar_find_function(
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

TZrBool backend_aot_c_debug_sidecar_count_locations(
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
            !backend_aot_exec_ir_validate_source_locations(entry->function)) {
            return ZR_FALSE;
        }
        count += entry->function->executionLocationInfoLength;
        if (count > UINT32_MAX) {
            return ZR_FALSE;
        }
    }

    *outCount = (TZrUInt32)count;
    return ZR_TRUE;
}

TZrBool backend_aot_c_debug_sidecar_write_reachability_manifest(
        FILE *file,
        const SZrAotExecIrModule *module,
        const SZrAotFunctionTable *functionTable) {
    TZrUInt32 count = 0u;
    TZrUInt32 nodeIndex = 0u;
    TZrUInt32 functionIndexSpace;

    if (file == ZR_NULL ||
        !backend_aot_c_debug_sidecar_count_locations(module, functionTable, &count)) {
        return ZR_FALSE;
    }
    if (fprintf(file,
                "/* reachability.debugSidecarManifest.version = 1 */\n") < 0 ||
        fprintf(file,
                "/* reachability.debugSidecarManifest.count = %u */\n",
                (unsigned)count) < 0) {
        return ZR_FALSE;
    }

    functionIndexSpace = backend_aot_function_table_index_space(functionTable);
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < functionIndexSpace;
         functionIndex++) {
        const SZrAotFunctionEntry *entry =
                backend_aot_c_debug_sidecar_find_function(functionTable, functionIndex);

        for (TZrUInt32 locationIndex = 0u;
             entry != ZR_NULL &&
             locationIndex < entry->function->executionLocationInfoLength;
             locationIndex++) {
            const SZrFunctionExecutionLocationInfo *location =
                    &entry->function->executionLocationInfoList[locationIndex];

            if (fprintf(file,
                        "/* reachability.debugSidecarManifest.node[%u] = "
                        "reason=edge.debug_sidecar predecessorFunction=%u "
                        "ownerFunction=%u locationIndex=%u instructionOffset=%lld "
                        "lineStart=%u columnStart=%u lineEnd=%u columnEnd=%u */\n",
                        (unsigned)nodeIndex,
                        (unsigned)entry->flatIndex,
                        (unsigned)entry->flatIndex,
                        (unsigned)locationIndex,
                        (long long)location->currentInstructionOffset,
                        (unsigned)location->lineInSource,
                        (unsigned)location->columnInSourceStart,
                        (unsigned)location->lineInSourceEnd,
                        (unsigned)location->columnInSourceEnd) < 0) {
                return ZR_FALSE;
            }
            nodeIndex++;
        }
    }

    return (TZrBool)(nodeIndex == count && ferror(file) == 0);
}
