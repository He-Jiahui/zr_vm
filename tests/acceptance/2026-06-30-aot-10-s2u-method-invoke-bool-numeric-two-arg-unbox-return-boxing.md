# AOT 10-S2U Method.Invoke Bool Numeric Two-Arg Unbox And Return Boxing

## Scope
- Generated AOT C reflection invoker buckets for `bool(int, int)`, `bool(uint, uint)`, and `bool(float, float)` numeric comparison calls.
- Plan coverage: `10-S2U` argument marshaling, `10-S3Y` generated reflection invocation, and `11-S2D` MethodInfo/functionIndex binding consumption.
- Layers covered: AOT C backend reflection invoker emission, typed bool numeric comparison two-arg thunk predicate exposure, parser source contracts, Unix shared-library smoke, and reflection/metadata adjacent runtime tests.

## Baseline
- Before this slice, generated `Method.Invoke` had no-arg i64/u64/bool/f64 return-boxing buckets, one-arg i64/u64/bool/f64 unbox buckets, and i64/u64/bool/f64 same-return two-arg buckets, but no bool-return numeric comparison two-argument buckets.
- RED command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'`
- RED result: the source contract failed on missing `static TZrBool backend_aot_c_method_metadata_has_bool_i64_two_arg_reflection_case(`.
- Existing baseline: MSVC shared-library smoke remains Unix-only ignored.

## Implementation
- `backend_aot_c_typed_bool_thunks.h` exposes existing bool-return numeric comparison predicates:
  `backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(...)`,
  `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(...)`, and
  `backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(...)`.
- `backend_aot_c_reflection_bool_numeric_invokers.h/.c` emits `zr_aot_try_invoke_bool_i64_two_arg(...)`,
  `zr_aot_try_invoke_bool_u64_two_arg(...)`, and `zr_aot_try_invoke_bool_f64_two_arg(...)`.
- Each generated helper checks non-null method/signature/return type, bool return base type, `parameterCount == 2u`,
  non-null `parameterTypes`, matching numeric parameter base types for both declared parameters, non-null args,
  matching runtime arg value types for both slots, and non-null `outReturn`.
- Matched functionIndex cases unbox both native payloads, call `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1)`,
  and box through `ZrCore_Value_InitAsBool(...)`.
- `backend_aot_c_reflection_invokers.c` remains the orchestration boundary and dispatches these helpers after
  `bool(bool, bool)` and before f64-return helpers.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the bool numeric helper names, predicates, case writers,
  second-parameter and second-argument guards, numeric payload unbox strings, bool return boxing, and dispatcher paths.
- `tests/parser/test_aot_c_shared_library_smoke.c` adds `less_values(left: int, right: int): bool`,
  `unsigned_after(left: uint, right: uint): bool`, and `ratio_equal(left: float, right: float): bool`; checks generated
  `case 13u`, `case 14u`, and `case 15u`; invokes each generated reflection invoker; and verifies boxed
  `ZR_VALUE_TYPE_BOOL` true results.
- Existing focused runtime tests remain adjacent coverage for public Method.Invoke shape/arity/base-type/return-slot
  behavior and MethodInfo binding consumers.

## Tooling Evidence
- Focused GREEN:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test'`
  passed with frame setup contracts 1/0 and shared-library smoke 13/0.
- WSL gcc focused matrix command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_reflection_method_invoke_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_method_binding_test zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_method_invoke_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_method_binding_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test'`
  passed: frame setup contracts 1/0, source contracts 22/0, shared-library smoke 13/0, reflection method invoke 5/0,
  reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.
- WSL clang focused matrix used the same target and binary list under `build-wsl-clang` and passed with the same counts.
- Windows MSVC Debug focused matrix used `build-msvc` with the same targets and Debug binaries, passed with the same counts,
  and shared-library smoke reported 13 ignored Unix-only cases.
- CTest adjacency passed 7/7 on WSL gcc, WSL clang, and Windows MSVC Debug for:
  `metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature`.

## Results
- Passed checks: all focused direct binaries and the three-platform CTest adjacency matrix listed above.
- Failed checks: the intended RED source-contract failure before implementation; no post-implementation failures remained in the focused matrix.
- Fixes made in response: exposed typed bool numeric comparison two-arg predicates, added a focused bool numeric reflection invoker module, and wired generated helper dispatch.
- File-size check: `backend_aot_c_reflection_invokers.c` is 935 lines, `backend_aot_c_reflection_bool_numeric_invokers.c` is 240 lines, and `backend_aot_c_method_metadata.c` is 646 lines after this slice.

## Acceptance Decision
- Accepted as a support sub-slice for `10-S2U / 10-S3Y`.
- This closes only generated bool-return numeric comparison two-arg reflection buckets.
- Remaining work: three-arg and wider buckets, object/inline returns, numeric widening, instance receiver handling, public `MethodInfo` materialization, MethodSpec-specific generated function slots, cross-module token rewrite, trim diagnostics, and the full trim analyzer.
