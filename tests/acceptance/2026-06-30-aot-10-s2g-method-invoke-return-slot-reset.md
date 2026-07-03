# 10-S2G / 10-S3K Method.Invoke Return Slot Reset

Timestamp: 2026-06-30 04:06:53 +08:00

Status: Completed sub-slice. Full 10-S2 / 10-S3 / 11-S2 remains open.

Plan references:
- `docs/plans/aot/10-reflection.md` §1 / §2
- `docs/plans/aot/11-metadata.md` §2

## Scope

The counted token-driven `Method.Invoke` dispatcher now clears the required return slot before invoking registered AOT code. When `methodInfo->signature->hasReturnValue` is true, `outReturn` is reset to null before dispatch so the return base-type post-guard can only accept a value written by the current invoker call.

This closes the stale-output case where callers could prefill `outReturn` with a matching type and an invoker that failed to write a fresh return value would still appear successful.

## RED

The new focused test in `tests/module/test_reflection_method_invoke.c` prefilled `outReturn` with a bool value under a bool-return signature, then used the synthetic invoker without writing a return value.

Command:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test'
```

Observed failure:

```text
test_reflection_invoke_method_token_rejects_stale_return_slot_when_invoker_does_not_write:FAIL: Expected FALSE Was TRUE
```

## GREEN

`zr_vm_core/src/zr_vm_core/reflection_token_resolve.c` now resets required `outReturn` slots before dispatch. The older counted arity fixture in `tests/module/test_reflection_token_resolve.c` was adjusted so success paths get their declared int64 return from the synthetic invoker instead of relying on a prefilled output slot.

Focused rerun passed:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test'
```

Result: `4 Tests 0 Failures 0 Ignored`.

## Validation

WSL gcc:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test'
```

Passed: reflection method invoke 4/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.

WSL clang:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test'
```

Passed: reflection method invoke 4/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.

Windows MSVC Debug:

```powershell
cmake --build build-msvc --config Debug --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8
build-msvc\bin\Debug\zr_vm_reflection_method_invoke_test.exe
build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe
build-msvc\bin\Debug\zr_vm_metadata_runtime_method_binding_test.exe
build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe
```

Passed: reflection method invoke 4/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.

CTest:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R "reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query" --output-on-failure'
ctest --test-dir build-msvc -C Debug -R "reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query" --output-on-failure
```

Passed: 4/4 on WSL gcc, WSL clang, and Windows MSVC Debug.

`git diff --check` exited 0 with existing line-ending warnings only.

## Non-Goals

- No return boxing or typed return register capture.
- No parameter unboxing, numeric widening, nullable/ownership/staticCType compatibility, or signature-aware invoker buckets.
- No public `MethodInfo` reflection object.
- No generated target call-frame equivalence proof.
- No cross-module token rewrite or full trim analyzer.
