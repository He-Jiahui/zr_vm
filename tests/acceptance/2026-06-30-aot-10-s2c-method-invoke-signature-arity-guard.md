# AOT 10-S2C / 10-S3G Method.Invoke Signature Arity Guard

## Scope
- Adds a counted public dispatcher for token-driven method invocation.
- The dispatcher resolves a method token, requires the AOT MethodInfo/function pointer/invoker carrier, checks the
  MethodInfo signature arity, and only then calls the registered invoker.
- This slice does not implement typed parameter unboxing, return boxing, signature type validation, public `MethodInfo`
  object materialization, or AOT/interpreter equivalence.

## Baseline
- `ZrCore_Reflection_InvokeMethodToken(...)` could dispatch a method token to the registered invoker.
- The uncounted dispatcher did not expose an argument-count boundary, so tests could not prove MethodInfo signature
  arity was consulted before dispatch.

## Test Inventory
- `tests/module/test_reflection_token_resolve.c`
  - `test_reflection_invoke_method_token_checks_signature_argument_count`
  - Verifies exact non-varargs arity dispatches to the invoker.
  - Verifies too-few arguments, too-many non-varargs arguments, and null `args` with nonzero `argCount` are rejected
    without dispatch.
  - Verifies varargs signatures accept `argCount >= parameterCount`.
- Adjacent regression:
  - `tests/module/test_metadata_runtime_method_binding.c`
  - `tests/module/test_metadata_runtime_query.c`

## Tooling Evidence
- RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8'`
  - Failed with implicit declaration and undefined references for
    `ZrCore_Reflection_InvokeMethodTokenWithArgCount`.
- GREEN and regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test'`
  - MSVC Debug with imported Visual Studio environment:
    `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8`
    plus the three focused executables.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^(reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure'`
  - `ctest --test-dir build-msvc -C Debug -R "^(reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure`
  - `git diff --check -- tests/module/test_reflection_token_resolve.c zr_vm_core/include/zr_vm_core/reflection.h zr_vm_core/src/zr_vm_core/reflection_token_resolve.c docs/plans/aot/10-reflection.md docs/plans/aot/11-metadata.md docs/plans/aot/index.md docs/module-system/typed-module-metadata.md .codex/sessions/20260620-2321-aot-07-12-codegen.md tests/acceptance/2026-06-30-aot-10-s2c-method-invoke-signature-arity-guard.md`

## Results
- WSL gcc: reflection token resolve 7/0; method binding 2/0; metadata runtime query 24/0; focused CTest 2/2.
- WSL clang: reflection token resolve 7/0; method binding 2/0; metadata runtime query 24/0.
- Windows MSVC Debug: reflection token resolve 7/0; method binding 2/0; metadata runtime query 24/0; focused CTest 2/2.
- WSL clang and MSVC builds still emit existing execution-dispatch computed-goto/unused-code warnings unrelated to
  this arity-guard slice.
- `git diff --check` exited 0 and reported only LF/CRLF line-ending warnings.

## Acceptance Decision
- Accepted for 10-S2C / 10-S3G.
- This closes only the token-driven invoke signature arity boundary.
- Signature-aware typed invoker buckets, parameter type unboxing, return boxing, signature type validation, public
  method reflection objects, MethodSpec-specific generated function slots, cross-module token rewrite, and full trim
  analyzer remain open.
