---
related_code:
  - tests/benchmarks/aot_runner/aot_coverage.c
  - tests/benchmarks/aot_runner/aot_coverage.h
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
implementation_files:
  - tests/benchmarks/aot_runner/aot_coverage.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/04-aot-runner-coverage.md
tests:
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
  - tests/cmake/ssa-tests.cmake
doc_type: acceptance-record
---

# SSA 07.04 sampling evidence through coverage merges

## Scope and root cause

`ZrTests_AotCoverage_Merge` previously treated every zero destination sampling
rate as an empty accumulator. A nonempty unknown-rate input followed by an
exact sample therefore acquired a full sampling rate and produced an available,
exact report. The same happened after a different-rate merge had deliberately
discarded its sampling claim. An empty source with a different known rate could
also discard a destination's valid rate.

The fix uses the existing effective semantic denominator to distinguish empty
inputs from unknown evidence. Only an empty destination adopts the source rate.
For a nonempty destination, an empty source leaves its rate unchanged. Two
nonempty inputs with different rates produce unknown evidence, including when
either rate is zero. Once unknown, subsequent nonempty exact samples cannot
restore knowledge of the earlier samples. No ABI field or persistent state was
added. Candidate validation, overflow rejection and commit-on-success remain
the existing mutation boundary.

## Baseline and test inventory

The new regression was written before the fix. Both GCC and MSVC built the
complete focused fixture successfully and its new first pair failed with:

```text
FAIL: sampling merge confused empty data with unknown evidence
exit_code=1
```

The pair matrix checks unknown+exact, exact+unknown, empty+known, empty with
another known rate+known, known+empty with zero/different rates,
unknown+empty known, and equal partial rates. It checks effective counters and
report availability/exactness, not only the stored rate. A chained
exact+partial+exact merge must remain unavailable. A declared nonzero
denominator without observed hits must not count as an empty destination.

The same fixture retains all preexisting overflow, failed mutation,
counter/denominator, native/helper/interpreter, compiled fallback, actual
backend, invocation failure, checksum and JSON-report checks. It is the
existing `zr_vm_ssa_aot_runner_coverage_test` target / `ssa_aot_runner_coverage`
CTest source; no CMake registration was changed.

## Validation environment and exact commands

All generated binaries, objects, logs, temporary compiler files and the JSON
fixture use `D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-sampling` (WSL:
`/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-sampling`). Windows processes start
in its `red` directory. The D-only `validate_sampling.py` driver changes cwd to
`<phase>-<compiler>`, sets `TMP`, `TEMP` and `TMPDIR` to that directory's `tmp`
child, and records exact subprocess argv, cwd and exit code in `build.log` and
`run.log`. It does not configure or build the full repository.

Toolchains: Ubuntu-22.04 GCC 11.4.0, Clang 14.0.0, MSVC x64 19.44.35228.0
(VsDevCmd 17.14.40). `gcc --version` and `clang --version` exited 0. MSVC
`cl /Bv` printed the version and exited 2 for its intentionally missing source
argument; this was a version probe, not a build.

The executed Linux launcher was the following, once for each Linux row below, with
the row's literal phase and compiler arguments:

```powershell
wsl.exe -d Ubuntu-22.04 -e /usr/bin/env PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin TMP=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-sampling/red TEMP=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-sampling/red TMPDIR=/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-sampling/red /usr/bin/python3 /mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/aot-sampling/validate_sampling.py red gcc
```

The Windows launchers ran with `TMP`, `TEMP` and `TMPDIR` set to the same D-only
`red` directory:

```powershell
python D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-sampling\validate_sampling.py red cl
python D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-sampling\validate_sampling.py green cl
python D:\tmp\zr_vm\ssa-20261002-01a0fc3b\aot-sampling\validate_sampling.py version cl
```

The driver imports the Visual Studio environment through the existing
`D:\tmp\zr_vm\ssa-control\bootstrap-msvc.cmd`, which calls the documented
`E:\Visual Studio\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64`.

The exact compiled source list is:

```text
tests/benchmarks/test_ssa_aot_runner_coverage.c
tests/benchmarks/aot_runner/aot_coverage.c
tests/benchmarks/aot_runner/aot_runner.c
tests/performance/perf_report.c
tests/performance/perf_statistics.c
```

