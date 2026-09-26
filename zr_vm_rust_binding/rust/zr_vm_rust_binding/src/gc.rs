use zr_vm_rust_binding_sys as sys;

use super::{check_status, Error, ProjectSession};

/// 一次协作式 GC slice 的统计快照；数值来自 native collector 和 binding owner。
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GcStepResult {
    /// Collector pause time reported by the completed slice.
    pub pause_micros: u64,
    /// VM roots retained by the collector and Rust binding boundary.
    pub root_count: u64,
    /// BUG: 当前 C 侧按 owner 引用数估算，也包含 checkpoint 引用；不等于 live Value 句柄数。
    pub cross_boundary_reference_count: u64,
}

impl ProjectSession {
    /// 在保留的 session 上执行一个 GC slice，并返回本轮暂停和 root 统计。
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
