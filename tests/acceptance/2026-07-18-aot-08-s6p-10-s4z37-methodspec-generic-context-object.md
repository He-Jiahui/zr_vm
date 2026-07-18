# 2026-07-18 AOT 08-S6P / 10-S4Z37 MethodSpec Generic Context Object

## Scope

This cross-stage slice materializes the existing 11-S5 MethodSpec metadata view as a GC-managed reflection context
object. It does not bind the context to a call info, substitute method-owned generic parameters, or execute a MethodSpec.

## Implementation

- `ZrCore_Reflection_BuildMethodSpecGenericContextObject()` validates a MethodSpec `SIGNATURE` token through
  `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`.
- Indexed arguments come from `ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView()`; no parallel MethodSpec table
  or argument parser is introduced.
- Primitive, direct TypeDef/TypeRef, nested generic, array, tuple, ownership, nullable, and union nodes reuse the existing
  recursive token-only metadata-node object builder.
- The reflection object exposes `metadataToken`, `genericMethodToken`, a full uint64 `genericSignatureHash`,
  `metadataRuntime`, generic-method flags, `genericArgumentCount`, and `genericArguments`.
- Invalid non-MethodSpec tokens, zero-argument malformed signatures, null state, and null runtime fail closed.
- No metadata format or metadata-runtime API changed.

## RED / GREEN

RED:

- The MethodSpec context scenario compiled and reached the final MSVC link.
- The link failed with one unresolved symbol for the missing public builder.

GREEN:

- A two-argument MethodSpec materializes primitive uint64 and TypeRef reflection argument objects.
- MethodSpec token, underlying method token, complete signature hash, runtime, flags, count, and array are preserved.
- A MemberDef token and null state are rejected.
- All preceding dynamic generic route, type context, full-GC, and VM execution tests remain green.
- The focused executable reports 20 tests with 0 failures.

An intermediate 20/1 result failed before the builder because the test fixture passed null code registration to
`ZrCore_Module_AttachMetadataRuntime()`. Supplying a zero-initialized non-null registration restored the existing module
runtime contract; production validation was not weakened.

## Validation

- WSL GCC 11.4: final focused CTest matrix 3/3 passed.
- WSL Clang 14.0: final focused CTest matrix 3/3 passed.
- Windows MSVC 19.44: final focused CTest matrix 3/3 passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- `reflection_generic_type_object.c` has no GCC or Clang warning.
- Scoped `git diff --check` passed with only existing LF/CRLF conversion notices.

## Acceptance Decision

Accepted as 08-S6P / 10-S4Z37 only. MethodSpec identity and generic arguments now have a public, GC-managed interpreter
context object that reuses the metadata single source of truth. Full 08-S6 remains open for call-frame binding,
method-owned GenericParam substitution and execution, dynamic value-type instances, and cross-module identity. Full
10-S4 still requires generic method reflection/invocation behavior and script-level lookup.
