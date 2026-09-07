---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck_bindings.c
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_local_binding_identity_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_semantic_query_parity.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: module-detail
---

# LSP Typecheck Canonical Local Bindings

## Purpose

The language-server typecheck pass enriches the parser-owned semantic snapshot
with inferred binding types. For a source local, this pass must attach the
existing `SymbolId` and `TypeId` to the type environment so identifier reads,
hover, navigation, references, highlights, and rename all observe one binding.

## Binding and Range Contract

Symbol collection publishes a variable symbol with the declaration node as its
location and the identifier pattern as its selection range. The declaration node
therefore begins at the `var` keyword, while `LookupAtPosition` expects a
position at or after the symbol selection range. Typecheck normalizes ordinary
identifier declarations to the pattern range before looking up the canonical
symbol. Parameter and foreach bindings retain their existing node ranges.

When the symbol has valid semantic identities, typecheck calls
`ZrParser_TypeEnvironment_RegisterCanonicalVariable` with the symbol's exact
selection range. This keeps subsequent parser reference facts tied to the
published declaration. A missing identity still uses the existing runtime-only
fallback; it must not be used to replace a source symbol that can be resolved.

## Lifetime and Exactness

`SymbolId` and `TypeId` are valid only in the current semantic context. The
typecheck environment stores the copied ids and declaration range; it does not
retain a borrowed symbol record. Query consumers must continue to resolve the
copied id through the same snapshot and fail closed when the identity or source
does not match. No request-time name, range, AST, or display-text matching is
part of this contract.

The range correction is a written exception for a narrow edit in the existing
large typecheck file. It changes one binding lookup input without adding a new
production responsibility; the regression fixture lives in a separate header.

## Regression Coverage

The analyzer regression covers inferred and explicitly typed locals, plus nested
same-name locals. It checks declaration/read roles, shared ids and type ids,
declaration-node identity, exact declaration range, and the absence of duplicate
canonical records. The LSP interface tests exercise the same identity through
structured local query and hover payloads. Parser symbol/reference tests and
source snapshot parity retain their lower-layer coverage.
