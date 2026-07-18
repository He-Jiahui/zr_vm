---
plan_id: aot-11-metadata
record_id: 2026-07-19-constructed-generic-method-object
status: completed
completed_at: 2026-07-19 06:00:49 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
evidence_scope: sub-milestone
---

# 11-S5C MethodSpec Reflection Consumer

## 状态与产出记录

- 完成时间：2026-07-19 06:00:49 +08:00
- 状态：11 既有 MethodSpec metadata 的反射消费子里程碑完成；11 统一 artifact 里程碑仍为部分完成。
- 完成项目：将既有 MethodSpec row/signature、underlying MethodDef、GenericParam owner range 和 string-pool name 组合为
  constructed generic method reflection object，不引入平行 metadata identity。
- 计划映射：11-S5C。

## Metadata 约束

- 构造前调用精确 MethodSpec resolver，并核对 token、record pointer、signature hash、argument list offset/count 与
  request arguments；任何漂移均拒绝物化。
- MethodSpec context 继续提供 signature arguments 和 runtime carrier，MethodDef definition object 继续提供真实名称和
  GenericParam 声明；constructed object 仅组合既有事实源。
- 未修改 `.zrp`/`.zro` schema、row、section、token 编码或 code-registration ABI。

## 验证与边界

- MSVC/GCC/Clang 动态泛型反射 29/0，聚焦 metadata/reflection CTest 6/6。
- 三编译器共享回归为 GC 66/0、instruction execution 31/0、instruction table 95/0。
- Canonical TypeNode artifact、stable ModuleIdentity、跨模块 MethodSpec binding、FFI/DebugMap sections 仍按 11 主计划
  实施。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6x-10-s4z45-11-s5c-constructed-generic-method-object.md`
