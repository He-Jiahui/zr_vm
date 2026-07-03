# AOT 10-S2W Method.Invoke Uint64 Three-Arg Unbox And Return Boxing

## Scope
- Generated AOT C reflection invoker bucket for `uint64(uint64, uint64, uint64)` calls.
- Plan coverage: `10-S2W` argument marshaling, `10-S3AA` generated reflection invocation, and `11-S2D` MethodInfo/functionIndex binding consumption.
- Layers covered: AOT C backend reflection invoker emission, typed u64 three-arg thunk predicate consumption, parser source contracts, Unix shared-library smoke, and reflection/metadata adjacent runtime tests.

## Baseline
- Before this slice, generated `Method.Invoke` had scalar no-arg, one-arg, two-arg, bool-return numeric comparison two-arg, and int64 three-arg buckets, but no uint64 three-argument bucket.
- RED command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test'`
- RED result: the source contract failed on missing `#include "backend_aot_c_typed_u64_three_arg_thunks.h"`.
- Existing baseline: MSVC shared-library smoke remains Unix-only ignored.

## Implementation
- `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c` now emits `zr_aot_try_invoke_u64_three_arg(...)` alongside the existing i64 three-arg helper.
- The generated helper checks non-null method/signature/return type, uint64 return base type, `parameterCount == 3u`,
  non-null `parameterTypes`, uint64 parameter base type for all three declared parameters, non-null args,
  matching runtime uint64 value type for all three slots, and non-null `outReturn`.
- Matched state-free functionIndex cases unbox all three `nativeUInt64` payloads, call
  `zr_aot_typed_u64_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`, and box through
  `ZrCore_Value_InitAsUInt(...)`.
- Matched divide/modulo functionIndex cases keep the stateful helper call shape
  `zr_aot_typed_u64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)` so generated divide/modulo-by-zero
  diagnostics remain available.
- `backend_aot_c_reflection_invokers.c` remains the orchestration boundary and dispatches this helper after
  `uint64(uint64, uint64)` and before bool-return helpers.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c` asserts the numeric three-arg split source file, helper name,
  u64 three-arg predicates, case writer, third-parameter and third-argument guards, third native uint64 unbox,
  state-free/stateful typed u64 call shapes, and dispatcher path.
- `tests/parser/test_aot_c_shared_library_smoke.c` adds
  `sum_three_unsigned(left: uint, middle: uint, right: uint): uint`; checks generated `case 17u`; verifies method token
  `0x03000011u`; invokes the generated reflection invoker with 50, 20, and 5; and verifies boxed
  `ZR_VALUE_TYPE_UINT64` value 75.
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
- Fixes made in response: added a uint64 three-arg reflection invoker bucket and wired generated helper dispatch.
- File-size check: `backend_aot_c_reflection_invokers.c` is 944 lines, `backend_aot_c_reflection_numeric_three_arg_invokers.c` is 198 lines, `backend_aot_c_reflection_bool_numeric_invokers.c` is 240 lines, and `backend_aot_c_method_metadata.c` is 646 lines after this slice.

## Acceptance Decision
- Accepted as a support sub-slice for `10-S2W / 10-S3AA`.
- This closes only the generated uint64(uint64,uint64,uint64) reflection bucket.
- Remaining work: f64/bool three-arg buckets, four-arg and wider buckets, object/inline returns, numeric widening,
  instance receiver handling, public `MethodInfo` materialization, MethodSpec-specific generated function slots,
  cross-module token rewrite, trim diagnostics, and the full trim analyzer.
