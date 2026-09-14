#include "zr_vm_parser/exec_ir_aggregate_layout.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define ZR_AGGREGATE_MATERIALIZE_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_AGGREGATE_MATERIALIZE_FNV_PRIME UINT64_C(1099511628211)

static EZrExecutionDiagnosticCode zr_materialize_execution_code(
        EZrExecIrAggregateStatus status) {
    switch (status) {
        case ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT:
            return ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        case ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH:
            return ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH;
        case ZR_EXEC_IR_AGGREGATE_GENERATION_STALE:
            return ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION;
        case ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH:
        case ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID:
            return ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH;
        case ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED:
        case ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH:
        case ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ:
            return ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED;
        case ZR_EXEC_IR_AGGREGATE_OK:
            return ZR_EXECUTION_DIAGNOSTIC_NONE;
        case ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT:
        case ZR_EXEC_IR_AGGREGATE_FIELD_ORDER:
        case ZR_EXEC_IR_AGGREGATE_INVALID_FIELD_LAYOUT:
        case ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS:
        case ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY:
        case ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED:
        case ZR_EXEC_IR_AGGREGATE_UNKNOWN_ESCAPE:
        case ZR_EXEC_IR_AGGREGATE_PUBLIC_LAYOUT:
        case ZR_EXEC_IR_AGGREGATE_REFLECTION_VISIBLE:
        case ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE:
        case ZR_EXEC_IR_AGGREGATE_SERIALIZATION_VISIBLE:
        case ZR_EXEC_IR_AGGREGATE_IDENTITY_NOT_RECONSTRUCTIBLE:
        case ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE:
        case ZR_EXEC_IR_AGGREGATE_ROOT_METADATA_MISSING:
        case ZR_EXEC_IR_AGGREGATE_DROP_ORDER_UNSAFE:
        case ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN:
        case ZR_EXEC_IR_AGGREGATE_LIFETIME_OPEN:
        case ZR_EXEC_IR_AGGREGATE_PROFILE_MISSING:
        case ZR_EXEC_IR_AGGREGATE_COST_UNPROFITABLE:
        case ZR_EXEC_IR_AGGREGATE_BRIDGE_TOO_EXPENSIVE:
        case ZR_EXEC_IR_AGGREGATE_UNSUPPORTED:
        case ZR_EXEC_IR_AGGREGATE_STATUS_COUNT:
        default:
            return ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    }
}

static void zr_materialize_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_AGGREGATE_MATERIALIZE_FNV_PRIME;
    }
}

static void zr_materialize_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_AGGREGATE_MATERIALIZE_FNV_PRIME;
    }
}

static void zr_materialize_diag(SZrExecIrAggregateDiagnostic *diagnostic,
                                EZrExecIrAggregateStatus status,
                                const SZrExecIrAggregateFacts *facts,
                                TZrUInt32 fieldIndex) {
    if (diagnostic == ZR_NULL) return;
    ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    diagnostic->status = status;
    diagnostic->fieldIndex = fieldIndex;
    diagnostic->execution.code = zr_materialize_execution_code(status);
    if (facts != ZR_NULL) {
        diagnostic->sourceId = facts->sourceId;
        diagnostic->instructionId = facts->instructionId;
        diagnostic->execution.functionToken = facts->functionToken;
        diagnostic->execution.blockId = facts->blockId;
        diagnostic->execution.instructionId = facts->instructionId;
        diagnostic->execution.sourceId = facts->sourceId;
    }
}

/* Validate metadata that can be checked without the source facts/candidate.
 * Resolve is intentionally callable at a boundary where those producers may
 * no longer be resident, so it must not expose a forged physical location
 * merely because the caller recomputed mapHash. */
