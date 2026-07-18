# 2026-07-18 AOT 08-S6R / 10-S4Z39 MethodSpec VM Function Execution

## Scope

This cross-stage slice executes a caller-resolved fixed VM function with an S6P/S6Q MethodSpec context in its active
call info. It does not resolve a function from a method token, expose script-level `MakeGenericMethod`, dynamically
instantiate a value type, or resolve cross-module generic identity.

## Implementation

- `ZrCore_Reflection_InvokeInterpreterGenericMethodSpecResolvedFunction()` reads the existing MethodSpec view and
  requires every underlying method GenericParam in `[0, argumentCount)` to resolve while rejecting an extra parameter.
- The supplied function must be a non-native, non-vararg VM function with instructions and an exact explicit-argument
  count. The API contract deliberately names it a resolved function; no unverifiable function identity is fabricated.
- The function and S6P context object remain pinned across argument staging and execution.
- The internal known-function call boundary accepts independent optional type and method contexts. It copies each
  supplied context after `PreCall` and before `Execute`; the existing type-only API remains a compatibility wrapper.
- The object-call boundary reuses its existing callable/argument pinning, stack anchors, argument staging, nested-call
  restoration, and result copying for a static call with no receiver.
- Invalid MethodSpec tokens, mismatched explicit arity, native/vararg functions, invalid state, and invalid result slots
  fail closed. The public result is reset before validation.
- No metadata format or metadata-runtime API changed.

## RED / GREEN

RED:

- The real VM execution scenario compiled and reached the final MSVC link.
- The link failed with one unresolved symbol for the missing public MethodSpec invoke API.

GREEN:

- A one-argument VM identity function receives int64 value 109 and returns 109.
- Its trace observer sees the MethodSpec context in the active call info and confirms that the independent type context
  remains empty.
- The observer resolves method-owned GenericParam[1] to the expected TypeRef argument object during bytecode execution.
- A MemberDef token and a zero-argument call against the one-parameter function fail and clear stale results.
- All preceding dynamic generic route, type context, MethodSpec context, full-GC, and instance-method tests remain green.
- The focused executable reports 22 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- WSL Clang 14.0: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- Windows MSVC 19.44: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- The focused matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- The new function and reflection implementation has no GCC or Clang warning. `object_call.c` retains the two Clang
  unused-helper warnings already present in the S6N/S6O baseline.
- Scoped `git diff --check` passed with only the repository's existing LF/CRLF conversion notices.

## Acceptance Decision

Accepted as 08-S6R / 10-S4Z39 only. A MethodSpec-owned generic context now participates in real VM execution with
explicit arguments, results, and active-frame GenericParam substitution. Full 08-S6 remains open for dynamic value-type
instances and cross-module identity. Full 10-S4 still requires method-token/function resolution and script-level generic
method reflection behavior.
