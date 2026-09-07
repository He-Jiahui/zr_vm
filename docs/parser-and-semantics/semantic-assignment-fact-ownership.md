---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_assignment_reference_diagnostic_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: module-detail
---

# Semantic Assignment Fact Ownership

## Analysis and Query Contract

LSP analysis submits the entire assignment expression to parser type inference.
`ZrParser_AssignmentType_Infer` infers the right operand, resolves the target
binding, and publishes the target's write reference before checking assignment
compatibility. The source remains a read reference. A rejected assignment can
therefore retain resolved reference identities for navigation and highlighting
without publishing the assignment as a successful exact expression.

The parser uses `ZrParser_AssignmentCompatibility_CheckDetailed` for ownership,
nullable owner, basic type, named constraint, contiguous view, and move-only
compatibility. Diagnostic primary ranges identify the right operand. Related
expected-type locations use the bound canonical declaration: an explicit local
annotation when available, otherwise the binding declaration range, or the
target expression range when no declaration range exists. This is analysis-time
AST access through a resolved binding, not a request-time name scan.

The LSP analyzer consumes the compiler's structured diagnostic through its
existing projection helper and retains the separate parser const-assignment
validator. It no longer infers each assignment operand independently or performs
a second compatibility check. The obsolete LSP expected-type lookup module is
removed. Initializer and return analysis still call the shared compatibility
API directly; their wider migration is outside this slice.

Reference and highlight queries consume the snapshot's canonical facts. They do
not recover assignment roles from AST shape or retained tracker entries.
Declaration and write facts continue to project as LSP Write highlights, and
read facts as Read highlights. Missing or stale identity keeps the existing
fail-closed behavior.

## Lifetime and Exactness

Bindings, semantic symbols, expression facts, and reference facts belong to the
same compiler semantic context. The producer copies the expected declaration
range before normalization or fact publication can intern types. No borrowed
symbol record survives that publication. Copying coordinates does not preserve
identity across a context reset or document update.

The analyzer initializes and frees the temporary inferred assignment type on
both success and failure. Published facts and structured diagnostics remain
owned by the semantic context. No new cache or public API is introduced.

## Evidence and Regression

The local Rust `compiler/rustc_hir_typeck/src/expr_use_visitor.rs` treats
`Assign(lhs, rhs, _)` as mutation of the left operand and consumption of the
right. Roslyn's `src/Features/Core/Portable/DocumentHighlighting/`
`AbstractDocumentHighlightsService.cs` projects `IsWrittenTo`, and its
`src/LanguageServer/ProtocolUnitTests/Highlights/DocumentHighlightTests.cs`
tests assignment writes separately from reads. The JDK compiler's
`src/jdk.compiler/share/classes/com/sun/tools/javac/comp/Attr.java` attributes
assignment targets in assignment context and checks the source against the
target type. All three source trees are under `lua/` in this repository.

ZR retains its existing declaration-as-Write convention; Roslyn's referenced
fixture uses Text for the declaration. No language syntax or ownership rule is
changed here.

Parser regressions cover ownership mismatch, scalar mismatch with precise
related annotation range, incompatible nominal objects, and null to a
nonnullable owner, while checking source/target reference roles after rejection.
Existing reference tests cover member and computed targets and reaching writes.
The LSP parity regression detaches the legacy tracker and projected symbol,
checks one read/two writes, replaces the document, and checks one read/three
writes. Source contracts prevent independent child inference and compatibility
checks from returning to the LSP assignment branch.

These are narrow changes to existing assignment responsibilities in two large
production files. This is the written exception to splitting those files for
this correction. New regression helpers live in a separate test header; the
removed LSP helper reduces the number of owners of assignment diagnostics.