static TZrBool zr_materialize_entry_intrinsic(
        const SZrExecIrMaterializationEntry *entry,
        TZrUInt32 fieldCount,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 fieldIndex;
    if (entry == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    fieldIndex = entry->logicalIndex;
    if (fieldIndex >= fieldCount || entry->physicalIndex >= fieldCount ||
        entry->byteSize == 0u ||
        entry->physicalOffset > UINT32_MAX - entry->byteSize ||
        entry->typeToken == 0u || entry->layoutId == 0u) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                            ZR_NULL, fieldIndex);
        return ZR_FALSE;
    }
    if (entry->ownership < ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
        entry->ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                            ZR_NULL, fieldIndex);
        return ZR_FALSE;
    }
    if ((entry->flags & ~ZR_EXEC_IR_AGGREGATE_FIELD_KNOWN_MASK) != 0u) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
                            ZR_NULL, fieldIndex);
        return ZR_FALSE;
    }
    if (entry->initialized > (TZrBool)ZR_TRUE) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, fieldIndex);
        return ZR_FALSE;
    }
    if ((entry->flags & ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT) != 0u &&
        entry->rootSlot == ZR_EXEC_IR_AGGREGATE_INVALID_INDEX) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_ROOT_METADATA_MISSING,
                            ZR_NULL, fieldIndex);
        return ZR_FALSE;
    }
    if ((entry->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
         entry->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED ||
         entry->ownership == ZR_EXEC_IR_OWNERSHIP_GC ||
         (entry->flags & ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED) != 0u) &&
        entry->dropOrder == ZR_EXEC_IR_AGGREGATE_INVALID_INDEX) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_DROP_ORDER_UNSAFE,
                            ZR_NULL, fieldIndex);
        return ZR_FALSE;
    }
    if ((entry->flags & ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED) != 0u &&
        entry->aliasClass == ZR_EXEC_IR_AGGREGATE_UNKNOWN_ALIAS) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN,
                            ZR_NULL, fieldIndex);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool zr_materialize_map_intrinsic(
        const SZrExecIrMaterializationMap *map,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    if (map == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        map->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->fieldCount == 0u ||
        map->fieldCount > ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ||
        map->entryCount > ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ||
        map->entryCount != map->fieldCount ||
        map->physicalLayoutHash == 0u || map->logicalTypeToken == 0u ||
        map->logicalLayoutId == 0u || map->logicalLayoutHash == 0u ||
        map->generation == 0u || map->bridgeVersion == 0u) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if ((map->flags & ZR_EXEC_IR_AGGREGATE_FLAG_IDENTITY_OBSERVED) != 0u &&
        map->identityToken == 0u) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if ((map->flags & ~ZR_EXEC_IR_AGGREGATE_FLAG_KNOWN_MASK) != 0u) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->identityPreserved != (TZrBool)ZR_TRUE ||
        map->finalized != (TZrBool)ZR_TRUE) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* Finalization canonicalizes entry order so equivalent maps assembled by
 * different producers have the same serialized/hash representation.  The
 * logical index is the stable primary key; physical index is only a tie-break
 * for defensive handling of a malformed partial map. */
static void zr_materialize_sort_entries(SZrExecIrMaterializationMap *map) {
    TZrUInt32 index;
    if (map == ZR_NULL) return;
    for (index = 1u; index < map->entryCount; ++index) {
        SZrExecIrMaterializationEntry value = map->entries[index];
        TZrUInt32 cursor = index;
        while (cursor != 0u) {
            const SZrExecIrMaterializationEntry *prior =
                &map->entries[cursor - 1u];
            if (prior->logicalIndex < value.logicalIndex ||
                (prior->logicalIndex == value.logicalIndex &&
                 prior->physicalIndex <= value.physicalIndex))
                break;
            map->entries[cursor] = *prior;
            --cursor;
        }
        map->entries[cursor] = value;
    }
}

void ZrParser_ExecIr_MaterializationMapInit(
        SZrExecIrMaterializationMap *map) {
    if (map == ZR_NULL) return;
    memset(map, 0, sizeof(*map));
    map->magic = ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC;
    map->schemaVersion = ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION;
}

