# 2026-07-19 AOT 08-S6AA / 10-S4Z48 / 11-S5F Generic Method Native Entry

## Scope

This slice adds the runtime-bound native stack entry above the existing bounded generic method reflection decoder. It
does not create or export a `zr.reflection` module and does not add a serialized native helper identity.

## Contract

- `ZrCore_Reflection_CreateMakeGenericMethodNativeClosure()` requires a real module-owned runtime and captures that
  metadata module object in one GC-owned, closed `SZrClosureValue`; the script-visible call has only definition and
  argument-array parameters.
- Owner-backed capture storage leaves the direct capture pointer null, so no stack address survives closure creation.
- After the capture closes successfully, the factory applies a `NATIVE_HANDLE` pin so the metadata module enters
  `OLD_PINNED`/`PINNED`; embedded runtime and reflection-carrier native pointers therefore remain address-stable.
- Generational full GC may move the closure and capture owner; `ZrCore_ClosureNative_GetCaptureValue()` still resolves
  and keeps the pinned metadata module live, and the entry derives its current runtime from that module.
- `ZrCore_Reflection_MakeGenericMethodNativeEntry()` requires its own native function identity, exactly one valid
  owner-backed metadata module capture, exactly two arguments, OBJECT definition shape, and ARRAY argument shape.
- Validation or MethodSpec resolution failure returns one null value and normalizes `stackTop` to `functionBase + 1`.
- The entry delegates object decoding, metadata validation, exact MethodSpec lookup, and object construction to the
  previously accepted S6Z/S4Z47/S5E boundary.

## TDD And Regression Evidence

- RED 1: MSVC compiled the test and public declarations, then linked with exactly two missing symbols:
  `ZrCore_Reflection_MakeGenericMethodNativeEntry` and
  `ZrCore_Reflection_CreateMakeGenericMethodNativeClosure`.
- RED 2: after the initial factory implementation, the full-GC owner invariant failed 31/1 because the direct capture
  address still pointed at a former stack slot. The owner-only representation closed that failure.
- RED 3: the owner-only raw-runtime factory still accepted a fixture runtime not owned by a GC module and failed 31/1.
  Capturing a validated metadata module object closed the lifetime gap.
- RED 4: a module capture alone did not establish the non-moving contract required by the embedded runtime and existing
  reflection carriers; the pin-state assertion failed 31/1. Applying `NATIVE_HANDLE` after successful capture closure
  and exercising a generational full GC closed the relocation gap.
- Negative coverage includes wrong capture type, missing capture, wrong arity, non-array argument, and the decoder's
  prior runtime/definition/argument failures.
- Dynamic generic reflection passes 32/0 on MSVC 19.44, GCC 11.4, and Clang 14.0.
- Final implementation focused metadata/reflection CTest passes 6/6 on MSVC.
- Final implementation shared regression passes GC 66/0, instruction execution 31/0, and instruction table 95/0 on
  MSVC.
- The new native source emits no GCC/Clang diagnostics, and final implementation dynamic generic reflection passes 32/0
  on both compilers.
- Before the module-lifetime correction, focused 6/6 and shared 66/31/95 passed on GCC and Clang. The final wide rerun
  could not complete because WSL restarted and cleared all isolated `/tmp` source/build directories; those pre-fix runs
  are retained as regression history, not claimed as final-source acceptance.
- The known `HEAD` profile-enum gap remains outside this commit; isolated compiler sources retain the previously
  validated profile definition while this commit does not stage concurrent profile changes.

## Acceptance Decision

Accepted as 08-S6AA / 10-S4Z48 / 11-S5F with final MSVC wide regression and final three-compiler dynamic generic
execution. Module-owned runtime native stack dispatch is closed. Reflection module construction and export,
registration replacement/unload policy, serialized helper identity, cross-module method binding, invoke thunks, and
full-AOT closure remain open.
