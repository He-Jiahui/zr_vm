# AOT 10-S2E / 10-S3I Method.Invoke Parameter Base-Type Guard

## Scope
- Extends the counted token-driven Method.Invoke dispatcher with a fixed-parameter base-type guard.
- After token binding, arity, and signature shape pass, concrete fixed `parameterTypes[i].baseType` entries must match
  the corresponding `SZrTypeValue.type`.
- `ZR_VALUE_TYPE_NULL` and `ZR_VALUE_TYPE_UNKNOWN` signature slots remain unforced for now, and varargs extra
  arguments are not checked by this slice.
- This slice does not implement typed unboxing, numeric widening, nullable/ownership/staticCType compatibility, return
  boxing, public `MethodInfo` object materialization, MethodSpec-specific code slots, cross-module token rewrite, or
  full trim analysis.

## Baseline
- `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)` checked token binding, argument count, varargs count, and
  signature shape.
- It still dispatched when a fixed parameter declared a concrete base type but the supplied `SZrTypeValue` had a
  different runtime type.

## Test Inventory
- `tests/module/test_reflection_method_invoke.c`
  - `test_reflection_invoke_method_token_rejects_incomplete_signature_shape`
  - `test_reflection_invoke_method_token_checks_fixed_parameter_base_types`
  - Verifies `baseType=BOOL` with `args[i].type=INT64` is rejected without dispatch.
  - Verifies changing the argument runtime type to `ZR_VALUE_TYPE_BOOL` allows dispatch.
- Adjacent regression:
  - `tests/module/test_reflection_token_resolve.c`
  - `tests/module/test_metadata_runtime_method_binding.c`
  - `tests/module/test_metadata_runtime_query.c`

## Tooling Evidence
- RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test'`
  - Failed in `test_reflection_invoke_method_token_checks_fixed_parameter_base_types` with
    `Expected FALSE Was TRUE`.
- GREEN and regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ctest --test-dir build-wsl-gcc -R "^(reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test && ctest --test-dir build-wsl-clang -R "^(reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure'`
  - MSVC Debug with imported Visual Studio environment:
    `cmake --build build-msvc --config Debug --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8`
    plus the four focused executables and
    `ctest --test-dir build-msvc -C Debug -R "^(reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure`.
  - `git diff --check -- tests/module/test_reflection_method_invoke.c tests/module/test_reflection_token_resolve.c zr_vm_core/src/zr_vm_core/reflection_token_resolve.c docs/plans/aot/10-reflection.md docs/plans/aot/11-metadata.md docs/plans/aot/index.md docs/module-system/typed-module-metadata.md .codex/sessions/20260620-2321-aot-07-12-codegen.md tests/acceptance/2026-06-30-aot-10-s2e-method-invoke-parameter-base-type-guard.md`

## Results
- WSL gcc: reflection method invoke 2/0; reflection token resolve 7/0; method binding 2/0; metadata runtime query
  24/0; focused CTest 3/3.
- WSL clang: reflection method invoke 2/0; reflection token resolve 7/0; method binding 2/0; metadata runtime query
  24/0; focused CTest 3/3.
- Windows MSVC Debug: reflection method invoke 2/0; reflection token resolve 7/0; method binding 2/0; metadata runtime
  query 24/0; focused CTest 3/3.
- Existing WSL clang/MSVC warning noise is unrelated to this parameter base-type guard slice.
- `git diff --check` exited 0 and reported only LF/CRLF line-ending warnings.

## Acceptance Decision
- Accepted for 10-S2E / 10-S3I.
- This closes only the fixed-parameter concrete baseType guard before registered invoker dispatch.
- Typed unboxing, numeric widening, nullable/ownership/staticCType compatibility, return boxing, public `MethodInfo`
  objects, MethodSpec-specific generated function slots, cross-module token rewrite, and full trim analyzer remain
  open.
