# 04-M4 domain identity 与单 mutator bridge 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
的 `M4 domain identity 与单 mutator bridge`。

## 状态与产出记录

- 完成时间：2026-07-22 09:17 +08:00
- 状态：已完成（M4 单 mutator runtime/bridge 晋级门）
- 已完成项目：
  - 已建立 runtime-internal `GcDomain` 创建/销毁和 state attach/detach 生命周期；每个
    domain 与 managed object 均携带 `id + generation`，无 attached domain 时拒绝 GC 分配。
  - 已发布 generation-checked `SZrGcRootHandle` create/clone/update/resolve/release API；clone
    共享 slot，shared update 使用 copy-on-write，domain/slot generation 拒绝 stale handle。
  - 已让 minor/major/compact collection 扫描并重写 domain root target；major collection
    同时扫描 permanent parent 的 managed children，避免永久 module/prototype 保存悬空 child。
  - 已把 direct Unique 和 live Shared resource 保活迁移到 structured ownership-root slot；
    final release/drop 精确注销 root，Syntax 04 resource/bridge 路径不再依赖 hidden ignore
    registry 猜测生命周期。
  - 已在 value barrier 与 object member/index storage mutation 前验证 owner、value 与 dynamic
    managed key 的 domain identity；普通跨 domain GC edge 在写入前拒绝且不落盘。
  - 已建立独立 canonical `Gc<T>` / `GcBox<T>` bridge kind、target-world 校验与显示文本；
    resource field 可声明 `Gc<ordinary class>`，`Gc<resource>` 与 `GcBox<ordinary class>`拒绝。
  - 已实现 consuming `intoGc(Unique<Resource>): GcBox<Resource>`；SemIR 保留 source Place，
    Shared、active loan 与 use-after-move拒绝，VM/AOT投影同一 structured ownership operation。
  - 已让 GcBox 不可达时执行 exactly-once resource Drop；Drop 中分配和 safepoint、显式 root
    保活、重复 full collection均无 double-drop。M4 落地时 AOT OwnDetach helper精确先尝试
    IntoGcBox，再进入 legacy Detach fallback；当前 source 已改用专用 `OWN_INTO_GC_BOX`
    opcode 与 `ZrLibrary_AotRuntime_OwnIntoGcBox`，`OWN_DETACH`/OwnDetach 只保留为旧 artifact
    reader（见下文“当前实现对齐”）。

## 最终验证结果

以下为 2026-07-22 M4 晋级时的历史证据，未在当前 tip 重新执行。

- GCC 11.4、Clang 14.0 与 MSVC 19.44 使用独立 build 目录串行完成同一 18-target 矩阵；
  每套 18/18 真实进程 `exit 0`，Unity 合计 551/551、0 failure。
- focused domain suite 5/5，覆盖 domain identity/cross-domain write、root handle
  copy-on-write/stale generation、explicit ownership root、minor/major/compact rewrite 与
  permanent-parent child scan。（当前 tip 上 `zr_vm_gc_domain_bridge_test` 仍由
  `tests/cmake/aot-contract-targets.cmake` 经 `zr_vm_add_unity_test_target` 只做
  `add_executable`，没有 `add_test` 名称，默认 CTest/CI 不运行；此 5/5 只能视为历史证据。）
- resource/bridge focused evidence 包含 resource Unique 19/19、Shared/Weak 11/11、pre
  Semantic IR 11/11、AOT ownership contract 1/1；expanded matrix同时覆盖 GC 66/66、parser
  75/75、type inference 119/119、compiler integration 127/127、canonical/dataflow/closure/
  exception/AOT/native fast path。
- GCC fresh build、Clang rebuild 与 MSVC 653-step rebuild均真实 `exit 0`；最终
  `git diff --check` 无 whitespace error。

## 当前实现边界

- M4 本身是单 mutator基线。M4 落地时 domain create/attach/detach由 global/state生命周期内部
  驱动；public host topology、mutator registry、domain-local STW handshake、native safepoint
  mode与 same-domain owner handoff由后续 M5 交付，见
  [`m5-domain-local-stw-owner-handoff.md`](m5-domain-local-stw-owner-handoff.md)（记录为
  2026-07-22 13:22 完成；`MutatorEnter/Leave/Attach/Detach`、`NativeEnter/Leave`、
  `StopTheWorldBegin/End`、`SharedMutationBegin` 已在
  `zr_vm_core/include/zr_vm_core/gc_domain.h` 公开）。
- `Gc<T>` 已有 canonical source type/field contract和公开 C `SZrGcRootHandle` carrier；本阶段
  不宣称通用 source constructor或完整AOT stack-map schema。`GcBox<T>` source bridge已通过
  `intoGc(uniqueResource)`落地。
- 仓库仍保留pre-Syntax04非resource ownership与其他runtime内部使用的legacy ignore API；
  M4晋级声明限定为新的resource/GcBox bridge不依赖该registry，不声称已删除所有legacy API。
- M4不包含多mutator、cross-domain transport、TransferEnvelope、concurrent major或M7
  artifact/LSP投影；这些由后续里程碑交付，见
  [`m5-domain-local-stw-owner-handoff.md`](m5-domain-local-stw-owner-handoff.md)、
  [`m6-cross-domain-transport.md`](m6-cross-domain-transport.md)（记录为 2026-07-22 19:19 完成）与
  [`m7-concurrent-major-artifact-aot-lsp.md`](m7-concurrent-major-artifact-aot-lsp.md)（记录为
  2026-07-23 00:27 完成）。

## 当前实现对齐（2026-11-23，基于 `18d902d6c`）

本节只记录 M4 之后已在代码中存在的事实，不改变任何 ownership 或 GC 策略。

- owner-to-GC bridge 的当前 opcode：source `intoGc(owner)` 经
  `compiler_ownership_builtin_opcode_from_kind`
  （`zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_support.c`）与 SemIR
  `ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX` 映射
  （`zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c`）发出
  `OWN_INTO_GC_BOX`。VM 的 `OWN_INTO_GC_BOX` 分支只调用 `ZrCore_Ownership_IntoGcBoxValue`；
  C/LLVM AOT 调用 `ZrLibrary_AotRuntime_OwnIntoGcBox`
  （`zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c`）。
- `OWN_DETACH` 与 `ZrLibrary_AotRuntime_OwnDetach` 只为旧 artifact 兼容保留，当前 source 不再
  发出。VM `OWN_DETACH` 分支与 AOT `aot_runtime_into_gc_box_or_detach` 保持同一顺序：先
  IntoGcBox，失败后才 Detach（参见
  `tests/acceptance/2026-08-29-ownership-legacy-detach-test-identity.md`）。
- 单 mutator 快路径在 M4 之后由 interpreter r2 的 R2-M2 修改：owner 线程上的 domain mutex
  省略、首次外部线程或第二个 mutator 前的原子 draining handoff，以及 per-state LIFO
  temporary roots。事实描述见
  [`docs/core-runtime/gc-domain-single-mutator-bridge.md`](../../../core-runtime/gc-domain-single-mutator-bridge.md)
  的 “Single-owner fast path”。
