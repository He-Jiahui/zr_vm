# 2026-07-19 AOT 08-S6W / 10-S4Z44 / 11-S5B MethodSpec Request Resolution

## Scope

This slice resolves a local open generic MethodDef plus concrete generic type argument descriptors to an existing
MethodSpec in the attached metadata runtime. It does not synthesize metadata, materialize a constructed method object, or
implement script dispatch.

## Baseline

- Open generic MethodDef reflection objects exposed exact GenericParam owner ranges.
- Existing MethodSpec context objects could be built and executed when the caller already knew the MethodSpec token.
- Recursive argument matching existed only inside the TypeSpec request resolver.

## Test Inventory

- `tests/module/test_reflection_dynamic_generic_instance.c` registers the focused scenarios.
- `tests/module/test_reflection_dynamic_generic_method_context.h` covers exact primitive + TypeRef matching, carrier
  identity, function-record and module-record discovery, arity/type/method mismatch, damaged owner records, invalid
  argument kinds, null input, and output clearing.
- Focused CTest: `metadata_runtime_query`, `metadata_runtime_method_binding`, `reflection_token_resolve`,
  `metadata_runtime_binding_compatibility`, `metadata_runtime_typespec_layout`, and
  `reflection_dynamic_generic_instance`.
- Shared regressions: GC, instruction execution, and instruction table.

## Tooling Evidence

- RED: MSVC linked with exactly one missing symbol,
  `ZrCore_Reflection_ResolveConstructedGenericMethod`.
- GREEN: the dynamic generic target reports 28 tests / 0 failures under MSVC 19.44, GCC 11.4, and Clang 14.0.
- The new resolver and shared matcher emit no GCC/Clang diagnostics; MSVC emits only the project-wide `/W4` overriding
  `/W3` notice for changed objects.
- A shared WSL source directory was overwritten by a concurrent synchronization and produced an obsolete 25-test run;
  that result was discarded. Final GCC evidence used a hash-fixed private S6W source directory. Clang recompiled the
  latest test/header in the existing isolated build and passed 28/0. A separate fresh Clang configure stalled in WSL 9P
  RPC before generating build files and is not treated as code evidence.
- The pre-existing `HEAD` profile-enum baseline gap documented by S6V remains outside this change.

## Results

- Resolution requires a valid local MethodDef owner view and exact GenericParam arity.
- Function and module metadata records are scanned deterministically; each candidate is re-read through the existing
  MethodSpec signature and indexed argument views.
- TypeSpec and MethodSpec requests now share one recursive signature matcher for all currently encoded compound nodes.
- Success preserves MethodSpec/method records, signature hash, argument coordinates, and the borrowed request arguments.
- Every failure clears the output carrier, and no MethodSpec or registration entry is fabricated.
- GCC, Clang, and MSVC focused CTest each pass 6/6. Shared regressions pass 66/0, 31/0, and 95/0 on each compiler.

## Acceptance Decision

Accepted as 08-S6W / 10-S4Z44 / 11-S5B. Exact local MethodSpec request resolution is closed without changing zrp or
code-registration ABI. Constructed method objects, script `MakeGenericMethod`, cross-module method binding, and full-AOT
reflection closure remain open.
