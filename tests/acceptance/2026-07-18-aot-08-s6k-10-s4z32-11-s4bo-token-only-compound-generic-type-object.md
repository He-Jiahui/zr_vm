# 2026-07-18 AOT 08-S6K / 10-S4Z32 / 11-S4BO Token-Only Compound Generic Type Object

## Scope

This cross-stage slice completes current-runtime token-only generic argument object materialization for every compound
type-signature node emitted by the current writer: nested generic instance, array, tuple, ownership, nullable, and union.

Cross-module identity and interpreter execution are outside this slice.

## Implementation

- `ZrCore_MetadataRuntime_ResolveSignatureTypeNodeRecord()` matches complete signature-node byte spans against attached
  TypeDef/TypeRef records and local TypeSpec records.
- Existing TypeSpec and MethodSpec direct argument binding views reuse the same resolver.
- Nested `GENERIC_INST` becomes a type-token argument only when a real local TypeSpec record has the same complete span.
- Array, tuple, ownership, nullable, and union nodes recursively produce the same public object shape as request-driven
  descriptors under a 64-level fail-closed depth limit.
- Invalid primitive, array-rank, ownership, or union payloads fail closed.
- Metadata node binding moved from `metadata_runtime.c` into `metadata_runtime_type_node_binding.c`, reducing the former
  from 1036 to 979 lines. Reflection object assertions moved into a dedicated test support header.

## RED / GREEN

RED:

- The token-only outer TypeSpec failed to materialize nested generic and array arguments.
- The token-only compound TypeSpec failed to materialize tuple, ownership, union, nullable, and primitive descendants.
- The focused target reported 15 tests with 2 failures at the two object non-null assertions.

GREEN:

- The outer object carries the exact nested TypeSpec token plus rank-1 array and direct element TypeDef token.
- The compound object carries ordered tuple children, ownership qualifier, named union identity, nullable child, and
  primitive identity.
- The focused target reports 15 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- GCC and Clang logs contain no warnings attributed to `metadata_runtime_type_node_binding.c` or
  `reflection_generic_type_object.c`.

## Acceptance Decision

Accepted as 08-S6K / 10-S4Z32 / 11-S4BO only. Current-runtime token-only argument object materialization now covers the
writer's complete compound node set without fabricated nested tokens. Full 08-S6 still requires interpreter generic
parameter substitution and execution; full 10-S4 still requires script-object methods and cross-module identity; full
11-S4 still requires cross-module canonical binding and remaining metadata consumers.
