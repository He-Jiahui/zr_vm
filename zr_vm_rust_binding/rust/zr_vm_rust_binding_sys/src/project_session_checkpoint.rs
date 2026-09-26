#[repr(C)]
pub struct ZrRustBindingProjectSessionCheckpoint {
    _private: [u8; 0],
}

extern "C" {
    pub fn ZrRustBinding_ProjectSession_Checkpoint(
        session: *mut super::ZrRustBindingProjectSession,
        out_checkpoint: *mut *mut ZrRustBindingProjectSessionCheckpoint,
    ) -> super::ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectSession_Rollback(
        session: *mut super::ZrRustBindingProjectSession,
        checkpoint: *const ZrRustBindingProjectSessionCheckpoint,
    ) -> super::ZrRustBindingStatus;
    pub fn ZrRustBinding_ProjectSessionCheckpoint_Free(
        checkpoint: *mut ZrRustBindingProjectSessionCheckpoint,
    ) -> super::ZrRustBindingStatus;
}
