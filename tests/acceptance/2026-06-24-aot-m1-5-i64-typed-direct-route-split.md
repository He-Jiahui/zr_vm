# AOT M1.5 07-S5 I64 Typed-Direct Route Split

Timestamp: 2026-06-24 01:03:26 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move i64 typed-direct route proof ownership out of `backend_aot_c_typed_direct_calls.c`.
- Preserve generated C behavior and route order for i64 no/one/two/three-arg typed direct calls.
- Affected layers: AOT C backend typed-direct route proof, typed-call source-shape contracts, and i64 shared-library smoke coverage.

## Baseline

- `backend_aot_c_typed_direct_calls.c` still carried the i64 route proof functions after bool, u64, and f64 route proof modules had been split.
- The i64 proof block validated destination/argument scalar locals, written-before evidence, callee thunk eligibility, and callee table lookup.
- Top-level typed-direct dispatch still needs to own route ordering, result sync decisions, and writer calls.

## Test Inventory

- RED focused contract: `zr_vm_aot_c_typed_call_contracts_test`.
- GREEN focused contract: `zr_vm_aot_c_typed_call_contracts_test`.
- Focused execution smoke: `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`.
- Relevant AOT regression executable chain: source contracts, call contracts, typed-call contracts, constant contracts, global contracts, logical contracts, frame setup contracts, return contracts, value SemIR contracts, shared-library smoke, call smoke, typed direct-call smoke, bool typed direct-call smoke, and u64 typed direct-call smoke.

## Tooling Evidence

- Toolchain: WSL GCC / CMake Debug build under `build-wsl-gcc`.
- RED command:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
- RED result: i64 typed-call contract failed 1/4 with `Expected Non-NULL` because `backend_aot_c_typed_direct_i64_calls.{h,c}` did not exist.
- Focused GREEN commands:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`
- Regression command: direct chained execution of the relevant AOT binaries under `build-wsl-gcc/bin`.

## Results

- Focused typed-call contracts: 4/0.
- Focused i64 typed direct-call shared-library smoke: 5/0.
- Relevant AOT executable chain: source 19/0, call 4/0, typed call 4/0, constant 5/0, global 7/0, logical 4/0, frame setup 1/0, return 1/0, value SemIR 4/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, bool 28/0, u64 25/0.
- Focused typed-call GREEN printed CMake `GLOB mismatch` after the pass. This is the expected source glob regeneration notice for the new `.c` file and is not a test failure.
- I64 smoke build output confirmed `backend_aot_c_typed_direct_i64_calls.c.o` compiled into `zr_vm_parser_shared`.
- Scoped `git diff --check` over touched files exited 0 with only LF/CRLF warnings.

## Size Notes

- `backend_aot_c_typed_direct_calls.c`: 467 physical / 436 non-empty lines.
- `backend_aot_c_typed_direct_i64_calls.c`: 152 / 132.
- `backend_aot_c_typed_direct_i64_calls.h`: 43 / 40.
- `tests/parser/test_aot_c_typed_call_i64_contracts.c`: 302 / 297.

## Acceptance Decision

- Accepted for this support sub-slice.
- This split changes route proof ownership only and preserves existing generated C behavior.
- Remaining open work: 07-S5 full typed ABI, inline structs, `in`/`out` writeback, deopt/dynamic bridge templates, general typed-return ABI, dynamic value access hardening, full 07-S5 acceptance, and stages 08-12.
