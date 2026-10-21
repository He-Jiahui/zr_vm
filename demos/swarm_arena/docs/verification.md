# Verification evidence

## GD-C3 rebuilt GD-C2 baseline (2026-10-20)

Before adding GD-C3 code, the fresh instance rebuilt all three demo trees and
recreated the engine Release, detector and forced-GC trees. System CMake 3.31.6,
Ninja, Clang/LLVM 19.1.7 and system libffi replace the lost local tool directories.
The baseline has no behavior or test-count differences: demo **67/67** in Release
(7.43 s), ASan/UBSan (16.89 s), and forced full GC + ASan/UBSan (16.71 s).
Each retains 38/38 required mode checks, 58/62 successful checks overall, P22
18/18 required pairs, the eight native C tests, and both native plugin smokes.
There are no detector findings. The four optional P19/v2-package failures remain
expected failures, with unchanged feature status.

The unchanged engine Release full core suite passes **515/515** (63.63 s), with
`ZR_VM_TEST_TIER=core`, excluding `performance_report`. The other engine trees
build the CLI; their full core suites are not rerun for demo-only work.
Every demo tree retains the identical 515-test core inventory.

Configure uses Ninja, `CMAKE_C_COMPILER=/usr/bin/clang-19`, tests and performance
CTest registration ON, language server/extension and Rust binding OFF, and
`ZR_VM_LIBFFI_INCLUDE_DIR=/usr/include/x86_64-linux-gnu`,
`ZR_VM_LIBFFI_LIBRARY=/usr/lib/x86_64-linux-gnu/libffi.so`.
Release uses `CMAKE_BUILD_TYPE=Release`; detector trees use Debug with
`CMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'`, adding
`-DZR_DEBUG_GARBAGE_COLLECT_MEM_TEST` for forced GC. Demo trees additionally
set `ZR_VM_BUILD_DEMOS=ON`. No lost tool directory is placed on PATH.

Raw configure/build/test logs and inventories are under `/tmp/zr-swarm-gd-c3-*`;
`/tmp/zr-swarm-gd-c3-rebuild-results.jsonl` records exact commands and timings.
CTest Temporary and probe-results are symlinked there. The native smoke runner
now respects `TMPDIR`; setting it to `/tmp/zr-swarm-gd-c3-runtime` also keeps its
private logs under this task's evidence root. Detector runs retain
`ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`.

## GD-C2 native runtime and null backend (2026-10-20)

Implementation commits: `b2466ea3` (shared runtime and native C tests) and
`05f9fd17` (host/gfx descriptor modules, plugins and native smokes), followed by
`0e3c7965` (prepare compact SoA backing before the bulk read loops). No engine
source changes were needed. A fresh engine fetch confirmed `origin/main`
`291be0cb` was already an ancestor of engine HEAD `ed10137f`; no new merge or
engine push was required.

All three existing demo trees built `zr_vm_cli_executable`, `zrgame_plugins`,
`zrgame_ffi` and `zrgame_native_tests` with zero warnings in the new native
sources. Both plugins have a dynamic dependency on the single
`libzrgame_runtime.so` in the native output directory. `zrgame_modules` also
provides the descriptors as a static archive for later embedding.

| Gate | Release | ASan/UBSan | Forced full GC + ASan/UBSan |
| --- | --- | --- | --- |
| Native C unit tests | 8/8 | 8/8 | 8/8 |
| Native plugin smokes, interp + binary | 2/2 | 2/2 | 2/2 |
| All demo CTests | 67/67 | 67/67 | 67/67 |
| GD-C1 required mode checks | 38/38 | 38/38 | 38/38 |
| GD-C1 successful mode checks, all cases | 58/62 | 58/62 | 58/62 |
| P22 required parity pairs | 18/18 | 18/18 | 18/18 |
| Engine CTest final outcomes | 515/515 | 515/515 | 129/129 selected |
| Null content golden | 299488900 | 299488900 | 299488900 |
| Detector findings | — | 0 | 0 |

