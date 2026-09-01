---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.24: Import-Chain Terminal Identity

## Scope

Move terminal members of an imported module-link chain from request-time AST
recursion to the parser `SymbolAt` external identity already published by the
semantic snapshot. This slice covers the terminal member query and its shared
metadata projection. Intermediate module hops and completion remain pending
because they do not yet publish an equivalent canonical member fact.

## TDD And Implementation

The RED test removed `analyzer->ast` after the module-link fixture had been
analyzed. The old `ResolveAtRange` chain path could not resolve
`system.console.printLine`. The same test also changed the canonical external
signature hash and required the query to fail closed instead of recovering from
the AST or member spelling.

`lsp_external_metadata_identity.c` now exposes a structured member resolver. It
requires a module prototype owner and selects exactly one row matching the
parser identity tuple: owner identity, external target kind, metadata token,
signature token, and signature hash. The existing provider is used only to
hydrate the already selected row, and the final payload is checked against the
same tuple. No target is authorized by a name lookup alone.

`lsp_semantic_query.c` performs this consumer before the AST/import-chain gate.
Non-module owners remain not applicable and continue through their established
consumers. A module owner with missing, ambiguous, stale, or mismatched
identity is invalid and returns false. Existing native imported-member behavior
and legacy intermediate-chain behavior remain unchanged.

## Verification

The same isolated source snapshot was built and run from:

- source: `/home/hejiahui/.cache/zr-lsp-inline-417-src`;
- GCC: `/home/hejiahui/.cache/zr-lsp-inline-417-gcc`;
- Clang: `/home/hejiahui/.cache/zr-lsp-inline-417-clang`.

GCC and Clang focused target builds and accepted serial runs completed with real
exit 0:

- semantic query symbols `24/24`;
- semantic query `30/30`;
- semantic query calls `30/30`;
- semantic query relations `29/29`;
- semantic query contract `6/6`;
- canonical consumers `21/21`;
- semantic facts `17/17`;
- type inference `124/124`;
- semantic-query parity;
- LSP source contracts;
- complete LSP interface.

The final GCC and Clang source-contract runs and the final Clang interface run
were also real exit 0. MSVC, the complete repository 16-target matrix, and
stdio/CLI smoke were not run for this focused submilestone and remain Task 8
work.

## 状态与产出记录

- 完成时间：2026-09-01 17:28 +08:00。
- 状态：Task 3.24 import-chain terminal identity consumer子里程碑已完成；
  Plan 03 Task 3与Task 8继续进行。
- 完成项目：AST-independent terminal module-link member query；exact external
  metadata row selection；identity mismatch/ambiguity fail-closed；native
  imported-member regression；GCC/Clang focused semantic and LSP gates；source
  contract coverage。
- 后续项目：canonical intermediate module-hop producer与completion consumer；
  virtual declaration URI；真实multi-provider nonzero generation；binary/native
  sourceless relation matrix；MSVC、完整矩阵与stdio smoke。
