use zr_vm_rust_binding_sys as sys;

use super::{check_status, Error, ProjectSession};

/// Result of one cooperative ZrVM collector slice.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GcStepResult {
    /// Collector pause time reported by the completed slice.
    pub pause_micros: u64,
    /// VM roots retained by the collector and Rust binding boundary.
    pub root_count: u64,
    /// Live ZrVM value handles currently retained across the C/Rust boundary.
    pub cross_boundary_reference_count: u64,
}

impl ProjectSession {
    /// Runs one collector slice using the host-provided maximum pause budget.
    pub fn gc_step(&mut self, max_pause_micros: u64) -> Result<GcStepResult, Error> {
        let mut result = sys::ZrRustBindingGcStepResult::default();
        check_status(unsafe {
            sys::ZrRustBinding_ProjectSession_GcStep(self.raw, max_pause_micros, &mut result)
        })?;
        Ok(GcStepResult {
            pause_micros: result.pauseMicros,
            root_count: result.rootCount,
            cross_boundary_reference_count: result.crossBoundaryReferenceCount,
        })
    }
}
