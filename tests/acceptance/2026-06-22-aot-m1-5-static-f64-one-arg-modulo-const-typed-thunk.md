# AOT M1.5 Static F64 One-Arg Modulo-Const Typed Thunk

## Scope
- Changed 07-S5 typed-to-typed direct-call code generation for one-argument f64 callees returning `arg0 % K`.
- Affected layers: AOT C typed thunk recognition/emission, static typed direct-call routing, parser AOT contract tests, f64 shared-library smoke tests, the Unix f64 smoke compile support link line, and AOT plan/module documentation.

## Baseline
- Before this slice, f64 one-arg typed thunks covered identity, add-constant, subtract-constant, multiply-constant, and nonzero divide-constant returns.
- RED baseline: `zr_vm_aot_c_call_contracts_test` failed because `backend_aot_c_try_get_f64_arg0_modulo_constant_return(` was absent.
- Existing repository baseline remains outside this focused acceptance scope; this run used the WSL GCC AOT focused group rather than claiming full-repository green.

## Test Inventory
- Contract: `tests/parser/test_aot_c_call_contracts.c` now requires the modulo-constant recognizer, `MOD_FLOAT`, the zero-constant rejection check, and the generated `fmod(arg0, K)` return expression.
- Integration: `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c` executes `func remainder(value: float): float { return value % 50.0; }` through a generated shared library and expects result 42.
- Support: `tests/parser/aot_c_typed_direct_call_f64_smoke_support.h` links generated Unix shared-library smoke artifacts with `-lm` so generated `fmod` resolves consistently with other math-function smoke tests.
- Boundary cases: accepts only float/double callable return and one float/double parameter, no varargs, optional parameter copy prefix, argument on the left, constant on the right, and nonzero float constants.
- Negative cases: zero constants are rejected by the recognizer; runtime dynamic denominator failure behavior and broader f64 modulo routes remain out of scope for this direct thunk slice.

## Tooling Evidence
- Tool: WSL Ubuntu-22.04 GCC/Ninja debug build at `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test'`
- GREEN focused command: same command after implementation and support link-line fix; observed call contracts 8/0 and f64 typed direct-call smoke 10/0.
- Broader AOT command:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_type_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test'`

## Results
- RED: call contracts reported 8 tests, 1 failure, missing `backend_aot_c_try_get_f64_arg0_modulo_constant_return(`.
- First post-implementation focused run: call contracts passed, but the generated f64 smoke shared-library link failed on unresolved `fmod`; the support header was missing `-lm`.
- GREEN focused: call contracts 8 tests / 0 failures; f64 typed direct-call smoke 10 tests / 0 failures.
- Broader focused WSL GCC AOT group: source contracts 19/0, call contracts 8/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, bool 2/0, u64 5/0, f64 10/0, arithmetic 5/0, bitwise 6/0, typed scalar 1/0, value-type 1/0, generic numeric 1/0, global 9/0, logical 4/0, power 1/0, frame setup 1/0, return 1/0, value SemIR 4/0.
- Implementation keeps zero-denominator constants out of the direct thunk route until the AOT direct-call failure channel is explicit.

## Acceptance Decision
- Accepted for the narrow 07-S5 sub-slice: static one-arg f64 nonzero modulo-constant typed thunk direct-call.
- Not accepted as full 07-S5: f64 dynamic divide/modulo, wider f64 expression routes, inline structs, in/out writeback, deopt/dynamic bridges, and full boundary marshaling remain open.
