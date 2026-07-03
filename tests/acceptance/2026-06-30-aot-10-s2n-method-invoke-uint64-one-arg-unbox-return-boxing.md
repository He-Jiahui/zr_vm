# AOT 10-S2N / 10-S3R Method.Invoke UInt64 One-Arg Unbox Return Boxing

## Scope

This acceptance record covers the generated AOT C reflection invoker bucket for `uint64(uint64)` methods.

The slice closes only the generated `Method.Invoke` argument-unbox and return-boxing path for methods that:

- have a MethodInfo signature with `hasReturnValue`;
- declare `returnType->baseType == ZR_VALUE_TYPE_UINT64`;
- declare `parameterCount == 1`;
- declare `parameterTypes[0].baseType == ZR_VALUE_TYPE_UINT64`;
- receive an `args[0]` value whose runtime type is `ZR_VALUE_TYPE_UINT64`;
- have an emitted `zr_aot_typed_u64_fn_<functionIndex>(TZrUInt64)` helper.

Unsupported cases still fall back to the full `FZrAotEntryThunk` execution thunk without writing `outReturn`.

## Baseline

Before this slice, generated reflection invokers handled no-arg scalar return boxing for i64, u64, bool, and f64, plus the first one-arg `int64(int64)` argument-unbox bucket. A one-arg uint64 MethodInfo invocation could pass through the shared invoker only as a full execution thunk, with no typed uint64 argument unbox and no business return boxing.

## Test Inventory

- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - Requires the u64 one-arg reflection bucket source contract.
  - Checks the u64 one-arg eligibility predicate, generated case writer, emitted `zr_aot_try_invoke_u64_one_arg(...)`, single-arg signature guard, argument runtime type guard, `args[0]` unbox expression, typed helper call, `ZrCore_Value_InitAsUInt(...)`, and shared invoker dispatch.
- `tests/parser/test_aot_c_shared_library_smoke.c`
  - Adds `pub func echo_unsigned(value: uint): uint { return value; }`.
  - Checks generated text for `case 6u`, `zr_aot_typed_u64_fn_6(zr_aot_arg0)`, and boxed-uint writing.
  - Invokes `module->methodInfos[6]->invoker(...)` through the generated shared library with argument 101 and verifies `ZR_VALUE_TYPE_UINT64` with value `101`.

## RED

Command:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'
```

Expected failure before implementation:

```text
Missing source contract text: static TZrBool backend_aot_c_method_metadata_has_u64_one_arg_reflection_case(
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

Accepted for 10-S2N / 10-S3R.

This slice only closes generated uint64 one-arg argument unbox and return boxing. Multi-argument buckets, other scalar parameters, object/inline returns, numeric widening, instance receiver handling, complete signature buckets, public `MethodInfo` materialization, MethodSpec-specific generated code slots, cross-module token rewrite, and full trim analysis remain open.
