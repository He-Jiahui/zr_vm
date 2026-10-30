# GD-M7 — non-gating CI

The `demo` job in [interpreter-gates.yml](../../../.github/workflows/interpreter-gates.yml)
has independent Release and ASan/UBSan matrix legs on `ubuntu-latest`, with
job-level `continue-on-error: true` and `fail-fast: false`. Neither interpreter
job depends on it. Existing workflow triggers, permissions, correctness steps
and performance steps are unchanged. The feature branch can use the existing
`workflow_dispatch` trigger; pushes trigger this workflow only on its existing
`main` and `perf/interpreter-20261017` branches.

Both legs enable `ZR_VM_BUILD_DEMOS`, tests, CLI and network, disable the
language server and extension, and build only the CLI, Swarm host, host/gfx
plugins, FFI fixture and native runtime tests. Compilation uses one job below
6 GiB MemAvailable, two otherwise; CTests run serially. Temporary projects stay
inside each leg's build directory through `TMPDIR`.

Release enables the C session binding (`BUILD_RUST_BINDING=ON`, no Cargo build).
Its eleven tests are the demo portion of GD-M6's green 12-test regression set:

- `demo_native_runtime`, `demo_native_smoke_interp`, `demo_native_smoke_binary`.
- `demo_cli_smoke_interp_dod`, `demo_cli_smoke_binary_dod`.
- `demo_host_smoke_dod`, `demo_host_smoke_c`, `demo_cli_host_parity`,
  `demo_lane_equivalence`.
- `demo_session_smoke`, `demo_session_budget`.

The additional label filter `-L '^demo$'` and anchored name filter keep this
selection separate from core CTests. No benchmark tier or core label changes.
The existing smoke runners check committed hashes, state and failure contracts;
CI does not regenerate goldens or run the GD-M6 AOT finding probe.

The `swarm-demo-perf-matrix` artifact contains the existing W10 runner's
`build-demo-release/perf-matrix/result.json` (schema `zr-swarm-w10/1`), Markdown
and raw logs when produced. This is a small **W10 boundary-call matrix**:
three functions across four call paths, interpreted mode, 2,000 calls,
120 warmup calls and one repetition. Checksums and execution markers are
verified. Raw loop timings include loop/checksum/argument costs; affinity,
load averages and regression eligibility are recorded by the runner. There is
no timing threshold. This CI sample does not implement the deferred full
W0–W14 matrix, AOT timing, capacity sweep or a stable performance baseline.
The perf step runs after a successful build even if a Release CTest fails.
Missing results after an earlier failure are ignored by artifact upload;
failed measurements retain their real status in the runner's JSON.

ASan/UBSan uses a separate Debug tree, with both sanitizers applied through
`CMAKE_C_FLAGS` to the VM, host and demo plugins. It runs the three native
tests, both CLI DOD smokes, and DOD/C host smokes (seven tests). It disables the
session binding, so this lane makes no session/rollback coverage claim.
`ASAN_OPTIONS=detect_leaks=0` matches the existing interpreter sanitizer policy;
UBSan stops on errors. This is memory-error smoke coverage, with no leak-clean
shutdown claim. Sanitizer findings still fail the demo leg and are retained in
its logs; job-level tolerance prevents them from gating interpreter correctness.

Each matrix leg always attempts to upload `swarm-demo-release` or
`swarm-demo-asan`, containing CTest and runner diagnostics. A failed build or
cancelled runner can leave these artifacts absent. GHA execution remains to be
observed through a workflow run; local validation is recorded in the external
GD-M7 report.
