# Debug Phase 1 Core Hooks

## Scope

- Phase: `docs/plans/debug/01-core-hook-fixes.md`.
- Goal: make core debug hook wiring usable before adding broader introspection APIs.
- Covered layers: `zr_vm_core` debug hook/getinfo APIs, focused debug tests, existing debug agent regressions, and a semantic-summary write fact regression found during validation.

## Baseline

- RED was established by adding `tests/debug/test_debug_hook_core.c` before implementation. The focused target failed to build because `SZrDebugActivation`, `ZrCore_Debug_GetStack`, `ZrCore_Debug_GetInfo`, `ZrCore_Debug_SetHook`, and the hook getter APIs did not exist.
- The checkout already had unrelated dirty import/project/runtime work. Initial full `cli_debug_e2e` validation exposed an import baseline failure: `tests/fixtures/projects/import_basic/import_basic.zrp` raised `import signature mismatch` even when run directly without debug. That blocker is resolved in this acceptance slice before marking Phase 1 complete.

## Completed Items

- Added `SZrDebugActivation` as the core stack-frame activation handle.
- Added `ZrCore_Debug_GetStack` and `ZrCore_Debug_GetInfo`.
- Kept `ZrCore_DebugInfo_Get` as a level 0 compatibility wrapper while honoring the requested info mask.
- Added `ZrCore_Debug_SetHook`, `ZrCore_Debug_GetHook`, `ZrCore_Debug_GetHookMask`, and `ZrCore_Debug_GetHookCount`.
- Wired `ZR_DEBUG_HOOK_MASK_COUNT` into `ZrCore_Debug_TraceExecution`.
- Propagated LINE/COUNT instruction traps to current VM call frames when hook state changes.
- Kept CALL/RETURN-only hooks on the call/return path instead of forcing per-instruction tracing.
- Added focused test coverage in `tests/debug/test_debug_hook_core.c`.
- Registered CTest `debug_hook_core`.
- Adjusted `tests/debug/test_debug_trace.c` to request closure info explicitly now that `DebugInfo_Get` honors the type mask.
- Fixed Debug semantic-summary write/member-write fact display in `zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c`.
- Added `ZrParser_SemanticFacts_FindReferenceAtPositionByKind` and used it only for Debug identifier read fallback, so computed index reads remain visible without hiding assignment write/member-write facts.
- Improved the CLI debug e2e failure message so unexpected stop reasons report observed reason/source/line.
- Added `tests/cli/test_cli_import_basic_fixture.c` and CTest `cli_import_basic_fixture`.
- Fixed callable exported-variable module summaries so `pub var greet = () => ...` refreshes from prescan `VARIABLE`/`FIELD_SIG` to final `FUNCTION`/`METHOD_SIG` typed export identity.
- Added a shared `ZR_CONSTANT_REFERENCE_PATH_DECLARED` guard around the duplicate `SZrConstantReferencePath` typedef declarations required by current include combinations.

## Modularization Note

- `zr_vm_core/src/zr_vm_core/debug.c` is now in the warning range for file size, but this Phase 1 slice is still one core debug-hook responsibility. The next split point should be `debug_hook.c` and/or `debug_introspection.c` if Phase 2 grows the file further.
- `zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c` is already a very large existing responsibility. This slice made a narrow summary-refresh fix there; the next related import/metadata pass should split module-init export summary matching/refresh from unrelated module-init analysis before adding more behavior.

## Test Inventory

- `tests/debug/test_debug_hook_core.c`
  - COUNT hook fires once per traced instruction with `count=1`.
  - `SetHook(NULL,0,0)` clears hook, mask, and count and stops callbacks.
  - LINE and COUNT events both arrive when both masks are enabled.
  - LINE events remain deduplicated by function and source line.
  - `GetStack`/`GetInfo` resolve nested frames and respect type masks.
- `tests/debug/test_debug_trace.c`
- `tests/debug/test_debug_expression_diagnostics.c`
- `tests/debug/test_debug_metadata.c`
- `tests/debug/test_debug_agent.c`
- `tests/debug/test_debug_agent_protocol.c`
- `tests/debug/test_debug_variable_child_shape.c`
- `tests/cli/test_cli_debug_e2e.c`
  - Initially exposed the `import_basic` import baseline blocker.
  - Re-run after the import fix; the `import_basic` launch breakpoint path now passes.
- `tests/cli/test_cli_import_basic_fixture.c`
  - Copies the `import_basic` fixture to a generated test directory.
  - Compiles the project and expects 2 compiled, 0 skipped, 0 removed.
  - Runs the binary path and expects `hello from import`.

## Tooling Evidence

- WSL gcc configure:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-debug-phase1-wsl-gcc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++'`
  - Result: configured.
- WSL gcc RED:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-debug-phase1-wsl-gcc zr_vm_debug_hook_core_test -j2'`
  - Result before implementation: failed to build because the new Phase 1 public APIs and activation type were missing.
- WSL gcc focused GREEN:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_debug_hook_core_test'`
  - Result: 3 tests, 0 failures.
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_debug_trace_test'`
  - Result: PASS.
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_debug_expression_diagnostics_test'`
  - Result: PASS after the semantic-summary write/member-write fix.
- WSL gcc debug regression:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-debug-phase1-wsl-gcc -R "debug_(metadata|hook_core|trace|agent|agent_protocol|expression_diagnostics|variable_child_shape)$" --output-on-failure --parallel 2'`
  - Result: 7/7 tests passed.