The full demo runs took 16.70 s, 31.24 s and 31.08 s respectively, while
engine checks ran concurrently. The final native-only reruns cover exact
messages for six caught errors and preparation of compact array backing before
bulk reads. They pass 3/3 CTests in each configuration (0.14 s, 3.54 s, 3.58 s).
The C runner contains eight tests;
the other two CTests exercise interpreter and source-removed binary execution.

Both smokes import `zrgame.host` and require positive, nondecreasing
`nowMicros()`. They exercise every host/gfx API, the native STRUCT constructor
and `spriteInst` inline view (mode B included), scalar/inline/SoA count and hash
agreement, float32 rounding, nonzero rotation, explicit full GC, atomic invalid
SoA rejection, native error recovery, nested zones, and exact sink bytes.
The null snapshot leaves no image file. The C tests cover capacities and
overflow, copied text/hash, exact nested/recursive zone durations, zone stack
errors, sink capacity/stale handles, clock, RSS and drive state.

No GD-C1 expectations changed: P19 and P14's package case retain their four
expected failing mode checks, and the required gate remains green. These
native smokes are separate from C1's probe inventory. `probes/status.json`
adds a `gd_c2` result with current CLI/runtime/plugin hashes and source-removal
evidence without changing the C1 probe statuses.

Builds and tests use the unchanged settings:

```sh
export PATH="/workspace/zr_vm-perf/build/local-tools/usr/bin:/workspace/zr_vm-perf/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="/workspace/zr_vm-perf/build/local-tools/usr/lib/x86_64-linux-gnu"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core
for configuration in release asan gcstress; do
  cmake --build "build/demo-${configuration}" -j3 --target \
    zr_vm_cli_executable zrgame_plugins zrgame_ffi zrgame_native_tests
  ctest --test-dir "build/demo-${configuration}" -L demo -j4
  ctest --test-dir "build/demo-${configuration}" -L native -j3
done
```

The detector tree retains `-fsanitize=address,undefined -fno-omit-frame-pointer`;
the GC tree additionally retains `-DZR_DEBUG_GARBAGE_COLLECT_MEM_TEST`.
Leak detection remains disabled, so these results do not claim leak-clean
shutdown. Raw logs are only under `/tmp/zr-swarm-gd-c2-*`, including
`{release,asan,gcstress}-{build-final3,demo-final,native-final3}.log` and the
smoke runner's private temporary directories. CTest Temporary and GD-C1
probe-results retain their existing `/tmp` symlinks.

Engine regression checks use the unchanged engine worktree and its existing
builds. Release and ASan/UBSan each have successful final outcomes for 515/515
tests; the existing forced-GC selection has successful final outcomes for
129/129 tests. These exclude `performance_report` and use `ZR_VM_TEST_TIER=core`.
The initial engine invocations omitted the documented LLVM tool `PATH`;
`aot_c_metadata_binding_loader` and `language_pipeline` required targeted
reruns with the correct environment. No code or expectations were changed.
The full Release run passed 513/515 in 62.94 s and its corrected rerun passed
2/2 in 17.79 s. The full engine detector run passed 513/515 in 916.89 s and its
corrected rerun passed 2/2 in 597.77 s. The forced-GC selection passed 128/129
in 23.28 s and its corrected rerun passed 1/1 in 5.60 s. Thus every test in
each selected inventory has a successful final result, with zero detector
findings in both the full runs and reruns.

From `/workspace/zr_vm-perf`, with the environment exported above:

```sh
ctest --test-dir build/r2-merged-release -j4 -E '^performance_report$'
ctest --test-dir build/r2-asan -j4 -E '^performance_report$'
ctest --test-dir build/r2-merged-gcstress -j4 \
  --tests-from-file /tmp/zr-swarm-opt-gc-tests.txt --no-tests=error
ctest --test-dir build/r2-merged-release -j2 -R '^(aot_c_metadata_binding_loader|language_pipeline)$'
ctest --test-dir build/r2-asan -j2 -R '^(aot_c_metadata_binding_loader|language_pipeline)$'
ctest --test-dir build/r2-merged-gcstress -j2 -R '^aot_c_metadata_binding_loader$'
```

