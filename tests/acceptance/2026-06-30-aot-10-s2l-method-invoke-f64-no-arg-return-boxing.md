# AOT 10-S2L / 10-S3P Method.Invoke f64 No-Arg Return Boxing

## Scope

This acceptance record covers the generated AOT C reflection invoker bucket for f64 no-arg returns.

The slice closes only the generated `Method.Invoke` return-boxing path for methods that:

- have a MethodInfo signature with `hasReturnValue`;
- declare `returnType->baseType == ZR_VALUE_TYPE_DOUBLE`;
- declare `parameterCount == 0`;
- have an emitted `zr_aot_typed_f64_fn_<functionIndex>()` helper.

Unsupported cases still fall back to the full `FZrAotEntryThunk` execution thunk without writing `outReturn`.

## Baseline

Before this slice, generated reflection invokers handled i64, u64, and bool no-arg return boxing. A f64 no-arg MethodInfo invocation still ran the full entry thunk, whose return value is an execution-success flag, not the method business return.

## Test Inventory

- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - Requires the f64 reflection bucket source contract.
  - Checks the f64 thunk header include, eligibility predicate, generated case writer, emitted `zr_aot_try_invoke_f64_no_arg(...)`, `ZR_VALUE_TYPE_DOUBLE` guard, typed f64 helper call, `ZrCore_Value_InitAsFloat(...)`, and shared invoker dispatch.
- `tests/parser/test_aot_c_shared_library_smoke.c`
  - Adds `pub func ratio(): float { return 2.5; }`.
  - Checks generated text for `case 4u`, `zr_aot_typed_f64_fn_4()`, and boxed-float writing.
  - Invokes `module->methodInfos[4]->invoker(...)` through the generated shared library and verifies `ZR_VALUE_TYPE_DOUBLE` with value `2.5`.

## RED

Command:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'
```

Expected failure before implementation:

```text
Missing source contract text: static TZrBool backend_aot_c_method_metadata_has_f64_no_arg_reflection_case(
```

## GREEN

Command:

```text
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

Accepted for 10-S2L / 10-S3P.

This slice only closes generated f64 no-arg return boxing. Argument unboxing, object/inline returns, numeric widening, complete signature buckets, public `MethodInfo` materialization, MethodSpec-specific generated code slots, cross-module token rewrite, and full trim analysis remain open.
