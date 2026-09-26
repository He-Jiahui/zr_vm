---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck_bindings.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_declaration_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_declaration_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_local_binding_identity_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/parser/test_semantic_declaration_binding_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
  - .codex/plans/20260926-lsp-experience-repair.md
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
location and the identifier pattern as its selection range. Typecheck resolves
the existing record by the same AST declaration and binding name within the
current snapshot. It does not locate a declaration by a position or a same-name
symbol in another scope.

The shared declaration-binding module reuses this identity when inserting the
binding into the current lexical type environment. The environment retains the
declaration's exact selection range. Source bindings without an exact type keep
a valid `SymbolId` and an invalid `TypeId`; they remain discoverable and surface
`cannot infer exact type`. Source declarations without a canonical identity no
longer create anonymous replacement bindings during typecheck.

## Lifetime and Exactness

`SymbolId` and `TypeId` are valid only in the current semantic context. The
typecheck environment stores the copied ids and declaration range; it does not
retain a borrowed symbol record. Query consumers must continue to resolve the
copied id through the same snapshot and fail closed when the identity or source
does not match. No request-time name, range, AST, or display-text matching is
part of this contract.

Declaration registration is extracted into `semantic_analyzer_declaration_binding.c`
and `type_environment_bindings.c`. Return pre-inference, symbol collection and
typecheck share the same AST declaration identity. See
[canonical declaration bindings](canonical-declaration-bindings.md) for the
pre-inference, anonymous binding and unknown-type contracts.

## Regression Coverage

The analyzer regression covers inferred and explicitly typed locals, plus nested
same-name locals. It checks declaration/read roles, shared ids and type ids,
declaration-node identity, exact declaration range, and the absence of duplicate
canonical records. The LSP interface tests exercise the same identity through
structured local query and hover payloads. Parser symbol/reference tests and
source snapshot parity retain their lower-layer coverage.
