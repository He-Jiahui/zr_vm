#ifndef ZR_VM_PARSER_EXEC_IR_FRAME_ROOTS_H
#define ZR_VM_PARSER_EXEC_IR_FRAME_ROOTS_H

#include "exec_ir/exec_ir_frame_layout.h"

typedef enum EZrExecIrFrameRootKind {
    ZR_EXEC_IR_FRAME_ROOT_MANAGED = 0,
    ZR_EXEC_IR_FRAME_ROOT_DERIVED,
    ZR_EXEC_IR_FRAME_ROOT_INLINE_FIELD
} EZrExecIrFrameRootKind;

typedef struct SZrExecIrFrameRootSpec {
    TZrExecIrValueId valueId;
    EZrExecIrFrameRootKind kind;
    TZrUInt32 fieldByteOffset;
    TZrExecIrValueId baseValueId; /* required for DERIVED */
    TZrInt64 derivedOffset;
    TZrBool initialized;
} SZrExecIrFrameRootSpec;

typedef struct SZrExecIrFrameRoot {
    TZrExecIrValueId valueId;
    TZrUInt32 physicalSlot;
    TZrUInt32 frameByteOffset;
    EZrExecIrFrameRootKind kind;
    TZrUInt32 fieldByteOffset;
    TZrUInt32 basePhysicalSlot;
    TZrUInt32 baseFrameByteOffset;
    TZrInt64 derivedOffset;
    TZrBool initialized;
} SZrExecIrFrameRoot;

typedef struct SZrExecIrFrameRootMap {
    SZrExecIrFrameRoot *roots;
    TZrUInt32 rootCount;
    TZrUInt32 rootCapacity;
} SZrExecIrFrameRootMap;

typedef TZrBool (*FZrExecIrFrameRootVisitor)(SZrExecIrFrameRoot *root,
                                              TZrPtr slotAddress,
                                              TZrPtr baseAddress,
                                              TZrPtr userData);

typedef struct SZrExecIrFrameObservation {
    const SZrExecIrPackedFrameLayout *layout;
    TZrByte *frameBase;
    const TZrUInt64 *scalarValues;
    TZrUInt32 scalarValueCount;
    TZrUInt64 *writebackValues;
    TZrUInt32 writebackCapacity;
    TZrUInt32 *invalidatedPhysicalSlots;
    TZrUInt32 invalidatedCapacity;
    TZrUInt32 invalidatedCount;
} SZrExecIrFrameObservation;

ZR_PARSER_API void ZrParser_ExecIr_FrameRootMapInit(SZrExecIrFrameRootMap *map);
ZR_PARSER_API void ZrParser_ExecIr_FrameRootMapFree(SZrExecIrFrameRootMap *map);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildFrameRootMap(
        const SZrExecIrPackedFrameLayout *layout,
        const SZrExecIrFrameRootSpec *specs, TZrUInt32 specCount,
        SZrExecIrFrameRootMap *map, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_VisitFrameRoots(
        const SZrExecIrFrameRootMap *map, TZrByte *frameBase,
        FZrExecIrFrameRootVisitor visitor, TZrPtr userData,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ObserveFrame(
        SZrExecIrFrameObservation *observation,
        SZrExecIrDiagnostic *diagnostic);

/* Explicit active ValueIds in borrowed live storage; UInt64 physical-byte mirrors.
 * The caller certifies initialized/live selections, typed readable lengths and
 * serialized ownership. Numeric geometry does not authorize pointer access.
 * Selected SCALAR/VALUE physical widths are 1..8; logical type/width recovery,
 * REF/BOXED/INLINE observation and GC/backend integration remain unsupported.
 * NULL scalarValues/count0 observes without writing frame; nonnull mirrors cover
 * all logical indices. Empty selection writes counters0 only. Selected outputs
 * are disjoint from all readonly inputs and each other; diagnostic is independent.
 * Failure preserves frame/result/writeback/invalidation; diagnostic may change. */
typedef struct SZrExecIrActiveFrameObservation {
    const SZrExecIrPackedFrameLayout *layout;
    TZrByte *frameBase;
    size_t frameCapacityBytes;
    const TZrExecIrValueId *activeValueIds;
    TZrUInt32 activeValueCount;
    const TZrUInt64 *scalarValues;
    TZrUInt32 scalarValueCount;
    TZrUInt64 *writebackValues;
    TZrUInt32 writebackCapacity;
    TZrUInt32 *invalidatedPhysicalSlots;
    TZrUInt32 invalidatedCapacity;
} SZrExecIrActiveFrameObservation;
typedef struct SZrExecIrActiveFrameObservationResult {
    TZrUInt32 observedValueCount;
    TZrUInt32 invalidatedCount;
} SZrExecIrActiveFrameObservationResult;
ZR_PARSER_API TZrBool ZrParser_ExecIr_ObserveActiveFrame(
        const SZrExecIrActiveFrameObservation *request,
        SZrExecIrActiveFrameObservationResult *result,
        SZrExecIrDiagnostic *diagnostic);

#endif
