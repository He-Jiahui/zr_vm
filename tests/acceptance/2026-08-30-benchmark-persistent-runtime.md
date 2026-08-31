---
title: Benchmark persistent runtime acceptance
date: 2026-08-30
status: complete
---

# Persistent Runtime Acceptance

Task 2 introduces a versioned persistent benchmark protocol while preserving
process mode as the default. One-shot correctness is executed first; only then
does `ZR_VM_PERF_SCOPE=steady` measure numeric/dispatch loops.

## Evidence

- RED: the pre-change WSL GCC harness rejected `--persistent` as an unknown
  option and allowed missing scope/reuse validation.
- GREEN: strict WSL GCC (`-std=c11 -Wall -Wextra -Wpedantic -Werror`) builds the
  runner, transport, report module, and fixture; the complete fault matrix ends
  with `-- Persistent performance protocol PASS`.
- The same strict compile and complete fault matrix pass with WSL Clang. On
  Windows, Visual Studio 17.14 / MSVC 14.44 passes `/std:c11 /W4 /WX` for the
  runner, transport, report module, and fixture, followed by the complete CMake
  fault matrix.
- The matrix covers malformed/overlong lines, READY mismatch/timeout, ordered
  DONE checksum/index failures, ERROR, request/STOP timeout, early exit,
  process-group cleanup, same PID, and session-only RSS. Exact response grammar
  fixtures also reject leading whitespace, tabs, repeated spaces, `+1`, and
  leading-zero indices such as `01`.
- Lua and QuickJS server implementations load scripts once and return fresh
  handler state per request. .NET uses strict line parsing and reports JIT reuse
  only when warmup is nonzero.

Focused WSL core runtime smokes used `warmup=1` and `iterations=2`:

| Runtime | Numeric checksum / PID | Dispatch checksum / PID | Exit |
| --- | --- | --- | ---: |
| Lua | 793446923 / 547 | 320214929 / 610 | 0 |
| QuickJS | 793446923 / 551 | 320214929 / 613 | 0 |
| .NET | 793446923 / 554 | 320214929 / 616 | 0 |

Every run kept one PID, used per-run RSS `null`, and reported a non-null session
peak. .NET reported `jit_state_reused=true`; Lua and QuickJS reported it false.
Language workload exceptions write canonical `ERROR <index> workload-exception`
before terminating nonzero.

## ZR binary

`zr_vm_zr_benchmark_server` is linked to the core/library/system public APIs.
The suite refreshes generated `.zro` files with the same-build CLI
`--compile`, then starts the server with a binary-only loader. The server loads
the generated entry once and executes it repeatedly; source recompilation is
not a fallback. Runtime banners are redirected away from protocol stdout.

The checked-in `.zro` artifacts are not cross-build evidence. The focused WSL
GCC smoke copied only each project and source tree, then used the same-build CLI
to compile both projects. Numeric and dispatch each reported
`compiled=2 skipped=0`. The directly linked strict-warning server and runner
then produced:

| Case | Expected checksum | Same PID | Session peak bytes | Exit |
| --- | ---: | ---: | ---: | ---: |
| `numeric_loops/core` | 793446923 | 564 | 6197248 | 0 |
| `dispatch_loops/core` | 320214929 | 566 | 6254592 | 0 |

Both reports use `measurement_scope=persistent_runtime`,
`prepare_scope=bytecode_compile_and_load_before_measurement`,
`runtime_reused=true`, `compiler_reused=false`, and per-sample
`peak_working_set_bytes=null`. The session PIDs are evidence from this run only,
not stable identifiers.

The focused WSL GCC suite then ran both core cases across ZR binary, Lua,
QuickJS, and .NET with `warmup=1` and `iterations=2`. All eight rows passed and
were written under `performance_steady`; no process-mode report was created or
overwritten. This suite run also caught and fixed the generated ZR scale module
using removed keywordless function syntax. The support contract now requires
the canonical `pub fn scale(): int` template.

## Toolchains

| Toolchain | Protocol fixture | Real runtime smoke | Full steady matrix |
| --- | --- | --- | --- |
| WSL GCC | PASS | PASS, Lua/QJS/.NET/ZR numeric + dispatch | PASS, focused 2-case/4-runtime suite |
| WSL Clang | PASS | not run | not run |
| MSVC | PASS | not run | not run |

Steady mode fails closed when the ZR server executable is absent and is
incompatible with profile/Callgrind mode. Unsupported runtime/case mappings are
explicit `SKIP`, never process fallbacks.

Task 2 is complete. The focused suite is a functional protocol and reporting
acceptance run, not a statistical performance gate: two samples do not satisfy
Task 3's calibration, sample-count, CV, and confidence-interval requirements.
