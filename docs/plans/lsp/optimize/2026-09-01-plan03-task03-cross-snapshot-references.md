---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_external_metadata_identity.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.h
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/fixtures/projects/import_pub_function/src/secondary.zr
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.21: Cross-Snapshot Project References

## Scope

Complete project references for imported source callables by joining parser
external reference facts to exact metadata declaration identity across semantic
snapshots. Both source declarations and imported uses must produce the same
reference set without member-name, signature-text, or AST reconstruction.

## TDD And Implementation

The parser RED required a public external-reference query and failed to compile
before the API existed. `ZrParser_SemanticQuery_ExternalReferences` now returns
only resolved facts with complete owner, metadata token, signature token,
signature hash, and target kind. It copies value fields into a caller-owned
array, keeps owner strings borrowed from the snapshot, filters incomplete
facts, and uses a stable source/range/token/SymbolId ordering.

Typed source export, runtime import, IO import, and summary function rows now
preserve exact declaration coordinates in `SZrTypeMemberInfo`. The LSP exact
identity resolver maps the parser tuple to one metadata row and then to one
project source declaration. Missing or duplicate rows fail closed. Nonzero
provider generation must match the current provider generation; zero remains
explicitly unavailable and is accepted without guessing a generation.

The cross-snapshot aggregator obtains each project source analyzer through the
snapshot-aware cache, calls the parser query, resolves every tuple, and appends
only references whose declaration URI and full range equal the starting
declaration. It uses existing cancellation and deduplication helpers. The
source-contract test rejects import-binding collection, member-name matching,
AST declaration matching, and signature-text reconstruction in the new
modules.

The interface fixture adds a second importer. The tightened RED showed that a
source declaration query already returned the declaration and both imported
uses, while a query started at the first imported use returned only the
declaration and current-file use. The imported-member path now projects its
already-resolved declaration URI/range into the same cross-snapshot query, so
both directions return all three exact locations.

## Verification

The isolated source snapshot was built and run from:

- source: `/home/hejiahui/.cache/zr-lsp-inline-417-src`;
- GCC build: `/home/hejiahui/.cache/zr-lsp-inline-417-gcc`;
- Clang build: `/home/hejiahui/.cache/zr-lsp-inline-417-clang`.

Both toolchains completed the same focused executables with real exit 0:

- symbols `24/24`, query `30/30`, calls `30/30`, relations `28/28`;
- query contract `6/6`, canonical consumers `21/21`, semantic facts `17/17`;
- type inference `124/124`;
- semantic-query parity and all LSP source-contract checks;
- the complete LSP interface executable, including declaration-to-two-importer
  and importer-to-two-importer reference assertions.

The previous final interface failure is removed on both GCC and Clang: fixed1
became zero. MSVC, the complete repository target matrix, and stdio smoke were
not run for this focused submilestone.

## 状态与产出记录

- 完成时间：2026-09-01 14:00 +08:00。
- 状态：Task 3.21 cross-snapshot project references子里程碑已完成；Plan 03
  Task 3与Task 8继续进行。
- 完成项目：parser external-reference value query；typed export/import exact
  declaration coordinates；LSP exact metadata identity resolver；snapshot-aware
  project reference aggregation；双importer双向references回归；禁止name/text/AST
  fallback的source contract；GCC/Clang focused门禁。
- 后续项目：virtual declaration URI；真实multi-provider nonzero generation；
  binary/native无source definition矩阵；MSVC、完整矩阵与stdio smoke总门禁。
