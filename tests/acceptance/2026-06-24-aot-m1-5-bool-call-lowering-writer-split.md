# AOT M1.5 07-S5 Bool Call Lowering Writer Split

Timestamp: 2026-06-24 00:32:08 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move bool typed direct-call lowering writer ownership out of `backend_aot_c_lowering_calls.c`.
- Preserve generated C behavior for bool no/one/two/three-arg direct calls and i64/u64/f64 parameter bool-result two-arg direct calls.
- Affected layers: AOT C backend call lowering, typed-call source-shape contracts, and shared-library smoke coverage.

## Baseline

- `backend_aot_c_lowering_calls.c` had grown to 933 physical / 885 non-empty lines before the preceding bool thunk split and still carried generic/dynamic call boundaries plus all i64/bool/u64/f64 direct-call lowering writers.
- The bool direct-call lowering writer block was a coherent support boundary and did not need to stay in the aggregate file.

## Test Inventory

- RED focused contract: `zr_vm_aot_c_typed_call_contracts_test`.
- GREEN focused contract: `zr_vm_aot_c_typed_call_contracts_test`.
- Focused execution smoke: `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`.
- Relevant AOT regression executable chain: source contracts, call contracts, typed-call contracts, constant contracts, global contracts, logical contracts, frame setup contracts, return contracts, value SemIR contracts, shared-library smoke, call smoke, typed direct-call smoke, bool typed direct-call smoke, and u64 typed direct-call smoke.
- Boundary cases covered by the bool smoke include no/one/two/three bool direct calls plus i64/u64/f64 two-argument comparisons returning bool.

## Tooling Evidence

- Toolchain: WSL GCC / CMake Debug build under `build-wsl-gcc` (`GNU 11.4.0` observed during configure output).
- RED command:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
- RED result: bool typed-call contract failed 1/4 with `Expected Non-NULL` because `backend_aot_c_lowering_typed_bool_calls.c` did not exist.
- Focused GREEN commands:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Regression command: direct chained execution of the relevant AOT binaries under `build-wsl-gcc/bin`.

## Results

- Focused typed-call contracts: 4/0.
- Focused bool typed direct-call shared-library smoke: 28/0.
- Relevant AOT executable chain: source 19/0, call 4/0, typed call 4/0, constant 5/0, global 7/0, logical 4/0, frame setup 1/0, return 1/0, value SemIR 4/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, bool 28/0, u64 25/0.
- Two earlier scripted summary wrappers printed passing per-test summaries but were not used as clean exit-code evidence because PowerShell/WSL CR and variable expansion interfered with the wrapper exit path. The final direct chained executable command exited 0.
- Touched-file whitespace checks passed; tracked `git diff --check` reported only existing LF/CRLF warnings.

## Size Notes

- `backend_aot_c_lowering_calls.c`: 664 physical / 630 non-empty lines.
- `backend_aot_c_lowering_typed_bool_calls.c`: 272 / 257.
- `tests/parser/test_aot_c_typed_call_bool_contracts.c`: 350 / 344.
- Growth watch remains on `backend_aot_c_typed_u64_thunks.c`: 832 / 748.

## Acceptance Decision

- Accepted for this support sub-slice.
- This split changes ownership only and preserves existing generated C behavior.
- Remaining open work: 07-S5 full typed ABI, inline structs, `in`/`out` writeback, deopt/dynamic bridge templates, general typed-return ABI, dynamic value access hardening, full 07-S5 acceptance, and stages 08-12.
