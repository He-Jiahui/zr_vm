---
doc_type: acceptance-record
phase: 6.2
title: line coverage and CLI coverage switch
status: accepted
updated_at: 2026-06-22 05:44:30 +08:00
---

# Debug Phase 6.2 Coverage

## Scope

- Expose executable active line enumeration from core debug metadata.
- Implement line coverage on top of the public LINE hook.
- Add CLI `--coverage[=out]` for normal project runs.
- Keep coverage disabled by default and reject incompatible debug/profile combinations.

Bytecode disassembly is accepted separately in `tests/acceptance/2026-06-22-debug-phase6-disassembly.md`.
Heap summary is accepted separately in `tests/acceptance/2026-06-22-debug-phase6-heap-summary.md`.

## Baseline RED

| Check | Initial result |
|------|----------------|
| `tests/profile/test_coverage.c` | Failed to build before `zr_vm_lib_debug/coverage.h` existed |
| core activelines test | Failed because `ZrCore_Debug_GetActiveLines` did not exist |
| CLI args coverage parse test | Failed because `SZrCliCommand` did not expose `coverageEnabled` / `coverageOutputPath` |
| CLI import fixture coverage report | Failed before runtime started coverage and wrote the requested output file |

## Implemented

- Added `ZrCore_Debug_GetActiveLines(function, outLines, cap)` backed by `SZrFunctionExecutionLocationInfo`.
- Added `zr_vm_lib_debug/coverage.h` and `zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c`.
- Coverage registers an entry function tree before execution, records executable line denominators, and marks LINE hook hits as executed.
- Coverage saves/restores the previous hook state and keeps one active coverage recorder per state.
- Added `tests/profile/test_coverage.c`.
- Added CLI `--coverage` and `--coverage=<out>` parse support.
- Runtime writes `ZR_COVERAGE lines` reports to stdout or the requested file after successful execution.
- CLI rejects `--coverage` with `--debug`, `--profile`, and non-run modes.

## Validation

| Build | Command / range | Result |
|------|------------------|--------|
| WSL/GCC Debug | `coverage|cli_args|cli_import_basic_fixture` | 3/3 PASS |
| WSL/Clang Debug | `coverage|cli_args|cli_import_basic_fixture` | 3/3 PASS |
| Windows/MSVC Debug | `coverage|cli_args|cli_import_basic_fixture` | 3/3 PASS |

## Decision

Accepted for Phase 6.2 coverage. Later Phase 6 tooling is tracked in separate acceptance records.