All paths are prefixed with `/mnt/e/Git/zr_vm/` on Linux and `E:\Git\zr_vm\`
on Windows. Include directories are `tests/benchmarks/aot_runner`,
`tests/performance` and `zr_vm_common/include`, with the same absolute prefix.
Linux builds use `-std=c11 -Wall -Wextra -Werror`, those `-I` directories,
the five sources, `-lm`, and `-o <phase-directory>/coverage-test`. GREEN adds
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`.
MSVC uses `/nologo /std:c11 /utf-8 /W4 /MDd /D_CRT_SECURE_NO_WARNINGS`, the
three `/I` directories and five sources, `/Fo<phase-directory>/` and
`/Fe<phase-directory>/coverage-test.exe`.

## Results and encountered failures

| Phase / compiler | Build exit | Fixture exit | Evidence |
| --- | --- | --- | --- |
| `red gcc` | 0 | 1 | New sampling regression fails before fix |
| `red cl` | 0 | 1 | Same new regression fails before fix |
| `green cl` | 0 | 0 | `ssa aot runner coverage PASS` |
| `green gcc` | Timeout | Not run | First 120-second compiler limit exceeded |
| `green clang` | Timeout | Not run | First 120-second compiler limit exceeded |
| `green-retry gcc` | 0 | 0 | Full fixture PASS, no ASan/UBSan diagnostic |
| `green-retry clang` | Timeout | Not run | Static sanitizer linker exceeded 600 seconds |
| `green-shared clang` | 0 | 0 | Full fixture PASS, no ASan/UBSan diagnostic; shared ASan runtime |

The Clang retry's owned linker PID 53607 was observed in `/proc` as
`State: D (disk sleep)` with `wchan=p9_client_rpc` after 457 seconds. This
locates the wait in WSL's cross-drive file I/O. After the compiler's timeout,
the recorded linker PID was checked for a possible owned-process stop and had
already exited; no signal was sent. The next Clang attempt keeps
ASan/UBSan enabled and adds `-shared-libasan`, with
`LD_LIBRARY_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/lib/linux`, to reduce static
runtime output. All outputs still stay in D-only directories.

The first default-distro WSL launcher, started from the repository cwd, was
stopped as an owned tool session before producing a binary or test log. The
explicit distro/D-cwd driver subsequently obtained the GCC RED evidence. The
initial native Python invocation could not resolve `cl` using the parent PATH;
the driver now resolves its absolute executable from the imported Visual
Studio environment.

An initial MSVC `/W4 /WX` build exited 2 on the fixture's three preexisting
`int` to `TZrBool` conversions in perf-report assertions. `/W4` RED and GREEN
builds retain and display those C4244 warnings, rather than changing unrelated
assertions or disabling the warning. No new sampling assertion produces that
warning. Full-tree build or test status is outside this focused evidence.

## Acceptance decision

Accepted as a bounded sampling-merge correction: GCC and Clang sanitizer
builds and the MSVC compatibility build ran the complete focused fixture and
passed. The static Clang sanitizer link remains an observed environmental
timeout; its shared-runtime build preserves both instrumentation families and
executes the same cases successfully. `git diff --check` passed for the changed
tracked files, with only the repository's LF/CRLF informational warnings.
The existing CTest target was not reconfigured or invoked; these are direct
builds of its complete source list.

Root independently rebuilt and ran the five-source MSVC fixture (`root-check
cl`, build/run exit 0), then ran both resulting Linux sanitizer binaries with
`ASAN_OPTIONS=detect_leaks=1` and `UBSAN_OPTIONS=halt_on_error=1`. Both returned
0 and printed `ssa aot runner coverage PASS`, with no sanitizer diagnostic.
The exact root launch argv and exit codes are retained under the same D-only
task root in `control/aot-root-gcc.log`, `control/aot-root-clang.log`, and
`aot-sampling/root-check-cl/{build,run}.log`.

This bounded change addresses sampling
metadata in the AOT coverage test harness; it does not establish generated
artifact coverage, the representative workload 90% target, or completion of
SSA 07.04. Allocation/OOM/cancellation behavior is unchanged and no allocator,
resource owner or asynchronous operation is introduced.
