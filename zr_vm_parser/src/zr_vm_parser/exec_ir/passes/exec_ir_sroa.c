#include "zr_vm_parser/exec_ir_aggregate_layout.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define ZR_AGGREGATE_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_AGGREGATE_FNV_PRIME UINT64_C(1099511628211)

static void zr_aggregate_diag(SZrExecIrAggregateDiagnostic *diagnostic,
                              EZrExecIrAggregateStatus status,
                              const SZrExecIrAggregateFacts *facts,
                              TZrUInt32 fieldIndex) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
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

static void zr_aggregate_diag_hash(SZrExecIrAggregateDiagnostic *diagnostic,
                                   EZrExecIrAggregateStatus status,
                                   TZrUInt64 expected, TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->status = status;
    diagnostic->fieldIndex = ZR_EXEC_IR_AGGREGATE_INVALID_INDEX;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
    diagnostic->execution.code = ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH;
    diagnostic->execution.expectedHash = expected;
    diagnostic->execution.actualHash = actual;
}

static void zr_aggregate_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_AGGREGATE_FNV_PRIME;
    }
}

static void zr_aggregate_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0u; index < 8u; ++index) {
        *hash ^= (TZrUInt8)(value >> (index * 8u));
        *hash *= ZR_AGGREGATE_FNV_PRIME;
    }
}

static TZrBool zr_aggregate_bool_valid(TZrBool value) {
    return (TZrBool)(value <= (TZrBool)ZR_TRUE);
}

static TZrBool zr_aggregate_pow2(TZrUInt32 value) {
    return (TZrBool)(value != 0u && (value & (value - 1u)) == 0u);
}

static TZrBool zr_field_unbox_eligible(
        const SZrExecIrAggregateFieldFact *field) {
    TZrUInt32 semanticFlags =
        ZR_EXEC_IR_AGGREGATE_FIELD_REF |
        ZR_EXEC_IR_AGGREGATE_FIELD_OWNERSHIP |
        ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT |
        ZR_EXEC_IR_AGGREGATE_FIELD_NESTED_LAYOUT |
        ZR_EXEC_IR_AGGREGATE_FIELD_IDENTITY_BEARING;
    if (field == ZR_NULL) return ZR_FALSE;
    return (TZrBool)(field->byteSize <= sizeof(TZrUInt64) &&
                     (field->flags & semanticFlags) == 0u &&
                     field->ownership != ZR_EXEC_IR_OWNERSHIP_UNIQUE &&
                     field->ownership != ZR_EXEC_IR_OWNERSHIP_SHARED &&
                     field->ownership != ZR_EXEC_IR_OWNERSHIP_GC);
}

void ZrParser_ExecIr_AggregateDiagnosticInit(
        SZrExecIrAggregateDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->status = ZR_EXEC_IR_AGGREGATE_OK;
    diagnostic->fieldIndex = ZR_EXEC_IR_AGGREGATE_INVALID_INDEX;
}

const TZrChar *ZrParser_ExecIr_AggregateStatusName(
        EZrExecIrAggregateStatus status) {
    switch (status) {
        case ZR_EXEC_IR_AGGREGATE_OK: return "ok";
        case ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH: return "schema-mismatch";
        case ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT: return "field-limit";
        case ZR_EXEC_IR_AGGREGATE_FIELD_ORDER: return "field-order";
        case ZR_EXEC_IR_AGGREGATE_INVALID_FIELD_LAYOUT: return "invalid-field-layout";
        case ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS: return "unknown-flags";
        case ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY: return "union-overlay";
        case ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ: return "uninitialized-read";
        case ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED: return "address-observed";
        case ZR_EXEC_IR_AGGREGATE_UNKNOWN_ESCAPE: return "unknown-escape";
        case ZR_EXEC_IR_AGGREGATE_PUBLIC_LAYOUT: return "public-layout";
        case ZR_EXEC_IR_AGGREGATE_REFLECTION_VISIBLE: return "reflection-visible";
        case ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE: return "ffi-visible";
        case ZR_EXEC_IR_AGGREGATE_SERIALIZATION_VISIBLE:
            return "serialization-visible";
        case ZR_EXEC_IR_AGGREGATE_IDENTITY_NOT_RECONSTRUCTIBLE:
            return "identity-not-reconstructible";
        case ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE: return "ownership-unsafe";
        case ZR_EXEC_IR_AGGREGATE_ROOT_METADATA_MISSING:
            return "root-metadata-missing";
        case ZR_EXEC_IR_AGGREGATE_DROP_ORDER_UNSAFE:
            return "drop-order-unsafe";
        case ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN: return "alias-unproven";
        case ZR_EXEC_IR_AGGREGATE_LIFETIME_OPEN: return "lifetime-open";
        case ZR_EXEC_IR_AGGREGATE_PROFILE_MISSING: return "profile-missing";
        case ZR_EXEC_IR_AGGREGATE_COST_UNPROFITABLE: return "cost-unprofitable";
        case ZR_EXEC_IR_AGGREGATE_BRIDGE_TOO_EXPENSIVE:
            return "bridge-too-expensive";
        case ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH: return "hash-mismatch";
        case ZR_EXEC_IR_AGGREGATE_GENERATION_STALE: return "generation-stale";
        case ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH: return "identity-mismatch";
        case ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID:
            return "physical-layout-invalid";
        case ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED:
            return "materialization-required";
        case ZR_EXEC_IR_AGGREGATE_UNSUPPORTED: return "unsupported";
        case ZR_EXEC_IR_AGGREGATE_STATUS_COUNT:
        default: return "invalid";
    }
}

