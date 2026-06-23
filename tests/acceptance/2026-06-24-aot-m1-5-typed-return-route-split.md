# AOT M1.5 07-S5 Typed-Return Route Split

Timestamp: 2026-06-24 01:30:26 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move scalar typed-return route selection for i64/bool/u64/f64 out of `backend_aot_c_function_body.c`.
- Preserve existing native-to-VM return boundary behavior and generated C writer calls.
- Affected layers: AOT C function-body dispatch, typed-return route module, return source-shape contracts, and generated shared-library smoke coverage.

## Baseline

- `FUNCTION_RETURN` in `backend_aot_c_function_body.c` directly checked scalar local return predicates for i64/bool/u64/f64 and directly wrote the matching return boundary.
- The function-body file was still oversized and owned both opcode dispatch and typed-return route selection.
- Generic/value-SemIR return fallback still needs to remain in the function body for unsupported or non-scalar boundary paths.

## Test Inventory

- RED focused contract: `zr_vm_aot_c_return_contracts_test`.
- GREEN focused contract: `zr_vm_aot_c_return_contracts_test`.
- Relevant AOT regression executable chain: source contracts, call contracts, typed-call contracts, constant contracts, global contracts, logical contracts, frame setup contracts, return contracts, value SemIR contracts, shared-library smoke, call smoke, typed direct-call smoke, bool typed direct-call smoke, and u64 typed direct-call smoke.
- Parser shared-library build check: `zr_vm_parser_shared`.

## Tooling Evidence

- Toolchain: WSL GCC / CMake Debug build under `build-wsl-gcc`.
- RED command:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_return_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test`
- RED result: return contract failed 1/1 with `Expected Non-NULL` because `backend_aot_c_typed_return.{h,c}` did not exist.
- Focused GREEN command:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_return_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test`
- Regression command: direct chained execution of the relevant AOT binaries under `build-wsl-gcc/bin`, split into contract and smoke groups after one broad all-in-one command hit the 304s tool timeout while a long smoke was still running.
- Parser shared build command:
  `cmake --build build-wsl-gcc --target zr_vm_parser_shared`

## Results

- Focused return contracts: 1/0.
- Relevant AOT executable chain: source 19/0, call 4/0, typed call 4/0, constant 5/0, global 7/0, logical 4/0, frame setup 1/0, return 1/0, value SemIR 4/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, bool 28/0, u64 25/0.
- `zr_vm_parser_shared` built successfully.
- Object evidence confirmed `backend_aot_c_typed_return.c.o` compiled into `zr_vm_parser_shared`.
- The timed-out all-in-one command is not used as failure evidence; the split validation commands above passed after the timeout.
- Scoped `git diff --check` over touched implementation/test files exited 0 with only LF/CRLF warnings.

## Size Notes

- `backend_aot_c_function_body.c`: 2082 physical / 2034 non-empty lines.
- `backend_aot_c_typed_return.c`: 50 / 42.
- `backend_aot_c_typed_return.h`: 14 / 10.
- `tests/parser/test_aot_c_return_contracts.c`: 370 / 344.

## Acceptance Decision

- Accepted for this support sub-slice.
- This split changes typed-return route ownership only and preserves existing generated C behavior.
- Remaining open work: 07-S5 full typed ABI, inline structs, `in`/`out` writeback, deopt/dynamic bridge templates, dynamic value access hardening, full 07-S5 acceptance, and stages 08-12.
