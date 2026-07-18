# 2026-07-19 AOT 08-S6V / 10-S4Z43 / 11-S5A Generic Method Definition Object

## Scope

This slice publishes the first public reflection object for an open generic MethodDef. It exposes the exact GenericParam
owner range from attached zrp metadata, validates the complete range, and materializes GC-managed method-definition and
parameter objects. It does not match concrete type arguments, create a MethodSpec, or implement script dispatch.

## Baseline

- GenericParam views could read one parameter by owner token and logical index, but the exact owner range was private.
- Reading parameters until the first failure could misinterpret damaged metadata as a shorter valid generic method.
- MethodSpec context objects represented constructed signatures, but no public open generic method object existed.

## Test Inventory

- `tests/module/test_metadata_runtime_query.c`: TypeDef and MethodDef owner ranges, row/record identity, null input,
  unattached metadata, invalid token, and output clearing.
- `tests/module/test_reflection_dynamic_generic_instance.c` and
  `tests/module/test_reflection_dynamic_generic_method_context.h`: real method and parameter names from the string pool,
  token/runtime/flags/signature/constraint fields, two-parameter ordering, zero count, oversized declared count, damaged
  owner, wrong token, and null state.
- Focused CTest: `metadata_runtime_query`, `metadata_runtime_method_binding`, `reflection_token_resolve`,
  `metadata_runtime_binding_compatibility`, `metadata_runtime_typespec_layout`, and
  `reflection_dynamic_generic_instance`.
- Shared regressions: GC, instruction execution, and instruction table.

## Tooling Evidence

- WSL GCC 11.4: `/tmp/zr_vm-aot-08-s6b-isolated-gcc`.
- WSL Clang 14.0: `/tmp/zr_vm-aot-08-s6b-isolated-clang`.
- Windows MSVC 19.44 Debug: `%TEMP%/zr_vm-aot-08-s6b-msvc-red`, initialized through `VsDevCmd`.
- RED 1: the two focused MSVC targets linked with exactly one missing symbol each:
  `ZrCore_MetadataRuntime_ReadGenericOwnerView` and `ZrCore_Reflection_BuildGenericMethodDefinitionObject`.
- RED 2: after adding real zrp names, dynamic reflection reported 25 tests / 1 failure because the method name remained
  `genericMethodDefinition` instead of `Map`.
- Final changed implementation recompiles under GCC and Clang emitted no diagnostics. MSVC emitted only the project-wide
  command-line notice that `/W4` overrides `/W3` for those objects.
- A `HEAD + index` snapshot configured independently under MSVC. Its ordinary build exposed a pre-existing baseline gap:
  unchanged execution files in `HEAD` already reference `ZR_PROFILE_HELPER_VALUE_CONSTRUCT`, while the `HEAD` version of
  `profile.h` does not declare it. To avoid importing the concurrent 07-S7 slice, the staged-tree verification alone
  aliased that missing symbol to `ZR_PROFILE_HELPER_SET_BY_INDEX`; all six focused targets then built and CTest passed
  6/6. The alias is not part of this change.

## Results

- Metadata runtime exposes owner token/record, TypeDef or MethodDef row, first GenericParam index, and count; every
  failure clears the output view.
- The reflection builder validates the entire declared MethodDef range and rejects holes, owner drift, reordered or
  missing indices, invalid tokens, and zero-parameter definitions.
- Method and parameter names use zrp string-pool data; missing string metadata falls back to stable placeholder names.
- GCC, Clang, and MSVC focused CTest each pass 6/6.
- On each compiler, GC passes 66/0, instruction execution passes 31/0, and the instruction table passes 95/0.

## Acceptance Decision

Accepted as 08-S6V / 10-S4Z43 / 11-S5A. The public open generic method definition surface is present and fail-closed
without changing zrp rows, sections, or code-registration ABI. Concrete type-argument matching, constructed generic
method objects, script `MakeGenericMethod`, cross-module method binding, and full-AOT reflection closure remain open.
