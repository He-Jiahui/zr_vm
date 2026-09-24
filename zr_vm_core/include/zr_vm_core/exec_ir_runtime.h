#ifndef ZR_VM_CORE_EXEC_IR_RUNTIME_H
#define ZR_VM_CORE_EXEC_IR_RUNTIME_H

#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_core/object.h"

/* Backend/runtime projection of logical fields onto existing prototype data.
 * These pointers are runtime-only and never belong in serialized ExecIR. */
typedef struct SZrExecIrRuntimeFieldBinding {
    TZrUInt32 fieldIndex;
    TZrUInt32 memberDescriptorIndex;
} SZrExecIrRuntimeFieldBinding;

typedef struct SZrExecIrRuntimeTypeBinding {
    TZrExecIrTypeToken typeToken;
    TZrUInt32 layoutId;
    SZrObjectPrototype *prototype;
    TZrUInt64 layoutGeneration;
    const SZrExecIrRuntimeFieldBinding *fields;
    TZrUInt32 fieldCount;
} SZrExecIrRuntimeTypeBinding;

typedef struct SZrExecIrMaterializedObjects {
    /* Caller-owned storage; all count entries must be null or valid objects.
     * Publish these values to the caller's roots before the next safepoint. */
    SZrObject **objects;
    TZrUInt32 count;
    TZrUInt32 capacity;
} SZrExecIrMaterializedObjects;

typedef struct SZrExecIrObjectMaterializationRequest {
    struct SZrState *state;
    /* target must be NULL; the logical snapshot is prepared privately. */
    SZrExecIrResumeRequest checkpoint;
    /* valueId - 1 indexed. Moving GC may update pointer payloads, including
     * on failure; the values' logical identities and ownership stay intact. */
    SZrTypeValue *values;
    TZrUInt32 valueCount;
    /* Moving GC likewise updates the prototype pointers in this table. */
    SZrExecIrRuntimeTypeBinding *types;
    TZrUInt32 typeCount;
    SZrExecIrMaterializedObjects *target;
} SZrExecIrObjectMaterializationRequest;

/* Rebuild the selected graph using real GC objects, with one object per recipe
 * identity. Constructors, getters, setters and user drops are never replayed.
 * Call only at a legal GC safepoint; detached/critical native modes are rejected.
 * Callers sharing a domain with other active threads must root their inputs
 * before entering a GC-aware mutator scope and keep that scope through this
 * call and publication of output roots. Inactive entry is single-mutator only.
 * Plain GC/value fields are supported; ownership-bearing values and resource
 * prototypes fail closed until the frame's ownership transfer can commit too.
 * Failure preserves the previous target; success publishes objects in recipe
 * order. Temporary roots and detached storage are released on every exit. */
ZR_CORE_API TZrBool ZrCore_ExecIr_MaterializeObjects(
        const SZrExecIrObjectMaterializationRequest *request,
        SZrExecIrDiagnostic *diagnostic);

#endif
