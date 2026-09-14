#ifndef ZR_VM_PARSER_EXEC_IR_AGGREGATE_LAYOUT_H
#define ZR_VM_PARSER_EXEC_IR_AGGREGATE_LAYOUT_H

#include <limits.h>
#include <stdint.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/exec_ir_alias.h"
#include "zr_vm_parser/conf.h"

/*
 * Aggregate layout facts are a parser/backend contract.  The facts and all
 * plans below contain only integer identities and fixed-size arrays; no plan
 * retains an AST, runtime object, or host address.  A producer may discard a
 * plan at any safepoint and rebuild it from the logical facts.
 */
#define ZR_EXEC_IR_AGGREGATE_LAYOUT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_IR_AGGREGATE_LAYOUT_MAGIC ((TZrUInt32)0x41474c31u)
#define ZR_EXEC_IR_AGGREGATE_MAX_FIELDS ((TZrUInt32)64u)
#define ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS \
    ((ZR_EXEC_IR_AGGREGATE_MAX_FIELDS + 31u) / 32u)
#define ZR_EXEC_IR_AGGREGATE_INVALID_INDEX ((TZrUInt32)UINT32_MAX)
#define ZR_EXEC_IR_AGGREGATE_UNKNOWN_MEASUREMENT ((TZrUInt64)UINT64_MAX)
#define ZR_EXEC_IR_AGGREGATE_UNKNOWN_ALIAS ((TZrUInt64)0u)

typedef enum EZrExecIrAggregateStatus {
    ZR_EXEC_IR_AGGREGATE_OK = 0,
    ZR_EXEC_IR_AGGREGATE_INVALID_ARGUMENT,
    ZR_EXEC_IR_AGGREGATE_SCHEMA_MISMATCH,
    ZR_EXEC_IR_AGGREGATE_FIELD_LIMIT,
    ZR_EXEC_IR_AGGREGATE_FIELD_ORDER,
    ZR_EXEC_IR_AGGREGATE_INVALID_FIELD_LAYOUT,
    ZR_EXEC_IR_AGGREGATE_UNKNOWN_FLAGS,
    ZR_EXEC_IR_AGGREGATE_UNION_OVERLAY,
    ZR_EXEC_IR_AGGREGATE_UNINITIALIZED_READ,
    ZR_EXEC_IR_AGGREGATE_ADDRESS_OBSERVED,
    ZR_EXEC_IR_AGGREGATE_UNKNOWN_ESCAPE,
    ZR_EXEC_IR_AGGREGATE_PUBLIC_LAYOUT,
    ZR_EXEC_IR_AGGREGATE_REFLECTION_VISIBLE,
    ZR_EXEC_IR_AGGREGATE_FFI_VISIBLE,
    ZR_EXEC_IR_AGGREGATE_SERIALIZATION_VISIBLE,
    ZR_EXEC_IR_AGGREGATE_IDENTITY_NOT_RECONSTRUCTIBLE,
    ZR_EXEC_IR_AGGREGATE_OWNERSHIP_UNSAFE,
    ZR_EXEC_IR_AGGREGATE_ROOT_METADATA_MISSING,
    ZR_EXEC_IR_AGGREGATE_DROP_ORDER_UNSAFE,
    ZR_EXEC_IR_AGGREGATE_ALIAS_UNPROVEN,
    ZR_EXEC_IR_AGGREGATE_LIFETIME_OPEN,
    ZR_EXEC_IR_AGGREGATE_PROFILE_MISSING,
    ZR_EXEC_IR_AGGREGATE_COST_UNPROFITABLE,
    ZR_EXEC_IR_AGGREGATE_BRIDGE_TOO_EXPENSIVE,
    ZR_EXEC_IR_AGGREGATE_HASH_MISMATCH,
    ZR_EXEC_IR_AGGREGATE_GENERATION_STALE,
    ZR_EXEC_IR_AGGREGATE_IDENTITY_MISMATCH,
    ZR_EXEC_IR_AGGREGATE_PHYSICAL_LAYOUT_INVALID,
    ZR_EXEC_IR_AGGREGATE_MATERIALIZATION_REQUIRED,
    ZR_EXEC_IR_AGGREGATE_UNSUPPORTED,
    ZR_EXEC_IR_AGGREGATE_STATUS_COUNT
} EZrExecIrAggregateStatus;

