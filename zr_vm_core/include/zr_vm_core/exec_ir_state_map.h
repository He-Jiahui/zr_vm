#ifndef ZR_VM_CORE_EXEC_IR_STATE_MAP_H
#define ZR_VM_CORE_EXEC_IR_STATE_MAP_H

#include "zr_vm_core/exec_ir.h"

/* A logical checkpoint is independent of any physical frame/register slot. */
typedef enum EZrExecIrStateMapPhase {
    ZR_EXEC_IR_STATE_BEFORE_EFFECT = 0,
    ZR_EXEC_IR_STATE_AFTER_EFFECT,
    ZR_EXEC_IR_STATE_CLEANUP_COMPLETE,
    ZR_EXEC_IR_STATE_PHASE_COUNT
} EZrExecIrStateMapPhase;

#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE ((TZrUInt32)1u << 4u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL ((TZrUInt32)1u << 5u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT ((TZrUInt32)1u << 6u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP ((TZrUInt32)1u << 7u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_KNOWN_MASK \
    (ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC | ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW | \
     ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND | ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT | \
     ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE | ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL | \
     ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT | ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP)
#define ZR_EXEC_IR_STATE_MAP_EXCEPTION_MASK \
    (ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW | ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND)

/* Ownership state is recorded by value ID, never by a host address. */
typedef enum EZrExecIrStateMapOwnerState {
    ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN = 0,
    ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED,
    ZR_EXEC_IR_STATE_MAP_OWNER_MOVED,
    ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED,
    ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT
} EZrExecIrStateMapOwnerState;

typedef struct SZrExecIrStateMapEntry {
    TZrExecIrSourceId sourceId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrDeoptId deoptId;
    TZrUInt32 resumeId;
    TZrUInt32 cleanupState;
    TZrUInt32 boundaryFlags;
    EZrExecIrStateMapPhase phase;
    SZrExecIrRange liveValues;
    SZrExecIrRange rootValues;
    /* One owner-state entry corresponds to each liveValues entry. */
    SZrExecIrRange ownerStates;
    TZrExecIrEffectTokenId effectIn;
    TZrExecIrEffectTokenId effectOut;
    TZrExecIrBlockId handlerBlockId;
    TZrUInt32 exceptionState;
} SZrExecIrStateMapEntry;

/* Function-owned logical state-map side table. All references are IDs/ranges. */
struct SZrExecIrStateMap {
    /* Stable identity prevents a map from one function being resumed in
     * another function that happens to share a generation number. */
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    SZrExecIrStateMapEntry *entries;
    TZrUInt32 entryCount;
    TZrUInt32 entryCapacity;
    TZrExecIrValueId *valuePool;
    TZrUInt32 valueCount;
    TZrUInt32 valueCapacity;
    TZrExecIrValueId *rootPool;
    TZrUInt32 rootCount;
    TZrUInt32 rootCapacity;
    TZrUInt32 *ownerStatePool;
    TZrUInt32 ownerStateCount;
    TZrUInt32 ownerStateCapacity;
    TZrUInt64 generation;
};

/* The target is caller-owned and is changed only after all preparation passes. */
typedef struct SZrExecIrMaterializedState {
    TZrExecIrSourceId sourceId;
    TZrUInt32 resumeId;
    TZrUInt32 cleanupState;
    EZrExecIrStateMapPhase phase;
    TZrUInt64 generation;
    TZrExecIrValueId *values;
    TZrUInt32 valueCount;
    TZrUInt32 valueCapacity;
    TZrExecIrValueId *roots;
    TZrUInt32 rootCount;
    TZrUInt32 rootCapacity;
    TZrUInt32 *ownerStates;
    TZrUInt32 ownerStateCount;
    TZrUInt32 ownerStateCapacity;
    TZrExecIrEffectTokenId effectIn;
    TZrExecIrEffectTokenId effectOut;
    TZrExecIrBlockId handlerBlockId;
    TZrUInt32 exceptionState;
    TZrUInt32 boundaryFlags;
    TZrExecIrDeoptId deoptId;
    SZrExecIrDeoptAggregate *aggregates;
    TZrUInt32 aggregateCount;
    TZrUInt32 aggregateCapacity;
    SZrExecIrDeoptAggregateField *aggregateFields;
    TZrUInt32 aggregateFieldCount;
    TZrUInt32 aggregateFieldCapacity;
} SZrExecIrMaterializedState;

typedef struct SZrExecIrResumeRequest {
    const SZrExecIrFunction *function;
    const SZrExecIrStateMap *map;
    TZrMetadataToken functionToken;
    TZrUInt64 generation;
    /* Optional extra identity check; zero means use the map/function pair. */
    TZrUInt64 signatureHash;
    TZrExecIrSourceId sourceId;
    TZrUInt32 resumeId;
    EZrExecIrStateMapPhase phase;
    SZrExecIrMaterializedState *target;
} SZrExecIrResumeRequest;

ZR_CORE_API void ZrCore_ExecIr_StateMapInit(SZrExecIrStateMap *map);
ZR_CORE_API void ZrCore_ExecIr_StateMapFree(SZrExecIrStateMap *map);
ZR_CORE_API TZrBool ZrCore_ExecIr_StateMapClone(const SZrExecIrStateMap *source,
                                                SZrExecIrStateMap *destination);
ZR_CORE_API TZrBool ZrCore_ExecIr_StateMapBoundaryFlags(
        const SZrExecIrInstruction *instruction,
        TZrUInt32 *flags);
ZR_CORE_API const SZrExecIrStateMapEntry *ZrCore_ExecIr_StateMapFind(
        const SZrExecIrStateMap *map,
        TZrExecIrSourceId sourceId,
        TZrUInt32 resumeId,
        EZrExecIrStateMapPhase phase);
ZR_CORE_API void ZrCore_ExecIr_MaterializedStateInit(SZrExecIrMaterializedState *state);
ZR_CORE_API void ZrCore_ExecIr_MaterializedStateFree(SZrExecIrMaterializedState *state);
ZR_CORE_API TZrBool ZrCore_ExecIr_MaterializeState(
        const SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic);

#endif
