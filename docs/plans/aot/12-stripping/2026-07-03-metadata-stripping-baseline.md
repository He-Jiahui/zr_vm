---
plan_id: aot-12-stripping
record_id: 2026-07-03-metadata-stripping-baseline
status: completed
completed_at: 2026-07-03 23:38 +08:00
source_plans:
  - docs/plans/aot/12-code-stripping.md
evidence_scope: historical-baseline
---

# Metadata And Code Stripping Baseline

## 可复用结论

- opt-in AOT stripping 已有 function/type/member/token reachability、metadata pool pruning、token remap、manifest preserve/export roots 与 size diagnostics 基线。
- malformed or orphan retained metadata references已有 fail-closed validation tests。
- reflection annotations and dynamic dependency roots 已有部分 compiler-to-writer carrier。

## 证据入口

- `tests/parser/test_aot_c_code_stripping.c`
- `tests/parser/test_aot_c_zrp_metadata_pruning.c`
- `tests/parser/test_aot_c_zrp_metadata_methodspec_pruning.c`
- `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c`

## 未完成边界

新计划必须按 public Canonical Type/Callable/FfiSignature/DebugMap contracts 重新建立 closed-world reachability；不能因旧 token row 被保留就推断新语义可用。

