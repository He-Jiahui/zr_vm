# AOT 10-S2Y Method.Invoke Bool Three-Arg Unbox And Return Boxing

## Scope
- Generated AOT C reflection invoker bucket for `bool(bool, bool, bool)` calls.
- Plan coverage: `10-S2Y` argument marshaling, `10-S3AC` generated reflection invocation, and `11-S2D` MethodInfo/functionIndex binding consumption.
- Layers covered: AOT C backend reflection invoker emission, typed bool three-arg thunk predicate consumption, parser source contracts, Unix shared-library smoke, and reflection/metadata adjacent runtime tests.

## Baseline
- Before this slice, generated `Method.Invoke` had scalar no-arg, one-arg, two-arg, bool-return numeric comparison two-arg, and int64/uint64/f64 three-arg buckets, but no bool three-argument bucket.
- RED command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'`
- RED result: the source contract failed on `TEST_ASSERT_NOT_NULL(reflectionBoolThreeArgInvokersSourceText)` because the bool three-arg invoker source file did not exist yet.
- Existing baseline: MSVC shared-library smoke remains Unix-only ignored.

## Implementation
- `backend_aot_c_reflection_bool_three_arg_invokers.h/.c` now emits `zr_aot_try_invoke_bool_three_arg(...)`.
- The generated helper checks non-null method/signature/return type, bool return base type, `parameterCount == 3u`,
  non-null `parameterTypes`, bool parameter base type for all three declared parameters, non-null args,
  matching runtime bool value type for all three slots, and non-null `outReturn`.
- Matched functionIndex cases unbox all three `nativeBool` payloads, call
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`, and box through
  `ZrCore_Value_InitAsBool(...)`.
- `backend_aot_c_typed_bool_three_arg_thunks.c` now recognizes the current cleanup-reset short-circuit AND shape emitted
  for `left && middle && right`, so eligible bool three-arg typed helpers can be selected by the reflection writer.
- `backend_aot_c_reflection_invokers.c` remains the orchestration boundary and dispatches this helper after
  `bool(bool, bool)` and before bool-return numeric helpers.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the bool three-arg split source file, include wiring,
  helper name, bool three-arg predicate, case writer, third-parameter and third-argument guards, third native bool unbox,
  typed bool call shape, return boxing, and dispatcher path.
- `tests/parser/test_aot_c_shared_library_smoke.c` adds
  `all_truth(left: bool, middle: bool, right: bool): bool`; checks generated `case 19u`; verifies method token
  `0x03000013u`; invokes the generated reflection invoker with all true; and verifies boxed `ZR_VALUE_TYPE_BOOL` true.
- Existing focused runtime tests remain adjacent coverage for public Method.Invoke shape/arity/base-type/return-slot
  behavior and MethodInfo binding consumers.

## Tooling Evidence
- Focused GREEN:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test'`
  passed with frame setup contracts 1/0 and shared-library smoke 13/0.
- WSL gcc focused matrix passed: frame setup contracts 1/0, source contracts 22/0, shared-library smoke 13/0,
  reflection method invoke 5/0, reflection token resolve 7/0, metadata runtime method binding 2/0, metadata runtime query 24/0.
- WSL clang focused matrix used the same target and binary list under `build-wsl-clang` and passed with the same counts.
- Windows MSVC Debug focused matrix used `build-msvc` with the same targets and Debug binaries, passed with the same counts,
  and shared-library smoke reported 13 ignored Unix-only cases.
- CTest adjacency passed 7/7 on WSL gcc, WSL clang, and Windows MSVC Debug for:
  `metadata_runtime_query|metadata_runtime_method_binding|reflection_token_resolve|reflection_method_invoke|aot_runtime_typed_direct_call_compatibility|aot_c_metadata_binding_loader|aot_c_method_info_signature`.

## Results
- Passed checks: all focused direct binaries and the three-platform CTest adjacency matrix listed above.
- Failed checks: the intended RED source-contract failure before implementation; no post-implementation failures remained in the focused matrix.
- Fixes made in response: added a bool three-arg reflection invoker bucket, wired generated helper dispatch, and extended bool three-arg typed thunk recognition for the cleanup-reset short-circuit AND shape.
- File-size check: `backend_aot_c_reflection_invokers.c` is 953 lines, `backend_aot_c_reflection_bool_three_arg_invokers.c` is 85 lines, `backend_aot_c_typed_bool_three_arg_thunks.c` is 335 lines, `backend_aot_c_reflection_numeric_three_arg_invokers.c` is 296 lines, `backend_aot_c_reflection_bool_numeric_invokers.c` is 240 lines, and `backend_aot_c_method_metadata.c` is 646 lines after this slice.

## Acceptance Decision
- Accepted as a support sub-slice for `10-S2Y / 10-S3AC`.
- This closes only the generated bool(bool,bool,bool) reflection bucket.
- Remaining work: four-arg and wider buckets, object/inline returns, numeric widening, instance receiver handling,
  public `MethodInfo` materialization, MethodSpec-specific generated function slots, cross-module token rewrite,
  trim diagnostics, and the full trim analyzer.
