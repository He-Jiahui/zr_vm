# Debug Phase 3 Traceback And Errors

## Scope

- Phase: `docs/plans/debug/03-traceback-and-errors.md`.
- Goal: add a reusable traceback text generator and attach traceback text to normalized runtime errors before unwind.
- Covered layers in this slice: core debug API, exception object normalization, unhandled exception text formatting, focused debug tests, and CMake/CTest registration.

## Completed Items

- Added `ZrCore_Debug_Traceback(state, prefixMessage, level, maxFrames, buffer, bufferSize)`.
- Kept traceback implementation in `zr_vm_core/src/zr_vm_core/debug_traceback.c` instead of adding more responsibility to the already large `debug.c`.
- Implemented script-frame rendering as `  at <function> (<source>:<line>)`.
- Implemented native-frame rendering path as `  at <function> [native]` when `Debug_GetInfo` reports a native frame.
- Added focused coverage for native/script/native mixed frames.
- Implemented Lua-style middle-frame folding with `... (skipping N levels)`.
- Implemented truncation-safe append helpers that always leave a NUL terminator when `bufferSize > 0`.
- Added exception `stack` text field during normalization while preserving the existing structured `stacks` array.
- Updated unhandled exception formatting to prefer the unified `stack` text and fall back to structured `stacks`.
- Added focused coverage that `ZrCore_Exception_PrintUnhandled` emits the unified stack text rather than the legacy `ip=` rendering when `stack` is present.
- Added `tests/debug/test_debug_traceback.c` and registered the `debug_traceback` target/test.
- Added CLI e2e coverage for unhandled error output that includes user-facing traceback frames.
- Added debug agent protocol coverage for exception stopped events carrying `exceptionStack`.
- Updated the VS Code DAP adapter to advertise and answer `exceptionInfo`, preserving the runtime exception stack and mirroring it to stderr output events.

## Validation Evidence

- Red check before implementation:
  - Command: `gcc -std=c99 -Werror=implicit-function-declaration -fsyntax-only tests/debug/test_debug_traceback.c ...`
  - Result: failed on missing `ZrCore_Debug_Traceback`, as expected for the new API contract.
- Syntax checks after implementation:
  - Command: `gcc -std=gnu11 -fsyntax-only tests/debug/test_debug_traceback.c ...`
  - Result: passed.
  - Command: `gcc -std=gnu11 -fsyntax-only zr_vm_core/src/zr_vm_core/debug_traceback.c ...`
  - Result: passed.
- Focused behavior check:
  - Command: manual WSL/GCC link of `tests/debug/test_debug_traceback.c`, `debug_traceback.c`, and `exception.c` against the existing Phase 2 WSL gcc parser/core libraries, followed by `build/codex-debug-traceback-manual/zr_vm_debug_traceback_test`.
  - Result: 4 tests, 0 failures.
- Formal WSL/GCC focused build and CTest:
  - Scope: `zr_vm_debug_traceback_test`, `zr_vm_cli_executable`, `zr_vm_cli_debug_e2e_test`, `zr_vm_debug_agent_test`.
  - Command: `ctest --test-dir build/codex-debug-phase3-wsl-gcc -R "^(debug_traceback|cli_debug_e2e|debug_agent|debug_trace|debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure`.
  - Result: 7 tests, 0 failures.
- CLI traceback smoke:
  - Command: `zr_vm_cli -e` with nested `leaf`/`middle`/`root` calls and a thrown payload.
  - Result: output included `Error:`, `payload:`, and `leaf`/`middle`/`root` traceback frames.
- Extension/DAP checks:
  - Command: `npm --prefix zr_vm_language_server_extension run compile`.
  - Result: passed.
  - Command: `npm --prefix zr_vm_language_server_extension run test:unit`.
  - Result: 29 tests, 0 failures.

## Current Limits

- Phase 3 functional DoD is complete.
- Milestone A `tests/` matrix and extension desktop smoke are complete as of 2026-06-21 22:43:43 +08:00, satisfying `docs/plans/debug/07-testing-and-acceptance.md` §7.5 for the Phase 4 entry gate.
- The first Milestone A attempt exposed non-Phase-3 infrastructure/build issues, now resolved for Windows/MSVC:
  - `build-msvc` initially failed before CTest on unresolved internal helper symbols; that build blocker was fixed by exporting the internal helpers used by existing tests.
  - `build-wsl-gcc` and `build-wsl-clang` root build directories time out in CMake dependency scanning before focused P3 tests can run there; stale residual processes from those attempts were cleaned up.
- Windows/MSVC Milestone A full matrix is green as of 2026-06-21: full Debug build passed and `ctest --test-dir build-msvc -C Debug --output-on-failure --timeout 480` finished 66/66 PASS.
- WSL/GCC Milestone A matrix is green as of 2026-06-21: focused gate 7/7 PASS and full `ctest --test-dir build-wsl-gcc --output-on-failure --timeout 600` finished 66/66 PASS.
- WSL/Clang Milestone A matrix is green as of 2026-06-21: focused gate 7/7 PASS and full `ctest --test-dir build-wsl-clang --output-on-failure --timeout 600` finished 66/66 PASS.
- VS Code extension validation is green as of 2026-06-21: `compile` PASS, `test:unit` 29/29 PASS, and `test:e2e:desktop:debug` returned exit 0.
- CLI unhandled-error exit status was not changed by this phase; this slice only standardizes the diagnostic text.

## 2026-06-21 Milestone A Retest

- Fixed the debug-hook traceback regression by keeping hook stack reservation within VM frame storage, refreshing the interpreter's cached frame base after hooks/relocation, and recomputing frame-layout generic call return destinations after call-window preparation.
- Fixed the Windows full-CTest `core_runtime` regression by keeping generic `ZrCore_Function_PostCall` on the historical stack-top contract while leaving previous-frame-storage preservation in the hot single-result/frame-layout fast paths.
- Evidence: Windows/MSVC focused `debug_traceback|debug_agent` 2/2 PASS; Windows/MSVC prior-failure + debug subset 10/10 PASS; Windows/MSVC full CTest 66/66 PASS; WSL/GCC focused gate 7/7 PASS and full CTest 66/66 PASS; WSL/Clang focused gate 7/7 PASS and full CTest 66/66 PASS; extension compile PASS, unit 29/29 PASS, desktop debug smoke exit 0.
- Remaining gate: none for Milestone A. Phase 4 was not started in this acceptance slice and should begin from `docs/plans/debug/04-script-debug-library.md`.

## Acceptance Decision

- Accepted for Phase 3 functional DoD: API, script/native stack rendering, folding, truncation, exception `stack` capture, CLI output, and DAP exception detail are implemented and validated.
- Milestone A closeout accepted: the full cross-build test matrix and extension desktop debug smoke are complete. Phase 4 has not started in this slice; it is now unblocked under the strict §7.5 gate.
