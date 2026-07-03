# AOT 10-S2J / 10-S3N Method.Invoke UInt64 No-Arg Return Boxing

## Scope

- Generated AOT C reflection invokers now include a narrow uint64/no-arg return-boxing bucket.
- Affected layers: AOT C generator, typed u64 thunk predicate header, generated shared-library descriptor/smoke, MethodInfo signature consumers, and reflection invoke metadata carrier tests.

## Baseline

- 10-S2I left generated `Method.Invoke` return boxing with only the int64/no-arg bucket.
- The full `FZrAotEntryThunk` return value is already proven to be an execution success flag, so uint64 business returns must be read through the generated typed u64 helper.
- Existing build warning baselines remain unchanged; MSVC shared-library smoke still ignores Unix-only dynamic-library cases.

## Test Inventory

- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the generated u64 reflection bucket helper, typed u64 predicate, generated cases, `ZrCore_Value_InitAsUInt(...)`, and invoker dispatch order.
- `tests/parser/test_aot_c_shared_library_smoke.c` compiles generated C, loads the module descriptor, invokes `unsigned_answer(): uint` through its generated reflection invoker, and verifies boxed uint64 value `13`.
- Reflection/metadata focused coverage still runs `test_reflection_method_invoke.c`, `test_reflection_token_resolve.c`, `test_metadata_runtime_method_binding.c`, and `test_metadata_runtime_query.c`.
- Boundary covered: uint64 return, zero fixed parameters, valid return slot, and `method->functionIndex` dispatch. Unsupported signatures still execute fallback without writing `outReturn`.
- Negative coverage remains from earlier arity, signature-shape, return-slot, and return-base-type guards.

## Tooling Evidence

- WSL gcc focused build/run passed: frame setup contracts 1/0, source contracts 22/0, shared-library smoke 13/0, reflection method invoke 5/0, reflection token resolve 7/0, method binding 2/0, metadata runtime query 24/0.
- WSL clang focused build/run passed with the same counts.
- Windows MSVC Debug focused build/run passed with the same counts, with shared-library smoke reporting 13 ignored Unix-only cases.
- CTest passed 7/7 on WSL gcc, WSL clang, and Windows MSVC Debug for `metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature`.
- `git diff --check` exited 0 and only reported existing LF/CRLF normalization warnings.

Commands:

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test'
cmake --build build-msvc --config Debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test --parallel 8
.\build-msvc\bin\Debug\zr_vm_aot_c_frame_setup_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_shared_library_smoke_test.exe; .\build-msvc\bin\Debug\zr_vm_reflection_method_invoke_test.exe; .\build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe; .\build-msvc\bin\Debug\zr_vm_metadata_runtime_method_binding_test.exe; .\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_runtime_typed_direct_call_compatibility_test zr_vm_aot_c_metadata_binding_loader_test zr_vm_aot_c_method_info_signature_test -j 8 && ctest --test-dir build-wsl-gcc --output-on-failure -R "metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature"'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_runtime_typed_direct_call_compatibility_test zr_vm_aot_c_metadata_binding_loader_test zr_vm_aot_c_method_info_signature_test -j 8 && ctest --test-dir build-wsl-clang --output-on-failure -R "metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature"'
cmake --build build-msvc --config Debug --target zr_vm_aot_runtime_typed_direct_call_compatibility_test zr_vm_aot_c_metadata_binding_loader_test zr_vm_aot_c_method_info_signature_test --parallel 8
ctest --test-dir build-msvc -C Debug --output-on-failure -R "metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature"
git diff --check
```

## Results

- RED: generated source contract failed on missing `backend_aot_c_method_metadata_has_u64_no_arg_reflection_case(`, proving the test detected the absent u64 reflection bucket.
- GREEN: generated `zr_aot_try_invoke_u64_no_arg(...)` dispatches by `functionIndex`, calls `zr_aot_typed_u64_fn_<index>()`, and boxes the result with `ZrCore_Value_InitAsUInt(...)`.
- Runtime smoke proved `unsigned_answer(): uint` returns boxed `ZR_VALUE_TYPE_UINT64` with native value `13`.

## Acceptance Decision

Accepted for the narrow generated uint64/no-arg reflection return-boxing bucket.

Remaining risks and non-goals: argument unboxing, bool/f64/object/inline returns, numeric widening, complete signature buckets, typed target ABI carrier, MethodSpec specialized code slot, public `MethodInfo` object materialization, cross-module token rewrite, and full trim analyzer.
