#include "zr_vm_parser/exec_ir_aggregate_layout.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define ZR_AGGREGATE_MATERIALIZE_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_AGGREGATE_MATERIALIZE_FNV_PRIME UINT64_C(1099511628211)

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
    if (facts != ZR_NULL) {
        diagnostic->sourceId = facts->sourceId;
        diagnostic->instructionId = facts->instructionId;
        diagnostic->execution.functionToken = facts->functionToken;
        diagnostic->execution.blockId = facts->blockId;
        diagnostic->execution.instructionId = facts->instructionId;
        diagnostic->execution.sourceId = facts->sourceId;
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
    if (identityToken == 0u) identityToken = facts->identityToken;
    if (facts->identityToken != 0u && identityToken != facts->identityToken) {
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
        map->finalized) {
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
    if (field->byteSize == 0u || physicalOffset > UINT32_MAX - field->byteSize) {
        zr_materialize_diag(diagnostic,
                            ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
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
        map->finalized) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (map->fieldCount == 0u || map->entryCount != map->fieldCount) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT,
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
    if (!map->identityPreserved) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH,
                            ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
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
    if (map->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        map->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ||
        !map->finalized || !map->identityPreserved) {
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
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (map == ZR_NULL || entry == ZR_NULL) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                            ZR_NULL, logicalIndex);
        return ZR_FALSE;
    }
    if (!map->finalized || logicalIndex >= map->fieldCount) {
        zr_materialize_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                            ZR_NULL, logicalIndex);
        return ZR_FALSE;
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
