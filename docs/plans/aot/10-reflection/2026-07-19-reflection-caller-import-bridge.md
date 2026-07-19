---
plan_id: aot-10-reflection
record_id: 2026-07-19-reflection-caller-import-bridge
status: completed
completed_at: 2026-07-19 15:29:57 +08:00
source_plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 10-S4Z51 zr.reflection Caller-Context Import

## 状态与产出记录

- 完成时间：2026-07-19 15:29:57 +08:00
- 状态：10 caller-context `zr.reflection` import 子里程碑完成；10 完整反射 API 里程碑仍为部分完成。
- 完成项目：把上一阶段 target-owned service 接入普通/guard import，同时保持 caller runtime 隔离、签名校验、
  exact path 语义和全局 path cache 隔离。
- 计划映射：10-S4Z51。

## Import 契约

- 仅 byte length 与内容都精确等于 `zr.reflection` 时启用 bridge；`zr.reflection.extra` 与含 embedded NUL 的
  路径继续走普通 import 语义。
- 查找扫描 loaded module registry，要求 string key、module value、GC/native flags、READY state、metadata runtime
  ownership 和 code registration 均合法。任何 malformed pair、计数不一致或 bucket cycle 直接 fail-closed。
- 同一 runtime 重复 import 返回相同 service；不同 caller runtimes 返回不同 service；全局 `zr.reflection` path
  cache 始终为空。
- exact reflection guard 在无法解析 caller 时清除 stale module diagnostic 并返回 null；required import 仍沿用原
  unavailable/signature mismatch 诊断路径。

## 验证与未完成边界

- 初始 RED 为 35 项中 bridge 用例返回 null；实现后转为 35/0。
- mutation RED 分别证明 loader caller 刷新和两层 native string-key 校验不可删除；registry overflow/self-cycle、
  owner self-cycle/双节点 cycle、alias ambiguity 和 OOM full-GC 均有回归覆盖。
- 最终 MSVC/GCC/Clang 动态泛型均为 35/0；MSVC 聚焦 CTest 6/6，共享回归 66/0 + 31/0 + 95/0。
- `zr.reflection.declaration`、createInstance/property/invoke API、global native registration、replacement/unload 和
  module generation 仍待后续阶段。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ad-10-s4z51-11-s5i-reflection-caller-import-bridge.md`
