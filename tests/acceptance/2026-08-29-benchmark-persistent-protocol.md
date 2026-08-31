---
title: Benchmark persistent protocol and steady-state acceptance
date: 2026-08-29
status: complete
---

# Benchmark Persistent Protocol Acceptance

This record covers Task 2 of the benchmark optimization plan. It keeps
one-shot correctness separate from steady measurement and records the evidence
required to compare process and persistent rows.

## RED evidence

Before implementation, the direct WSL GCC harness failed with:

```text
Unknown or incomplete option: --persistent
```

The same harness also rejected missing persistent scope and accepted a
persistent-only option in process mode; both validation gaps are now covered.

## GREEN evidence

Focused WSL commands use `gcc` and `clang` with
`-std=c11 -Wall -Wextra -Wpedantic -Werror` for `perf_runner.c`,
`persistent_protocol.c`, `perf_report.c`, and the protocol fixture, then run
`run_perf_runner_persistent_protocol_test.cmake`. Both pass:

```text
-- Persistent performance protocol PASS
```

The fixture covers READY timeout/mismatch/malformed responses, ordered DONE
checksum and index failures, ERROR, malformed and overlong lines, early exit
before/after READY, request timeout, STOP timeout/nonzero exit, process-group
cleanup, same-PID samples, and session-only peak RSS. Grammar-negative cases
include leading whitespace, tabs, repeated spaces, `+1`, and `01` indices.

Visual Studio 17.14 / MSVC 14.44 also passes `/std:c11 /W4 /WX` compilation
and the complete CMake fault matrix.

Focused WSL core runtime smokes use checksum contracts
`benchmark-checksum-v1:<case>:core`, `warmup=1`, and `iterations=2`. Lua,
QuickJS, and .NET each return `793446923` for numeric and `320214929` for
dispatch, retain one PID, report session RSS with per-run RSS `null`, and exit
zero. .NET reports `jit_state_reused=true`; the script runtimes report false.

## ZR binary boundary

`zr_vm_zr_benchmark_server` is a separate target linked to core/library/system
public APIs. The suite first invokes the CLI `--compile` step in the same build
tree, then starts the server with the generated project. The server installs a
binary-only source loader, loads the `main.zro` entry once, and executes the
root function for each warmup/measured request. It redirects runtime banners to
`/dev/null`/`NUL` so stdout remains protocol-only.

The checked-in `tests/benchmarks/**/zr/bin/*.zro` files are not valid evidence
for a different core build; fresh compilation is mandatory before a real ZR
smoke. A stale artifact may SIGBUS during binary deserialization and must not
be used to change the server design.

## Acceptance matrix

| Check | GCC | Clang | MSVC |
| --- | --- | --- | --- |
| protocol fixture, strict warnings | PASS | PASS | PASS |
| Lua/QuickJS persistent numeric + dispatch smoke | PASS | pending | pending |
| .NET persistent numeric + dispatch and JIT-state contract | PASS | pending | pending |
| ZR fresh compile then binary server smoke | PASS, numeric + dispatch | pending | pending |
| focused steady numeric/dispatch suite, ZR/Lua/QJS/.NET | PASS | pending | pending |

For steady runs, missing `zr_vm_zr_benchmark_server` fails closed rather than
silently falling back to process mode. Profile/Callgrind is a separate mode and
cannot be combined with `ZR_VM_PERF_SCOPE=steady`.

The WSL GCC focused suite uses `warmup=1` and `iterations=2`; all eight rows
pass and preserve their actual persistent commands, reuse flags, same-PID
evidence, and session RSS. Its reports are isolated under
`performance_steady`. These two-sample results prove Task 2 behavior only and
are not eligible for the Task 3 statistical gate.
