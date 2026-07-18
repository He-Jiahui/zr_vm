# 2026-07-18 AOT 08-S6H / 10-S4Z29 Public Constructed-Generic Type Object

## Scope

This cross-stage slice materializes a request-resolved dynamic generic carrier as a public reflection type object:

- Collected requests preserve their AOT TypeSpec and layout identity.
- Uncollected requests preserve their interpreter-deopt route without fabricating metadata or layout.
- Recursive argument descriptors are copied into a GC-managed object graph.

Token-only object materialization, `MakeGenericType`, interpreter generic execution, and cross-module identity remain open.

## Implementation

- `ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject()` re-runs constructed-generic resolution and rejects stale
  route, TypeSpec, or layout carriers before object creation.
- The public `kind == "type"` object exposes route, AOT collection state, metadata/base tokens, argument count, layout id,
  same-runtime native pointer, and `genericArguments`.
- Argument objects recursively copy primitive, type-token, array, tuple, ownership, nullable, and union identity.
- The implementation lives in a new 462-line module rather than extending the existing large reflection implementation.
- Objects are pinned before subsequent allocations or inserted into a pinned parent, then unpinned after the graph owns
  the references.

## RED / GREEN

RED:

- Clean MSVC compilation reached link and failed on the missing public builder symbol.

GREEN:

- A collected nested tuple/ownership/union/nullable request produces an AOT type object with its TypeSpec/layout.
- Mutating caller-owned descriptor fields after materialization does not change the recursive object identity.
- An uncollected primitive/token request produces an interpreter-deopt type object with token 0 and layout id -1.
- Null state/runtime/carrier inputs fail closed.
- The focused target passes 13 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44 clean build: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- The new source reports no GCC, Clang, or MSVC warnings; scoped `git diff --check` passed before documentation updates.

## Acceptance Decision

Accepted as 08-S6H / 10-S4Z29 only. Public constructed-generic reflection identity now survives beyond the borrowed
request descriptor lifetime. Full 08-S6 still requires a real `MakeGenericType` entry and interpreter consumer capable
of substituting and executing an uncollected instance; full 10-S4 retains token-only/cross-module generic object gaps.
