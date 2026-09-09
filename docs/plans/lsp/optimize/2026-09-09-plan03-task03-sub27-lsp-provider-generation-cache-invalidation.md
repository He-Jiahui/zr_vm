---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_scope_cache.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_cross_snapshot_external_reference_cases.h
  - tests/language_server/test_lsp_analysis_provider_generation_cases.h
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.27: LSP Provider Generation Cache Invalidation

## Contract

The LSP context provider generation is now an analyzer input rather than a
counter used only by snapshots and metadata validation. Every analyzer stores
the generation that its next analysis must publish. LSP analyzer lookup and
project analysis synchronize this value with the context. When it changes,
the whole-document cache and scoped query analyzer are invalidated, and the
old borrowed semantic context is hidden until the next analysis rebuilds it.

`PrepareState` writes the stored generation into the new parser semantic
context before semantic facts are published. Snapshot detach preserves the
live analyzer's generation, and a new scoped query analyzer inherits it.
Direct parser/analyzer callers continue to use generation zero unless an LSP
host supplies a nonzero value.

## Regression

The parity harness adds a same-AST provider-change case. It warms whole-document
and scoped caches, verifies a cache hit, increments the LSP provider generation,
and requires the next project analysis to execute again with a fresh scoped
analyzer. It then checks that external references from the rebuilt analyzer and
its new scoped analyzer carry the current generation. Existing binary/native
cross-snapshot cases also require every external canonical symbol to carry the
current nonzero context generation.

## Verification

The initial GCC parity run was RED with three deterministic failures: binary
and native external facts carried generation zero, and same-AST project
analysis reused its cache after `ProviderChanged`. After the LSP wiring change,
the focused target built and ran successfully:

```text
cmake --build /home/hejiahui/.codex-builds/l8-callable-value-gcc \
  --target zr_vm_language_server_semantic_query_parity_test --parallel 16
/home/hejiahui/.codex-builds/l8-callable-value-gcc/bin/zr_vm_language_server_semantic_query_parity_test
23/23 cases passed; process exit 0
```

This record does not claim the full Plan 03 or Plan 08 matrix. Real provider
reload, multi-project generation, sourceless virtual declaration URIs, Clang/
MSVC parity, and the parent acceptance gates remain pending.

## 状态与产出记录

- 完成时间：2026-09-09 +08:00。
- 状态：Task 3.27 已完成；Plan 03 Task 3、7、8 整体门槛保持未完成。
- 产出：LSP analyzer generation propagation, same-AST cache invalidation,
  scoped generation inheritance, binary/native generation regressions and
  module/plan documentation.
- 后续：接入真实 provider reload 与多项目 generation，再补齐 virtual URI
  producer 和跨工具链完整验收。