typedef enum EZrExecIrAggregateStrategy {
    ZR_EXEC_IR_AGGREGATE_STRATEGY_GENERIC = 0,
    ZR_EXEC_IR_AGGREGATE_STRATEGY_SROA,
    ZR_EXEC_IR_AGGREGATE_STRATEGY_AOS,
    ZR_EXEC_IR_AGGREGATE_STRATEGY_SOA,
    ZR_EXEC_IR_AGGREGATE_STRATEGY_BOXED
} EZrExecIrAggregateStrategy;

/* Cost evidence is intentionally explicit: estimated locality is never
 * presented as a PMU/cache-miss measurement. */
typedef enum EZrExecIrAggregateEvidence {
    ZR_EXEC_IR_AGGREGATE_EVIDENCE_ESTIMATED = 0,
    ZR_EXEC_IR_AGGREGATE_EVIDENCE_MEASURED
} EZrExecIrAggregateEvidence;

typedef enum EZrExecIrAggregateFlags {
    ZR_EXEC_IR_AGGREGATE_FLAG_UNION_OR_OVERLAY = (TZrUInt32)1u << 0u,
    ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_LAYOUT = (TZrUInt32)1u << 1u,
    ZR_EXEC_IR_AGGREGATE_FLAG_REFLECTION_VISIBLE = (TZrUInt32)1u << 2u,
    ZR_EXEC_IR_AGGREGATE_FLAG_FFI_VISIBLE = (TZrUInt32)1u << 3u,
    ZR_EXEC_IR_AGGREGATE_FLAG_ADDRESS_OBSERVED = (TZrUInt32)1u << 4u,
    ZR_EXEC_IR_AGGREGATE_FLAG_SERIALIZATION_VISIBLE = (TZrUInt32)1u << 5u,
    ZR_EXEC_IR_AGGREGATE_FLAG_UNKNOWN_ESCAPE = (TZrUInt32)1u << 6u,
    ZR_EXEC_IR_AGGREGATE_FLAG_IDENTITY_OBSERVED = (TZrUInt32)1u << 7u,
    ZR_EXEC_IR_AGGREGATE_FLAG_ALIAS_OBSERVED = (TZrUInt32)1u << 8u,
    ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY = (TZrUInt32)1u << 9u,
    ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY = (TZrUInt32)1u << 10u,
    ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY = (TZrUInt32)1u << 11u,
    ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_SLICE = (TZrUInt32)1u << 12u,
    ZR_EXEC_IR_AGGREGATE_FLAG_PROFILE_AVAILABLE = (TZrUInt32)1u << 13u,
    ZR_EXEC_IR_AGGREGATE_FLAG_DROP_ORDER_OBSERVED = (TZrUInt32)1u << 14u,
    /* Debug materialisation is a logical-layout boundary just like deopt,
     * native, and exception hand-off.  Keep this bit append-only so the
     * existing schema values remain stable for producers that already emit
     * the lower bits. */
    ZR_EXEC_IR_AGGREGATE_FLAG_DEBUG_BOUNDARY = (TZrUInt32)1u << 15u
} EZrExecIrAggregateFlags;

#define ZR_EXEC_IR_AGGREGATE_FLAG_KNOWN_MASK \
    ((TZrUInt32)(ZR_EXEC_IR_AGGREGATE_FLAG_UNION_OR_OVERLAY | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_LAYOUT | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_REFLECTION_VISIBLE | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_FFI_VISIBLE | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_ADDRESS_OBSERVED | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_SERIALIZATION_VISIBLE | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_UNKNOWN_ESCAPE | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_IDENTITY_OBSERVED | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_ALIAS_OBSERVED | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_PUBLIC_SLICE | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_PROFILE_AVAILABLE | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_DROP_ORDER_OBSERVED | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_DEBUG_BOUNDARY))

#define ZR_EXEC_IR_AGGREGATE_FLAG_MATERIALIZATION_BOUNDARY_MASK \
    ((TZrUInt32)(ZR_EXEC_IR_AGGREGATE_FLAG_DEBUG_BOUNDARY | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_DEOPT_BOUNDARY | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_NATIVE_BOUNDARY | \
                 ZR_EXEC_IR_AGGREGATE_FLAG_EXCEPTION_BOUNDARY))

