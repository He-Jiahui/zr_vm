---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
related_module_docs:
  - docs/cli-and-tooling/lsp-cross-snapshot-external-references.md
tests:
  - tests/language_server/test_lsp_cross_snapshot_external_reference_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: milestone-record
---

# Plan 03 Task 7.73: Cross-Snapshot External References

## Failure and Correction

Imported members with exact external metadata identity previously searched
current-document references by SymbolId. Cross-file lookup additionally required
a source declaration range. Binary and native members could therefore omit
another source module's references even though its parser snapshot contained a
complete external reference identity.

The new `AppendExternal` path consumes parser `ExternalReferences` in the current
document and project source import graph. It compares exact owner, generation,
metadata token, signature token/hash and target kind, independently of local
SymbolId and import alias. The existing range projector suppresses duplicates.
The query gate validates hydrated member identity and nonzero provider generation
before emitting declaration/reference locations or same-document highlights.

## Reproduction and Coverage

GCC RED retained all 18 prior parity passes and failed both new cases: binary
and native queries each returned one reference instead of two. Raw output is
`.codex/task773-gcc-red.log`. The fixtures include different import aliases and a
same-named local decoy; the sibling AST, symbol table and reference tracker are
detached before querying its canonical facts.

Eight independent sibling-fact mutations cover missing/mismatched metadata token,
signature token/hash, owner, provider generation, unresolved status and unknown
target kind. Invalid candidates leave only the requesting document's reference.
An incomplete target rejects include-declaration requests; a stale target also
rejects references and highlights.

The first GREEN attempt exposed a test setup gap: binary callable inference
publishes two resolved Call facts at one range. GDB confirmed that mutating only
the first left the second complete identity available. The final test preserves
the fact array and mutates all external facts at the tested range, then restores
them. Raw debugger evidence is `.codex/task773-external-facts-gdb.log`.

## Status and Evidence

- Start: 2026-09-07 20:25 +08:00.
- Completed: 2026-09-07 22:24 +08:00.
- Status: completed for the imported-member cross-snapshot reference slice.
- Source: shared main after `bc49ad11`; concurrent protocol, core, AOT and
  call-binding changes are outside this slice.
- Build commands use `cmake --build <build-dir> --target
  zr_vm_language_server_semantic_query_parity_test
  zr_vm_language_server_lsp_interface_test
  zr_vm_language_server_lsp_project_features_test
  zr_vm_language_server_lsp_source_contracts_test -j 6`.
- Raw logs: `.codex/task773-*`.

MSVC Debug static parity and GCC parity pass all 20 cases with exit 0. The clean
Clang ASan/UBSan build also passes all 20 functional cases; LSan reports the
pre-existing 5069-byte/41-allocation leak set and exits 1. GCC and MSVC interface
each retain three failures: Class Member Navigation And Completion; Hover And
Completion Surface Explicit Exact Type Failures; Container Matrix Project Infers
Bucket And Foreach Types. Concurrent type-use work reduced the previous six-item
set; that improvement is not attributed to this change. Project features retains
the frozen 14 failures. GCC, Clang and MSVC source contracts each retain one
unrelated stdio assertion for `cJSON_CreateString("declaration")`; all new
reference contracts pass.

## Remaining Gates

This slice covers imported member reference queries in the scanned source graph.
Module-entry references, receiver type-member AST adapters, unconnected workspace
files, external rename/implementation/hierarchy and complete Task 3/7/8 acceptance
remain pending. The module document records ownership, borrowed lifetimes,
upstream evidence and the limited edit to the oversized query orchestrator.

The Task 7.72 record and indexes also correct a verification transcription error:
Clang parity passed its 18 functional cases but reported 544 bytes in four LSan
allocations and exited 1. Its old exit-0 statement was not supported by the raw
log; this correction does not change that phase's production code.