Current engine raw logs use `/tmp/zr-swarm-gd-c2-{release,asan,gcstress}-engine.log`
and `-engine-rerun.log`. The forced-GC selection is the same 129 test names in
`/tmp/zr-swarm-opt-gc-tests.txt` used by the previous engine acceptance note.
This is not a full forced-GC language-suite run.

Only GD-C2 is implemented. Game content, GD-C3+, `zr_swarm`, kernels/hybrid,
software/GL rendering, images, performance matrices, runtime reflection and
package import integration remain deferred.

## GD-C1 optional probes integration (2026-10-20)

Engine `ed10137f0dcc5c2ddf50eaeed1a41a70aef8fcc3` was pushed and then merged as
`49428df9f880578e0ede99e8d254b95f91cf7c3c` without resetting the demo branch.
The existing CLI, embedded host, FFI fixture, and both descriptor fixtures were
rebuilt in all three existing demo trees. No game implementation started.

| Configuration | Demo CTests | Required checks | Successful checks | P22 required pairs | CTest time |
| --- | --- | --- | --- | --- | --- |
| Release | 64/64 | 38/38 | 58/62 | 18/18 | 8.81 s |
| ASan/UBSan | 64/64 | 38/38 | 58/62 | 18/18 | 15.77 s |
| Forced full GC + ASan/UBSan | 64/64 | 38/38 | 58/62 | 18/18 | 15.33 s |

P02, P07, P11 binary, P17, P18, P20, and the P14 native alias now pass, and their
expected-failure settings were removed. P19 and the package case retain FAIL
status and four exact expected diagnostics across the two execution modes.
P22 reports only those two pairs without successful optional parity.
There are no detector findings in the final runs. CLI/runtime-library hashes and
per-case commands, source-removal state, and execution markers are in status.json and
the raw evidence directories below.

```sh
export PATH="/workspace/zr_vm-perf/build/local-tools/usr/bin:/workspace/zr_vm-perf/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="/workspace/zr_vm-perf/build/local-tools/usr/lib/x86_64-linux-gnu"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core
cmake --build build/demo-release -j3 --target zr_vm_cli_executable zr_swarm_probe_host zrgame_ffi zrgame_probe zrgame_inline_probe
cmake --build build/demo-asan -j3 --target zr_vm_cli_executable zr_swarm_probe_host zrgame_ffi zrgame_probe zrgame_inline_probe
cmake --build build/demo-gcstress -j3 --target zr_vm_cli_executable zr_swarm_probe_host zrgame_ffi zrgame_probe zrgame_inline_probe
ctest --test-dir build/demo-release -L demo -j4 --output-on-failure
ctest --test-dir build/demo-asan -L demo -j4 --output-on-failure
ctest --test-dir build/demo-gcstress -L demo -j4 --output-on-failure
```

The detector and forced-GC build flags are unchanged. New raw build and CTest
logs are `/tmp/zr-swarm-opt-demo-{release,asan,gcstress}-{build,ctest}.log`;
private probe projects/results are under
`/tmp/zr-swarm-opt-demo-{release,asan,gcstress}-probes/`. Local probe-results
and CTest Temporary paths are symlinks to `/tmp`.

Engine validation is Release 515/515, detector 515/515 final per-test outcomes
(full run plus the two metadata-expectation reruns), forced-GC 129/129 selected,
all 28 Swarm tests in each configuration, and the Release performance gate 1/1.
See the optional-probe acceptance note for the precise coverage limits and
remaining reflection/package work. Historical sections below describe prior
revisions and are superseded by this section.

## GD-C1 engine integration (2026-10-20)

Engine `d01387a9211dc6eed519f5fb0c4321fa258f2eab` was merged as `ae003e05ce4c8dc0b7c3f258ee4eaa6dacce1d9b` without resetting the demo
branch. All three existing demo build trees were rebuilt, including CLI,
embedded host, FFI library, and both descriptor plugins.