typedef enum EZrExecIrAggregateFieldFlags {
    ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT = (TZrUInt32)1u << 0u,
    ZR_EXEC_IR_AGGREGATE_FIELD_OWNERSHIP = (TZrUInt32)1u << 1u,
    ZR_EXEC_IR_AGGREGATE_FIELD_REF = (TZrUInt32)1u << 2u,
    ZR_EXEC_IR_AGGREGATE_FIELD_ADDRESS_OBSERVED = (TZrUInt32)1u << 3u,
    ZR_EXEC_IR_AGGREGATE_FIELD_FFI_VISIBLE = (TZrUInt32)1u << 4u,
    ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED = (TZrUInt32)1u << 5u,
    ZR_EXEC_IR_AGGREGATE_FIELD_IDENTITY_BEARING = (TZrUInt32)1u << 6u,
    ZR_EXEC_IR_AGGREGATE_FIELD_OVERLAY = (TZrUInt32)1u << 7u,
    ZR_EXEC_IR_AGGREGATE_FIELD_NESTED_LAYOUT = (TZrUInt32)1u << 8u,
    ZR_EXEC_IR_AGGREGATE_FIELD_BORROWED_ACROSS_SUSPEND = (TZrUInt32)1u << 9u,
    ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED = (TZrUInt32)1u << 10u,
    ZR_EXEC_IR_AGGREGATE_FIELD_NATIVE_RETAINED = (TZrUInt32)1u << 11u
} EZrExecIrAggregateFieldFlags;

#define ZR_EXEC_IR_AGGREGATE_FIELD_KNOWN_MASK \
    ((TZrUInt32)(ZR_EXEC_IR_AGGREGATE_FIELD_GC_ROOT | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_OWNERSHIP | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_REF | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_ADDRESS_OBSERVED | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_FFI_VISIBLE | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_DROP_OBSERVED | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_IDENTITY_BEARING | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_OVERLAY | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_NESTED_LAYOUT | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_BORROWED_ACROSS_SUSPEND | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_ALIAS_OBSERVED | \
                 ZR_EXEC_IR_AGGREGATE_FIELD_NATIVE_RETAINED))