void ZrParser_ExecIr_AggregateFactsInit(SZrExecIrAggregateFacts *facts) {
    if (facts == ZR_NULL) return;
    memset(facts, 0, sizeof(*facts));
    facts->magic = ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC;
    facts->schemaVersion = ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION;
    facts->bridgeVersion = 1u;
    facts->fieldCount = 0u;
}

TZrUInt64 ZrParser_ExecIr_AggregateFactsHash(
        const SZrExecIrAggregateFacts *facts) {
    TZrUInt64 hash = ZR_AGGREGATE_FNV_OFFSET;
    TZrUInt32 index;
    if (facts == ZR_NULL) return 0u;
    zr_aggregate_hash_u32(&hash, facts->logicalTypeToken);
    zr_aggregate_hash_u32(&hash, facts->logicalLayoutId);
    zr_aggregate_hash_u64(&hash, facts->logicalLayoutHash);
    zr_aggregate_hash_u64(&hash, facts->generation);
    zr_aggregate_hash_u32(&hash, facts->fieldCount);
    zr_aggregate_hash_u32(&hash, facts->flags);
    zr_aggregate_hash_u64(&hash, facts->identityToken);
    zr_aggregate_hash_u32(&hash, facts->bridgeVersion);
    zr_aggregate_hash_u32(&hash, facts->sourceId);
    zr_aggregate_hash_u32(&hash, facts->functionToken);
    zr_aggregate_hash_u32(&hash, facts->blockId);
    zr_aggregate_hash_u32(&hash, facts->instructionId);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->privateClosedWorld);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->closedLifetime);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->reconstructibleIdentity);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->noUnknownEscape);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->noAddressObservation);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->publicLayout);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->reflectionVisible);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->ffiVisible);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->serializationObserved);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->aliasProven);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)facts->dropOrderProven);
    for (index = 0u; index < facts->fieldCount &&
                      index < ZR_EXEC_IR_AGGREGATE_MAX_FIELDS; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[index];
        zr_aggregate_hash_u32(&hash, field->fieldId);
        zr_aggregate_hash_u32(&hash, field->logicalIndex);
        zr_aggregate_hash_u32(&hash, field->typeToken);
        zr_aggregate_hash_u32(&hash, field->layoutId);
        zr_aggregate_hash_u32(&hash, field->logicalOffset);
        zr_aggregate_hash_u32(&hash, field->byteSize);
        zr_aggregate_hash_u32(&hash, field->byteAlign);
        zr_aggregate_hash_u32(&hash, field->dropOrder);
        zr_aggregate_hash_u32(&hash, field->rootSlot);
        zr_aggregate_hash_u64(&hash, field->fieldHash);
        zr_aggregate_hash_u64(&hash, field->aliasClass);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->ownership);
        zr_aggregate_hash_u32(&hash, field->flags);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->initialized);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->readBeforeInit);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->ownershipTransferProven);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->aliasProven);
        zr_aggregate_hash_u32(&hash, field->useCount);
        zr_aggregate_hash_u32(&hash, field->loopUseCount);
        zr_aggregate_hash_u64(&hash, field->aliasLocation.baseId);
        zr_aggregate_hash_u64(&hash, field->aliasLocation.projectionId);
        zr_aggregate_hash_u32(&hash, field->aliasLocation.layoutId);
        zr_aggregate_hash_u64(&hash, field->aliasLocation.generation);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->aliasLocation.baseKind);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->aliasLocation.hasStableBase);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->aliasLocation.escaped);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->aliasLocation.unknownWrite);
        zr_aggregate_hash_u32(&hash, (TZrUInt32)field->aliasLocation.projectionDisjoint);
    }
    return hash == 0u ? 1u : hash;
}

