# 2026-07-18 AOT 08-S6N / 10-S4Z35 Interpreter Generic Call-Info Context

## Scope

This cross-stage slice carries the S6L interpreter generic instance context into a VM call frame and keeps that context
valid across a compacting full GC. It reuses the S6M GenericParam substitution rules through the call info.

Actual uncollected generic method execution, dynamic value-type instances, cross-module identity, and script-level
methods are outside this slice.

## Implementation

- `SZrCallInfo.interpreterGenericContext` stores the context as a GC-visible `SZrTypeValue`, not a raw pointer.
- `ZrCore_Reflection_BindInterpreterGenericInstanceCallInfo()` validates an S6L instance and binds its type object only
  to a VM call info. Failed binds clear any previous context.
- `ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject()` returns the validated bound type object.
- `ZrCore_Reflection_ResolveInterpreterGenericCallInfoParameterTypeObject()` reuses the S6M metadata-runtime,
  open-base, arity, owner, and parameter-index checks.
- Ordinary/native/VM initialization, exact-argument hot setup, and tail-call reuse clear the carrier to prevent context
  leakage between calls.
- Active call infos mark the context during GC and rewrite its forwarding address during compacting collection.
- No metadata format or metadata-runtime API changes were required; the slice consumes the existing 11-S5 view.

## RED / GREEN

RED:

- The full-GC call-info scenario compiled and reached the final MSVC link.
- The link failed with three unresolved symbols for the missing bind, get, and call-info parameter-resolution APIs.

GREEN:

- A deoptimized reference-class generic instance binds to a VM call info.
- The call info reports the original open generic base token and resolves its TypeDef argument.
- After a compacting full GC, the rewritten call-info context still resolves the same TypeDef argument.
- Binding a non-instance type object fails and clears the previous context.
- All preceding dynamic generic route, type-object, interpreter instance, and substitution tests remain green.
- The focused executable reports 18 tests with 0 failures.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3 passed; shared GC 66/0 and instruction execution 31/0 passed.
- WSL Clang 14.0: focused CTest matrix 3/3 passed; shared GC 66/0 and instruction execution 31/0 passed.
- Windows MSVC 19.44: focused CTest matrix 3/3 passed; shared GC 66/0 and instruction execution 31/0 passed.
- The focused matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- GCC and Clang logs contain no warnings attributed to the changed call-info, function, GC, or interpreter-generic
  implementation files.
- Scoped `git diff --check` passed; only the repository's existing LF/CRLF conversion notices were emitted.

## Acceptance Decision

Accepted as 08-S6N / 10-S4Z35 only. The interpreter generic context now has an explicit VM call-frame owner and survives
moving GC without a stale pointer. Full 08-S6 remains open until an uncollected generic method actually executes through
that context. Full 10-S4 still requires generic method reflection, script-object methods, and cross-module identity.
