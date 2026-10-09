#ifndef ZR_VM_CORE_EXEC_IR_OWNED_MODULE_H
#define ZR_VM_CORE_EXEC_IR_OWNED_MODULE_H

#include "zr_vm_core/exec_ir.h"

/** Independently owns a module and its reachable storage; do not copy ownership. */
typedef struct SZrExecIrOwnedModule SZrExecIrOwnedModule;

/**
 * Deeply snapshots storage and replaces the unique destination owner on success.
 * source and its complete typed storage must be initialized, readable, stable
 * and free of concurrent modification throughout the call. destination must
 * point to an exclusive live slot containing NULL or an owner from this API.
 * The slot and optional diagnostic must not overlap source, its storage, the
 * old owner or its storage, or each other. These are caller preconditions.
 *
 * source may be the old owner's borrowed view: all source reads finish before
 * successful replacement releases the old owner. Failure preserves source,
 * the destination slot and the old owner. Success owns an independent copy.
 *
 * This is a multi-allocation storage snapshot using CloneModule, not semantic
 * verification or execution admission. It preserves that clone's existing
 * capacity policy and optional present-empty headers, and does not remap IDs.
 *
 * An optional diagnostic is completely reset on entry. NULL source or slot
 * reports INVALID_ARGUMENT with zero payload. Envelope allocation failure
 * reports OUT_OF_MEMORY with expectedVersion=1, actualVersion=0 and all other
 * payload fields zero. Lower CloneModule failure preserves its complete
 * diagnostic unchanged. Success leaves the complete diagnostic zeroed.
 */
ZR_CORE_API TZrBool ZrCore_ExecIr_CloneOwnedModule(
        const SZrExecIrModule *source, SZrExecIrOwnedModule **destination,
        SZrExecIrDiagnostic *diagnostic);

/**
 * Borrows the module until owner release or successful replacement.
 * NULL owner returns NULL. Do not modify the view or its nested storage.
 */
ZR_CORE_API const SZrExecIrModule *ZrCore_ExecIr_OwnedModuleView(
        const SZrExecIrOwnedModule *owner);

/** Releases the unique owner and clears its slot; NULL and empty slots are safe. */
ZR_CORE_API void ZrCore_ExecIr_OwnedModuleFree(SZrExecIrOwnedModule **owner);

#endif /* ZR_VM_CORE_EXEC_IR_OWNED_MODULE_H */
