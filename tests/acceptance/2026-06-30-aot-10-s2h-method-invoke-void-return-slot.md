# 10-S2H / 10-S3L Method.Invoke Void Return Slot

Timestamp: 2026-06-30 04:17:27 +08:00

Status: Completed sub-slice. Full 10-S2 / 10-S3 / 11-S2 remains open.

Plan references:
- `docs/plans/aot/10-reflection.md` §1 / §2
- `docs/plans/aot/11-metadata.md` §2

## Scope

The counted token-driven `Method.Invoke` dispatcher now canonicalizes void/no-return methods by resetting `outReturn` to null after registered invoker dispatch when `methodInfo->signature->hasReturnValue` is false.

This prevents a no-return method from exposing an invoker-written value or a stale caller-provided output slot through the public return slot.

## RED

The new focused test in `tests/module/test_reflection_method_invoke.c` used a no-return MethodInfo signature, prefilled the return slot, then configured the synthetic invoker to write an int64 return anyway.

Command:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test'
```

Observed failure:

```text
test_reflection_invoke_method_token_clears_void_return_slot_after_dispatch:FAIL: Expected 0 Was 5
```

## GREEN

`zr_vm_core/src/zr_vm_core/reflection_token_resolve.c` now resets `outReturn` to null after dispatch for no-return signatures. The call still succeeds and the invoker still runs, but the public output slot is normalized to null.

Focused rerun passed:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test'
```

Result: `5 Tests 0 Failures 0 Ignored`.

## Validation

WSL gcc:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test'
```

Passed: reflection method invoke 5/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.

WSL clang:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test'
```

Passed: reflection method invoke 5/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.

Windows MSVC Debug:

```powershell
cmake --build build-msvc --config Debug --target zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8
build-msvc\bin\Debug\zr_vm_reflection_method_invoke_test.exe
build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe
build-msvc\bin\Debug\zr_vm_metadata_runtime_method_binding_test.exe
build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe
```

Passed: reflection method invoke 5/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.

CTest:

```sh
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang -R "reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query" --output-on-failure'
ctest --test-dir build-msvc -C Debug -R "reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query" --output-on-failure
```

Passed: 4/4 on WSL gcc, WSL clang, and Windows MSVC Debug.

## Non-Goals

- No return boxing or typed return register capture.
- No parameter unboxing, numeric widening, nullable/ownership/staticCType compatibility, or signature-aware invoker buckets.
- No public `MethodInfo` reflection object.
- No generated target call-frame equivalence proof.
- No cross-module token rewrite or full trim analyzer.
