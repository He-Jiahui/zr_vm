# 2026-07-18 AOT 08-S6G Union Generic Argument Identity

## Scope

08-S6G adds named union identity to local constructed-generic requests:

- `UNION` carries the encoded value type and current metadata runtime's union-name string-heap offset.
- Union arity and ordered children participate in exact identity.
- Union children recursively compose with the previously supported argument kinds.

Cross-module canonicalization and interpreter dynamic-instance execution remain open.

## Implementation

- `EZrReflectionGenericTypeArgumentKind` gains append-only `UNION`.
- `SZrReflectionGenericTypeArgument` gains `unionValueType` and `unionNameStringOffset`, reusing the borrowed child list.
- Validation rejects invalid value types, empty name offsets, invalid children, and inconsistent child count/pointer pairs.
- Matching compares both union payloads, exact child count, and every ordered child.
- Tuple and union matching share one metadata child-list walker, so recursive offset advancement has one implementation.

## RED / GREEN

RED:

- Clean MSVC compilation failed because the union kind and identity fields did not exist.

GREEN:

- A collected `Tuple<unique Base, Union<nullable int64>>` shape resolves to its exact TypeSpec and registered layout.
- Changing the union value type, name offset, arity, nullable child, enclosing ownership qualifier, or tuple arity leaves
  the request on interpreter deopt instead of binding the collected TypeSpec.
- A union descriptor with a nonzero child count and null child list fails closed.
- The focused target passes 11 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44 clean build: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- Scoped `git diff --check` passed before documentation updates.

## Acceptance Decision

Accepted as 08-S6G only. Local constructed-generic matching now covers the compound nodes emitted by the current typed
signature writer. Full 08-S6 still requires cross-module TypeSpec identity canonicalization/discovery and an interpreter
consumer that materializes and executes uncollected instances.
