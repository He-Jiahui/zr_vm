# AOT 10-S2S Method.Invoke Bool Two-Arg Unbox And Return Boxing

## Scope
- Generated AOT C reflection invoker bucket for `bool(bool, bool)` calls.
- Plan coverage: `10-S2S` argument marshaling, `10-S3W` generated reflection invocation, and `11-S2D` MethodInfo/functionIndex binding consumption.
- Layers covered: AOT C backend reflection invoker emission, typed bool two-arg thunk predicate exposure, parser source contracts, Unix shared-library smoke, and reflection/metadata adjacent runtime tests.

## Baseline
- Before this slice, generated `Method.Invoke` had no-arg i64/u64/bool/f64 return-boxing buckets, one-arg i64/u64/bool/f64 unbox buckets, and i64/u64 two-arg buckets, but no bool two-argument bucket.
- RED command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'`
- RED result: the source contract failed on missing `static TZrBool backend_aot_c_method_metadata_has_bool_two_arg_reflection_case(`.
- Existing baseline: MSVC shared-library smoke remains Unix-only ignored, and existing MSVC warnings in unrelated files remain warning-only.

## Implementation
- `backend_aot_c_typed_bool_thunks.h` exposes `backend_aot_c_can_emit_typed_bool_two_arg_thunk(...)`.
- `backend_aot_c_reflection_invokers.c` emits `zr_aot_try_invoke_bool_two_arg(...)`.
- The generated helper checks non-null method/signature/return type, bool return base type, `parameterCount == 2u`, non-null `parameterTypes`, bool parameter base type for both declared parameters, non-null args, `args[0].type == ZR_VALUE_TYPE_BOOL`, `args[1].type == ZR_VALUE_TYPE_BOOL`, and non-null `outReturn`.
- Matched functionIndex cases unbox both `nativeBool` payloads, call `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1)`, and box through `ZrCore_Value_InitAsBool(...)`.
- Unsupported functionIndex cases continue to fall through to the shared entry thunk without interpreting the entry thunk execution-success flag as a business return value.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the bool two-arg generated helper, predicate, case writer, second-parameter guard, second-argument type guard, second native bool unbox, typed bool call shape, and dispatcher path.
- `tests/parser/test_aot_c_shared_library_smoke.c` adds `same_truth(left: bool, right: bool): bool`, checks generated `case 11u`, invokes the generated reflection invoker with true and true, and verifies boxed `ZR_VALUE_TYPE_BOOL` true.
- Existing focused runtime tests remain adjacent coverage for public Method.Invoke shape/arity/base-type/return-slot behavior and MethodInfo binding consumers.

## Tooling Evidence
- Focused GREEN:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test'`
  passed with frame setup contracts 1/0 and shared-library smoke 13/0.
- WSL gcc focused matrix command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test'`
  passed: frame setup contracts 1/0, source contracts 22/0, shared-library smoke 13/0, reflection method invoke 5/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.
- WSL clang focused matrix used the same target and binary list under `build-wsl-clang` and passed with the same counts.
- Windows MSVC Debug focused matrix used `build\codex-msvc-debug` with the same targets and Debug binaries, passed with the same counts, and shared-library smoke reported 13 ignored Unix-only cases.
- CTest adjacency passed 7/7 on WSL gcc, WSL clang, and Windows MSVC Debug for:
  `metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature`.

## Results
- Passed checks: all focused direct binaries and the three-platform CTest adjacency matrix listed above.
- Failed checks: the intended RED source-contract failure before implementation; no post-implementation failures remained in the focused matrix.
- Fixes made in response: added typed bool two-arg predicate exposure and generated reflection invoker two-arg bool marshaling.

## Acceptance Decision
- Accepted as a support sub-slice for `10-S2S / 10-S3W`.
- This closes only the generated bool(bool,bool) two-arg reflection bucket.
- Remaining work: bool-return numeric comparison buckets, f64 two-arg buckets, three-arg and wider buckets, object/inline returns, numeric widening, instance receiver handling, public `MethodInfo` materialization, MethodSpec-specific generated function slots, cross-module token rewrite, trim diagnostics, and the full trim analyzer.
