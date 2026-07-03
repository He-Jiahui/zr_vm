# AOT 10-S2 Maint Reflection Invoker Emitter Split

## Scope

This acceptance record covers the maintenance split that moves generated reflection invoker emission out of `backend_aot_c_method_metadata.c` into `backend_aot_c_reflection_invokers.h/.c`.

The slice does not add a new `Method.Invoke` signature bucket. It preserves the existing generated behavior for:

- i64 no-arg return boxing;
- u64 no-arg return boxing;
- bool no-arg return boxing;
- f64 no-arg return boxing;
- i64 one-arg argument unbox + return boxing;
- u64 one-arg argument unbox + return boxing.

## Baseline

After 10-S2N / 10-S3R, `backend_aot_c_method_metadata.c` owned both MethodInfo metadata emission and reflection invoker bucket emission, and had grown to 948 lines. More buckets would have continued to add unrelated invoker-generator responsibility to the MethodInfo metadata module.

## Implementation

- Added `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.h`.
- Added `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.c`.
- Moved `backend_aot_write_c_reflection_invokers(...)` and its generated bucket case helpers into the new module.
- Updated `backend_aot_c_emitter.c` to include the new invoker header directly.
- Removed the invoker emitter API from `backend_aot_c_method_metadata.h`.
- Kept `backend_aot_c_method_metadata.c` scoped to MethodInfo/signature/method-token/GC-root metadata emission.
- Updated `tests/parser/test_aot_c_frame_setup_contracts.c` to read the new invoker source file and keep the existing generated bucket source contracts enforced.

## Result

- `backend_aot_c_method_metadata.c`: 552 lines after the split.
- `backend_aot_c_reflection_invokers.c`: 397 lines.

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

Accepted as a 10-S2/10-S3 maintenance prerequisite for future generated reflection invoker buckets.

This slice only closes module-boundary cleanup. Multi-argument buckets, other scalar parameters, object/inline returns, numeric widening, instance receiver handling, complete signature buckets, public `MethodInfo` materialization, MethodSpec-specific generated code slots, cross-module token rewrite, and full trim analysis remain open.
