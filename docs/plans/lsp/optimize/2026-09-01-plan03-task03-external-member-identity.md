---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_target_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_token_canonical.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.20: External Member Identity

## Scope

Publish canonical identity for imported module-chain members that have no source
declaration range, then make LSP navigation and semantic tokens consume that
identity without a member-name, signature-text, or AST fallback.

## TDD And Implementation

The parser RED fixed a native root-to-leaf module chain and required repeated
module and callable uses to return the same stable SymbolId, canonical TypeId,
owner identity, metadata token, signature token, signature hash, and target
kind. The public reference fact and `SymbolAt` query now carry those fields.
Imported source/runtime/io/summary rows also preserve their canonical owner and
base-definition identities, so the producer can publish a complete external
target instead of an unresolved same-name member.

Reference-fact append owns a clone of the external owner string. Query strings
and pointers remain borrowed for the semantic snapshot lifetime. Provider
generation is projected unchanged; zero explicitly means unavailable and must
not be reconstructed from provider load order, URI, or module spelling.

The LSP metadata provider attaches the exact canonical member row to a resolved
candidate. Source declaration URI/range remains preferred when present. Without
that range, `lsp_external_target_identity.c` requires owner identity, metadata
token, signature token, signature hash, and target kind to match the parser
query. A deliberately changed signature hash returns no navigation result.

Semantic token classification now reads the external target kind from
`SymbolAt`. The token scanner's metadata import-chain lookup, member-name kind
reconstruction, and fallback preference logic were removed. Module members are
classified as namespaces and callable members as methods only after canonical
identity resolves.

## Verification

On isolated GCC and Clang builds, each focused executable completed with real
exit 0:

- symbols `24/24`, query `30/30`, calls `30/30`, relations `28/28`;
- query contract `6/6`, canonical consumers `21/21`, semantic facts `17/17`;
- type inference `124/124`;
- LSP semantic-query parity and all source-contract checks passed.

Both interface executables retained expected real exit 1 with exactly one
remaining failure: `LSP Project References Include Imported Function Usage`.
The module-link-chain navigation and import-chain semantic-token cases pass on
both toolchains. The remaining case belongs to the next cross-snapshot project
reference milestone and was not hidden by a marker allowance.

MSVC, the complete target matrix, and stdio smoke were not run for this focused
submilestone. Plan 03 Task 3 still needs virtual declaration URIs, nonzero
multi-provider generation coverage, and the broader source/binary/native
relation matrix.

## 状态与产出记录

- 完成时间：2026-09-01 12:44 +08:00。
- 状态：Task 3.20 external member identity子里程碑已完成；Plan 03 Task 3、Task 7和
  Task 8继续进行。
- 完成项目：parser external target fact/query合同；source/binary/native imported row owner
  projection；LSP exact metadata-row matcher；module-link navigation；canonical semantic token
  分类；删除scanner metadata/name fallback；GCC/Clang focused门禁。
- 后续项目：用同一identity完成project cross-snapshot references；补virtual declaration URI和
  nonzero provider generation；执行MSVC、完整矩阵与stdio smoke总门禁。
