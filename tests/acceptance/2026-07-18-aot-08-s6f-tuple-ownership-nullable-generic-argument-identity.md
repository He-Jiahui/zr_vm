# 2026-07-18 AOT 08-S6F Tuple, Ownership, and Nullable Generic Argument Identity

## Scope

08-S6F adds recursive tuple, ownership, and nullable argument identity to local constructed-generic requests:

- `TUPLE` carries an ordered borrowed child list and exact child count.
- `OWNERSHIP` carries a stable qualifier plus a borrowed element descriptor.
- `NULLABLE` carries a borrowed base descriptor.
- All three compose with primitive, direct token, nested TypeSpec, array, and each other under the existing depth gate.

Union identity, cross-module TypeSpec discovery/remap, and interpreter dynamic-instance execution remain open.

## Implementation

- `EZrReflectionGenericTypeArgumentKind` gains append-only `TUPLE`, `OWNERSHIP`, and `NULLABLE` values.
- `EZrReflectionOwnershipQualifier` gives the reflection request surface stable ownership payload values without adding
  a core dependency on parser AST declarations.
- `SZrReflectionGenericTypeArgument` gains borrowed `childTypes`, `childCount`, and `ownershipQualifier` fields.
- Validation rejects empty tuple child lists, invalid nested children, `NONE` ownership, missing ownership elements,
  and missing nullable elements.
- Matching walks metadata tuple children in encoded order, compares exact ownership payloads, and recursively matches
  nullable bases. The recursion limit remains 64.

## RED / GREEN

RED:

- Clean MSVC compilation failed at the missing compound kinds and request carrier fields.

GREEN:

- A collected `Tuple<unique Base, nullable int64>` argument resolves to its exact local TypeSpec and registered layout.
- Ownership qualifier, nullable base, and tuple arity mismatches remain interpreter-deopt requests instead of matching.
- Malformed tuple, ownership, and nullable request descriptors fail closed.
- The focused target passes 11 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44 clean build: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- Scoped `git diff --check` passed before documentation updates.

## Acceptance Decision

Accepted as 08-S6F only. Tuple, ownership, and nullable nodes now participate in exact local constructed-generic
identity. Full 08-S6 still requires union identity, cross-module TypeSpec discovery/remap, and an interpreter consumer
that materializes and executes uncollected instances.
