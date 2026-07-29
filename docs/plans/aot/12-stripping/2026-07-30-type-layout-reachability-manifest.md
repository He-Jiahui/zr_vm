---
plan_id: aot-12-stripping
record_id: 2026-07-30-type-layout-reachability-manifest
status: completed
completed_at: 2026-07-30 05:41:33 +08:00
source_plans:
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1C / 12-S2G / 12-S6B Type-Layout Reachability Manifest

## 状态与产出记录

- 完成时间：2026-07-30 05:41:33 +08:00
- 状态：type/layout reachability reason 与 deterministic manifest 子里程碑完成；AOT 12 的 S1、S2、S6
  仍为部分完成，AOT 07~12 总目标保持进行中。
- 完成项目：新增独立 type-layout reachability 模块，为每个 retained layout 生成有限 reason，不继续扩张已
  超过 1100 行的 layout emission 文件。
- 完成项目：frame slot 依赖发布为 `edge.frame_layout` 并记录 retained function flat index；dynamic dependency
  layout/type/field annotation 发布为 `root.reflection_annotation`，同一布局同时命中时 root 优先。
- 完成项目：manifest version 1 按 `typeLayoutId` 升序稳定输出，并与独立计算的 `typeLayoutsAfter` 数量严格
  对账。
- 完成项目：collector 对 function table、重复或非法 root、无法解析的 struct/union layout、无效 frame
  storage 与 count 不一致设置拒绝门；本切片直接以 unresolved retained layout 负例证明 public writer
  fail closed 且 emitter 删除半成品。
- 完成项目：补齐 ID 0 的 frame edge 与 annotation root、trimmed layout 缺席、field-token root precedence、
  稳定顺序和 flat index 2 前驱来源证据。
- 计划映射：12-S1 graph schema、12-S2 root policy、12-S6 reporting 的 type/layout-node 子切片。

## 代码与文档产出

- `backend_aot_c_type_layout_reachability.c/.h` 负责完整校验、确定性收集和 versioned manifest 写入。
- `backend_aot_c_emitter.c` 在 function filtering 后、layout after-count 计算后调用 manifest gate。
- `test_aot_c_code_stripping.c` 由 10 个用例扩展为 14 个，并验证 ID 0、稳定 flat-index provenance、root
  precedence 和 unresolved retained layout 失败路径。
- `docs/parser-and-semantics/aot-type-layout-reachability-manifest.md` 记录 reason、前驱、顺序和 fail-closed
  契约。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-s1c-s2g-s6b-type-layout-reachability-manifest.md`。

## 验证结果

- RED：旧 emitter 的 11-test 回退运行出现 4 个预期失败，其中 unresolved layout 明确为
  `Expected FALSE Was TRUE`；独立评审后追加的 ID 0 RED 为 14 项中 2 项预期失败。
- WSL GCC 11.4：focused CTest 2/2；直接运行 reachability 17/0、code stripping 14/0。
- WSL Clang 14.0：focused CTest 2/2；直接运行 reachability 17/0、code stripping 14/0。
- Windows MSVC 19.44 全新目录：focused CTest 2/2；直接运行 reachability 17/0、code stripping 14/0。
- 实际生成 C 已检查 layout 0/1/2 升序、ID 0 root/edge、root-over-edge、flat-function predecessor 2 和
  trimmed node 缺席。

## 未完成边界

- generic instance/dictionary、native import/callback、module initializer、reflection metadata、DebugMap sidecar 与
  resource Drop 节点尚未全部纳入统一 graph。
- debug/release-safe/release-aggressive closed-world policy、missing descriptor matrix 与完整 untrimmed-cause 报告
  仍需后续子里程碑。
- allocation 与只读/失败输出流等内部防御分支尚未做独立 fault injection，本记录不把它们列为已验证负例。
- 完整 code/metadata trim、compacted artifact publication、source/binary/full-AOT loader parity、binary size 和
  behavior parity 仍未关闭。
