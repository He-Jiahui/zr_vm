#ifndef ZR_VM_CORE_EXEC_IR_MODULE_COPY_H
#define ZR_VM_CORE_EXEC_IR_MODULE_COPY_H

#include "zr_vm_core/exec_ir.h"

/**
 * Deep-copy a whole module with one same-identity function replacement.
 *
 * Intended GREEN storage contract: clone all source module pools/functions,
 * then clone replacement into the selected candidate slot. All fallible work
 * precedes release of the old destination and publication of the candidate.
 * Failure leaves source, replacement and old destination unchanged and usable;
 * success invalidates any borrow from the released old destination. Unselected
 * functions and module pools retain their logical content. Selected content
 * comes from replacement, including its frame and contract.layoutHash; module
 * contract.layoutHash stays the source value. Existing clone normalization
 * applies to capacities; padding and spare bytes are outside content equality.
 * Optional present-empty GC/state/frame headers retain their presence.
 *
 * Source/replacement and every reachable typed header/pool must be initialized,
 * genuinely readable, stable and serialized during the call. Destination must
 * be a unique owning module initialized by ModuleInit or existing storage APIs.
 * Destination header and optional writable diagnostic must not overlap input
 * headers or their reachable storage, or each other. Separate owning records
 * must not share pools; partial/header/shallow ownership aliases are forbidden.
 * These are caller preconditions, not arbitrary-pointer safety guarantees.
 * Exact source==destination is refused. Replacement may borrow the selected
 * source slot or a function in the old destination: all input reads finish
 * before old-destination release. Such a borrow is not a separate owning record.
 * The caller serializes mutation, allocation lifetime and publication.
 *
 * This API copies storage, not semantic or execution admission. It does not
 * call VerifyModule, VerifyFunction_ALL or ExecutionContract_Check, repair IDs,
 * remap CFG/SSA/maps, generate hashes, or own/copy a canonical registry. Numeric
 * canonical identities are retained values. Replacement references must use
 * the copied module's same constant/layout numbering; storage copying alone
 * does not establish semantic compatibility. Equal zero identities, unknown
 * capability/effect bits, nonzero reserved fields and noncurrent version values
 * are allowed equality values, subject to existing lower clone storage checks.
 *
 * Diagnostic is optional. All nine fields are reset at entry and success.
 * First-error order for the intended GREEN implementation is:
 *  1. NULL source/replacement/destination or exact source==destination:
 *     INVALID_ARGUMENT, all payload fields zero.
 *  2. selectedId==0 or >source.functionCount: INVALID_RANGE,
 *     expectedVersion=source.functionCount, actualVersion=selectedId;
 *     functionToken, locations and hashes zero. No function-slot access occurs.
 *  3. CloneModule failure: preserve every actual lower diagnostic field.
 *  4. Selected candidate slot.id != selectedId: INVALID_BLOCK, expected=selectedId.
 *  5. replacement.id != selectedId: INVALID_BLOCK, expected=selectedId.
 *  6. functionToken mismatch: TARGET_MISMATCH.
 *  7. function signatureHash mismatch: SIGNATURE_MISMATCH.
 *  8-12. Contract schemaVersion, abiVersion, logicalVersion, reserved0, reserved1,
 *        in that order: VERSION_MISMATCH.
 * 13. Contract generation mismatch: STALE_GENERATION.
 * 14. Contract targetToken mismatch: TARGET_MISMATCH.
 * 15. Contract signatureHash mismatch: SIGNATURE_MISMATCH.
 * 16. Contract moduleHash mismatch: MODULE_MISMATCH.
 * 17. Contract requiredCapabilities mismatch: CAPABILITY_MISMATCH.
 * 18. Contract declaredEffects mismatch: EFFECT_MISMATCH.
 * 19. CloneFunction replacement failure: preserve every actual lower field.
 *
 * Code names above denote existing ZR_EXECUTION_DIAGNOSTIC_* enums, except
 * INVALID_RANGE/INVALID_BLOCK use ZR_EXEC_IR_DIAGNOSTIC_* enums. For orders4-18,
 * reset the whole diagnostic, set functionToken=selected candidate.functionToken,
 * and leave blockId/instructionId/sourceId zero. Expected is the selected cloned
 * source field; actual is replacement (order4 actual is candidate slot.id).
 * For 32-bit IDs/tokens/version/reserved/capability/effect comparisons, populate
 * expectedVersion/actualVersion only. For 64-bit signature/generation/moduleHash
 * comparisons, populate expectedHash/actualHash only. Other payload fields stay
 * zero. layoutHash is deliberately excluded from immutable-field equality.
 * Lower clone failures retain all nine fields unchanged through cleanup.
 * Thus range refusal precedes malformed source storage; source clone failure
 * precedes identity mismatch; identity mismatch precedes replacement storage
 * refusal. No whole-output rollback across nonlocal exceptions is promised.
 *
 * Current RED staging implements only orders1-2, then refuses other readable
 * inputs with UNSUPPORTED and zero payload. It performs no clone/allocation,
 * frees no ownership and never publishes a destination. GREEN requires a
 * separate implementation and validation gate.
 */
ZR_CORE_API TZrBool ZrCore_ExecIr_CloneModuleReplacingFunction(
        const SZrExecIrModule *source, TZrExecIrFunctionId selectedId,
        const SZrExecIrFunction *replacement, SZrExecIrModule *destination,
        SZrExecIrDiagnostic *diagnostic);

#endif /* ZR_VM_CORE_EXEC_IR_MODULE_COPY_H */
