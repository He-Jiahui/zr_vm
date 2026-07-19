# Syntax 02 M3 shared/mutable loan and NLL acceptance

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M3 Shared/mutable loan 与 NLL`。

## Scope

- shared/mutable loan creation and conflict matrix。
- ref value propagation through semantic values and Place Store/Load。
- CFG backward liveness and last-use region contraction。
- nested mutable/shared reborrow parent freeze/suspension and restore。
- Place projection disjoint/unknown overlap diagnostics。
- branch-only use, loop-carried loan, move/drop during borrow。

## RED evidence

专项测试最初无法编译，因为 flow result 尚无 instruction LoanId liveness、loan region facts
和查询 API，diagnostic 也没有 overlap、loan origin、Place declaration 和 last-use range。

首版传播使用全函数 Place loan 并集。新增 ref slot 覆盖写回归后，旧引用被新引用替换，
后续 Load 仍错误携带两个 LoanId，导致旧 source Place 的合法写入被拒绝。修复前专项结果为
7 tests/1 failure。传播改为 CFG reaching-value kill/gen 后，该回归和原有用例一起转绿。

独立审查随后发现 shared LoanId 可错误授权写入/mutable reborrow、reborrow parent cycle 可令
ancestor traversal 不收敛、join ref 的多个 parent 被压缩为一个，以及 dead block 仍产生
loan conflict。四类 RED 均被专项回归复现后再修复；未带修复直接进入提交。

末轮审查继续发现 reborrow provenance 只检查对称 overlap，因而允许从父 `obj.field`
扩大为子 `obj`。新增方向性反例后，校验改为只接受相同 Place、父 Place 的更窄投影，或
动态索引/解引用造成的保守 UNKNOWN；零 parent、disjoint parent 和 widening 均拒绝。

## GREEN implementation

- BorrowShared/BorrowMut/Reborrow 的 created value 建立稳定 LoanId 映射；copy、move、convert
  和 ref Place Store/Load 保留 LoanId。
- 先用保守发现过程筛出可能携带 ref 的 Place，再只为这些 Place 计算 CFG 前向状态；Store
  替换当前集合，Load 读取到达集合，join/back edge 进入固定点。
- 按 CFG successor 反向计算每条指令 live-in/live-out；borrow origin 是 kill 边界，最后可能
  使用决定正常 region 终点。
- reborrow 从输入 ref value 推导 direct parent set；join ref 保留全部可能 parent，传递
  ancestor closure 在 liveness 前收敛，cycle 使分析失败。子 loan 活跃时冻结或暂停全部父
  访问，最后使用后恢复仍有后继使用的父 loan。
- 每个 reachable reborrow 必须有 parent，instruction Place 与 loan source Place 对每个
  parent 都只能保持相同或继续窄化；禁止 disjoint 来源和从字段反向扩大到基对象。
- loan path 授权校验 shared/mutable capability：shared 只允许 read/shared reborrow，
  exclusive access 和 mutable reborrow 必须由 mutable loan 授权。
- shared/shared 共存；mutable 与 shared/mutable 冲突；active loan 阻止 overlap Place 的
  Store/Initialize/Move/Drop，mutable 还阻止绕过引用的 direct read。
- Place graph 的 disjoint projection 直接放行；动态索引 unknown overlap 保守冲突，diagnostic
  保留 UNKNOWN 分类以及 declaration/origin/last-use 范围。
- disconnected/unreachable block 不参与 ref reaching values、loan uses、liveness、region 或
  conflict diagnostic。
- 分析核心 956 行、冲突/授权 287 行、facts 发布/查询 145 行，均保持低于 1000 行；没有
  新增 VM/AOT runtime borrow fallback。

## Verification

MSVC 19.44.35228.0 全新 Debug `/W4` 构建（`b-s02m3-msvc-r1`，548 steps）通过。
Clang 14.0.0 完成 475-step 全量目标构建，GCC 11.4.0 完成增量目标构建。三个工具链均
串行通过相同 7 个套件、165/165 项：

- reference loan/NLL 15
- pre-Semantic IR 6
- Place/CFG graph 4
- reference Place/out flow 5
- reference syntax contract 7
- type inference 118
- named arguments 10

## Promotion gate

- shared/mutable conflict and multiple shared loans：PASS。
- Rust-style move/drop while borrowed negatives：PASS。
- C#-style branch-only ref use and post-join last-use release：PASS。
- ref value Store/Load propagation and overwrite kill：PASS。
- nested mutable-to-mutable-to-shared reborrow parent restore：PASS。
- shared capability write-through/mutable-reborrow rejection：PASS。
- CFG-join multi-parent reborrow and parent-cycle rejection：PASS。
- reborrow zero/disjoint parent and source widening rejection：PASS。
- constant-index disjoint and dynamic-index unknown alias：PASS。
- loop back-edge liveness and exit release：PASS。
- unreachable block exclusion：PASS。
- single-function 512 disjoint Place/Loan scale：PASS。
- structured origin/declaration/last-use/overlap diagnostic payload：PASS。
- no VM/AOT runtime borrow fallback：PASS。

结论：M3 promotion gate 为 GO；独立代码审查结果为 Critical 0 / Important 0，进入
提交前 staged diff 边界检查。
