#ifndef ZR_VM_CORE_GC_DOMAIN_CLONE_H
#define ZR_VM_CORE_GC_DOMAIN_CLONE_H

/*
 * Structured clone façade for isolated GC domains.
 *
 * A clone is always a copy: it never moves a source ownership value and it
 * never publishes a source-domain managed pointer to the target domain.  The
 * implementation delegates graph encoding and the Prepare/Publish/Claim/
 * Commit state machine to ownership_transfer, while this API keeps the source
 * and target domain admission checks together and makes failure cleanup
 * explicit to callers.
 */

#include "zr_vm_core/ownership_transfer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SZrGcDomainCloneTransaction SZrGcDomainCloneTransaction;

#define ZR_GC_DOMAIN_CLONE_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_DOMAIN_CLONE_SCHEMA_HASH UINT64_C(0x434c4f4e455f5631)

/*
 * Prepare performs the complete source-graph preflight.  `source` and
 * `quota` are borrowed for the call only.  A successful transaction owns its
 * encoded graph until Commit, Abort, or Free; no source value is consumed.
 */
ZR_CORE_API SZrGcDomainCloneTransaction *ZrCore_GcDomainClone_Prepare(
        struct SZrState *sourceState,
        struct SZrState *targetState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        SZrDomainTransferDiagnostic *diagnostic);

/* Publish and claim are separate so a scheduler can place the transaction in
 * a queue and identify the exact worker/epoch that owns the claim. */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Publish(
        SZrGcDomainCloneTransaction *transaction,
        SZrDomainTransferDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Claim(
        SZrGcDomainCloneTransaction *transaction,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrDomainTransferDiagnostic *diagnostic);

/* Commit requires a null-initialized destination value. */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Commit(
        SZrGcDomainCloneTransaction *transaction,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);

/* Abort is an explicit cancellation boundary.  It uses the source as the
 * cancellation authority, including after target-domain shutdown, and closes
 * the envelope once the abort is linearized. */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Abort(
        SZrGcDomainCloneTransaction *transaction,
        SZrDomainTransferDiagnostic *diagnostic);

/* Free is an idempotent wrapper disposer.  Successful Commit/Abort already
 * close the runtime envelope, so this call is also safe after source teardown
 * when only the wrapper remains. */
ZR_CORE_API void ZrCore_GcDomainClone_Free(
        SZrGcDomainCloneTransaction *transaction);

ZR_CORE_API TZrBool ZrCore_GcDomainClone_GetSnapshot(
        const SZrGcDomainCloneTransaction *transaction,
        SZrOwnershipTransferSnapshot *outSnapshot);

/* Convenience path for synchronous callers.  It performs all four transfer
 * phases and disposes the envelope before returning. */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Execute(
        struct SZrState *sourceState,
        struct SZrState *targetState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_DOMAIN_CLONE_H */
