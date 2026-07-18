# 2026-07-18 AOT 08-S6C Constructed Generic Request Resolution

## Scope

08-S6C accepts a `MakeGenericType`-style request described by an open generic base token plus runtime argument
identities:

- Primitive arguments use an `EZrValueType` identity.
- Direct type arguments use a resolvable TypeDef or TypeRef token.
- An exact current-module TypeSpec match reuses the 08-S6B AOT/interpreter route.
- An uncollected argument combination for a proven generic base returns an interpreter-deopt request carrier.

This slice does not execute interpreter dynamic instantiation and does not support nested generic or array argument
identities.

## Implementation

- `zr_vm_core/include/zr_vm_core/reflection.h` publishes `EZrReflectionGenericTypeArgumentKind`,
  `SZrReflectionGenericTypeArgument`, the borrowed `requestedArguments` carrier field, and
  `ZrCore_Reflection_ResolveConstructedGenericType()`.
- `zr_vm_core/src/zr_vm_core/reflection_generic_instance.c` validates the request, scans local TypeSpec token records,
  compares base/arity/arguments through metadata runtime generic views, and reuses the S6B route for exact matches.
- Missing combinations return `INTERPRETER_DEOPT` only when another valid TypeSpec proves the requested base is generic.
  No TypeSpec/signature token or layout is synthesized.

## RED / GREEN

RED:

- Clean MSVC compilation failed because the argument carrier, borrowed request view, and constructed-generic resolver
  API did not exist.

GREEN:

- A two-argument request (`int64`, direct TypeDef) resolves the existing TypeSpec and registered AOT layout.
- Replacing `int64` with `bool` returns an uncollected interpreter-deopt request with no static TypeSpec/layout.
- Invalid kind, `UNKNOWN` primitive, TypeSpec-as-base, null arguments, and zero arity fail closed.
- The focused target passes 6 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44 clean build: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.

## Acceptance Decision

Accepted as 08-S6C only. Runtime constructed-generic requests now distinguish an existing static TypeSpec from an
uncollected argument combination without inventing static metadata. Full 08-S6 still requires an interpreter consumer
that materializes and executes the dynamic instance; nested arguments and cross-module TypeSpec discovery remain open.
