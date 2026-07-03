# AOT 10-S2B / 10-S3F Token-Driven Method.Invoke Dispatcher

## Scope
- Adds a minimal public dispatcher for token-driven method invocation.
- The dispatcher resolves a method token, requires the AOT MethodInfo/function pointer/invoker carrier, and calls the
  registered invoker.
- This slice does not implement signature-aware argument unboxing, return boxing, public `MethodInfo` object
  materialization, or AOT/interpreter equivalence.

## Baseline
- `ZrCore_Reflection_ResolveToken()` could expose MethodInfo, entry thunk, and invoker fields for MethodDef and
  MethodSpec-related method tokens.
- No public API consumed those fields to dispatch a token to the registered AOT invoker.

## Test Inventory
- `tests/module/test_reflection_token_resolve.c`
  - `test_reflection_invoke_method_token_dispatches_aot_invoker`
  - Verifies `ZrCore_Reflection_InvokeMethodToken(...)` passes state, entry thunk, MethodInfo, receiver, arguments,
    and return target to the registered invoker.
  - Verifies null state/runtime/outReturn and non-method tokens are rejected without dispatch.
- Adjacent regression:
  - `tests/module/test_metadata_runtime_method_binding.c`
  - `tests/module/test_metadata_runtime_query.c`

## Tooling Evidence
- RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8'`
  - Failed with implicit declaration and undefined references for `ZrCore_Reflection_InvokeMethodToken`.
- GREEN and regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test'`
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test'`
  - MSVC Debug with imported Visual Studio environment:
    `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8`
    plus the three focused executables.
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^(reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure'`
  - `ctest --test-dir build-msvc -C Debug -R "^(reflection_token_resolve|metadata_runtime_method_binding)$" --output-on-failure`
  - `git diff --check -- tests/module/test_reflection_token_resolve.c zr_vm_core/include/zr_vm_core/reflection.h zr_vm_core/src/zr_vm_core/reflection_token_resolve.c docs/plans/aot/10-reflection.md docs/plans/aot/11-metadata.md docs/plans/aot/index.md docs/module-system/typed-module-metadata.md .codex/sessions/20260620-2321-aot-07-12-codegen.md tests/acceptance/2026-06-30-aot-10-s2b-token-driven-method-invoke-dispatcher.md`

## Results
- WSL gcc: reflection token resolve 6/0; method binding 2/0; metadata runtime query 24/0; focused CTest 2/2.
- WSL clang: reflection token resolve 6/0; method binding 2/0; metadata runtime query 24/0.
- Windows MSVC Debug: reflection token resolve 6/0; method binding 2/0; metadata runtime query 24/0; focused CTest 2/2.
- WSL clang and MSVC builds still emit existing execution-dispatch computed-goto/unused-code warnings unrelated to
  this dispatcher slice.
- `git diff --check` exited 0 and reported only LF/CRLF line-ending warnings.

## Acceptance Decision
- Accepted for 10-S2B / 10-S3F.
- This closes only the token-to-registered-invoker control-flow gap.
- Signature-aware invoker buckets, parameter unboxing, return boxing, public method reflection objects,
  MethodSpec-specific generated function slots, cross-module token rewrite, and full trim analyzer remain open.
