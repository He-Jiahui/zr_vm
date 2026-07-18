# 2026-07-18 AOT 08-S6B Reflection Dynamic Generic TypeSpec Route

## Scope

08-S6B establishes the runtime decision carrier for an existing generic TypeSpec:

- A metadata-valid TypeSpec with a registered code-layout entry routes to AOT.
- The same closed generic identity without a registered layout routes to interpreter deopt.
- Invalid or non-TypeSpec inputs fail and clear the output carrier.

This does not close full 08-S6. It does not synthesize a new TypeSpec from `MakeGenericType` arguments or execute the
interpreter-side dynamic instantiation.

## Implementation

- `zr_vm_core/include/zr_vm_core/reflection.h` publishes `EZrReflectionGenericInstanceRoute`,
  `SZrReflectionDynamicGenericTypeInstance`, and `ZrCore_Reflection_ResolveDynamicGenericTypeInstance()`.
- `zr_vm_core/src/zr_vm_core/reflection_generic_instance.c` validates the generic binding through
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()`, preserves signature/base/argument identity, and consults
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` as the single AOT-layout source of truth.
- `tests/module/test_reflection_dynamic_generic_instance.c` covers invalid input, interpreter deopt for an uncollected
  TypeSpec, and AOT routing for a registered layout.

## RED / GREEN

RED:

- Clean MSVC compilation failed at the new test because the dynamic generic carrier, route enum, and resolver API did
  not exist.

GREEN:

- Missing layout returns success with `ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT`, complete generic
  identity, `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`, and a null layout.
- Registered layout returns `ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT` with layout id 42 and the registry layout.
- Null runtime, non-TypeSpec token, and null output are rejected; writable output is reset before rejection.

## Validation

- WSL GCC 11.4, isolated source/build: 3/3 CTest passed.
- WSL Clang 14.0, isolated source/build: 3/3 CTest passed.
- Windows MSVC 19.44, clean build: 3/3 CTest passed.
- The matrix contains `reflection_dynamic_generic_instance`, `metadata_runtime_typespec_layout`, and
  `reflection_token_resolve`.
- `git diff --check` passes for the scoped code and test changes, with only existing line-ending warnings.

## Acceptance Decision

Accepted as 08-S6B only. Existing generic TypeSpec identities now have an explicit AOT/interpreter-deopt route.
Full 08-S6 still requires argument-driven `MakeGenericType` construction and interpreter dynamic-instantiation
execution; reflection construction closure and full-AOT reachability remain open.
