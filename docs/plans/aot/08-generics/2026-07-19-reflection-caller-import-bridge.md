---
plan_id: aot-08-generics
record_id: 2026-07-19-reflection-caller-import-bridge
status: completed
completed_at: 2026-07-19 15:29:57 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
evidence_scope: sub-milestone
---

# 08-S6AD Reflection Caller Import Bridge

## 状态与产出记录

- 完成时间：2026-07-19 15:29:57 +08:00
- 状态：08 generic reflection caller import 子里程碑完成；08 generic reflection/method invoke 里程碑仍为
  部分完成。
- 完成项目：普通与 guard import 均可按实际 caller 的 canonical owner root 解析 runtime-bound
  `zr.reflection` service；同一 target 复用、不同 target 隔离，且不写入 process-global path cache。
- 计划映射：08-S6AD；对应里程碑 5“generic reflection/method invoke”。

## 产出

- `import zr.reflection` 先从真实调用链取得 caller function，再在 loaded module registry 中寻找拥有相同
  canonical owner root 的 READY metadata runtime，最终复用 target-owned reflection service。
- 一个 target module 的多个 path alias 允许命中同一 runtime；两个不同 modules 命中同一 caller root 时
  fail-closed，避免跨 module 泛型上下文复用。
- owner chain 设 1024 层上限；self-cycle、双节点 cycle、未知 caller、缺失 caller 和不完整 runtime 均返回 null。
- bridge 返回后 loader 从 stack anchor 和 call chain 重新获取 path/caller，再执行原 import signature verifier；
  OOM full-GC retry 夹具证明不重新获取 caller 时会错误放行。

## 验证与边界

- 动态泛型反射目标在 MSVC 19.44、GCC 11.4、Clang 14.0 下均为 35/0；改动文件 GCC/Clang warning 为 0。
- MSVC metadata/reflection CTest 为 6/6；共享回归为 GC 66/0、instruction execution 31/0、instructions 95/0。
- 独立复审闭合 embedded-NUL path、malformed registry、bucket cycle、GC stale caller、托管测试 root 与 owner
  cycle 覆盖，最终无 Critical/Important。
- generic invoke thunk、declaration reflection、cross-module dedup/stripping roots 和 full-AOT closure 仍待后续阶段。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ad-10-s4z51-11-s5i-reflection-caller-import-bridge.md`
