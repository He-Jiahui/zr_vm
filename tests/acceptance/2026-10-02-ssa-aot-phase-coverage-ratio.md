---
related_code:
  - tests/performance/perf_report.c
  - tests/performance/perf_report.h
  - tests/benchmarks/aot_runner/aot_coverage.c
implementation_files:
  - tests/performance/perf_report.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/04-aot-runner-coverage.md
tests:
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
  - tests/cmake/ssa-tests.cmake
doc_type: acceptance-record
status: scoped-accepted
---

# SSA 07.04 AOT phase native coverage ratio consistency

## Scope and root cause

The phase report validator previously checked that `nativeCoverage` was finite
and within `[0, 1]`, and separately validated semantic counter sums. It accepted
native coverage `1.0` for eight native, one helper and one interpreter site out
of ten executed sites. The JSON writer therefore accepted a contradictory
100% report. The real coverage producer computes `0.8` for these counters.

Available phase coverage now requires exactly the producer's expression:
`(double)nativeSites / (double)executedSemanticSites`. Helpers do not count as
native execution, and static `semanticSites` does not supply the dynamic
denominator. No epsilon or new profile ABI is introduced. This C report API
accepts the computed double; it is not a reader for the writer's nine-decimal
JSON representation. Existing finite/range, counter sum, overflow and
unavailable `-1`/zero-counter rules remain in place.

The writer validates before opening the destination. Rejecting an inconsistent
ratio therefore also protects an existing report from truncation.

## Regression inventory and RED

The fixture first obtains each report from `ZrTests_AotCoverage_Compute`.
It checks valid `0.0`, `1.0` and `1.0 / 3.0`, including the all-native
`UINT64_MAX` endpoint, plus unavailable coverage. With eight native sites and
ten executed sites but 100 static sites, it accepts `0.8` and rejects `1.0`,
helper-inclusive `0.9` and static-denominator `0.08`. It rejects the rounded
`0.333333333` replacement for the computed one-third double.

The rejected-write regression writes a sentinel file, passes an inconsistent
ratio to `ZrPerfReport_WriteAotJson`, checks rejection, and verifies the exact
original bytes and file length before removing its own file. Existing runner,
overflow, merge, fallback, identity, checksum and JSON checks still execute.
The fixture remains the registered `ssa_aot_runner_coverage` source; no CMake
registration or previously accepted sampling record was changed.

Before the production change, MSVC compiled the complete five-source fixture
with exit 0; the fixture exited 1 with:

```text
FAIL: phase report accepted a ratio inconsistent with native counters
```

The RED source snapshots and hashes are retained in `red-snapshot-cl`.
GCC subsequently built and ran its identical saved pre-fix source snapshot,
also with build exit 0 and the same regression failure / fixture exit 1.

## Commands and artifact ownership

All binaries, objects, source/header snapshots, logs, receipts, compiler
temporary files and fixture JSON files use
`D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-phase-ratio`, or the identical WSL
path `/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-phase-ratio`.
The D-only `validate_ratio.py` driver changes cwd to `<phase>-<compiler>`, sets
`TMP`, `TEMP` and `TMPDIR` to that directory's `tmp`, snapshots the source and
include headers with SHA-256 receipts, and compiles those exact snapshots.
Every `ownership.json`, `build.log` and `run.log` records the actual argv, cwd,
owned PID and exit code. No full-tree configure or matrix is involved.

The executed Linux launcher, with the literal phase/compiler combinations
listed in the results below, is:

```powershell
wsl.exe -d Ubuntu-22.04 -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin TMP=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-phase-ratio TEMP=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-phase-ratio TMPDIR=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-phase-ratio /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-phase-ratio/validate_ratio.py green gcc
```

Windows launchers, from that D-only cwd with all three temporary variables set
to the same task root, are:

```powershell
python D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-phase-ratio\validate_ratio.py red cl
python D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-phase-ratio\validate_ratio.py red-snapshot cl
python D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-phase-ratio\validate_ratio.py green cl
```

The initial native launcher attempted the existing `bootstrap-msvc.cmd`; the
successful native runs reused root's captured
`control/msvc-toolchain-env.json` from the documented VsDevCmd environment.
No environment values are emitted into the logs.

