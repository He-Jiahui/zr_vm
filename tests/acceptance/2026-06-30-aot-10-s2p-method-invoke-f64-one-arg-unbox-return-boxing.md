# AOT 10-S2P Method.Invoke F64 One-Arg Unbox And Return Boxing

## Scope
- Generated AOT C reflection invoker bucket for `float(float)` / f64 one-arg calls.
- Plan coverage: `10-S2P` argument marshaling, `10-S3T` generated reflection invocation, and `11-S2D` MethodInfo/functionIndex binding consumption.
- Layers covered: AOT C backend reflection invoker emission, typed f64 thunk predicate exposure, parser source contracts, Unix shared-library smoke, and reflection/metadata adjacent runtime tests.

## Baseline
- Before this slice, generated `Method.Invoke` had no-arg i64/u64/bool/f64 return-boxing buckets and i64/u64/bool one-arg unbox buckets, but no f64 one-arg bucket.
- RED command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'`
- RED result: the source contract failed on missing `static TZrBool backend_aot_c_method_metadata_has_f64_one_arg_reflection_case(`.
- Existing baseline: MSVC shared-library smoke remains Unix-only ignored, and existing MSVC warnings in unrelated files remain warning-only.

## Implementation
- `backend_aot_c_typed_f64_thunks.h` exposes `backend_aot_c_can_emit_typed_f64_one_arg_thunk(...)`.
- `backend_aot_c_reflection_invokers.c` emits `zr_aot_try_invoke_f64_one_arg(...)`.
- The generated helper checks non-null method/signature/return type, double return base type, `parameterCount == 1u`, non-null `parameterTypes`, double parameter base type, non-null args, `args[0].type == ZR_VALUE_TYPE_DOUBLE`, and non-null `outReturn`.
- Matched functionIndex cases unbox `args[0].value.nativeObject.nativeDouble`, call `zr_aot_typed_f64_fn_<index>(zr_aot_arg0)`, and box through `ZrCore_Value_InitAsFloat(...)`.
- Unsupported functionIndex cases continue to fall through to the shared entry thunk without interpreting the entry thunk execution-success flag as a business return value.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the f64 one-arg generated helper, predicate, case writer, parameter guard, argument type guard, native double unbox, typed f64 call, and dispatcher path.
- `tests/parser/test_aot_c_shared_library_smoke.c` adds `echo_ratio(value: float): float`, checks generated `case 8u`, invokes the generated reflection invoker with 1.75, and verifies boxed `ZR_VALUE_TYPE_DOUBLE` 1.75.
- Existing focused runtime tests remain adjacent coverage for public Method.Invoke shape/arity/base-type/return-slot behavior and MethodInfo binding consumers.

## Tooling Evidence
- WSL gcc GREEN:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8`
  plus `zr_vm_aot_c_frame_setup_contracts_test` 1/0 and `zr_vm_aot_c_shared_library_smoke_test` 13/0.
- WSL gcc focused matrix passed: frame setup contracts 1/0, source contracts 22/0, shared-library smoke 13/0, reflection method invoke 5/0, reflection token resolve 7/0, metadata runtime method binding 2/0, and metadata runtime query 24/0.
- WSL clang focused matrix passed with the same counts.
- Windows MSVC Debug focused matrix passed with the same counts, with shared-library smoke reported as 13 ignored Unix-only cases.
- CTest adjacency passed 7/7 on WSL gcc, WSL clang, and Windows MSVC Debug for:
  `metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature`.
- `git diff --check` returned 0; only Git line-ending conversion warnings were reported, with no whitespace errors.

## Acceptance Decision
- Accepted as a support sub-slice for `10-S2P / 10-S3T`.
- This closes only the generated f64 one-arg reflection bucket.
- Remaining work: multi-argument buckets, object/inline returns, numeric widening, instance receiver handling, public `MethodInfo` materialization, MethodSpec-specific generated function slots, cross-module token rewrite, trim diagnostics, and the full trim analyzer.
