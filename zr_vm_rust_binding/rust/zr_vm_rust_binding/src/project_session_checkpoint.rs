use std::ptr;

use crate::{check_status, Error, ProjectSession};
use zr_vm_rust_binding_sys as sys;

/// A retained VM checkpoint. The native handle owns the VM roots and the
/// logical object graph captured at the quiescent session boundary.
pub struct ProjectSessionCheckpoint {
    raw: *mut sys::ZrRustBindingProjectSessionCheckpoint,
}

impl ProjectSession {
    pub fn checkpoint(&mut self) -> Result<ProjectSessionCheckpoint, Error> {
        let mut raw = ptr::null_mut();
        check_status(unsafe { sys::ZrRustBinding_ProjectSession_Checkpoint(self.raw, &mut raw) })?;
        Ok(ProjectSessionCheckpoint { raw })
    }

    pub fn rollback(&mut self, checkpoint: &ProjectSessionCheckpoint) -> Result<(), Error> {
        check_status(unsafe {
            sys::ZrRustBinding_ProjectSession_Rollback(self.raw, checkpoint.raw)
        })
    }
}

impl Drop for ProjectSessionCheckpoint {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                let _ = sys::ZrRustBinding_ProjectSessionCheckpoint_Free(self.raw);
            }
            self.raw = ptr::null_mut();
        }
    }
}