TZrUInt64 ZrParser_ExecIr_MaterializationMapHash(
        const SZrExecIrMaterializationMap *map) {
    TZrUInt64 hash = ZR_AGGREGATE_MATERIALIZE_FNV_OFFSET;
    TZrUInt32 index;
    if (map == ZR_NULL) return 0u;
    zr_materialize_hash_u32(&hash, map->magic);
    zr_materialize_hash_u32(&hash, map->schemaVersion);
    zr_materialize_hash_u32(&hash, map->logicalTypeToken);
    zr_materialize_hash_u32(&hash, map->logicalLayoutId);
    zr_materialize_hash_u64(&hash, map->logicalLayoutHash);
    zr_materialize_hash_u64(&hash, map->physicalLayoutHash);
    zr_materialize_hash_u64(&hash, map->generation);
    zr_materialize_hash_u32(&hash, map->bridgeVersion);
    zr_materialize_hash_u64(&hash, map->identityToken);
    zr_materialize_hash_u32(&hash, map->fieldCount);
    zr_materialize_hash_u32(&hash, map->entryCount);
    zr_materialize_hash_u32(&hash, map->flags);
    zr_materialize_hash_u32(&hash, (TZrUInt32)map->identityPreserved);
    zr_materialize_hash_u32(&hash, (TZrUInt32)map->finalized);
    for (index = 0u; index < map->entryCount &&
                      index < ZR_EXEC_IR_AGGREGATE_MAX_FIELDS; ++index) {
        const SZrExecIrMaterializationEntry *entry = &map->entries[index];
        zr_materialize_hash_u32(&hash, entry->logicalIndex);
        zr_materialize_hash_u32(&hash, entry->physicalIndex);
        zr_materialize_hash_u32(&hash, entry->physicalOffset);
        zr_materialize_hash_u32(&hash, entry->byteSize);
        zr_materialize_hash_u32(&hash, entry->typeToken);
        zr_materialize_hash_u32(&hash, entry->layoutId);
        zr_materialize_hash_u32(&hash, entry->rootSlot);
        zr_materialize_hash_u32(&hash, entry->dropOrder);
        zr_materialize_hash_u64(&hash, entry->aliasClass);
        zr_materialize_hash_u32(&hash, (TZrUInt32)entry->ownership);
        zr_materialize_hash_u32(&hash, entry->flags);
        zr_materialize_hash_u32(&hash, (TZrUInt32)entry->initialized);
    }
    return hash == 0u ? 1u : hash;
}