TZrBool ZrParser_ExecIr_AggregateFactsValidate(
        const SZrExecIrAggregateFacts *facts,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    TZrUInt32 previousOffset = 0u;
    TZrUInt32 previousEnd = 0u;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (facts == ZR_NULL) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                          ZR_NULL, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (facts->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        facts->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        if (diagnostic != ZR_NULL) {
            diagnostic->expectedHash = ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION;
            diagnostic->actualHash = facts->schemaVersion;
        }
        return ZR_FALSE;
    }
    if (facts->fieldCount == 0u ||
        facts->fieldCount > ZR_EXEC_IR_AGGREGATE_MAX_FIELDS) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (facts->logicalTypeToken == 0u || facts->logicalLayoutId == 0u ||
        facts->logicalLayoutHash == 0u || facts->generation == 0u ||
        facts->bridgeVersion == 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if ((facts->flags & ~ZR_EXEC_IR_AGGREGATE_FLAG_KNOWN_MASK) != 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_aggregate_bool_valid(facts->privateClosedWorld) ||
        !zr_aggregate_bool_valid(facts->closedLifetime) ||
        !zr_aggregate_bool_valid(facts->reconstructibleIdentity) ||
        !zr_aggregate_bool_valid(facts->noUnknownEscape) ||
        !zr_aggregate_bool_valid(facts->noAddressObservation) ||
        !zr_aggregate_bool_valid(facts->publicLayout) ||
        !zr_aggregate_bool_valid(facts->reflectionVisible) ||
        !zr_aggregate_bool_valid(facts->ffiVisible) ||
        !zr_aggregate_bool_valid(facts->serializationObserved) ||
        !zr_aggregate_bool_valid(facts->aliasProven) ||
        !zr_aggregate_bool_valid(facts->dropOrderProven)) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    for (index = 0u; index < facts->fieldCount; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[index];
        TZrUInt32 end;
        if ((field->flags & ~ZR_EXEC_IR_AGGREGATE_FIELD_KNOWN_MASK) != 0u) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
                              facts, index);
            return ZR_FALSE;
        }
        if (field->logicalIndex != index || field->fieldId == 0u ||
            field->typeToken == 0u || field->layoutId == 0u) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                              facts, index);
            return ZR_FALSE;
        }
        if (field->byteSize == 0u || !zr_aggregate_pow2(field->byteAlign) ||
            field->logicalOffset > UINT32_MAX - field->byteSize) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_INVALID_FIELD_LAYOUT,
                              facts, index);
            return ZR_FALSE;
        }
        end = field->logicalOffset + field->byteSize;
        if (index != 0u && field->logicalOffset < previousOffset) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                              facts, index);
            return ZR_FALSE;
        }
        if ((facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_UNION_OR_OVERLAY) == 0u &&
            (field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_OVERLAY) == 0u &&
            index != 0u && field->logicalOffset < previousEnd) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_FIELD_LAYOUT,
                              facts, index);
            return ZR_FALSE;
        }
        if (field->ownership >= ZR_EXEC_IR_OWNERSHIP_COUNT) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT) != 0u &&
            field->rootSlot == ZR_EXEC_IR_AGGREGATE_INVALID_INDEX) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_ROOT_METADATA_MISSING,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
             field->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED ||
             field->ownership == ZR_EXEC_IR_OWNERSHIP_GC) &&
            !field->ownershipTransferProven) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
             field->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED ||
             field->ownership == ZR_EXEC_IR_OWNERSHIP_GC ||
             (field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED) != 0u) &&
            !facts->dropOrderProven) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_DROP_ORDER_UNSAFE,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED) != 0u &&
            (!field->aliasProven || !facts->aliasProven ||
             field->aliasClass == ZR_EXEC_IR_AGGREGATE_UNKNOWN_ALIAS ||
             field->aliasLocation.unknownWrite || field->aliasLocation.escaped ||
             !field->aliasLocation.hasStableBase ||
             (field->aliasLocation.generation != 0u &&
              field->aliasLocation.generation != facts->generation))) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN,
                              facts, index);
            return ZR_FALSE;
        }
        if (field->readBeforeInit && field->initialized) {
            /* A producer must not claim both an uninitialised read and a
             * fully initialised field; zero-initialisation is represented by
             * initialized=true and readBeforeInit=false. */
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ,
                              facts, index);
            return ZR_FALSE;
        }
        if (field->readBeforeInit && !field->initialized) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ,
                              facts, index);
            return ZR_FALSE;
        }
        previousOffset = field->logicalOffset;
        previousEnd = end;
    }
    for (index = 0u; index < facts->fieldCount; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[index];
        TZrUInt32 next;
        TZrBool orderObservable =
            (TZrBool)(field->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
                      field->ownership != ZR_EXEC_IR_OWNERSHIP_BORROWED) ||
            (TZrBool)((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED) != 0u);
        if (!orderObservable) continue;
        for (next = index + 1u; next < facts->fieldCount; ++next) {
            const SZrExecIrAggregateFieldFact *other = &facts->fields[next];
            TZrBool otherObservable =
                (TZrBool)(other->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN &&
                          other->ownership != ZR_EXEC_IR_OWNERSHIP_BORROWED) ||
                (TZrBool)((other->flags & ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED) != 0u);
            if (otherObservable && field->dropOrder == other->dropOrder) {
                zr_aggregate_diag(diagnostic,
                                  ZR_EXEC_IR_AGGREGATE_DROP_ORDER_UNSAFE,
                                  facts, next);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}
void ZrParser_ExecIr_SroaCandidateInit(SZrExecIrSroaCandidate *candidate) {
    TZrUInt32 index;
    if (candidate == ZR_NULL) return;
    memset(candidate, 0, sizeof(*candidate));
    candidate->magic = ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC;
    candidate->schemaVersion = ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION;
    candidate->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_GENERIC;
    candidate->identityPreserved = ZR_FALSE;
    candidate->requiresMaterialization = ZR_FALSE;
    for (index = 0u; index < ZR_EXEC_IR_AGGREGATE_MAX_FIELDS; ++index) {
        candidate->fieldToScalar[index] = ZR_EXEC_IR_AGGREGATE_INVALID_INDEX;
        candidate->scalarToField[index] = ZR_EXEC_IR_AGGREGATE_INVALID_INDEX;
        candidate->dropOrder[index] = ZR_EXEC_IR_AGGREGATE_INVALID_INDEX;
    }
}

static TZrBool zr_sroa_gate(const SZrExecIrAggregateFacts *facts,
                            SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (!ZrParser_ExecIr_AggregateFactsValidate(facts, diagnostic)) return ZR_FALSE;
    if (!facts->privateClosedWorld || facts->publicLayout ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_LAYOUT) != 0u ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_SLICE) != 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_PUBLIC_LAYOUT,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (facts->reflectionVisible ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_REFLECTION_VISIBLE) != 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_REFLECTION_VISIBLE,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (facts->ffiVisible ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_FFI_VISIBLE) != 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (facts->serializationObserved ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_SERIALIZATION_VISIBLE) != 0u) {
        zr_aggregate_diag(diagnostic,
                          ZR_EXEC_IR_AGGREGATE_SERIALIZATION_VISIBLE,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->noAddressObservation ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_ADDRESS_OBSERVED) != 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->noUnknownEscape ||
        (facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_UNKNOWN_ESCAPE) != 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNKNOWN_ESCAPE,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if ((facts->flags & ZR_EXEC_IR_AGGREGATE_FLAG_UNION_OR_OVERLAY) != 0u) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!facts->reconstructibleIdentity) {
        zr_aggregate_diag(diagnostic,
                          ZR_EXEC_IR_AGGREGATE_IDENTITY_NOT_RECONSTRUCTIBLE,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    for (index = 0u; index < facts->fieldCount; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[index];
        if (field->readBeforeInit) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_ADDRESS_OBSERVED) != 0u ||
            (field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_NATIVE_RETAINED) != 0u) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_FFI_VISIBLE) != 0u) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_OVERLAY) != 0u) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY,
                              facts, index);
            return ZR_FALSE;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_BORROWED_ACROSS_SUSPEND) != 0u) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                              facts, index);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_SroaCanScalarize(
        const SZrExecIrAggregateFacts *facts,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    return zr_sroa_gate(facts, diagnostic);
}

