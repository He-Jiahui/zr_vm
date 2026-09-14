#ifndef ZR_VM_CORE_EXECUTION_FRAME_LAYOUT_H
#define ZR_VM_CORE_EXECUTION_FRAME_LAYOUT_H

/*
 * Runtime-neutral packed-frame contract.
 *
 * The parser owns the liveness analysis that produces a layout.  The core
 * owns only this checked, pointer-free description and the adapters that
 * consume it at a GC/debug/native boundary.  Pointers in these structures are
 * views over caller-owned storage; they are never part of a layout hash or an
 * artifact payload.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/exec_ir.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SZrState;

#define ZR_EXECUTION_FRAME_LAYOUT_MAGIC ((TZrUInt32)0x31464c59u) /* YLF1 */
#define ZR_EXECUTION_FRAME_LAYOUT_SCHEMA_VERSION ((TZrUInt32)1u)

typedef enum EZrExecutionFrameSlotClass {
    ZR_EXECUTION_FRAME_SLOT_BOXED = 0,
    ZR_EXECUTION_FRAME_SLOT_SCALAR,
    ZR_EXECUTION_FRAME_SLOT_INLINE_SPAN,
    ZR_EXECUTION_FRAME_SLOT_REFERENCE,
    ZR_EXECUTION_FRAME_SLOT_CLASS_COUNT
} EZrExecutionFrameSlotClass;

#define ZR_EXECUTION_FRAME_SLOT_FLAG_ADDRESS_ESCAPED ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_FRAME_SLOT_FLAG_MATERIALIZE ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_FRAME_SLOT_FLAG_PARAMETER ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_FRAME_SLOT_FLAG_KNOWN_MASK \
    (ZR_EXECUTION_FRAME_SLOT_FLAG_ADDRESS_ESCAPED | \
     ZR_EXECUTION_FRAME_SLOT_FLAG_MATERIALIZE | \
     ZR_EXECUTION_FRAME_SLOT_FLAG_PARAMETER | \
     ZR_EXECUTION_FRAME_SLOT_FLAG_INITIALIZED)

typedef struct SZrExecutionFrameSlot {
    TZrUInt32 logicalSlot;
    TZrUInt32 physicalSlot;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 liveStart;
    TZrUInt32 liveEnd; /* half-open; equal bounds denote an empty lifetime */
    TZrMetadataToken typeToken;
    EZrExecutionFrameSlotClass slotClass;
    TZrUInt32 flags;
} SZrExecutionFrameSlot;

typedef struct SZrExecutionFrameLayout {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 logicalSlotCount;
    TZrUInt32 storageSlotCount;
    TZrUInt32 parameterPrefixCount;
    TZrUInt32 returnBufferOffset;
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    const SZrExecutionFrameSlot *slots;
    TZrUInt32 slotCount;
    TZrUInt64 layoutHash;
} SZrExecutionFrameLayout;

typedef enum EZrExecutionFrameDiagnosticCode {
    ZR_EXECUTION_FRAME_DIAGNOSTIC_NONE = 0,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_MAGIC,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_UNKNOWN_FLAGS,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_SLOT,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_DUPLICATE_SLOT,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_SLOT_OVERLAP,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_INVALID_ALIGNMENT,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_OVERFLOW,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_LAYOUT_HASH,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_UNMAPPED,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_ROOT_INVALID,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_OUT_OF_MEMORY,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_FRAME_BOUNDS,
    ZR_EXECUTION_FRAME_DIAGNOSTIC_COUNT
} EZrExecutionFrameDiagnosticCode;

