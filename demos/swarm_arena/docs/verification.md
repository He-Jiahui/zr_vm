# Verification evidence

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
