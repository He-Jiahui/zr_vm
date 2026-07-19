---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_internal.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_internal.h
plan_sources:
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
tests:
  - tests/parser/test_reference_escape_closure_suspension.c
  - tests/acceptance/2026-07-20-syntax-02-m5-reference-escape-closure-suspension.md
doc_type: module-detail
---

# Reference Escape, Closure, And Suspension

Syntax plan 02 M5 makes reference escape a compile-time contract. A reference
has a maximum safe destination in the ordered lattice `local < function <
caller < heap/static`; `unknown` is never treated as proof that a destination is
safe. VM and AOT receive only programs that pass this gate and do not maintain a
runtime borrow table.

## TypeRef identity

`SZrType` and `SZrInferredType` preserve writable versus readonly reference
access independently from the legacy Borrow/Loan ownership qualifier. The
canonical adapter projects these values to `ZR_CANONICAL_TYPE_REF` with
`WRITABLE` or `READONLY` access. `scoped` limits the source region and therefore
does not create another nominal TypeId.

The parser accepts `ref T`, `ref readonly T`, `scoped ref T`, and `scoped ref
readonly T`. Nested references and invalid `scoped` uses are rejected. A module
alias such as `scoped.Array<T>` remains an ordinary qualified type because the
contextual keyword is recognized only when the next token is `ref`.

## Canonical escape facts

`SZrSemanticEscapeFact` records a stable fact id, operation kind, source RegionId
and PlaceId, source upper bound, target escape, and separate origin/target
ranges. Semantic IR validation checks the ids, enum domains, regions, Places and
target bounds before flow analysis.

Flow analysis emits `ZR_SEMANTIC_FLOW_ESCAPE_VIOLATION` when the target is wider
than the source or the source bound is unknown. Consumers query the diagnostic
by escape fact id and receive both ranges and both lattice states. This keeps an
unknown source conservative without overloading Place alias results.

## Source validation

Both compiler entry points run the reference escape pre-pass before task-effect
validation or bytecode publication. Lexical bindings carry source range,
escape bound, scoped/out/writable flags and suspension epoch. Conditional ref
expressions merge to the stricter bound.

The pre-pass rejects:

- returning `in`, `out`, `scoped ref`, or any function-local reference;
- storing a short reference in a module/global, member, object or container;
- capturing a scoped/out reference in a lambda or local named function;
- returning a closure whose captured reference cannot reach the caller;
- accessing a writable reference while a closure's mutable capture is live;
- using a reference after an `await` or generator suspension.

Every rejection uses a structured diagnostic whose primary range is the escape
or conflicting use and whose related range is the reference origin. Calls
consume the callee closure provenance: a call result does not become an alias of
the closure unless a later callable-return contract explicitly proves that.

## Closure and suspension liveness

A writable capture acts as a mutable loan until the final use of the closure
binding. External access before that use is rejected; access after the closure's
last use is allowed. Lambdas and local named functions share the same capture
rules.

Reference bindings record the suspension epoch at declaration. The transformed
await helper and the current generator suspension node advance the epoch. A
reference used in a later epoch reports the origin and suspension ranges. An
owned value observed before suspension remains legal because it does not carry
reference provenance into the coroutine frame.

## Async and native surface

The parser recognizes target `async fn` without silently wrapping its declared
return TypeRef; legacy `%async` keeps its compatibility lowering. Target
`native extern("library")` requires bodyless declarations to start with `fn`,
while legacy `%extern` remains accepted during migration. Both `async` and
`native` remain contextual, so ordinary identifiers with those names continue
to parse outside their declaration shapes.

A native ref argument has call-local capture by default. Long-lived native
storage requires a future explicit handle/pin contract; M5 never widens an
ordinary ref ABI implicitly.

## Milestone boundary

M5 completes caller/function/heap escape, ref return, closure capture and
suspension rejection for the reference checker. Ref-struct layout and field
capability rules belong to syntax plan 03. The final `async fn` runtime carrier
and formal `yield` surface belong to plans 12 and 13. M6 will publish artifact
and LSP projections from canonical facts; consumers must not reconstruct escape
or call-target identity from source names.
