---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.25: Import-Chain Module Hop Identity

## Scope

Move an intermediate imported module-link segment such as `system.console`
from request-time AST recursion to the parser `EXTERNAL_TARGET_MODULE`
reference fact. This slice covers position query resolution, exact metadata
projection, and fail-closed behavior for stale or conflicting identity. It
does not add a parser producer, completion support, or cursor reconstruction.

## TDD And Implementation

The RED fixture analyzed a module-link chain and then detached `analyzer->ast`
before querying the `console` segment. The query initially failed because the
external identity adapter treated module targets as unsupported. The same
fixture changed the canonical signature hash and required the AST-independent
query to reject the tampered identity.

The metadata identity adapter now recognizes a module target only when the
selected metadata row projects a module prototype. It requires the canonical
owner identity, target kind, metadata token, signature token, and signature
hash to match exactly one row. The provider's existing member lookup is used
only after that exact row has been selected to hydrate its structured payload;
the row identity, not its spelling, authorizes the result.

`lsp_semantic_query.c` invokes this consumer after the canonical `SymbolAt`
query and before the AST/import-chain gate. It projects the resolved module
member and its provider payload without requiring a request-time AST. Missing,
ambiguous, stale, or tampered identity returns no result and cannot fall back
to module or member text.

## Verification

The implementation was synchronized into the isolated source snapshot:

- source: `/home/hejiahui/.cache/zr-lsp-inline-417-src`;
- GCC build: `/home/hejiahui/.cache/zr-lsp-inline-417-gcc`;
- Clang build: `/home/hejiahui/.cache/zr-lsp-inline-417-clang`.

GCC and Clang focused runs completed with real process exit 0. The new
AST-independent module-hop test, its tampered-identity fail-closed assertion,
the existing terminal imported-member case, the source-contract suite, and
the complete LSP interface suite all passed. The existing focused semantic
query, canonical consumer, semantic facts, relations, symbols, calls, query
contract, type-inference, and parity gates remained green.

MSVC, the repository-wide 16-target matrix, and the three stdio/CLI smoke
suites were not run for this focused submilestone; they remain Plan 03 Task 8
work. Completion and module-link cursor reconstruction also remain outside
this slice until the producer publishes an exact canonical member range for
those requests.

## 状态与产出记录

- 完成时间：2026-09-01 17:48 +08:00。
- 状态：Task 3.25 import-chain module-hop identity consumer子里程碑已完成；
  Plan 03 Task 3与Task 8继续进行。
- 完成项目：`EXTERNAL_TARGET_MODULE` AST-independent position query；module
  prototype与metadata row exact identity matching；provider structured module
  payload projection；tampered/ambiguous/stale identity fail-closed；GCC/Clang
  focused semantic and LSP gates；source-contract coverage。
- 后续项目：completion consumer与canonical cursor range；virtual declaration
  URI；真实multi-provider nonzero generation；binary/native sourceless relation
  matrix；MSVC、完整矩阵与stdio smoke。
