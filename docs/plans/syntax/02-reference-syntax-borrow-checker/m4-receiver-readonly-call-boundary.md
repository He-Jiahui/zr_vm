# 02-M4 Receiver/read-only 与 call boundary 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M4 Receiver/read-only 与 call boundary`。

## 状态与产出记录

- 完成时间：2026-07-20 05:13 +08:00
- 状态：已完成
- 完成项目：
  - class、struct、interface method AST 保存 default/const receiver modifier；top-level/static
    `const fn` 定向拒绝，`readonly T` 保持独立 capability view。
  - `SZrTypeMemberInfo.receiverEffect` 成为 source、override/interface、generic/dynamic 和
    imported/native member 的统一 receiver contract；readonly requirement 不允许 writable
    implementation。
  - packed v34 `SZrCompiledMemberInfo` 维持 31 个 `TZrUInt32`；reader 与 compiler producer
    均有编译期布局断言，callable effect 通过既有 `isConst` bit 投影并仅对 receiver-bearing
    member kind 恢复。
  - native method/meta metadata 只消费显式 `READONLY_RECEIVER`，不再从 inline runtime flag
    猜语义 effect；builtin IArrayLike 与 container Array/Map GET descriptor 已迁移，SET 保持
    mutable。
  - `ZrParser_ReceiverCall_Analyze*` 对 class/struct/interface/override/generic/dynamic/native
    dispatch 使用同一 canonical capability matrix；Unique/Shared/Weak auto-deref 边界完整。
  - compiler-generated mutable receiver 在参数前 reserve、调用前 activate、调用后 end；
    readonly receiver 建立即时 shared loan，显式 ref 不进入 two-phase 路径。
  - activation 使用 CFG forward must-active、may-active 与 available-reservation facts；
    branch-only activation、join 与 loop backedge 不依赖全局 instruction ID 数值顺序。
  - compiler 在 executable publication 前执行 pre-Semantic-IR structural + flow gate，并显式
    记录 receiver LoanId，覆盖 immediate shared 与 two-phase mutable receiver conflict。
  - receiver semantic Place 与 ABI staging slot 分离；local/field projection 复用 resolved
    member identity，projected alias、readonly outer nested write 与 mutable outer nested write
    均被拒绝，合法 `buffer.push(buffer.read())` 实际执行并返回 1。
  - resolved receiver-call target 的 canonical identity 明确保留给 M6：后续事实/query 必须
    发布并消费 `SymbolId`/declaration range，LSP 不得按 member name 推断。
  - 未新增 VM/AOT runtime borrow fallback；GCC 11.4、Clang 14.0、MSVC 19.44.35228
    `/W4` 各通过相同八套 184/184 项，GCC 额外通过 compiler integration 127/127；最终
    独立只读复审为 Critical 0 / Important 0。
- 验收证据：
  - `tests/acceptance/2026-07-20-syntax-02-m4-receiver-readonly-call-boundary.md`
  - `docs/parser-and-semantics/receiver-readonly-call-boundary.md`
  - GCC build：`/home/hejiahui/zr_vm-syntax-02-m4-gcc`
  - Clang build：`/home/hejiahui/zr_vm-syntax-02-m4-clang`
  - MSVC build：`build-syntax-02-m4-msvc`
- 里程碑提交：初始实现进入 `c9c51d9`；最终评审修复、验收证据与本记录随
  `fix(syntax): close receiver call boundary milestone` 一并提交。

## 边界与后继

- M4 完成 receiver readonly capability、owner auto-deref、override/interface variance、
  native/artifact projection 与 call-scoped two-phase loans；不实现 caller/function/heap
  escape、ref return、closure capture 或 suspension 检查。
- 当前 compiler sidecar 使用 execution-order entry/exit CFG，足以覆盖 M4 call-scoped loan；
  完整 statement CFG lowering 可在后续里程碑替换而不改变公共 loan facts。
- 下一阶段 M5 将实现 caller/function/heap escape、ref return、closure、async/generator，
  并要求所有非法逃逸具有精确 origin/escape 诊断且 VM/AOT 不含 runtime borrow fallback。
