# 03-M4 Span core 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md`
的 `M4 Span core`。

## 状态与产出记录

- 完成时间：2026-07-21 10:19 +08:00
- 状态：已完成
- 完成项目：
  - 已发布 `Span<T>` / `ReadOnlySpan<T>` native TypeDef、ref-like 与 mutable/readonly
    contiguous-view protocol、source/start/length/slice/readonly-conversion/create role；parser
    从结构化 descriptor 投影 canonical generic TypeDef，不比较类型名或 member name。
  - 已完成 `Array<T>.span()`、mutable/read-only index、slice、readonly weakening、default/empty、
    exact-overload ranking 和 element mismatch/capability strengthening rejection；常规路径直接
    lower 到 inline field operations，不分配 object wrapper，也不依赖 native callback。
  - 已新增 `SZrSemanticContiguousViewFact` 与 `SZrSemanticBoundsFact`：冻结 array/owner/
    native-pinned/view source kind、view ValueId、source Place、region、known range、source loan、
    bounds proof 与 elision bit；validator 拒绝缺少 constant proof 或 source loan 的非法事实。
  - 已将 owner mutable loan 与 native-pinned shared loan接入 value-driven NLL；后续 view use
    保持 loan 活跃时，source move/drop 产生 `LOAN_CONFLICT`，事实没有 view use 时不人为延长
    lexical lifetime。
  - 已完成 check-elimination 微基准：同一结果为 22 的 constant/dynamic index 程序中，仅
    constant `slice(0, 3)[1]` 证明上下界并删除检查；dynamic index 保留额外 conditional branch。
  - 已完成严格 AOT C artifact/shared-library 回归：default Span 的 empty slice、readonly
    conversion 与 length 在 VM/AOT 均返回 0；scalar-local operand 写入 inline primitive field
    由专用 emitter 直接 lowering，避免读取被优化器省略的 dense slot。
  - 已完成 array source 的 moving-GC 生命周期门禁：array-backed `Span<int>` 存活期间触发
    immediate full compact GC，随后通过同一 view 写入并读回 42；probe 返回值使用独立 local，
    避免测试 callback 的临时返回槽与 live inline view field 发生复用。
  - 已补 `zr.container` contiguous-view 模块文档，明确 protocol/role、representation、bounds、
    lifetime 与 M4/M5 边界。

## 当前验证结果

- 固定验收快照：基线 `66e6805ea0d151f2740a558b2742cff2a7d2fe82`；该提交的 15 个
  exact paths blob mismatch=0，M4 的 32 个 code/test/module/plan/record paths SHA-256
  mismatch=0。
- GCC、Clang、MSVC 各 9/9 目标真实进程 exit 0；每套共 322 Tests、0 Failures。GCC/Clang
  0 Ignored；MSVC 2 Ignored，均为既有 Unix-only strict AOT shared-library 执行边界。
- `zr_vm_span_core_test`：15 Tests、0 Failures、0 Ignored，包含 array Span active 时 full
  compact GC、GC 后写回与读取。
- `zr_vm_aot_c_value_type_shared_library_smoke_test`：GCC/Clang 6 Tests、0 Failures、
  0 Ignored；MSVC 6 Tests、0 Failures、2 Ignored。新增 Span case 真实生成 `.zro`、strict
  AOT C、Unix shared library 并通过 AOT runtime 执行。
- 其余每套矩阵：pre-SemIR 9/9、type inference 119/119、ref struct restrictions 10/10、
  canonical consumers 15/15、artifact schema 13/13、AOT SemIR contracts 8/8、compiler
  integration 127/127。
- RED 证据：
  - `.codex/logs/s03m4-bounds-red-test.log`
  - `.codex/logs/s03m4-source-loan-red-test.log`
  - `.codex/logs/s03m4-aot-span-test7.log`
- 最终 GREEN 证据：
  - `.codex/logs/s03m4-span-green11.log`
  - `.codex/logs/s03m4-aot-span-test11.log`
  - `.codex/logs/s03m4-source-loan-presemir-green-test2.log`
  - `.codex/logs/s03m4-final66e-gcc-summary.log`
  - `.codex/logs/s03m4-final66e-clang-summary.log`
  - `.codex/logs/s03m4-final66e-msvc-summary.log`
  - `.codex/logs/s03m4-final66e-{gcc,clang,msvc}-span_core.log`
  - `.codex/logs/s03m4-final66e-{gcc,clang,msvc}-aot_shared.log`

## 参考证据

- .NET `System/Span.cs`、`ReadOnlySpan.cs`：运行时形态是 reference + length，index 使用
  无符号范围检查，slice 使用防溢出的 `start/length` 验证，mutable view 只能向 readonly
  view 弱化。
- .NET `System.Memory/tests/Span/AsSpan.cs`：覆盖 empty、start==length、负数、越界、
  segment 与写回原数组。
- C# `span-safety.md`：ref struct 不得装箱、进入普通 field/array、逃逸 closure 或跨
  await/yield，返回和赋值受 safe-context 上界约束。
- Rust `core/src/slice/raw.rs`：native source 必须满足单一 allocation、alignment、
  initialized range、无地址回绕和 lifetime 绑定；mutable slice 期间禁止其他 alias 访问。
- Rust `tests/codegen-llvm/slice-indexing.rs`、`bounds-check-elision-slice-min.rs`：普通索引
  保留 bounds check，受 checked-add/length 支配的访问可以消除重复检查。
- Mono `sgen-pinning.c`、`mono/tests/gchandles.cs`：裸 native 地址只有在显式 pin handle
  活跃期间才能跨 moving GC 保持稳定。

## 当前实现边界

- compiler/runtime/AOT 只能消费 protocol、member role、canonical TypeId、TypeLayout 和
  Place/CFG facts；禁止比较 `Span`、`ReadOnlySpan` 或成员名字来决定语义。
- array、owner 与 native source 共用连续 view contract；native source 必须携带显式
  stable handle/pin，owner source 必须携带阻止 move/drop/reuse 的 borrow guard。
- default/empty view 长度为 0 且不可解引用；index 需要 `0 <= index < length`；slice
  使用 `start <= length && sliceLength <= length - start` 的防溢出检查。
- M4 验收包含 mutable/read-only index、slice、readonly weakening、VM/AOT 等价、
  GC/pin/owner 生命周期和 bounds-check elimination 的直接证据。
- M4 对 owner/native 来源冻结通用 protocol、source kind、source-loan 与 move/drop conflict
  事实；具体 `PoolLease` borrow、exception cleanup、pin/unpin 和跨模块 ABI 由 M5 实现，
  不以本里程碑的手工 SemIR lifecycle fixture 冒充具体 pooling/FFI API 已完成。
