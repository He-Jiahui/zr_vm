# AOT 10-S2K / 10-S3O Method.Invoke bool no-arg return boxing

## Scope

- Close the generated `Method.Invoke` bool no-arg return-boxing bucket.
- Consume the existing MethodInfo/functionIndex carrier from 11-S2D; do not change AOT ABI or public reflection APIs.
- Keep unsupported signatures on the existing full entry-thunk fallback path.

## Baseline

- `10-S2I / 10-S3M` generated int64 no-arg return boxing through `zr_aot_typed_i64_fn_<index>()`.
- `10-S2J / 10-S3N` generated uint64 no-arg return boxing through `zr_aot_typed_u64_fn_<index>()`.
- Before this slice, generated reflection invokers did not box bool no-arg typed helper returns.

## Test Inventory

- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - Requires `backend_aot_c_typed_bool_thunks.h`, bool bucket predicate/case writer, bool return-type guard, `zr_aot_typed_bool_fn_%u()`, `ZrCore_Value_InitAsBool(...)`, and invoker dispatch.
- `tests/parser/test_aot_c_shared_library_smoke.c`
  - Adds `pub func truth(): bool { return true; }`.
  - Checks generated C text for `zr_aot_try_invoke_bool_no_arg(...)`, `case 3u`, typed bool helper invocation, and `ZrCore_Value_InitAsBool(...)`.
  - Invokes `module->methodInfos[3]->invoker(...)` and requires boxed `ZR_VALUE_TYPE_BOOL` with true native value.

## RED

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'
```

Result:

- Failed as expected.
- Failure: `Missing source contract text: static TZrBool backend_aot_c_method_metadata_has_bool_no_arg_reflection_case(`.

## GREEN

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test'
```

Result:

- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 13/0.

## Validation

Focused binaries passed on WSL gcc, WSL clang, and Windows MSVC Debug:

- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- `zr_vm_aot_c_source_contracts_test`: 22/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 13/0 on WSL; MSVC Debug 13/0/13 ignored Unix-only.
- `zr_vm_reflection_method_invoke_test`: 5/0.
- `zr_vm_reflection_token_resolve_test`: 7/0.
- `zr_vm_metadata_runtime_method_binding_test`: 2/0.
- `zr_vm_metadata_runtime_query_test`: 24/0.

CTest adjacency matrix passed 7/7 on WSL gcc, WSL clang, and Windows MSVC Debug:

```text
metadata_runtime_query
metadata_runtime_method_binding
reflection_token_resolve
reflection_method_invoke
aot_runtime_typed_direct_call_compatibility
aot_c_metadata_binding_loader
aot_c_method_info_signature
```

`git diff --check` passed; it only reported Git line-ending conversion warnings, with no whitespace errors.

## Acceptance Decision

Accepted for 10-S2K / 10-S3O.

This slice only closes generated bool no-arg return boxing. Argument unboxing, f64/object/inline returns, numeric widening, complete signature buckets, public `MethodInfo` materialization, MethodSpec-specific generated code slots, cross-module token rewrite, and full trim analysis remain open.
