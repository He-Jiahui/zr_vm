# AOT 10-S2D / 10-S3H Method.Invoke Signature Shape Guard

## Scope
- Extends the counted token-driven Method.Invoke dispatcher with a defensive `SZrAotSignature` shape check.
- The dispatcher rejects incomplete MethodInfo signatures before calling the registered invoker:
  - `parameterCount > 0` requires `parameterTypes`.
  - `hasReturnValue` requires `returnType`.
- This slice does not implement typed argument compatibility, parameter unboxing, return boxing, public `MethodInfo`
  object materialization, MethodSpec-specific code slots, cross-module token rewrite, or full trim analysis.

## Baseline
- `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)` resolved a method token to MethodInfo/function pointer/invoker
  and checked arity with `parameterCount/hasVarArgs`.
- It did not reject signatures that declared parameters without a parameter-type table, or declared a return value
  without a return type carrier.

## Test Inventory
- `tests/module/test_reflection_method_invoke.c`
  - `test_reflection_invoke_method_token_rejects_incomplete_signature_shape`
  - Verifies a fixed-parameter signature with null `parameterTypes` is rejected without dispatch.
  - Verifies supplying `parameterTypes` allows dispatch.
  - Verifies a return-value signature with null `returnType` is rejected without dispatch.
  - Verifies supplying `returnType` allows dispatch.
- Adjacent regression:
  - `tests/module/test_reflection_token_resolve.c`
  - `tests/module/test_metadata_runtime_method_binding.c`
  - `tests/module/test_metadata_runtime_query.c`

## Tooling Evidence
- RED setup:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8'`
  - Initially failed because the build directory had not been reconfigured and did not yet know the new target. This
    was setup evidence, not accepted as the behavioral RED.
- RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc >/tmp/zr_vm_cmake_reconfigure.log && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test'`
  - Failed in `test_reflection_invoke_method_token_rejects_incomplete_signature_shape` with
    `Expected FALSE Was TRUE`.
- GREEN and regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ctest --test-dir build-wsl-gcc -R "^(reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-clang >/tmp/zr_vm_cmake_clang_reconfigure.log && cmake --build build-wsl-clang --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R "^(reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure'`
  - MSVC Debug with imported Visual Studio environment:
    `cmake -S . -B build-msvc`
    `cmake --build build-msvc --config Debug --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8`
    plus the four focused executables.
  - `ctest --test-dir build-msvc -C Debug -R "^(reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure`
  - `git diff --check -- tests/CMakeLists.txt tests/module/test_reflection_method_invoke.c tests/module/test_reflection_token_resolve.c zr_vm_core/src/zr_vm_core/reflection_token_resolve.c docs/plans/aot/10-reflection.md docs/plans/aot/11-metadata.md docs/plans/aot/index.md docs/module-system/typed-module-metadata.md .codex/sessions/20260620-2321-aot-07-12-codegen.md tests/acceptance/2026-06-30-aot-10-s2d-method-invoke-signature-shape-guard.md`

## Results
- WSL gcc: reflection method invoke 1/0; reflection token resolve 7/0; method binding 2/0; metadata runtime query
  24/0; focused CTest 3/3.
- WSL clang: reflection method invoke 1/0; reflection token resolve 7/0; method binding 2/0; metadata runtime query
  24/0; focused CTest 3/3.
- Windows MSVC Debug: reflection method invoke 1/0; reflection token resolve 7/0; method binding 2/0; metadata runtime
  query 24/0; focused CTest 3/3.
- WSL clang and MSVC builds still emit existing execution-dispatch computed-goto/unused-code warnings unrelated to
  this signature-shape guard slice.
- `git diff --check` exited 0 and reported only LF/CRLF line-ending warnings.

## Acceptance Decision
- Accepted for 10-S2D / 10-S3H.
- This closes only the token-driven Method.Invoke signature shape boundary.
- Typed argument compatibility, parameter type unboxing, return boxing, public `MethodInfo` objects,
  MethodSpec-specific generated function slots, cross-module token rewrite, and full trim analyzer remain open.
