#include "exec_ir_materialize_owners.h"

#include <string.h>

static TZrBool zr_owners_size_valid(TZrUInt32 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
            (TZrUInt64)count <= (TZrUInt64)(SIZE_MAX / elementSize));
}

static TZrBool zr_owners_fail(const SZrExecIrFunction *function,
                              const SZrExecIrStateMapEntry *entry,
                              SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        TZrUInt32 index;
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
        diagnostic->functionToken = function->functionToken;
        diagnostic->instructionId = entry->instructionId;
        diagnostic->sourceId = entry->sourceId;
        for (index = 0u; index < function->blockCount; ++index) {
            SZrExecIrRange range = function->blocks[index].instructionRange;
            if (entry->instructionId - 1u >= range.start &&
                entry->instructionId - 1u - range.start < range.count) {
                diagnostic->blockId = function->blocks[index].id;
                break;
            }
        }
    }
    return ZR_FALSE;
}

TZrBool zr_state_map_owners_valid(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index, at;
    if (entry->ownerStates.start > map->ownerStateCount ||
        entry->ownerStates.count > map->ownerStateCount - entry->ownerStates.start ||
        entry->ownerStates.count != entry->liveValues.count ||
        (entry->ownerStates.count != 0u && map->ownerStatePool == ZR_NULL))
        return zr_owners_fail(function, entry, diagnostic);
    for (at = 0u; at < entry->liveValues.count; ++at) {
        TZrExecIrValueId value = map->valuePool[entry->liveValues.start + at];
        if (value == 0u || value > function->valueCount)
            return zr_owners_fail(function, entry, diagnostic);
    }
    /* Every required value occurs exactly once. Conservative extra values
     * are safe only when statically available; an inactive or conditional
     * value must correspond to a real cleanup obligation. */
    for (index = 0u; index < function->valueCount; ++index) {
        TZrUInt32 occurrences = 0u, roots = 0u, slot = 0u;
        TZrBool live = ZrCore_ExecIr_StateMapLivenessContains(liveness,
                entry->instructionId, index + 1u,
                (TZrBool)(entry->phase != ZR_EXEC_IR_STATE_BEFORE_EFFECT));
        EZrExecIrOwnership kind = function->values[index].ownership;
        for (at = 0u; at < entry->liveValues.count; ++at) {
            if (map->valuePool[entry->liveValues.start + at] == index + 1u) {
                ++occurrences;
                slot = at;
            }
        }
        if ((live && occurrences != 1u) || (!live && occurrences > 1u))
            return zr_owners_fail(function, entry, diagnostic);
        if (occurrences != 0u) {
            EZrExecIrStateMapOwnerState expected = ZrCore_ExecIr_StateMapOwnerAt(
                    function, ownership, liveness, entry->instructionId, index + 1u, entry->phase);
            TZrUInt8 mask = ZrCore_ExecIr_StateMapOwnerMaskAt(
                    function, ownership, liveness, entry->instructionId, index + 1u, entry->phase);
            TZrBool rooted = (TZrBool)((kind == ZR_EXEC_IR_OWNERSHIP_GC ||
                    kind == ZR_EXEC_IR_OWNERSHIP_UNIQUE || kind == ZR_EXEC_IR_OWNERSHIP_SHARED) &&
                    (mask & ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)) != 0u);
            if (expected == ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT ||
                (!live && expected != ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED &&
                          expected != ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) ||
                map->ownerStatePool[entry->ownerStates.start + slot] != (TZrUInt32)expected)
                return zr_owners_fail(function, entry, diagnostic);
            for (at = 0u; at < entry->rootValues.count; ++at)
                if (map->rootPool[entry->rootValues.start + at] == index + 1u) ++roots;
            if (roots != (rooted ? 1u : 0u)) return zr_owners_fail(function, entry, diagnostic);
        }
    }
    return ZR_TRUE;
}

TZrBool zr_state_map_resolve_owners(
        const SZrExecIrResumeRequest *request, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOwnerAnalysis ownership = {0};
    SZrStateMapLiveness liveness = {0};
    const SZrExecIrFunction *function = request->function;
    TZrUInt32 index;
    TZrBool valid = ZR_FALSE;
    EZrExecutionDiagnosticCode code;
    if (!zr_owners_size_valid(request->ownerStateCount, sizeof(*request->ownerStates)) ||
        (request->ownerStates == ZR_NULL) != (request->ownerStateCount == 0u) ||
        (request->ownerStates != ZR_NULL && request->ownerStateCount != function->valueCount))
        return zr_owners_fail(function, entry, diagnostic);
    if (request->ownerStates != ZR_NULL) {
        if (!ZrCore_ExecIr_OwnerAnalysisBuild(function, &ownership, diagnostic)) return ZR_FALSE;
        code = ZrCore_ExecIr_StateMapLivenessBuild(function, &liveness);
        if (code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
            if (diagnostic != ZR_NULL) diagnostic->code = code;
            goto finish;
        }
    }
    for (index = 0u; index < entry->liveValues.count; ++index) {
        TZrExecIrValueId value = map->valuePool[entry->liveValues.start + index];
        TZrUInt32 state = map->ownerStatePool[entry->ownerStates.start + index];
        if (request->ownerStates != ZR_NULL) {
            TZrUInt8 mask = ZrCore_ExecIr_StateMapOwnerMaskAt(function, &ownership,
                    &liveness, entry->instructionId, value, entry->phase);
            TZrUInt32 concrete = request->ownerStates[value - 1u];
            if (concrete >= ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL ||
                (mask & ZR_EXEC_IR_OWNER_STATE_BIT(concrete)) == 0u ||
                (state != ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL && concrete != state)) {
                zr_owners_fail(function, entry, diagnostic);
                goto finish;
            }
            state = concrete;
        } else if (state == ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL && prepared != ZR_NULL) {
            zr_owners_fail(function, entry, diagnostic);
            goto finish;
        }
        if (prepared != ZR_NULL) prepared->ownerStates[index] = state;
    }
    if (prepared != ZR_NULL) {
        TZrUInt32 rootCount = 0u;
        for (index = 0u; index < prepared->rootCount; ++index) {
            TZrUInt32 live;
            for (live = 0u; live < prepared->valueCount; ++live) {
                if (prepared->values[live] == prepared->roots[index] &&
                    prepared->ownerStates[live] == ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED) {
                    prepared->roots[rootCount++] = prepared->roots[index];
                    break;
                }
            }
        }
        prepared->rootCount = rootCount;
    }
    valid = ZR_TRUE;
finish:
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    ZrCore_ExecIr_StateMapLivenessFree(&liveness);
    return valid;
}