- WSL clang focused GREEN:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-debug-phase1-wsl-clang -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && ninja -C build/codex-debug-phase1-wsl-clang zr_vm_debug_hook_core_test zr_vm_debug_trace_test zr_vm_debug_expression_diagnostics_test -j2 && ctest --test-dir build/codex-debug-phase1-wsl-clang -R "debug_(hook_core|trace|expression_diagnostics)$" --output-on-failure --parallel 2'`
  - Result: 3/3 tests passed. The first parallel run timed out during the initial build; the resumed focused run completed successfully.
- Windows/MSVC focused GREEN:
  - Command: `. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-debug-phase1-msvc; cmake --build build\codex-debug-phase1-msvc --config Debug --target zr_vm_debug_hook_core_test zr_vm_debug_trace_test zr_vm_debug_expression_diagnostics_test --parallel 2; ctest --test-dir build\codex-debug-phase1-msvc -C Debug -R "debug_(hook_core|trace|expression_diagnostics)$" --output-on-failure --parallel 2`
  - Result: imported MSVC `19.44.35228.0`, built the focused targets, and 3/3 tests passed.
- CLI debug e2e blocker:
  - Command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-debug-phase1-wsl-gcc -R "^cli_debug_e2e$" --output-on-failure'`
  - Result: `debug_wait_hits_import_basic_launch_breakpoint` stopped with reason `exception` at `tests/fixtures/projects/import_basic/src/main.zr:1` instead of `breakpoint`.
  - Direct baseline command: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_cli tests/fixtures/projects/import_basic/import_basic.zrp'`
  - Direct baseline result: `import signature mismatch: module 'greet' member 'greet' expectedHash=0x65a13beb317df2ed actualHash=0x65a13beb317df2ed`, showing the failure is present outside debug launch.
  - Recompile probe: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_cli --compile tests/fixtures/projects/import_basic/import_basic.zrp --run; echo __STATUS__$?'`
  - Recompile result: `compiled=2` but the same `import signature mismatch` remains.
  - Import focused baseline: `wsl bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-debug-phase1-wsl-gcc zr_vm_project_import_canonicalization_test -j2 && ./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_project_import_canonicalization_test'`
  - Import focused result: 35 tests, 0 failures. This narrows the blocker to the `import_basic` exported-callable fixture path rather than the debug hook/API slice.
- Import blocker root cause and fix:
  - Debugging `module_import_signature_blob_matches` showed `expectedLength=7`, `actualLength=16`, same target hash/token, expected blob starting as `FIELD_SIG`, and provider blob starting as `METHOD_SIG`.
  - Root cause: source module-init summary kept the prescan variable export identity for `pub var greet = () => ...`, while final typed metadata correctly emitted a callable function export.
  - Fix: module-init export matching now allows that variable-to-function refresh for the same exported name and synchronizes export kind, symbol kind, metadata token, signature token/hash, and signature blob range from the final typed symbol.
- WSL gcc final Phase 1 gate:
  - Command: `ctest --test-dir build/codex-debug-phase1-wsl-gcc -R "^(cli_import_basic_fixture|cli_debug_e2e|debug_metadata|debug_hook_core|debug_trace|debug_agent|debug_agent_protocol|debug_expression_diagnostics|debug_variable_child_shape)$" --output-on-failure --parallel 2`
  - Result: 9/9 tests passed.
  - Command: `./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_cli --compile tests/fixtures/projects/import_basic/import_basic.zrp --run`
  - Result: `compile summary: compiled=2 skipped=0 removed=0` and `hello from import`.
  - Command: `./build/codex-debug-phase1-wsl-gcc/bin/zr_vm_project_import_canonicalization_test`
  - Result: 35 tests, 0 failures.
- WSL clang focused final gate:
  - Command: `ninja -C build/codex-debug-phase1-wsl-clang zr_vm_cli_import_basic_fixture_test zr_vm_debug_expression_diagnostics_test -j4 && ctest --test-dir build/codex-debug-phase1-wsl-clang -R "^(cli_import_basic_fixture|debug_expression_diagnostics)$" --output-on-failure --parallel 2`
  - Result: 2/2 tests passed.
- Windows/MSVC focused final gate:
  - Command: `cmake --build build\codex-debug-phase1-msvc --config Debug --target zr_vm_cli_import_basic_fixture_test zr_vm_debug_expression_diagnostics_test`
  - Result: build passed.
  - Command: `ctest --test-dir build\codex-debug-phase1-msvc -C Debug -R "^(cli_import_basic_fixture|debug_expression_diagnostics)$" --output-on-failure`
  - Result: 2/2 tests passed.

## Results

- Phase 1 core hook/API behavior is accepted on WSL gcc, WSL clang, and Windows/MSVC focused debug coverage.
- Existing debug agent/metadata/expression/variable-child-shape regressions pass on the focused WSL gcc debug set.
- The `import_basic` source-project baseline and `cli_debug_e2e` launch-breakpoint path now pass after fixing callable export summary refresh.

## Acceptance Decision

- Accepted for full Phase 1 DoD: COUNT hook, SetHook/getters, GetStack/GetInfo, DebugInfo type-mask compatibility, Debug semantic-summary regression coverage, `import_basic` exported-callable import baseline, and `cli_debug_e2e` launch breakpoint coverage.
