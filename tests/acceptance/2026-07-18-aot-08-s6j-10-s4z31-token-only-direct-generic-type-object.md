# 2026-07-18 AOT 08-S6J / 10-S4Z31 Token-Only Direct-Argument Generic Type Object

## Scope

This cross-stage slice extends the public constructed-generic type-object builder to existing TypeSpec carriers that
were resolved by token rather than by a caller-provided recursive argument request.

The supported metadata arguments in this slice are primitive nodes and direct TypeDef/TypeRef nodes. Recursive compound
metadata arguments remain fail-closed.

## Implementation

- Token-only carriers are revalidated through `ZrCore_Reflection_ResolveDynamicGenericTypeInstance()`.
- Route, TypeSpec token, open base, arity, layout id, and layout pointer must still match the supplied carrier.
- `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()` supplies each direct metadata argument.
- Primitive nodes reuse the existing recursive request-object builder with a temporary primitive descriptor.
- Direct TypeDef/TypeRef nodes reuse that builder with the resolved argument token.
- Nested generic, array, tuple, ownership, nullable, and union metadata nodes return null until their recursive identity
  can be materialized without fabricating child tokens.

## RED / GREEN

RED:

- The collected `Generic<int64, Base>` token-only carrier resolved to AOT, but public object construction returned null.
- The focused target reported 13 tests with 1 failure at the non-null object assertion.

GREEN:

- The same carrier produces a public type object with two `genericArguments` entries.
- Argument 0 preserves primitive `INT64` identity.
- Argument 1 preserves the resolved direct TypeDef token.
- The focused target reports 13 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed.
- Windows MSVC 19.44: focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- GCC and Clang build logs contain no warnings attributed to `reflection_generic_type_object.c`.
- Scoped `git diff --check` passed before documentation updates.

## Acceptance Decision

Accepted as 08-S6J / 10-S4Z31 only. Existing TypeSpecs with primitive/direct metadata arguments now share the public
generic type-object boundary with request-resolved carriers. Full 08-S6 still requires interpreter generic parameter
substitution and execution for uncollected instances; full 10-S4 still requires recursive token-only compound arguments,
script-object methods, and cross-module identity.
