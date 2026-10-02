---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_frame_layout.h
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
  - tests/core/test_ssa_roots_observation.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/core/test_ssa_roots_observation.c
doc_type: acceptance-record
status: scoped-accepted
---

# SSA 04.04 core inline root bounds

## Scope

This slice repairs the runtime-neutral core support API
`ZrCore_Execution_VisitFrameRoots`. It separates the containing span bounds
from the pointer-sized inline-field access bounds, preserving descriptor-sized
access bounds for managed and derived roots. The checked-add destination is
initialized so an overflowing field address has a defined diagnostic value.
The core fixture uses always-active `CHECK` assertions, including with
`NDEBUG` defined. No production ExecBC/AOT GC or codegen integration is claimed.
Full 04.04 acceptance, native pin lifetime and parser root-map construction
remain separate work.

## Baseline and RED

The original API rejected a valid inline field at the end of a frame. On a
64-bit target the fixture has one 16-byte inline span at byte offset zero and
a pointer field at byte offset eight. `Layout_Finalize`, `RootMap_Build` and
`RootMap_Validate` succeed, but the original visitor incorrectly requires the
whole 16-byte span to fit after byte offset eight.

The new positive fixture was added before changing production code. GCC
compiled all three source files successfully; `ssa_core_roots_observation`
then aborted at the expected `VisitFrameRoots` success assertion, with 0/1
tests passing. The WSL wrapper returned exit code 1. The old production source
is retained as `red-execution_frame_roots.c` in the task directory.

MSVC first exposed a temporary driver setup error: it lacked C11 and UTF-8
options. Adding the required driver options resolved compilation. The actual
MSVC RED run then stopped at the same success assertion. Its CRT assertion
process remained active and locked the executable, causing a subsequent
`LNK1168` build failure. The complete image path of PID 43192 was checked as
the task's `msvc/zr_vm_ssa_core_roots_observation_test.exe` before terminating
that owned process. The RED CTest records the assertion and a 440.06-second
failed run. Final MSVC verification uses the separate `msvc-final` directory.
No foreign process or build was stopped or cleaned.

## Test inventory

The executable runs six fixture groups: existing managed/derived relocation,
existing observation and invalidation, existing validation failures, inline
field boundaries, checked field-address overflow, and non-inline access-span
preservation. The new checks cover:

- A final inline field ending exactly at the frame boundary, with the callback
  receiving its exact address and writing back only that pointer.
- A one-byte-short frame, rejected without callback or frame mutation.
- An out-of-span field offset, rejected by map construction while preserving
  the previously built map.
- An uninitialized root skipped by the visitor, a callback preserving a
  stationary root, and callback writeback of a null root.
- Host byte storage with an unaligned base, accessed through `memcpy`;
  malformed descriptor slot alignment is still rejected.
- An empty map with a zero-length frame, and null request, map, frame and
  callback inputs rejected with the exact invalid-argument diagnostic.
- A detached map with an accepted containing span but overflowing
  `frameByteOffset + fieldByteOffset`, rejected before callback with
  `FRAME_BOUNDS`, index zero and actual offset zero.
- Managed and derived roots with a pointer-sized tail that fits but a
  descriptor-sized access span that does not; both must fail before callback.
  Existing valid nonzero offsets and a complete access span ending exactly at
  the frame end remain accepted.

The stationary callback is not a native pin lease test. OOM, cancellation,
threading and source-language safepoints do not occur in this visitor slice.
The existing managed-before-derived fixture still verifies moving-base
writeback followed by derived pointer recomputation. Its backing objects were
expanded from one byte to eight bytes after Release warned that the original
`base + 4` expression exceeded the allocation; the existing assertion now
uses a valid derived pointer range.

## Tooling and commands

All source files remain in `E:/Git/zr_vm`; every temporary driver, build,
binary, compiler temporary and log is under
`D:/tmp/zr_vm/ssa-20261002-01a0fc3b/inline-root-bounds` (`TASK` below).
WSL uses Ubuntu-22.04, GCC 11.4.0, Clang 14.0.0 and CMake 3.22.1.
MSVC uses 19.44.35228.0 with the x64 VsDevCmd environment.

