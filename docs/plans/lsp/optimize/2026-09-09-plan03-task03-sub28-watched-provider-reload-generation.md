---
plan_id: optimize
task: plan03-task03-sub28
status: in-progress
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/snapshot/lsp_semantic_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_analysis_provider_generation_cases.h
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.28: Watched Provider Reload Generation

## Contract

Watched binary and descriptor-plugin refreshes now advance the LSP external
provider generation after the replacement project index is installed and before
module loading or open-document reanalysis. The ordinary document refresh
entry point keeps its existing caller-owned generation behavior; only the
watched provider replacement path advances the context.

Analyzer synchronization consumes the new generation during reanalysis. Old
snapshots fail their provider-generation fence. The source lifetime, parser
module-init cache, and canonical hover formatter keep their existing contracts.
The production edit is limited to ordering within project refresh. The large
project file remains intact because moving the refresh orchestration would
broaden this fix; its refresh and importer-reanalysis boundary remains a
modularization follow-up.

## Regression

The project-feature regressions capture the provider generation before each
watched binary and descriptor-plugin reload and require a strictly newer
generation after refresh. Before any completion or hover query, they require an
already-analyzed importer with the current semantic-context generation. A late
generation increment would invalidate the analyzer on lookup and fail this check.
They then require completion, member hover, and importer-local hover to use the
replacement provider facts.

The source, binary, and descriptor-plugin local hover assertions now follow
the existing canonical contract: source `float` is canonical DOUBLE and displays
as `Resolved Type: double`. Each updated hover must reject the old `int` section.
Metadata member completion and hover retain the provider signature spelling.
See [the earlier canonical assertion record](2026-09-07-plan01-task06-sub07-rename-canonical-type-assertions.md).

## Verification

The initial RED run failed the two new watched-reload generation assertions.
The final GCC project-feature run passes all three refresh cases:

```text
LSP Source Module Refresh Reanalyzes Open Documents  PASS
LSP Watched Binary Metadata Refresh Reanalyzes Open Documents  PASS
LSP Watched Descriptor Plugin Refresh Reanalyzes Open Documents  PASS
```

The complete project-feature executable still reports seven unrelated existing
failures in project discovery, imported constructor/meta, relative alias
imports, network semantic tokens, external metadata tokens, native constructor
tokens, and pooling. GCC semantic snapshot CTest remains `1/1`, and the
semantic-query parity executable passes all `21/21` registered test invocations
with process exit 0. This corrects the earlier Sub27 count of 23; current source
and output both contain 21 invocations.

The full project runner exits 1; the result is not inferred from a filtered
pipeline's status. Its remaining cases are:

- LSP Auto Discovers Project From Source File
- LSP Imported Constructor And Meta Call Infer Through Module Type
- LSP Relative And Alias Import Literal Navigation And Hover
- LSP Network Native Members Semantic Tokens Cover Chain And Receivers
- LSP Semantic Tokens Cover External Metadata Members
- LSP Semantic Tokens Cover Native Value Constructor Members
- LSP Pooling Hover Completion And Projection Expose Guard Contract

Local full output is retained under `.codex/lsp-optimize-validation/` as
`plan03-task03-sub28-project-gcc.log` and `plan03-task03-sub28-parity-gcc.log`.

```sh
cmake --build /home/hejiahui/.codex-builds/l8-callable-value-gcc \
  --target zr_vm_language_server_lsp_project_features_test \
  zr_vm_language_server_lsp_semantic_snapshot_test \
  zr_vm_language_server_semantic_query_parity_test --parallel 16
/home/hejiahui/.codex-builds/l8-callable-value-gcc/bin/zr_vm_language_server_lsp_project_features_test
ctest --test-dir /home/hejiahui/.codex-builds/l8-callable-value-gcc \
  --output-on-failure -R '^language_server_lsp_semantic_snapshot$'
/home/hejiahui/.codex-builds/l8-callable-value-gcc/bin/zr_vm_language_server_semantic_query_parity_test
```

Clang/MSVC validation of this change and the full Plan 03 matrix remain pending.
The earlier Clang ASan/UBSan rebuild was stopped after more than 20 minutes;
it is not current validation evidence. Multi-provider generation, sourceless
virtual declaration URIs, multi-definition relations, and the parent Task 3/7/8
acceptance gates also remain pending.

## 状态与产出记录

- 开始时间：2026-09-09 +08:00。
- 实际完成时间：
- GCC 验证时间：2026-09-09 10:20 +08:00。
- 状态：实现与 GCC 窄验证完成；Task 3.28 跨工具链验收进行中，Plan 03 Task 3、7、8
  整体门槛保持未完成。
- 源码版本：`d717af6c` 加本项 project/test 改动和共享工作区既有修改。
- 产出：project watched reload ordering, binary/native generation assertions,
  exact canonical local hover assertions, module documentation and this record.
- 后续：补齐多 provider/多项目 generation、无 source 的 virtual URI producer、
  multi-definition relation matrix 与跨工具链完整验收。