typedef struct SZrExecutionFrameDiagnostic {
    EZrExecutionFrameDiagnosticCode code;
    TZrUInt32 index;
    TZrUInt32 relatedIndex;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrExecutionFrameDiagnostic;

/* Name used by the plan's core-facing adapter draft. */
typedef SZrExecutionFrameDiagnostic SZrExecutionDiagnostic;

ZR_CORE_API void ZrCore_ExecutionFrameLayout_Init(
        SZrExecutionFrameLayout *layout);
ZR_CORE_API TZrBool ZrCore_ExecutionFrameLayout_Validate(
        const SZrExecutionFrameLayout *layout,
        SZrExecutionFrameDiagnostic *diagnostic);
ZR_CORE_API TZrUInt64 ZrCore_ExecutionFrameLayout_Hash(
        const SZrExecutionFrameLayout *layout);
ZR_CORE_API TZrBool ZrCore_ExecutionFrameLayout_Finalize(
        SZrExecutionFrameLayout *layout,
        SZrExecutionFrameDiagnostic *diagnostic);
ZR_CORE_API const SZrExecutionFrameSlot *ZrCore_ExecutionFrameLayout_FindLogical(
        const SZrExecutionFrameLayout *layout, TZrUInt32 logicalSlot);

typedef enum EZrExecutionFrameRootKind {
    ZR_EXECUTION_FRAME_ROOT_MANAGED = 0,
    ZR_EXECUTION_FRAME_ROOT_DERIVED,
    ZR_EXECUTION_FRAME_ROOT_INLINE_FIELD,
    ZR_EXECUTION_FRAME_ROOT_KIND_COUNT
} EZrExecutionFrameRootKind;

typedef enum EZrExecutionFrameRootStorage {
    ZR_EXECUTION_FRAME_ROOT_STORAGE_POINTER = 0,
    ZR_EXECUTION_FRAME_ROOT_STORAGE_VALUE_BYTES,
    ZR_EXECUTION_FRAME_ROOT_STORAGE_COUNT
} EZrExecutionFrameRootStorage;

typedef struct SZrExecutionFrameRootSpec {
    TZrUInt32 logicalSlot;
    EZrExecutionFrameRootKind kind;
    EZrExecutionFrameRootStorage storage;
    TZrUInt32 fieldByteOffset;
    TZrUInt32 baseLogicalSlot;
    TZrInt64 derivedOffset;
    TZrBool initialized;
} SZrExecutionFrameRootSpec;

typedef struct SZrExecutionFrameRoot {
    TZrUInt32 logicalSlot;
    TZrUInt32 physicalSlot;
    TZrUInt32 frameByteOffset;
    TZrUInt32 byteSize;
    EZrExecutionFrameRootKind kind;
    EZrExecutionFrameRootStorage storage;
    TZrUInt32 fieldByteOffset;
    TZrUInt32 baseLogicalSlot;
    TZrUInt32 basePhysicalSlot;
    TZrUInt32 baseFrameByteOffset;
    TZrInt64 derivedOffset;
    TZrBool initialized;
} SZrExecutionFrameRoot;

typedef struct SZrExecutionFrameRootMap {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt64 layoutHash;
    const SZrExecutionFrameLayout *layout; /* non-semantic producer view */
    SZrExecutionFrameRoot *roots;
    TZrUInt32 rootCount;
    TZrUInt32 rootCapacity;
} SZrExecutionFrameRootMap;

typedef TZrBool (*FZrExecutionFrameRootVisit)(
        SZrExecutionFrameRoot *root,
        TZrPtr slotAddress,
        TZrPtr baseAddress,
        TZrPtr userData);

typedef struct SZrFrameRootVisitor {
    const SZrExecutionFrameRootMap *rootMap;
    TZrByte *frameBase;
    TZrUInt32 frameByteSize;
    FZrExecutionFrameRootVisit visit;
    TZrPtr userData;
} SZrFrameRootVisitor;

ZR_CORE_API void ZrCore_ExecutionFrameRootMap_Init(
        SZrExecutionFrameRootMap *map);
ZR_CORE_API void ZrCore_ExecutionFrameRootMap_Free(
        SZrExecutionFrameRootMap *map);
ZR_CORE_API TZrBool ZrCore_ExecutionFrameRootMap_Build(
        const SZrExecutionFrameLayout *layout,
        const SZrExecutionFrameRootSpec *specs,
        TZrUInt32 specCount,
        SZrExecutionFrameRootMap *map,
        SZrExecutionFrameDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecutionFrameRootMap_Validate(
        const SZrExecutionFrameLayout *layout,
        const SZrExecutionFrameRootMap *map,
        SZrExecutionFrameDiagnostic *diagnostic);

/* The state argument is optional.  When supplied, an adapter may use it to
 * apply its own stack-bound checks; the map/frame contract remains usable by
 * AOT and JIT callers that do not expose a core state object. */
ZR_CORE_API TZrBool ZrCore_Execution_VisitFrameRoots(
        struct SZrState *state,
        const SZrFrameRootVisitor *visitor,
        SZrExecutionDiagnostic *diagnostic);

#define ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_FRAME_OBSERVE_KNOWN_MASK \
    (ZR_EXECUTION_FRAME_OBSERVE_WRITE_SCALARS | \
     ZR_EXECUTION_FRAME_OBSERVE_INVALIDATE)

typedef struct SZrFrameObservationRequest {
    const SZrExecutionFrameLayout *layout;
    TZrByte *frameBase;
    TZrUInt32 frameByteSize;
    const TZrUInt64 *scalarValues;
    TZrUInt32 scalarValueCount;
    TZrUInt64 *writebackValues;
    TZrUInt32 writebackCapacity;
    TZrUInt32 *invalidatedPhysicalSlots;
    TZrUInt32 invalidatedCapacity;
    TZrUInt32 *invalidatedCount;
    TZrUInt32 flags;
} SZrFrameObservationRequest;

ZR_CORE_API TZrBool ZrCore_Execution_ObserveFrame(
        const SZrFrameObservationRequest *request,
        SZrExecutionDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_EXECUTION_FRAME_LAYOUT_H */
