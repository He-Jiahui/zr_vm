---
plan_id: using-03-metadata
record_id: 2026-06-21-ownership-metadata-baseline
status: completed
completed_at: 2026-06-21 12:42 +08:00
source_plans:
  - docs/plans/using/03-metadata-and-token-model.md
evidence_scope: historical-baseline
---

# Ownership Metadata Baseline

## 可复用结论

- metadata token/signature infrastructure can represent ownership-shaped TypeSpec nodes and cross-module assembly references.
- provider identity, version ranges, TypeDef layout hash and selected TypeSpec bindings have roundtrip and stale-binding guards.
- `.zrm` already packages multiple `.zro` module payloads plus manifest/resources.

## 证据入口

- `tests/module/test_metadata_token_model.c`
- `tests/library/test_zrm_container.c`
- `tests/library/test_project_import_resolver.c`
- `docs/module-system/zrm-assembly-container.md`

## 未完成边界

Target owner/ref/readonly/resource/drop and FFI signature nodes must be encoded structurally from Canonical TypeRef. Existing ownership strings or legacy import aliases are migration input only.

