# AOT M1.5 07-S5 Call Boundary Writer Split

Timestamp: 2026-06-24 01:47:59 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move generic, dynamic, and static resolved VM-call boundary writer ownership out of `backend_aot_c_lowering_calls.c`.
- Preserve existing generated C markers, runtime helper calls, and scalar-local result synchronization behavior.
- Affected layers: AOT C call boundary emitters, call source-shape contracts, generated parser shared build, and shared-library call smoke coverage.

## Baseline

- `backend_aot_c_lowering_calls.c` still owned `backend_aot_write_c_core_function_call()`, `backend_aot_write_c_direct_function_call()`, `backend_aot_write_c_dynamic_function_call()`, and `backend_aot_write_c_static_direct_function_call()`.
- That kept `ZrLibrary_AotRuntime_CallStackValue()`, `ZrLibrary_AotRuntime_CallStaticDirect()`, and post-call scalar-local sync helper calls physically mixed with typed direct-call scalar writer definitions.
- The 07-S5 plan requires boundary marshaling templates to be isolated so `SZrValue` and VM/native conversions are visible at boundary modules instead of buried in function-body or typed writer aggregates.

## Test Inventory

- RED focused contract: `zr_vm_aot_c_call_contracts_test`.
- GREEN focused contract: `zr_vm_aot_c_call_contracts_test`.
- Parser shared build check: `zr_vm_parser_shared`.
- Relevant AOT regression executable chain: source contracts, call contracts, typed-call contracts, constant contracts, global contracts, logical contracts, frame setup contracts, return contracts, value SemIR contracts, shared-library smoke, call smoke, typed direct-call smoke, bool typed direct-call smoke, and u64 typed direct-call smoke.

## Tooling Evidence

- Toolchain: WSL GCC / CMake Debug build under `build-wsl-gcc`.
- RED command:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test`
- RED result: call contracts failed 3/4 with `Expected Non-NULL` because `backend_aot_c_call_boundaries.c` did not exist. The meta-call boundary test remained passing.
- Focused GREEN command:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test`
- Parser shared build command:
  `cmake --build build-wsl-gcc --target zr_vm_parser_shared`
- Regression command: direct chained execution of the relevant AOT binaries under `build-wsl-gcc/bin`.

## Results

- Focused call contracts: 4/0.
- `zr_vm_parser_shared` built successfully.
- Object evidence confirmed `backend_aot_c_call_boundaries.c.o` compiled into `zr_vm_parser_shared`.
- Relevant AOT executable chain: source 19/0, call 4/0, typed call 4/0, constant 5/0, global 7/0, logical 4/0, frame setup 1/0, return 1/0, value SemIR 4/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, bool 28/0, u64 25/0.
- Focused call-contract GREEN printed CMake `GLOB mismatch` after the pass. The follow-up parser shared build regenerated and compiled the new source object.
- Scoped structure check confirmed `backend_aot_c_lowering_calls.c` no longer contains the moved core/direct/dynamic/static VM-call boundary writer definitions or their direct `CallStackValue` / `CallStaticDirect` / scalar sync helper strings.
- Scoped `git diff --check` over touched implementation/test files exited 0 with only LF/CRLF warnings.

## Size Notes

- `backend_aot_c_lowering_calls.c`: 481 physical / 455 non-empty lines.
- `backend_aot_c_call_boundaries.c`: 185 / 177.
- `tests/parser/test_aot_c_call_contracts.c`: 424 / 386.

## Acceptance Decision

- Accepted for this support sub-slice.
- This split changes call-boundary writer ownership only and preserves existing generated C behavior.
- Remaining open work: 07-S5 full typed ABI, inline structs, `in`/`out` writeback, deopt/dynamic bridge templates, dynamic value access hardening, full 07-S5 acceptance, and stages 08-12.
