# AOT M1.5 07-S5 Bool Typed-Direct Route Split

Timestamp: 2026-06-24 00:50:03 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move bool typed-direct route proof ownership out of `backend_aot_c_typed_direct_calls.c`.
- Preserve generated C behavior and route order for bool no/one/two/three-arg typed direct calls and i64-parameter bool-result two-arg direct calls.
- Affected layers: AOT C backend typed-direct route proof, typed-call source-shape contracts, and bool shared-library smoke coverage.

## Baseline

- `backend_aot_c_typed_direct_calls.c` still carried the bool route proof functions after the u64 and f64 route proof modules had already been split.
- The bool proof block was a coherent support boundary: it validates bool destination/argument scalar locals, written-before evidence, callee thunk eligibility, and i64-argument bool-return calls.
- Top-level typed-direct dispatch still needs to own route ordering, result sync decisions, and writer calls.

## Test Inventory

- RED focused contract: `zr_vm_aot_c_typed_call_contracts_test`.
- GREEN focused contract: `zr_vm_aot_c_typed_call_contracts_test`.
- Focused execution smoke: `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`.
- Relevant AOT regression executable chain: source contracts, call contracts, typed-call contracts, constant contracts, global contracts, logical contracts, frame setup contracts, return contracts, value SemIR contracts, shared-library smoke, call smoke, typed direct-call smoke, bool typed direct-call smoke, and u64 typed direct-call smoke.

## Tooling Evidence

- Toolchain: WSL GCC / CMake Debug build under `build-wsl-gcc`.
- RED command:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
- RED result: bool typed-call contract failed 1/4 with `Expected Non-NULL` because `backend_aot_c_typed_direct_bool_calls.{h,c}` did not exist.
- Focused GREEN commands:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Regression command: direct chained execution of the relevant AOT binaries under `build-wsl-gcc/bin`.

## Results

- Focused typed-call contracts: 4/0.
- Focused bool typed direct-call shared-library smoke: 28/0.
- Relevant AOT executable chain: source 19/0, call 4/0, typed call 4/0, constant 5/0, global 7/0, logical 4/0, frame setup 1/0, return 1/0, value SemIR 4/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, bool 28/0, u64 25/0.
- The first focused GREEN build command timed out at 129 seconds in the tool. The same build/test command passed when rerun with a longer timeout.
- Bool smoke build output confirmed `backend_aot_c_typed_direct_bool_calls.c.o` compiled into `zr_vm_parser_shared`.
- Scoped `git diff --check` over touched files exited 0 with only LF/CRLF warnings.

## Size Notes

- `backend_aot_c_typed_direct_calls.c`: 610 physical / 560 non-empty lines.
- `backend_aot_c_typed_direct_bool_calls.c`: 189 / 165.
- `backend_aot_c_typed_direct_bool_calls.h`: 53 / 50.
- `tests/parser/test_aot_c_typed_call_bool_contracts.c`: 394 / 388.

## Acceptance Decision

- Accepted for this support sub-slice.
- This split changes route proof ownership only and preserves existing generated C behavior.
- Remaining open work: 07-S5 full typed ABI, inline structs, `in`/`out` writeback, deopt/dynamic bridge templates, general typed-return ABI, dynamic value access hardening, full 07-S5 acceptance, and stages 08-12.
