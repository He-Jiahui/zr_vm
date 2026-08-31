---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations_identity.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations_order.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_relation_provider_generation_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.18: Relation Provider Generation Identity

## Scope

Make provider generation part of canonical external relation identity. Module
identity alone cannot distinguish the same declaration edge published by two
generations of one binary, native, or project provider.

## TDD And Implementation

The RED constructed one external base-type edge twice with source generation 3
and target generation 10, then appended the same module/type edge with target
generation 11. Compilation failed because relation facts and public queries did
not expose either generation.

`SZrSemanticRelationFact` and `SZrParserSemanticRelationQuery` now carry source
and target provider generations. A nonzero generation participates in fact
equality and stable query ordering. Exact duplicates within one generation
collapse, while the generation-10 and generation-11 edges remain distinct and
are returned in deterministic order. Zero explicitly means unavailable; no
consumer may reconstruct it from names, URIs, source text, or provider load
order.

The test also verifies that canonical source and target module identities remain
projected alongside both generation values.

## Verification

On isolated GCC and Clang builds, each executable completed with real exit 0:

- semantic-query relations pass `24/24`;
- general query passes `30/30`, calls pass `30/30`, and symbols pass `23/23`;
- LSP semantic-query parity passes `15/15`;
- all 70 LSP source-contract checks pass.

Both interface executables retain expected real exit 1 and exactly the existing
fixed3 failures: project imported-function references, module-link-chain member
references, and import-chain semantic tokens. This change introduces no new
interface failure. Those cases still require the import metadata producer to
publish cross-snapshot declaration identity; this milestone does not add an LSP
name fallback or modify the Syntax05-owned import paths.

MSVC, the full 16-target matrix, and the three stdio smoke suites were not run
for this narrow parser query milestone.

## 状态与产出记录

- 完成时间：2026-08-31 14:41 +08:00。
- 状态：Task 3.18 子里程碑已完成；Plan 03 Task 3、Task 7与Task 8继续进行。
- 完成项目：relation source/target provider generation公共合同；generation-aware去重与稳定
  排序；query原样投影；GCC/Clang focused门禁；interface fixed3 delta 0。
- 后续项目：等待canonical import producer发布跨snapshot declaration identity；补齐Task 3
  多定义、partial/extern/native/binary及多项目矩阵，并执行MSVC、16-target与stdio总门禁。