TZrBool ZrParser_ExecIr_SroaBuildCandidate(
        const SZrExecIrAggregateFacts *facts, SZrExecIrSroaCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (candidate == ZR_NULL || facts == ZR_NULL) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    ZrParser_ExecIr_SroaCandidateInit(candidate);
    if (!zr_sroa_gate(facts, diagnostic)) return ZR_FALSE;
    candidate->strategy = ZR_EXEC_IR_AGGREGATE_STRATEGY_SROA;
    candidate->logicalTypeToken = facts->logicalTypeToken;
    candidate->logicalLayoutId = facts->logicalLayoutId;
    candidate->logicalLayoutHash = facts->logicalLayoutHash;
    candidate->generation = facts->generation;
    candidate->bridgeVersion = facts->bridgeVersion;
    candidate->fieldCount = facts->fieldCount;
    candidate->scalarizedFieldCount = facts->fieldCount;
    candidate->unboxedFieldCount = 0u;
    candidate->identityPreserved = ZR_TRUE;
    candidate->requiresMaterialization =
        (TZrBool)((facts->flags & (ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY |
                                   ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY |
                                   ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY)) != 0u);
    for (index = 0u; index < facts->fieldCount; ++index) {
        const SZrExecIrAggregateFieldFact *field = &facts->fields[index];
        TZrUInt32 word = index / 32u;
        TZrUInt32 bit = index % 32u;
        candidate->fieldToScalar[index] = index;
        candidate->scalarToField[index] = index;
        candidate->dropOrder[index] = field->dropOrder;
        if (zr_field_unbox_eligible(field)) {
            candidate->unboxedMask[word] |= (TZrUInt32)1u << bit;
            candidate->unboxedFieldCount++;
        }
        if (field->initialized) candidate->initializedMask[word] |= (TZrUInt32)1u << bit;
        if (field->ownership != ZR_EXEC_IR_OWNERSHIP_BORROWED &&
            field->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN) {
            candidate->ownershipMask[word] |= (TZrUInt32)1u << bit;
        }
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT) != 0u)
            candidate->rootMask[word] |= (TZrUInt32)1u << bit;
        if ((field->flags & ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED) != 0u)
            candidate->dropMask[word] |= (TZrUInt32)1u << bit;
    }
    candidate->planHash = ZrParser_ExecIr_SroaCandidateHash(candidate);
    return ZR_TRUE;
}

