---
doc_type: acceptance-record
phase: 6.1
title: deterministic profiling and CLI profile switch
status: accepted
updated_at: 2026-06-22 04:46:47 +08:00
---

# Debug Phase 6.1 Profiling

## Scope

- Implement deterministic call/return profiling on top of the public debug hook API.
- Implement COUNT-hook sampling profiling that records current function and line hot spots.
- Add CLI `--profile[=out]` for normal project runs.
- Keep profiling disabled by default and reject incompatible debug launch combinations.

Coverage is accepted separately in `tests/acceptance/2026-06-22-debug-phase6-coverage.md`.
Bytecode disassembly is accepted separately in `tests/acceptance/2026-06-22-debug-phase6-disassembly.md`.
Heap summary is accepted separately in `tests/acceptance/2026-06-22-debug-phase6-heap-summary.md`.

## Baseline RED

| Check | Initial result |
|------|----------------|
| `tests/profile/test_profile_deterministic.c` | Failed to build before `zr_vm_lib_debug/profile.h` existed |
| CLI args profile parse test | Failed because `SZrCliCommand` did not expose `profileEnabled` / `profileOutputPath` |
| CLI import fixture profile report | Failed because runtime did not start profiler or write the requested output file |
| sampling profile test | Failed to compile before `ZrDebugProfileSample`, `ZrDebug_Profile_StartWithSampling`, and sample getters existed |

## Implemented

- Added `zr_vm_lib_debug/profile.h` and `zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c`.
- Deterministic profiler records function identity, source, call count, return count, total time, and self time.
- Sampling profiler records `(function, line)` buckets via COUNT hook and exposes sample getters.
- Profiler saves/restores the previous hook state and refuses ambiguous hook-owner combinations.
- Added `tests/profile/test_profile_deterministic.c`.
- Added CLI `--profile` and `--profile=<out>` parse support.
- Runtime writes `ZR_PROFILE deterministic` and `ZR_PROFILE samples` reports to stdout or the requested file after successful execution.
- CLI rejects `--profile` with `--debug` and non-run modes.

## Validation

| Build | Command / range | Result |
|------|------------------|--------|
| WSL/GCC Debug | `profile_deterministic|cli_args|cli_import_basic_fixture` | 3/3 PASS |
| WSL/Clang Debug | `profile_deterministic|cli_args|cli_import_basic_fixture` | 3/3 PASS |
| Windows/MSVC Debug | `profile_deterministic|cli_args|cli_import_basic_fixture` | 3/3 PASS |

## Decision

Accepted for Phase 6.1 profiling. Later Phase 6 tooling is tracked in separate acceptance records.