| Configuration | Demo CTests | Required mode checks | Successful mode checks | P22 required pairs |
| --- | --- | --- | --- | --- |
| Release | 64/64 | 38/38 | 45/62 | 18/18 |
| ASan/UBSan | 64/64 | 38/38 | 45/62 | 18/18 |
| Forced full GC + ASan/UBSan | 64/64 | 38/38 | 45/62 | 18/18 |

The 17 optional failing mode checks retain FAIL status and exact expected-error
checks. P22 explicitly gates required P01–P20 cases and reports optional parity
separately. The feature gate also checks P21 and the current result files.
Demo CTest wall times are 6.95 s (Release), 15.90 s (ASan/UBSan), and 16.71 s
(forced GC). There are no detector findings in these final runs. Historical results below
describe earlier revisions and are superseded by this section.

```sh
export PATH="/workspace/zr_vm-perf/build/local-tools/usr/bin:/workspace/zr_vm-perf/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="/workspace/zr_vm-perf/build/local-tools/usr/lib/x86_64-linux-gnu"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core
cmake --build build/demo-release -j3 --target zr_vm_cli_executable zr_swarm_probe_host zrgame_ffi zrgame_probe zrgame_inline_probe
cmake --build build/demo-asan -j3 --target zr_vm_cli_executable zr_swarm_probe_host zrgame_ffi zrgame_probe zrgame_inline_probe
cmake --build build/demo-gcstress -j3 --target zr_vm_cli_executable zr_swarm_probe_host zrgame_ffi zrgame_probe zrgame_inline_probe
ctest --test-dir build/demo-release -L demo -j4 --output-on-failure
ctest --test-dir build/demo-asan -L demo -j4 --output-on-failure
ctest --test-dir build/demo-gcstress -L demo -j4 --output-on-failure
```

The existing Debug flags remain `-fsanitize=address,undefined
-fno-omit-frame-pointer`; the GC tree additionally defines
`ZR_DEBUG_GARBAGE_COLLECT_MEM_TEST`. New raw build/CTest logs are
`/tmp/zr-swarm-demo-{release,asan,gcstress}-{build,ctest}.log`. Per-case commands,
CLI hashes, source-removal state, execution markers, and raw logs are under
`/tmp/zr-swarm-demo-{release,asan,gcstress}-probes/`.

The engine acceptance note records full Release 499/499, full ASan/UBSan
499/499, forced-GC 112/112 selected, all 12 swarm regressions per tree, and the
Release performance gate 1/1. The demo-enabled trees retain 499 core tests when
excluding the `demo` label and `performance_report`; this is an inventory check,
not a second full core-suite run. GD-C2 and game work remain unstarted.

## GD-C0 (2026-10-18)

- Worktree: `/workspace/zr_vm-demo`, branch `demo/swarm-arena-20261018`,
  base `3fe24909` from `perf/interpreter-20261017`.
- Configure OFF and ON: both succeeded using CMake and Ninja from
  `/workspace/zr_vm-perf/build/tools/bin`, with Release, tests ON, language server
  and Rust binding OFF, and the report's absolute libffi include/library paths.
- CTest JSON inventories are identical: 426 registered tests with
  `ZR_VM_REGISTER_PERFORMANCE_CTEST=ON`; 425 excluding `performance_report`, as in
  the r2 core command. This is a registration check, not a claim that all tests ran.
- Evidence: `build/demo-release/configure-{off,on}.log` and
  `build/demo-release/ctest-{off,on}.json`. These generated files are ignored.
- The dedicated worktree initially lacked submodule contents. Each gitlink was
  populated at its recorded SHA by an independent `git clone --no-hardlinks
  --no-checkout` from the corresponding read-only perf dependency, followed by a
  detached checkout inside this worktree. No other worktree's submodule Git state
  was reused or modified.
- `.codex/config.toml` remains ignored and uncommitted.

## GD-C1 (2026-10-18) — stopped on required P01

The Release CLI build succeeded (`cmake --build build/demo-release -j6 --target
zr_vm_cli`, 990 build steps). Both P01 failure variants and passing controls run
against this freshly built binary. Earlier exploratory checks used the read-only
r2 CLI, and the results were then independently reproduced in this worktree.

