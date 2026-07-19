# 01-M2 Place 与通用 CFG 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md` 的 `M2 Place 与通用 CFG`。

## 状态与产出记录

- 完成时间：2026-07-19 13:36 +08:00
- 状态：已完成
- 完成项目：
  - 建立 session-local Place graph，使用稳定 `PlaceId`、parent trace、canonical `TypeId`、完整 projection path 与 source range 表达可寻址存储。
  - 覆盖 local、parameter、this、static、temporary、returnSlot、externalHandle 七种 base，以及 field、dynamic index、constant index、deref、union variant、tuple element 六种 projection。
  - 实现 Place overlap 四态查询，覆盖 equal、disjoint、overlap、unknown，并对 dynamic index、deref 与 external alias 保持保守结果。
  - 将 parser CFG 改为动态 typed outgoing-edge 数组；固定 2-slot 数组只保留兼容前缀，不再构成 CFG 能力上限。
  - 建立 normal、true、false、switch case/default、exception、cleanup、return、suspend、resume edge，以及 cleanup/suspension block 和唯一 terminator kind。
  - 让 reachability 与 forward/backward dataflow 统一通过动态 successor accessor 遍历全部后继。
  - 将 return/throw 的 exit 连接拆入 cleanup 路由，存在 finally/cleanup 时禁止直接旁路；break/continue 保持经 finally 路径到循环目标。
  - 将 union switch 穷尽性结果传给 CFG，精确标记冗余 default 不可达，同时保留非穷尽 default 的可达性。
  - 新增直接验收测试，覆盖全部 Place base/projection、四态 overlap、五后继 switch、全部 edge kind、suspension edge、builder branch/return edge 和 return-through-finally 无旁路。
  - 完成 GCC 11.4、Clang 14 暂存快照与 MSVC 19.44 工作树兼容性验证；最终 Critical/Important 审查为 GO（0 Critical、0 Important）。
- 验收证据：
  - `tests/acceptance/2026-07-19-syntax-01-m2-place-cfg.md`
  - `docs/parser-and-semantics/place-cfg-graph.md`
  - GCC 暂存快照：`/home/hejiahui/zr_vm-syntax-m2-staged-gcc-20260719-r10`
  - Clang 暂存快照：`/home/hejiahui/zr_vm-syntax-m2-staged-clang-20260719-r10`
  - MSVC 工作树：`build-syntax-01-m1-msvc`
  - 三套环境均通过 Place/CFG、union exhaustiveness、reachability、finally、try/catch、typed catch、loop flow、dataflow、parser、type inference 与 semantic facts 共 11 组目标。
- 里程碑提交：本记录随 `feat(syntax): complete place and cfg milestone` 一并提交。

## 边界与后继

- M2 的 Place graph 与 CFG identity 只在当前编译 session 内稳定，不进入 artifact schema；`.zrs/.zri/.zro` 持久化属于 M4。
- suspension block/edge 已可见，但 async/generator lowering 与跨 suspension loan 检查尚未实现。
- 下一里程碑严格进入 M3 前置 Semantic IR 与 facts：由 Place/Type/CFG 产生显式 load/store/init/move/copy/drop/borrow 指令，并验证 definite assignment、move、loan、escape 的 block join。
