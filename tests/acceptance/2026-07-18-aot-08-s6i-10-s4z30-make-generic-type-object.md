# 2026-07-18 AOT 08-S6I / 10-S4Z30 Public MakeGenericType Object Entry

## Scope

This cross-stage slice exposes a single public C entry for reflection-driven constructed generic type creation:

- Input is an open generic base token plus recursive argument descriptors.
- Output is the public constructed-generic type object from 08-S6H / 10-S4Z29.
- Collected and uncollected routes preserve their existing AOT/interpreter-deopt semantics.

Script-object methods and interpreter execution of uncollected instances remain open.

## Implementation

- `ZrCore_Reflection_MakeGenericTypeObject()` calls the existing constructed-generic resolver.
- Successful resolution is passed to the existing stale-carrier-checking object builder.
- No matching, routing, descriptor-copy, or object-field rules are duplicated in the new entry.
- Invalid input and unsupported generic bases fail closed through the existing resolver contract.

## RED / GREEN

RED:

- Clean MSVC reached link and failed on the missing MakeGenericType public symbol.

GREEN:

- A collected nested compound request returns an AOT object with its TypeSpec token and layout id.
- An uncollected primitive/token request returns an interpreter-deopt object.
- The focused target remains 13 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44 clean build: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- The implementation module reports no toolchain-specific warnings.

## Acceptance Decision

Accepted as 08-S6I / 10-S4Z30 only. Reflection callers now have a direct MakeGenericType-style C boundary that returns
the public type object. Full 08-S6 still requires interpreter generic parameter substitution and execution for an
uncollected instance; full 10-S4 still requires script-object and token-only/cross-module surfaces.