The complete compiled source list is:

```text
tests/benchmarks/test_ssa_aot_runner_coverage.c
tests/benchmarks/aot_runner/aot_coverage.c
tests/benchmarks/aot_runner/aot_runner.c
tests/performance/perf_report.c
tests/performance/perf_statistics.c
```

Include directories are `tests/benchmarks/aot_runner`, `tests/performance` and
`zr_vm_common/include` under each D-only snapshot tree. Linux uses
`-std=c11 -Wall -Wextra -Werror`, the three absolute `-I` directories, the
five absolute source paths, `-lm`, and `-o <phase-directory>/coverage-test`.
The `san gcc` row adds `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer`, with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1` for execution. MSVC uses
`/nologo /std:c11 /utf-8 /W4 /MDd /D_CRT_SECURE_NO_WARNINGS`, the three
absolute `/I` directories and five sources, `/Fo<phase-directory>/` and
`/Fe<phase-directory>/coverage-test.exe`.

## Verification results

Toolchains are Ubuntu-22.04 GCC 11.4.0, Clang 14.0.0, and MSVC x64
19.44.35228.0 (VsDevCmd 17.14.40), reused from this session's toolchain probes.
MSVC RED and GREEN retain three preexisting C4244 warnings at fixture lines
456, 460 and 463. No new assertion produces that warning; `/WX` is not claimed.

| Phase / compiler | Build exit | Fixture exit | Evidence |
| --- | --- | --- | --- |
| `red gcc` | 124 | Not run | Direct-source compiler timeout |
| `red cl` | Not run | Not run | VsDevCmd initialization timeout; launcher 1 |
| `red-snapshot cl` | 0 | 1 | New ratio regression fails before fix |
| `red-snapshot gcc` | 0 | 1 | Same failure from saved pre-fix sources |
| `green gcc` | 0 | 0 | Complete fixture PASS |
| `green clang` | 0 | 0 | Complete fixture PASS |
| `green cl` | 0 | 0 | Complete fixture PASS; existing three C4244 warnings |
| `san gcc` | 0 | 0 | Complete fixture PASS; no ASan/UBSan diagnostic |

The first direct E-source `red gcc` build exceeded its
600-second compiler limit (`build.log` exit 124; launcher exit 1) and did not
run a fixture. The initial `red cl` launch timed out after 60 seconds in
VsDevCmd initialization, before compilation. Neither is counted as RED.
The successful snapshot MSVC and GCC RED runs above supply the regression evidence.

The owned GCC compiler children were observed waiting in `p9_client_rpc`
while accessing D-only temporary assembly files. Their state/argv diagnostics
are retained in `owned-build-state.json` and `owned-child-state.json`.
All snapshot GREEN and sanitizer builds completed within the existing
600-second limit; no further retry or foreign-process stop was needed.

`git -C E:\Git\zr_vm diff --check` for the changed tracked files exited 0,
with only LF/CRLF informational warnings. The new acceptance file was checked
for trailing whitespace separately. `green-source-match.json` confirms that
the compiled source and header hashes match the final repository files. These
are direct builds of the complete registered fixture source list.

The root agent inspected the exact four-file change, and an independent
read-only review found no actionable findings. The review checked the real
producer expression, zero-denominator and overflow preconditions, and validation
before `fopen`. The root rebuilt `zr_vm_ssa_aot_runner_coverage_test` in the
reusable MSVC matrix, exit 0, then ran:

```text
ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R ^ssa_aot_runner_coverage$ --output-on-failure --no-tests=error -j 1
```

The registered test passed 1/1 in 2.98 seconds, exit 0. Exact build and test
commands/results are in `control/aot-phase-root-build.log` and
`control/aot-phase-root-ctest.log`. The root also independently reran the frozen
GCC ASan/UBSan executable with leak detection and halt-on-UB: exit 0 in
13.101 seconds, full fixture PASS, without sanitizer diagnostics. Its launcher
and result are in `control/aot-phase-root-gcc-sanitizer.log`. All root logs and
compiler temporary files remain below the same owned D-drive root.

This bounded test-harness correction does not establish generated-artifact
coverage, the representative workload 90% gate, or completion of SSA 07.04.
It introduces no allocator, resource owner or asynchronous operation.
