use zr_vm_rust_binding_sys as sys;

use super::{check_status, Error, ProjectSession};

/// ## Summary
/// 保存一次同步协作式 GC step 调用后读到的绑定层统计。它由
/// `ProjectSession::gc_step` 从 C ABI 输出结构按值复制而来，供 REPL、编辑器
/// 调用方在多次 session 导出调用之间观察采样时的最后一步耗时和根保活情况。
///
/// ## Fields
/// - `pause_micros` 是 native collector 的最后一步耗时；core 提前返回时可能未更新。
/// - `root_count` 是 binding 对外报告的根计数汇总。
/// - `cross_boundary_reference_count` 是由 session execution owner 引用计数推导的
///   live Value 句柄数。
///
/// ## Value model
/// 这是三个无符号标量组成的可复制快照，不持有 `ProjectSession`、VM global、
/// `Value` 或 checkpoint 的资源；返回后可独立保存。C 层把 `root_count` 计算为
/// collector 的 `ignoredObjectCount` 加上跨边界句柄计数，因此它不是 VM 内部全部
/// root 槽位的枚举。
///
/// ## Invariants and lifecycle
/// 快照描述本次 C 调用结束后读到的统计；core 提前返回时耗时遥测可来自此前一步。
/// 它不会延长任何 owner 或 root 的生命周期。
/// checkpoint 的 owner 引用由 C 层单独跟踪并从跨边界句柄计数中排除；live `Value`
/// 被释放后，后续 step 才会反映减少后的计数。
///
/// ## References
/// - `zr_vm_rust_binding/src/zr_vm_rust_binding/api.c:1209` 填充暂停观测值；
///   `zr_vm_rust_binding/src/zr_vm_rust_binding/api.c:1211` 定义公开根计数的汇总口径。
/// - `zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:118` 给出配套 ABI 结构；
///   `zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs:1372` 在持有 Value 时读取快照。
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GcStepResult {
    /// collector 最后一步的 elapsed microseconds；core 提前返回时可保留此前值，不保证预算满足。
    pub pause_micros: u64,
    /// C 层汇总的 `ignoredObjectCount + crossBoundaryReferenceCount`。
    pub root_count: u64,
    /// 同一 execution owner 下的 live `Value` 句柄引用数；checkpoint owner 引用不计入。
    pub cross_boundary_reference_count: u64,
}

impl ProjectSession {
    /// ## Summary
    /// 在已启动且仍由该 session 持有的 VM global 上请求一个 GC step，然后把
    /// native 统计复制为 `GcStepResult`。调用者可在导出调用之间显式推进 collector，
    /// 并据返回快照决定下一次协作时机。
    ///
    /// ## Parameters
    /// - `max_pause_micros`：传给 C 层 `ZrCore_GarbageCollector_SetPauseBudgetUs` 的
    ///   pause/work budget。零值合法；core 的非分代 step loop 会将零预算归一为至少
    ///   一次工作机会。该值是协作式工作/暂停目标，不能当作 wall-clock 硬上限，返回的
    ///   `pause_micros` 可能大于或小于它。
    ///
    /// ## Returns
    /// C 入口返回 OK 时，交付调用结束后读取的统计快照；它不拥有 native 资源，也不
    /// 延长 session 或任何 `Value` 的生命周期。core 的 void step 若因暂停域请求失败
    /// 提前返回，OK 不保证本次取得回收工作，`pause_micros` 可能保留此前的遥测。
    /// 返回 status 失败时，`check_status` 紧接 FFI 失败读取
    /// C 的进程级错误快照，并以该调用返回的 status 构造 `Error`；已核实的空指针、
    /// 缺少 main-thread state 或 `activeCall` 检查发生在预算写入和 GC 调用之前。
    ///
    /// ## Constraints and lifecycle
    /// - `self` 必须来自 `ProjectWorkspace::start_session` 创建的 standard runtime，且其
    ///   native owner、global 和 main-thread state 仍有效；bare runtime 的 session start
    ///   当前返回 `UNSUPPORTED`。
    /// - Rust 的 `&mut self` 只允许安全调用按顺序发生；C 层还拒绝共享 owner 正在执行
    ///   导出时的重入调用。导出、GC、checkpoint 和 rollback 共用这一 owner；owner 计数
    ///   非原子，外部 C/unsafe 调用也必须遵守同一执行线程串行边界。
    /// - 每个 live VM `Value` handle 都保留 owner 和 `TempValueRoot`；`Value` Drop 经
    ///   `Value_Free` 先结束该 root，再释放 owner。因此要在读取“已释放”统计前先 drop
    ///   句柄。checkpoint 也保留 owner，但 C 层用 `checkpointRefCount` 将这些 storage
    ///   roots 从公开跨边界计数中剔除。创建 checkpoint 时 C 还要求没有 `activeCall`、`refCount` 等于
    ///   `1 + checkpointRefCount` 且 main-thread state 有效；成功后 retain owner 并递增该计数，
    ///   Rust `ProjectSessionCheckpoint::drop` 释放 core 快照并配对递减/释放。其静止边界仍由
    ///   `ProjectSession::checkpoint` 负责。
    /// - FFI 只同步借用 `self.raw` 和局部可写结果；C 返回后不保存结果指针，Rust 快照
    ///   因此没有额外生命周期要求。
    /// - TODO: `zr_vm_core/src/zr_vm_core/gc/gc.c:887` 的分代路径可进入
    ///   `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:2380` 的 `Exception_Throw`；
    ///   `zr_vm_core/src/zr_vm_core/exception/exception.c:570` 的非局部跳转及
    ///   `zr_vm_core/src/zr_vm_core/exception/exception.c:594` 的 abort 不由本入口的 status 表达。
    ///   下一步沿 `zr_vm_rust_binding/src/zr_vm_rust_binding/api.c:1123` 的线程重置、
    ///   当前恢复点与 GC 收尾核查异常是否跨过 Rust FFI、session 是否还能继续使用；
    ///   现有普通成功与重入拒绝测试不能证明这一故障路径会返回 Rust `Error`。
    ///
    /// ## References
    /// - `zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs:1371` 与
    ///   `zr_vm_rust_binding/rust/zr_vm_rust_binding/src/lib.rs:1376` 在 Value 释放前后调用；
    ///   `zr_vm_rust_binding/rust/zr_vm_rust_binding/tests/execution_budget.rs:235` 检查句柄释放后的计数。
    /// - `zr_vm_rust_binding/src/zr_vm_rust_binding/api.c:1185` 给出 C 重入门禁，
    ///   `zr_vm_rust_binding/src/zr_vm_rust_binding/api.c:1195` 调用 core 的 void step；
    ///   `tests/rust_binding/test_rust_binding_api.c:1299` 是实际的嵌套调用拒绝场景。
    /// - `zr_vm_core/src/zr_vm_core/gc/gc.c:858` 的暂停请求失败会提前结束 void step；
    ///   `zr_vm_core/src/zr_vm_core/gc/gc.c:936` 是成功到达末尾时的遥测更新。
    /// - `zr_vm_rust_binding/src/zr_vm_rust_binding/value.c:390` 与
    ///   `zr_vm_rust_binding/src/zr_vm_rust_binding/project_session_checkpoint.c:74` 分别结束 Value root 和 checkpoint 引用。
    pub fn gc_step(&mut self, max_pause_micros: u64) -> Result<GcStepResult, Error> {
        let mut result = sys::ZrRustBindingGcStepResult::default();
        // SAFETY: self.raw belongs to this live ProjectSession, result is a writable
        // C-compatible local, and the synchronous C call does not retain either pointer.
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