```sh
/workspace/zr_vm-perf/build/tools/bin/cmake -S . -B build/demo-release \
  -DZR_VM_BUILD_DEMOS=ON \
  -DPython3_EXECUTABLE=/workspace/zr_vm-perf/build/tools/bin/python3
/workspace/zr_vm-perf/build/tools/bin/ctest --test-dir build/demo-release -L demo -j2 --output-on-failure
/workspace/zr_vm-perf/build/tools/bin/ctest --test-dir build/demo-release \
  --show-only=json-v1 -LE demo -E '^performance_report$'
```

- Release demo results: **6/7 checks pass, readiness gate fails**, CTest exit 8.
  Four checks reproduce the exact known error using `WILL_FAIL`; their probe
  result remains FAIL. Two controls actually pass. See
  `build/demo-release/ctest-demo.log` and
  `build/demo-release/demos/swarm_arena/probe-results/*/{result.json,run.log}`.
- Every binary check compiles a fresh `.zro`, removes the copied source directory
  and only then runs binary mode. There is no source fallback. Successful control
  runs also require `executed_via=binary`.
- Core inventory comparison: the same **425 names, commands and properties**
  before/after C1, excluding the `demo` label and `performance_report`. CMake's
  diagnostic backtrace indexes are ignored in this comparison. Evidence:
  `build/demo-release/ctest-core-after-probes.json` versus `ctest-off.json`.
  `ZR_VM_TEST_TIER=core` controls fixtures inside engine runners; `-LE demo` is
  necessary when listing/running just core tests in a demo-enabled tree.
- Final OFF/ON configure recheck also succeeded: OFF has the exact original
  426-test inventory (including the performance report); ON adds only seven
  `demo_` tests. Evidence: `build/demo-release/configure-final-{off,on}.log` and
  `ctest-final-{off,on}.json`.
- The full 425-test engine suite was not rerun. This batch changes demo probes,
  documentation and opt-in registration only. No game smoke, full scenario,
  hash-equivalence or perf tests exist yet.
- Probe P01 cost, full P22 parity, host roundtrip, W0–W14, goldens and screenshots
  remain unmeasured. P02–P22 are explicitly SKIP under the stop rule.

### ASan + UBSan diagnostic checks

Configured `build/demo-asan` with the same dependency paths and feature flags,
Debug and `-DCMAKE_C_FLAGS='-fsanitize=address,undefined
-fno-omit-frame-pointer'`; CLI target build succeeded. Ran:

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  /workspace/zr_vm-perf/build/tools/bin/ctest --test-dir build/demo-asan \
  -L demo -j2 --output-on-failure
```

Result: **6/7**, with only `demo_feature_gate` failing (CTest exit 8). Both engine
diagnostics reproduce in interp/binary and both controls pass. No ASan or UBSan
errors in those checks. Leak detection was disabled as in r2; this is **not** a
leak-clean shutdown claim. Evidence: `build/demo-asan/{configure.log,
build-cli.log,ctest-demo.log}` and its `demos/swarm_arena/probe-results/` directory.

### Forced-GC + ASan + UBSan diagnostic checks

Configured `build/demo-gcstress` identically to the ASan tree with the additional
`-DZR_DEBUG_GARBAGE_COLLECT_MEM_TEST` C flag; CLI target build succeeded. Ran:

```sh
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  /workspace/zr_vm-perf/build/tools/bin/ctest --test-dir build/demo-gcstress \
  -L demo -j2 --output-on-failure
```

Result: **6/7**, only `demo_feature_gate` failing (CTest exit 8). Main and nested
diagnostics reproduce in both execution modes; controls pass in both modes.
No ASan/UBSan errors in those checks, with leak detection disabled. Evidence:
`build/demo-gcstress/{configure.log,build-cli.log,ctest-demo.log}` and its
`demos/swarm_arena/probe-results/` directory. These are diagnostic checks of P01,
not the unimplemented gameplay/host/lane smoke coverage required by GD-H6.

Required failures and the remaining v1 DoD gaps are recorded in
[feature-readiness.md](feature-readiness.md). Implementation has not advanced to
GD-C2 or any H/M phase.