The focused temporary CMake project compiles exactly
`tests/core/test_ssa_roots_observation.c`, `execution_frame_roots.c` and
`execution_frame_observation.c`, includes the core/common public headers,
requires C11, and registers the existing target and CTest names
`zr_vm_ssa_core_roots_observation_test` / `ssa_core_roots_observation`.
It does not configure the repository's full tree or modify its CMake files.
Its final flags are `-Wall -Wextra` or MSVC `/W4 /utf-8`, without undefining
`NDEBUG`. CTest timeout is 30 seconds.

Each Linux invocation uses the following prefix, with `TASK` expanded to the
absolute `/mnt/d/tmp/zr_vm/ssa-20261002-01a0fc3b/inline-root-bounds` path:

```text
wsl.exe -d Ubuntu-22.04 --cd / -e /usr/bin/env
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  TMPDIR=TASK/tmp TMP=TASK/tmp TEMP=TASK/tmp
  /bin/bash TASK/green-wsl.sh <build-directory> <compiler> [flags]
```

The recorded script executes the exact configure/build/test sequence:

```bash
cd "$TASK"
cmake -S . -B "$mode" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER="$compiler" -DZR_REPO=/mnt/e/Git/zr_vm "$@"
cmake --build "$mode"
ctest --test-dir "$mode" -R '^ssa_core_roots_observation$' --output-on-failure
```

Recorded arguments are `gcc gcc`, `clang clang`, `gcc-release gcc
-DCMAKE_BUILD_TYPE=Release`, and `sanitizer clang
-DCMAKE_C_FLAGS=-fsanitize=address,undefined
-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`. The sanitizer process
also sets `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`.

Windows runs `cmd.exe /d /c TASK\green-msvc.cmd`. That batch sets
`TMP=TEMP=TMPDIR=TASK\tmp`, calls
`E:\Visual Studio\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64`, then
executes:

```text
cmake -S . -B msvc-final -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl
cmake --build msvc-final
ctest --test-dir msvc-final -R ^ssa_core_roots_observation$ --output-on-failure --interactive-debug-mode 0
```

Logs are `<mode>-green-configure.log`, `<mode>-green-build.log`,
`<mode>-green-ctest.log` and `<mode>-green-status.log` in `TASK`. RED logs have
the corresponding `-red-` prefix. WSL compiler/linker delay was observed in
`p9_client_rpc`; logs preserve that environment behavior without treating it
as a pass.

## Results and decision

### Non-inline review regression

Review identified a behavior regression in the first bounds repair: narrowing
every root's tail access to one pointer accepted non-inline descriptors that
the old visitor rejected. The formal builder and validator accept a managed
pointer root with byte offset zero, byte size 16 and field offset 24 in a
32-byte frame. Its pointer fits, but its full access span does not. A new
always-active fixture repeats the boundary for managed and derived roots,
then preserves successful nonzero field offsets of eight and sixteen bytes.

Before changing the first repair, GCC ASan/UBSan and MSVC incremental builds
both succeeded. Their exact CTests failed at the new `!VisitFrameRoots`
check (line 433): the visitor returned success. The corresponding logs are
`review-red-gcc-sanitizer-{build,ctest,status}.log` and
`review-red-msvc-{build,ctest,status}.log`. The first repair source is retained
as `review-red-execution_frame_roots.c`.

The corrected visitor selects pointer-sized access only for `INLINE_FIELD`.
`MANAGED` and `DERIVED` preserve their descriptor-sized access bounds from the
computed field address; the independent complete-container check stays in
place. All four existing caches are rebuilt incrementally, without a new
compiler-identification run, using `TASK/review-check.sh <cache> <log-prefix>`
or `TASK/review-msvc.cmd <log-prefix>`. Linux prefixes are
`review-green-gcc-sanitizer`, `review-green-gcc-release` and
`review-green-clang`; the Windows prefix is `review-green-msvc`. These scripts
run only `cmake --build <cache>` and the same exact CTest, retaining the
previous configure and baseline logs.

The non-inline review fix passed all four incremental GREEN gates:

| Final review run | Build | CTest | Outcome |
| --- | --- | --- | --- |
| GCC ASan/UBSan/LSan | 0 | 0 | 1/1 passed, 5.73 seconds, no sanitizer report |
| GCC Release/NDEBUG | 0 | 0 | 1/1 passed, 1.07 seconds |
| Clang Debug | 0 | 0 | 1/1 passed, 1.06 seconds |
| MSVC Debug | 0 | 0 | 1/1 passed, 2.23 seconds |

