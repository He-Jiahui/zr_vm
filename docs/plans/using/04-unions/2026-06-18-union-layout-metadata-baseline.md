---
plan_id: using-04-unions
record_id: 2026-06-18-union-layout-metadata-baseline
status: completed
completed_at: 2026-06-18 15:28 +08:00
source_plans:
  - docs/plans/using/04-union-types.md
  - docs/module-system/typed-module-metadata.md
evidence_scope: historical-baseline
---

# Union Layout And Metadata Baseline

## 可复用结论

- local union TypeDef metadata can carry variants, payload fields, canonical field TypeSig inputs and layout identity/hash.
- parser/CFG infrastructure has union switch/exhaustiveness and typed catch/branch coverage that can be reused by `if let`/`switch` planning.

## 证据入口

- `tests/module/test_metadata_token_model.c`
- `tests/parser/test_cfg_union_exhaustiveness.c`
- `tests/parser/test_type_inference.c`

## 不继承的完成声明

Union pattern matching no longer uses `using`. Target pattern Place projections, partial move/drop flags and exhaustive CFG cleanup must be revalidated under syntax 01/02/04.

