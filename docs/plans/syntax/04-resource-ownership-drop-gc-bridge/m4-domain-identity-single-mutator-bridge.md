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
  - 已实现 consuming `Unique<Resource>.intoGc(): GcBox<Resource>`；SemIR 保留 source Place，
    Shared、active loan 与 use-after-move拒绝，VM/AOT投影同一 structured ownership operation。
  - 已让 GcBox 不可达时执行 exactly-once resource Drop；Drop 中分配和 safepoint、显式 root
    保活、重复 full collection均无 double-drop。AOT OwnDetach helper精确先尝试 IntoGcBox，再
    进入 legacy Detach fallback。

## 最终验证结果

- GCC 11.4、Clang 14.0 与 MSVC 19.44 使用独立 build 目录串行完成同一 18-target 矩阵；
  每套 18/18 真实进程 `exit 0`，Unity 合计 551/551、0 failure。
- focused domain suite 5/5，覆盖 domain identity/cross-domain write、root handle
  copy-on-write/stale generation、explicit ownership root、minor/major/compact rewrite 与
  permanent-parent child scan。
- resource/bridge focused evidence 包含 resource Unique 19/19、Shared/Weak 11/11、pre
  Semantic IR 11/11、AOT ownership contract 1/1；expanded matrix同时覆盖 GC 66/66、parser
  75/75、type inference 119/119、compiler integration 127/127、canonical/dataflow/closure/
  exception/AOT/native fast path。
- GCC fresh build、Clang rebuild 与 MSVC 653-step rebuild均真实 `exit 0`；最终
  `git diff --check` 无 whitespace error。

## 当前实现边界

- M4 是单 mutator基线。domain create/attach/detach当前由 global/state生命周期内部驱动；
  public host topology、mutator registry、domain-local STW handshake、native safepoint mode与
  same-domain owner handoff属于M5。
- `Gc<T>` 已有 canonical source type/field contract和公开 C `SZrGcRootHandle` carrier；本阶段
  不宣称通用 source constructor或完整AOT stack-map schema。`GcBox<T>` source bridge已通过
  `Unique<Resource>.intoGc()`落地。
- 仓库仍保留pre-Syntax04非resource ownership与其他runtime内部使用的legacy ignore API；
  M4晋级声明限定为新的resource/GcBox bridge不依赖该registry，不声称已删除所有legacy API。
- M4不包含多mutator、cross-domain transport、TransferEnvelope、concurrent major或M7
  artifact/LSP投影；这些继续按M5-M7里程碑执行。