static TZrBool zr_materialize_basic_candidate(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    if (facts == ZR_NULL || candidate == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!ZrParser_ExecIr_AggregateFactsValidate(facts, diagnostic)) return ZR_FALSE;
    if (candidate->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        candidate->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ||
        candidate->fieldCount != facts->fieldCount ||
        candidate->logicalTypeToken != facts->logicalTypeToken ||
        candidate->logicalLayoutId != facts->logicalLayoutId ||
        candidate->logicalLayoutHash != facts->logicalLayoutHash ||
        candidate->generation != facts->generation ||
        candidate->bridgeVersion != facts->bridgeVersion) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_GENERATION_STALE,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (candidate->strategy != ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA &&
        candidate->strategy != ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNSUPPORTED,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    /* A map is consumed at a deopt/native boundary, so reject stale or
     * hand-edited physical metadata before accepting any entries. */
    if (!ZrParser_ExecIr_DataLayoutCandidateValidate(facts, candidate,
                                                     diagnostic)) {
        return ZR_FALSE;
    }
    if (!candidate->ownershipMigrated || !candidate->rootsMigrated ||
        !candidate->dropOrderPreserved) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_MaterializationMapBegin(
        SZrExecIrMaterializationMap *map,
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCandidate *candidate,
        TZrUInt64 identityToken,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (map == ZR_NULL || facts == ZR_NULL || candidate == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_materialize_basic_candidate(facts, candidate, diagnostic)) return ZR_FALSE;
    /* Do not mint or silently substitute an identity in a bridge map.  The
     * caller must carry the exact logical token; zero is valid only for a
     * value aggregate whose facts token is also zero. */
    if (identityToken != facts->identityToken) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_MaterializationMapInit(map);
    map->logicalTypeToken = facts->logicalTypeToken;
    map->logicalLayoutId = facts->logicalLayoutId;
    map->logicalLayoutHash = facts->logicalLayoutHash;
    map->physicalLayoutHash = candidate->physicalLayoutHash;
    map->generation = facts->generation;
    map->bridgeVersion = facts->bridgeVersion;
    map->identityToken = identityToken;
    map->fieldCount = facts->fieldCount;
    /* Preserve observability and boundary bits across the logical bridge. */
    map->flags = facts->flags;
    map->identityPreserved = ZR_TRUE;
    map->finalized = ZR_FALSE;
    map->entryCount = 0u;
    map->mapHash = 0u;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_MaterializationMapAddField(
        SZrExecIrMaterializationMap *map,
        const SZrExecIrAggregateFieldFact *field,
        TZrUInt32 physicalIndex,
        TZrUInt32 physicalOffset,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    SZrExecIrMaterializationEntry *entry;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (map == ZR_NULL || field == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        map->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ||
        map->finalized != (TZrBool)ZR_FALSE ||
        map->fieldCount == 0u ||
        map->fieldCount > ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ||
        map->entryCount > ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ||
        (map->flags & ~ZR_EXEC_IR_AGGREGATE_FLAG_KNOWN_MASK) != 0u) {
        if ((map->flags & ~ZR_EXEC_IR_AGGREGATE_FLAG_KNOWN_MASK) != 0u)
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
                                ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        else
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                                ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (field->logicalIndex >= map->fieldCount ||
        map->entryCount >= ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ||
        physicalIndex >= map->fieldCount) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if (field->byteSize == 0u || physicalOffset > UINT32_MAX - field->byteSize ||
        field->typeToken == 0u || field->layoutId == 0u) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if (field->ownership < ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
        field->ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if ((field->flags & ~ZR_EXEC_IR_AGGREGATE_FIELD_KNOWN_MASK) != 0u) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if (field->initialized > (TZrBool)ZR_TRUE) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    /* A materialisation entry has no read-before-init bit; accepting such a
     * field would silently turn an unsafe source state into a readable value
     * at the boundary.  Check boolean encodings separately so a malformed
     * value such as 2 is reported as invalid rather than as a semantic proof
     * failure. */
    if (field->readBeforeInit > (TZrBool)ZR_TRUE ||
        field->ownershipTransferProven > (TZrBool)ZR_TRUE ||
        field->aliasProven > (TZrBool)ZR_TRUE ||
        field->aliasLocation.baseKind < ZR_EXEC_IR_ALIAS_BASE_UNKNOWN ||
        field->aliasLocation.baseKind > ZR_EXEC_IR_ALIAS_BASE_EXTERNAL ||
        field->aliasLocation.hasStableBase > (TZrBool)ZR_TRUE ||
        field->aliasLocation.escaped > (TZrBool)ZR_TRUE ||
        field->aliasLocation.unknownWrite > (TZrBool)ZR_TRUE ||
        field->aliasLocation.projectionDisjoint > (TZrBool)ZR_TRUE) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if (field->readBeforeInit) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if ((field->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
         field->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED ||
         field->ownership == ZR_EXEC_IR_OWNERSHIP_GC) &&
        !field->ownershipTransferProven) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED) != 0u &&
        (!field->aliasProven || field->aliasClass == ZR_EXEC_IR_AGGREGATE_UNKNOWN_ALIAS ||
         field->aliasLocation.baseKind == ZR_EXEC_IR_ALIAS_BASE_UNKNOWN ||
         field->aliasLocation.baseId == 0u ||
         field->aliasLocation.projectionId == 0u ||
         field->aliasLocation.layoutId == 0u ||
         !field->aliasLocation.hasStableBase || field->aliasLocation.escaped ||
         field->aliasLocation.unknownWrite ||
         (field->aliasLocation.generation != 0u &&
          field->aliasLocation.generation != map->generation))) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        if (map->entries[index].logicalIndex == field->logicalIndex ||
            map->entries[index].physicalIndex == physicalIndex) {
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                                ZR_NULL, field->logicalIndex);
            return ZR_FALSE;
        }
    }
    if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT) != 0u &&
        field->rootSlot == ZR_EXEC_IR_AGGREGATE_INVALID_INDEX) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_ROOT_METADATA_MISSING,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if ((field->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
         field->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED ||
         field->ownership == ZR_EXEC_IR_OWNERSHIP_GC ||
         (field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED) != 0u) &&
        field->dropOrder == ZR_EXEC_IR_AGGREGATE_INVALID_INDEX) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_DROP_ORDER_UNSAFE,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED) != 0u &&
        field->aliasClass == ZR_EXEC_IR_AGGREGATE_UNKNOWN_ALIAS) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN,
                            ZR_NULL, field->logicalIndex);
        return ZR_FALSE;
    }
    entry = &map->entries[map->entryCount++];
    memset(entry, 0, sizeof(*entry));
    entry->logicalIndex = field->logicalIndex;
    entry->physicalIndex = physicalIndex;
    entry->physicalOffset = physicalOffset;
    entry->byteSize = field->byteSize;
    entry->typeToken = field->typeToken;
    entry->layoutId = field->layoutId;
    entry->rootSlot = field->rootSlot;
    entry->dropOrder = field->dropOrder;
    entry->aliasClass = field->aliasClass;
    entry->ownership = field->ownership;
    entry->flags = field->flags;
    entry->initialized = field->initialized;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_MaterializationMapFinalize(
        SZrExecIrMaterializationMap *map,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (map == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        map->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ||
        (map->flags & ~ZR_EXEC_IR_AGGREGATE_FLAG_KNOWN_MASK) != 0u ||
        map->finalized) {
        if ((map->flags & ~ZR_EXEC_IR_AGGREGATE_FLAG_KNOWN_MASK) != 0u)
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
                                ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        else
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                                ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->fieldCount == 0u ||
        map->fieldCount > ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ||
        map->entryCount > ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ||
        map->entryCount != map->fieldCount ||
        map->logicalTypeToken == 0u || map->logicalLayoutId == 0u ||
        map->logicalLayoutHash == 0u || map->physicalLayoutHash == 0u ||
        map->generation == 0u || map->bridgeVersion == 0u) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if ((map->flags & ZR_EXEC_IR_AGGREGATE_FLAG_IDENTITY_OBSERVED) != 0u &&
        map->identityToken == 0u) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    for (index = 0u; index < map->fieldCount; ++index) {
        TZrUInt32 cursor;
        TZrBool found = ZR_FALSE;
        for (cursor = 0u; cursor < map->entryCount; ++cursor) {
            if (map->entries[cursor].logicalIndex == index) {
                found = ZR_TRUE;
                break;
            }
        }
        if (!found) {
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                                ZR_NULL, index);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrMaterializationEntry *entry = &map->entries[index];
        TZrUInt32 prior;
        if (!zr_materialize_entry_intrinsic(entry, map->fieldCount,
                                            diagnostic)) return ZR_FALSE;
        for (prior = 0u; prior < index; ++prior) {
            if (map->entries[prior].physicalIndex == entry->physicalIndex) {
                zr_materialize_diag(diagnostic,
                                    ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                                    ZR_NULL, entry->logicalIndex);
                return ZR_FALSE;
            }
            if (entry->physicalOffset <
                        map->entries[prior].physicalOffset +
                            map->entries[prior].byteSize &&
                map->entries[prior].physicalOffset <
                        entry->physicalOffset + entry->byteSize) {
                zr_materialize_diag(diagnostic,
                                    ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                                    ZR_NULL, entry->logicalIndex);
                return ZR_FALSE;
            }
        }
    }
    if (map->identityPreserved != (TZrBool)ZR_TRUE) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    zr_materialize_sort_entries(map);
    map->finalized = ZR_TRUE;
    map->mapHash = ZrParser_ExecIr_MaterializationMapHash(map);
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_MaterializationMapValidate(
        const SZrExecIrMaterializationMap *map,
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt64 expectedHash;
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (map == ZR_NULL || facts == ZR_NULL || candidate == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_materialize_basic_candidate(facts, candidate, diagnostic)) return ZR_FALSE;
    if (!zr_materialize_map_intrinsic(map, diagnostic)) return ZR_FALSE;
    if (map->flags != facts->flags) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->logicalTypeToken != facts->logicalTypeToken ||
        map->logicalLayoutId != facts->logicalLayoutId ||
        map->logicalLayoutHash != facts->logicalLayoutHash ||
        map->physicalLayoutHash != candidate->physicalLayoutHash ||
        map->generation != facts->generation ||
        map->bridgeVersion != facts->bridgeVersion ||
        map->fieldCount != facts->fieldCount ||
        map->entryCount != map->fieldCount ||
        map->identityToken != facts->identityToken) {
        if (map->identityToken != facts->identityToken) {
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH,
                                facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        } else {
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_GENERATION_STALE,
                                facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        }
        return ZR_FALSE;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrMaterializationEntry *entry = &map->entries[index];
        const SZrExecIrAggregateFieldFact *field;
        TZrUInt32 physicalPosition;
        TZrBool physicalFound = ZR_FALSE;
        if (entry->logicalIndex >= facts->fieldCount) {
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                                facts, entry->logicalIndex);
            return ZR_FALSE;
        }
        field = &facts->fields[entry->logicalIndex];
        for (physicalPosition = 0u;
             physicalPosition < candidate->fieldCount; ++physicalPosition) {
            if (candidate->fieldOrder[physicalPosition] == entry->logicalIndex) {
                physicalFound = ZR_TRUE;
                if (entry->physicalIndex != physicalPosition) {
                    zr_materialize_diag(
                            diagnostic,
                            ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                            facts, entry->logicalIndex);
                    return ZR_FALSE;
                }
                if (entry->physicalOffset !=
                            candidate->columnOffset[physicalPosition] ||
                    entry->byteSize != candidate->columnStride[physicalPosition]) {
                    zr_materialize_diag(
                            diagnostic,
                            ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                            facts, entry->logicalIndex);
                    return ZR_FALSE;
                }
                break;
            }
        }
        if (!physicalFound) {
            zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                                facts, entry->logicalIndex);
            return ZR_FALSE;
        }
        if (entry->typeToken != field->typeToken ||
            entry->layoutId != field->layoutId ||
            entry->byteSize != field->byteSize ||
            entry->rootSlot != field->rootSlot ||
            entry->dropOrder != field->dropOrder ||
            entry->ownership != field->ownership ||
            entry->flags != field->flags ||
            entry->initialized != field->initialized ||
            entry->aliasClass != field->aliasClass) {
            zr_materialize_diag(diagnostic,
                                ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                                facts, entry->logicalIndex);
            return ZR_FALSE;
        }
        {
            TZrUInt32 prior;
            for (prior = 0u; prior < index; ++prior) {
                if (map->entries[prior].logicalIndex == entry->logicalIndex ||
                    map->entries[prior].physicalIndex == entry->physicalIndex) {
                    zr_materialize_diag(diagnostic,
                                        ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                                        facts, entry->logicalIndex);
                    return ZR_FALSE;
                }
            }
        }
    }
    expectedHash = ZrParser_ExecIr_MaterializationMapHash(map);
    if (map->mapHash != expectedHash) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH,
                            facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expectedHash;
            diagnostic->actualHash = map->mapHash;
            diagnostic->execution.expectedHash = expectedHash;
            diagnostic->execution.actualHash = map->mapHash;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_MaterializationMapResolve(
        const SZrExecIrMaterializationMap *map,
        TZrUInt32 logicalIndex,
        SZrExecIrMaterializationEntry *entry,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt64 expectedHash;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (map == ZR_NULL || entry == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, logicalIndex);
        return ZR_FALSE;
    }
    if (!zr_materialize_map_intrinsic(map, diagnostic)) return ZR_FALSE;
    if (logicalIndex >= map->fieldCount) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT,
                            ZR_NULL, logicalIndex);
        return ZR_FALSE;
    }
    expectedHash = ZrParser_ExecIr_MaterializationMapHash(map);
    if (map->mapHash != expectedHash) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH,
                            ZR_NULL, logicalIndex);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = expectedHash;
            diagnostic->actualHash = map->mapHash;
            diagnostic->execution.expectedHash = expectedHash;
            diagnostic->execution.actualHash = map->mapHash;
        }
        return ZR_FALSE;
    }
    /* Resolve is intentionally usable without a separate Validate call, so
     * repeat the intrinsic entry checks before exposing a physical location. */
    for (index = 0u; index < map->entryCount; ++index) {
        const SZrExecIrMaterializationEntry *current = &map->entries[index];
        TZrUInt32 prior;
        if (!zr_materialize_entry_intrinsic(current, map->fieldCount,
                                            diagnostic)) return ZR_FALSE;
        for (prior = 0u; prior < index; ++prior) {
            if (map->entries[prior].logicalIndex == current->logicalIndex ||
                map->entries[prior].physicalIndex == current->physicalIndex) {
                zr_materialize_diag(diagnostic,
                                    ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                                    ZR_NULL, current->logicalIndex);
                return ZR_FALSE;
            }
            if (current->physicalOffset <
                        map->entries[prior].physicalOffset +
                            map->entries[prior].byteSize &&
                map->entries[prior].physicalOffset <
                        current->physicalOffset + current->byteSize) {
                zr_materialize_diag(diagnostic,
                                    ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
                                    ZR_NULL, current->logicalIndex);
                return ZR_FALSE;
            }
        }
    }
    for (index = 0u; index < map->entryCount; ++index) {
        if (map->entries[index].logicalIndex == logicalIndex) {
            *entry = map->entries[index];
            return ZR_TRUE;
        }
    }
    zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                        ZR_NULL, logicalIndex);
    return ZR_FALSE;
}
