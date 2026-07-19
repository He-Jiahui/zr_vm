---
plan_id: aot-11-metadata
record_id: 2026-07-19-runtime-bound-reflection-module
status: completed
completed_at: 2026-07-19 09:50:58 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
evidence_scope: sub-milestone
---

# 11-S5G Runtime-Bound Reflection Service Module

## 状态与产出记录

- 完成时间：2026-07-19 09:50:58 +08:00
- 状态：11 runtime-bound reflection service module 子里程碑完成；11 ModuleIdentity/registration 里程碑仍为部分完成。
- 完成项目：建立 process-local reflection service module 与 target metadata runtime 的显式绑定边界，保持
  target module、service module 和持久 metadata identity 三者分离。
- 计划映射：11-S5G。

## Metadata 与生命周期约束

- service module 不附加 target 的 `SZrMetadataRuntime`，也不创建/修改 `.zrp` row、token、signature、
  code-registration slot 或 manifest export。
- target runtime 必须满足 `runtime->module` 与 `ZrCore_Module_GetMetadataRuntime(module)` 双向一致；closure capture
  保存 module object，持久 identity 不使用 runtime pointer。
- 内部 unpinned closure factory 仅供原子模块构造；持久 `NATIVE_HANDLE` pin 延迟到 public export 安装并回读成功后。
- global registry/cache owner、module generation、replacement/unload 和 ABI/version descriptor 均明确留待下一阶段。

## 验证与未完成边界

- 最终 MSVC/GCC/Clang 动态泛型 33/0；新增 source 无 GCC/Clang 自身诊断。
- 最终 MSVC 聚焦 CTest 6/6，共享回归 GC 66/0、instruction execution 31/0、instruction table 95/0。
- 复审确认失败清理、移动式 GC pointer reload 和测试根化顺序均已闭合，无剩余 Critical/Important。
- Canonical ModuleIdentity、runtime-specific cache key、registration replacement/unload 与 artifact ABI 仍待后续阶段。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6ab-10-s4z49-11-s5g-runtime-bound-reflection-module.md`