TZrUInt64 ZrParser_ExecIr_SroaCandidateHash(
        const SZrExecIrSroaCandidate *candidate) {
    TZrUInt64 hash = ZR_AGGREGATE_FNV_OFFSET;
    TZrUInt32 index;
    if (candidate == ZR_NULL) return 0u;
    zr_aggregate_hash_u32(&hash, (TZrUInt32)candidate->strategy);
    zr_aggregate_hash_u32(&hash, candidate->logicalTypeToken);
    zr_aggregate_hash_u32(&hash, candidate->logicalLayoutId);
    zr_aggregate_hash_u64(&hash, candidate->logicalLayoutHash);
    zr_aggregate_hash_u64(&hash, candidate->generation);
    zr_aggregate_hash_u32(&hash, candidate->bridgeVersion);
    zr_aggregate_hash_u32(&hash, candidate->fieldCount);
    zr_aggregate_hash_u32(&hash, candidate->scalarizedFieldCount);
    zr_aggregate_hash_u32(&hash, candidate->unboxedFieldCount);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)candidate->requiresMaterialization);
    zr_aggregate_hash_u32(&hash, (TZrUInt32)candidate->identityPreserved);
    for (index = 0u; index < candidate->fieldCount &&
                      index < ZR_EXEC_IR_AGGREGATE_MAX_FIELDS; ++index) {
        zr_aggregate_hash_u32(&hash, candidate->fieldToScalar[index]);
        zr_aggregate_hash_u32(&hash, candidate->scalarToField[index]);
        zr_aggregate_hash_u32(&hash, candidate->dropOrder[index]);
    }
    for (index = 0u; index < ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS; ++index) {
        zr_aggregate_hash_u32(&hash, candidate->initializedMask[index]);
        zr_aggregate_hash_u32(&hash, candidate->unboxedMask[index]);
        zr_aggregate_hash_u32(&hash, candidate->ownershipMask[index]);
        zr_aggregate_hash_u32(&hash, candidate->rootMask[index]);
        zr_aggregate_hash_u32(&hash, candidate->dropMask[index]);
    }
    return hash == 0u ? 1u : hash;
}

