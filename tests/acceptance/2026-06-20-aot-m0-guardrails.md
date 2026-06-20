# AOT M0 Guardrails And Golden Scalar Scaffold

## Scope

- Execute M0 from `docs/plans/aot/06-implementation-blueprint.md`: baseline and guardrails only.
- Added an AOT C guardrail contract for forbidden VM fallback tokens and allowed runtime boundary calls.
- Added a scalar golden smoke that compiles the same typed scalar source through the interpreter and generated AOT C shared-library path, then compares the returned integer result.
- Registered focused CTest entries for the new guardrail and golden smoke checks.
- No production behavior change was made in this slice.

## Baseline Snapshot

- Existing generated shared-library smoke output still shows half-lowering:
  - `build/codex-wsl-gcc-debug/tests_generated/aot_c_shared_library/src/aot_c_shared_library_smoke.c`
  - `ZrCore_Stack_GetValue(`: 10
  - `ZR_VALUE_FAST_SET(`: 3
  - `ZrLibrary_AotRuntime_Add(state, &frame`: 0
  - `ZrLibrary_AotRuntime_Return`: 0
- Existing numeric arithmetic project output still shows storage fallback:
  - `build/codex-wsl-gcc-debug/tests_generated/aot_c_shared_library/numeric_arithmetic_project/bin/aot_c/src/main.c`
  - `ZrCore_Stack_GetValue(`: 144
  - `ZR_VALUE_FAST_SET(`: 13
  - `ZrLibrary_AotRuntime_Add(state, &frame`: 0
  - `ZrLibrary_AotRuntime_Return`: 0
- New M0 scalar golden output gives the first interpreter-vs-AOT comparison sample and confirms the current scalar path still has storage fallback to remove in M2:
  - `build/codex-wsl-gcc-debug/tests_generated/aot_c_golden_scalar/project/bin/aot_c/src/main.c`
  - `ZrCore_Stack_GetValue(`: 10
  - `ZR_VALUE_FAST_SET(`: 4
  - `ZrLibrary_AotRuntime_Add(state, &frame`: 0
  - `ZrLibrary_AotRuntime_Return`: 0

## Evidence

- Timestamp: 2026-06-20 01:36:50 +08:00.
- Build directory: `build/codex-wsl-gcc-debug`.
- TDD RED guardrail token detector:
  - `cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_guardrail_contracts_test -j 8`
  - Result before implementation: link failed with undefined `aot_c_guardrail_find_forbidden_token`.
- TDD GREEN guardrail token detector:
  - `cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_guardrail_contracts_test -j 8`
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_guardrail_contracts_test`
  - Result: `2 Tests 0 Failures 0 Ignored OK`.
- TDD RED runtime call classifier:
  - `cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_guardrail_contracts_test -j 8`
  - Result before implementation: link failed with undefined `aot_c_guardrail_runtime_call_allowed`.
- TDD GREEN runtime call classifier:
  - `cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_guardrail_contracts_test -j 8`
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_guardrail_contracts_test`
  - Result: `4 Tests 0 Failures 0 Ignored OK`.
- TDD RED scalar golden interpreter side:
  - `cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_golden_scalar_smoke_test -j 8`
  - Result before implementation: link failed with undefined `execute_interpreter_i64`.
- TDD GREEN scalar golden comparison:
  - `cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_golden_scalar_smoke_test -j 1`
  - `timeout 180 ./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_golden_scalar_smoke_test`
  - Result: `1 Tests 0 Failures 0 Ignored OK`.
- Focused source-contract regression:
  - `cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_guardrail_contracts_test zr_vm_aot_c_source_contracts_test -j 8`
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_guardrail_contracts_test`
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test`
  - Result: guardrail `4 Tests 0 Failures 0 Ignored OK`; source contracts `17 Tests 0 Failures 0 Ignored OK`.
- CTest registration check:
  - `ctest --test-dir build/codex-wsl-gcc-debug -R "aot_c_guardrail_contracts|aot_c_golden_scalar_smoke|aot_c_source_contracts" --output-on-failure`
  - Result: `100% tests passed, 0 tests failed out of 2`.

## Acceptance Decision

- M0 status: complete.
- The slice establishes the baseline snapshot and comparison/check scaffold required by the plan.
- The scalar golden and wider existing AOT outputs still contain VM stack/value fallback tokens, so M2 remains open and must not be reported as complete from this evidence.
