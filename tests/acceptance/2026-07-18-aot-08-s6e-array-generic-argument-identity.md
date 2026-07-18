# 2026-07-18 AOT 08-S6E Array Generic Argument Identity

## Scope

08-S6E adds recursive array argument identity to constructed-generic requests:

- `ARRAY` carries an exact rank and a borrowed element argument descriptor.
- Element identity can recursively use primitive, direct TypeDef/TypeRef, nested TypeSpec, or another array.
- Validation and matching fail closed at a depth of 64.

Tuple, union, ownership, and other compound signature nodes, cross-module identity, and interpreter dynamic-instance
execution remain open.

## Implementation

- `SZrReflectionGenericTypeArgument` gains append-only `ARRAY`, `arrayRank`, and `elementType` fields.
- Request validation rejects rank zero, null elements, invalid element identities, and recursive/cyclic descriptors that
  reach the depth limit.
- Candidate array nodes are read through `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` and recursively matched.
- Top-level direct token arguments still require exact resolved-token equality. Structural comparison against a token's
  validated signature root is used only for compound children that do not carry an independent resolved token.

## RED / GREEN

RED:

- Clean MSVC compilation failed because the array kind, rank, and element fields did not exist.

GREEN:

- The outer TypeSpec now contains a `Base[]` second argument and resolves with an exact rank-1 request.
- A rank-2 request remains an uncollected interpreter-deopt request instead of matching rank 1.
- A self-referential array descriptor is rejected by the recursion-depth gate.
- The focused target passes 9 tests with 0 failures.
- Test arguments use designated initializers; GCC and Clang report no new missing-field initializer warnings.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44 clean build: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.

## Acceptance Decision

Accepted as 08-S6E only. Recursive arrays now participate in exact local constructed-generic identity. Full 08-S6 still
requires remaining compound signature kinds, cross-module TypeSpec discovery/remap, and an interpreter consumer that
materializes and executes uncollected instances.
