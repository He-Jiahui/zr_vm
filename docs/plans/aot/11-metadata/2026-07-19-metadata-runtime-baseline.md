---
plan_id: aot-11-metadata
record_id: 2026-07-19-metadata-runtime-baseline
status: completed
completed_at: 2026-07-19 03:53 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/module-system/typed-module-metadata.md
  - docs/plans/aot/07-12-codegen/2026-07-19-08-s6w-10-s4z44-11-s5b.md
evidence_scope: historical-baseline
---

# Metadata Runtime Baseline

## 可复用结论

- `.zro` 已有 metadata token records、signature blob、string pool、TypeDef/TypeRef/TypeSpec/MemberDef/MemberRef/MethodSpec-shaped identities。
- attached metadata runtime 能查询 type layout、field/member binding、generic parameter ranges、manifest exports 和 code registration。
- provider binding 已有 assembly identity/version range、module signature and token remap checks，并对 stale/malformed metadata fail closed。
- MethodSpec request resolution已验证underlying MethodDef identity、owner range、argument count/order与完整recursive signature hash/shape，不以名称近似匹配。

## 证据入口

- `tests/module/test_metadata_runtime_query.c`
- `tests/module/test_metadata_runtime_binding_compatibility.c`
- `tests/module/test_metadata_runtime_manifest_exports.c`
- `tests/library/test_project_import_provider_version_selection.c`
- `tests/acceptance/2026-07-19-aot-08-s6w-10-s4z44-11-s5b-methodspec-request-resolution.md`

## 不继承的完成声明

新 artifact schema 仍需增加 Canonical TypeNode、ref/readonly/owner/resource/callable/FFI contracts、stable ModuleIdentity 和 DebugMap。旧扁平 TypeRef 或私有字符串 sidecar 不能作为目标格式。
