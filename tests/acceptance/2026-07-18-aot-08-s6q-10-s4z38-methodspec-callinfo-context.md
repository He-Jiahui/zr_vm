# 2026-07-18 AOT 08-S6Q / 10-S4Z38 MethodSpec Call-Info Context

## Scope

This cross-stage slice binds the S6P MethodSpec context to a VM call info, substitutes method-owned GenericParams, and
keeps the context valid across a compacting full GC. It does not execute a MethodSpec, dynamically instantiate a value
type, resolve cross-module generic identity, or perform script-level member-name lookup.

## Implementation

- `SZrCallInfo.interpreterGenericMethodContext` is a GC-visible `SZrTypeValue` independent from the S6N generic type
  instance context.
- `ZrCore_Reflection_BindInterpreterGenericMethodSpecCallInfo()` builds the S6P context from the existing MethodSpec
  view and binds it only to a non-native VM call info. Failed binds clear any previous method context.
- `ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject()` returns the validated reflection object from an
  active call info.
- `ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject()` reuses the 11-S5 GenericParam
  owner/index view and validates metadata runtime, underlying generic method token, owner range, context arity, and
  parameter index before returning the concrete argument type object.
- Ordinary/native/VM initialization, the exact-argument hot path, and call-info reuse clear both generic carriers.
- Active call infos mark the method context during GC and rewrite its forwarding address during compacting collection.
- The focused fixture carries valid TypeDef, MethodDef, GenericParam, MethodSpec, and signature-pool metadata; production
  ZRP validation remains unchanged.
- No metadata format or metadata-runtime API changed.

## RED / GREEN

RED:

- The MethodSpec call-info/full-GC scenario compiled and reached the final MSVC link.
- The link failed with three unresolved symbols for the missing bind, get, and method-parameter-resolution APIs.

GREEN:

- A two-argument MethodSpec binds to a VM call info and reports its MethodSpec identity.
- Method-owned GenericParam[0] resolves to primitive uint64 before collection.
- After a compacting full GC, the rewritten context resolves GenericParam[1] to the expected TypeRef object.
- A wrong owner token fails closed; binding a non-MethodSpec token fails and clears the previous context.
- All preceding dynamic generic route, type context, full-GC, and VM execution tests remain green.
- The focused executable reports 21 tests with 0 failures.

An intermediate 21/2 result failed while attaching the shared fixture because it declared a `MemberDef RID 7`
GenericParam owner without a MethodDef table. The fixture now uses a real TypeDef/MethodDef owner and an explicit
two-parameter method range. Production metadata validation was not weakened.

## Validation

- WSL GCC 11.4: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- WSL Clang 14.0: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- Windows MSVC 19.44: focused CTest matrix 3/3; shared GC 66/0 and instruction execution 31/0.
- The focused matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- GCC and Clang logs contain no warnings attributed to the changed call-info, function, GC, interpreter-generic, or
  focused test files.
- Scoped `git diff --check` passed with only the repository's existing LF/CRLF conversion notices.

## Acceptance Decision

Accepted as 08-S6Q / 10-S4Z38 only. MethodSpec identity and method-owned generic arguments now have an explicit,
moving-GC-safe VM call-frame owner and substitution path. Full 08-S6 remains open for actual MethodSpec execution,
dynamic value-type instances, and cross-module identity. Full 10-S4 still requires generic method invocation behavior
and script-level member lookup.