Each log-prefix's `-status.log` records `build=0 ctest=0`. The final source
and tests were unchanged throughout these four runs. No extra compiler-ID
or static Clang sanitizer attempt was introduced. The four-file write set is
frozen for the root agent's independent formal-target verification and commit.

### Initial repair evidence and tool replacement

GCC Debug, Clang Debug and MSVC Debug focused CTests each passed 1/1, with
configure, build and CTest exit codes zero. These ordinary runs include the
core bounds repair and always-active checks. The subsequent edits normalized
the bounds-expression indentation and enlarged the old derived-pointer test
buffer. Final GCC Release/NDEBUG and GCC ASan/UBSan rebuilt those final source
files and each passed 1/1. Release's actual flags include `-O3 -DNDEBUG`,
without `-UNDEBUG`.

| Run | Configure | Build | CTest | Outcome |
| --- | --- | --- | --- | --- |
| GCC Debug | 0 | 0 | 0 | 1/1 passed |
| Clang Debug | 0 | 0 | 0 | 1/1 passed |
| MSVC Debug | 0 | 0 | 0 | 1/1 passed |
| GCC final Release/NDEBUG | 0 | 0 | 0 | 1/1 passed |
| GCC final ASan/UBSan/LSan | 0 | 0 | 0 | 1/1 passed, no sanitizer report |

Clang ASan/UBSan stalled at compiler identification, with its linker waiting
in `p9_client_rpc` for more than 833 seconds. The wrapper, CMake, compiler and
linker PID/start-tick/cwd/argv ownership was captured in
`sanitizer-owned-stop.json` before sending SIGTERM only to that tree. The
wrapper exited 1; `sanitizer-timeout.log` retains the infrastructure stall.
No pass is claimed for that run. GCC's already verified `gcc` CMake cache is
used for the replacement sanitizer gate, avoiding compiler identification.
Its flags are `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`
and linker flags `-fsanitize=address,undefined`, with the same ASan/UBSan
environment and exact CTest. The actual script is `TASK/gcc-sanitizer.sh`;
logs are `gcc-sanitizer-{configure,build,ctest,status}.log`.

Optimized GCC also reported pre-existing possibly uninitialized `baseIndex`
and `fieldEnd` paths in root-map construction. Those producer paths are
outside this visitor bounds slice and remain separate work; no warning is
silenced. The final Release log retains both warnings, with no remaining
derived-fixture array-bounds warning. GCC sanitizer configure/build/CTest
completed with zero exit codes and no AddressSanitizer, UndefinedBehaviorSanitizer
or LeakSanitizer report. Final Release rebuild/CTest also completed with zero
exit codes; its evidence is `gcc-release-final-{build,ctest,status}.log`.

This slice is accepted for the core support API and all six fixture groups,
including the managed/derived compatibility boundaries found during review.
The root's independent formal target verification is recorded below.
Production pipeline integration and full 04.04 remain open.

## Root independent final validation

After the non-inline review correction, the root rebuilt the existing formal
`zr_vm_ssa_core_roots_observation_test` target in its fresh MSVC Debug repository
tree. Build exited zero, and the registered test passed in 1.13 seconds:

```powershell
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/core-roots-async-stale-final-build.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc cmake --build D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc --target zr_vm_ssa_core_roots_observation_test zr_vm_ssa_async_frame_budget_test -j 4
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_native.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/core-roots-async-stale-final-ctest.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc ctest --test-dir D:/tmp/zr_vm/ssa-20261002-01a0fc3b/matrix/msvc -R "^(ssa_core_roots_observation|ssa_async_frame_budget)$" --output-on-failure --no-tests=error -j 1
python D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/run_linux_artifact.py D:/tmp/zr_vm/ssa-20261002-01a0fc3b/control/roots-final-root-gcc-sanitizer.log D:/tmp/zr_vm/ssa-20261002-01a0fc3b/inline-root-bounds/gcc/zr_vm_ssa_core_roots_observation_test
```

The two-test formal CTest run passed 2/2 with exit zero. The independent final
GCC ASan/UBSan/LSan run also exited zero, without sanitizer diagnostics.
The independent gpt-6-sol reviewer confirmed the non-inline finding was closed
and found no remaining actionable issue. Every generated root output and
temporary file stayed under the D-drive artifact root.
