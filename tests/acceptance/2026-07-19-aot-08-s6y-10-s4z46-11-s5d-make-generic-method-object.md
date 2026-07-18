# 2026-07-19 AOT 08-S6Y / 10-S4Z46 / 11-S5D MakeGenericMethod C Object Entry

## Scope

This slice adds a direct C boundary that accepts an open generic MethodDef token plus concrete generic argument
descriptors and returns the exact existing MethodSpec reflection object. Script object decoding and native dispatch are
explicitly outside this slice.

## Contract

- `ZrCore_Reflection_MakeGenericMethodObject()` mirrors the existing type-level make entry.
- It delegates matching to `ZrCore_Reflection_ResolveConstructedGenericMethod()` and delegates object construction and
  carrier revalidation to `ZrCore_Reflection_BuildConstructedGenericMethodObject()`.
- It accepts only exact attached-runtime MethodSpec metadata and never synthesizes rows, cache entries, or code slots.
- Argument mismatch, a MethodSpec token used as the open method, and null state/runtime/arguments fail closed.

## Evidence

- RED: MSVC compiled all changed sources and linked with exactly one missing symbol,
  `ZrCore_Reflection_MakeGenericMethodObject`.
- GREEN: the dynamic generic executable reports 30/0 under MSVC 19.44, GCC 11.4, and Clang 14.0.
- Focused metadata/reflection CTest reports 6/6 on all three compilers.
- The implementation is a thin composition over the S6W/S6X resolver and object builder. It does not alter GC or object
  graph behavior; the immediately preceding three-compiler 66/0, 31/0, and 95/0 shared regressions remain applicable.
- The pre-existing `HEAD` profile-enum gap remains outside this slice; no 07 profile files are included.

## Acceptance Decision

Accepted as 08-S6Y / 10-S4Z46 / 11-S5D. The public C make boundary is closed. Script argument-object decoding,
`MakeGenericMethod` native dispatch, cross-module method binding, invoke thunks, and full-AOT closure remain open.