TZrBool ZrParser_ExecIr_SroaCandidateValidate(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrSroaCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic) {
    TZrUInt64 expectedHash;
    TZrUInt32 index;
    if (diagnostic != ZR_NULL) ZrParser_ExecIr_AggregateDiagnosticInit(diagnostic);
    if (facts == ZR_NULL || candidate == ZR_NULL) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!zr_sroa_gate(facts, diagnostic)) return ZR_FALSE;
    if (candidate->magic != ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ||
        candidate->schemaVersion != ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ||
        candidate->strategy != ZR_EXEC_IR_AGGREGATE_STRATEGY_SROA) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (candidate->fieldCount != facts->fieldCount ||
        candidate->logicalTypeToken != facts->logicalTypeToken ||
        candidate->logicalLayoutId != facts->logicalLayoutId ||
        candidate->logicalLayoutHash != facts->logicalLayoutHash ||
        candidate->generation != facts->generation ||
        candidate->bridgeVersion != facts->bridgeVersion) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_GENERATION_STALE,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (candidate->scalarizedFieldCount != candidate->fieldCount) {
        zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    if (!candidate->identityPreserved) {
        zr_aggregate_diag(diagnostic,
                          ZR_EXEC_IR_AGGREGATE_IDENTITY_NOT_RECONSTRUCTIBLE,
                          facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
        return ZR_FALSE;
    }
    {
        TZrBool expectedMaterialization =
            (TZrBool)((facts->flags & (ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY |
                                       ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY |
                                       ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY)) != 0u);
        if (candidate->requiresMaterialization != expectedMaterialization) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                              facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < candidate->fieldCount; ++index) {
        if (candidate->fieldToScalar[index] != index ||
            candidate->scalarToField[index] != index ||
            candidate->dropOrder[index] != facts->fields[index].dropOrder) {
            zr_aggregate_diag(diagnostic, ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
                              facts, index);
            return ZR_FALSE;
        }
        if (facts->fields[index].initialized &&
            (candidate->initializedMask[index / 32u] &
             ((TZrUInt32)1u << (index % 32u))) == 0u) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ,
                              facts, index);
            return ZR_FALSE;
        }
        {
            TZrUInt32 bit = (TZrUInt32)1u << (index % 32u);
            TZrUInt32 word = index / 32u;
            TZrBool expectedOwnership =
                (TZrBool)(facts->fields[index].ownership !=
                              ZR_EXEC_IR_OWNERSHIP_BORROWED &&
                          facts->fields[index].ownership !=
                              ZR_EXEC_IR_OWNERSHIP_UNKNOWN);
            TZrBool expectedRoot = (TZrBool)(
                (facts->fields[index].flags & ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT) != 0u);
            TZrBool expectedDrop = (TZrBool)(
                (facts->fields[index].flags & ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED) != 0u);
            if ((TZrBool)((candidate->ownershipMask[word] & bit) != 0u) !=
                    expectedOwnership ||
                (TZrBool)((candidate->rootMask[word] & bit) != 0u) != expectedRoot ||
                (TZrBool)((candidate->dropMask[word] & bit) != 0u) != expectedDrop) {
                zr_aggregate_diag(diagnostic,
                                  ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
                                  facts, index);
                return ZR_FALSE;
            }
        }
        {
            TZrBool actualUnbox = (TZrBool)(
                (candidate->unboxedMask[index / 32u] &
                 ((TZrUInt32)1u << (index % 32u))) != 0u);
            if (actualUnbox != zr_field_unbox_eligible(&facts->fields[index])) {
                zr_aggregate_diag(diagnostic,
                                  ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                                  facts, index);
                return ZR_FALSE;
            }
        }
    }
    {
        TZrUInt32 expectedUnboxed = 0u;
        for (index = 0u; index < facts->fieldCount; ++index)
            if (zr_field_unbox_eligible(&facts->fields[index])) expectedUnboxed++;
        if (candidate->unboxedFieldCount != expectedUnboxed) {
            zr_aggregate_diag(diagnostic,
                              ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
                              facts, ZR_EXEC_IR_AGGREGATE_INVALID_INDEX);
            return ZR_FALSE;
        }
    }
    expectedHash = ZrParser_ExecIr_SroaCandidateHash(candidate);
    if (candidate->planHash != expectedHash) {
        zr_aggregate_diag_hash(diagnostic, ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH,
                               expectedHash, candidate->planHash);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
