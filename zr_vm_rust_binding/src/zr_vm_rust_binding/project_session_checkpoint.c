#include "internal.h"

#include "zr_vm_core/session_checkpoint.h"

#include <stdlib.h>

/* checkpoint 要求 session 静止且没有跨边界 live Value root；额外 owner 引用让快照能独立释放。 */
ZrRustBindingStatus ZrRustBinding_ProjectSession_Checkpoint(
        ZrRustBindingProjectSession *session,
        ZrRustBindingProjectSessionCheckpoint **outCheckpoint) {
    ZrRustBindingProjectSessionCheckpoint *checkpoint;

    if (session == ZR_NULL || session->owner == ZR_NULL ||
        session->owner->global == ZR_NULL || outCheckpoint == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "project session or checkpoint output is null");
    }
    if (session->owner->activeCall ||
        session->owner->refCount != 1u + session->owner->checkpointRefCount ||
        session->owner->global->mainThreadState == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "project session is not at a quiescent checkpoint boundary");
    }
    checkpoint = (ZrRustBindingProjectSessionCheckpoint *)calloc(1u, sizeof(*checkpoint));
    if (checkpoint == ZR_NULL) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INTERNAL_ERROR,
                                         "failed to allocate project session checkpoint");
    }
    if (!ZrCore_SessionCheckpoint_Create(session->owner->global->mainThreadState,
                                         (SZrSessionCheckpoint **)&checkpoint->checkpoint)) {
        free(checkpoint);
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_UNSUPPORTED,
                                         "retained VM state cannot be checkpointed at this boundary");
    }
    checkpoint->owner = session->owner;
    zr_rust_binding_execution_owner_retain(checkpoint->owner);
    checkpoint->owner->checkpointRefCount++;
    *outCheckpoint = checkpoint;
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

/* 仅同一 owner 的快照可回滚，且不能在导出调用中或 live Value 存在时替换 VM 状态。 */
ZrRustBindingStatus ZrRustBinding_ProjectSession_Rollback(
        ZrRustBindingProjectSession *session,
        const ZrRustBindingProjectSessionCheckpoint *checkpoint) {
    if (session == ZR_NULL || session->owner == ZR_NULL || checkpoint == ZR_NULL ||
        checkpoint->owner != session->owner || checkpoint->checkpoint == ZR_NULL ||
        session->owner->activeCall || session->owner->global == ZR_NULL ||
        session->owner->global->mainThreadState == ZR_NULL ||
        session->owner->refCount != 1u + session->owner->checkpointRefCount) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "project session or checkpoint is invalid");
    }
    if (!ZrCore_SessionCheckpoint_Rollback(session->owner->global->mainThreadState,
                                           checkpoint->checkpoint)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_RUNTIME_ERROR,
                                         "retained VM state rollback failed");
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}

ZrRustBindingStatus ZrRustBinding_ProjectSessionCheckpoint_Free(
        ZrRustBindingProjectSessionCheckpoint *checkpoint) {
    if (checkpoint != ZR_NULL) {
        if (checkpoint->checkpoint != ZR_NULL && checkpoint->owner != ZR_NULL &&
            checkpoint->owner->global != ZR_NULL &&
            checkpoint->owner->global->mainThreadState != ZR_NULL) {
            ZrCore_SessionCheckpoint_Free(checkpoint->owner->global->mainThreadState,
                                          checkpoint->checkpoint);
        }
        if (checkpoint->owner != ZR_NULL && checkpoint->owner->checkpointRefCount > 0u) {
            checkpoint->owner->checkpointRefCount--;
        }
        zr_rust_binding_execution_owner_release(checkpoint->owner);
        free(checkpoint);
    }
    zr_rust_binding_clear_error();
    return ZR_RUST_BINDING_STATUS_OK;
}
