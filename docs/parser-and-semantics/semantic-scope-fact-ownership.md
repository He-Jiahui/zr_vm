---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/include/zr_vm_parser/semantic.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_scope_symbol_lifetime_cases.h
doc_type: module-detail
---

# Source Scope Fact Ownership

## Purpose

`ZrParser_Semantic_BuildSourceScopeFacts` publishes lexical scopes and visible
declarations into the parser semantic context. Each scope and visible symbol
records its canonical owner so downstream queries can distinguish members,
parameters, locals, and nested bodies without reconstructing lexical ownership.

## Publication and Lifetime

The builder consumes the source AST and declaration symbols already resolved in
the same semantic context. A type scope belongs to its type `SymbolId`; a method
scope, its parameters, and its body scopes belong to the method `SymbolId`.
Class, struct, and interface declarations follow the same ownership rule.

Symbol lookup returns a borrowed record inside `context->symbols`. Registering
another symbol can grow that array and invalidate every borrowed record pointer.
Generic parameter publication can perform exactly this mutation. Consequently,
type and method visitors copy the owner's `SymbolId` before publishing generic
parameters or children, then use the value for all later publication. Free
function and lambda visitors already follow this pattern.

The copied ID is stable only within the owning semantic context. It does not
extend the lifetime of the record or establish identity across snapshot resets,
provider reloads, or different contexts. Published facts remain context-owned
and are released by the existing context reset/free operations.

## Exactness and Failure

The owner is the resolved declaration's canonical ID. Array relocation must not
change it to the invalid ID or to another declaration's ID. The builder does not
repair a lost owner by matching a name, interpreting type display text, or
re-running expression inference. Existing unresolved-declaration and allocation
failure behavior remains the source scope builder's contract.

The correction stays in the two existing visitor functions. It adds no new
production responsibility to the large scope builder. The allocator regression
fixture is in its own test header so its lifecycle machinery does not further
expand the existing symbol-query test file.

## Regression Evidence

Six tests cover generic types and generic methods in classes, structs, and
interfaces. Each fixture fills the symbol array, then installs a test allocator
that forces the next symbol growth to move storage. It copies the records to
the replacement allocation and quarantines the cleared old allocation until
publication finishes. This makes a stale owner read deterministic even when a
platform allocator would normally grow the array in place.

The tests check the exact owner IDs for the field, regular method parameter,
and method body where present. The allocator is restored and both allocations
are released before the final assertions. Before the fix, all six fixtures
lose an owner; after the fix, all pass under GCC, Clang ASan/UBSan, and MSVC.
The Clang full LSP interface also passes the closed-generic signature case that
previously stopped at a heap-use-after-free in the type visitor. Full-suite
acceptance, including other diagnostics and sanitizer reports, is tracked in
the Task 7.68 milestone record.
