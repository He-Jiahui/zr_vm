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
- Milestone A still needs the full `tests/` matrix across the three planned build directories plus extension desktop smoke before starting Phase 4, per `docs/plans/debug/07-testing-and-acceptance.md` §7.5.
- The first Milestone A attempt exposed non-Phase-3 infrastructure/build issues:
  - `build-msvc` initially failed before CTest on unresolved internal helper symbols; that build blocker was fixed by exporting the internal helpers used by existing tests.
  - `build-wsl-gcc` and `build-wsl-clang` root build directories time out in CMake dependency scanning before focused P3 tests can run there; stale residual processes from those attempts were cleaned up.
- Windows/MSVC now builds fully, but full CTest is still not green. The initial full run was 11/66 fail in non-debug suites (`core_runtime`, `language_pipeline`, `containers`, `language_server`, `language_server_stdio_inline_value_semantic_smoke`, `cli_repl_expression_assignment_context_smoke`, `projects`, `escape_pipeline`, `cli_repl_e2e`, `metadata_module_hash_golden`, `system_fs`).
- `core_runtime` was then investigated and fixed: interpreter meta access now matches the AOT hidden accessor contract for `META_GET`/`META_SET`. Focused WSL/GCC and Windows/MSVC `zr_vm_instructions_test` runs pass, and Windows/MSVC `core_runtime` passes. Full CTest has not yet been rerun after that focused fix.
- Windows/MSVC debug suites did pass in the full run, including `cli_debug_e2e`, `debug_traceback`, `debug_agent`, `debug_agent_protocol`, `debug_expression_diagnostics`, and `debug_variable_child_shape`.
- CLI unhandled-error exit status was not changed by this phase; this slice only standardizes the diagnostic text.

## Acceptance Decision

- Accepted for Phase 3 functional DoD: API, script/native stack rendering, folding, truncation, exception `stack` capture, CLI output, and DAP exception detail are implemented and validated.
- Milestone A closeout remains blocked on the full cross-build test matrix and extension desktop smoke; Phase 4 has not started under the strict §7.5 gate.
