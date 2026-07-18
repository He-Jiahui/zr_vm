# 2026-07-18 AOT 08-S6D Nested TypeSpec Argument Identity

## Scope

08-S6D extends constructed-generic request resolution so a local TypeSpec token can represent a nested closed-generic
argument. The request must match the complete nested `GENERIC_INST` signature structure, not only its node kind or
top-level payload.

This slice remains within one metadata runtime. Array and other compound argument identities, cross-module token/string
remap, and interpreter dynamic-instance execution remain open.

## Implementation

- `ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN` accepts metadata-valid local TypeSpec tokens in addition to direct
  TypeDef/TypeRef tokens.
- `reflection_nested_type_spec_argument_matches()` reads the requested TypeSpec root and candidate argument node through
  metadata runtime structured views, validates both byte spans, and compares the complete encoded node structure.
- No type-name comparison, shallow node-kind match, or synthetic token is used.
- The focused fixture contains an inner TypeSpec and an outer TypeSpec whose first argument is the complete inner
  `GENERIC_INST` signature.

## RED / GREEN

RED:

- The nested request compiled but returned false: 7 tests, 1 failure.

GREEN:

- An inner-TypeSpec argument resolves the outer TypeSpec and its registered layout.
- An outer-TypeSpec-as-self request has the same top-level node kind but different structure and returns an uncollected
  interpreter-deopt request instead of falsely binding the outer TypeSpec.
- The focused target passes 8 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44 clean build: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.

## Acceptance Decision

Accepted as 08-S6D only. Nested local closed-generic arguments now have exact structural identity. Full 08-S6 still
requires interpreter dynamic-instance materialization/execution, array and remaining compound argument identities, and
cross-module TypeSpec discovery after metadata remap.
