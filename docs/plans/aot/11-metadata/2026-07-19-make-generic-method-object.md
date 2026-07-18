---
plan_id: aot-11-metadata
record_id: 2026-07-19-make-generic-method-object
status: completed
completed_at: 2026-07-19 06:12:26 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/10-reflection.md
evidence_scope: sub-milestone
---

# 11-S5D Existing MethodSpec Make Consumer

## 状态与产出记录

- 完成时间：2026-07-19 06:12:26 +08:00
- 状态：11 MethodSpec consumer 组合入口子里程碑完成；11 统一 artifact 里程碑仍为部分完成。
- 完成项目：以 MethodDef token 和 argument descriptor vector 查询既有 MethodSpec，并把解析结果直接交给已验证的
  constructed method object builder。
- 计划映射：11-S5D。

## Metadata 约束与验证

- 不按名称查找，不创建 MethodSpec row，不修改 signature blob、token record 或 registration ABI。
- invalid MethodDef identity、arity/type mismatch 或空输入稳定返回 null。
- MSVC/GCC/Clang 动态泛型 30/0，聚焦 metadata/reflection CTest 6/6；本切片无 metadata schema 变化。

## 未完成边界

- 跨模块 MethodSpec binding 和 Canonical ModuleIdentity。
- 新 artifact 的 Canonical TypeNode、FFI/DebugMap contracts 与 schema migration。

## 验收入口

- `tests/acceptance/2026-07-19-aot-08-s6y-10-s4z46-11-s5d-make-generic-method-object.md`
