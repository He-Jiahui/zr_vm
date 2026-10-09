#ifndef ZR_VM_CORE_FUNCTION_EXEC_IR_H
#define ZR_VM_CORE_FUNCTION_EXEC_IR_H

#include "zr_vm_core/exec_ir_owned_module.h"
#include "zr_vm_core/function.h"

/**
 * Associates an independent Core ExecIR storage snapshot with a Function.
 * Every non-NULL Function must be live, initialized, current-sized and rebuilt
 * with current headers. Mutation, lifecycle and borrowing require external
 * serialization. Source and its complete typed storage must remain readable
 * and stable during the call. Diagnostic and the owner slot must not overlap
 * source, its storage, the old owner or its storage, or each other.
 *
 * The generic CloneOwnedModule contract applies: failure preserves source and
 * the old owner and its complete lower diagnostic; success replaces only the
 * ExecIR owner after cloning. Source may be the current borrowed ExecIR view.
 * NULL Function/source reports INVALID_ARGUMENT after completely resetting the
 * optional diagnostic. Success leaves the complete diagnostic zeroed.
 *
 * This runtime-only association does not perform semantic verification, remap
 * IDs, change AOT ownership, install execution entries or serialize the slot.
 *
 * RED status: this constructor currently refuses all non-NULL Function/source
 * pairs with UNSUPPORTED, zero diagnostic payload and false return. It performs
 * no allocation, publication or source/owner mutation until separate GREEN GO.
 */
ZR_CORE_API TZrBool ZrCore_Function_SnapshotExecIr(
        SZrFunction *function, const SZrExecIrModule *source,
        SZrExecIrDiagnostic *diagnostic);

/**
 * Borrows storage; NULL or unattached Function returns NULL. The borrow ends on
 * successful replacement, Function destruction or record move. Reborrow from
 * the destination after a move. Do not modify the view or nested storage.
 * Non-NULL Functions satisfy the current-runtime requirements above; no older
 * smaller-record compatibility or concurrent borrow lease is provided.
 */
ZR_CORE_API const SZrExecIrModule *ZrCore_Function_GetExecIr(
        const SZrFunction *function);

/**
 * Clears only ExecIR aliases in fresh immediate shallow copied child shells,
 * before publication/destruction. Never apply to actual unique owners or moved
 * destinations. NULL storage is a no-op. No free, allocation or recursion occurs;
 * original records and their descendant owners remain unchanged. Other copied
 * buffers remain borrowed, so arbitrary shells cannot be independently destroyed
 * through Function_Free. Clear AOT aliases separately with its existing helper.
 */
ZR_CORE_API void ZrCore_Function_ForgetCopiedExecIrOwnerAliases(
        SZrFunction *copiedChildren, TZrUInt32 count);

#endif /* ZR_VM_CORE_FUNCTION_EXEC_IR_H */
