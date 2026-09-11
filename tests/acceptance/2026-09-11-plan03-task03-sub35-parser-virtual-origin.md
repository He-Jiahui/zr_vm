---
plan_id: optimize
task: plan03-task03-sub35
status: partial-acceptance
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Task 3.35 Acceptance Evidence

## Scope

This record covers the parser-owned publication of metadata-projected virtual
declaration URIs for import-origin relation facts and the project/no-project
LSP analysis wiring needed to install the resolver during compilation. It does
not accept binary virtual documents, multi-definition relations or the parent
Task 3/7/8 gates.

## Test Inventory

- Parser direct-import regression: a callback maps `zr.math` to
  `zr-decompiled:/zr.math.zr`; the compiled relation must retain that URI.
- LSP native import regression: a request with its AST detached resolves the
  canonical native declaration and verifies the relation URI; missing origin,
  missing relation, missing metadata, mismatched URI and ambiguous origin all
  fail closed.

## Tooling Evidence

The fresh ext4-backed GCC build used the existing validation CMake tree and
skipped only its ignored glob reconfiguration step. Compilation completed with
the repository's existing descriptor initializer warnings and no new compiler
errors. The parser target exited 0 with 29/29 tests. The full interface target
exited 1 because its two established baseline failures remain; the new native
canonical-origin case passed. The project target was interrupted during its
known long circular-import baseline run and is intentionally not counted.

## Acceptance Decision

The focused producer/lifecycle slice is accepted and ready for a scoped commit.
Clang parser and interface reruns are included above; the Clang interface run
retains the same two functional baseline failures and its existing LSan report.
MSVC, the full project runner, sanitizer-clean accounting and the 16-target
gate remain pending. The existing interface baseline failures are recorded
rather than attributed to this change.
