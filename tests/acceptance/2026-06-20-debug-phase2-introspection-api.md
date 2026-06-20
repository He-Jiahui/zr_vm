# Debug Phase 2 Introspection API

## Scope

- Phase: `docs/plans/debug/02-introspection-api.md`.
- Goal: expose Lua-style local/upvalue introspection in `zr_vm_core` and reduce DAP snapshot drift by reusing core debug APIs where practical.
- Covered layers: core debug API, DAP closure/upvalue snapshot resolution, focused debug tests, and runtime fixes found while validating the language debug gauntlet.

## Completed Items

- Added `EZrDebugNameWhat` and `SZrDebugInfo.nameWhat`; Phase 2 fills `UNKNOWN` and leaves richer call-site inference for a later pass.
- Added `ZrCore_Debug_GetLocal`, `ZrCore_Debug_SetLocal`, `ZrCore_Debug_GetUpvalue`, `ZrCore_Debug_SetUpvalue`, and `ZrCore_Debug_GetUpvalueId`.
- Implemented local enumeration by active PC range and 1-based Lua-style indexes.
- Added inline frame value-slot handling for debug local reads, including materializing inline struct slots for read and rejecting inline struct writes.
- Added upvalue read/write/id APIs over runtime closure cells.
- Switched DAP closure/upvalue scope lookup and identifier resolution to the core upvalue APIs.
- Added `tests/debug/test_debug_introspection.c` and CTest `debug_introspection`.
- Fixed byte-frame value-slot overlap regressions found by the language debug gauntlet:
  - open closure values now dereference stack/byte-frame addresses as `SZrTypeValue *`;
  - closure-close barriers skip null/non-GC closed values;
  - raw-object stack writes prepare destinations with guarded ownership release instead of unconditional release;
  - existing call/return/frame copy paths were hardened for overlapping dense stack and inline value slots.

## Validation Evidence

- Windows/MSVC Debug build:
  - Command: `cmake --build build\codex-debug-phase1-msvc --config Debug --target zr_vm_cli_executable zr_vm_debug_agent_test zr_vm_debug_introspection_test zr_vm_debug_variable_child_shape_test zr_vm_debug_expression_diagnostics_test zr_vm_closure_capture_runtime_test --parallel 8`
  - Result: build passed with existing warnings.
- Direct source project module:
  - Command: `zr_vm_cli --project tests\fixtures\projects\language_debug_gauntlet\language_debug_gauntlet.zrp -m fixed_data --execution-mode interp`
  - Result: output `null`, exit 0.
- Direct source project entry:
  - Command: `zr_vm_cli tests\fixtures\projects\language_debug_gauntlet\language_debug_gauntlet.zrp --execution-mode interp`
  - Result: output `GAUNTLET_OK checksum=13910`, exit 0.
- Closure capture regression:
  - Command: `zr_vm_closure_capture_runtime_test`
  - Result: 1 test, 0 failures.
- Phase 2 debug regression set:
  - Command: `ctest --test-dir build\codex-debug-phase1-msvc -C Debug -R "^(debug_introspection|debug_variable_child_shape|debug_agent|debug_expression_diagnostics)$" --output-on-failure`
  - Result: 4/4 tests passed.

## WSL Status

- WSL gcc existing directory:
  - `debug_expression_diagnostics` passed.
  - `debug_variable_child_shape` passed.
  - `debug_introspection` initially failed because `zr_vm_debug_introspection_test` was not present.
  - Follow-up on 2026-06-20 10:52:13 +08:00: `ninja -C build/codex-debug-phase1-wsl-gcc -j2 zr_vm_debug_introspection_test` produced the executable, and `ctest --test-dir build/codex-debug-phase1-wsl-gcc -R "^(debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure` passed 3/3.
- WSL clang existing directory:
  - `debug_expression_diagnostics` passed.
  - `debug_introspection` initially failed because `zr_vm_debug_introspection_test` was not present.
  - Follow-up on 2026-06-20 10:52:13 +08:00: `ninja -C build/codex-debug-phase1-wsl-clang -j2 zr_vm_debug_introspection_test zr_vm_debug_variable_child_shape_test` produced the missing executables, and `ctest --test-dir build/codex-debug-phase1-wsl-clang -R "^(debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure` passed 3/3.
- The earlier timeout was caused by stale WSL build directories needing large dependency rebuilds while an unrelated long-running WSL build under `build/codex-wsl-gcc-debug` occupied the same machine. After that external build ended, both WSL focused gates completed.

## Acceptance Decision

- Accepted for Phase 2.
- Core implementation, Windows/MSVC focused acceptance, WSL gcc focused acceptance, and WSL clang focused acceptance are complete.
- Remaining scope intentionally deferred by the plan: richer `nameWhat` call-site classification beyond `UNKNOWN` and raw-stack negative-index views.
