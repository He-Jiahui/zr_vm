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

Later phases append their commands, outcomes and gaps here. Required probe
failures are recorded in [feature-readiness.md](feature-readiness.md).