typedef struct SZrExecIrAggregateDiagnostic {
    EZrExecIrAggregateStatus status;
    SZrExecIrDiagnostic execution;
    TZrUInt32 fieldIndex;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrExecIrAggregateDiagnostic;

typedef struct SZrExecIrAggregateFieldFact {
    TZrUInt32 fieldId;
    TZrUInt32 logicalIndex;
    TZrExecIrTypeToken typeToken;
    TZrUInt32 layoutId;
    TZrUInt32 logicalOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 useCount;
    TZrUInt32 loopUseCount;
    TZrUInt32 dropOrder;
    TZrUInt32 rootSlot;
    TZrUInt64 fieldHash;
    TZrUInt64 aliasClass;
    EZrExecIrOwnership ownership;
    TZrUInt32 flags;
    TZrBool initialized;
    TZrBool readBeforeInit;
    TZrBool ownershipTransferProven;
    TZrBool aliasProven;
    SZrExecIrAliasLocation aliasLocation;
} SZrExecIrAggregateFieldFact;

typedef struct SZrExecIrAggregateFacts {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrExecIrTypeToken logicalTypeToken;
    TZrUInt32 logicalLayoutId;
    TZrUInt64 logicalLayoutHash;
    TZrUInt64 generation;
    TZrUInt32 sourceId;
    TZrUInt32 functionToken;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 flags;
    TZrUInt32 fieldCount;
    TZrUInt64 identityToken;
    TZrUInt32 bridgeVersion;
    TZrBool privateClosedWorld;
    TZrBool closedLifetime;
    TZrBool reconstructibleIdentity;
    TZrBool noUnknownEscape;
    TZrBool noAddressObservation;
    TZrBool publicLayout;
    TZrBool reflectionVisible;
    TZrBool ffiVisible;
    TZrBool serializationObserved;
    TZrBool aliasProven;
    TZrBool dropOrderProven;
    SZrExecIrAggregateFieldFact fields[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
} SZrExecIrAggregateFacts;

typedef struct SZrExecIrSroaCandidate {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrExecIrAggregateStrategy strategy;
    TZrExecIrTypeToken logicalTypeToken;
    TZrUInt32 logicalLayoutId;
    TZrUInt64 logicalLayoutHash;
    TZrUInt64 generation;
    TZrUInt32 bridgeVersion;
    TZrUInt32 fieldCount;
    TZrUInt32 scalarizedFieldCount;
    TZrUInt32 unboxedFieldCount;
    TZrUInt32 initializedMask[ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS];
    TZrUInt32 unboxedMask[ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS];
    TZrUInt32 ownershipMask[ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS];
    TZrUInt32 rootMask[ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS];
    TZrUInt32 dropMask[ZR_EXEC_IR_AGGREGATE_MAX_MASK_WORDS];
    TZrUInt32 fieldToScalar[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    TZrUInt32 scalarToField[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    TZrUInt32 dropOrder[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    TZrBool requiresMaterialization;
    TZrBool identityPreserved;
    TZrUInt64 planHash;
} SZrExecIrSroaCandidate;

typedef struct SZrExecIrDataLayoutCost {
    TZrUInt64 elementCount;
    TZrUInt64 loopTripCount;
    TZrUInt64 estimatedMoveBytes;
    TZrUInt64 estimatedBridgeCost;
    TZrUInt64 estimatedLocalityBenefit;
    TZrUInt64 measuredCacheMissesBefore;
    TZrUInt64 measuredCacheMissesAfter;
    EZrExecIrAggregateEvidence evidence;
    TZrBool profileAvailable;
} SZrExecIrDataLayoutCost;

typedef struct SZrExecIrDataLayoutCandidate {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrExecIrAggregateStrategy strategy;
    EZrExecIrAggregateStatus fallbackReason;
    TZrExecIrTypeToken logicalTypeToken;
    TZrUInt32 logicalLayoutId;
    TZrUInt64 logicalLayoutHash;
    TZrUInt64 physicalLayoutHash;
    TZrUInt64 generation;
    TZrUInt32 bridgeVersion;
    TZrUInt32 fieldCount;
    TZrUInt32 fieldOrder[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    TZrUInt32 columnStride[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    TZrUInt32 columnOffset[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    TZrUInt64 estimatedMoveBytes;
    TZrUInt64 estimatedBridgeCost;
    TZrUInt64 estimatedLocalityBenefit;
    TZrUInt64 measuredCacheMissesBefore;
    TZrUInt64 measuredCacheMissesAfter;
    EZrExecIrAggregateEvidence evidence;
    TZrBool closedLifetime;
    TZrBool ownershipMigrated;
    TZrBool rootsMigrated;
    TZrBool dropOrderPreserved;
    TZrBool requiresMaterialization;
    TZrUInt64 planHash;
} SZrExecIrDataLayoutCandidate;

typedef struct SZrExecIrMaterializationEntry {
    TZrUInt32 logicalIndex;
    TZrUInt32 physicalIndex;
    TZrUInt32 physicalOffset;
    TZrUInt32 byteSize;
    TZrExecIrTypeToken typeToken;
    TZrUInt32 layoutId;
    TZrUInt32 rootSlot;
    TZrUInt32 dropOrder;
    TZrUInt64 aliasClass;
    EZrExecIrOwnership ownership;
    TZrUInt32 flags;
    TZrBool initialized;
} SZrExecIrMaterializationEntry;

typedef struct SZrExecIrMaterializationMap {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrExecIrTypeToken logicalTypeToken;
    TZrUInt32 logicalLayoutId;
    TZrUInt64 logicalLayoutHash;
    TZrUInt64 physicalLayoutHash;
    TZrUInt64 generation;
    TZrUInt32 bridgeVersion;
    TZrUInt64 identityToken;
    TZrUInt32 fieldCount;
    TZrUInt32 entryCount;
    TZrUInt32 flags;
    TZrBool identityPreserved;
    TZrBool finalized;
    SZrExecIrMaterializationEntry entries[ZR_EXEC_IR_AGGREGATE_MAX_FIELDS];
    TZrUInt64 mapHash;
} SZrExecIrMaterializationMap;

typedef struct SZrExecIrLayoutTransformPlan {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrExecIrAggregateStrategy strategy;
    EZrExecIrAggregateStatus fallbackReason;
    TZrExecIrTypeToken logicalTypeToken;
    TZrUInt32 logicalLayoutId;
    TZrUInt64 logicalLayoutHash;
    TZrUInt64 physicalLayoutHash;
    TZrUInt64 generation;
    TZrUInt32 bridgeVersion;
    TZrUInt32 fieldCount;
    TZrUInt32 flags;
    TZrUInt64 planHash;
    TZrBool requiresMaterialization;
    TZrBool ownershipMigrated;
    TZrBool rootsMigrated;
    TZrBool dropOrderPreserved;
} SZrExecIrLayoutTransformPlan;

ZR_PARSER_API void ZrParser_ExecIr_AggregateDiagnosticInit(
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_AggregateStatusName(
        EZrExecIrAggregateStatus status);

ZR_PARSER_API void ZrParser_ExecIr_AggregateFactsInit(
        SZrExecIrAggregateFacts *facts);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AggregateFactsValidate(
        const SZrExecIrAggregateFacts *facts,
        SZrExecIrAggregateDiagnostic *diagnostic);
/* Structural validation accepts conservative/unknown semantic proofs so an
 * ordinary AoS or generic fallback can still be described.  Optimisation
 * passes must use the full validator above (or their semantic gate) before
 * applying SROA/SoA. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_AggregateFactsValidateStructural(
        const SZrExecIrAggregateFacts *facts,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_AggregateFactsHash(
        const SZrExecIrAggregateFacts *facts);

ZR_PARSER_API void ZrParser_ExecIr_SroaCandidateInit(
        SZrExecIrSroaCandidate *candidate);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SroaCanScalarize(
        const SZrExecIrAggregateFacts *facts,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SroaBuildCandidate(
        const SZrExecIrAggregateFacts *facts, SZrExecIrSroaCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_SroaCandidateValidate(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrSroaCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_SroaCandidateHash(
        const SZrExecIrSroaCandidate *candidate);

ZR_PARSER_API void ZrParser_ExecIr_DataLayoutCostInit(
        SZrExecIrDataLayoutCost *cost);
ZR_PARSER_API void ZrParser_ExecIr_DataLayoutCandidateInit(
        SZrExecIrDataLayoutCandidate *candidate);
ZR_PARSER_API TZrBool ZrParser_ExecIr_DataLayoutCanUseSoA(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCost *cost,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_DataLayoutBuildCandidate(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCost *cost,
        SZrExecIrDataLayoutCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_DataLayoutCandidateValidate(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_DataLayoutCandidateHash(
        const SZrExecIrDataLayoutCandidate *candidate);

ZR_PARSER_API void ZrParser_ExecIr_MaterializationMapInit(
        SZrExecIrMaterializationMap *map);
ZR_PARSER_API TZrBool ZrParser_ExecIr_MaterializationMapBegin(
        SZrExecIrMaterializationMap *map,
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCandidate *candidate,
        TZrUInt64 identityToken,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_MaterializationMapAddField(
        SZrExecIrMaterializationMap *map,
        const SZrExecIrAggregateFieldFact *field,
        TZrUInt32 physicalIndex,
        TZrUInt32 physicalOffset,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_MaterializationMapFinalize(
        SZrExecIrMaterializationMap *map,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_MaterializationMapValidate(
        const SZrExecIrMaterializationMap *map,
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCandidate *candidate,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_MaterializationMapResolve(
        const SZrExecIrMaterializationMap *map,
        TZrUInt32 logicalIndex,
        SZrExecIrMaterializationEntry *entry,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_MaterializationMapHash(
        const SZrExecIrMaterializationMap *map);

ZR_PARSER_API TZrBool ZrParser_ExecIr_PlanAggregateLayout(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrDataLayoutCost *cost,
        SZrExecIrLayoutTransformPlan *plan,
        SZrExecIrAggregateDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ApplyAggregateLayout(
        const SZrExecIrAggregateFacts *facts,
        const SZrExecIrLayoutTransformPlan *plan,
        SZrExecIrAggregateDiagnostic *diagnostic);

/* Compatibility spellings used by pass prototypes and older plan notes. */
#define ZrParser_ExecIr_AggregateFacts_Init ZrParser_ExecIr_AggregateFactsInit
#define ZrParser_ExecIr_AggregateFacts_Validate ZrParser_ExecIr_AggregateFactsValidate
#define ZrParser_ExecIr_AggregateFacts_ValidateStructural \
    ZrParser_ExecIr_AggregateFactsValidateStructural
#define ZrParser_ExecIr_AggregateFacts_StructuralValidate \
    ZrParser_ExecIr_AggregateFactsValidateStructural
#define ZrParser_ExecIr_SroaCanUse ZrParser_ExecIr_SroaCanScalarize
#define ZrParser_ExecIr_SROACanScalarize ZrParser_ExecIr_SroaCanScalarize
#define ZrParser_ExecIr_SROABuildCandidate ZrParser_ExecIr_SroaBuildCandidate
#define ZrParser_ExecIr_UnboxCanUse ZrParser_ExecIr_SroaCanScalarize
#define ZrParser_ExecIr_UnboxBuildCandidate ZrParser_ExecIr_SroaBuildCandidate
#define ZrParser_ExecIr_SoACanUse ZrParser_ExecIr_DataLayoutCanUseSoA
#define ZrParser_ExecIr_SoaCanUse ZrParser_ExecIr_DataLayoutCanUseSoA
#define ZrParser_ExecIr_SoABuildCandidate ZrParser_ExecIr_DataLayoutBuildCandidate
#define ZrParser_ExecIr_MaterializationMap_Init \
    ZrParser_ExecIr_MaterializationMapInit
#define ZrParser_ExecIr_MaterializationMap_Validate \
    ZrParser_ExecIr_MaterializationMapValidate

#endif /* ZR_VM_PARSER_EXEC_IR_AGGREGATE_LAYOUT_H */
