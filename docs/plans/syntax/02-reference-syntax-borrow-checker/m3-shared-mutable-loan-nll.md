# 02-M3 Shared/mutable loan 与 NLL 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M3 Shared/mutable loan 与 NLL`。

## 状态与产出记录

- 完成时间：2026-07-20 01:26 +08:00
- 状态：已完成
- 完成项目：
  - `BorrowShared`、`BorrowMut`、`Reborrow` 建立稳定 LoanId；copy、move、convert 和
    ref Place Store/Load 保留 loan provenance。
  - ref Place 使用 CFG reaching-value kill/gen；覆盖写会终止旧引用传播，branch join 与
    loop back edge 保留全部可能 LoanId。
  - 基于 CFG successor 反向计算 instruction live-in/live-out，以 borrow origin 为 kill
    边界，并在最后一次可能使用后收缩 loan region。
  - reborrow 保留 direct parent set 与传递 ancestor closure，拒绝 parent cycle；多分支
    join 的全部可能 parent 都参与 freeze/suspend 与恢复。
  - reachable reborrow 必须有 parent，且 instruction/source Place 对每个 parent 只能相同
    或继续窄化；零 parent、disjoint 来源和从字段扩大到基对象均拒绝。
  - shared loan 只授权读取和 shared reborrow；exclusive access 与 mutable reborrow 必须
    由 mutable loan 授权，防止 LoanId 绕过 Place 冲突矩阵。
  - shared/shared 共存，mutable 与 overlap loan 冲突；Store、Initialize、Move、Drop 和
    绕过 mutable loan 的读取按 live loan 检查，disjoint 放行，unknown alias 保守拒绝。
  - diagnostic 发布 conflict Place/Loan、overlap 分类、Place declaration、loan origin 和
    last-use range；disconnected block 不污染 liveness、region 或诊断。
  - 覆盖 Rust/C# 对应负例、nested reborrow、动态索引 unknown alias、循环 loan、512 个
    disjoint loan 规模测试；未新增 VM/AOT runtime borrow fallback。
  - GCC 11.4、Clang 14、MSVC 19.44 `/W4` 各串行通过相同 7 个套件、165/165 项；独立
    代码审查结果为 Critical 0 / Important 0。
- 验收证据：
  - `tests/acceptance/2026-07-20-syntax-02-m3-reference-loan-nll.md`
  - `docs/parser-and-semantics/reference-loan-nll.md`
  - GCC build：`/home/hejiahui/zr_vm-syntax-02-m2-gcc-worktree`
  - Clang build：`/home/hejiahui/zr_vm-syntax-02-m2-clang-worktree`
  - MSVC build：`b-s02m3-msvc-r1`
- 里程碑提交：本记录随
  `feat(syntax): complete loan liveness milestone` 一并提交。

## 边界与后继

- M3 只完成 loan creation、liveness、overlap、reborrow 与 last-use，不实现 receiver
  readonly/call-boundary policy，也不实现 escape、closure 或 suspension 检查。
- 下一阶段 M4 将 readonly receiver、owner auto-deref、two-phase receiver borrow 与
  class/struct/interface/override/generic/dynamic/native 调用矩阵接入统一 loan facts。
