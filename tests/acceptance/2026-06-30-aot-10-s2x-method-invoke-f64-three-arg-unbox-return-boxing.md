# AOT 10-S2X Method.Invoke F64 Three-Arg Unbox And Return Boxing

## Scope
- Generated AOT C reflection invoker bucket for `double(double, double, double)` calls.
- Plan coverage: `10-S2X` argument marshaling, `10-S3AB` generated reflection invocation, and `11-S2D` MethodInfo/functionIndex binding consumption.
- Layers covered: AOT C backend reflection invoker emission, typed f64 three-arg thunk predicate consumption, parser source contracts, Unix shared-library smoke, and reflection/metadata adjacent runtime tests.

## Baseline
- Before this slice, generated `Method.Invoke` had scalar no-arg, one-arg, two-arg, bool-return numeric comparison two-arg, int64 three-arg, and uint64 three-arg buckets, but no f64 three-argument bucket.
- RED command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'`
- RED result: the source contract failed on missing `static TZrBool backend_aot_c_method_metadata_has_f64_three_arg_reflection_case(`.
- Existing baseline: MSVC shared-library smoke remains Unix-only ignored.

## Implementation
- `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c` now emits `zr_aot_try_invoke_f64_three_arg(...)` alongside the existing i64 and u64 three-arg helpers.
- `backend_aot_c_typed_f64_thunks.h` exposes the existing `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` and
  `backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk()` predicates so the reflection writer can select eligible generated cases.
- The generated helper checks non-null method/signature/return type, double return base type, `parameterCount == 3u`,
  non-null `parameterTypes`, double parameter base type for all three declared parameters, non-null args,
  matching runtime double value type for all three slots, and non-null `outReturn`.
- Matched state-free functionIndex cases unbox all three `nativeDouble` payloads, call
  `zr_aot_typed_f64_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`, and box through
  `ZrCore_Value_InitAsFloat(...)`.
- Matched divide/modulo functionIndex cases keep the stateful helper call shape
  `zr_aot_typed_f64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)` so generated divide/modulo-by-zero
  diagnostics remain available.
- `backend_aot_c_reflection_invokers.c` remains the orchestration boundary and dispatches this helper after
  `double(double, double)` and before bool-return helpers.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the numeric three-arg split source file, helper name,
  f64 three-arg predicates, case writer, third-parameter and third-argument guards, third native double unbox,
  state-free/stateful typed f64 call shapes, and dispatcher path.
- `tests/parser/test_aot_c_shared_library_smoke.c` adds
  `sum_three_ratio(left: float, middle: float, right: float): float`; checks generated `case 18u`; verifies method token
  `0x03000012u`; invokes the generated reflection invoker with 1.5, 2.25, and 3.25; and verifies boxed
  `ZR_VALUE_TYPE_DOUBLE` value 7.0.
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
- Fixes made in response: added an f64 three-arg reflection invoker bucket, exposed existing typed f64 three-arg predicates, and wired generated helper dispatch.
- File-size check: `backend_aot_c_reflection_invokers.c` is 948 lines, `backend_aot_c_reflection_numeric_three_arg_invokers.c` is 296 lines, `backend_aot_c_reflection_bool_numeric_invokers.c` is 240 lines, and `backend_aot_c_method_metadata.c` is 646 lines after this slice.

## Acceptance Decision
- Accepted as a support sub-slice for `10-S2X / 10-S3AB`.
- This closes only the generated double(double,double,double) reflection bucket.
- Remaining work: bool three-arg buckets, four-arg and wider buckets, object/inline returns, numeric widening,
  instance receiver handling, public `MethodInfo` materialization, MethodSpec-specific generated function slots,
  cross-module token rewrite, trim diagnostics, and the full trim analyzer.
