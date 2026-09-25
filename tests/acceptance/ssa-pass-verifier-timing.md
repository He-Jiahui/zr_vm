---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_pass_manager.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
tests:
  - tests/parser/test_ssa_pass_manager_scalar.c
doc_type: testing-guide
status: focused-passed
---

# SSA per-pass verifier timing acceptance

## Scope

Each successful scalar pass exposes the count and cumulative CPU ticks of
the two complete verifier checks around its run.  The separate preflight
verification is not charged to a pass.  Existing pass `elapsedTicks` excludes
verifier time; neither metric drives budget or optimization policy.  A fast
call can produce zero ticks, so the test checks the deterministic invocation
count rather than asserting nonzero clock resolution.

## Reproduce

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
```

Before implementation, the new fixture failed to compile because scalar
remarks had no verifier-check count.  After implementation, every emitted
remark in both fixed-point runs reports exactly two checks.

## Toolchain results (2026-09-26)

- WSL GCC 11.4 and Clang 14: each rebuilt the scalar pipeline, deopt
  aggregates, GVN, LICM, vectorizer, and optimization-remarks test targets;
  focused CTest passed 6/6 on each compiler.
- Windows MSVC 19.44: rebuilt the same six targets; focused CTest passed
  6/6.  Existing `/W3` versus `/W4` override and C4293 warnings in the
  optimization-remarks fixture were emitted without test failures.
- WSL GCC ASan/UBSan: rebuilt scalar pipeline target; focused CTest 1/1
  passed with no sanitizer failure.
- GCC SSA-labelled CTest passed 80/80 after the six affected targets were
  freshly rebuilt; other SSA executables remained from prior builds.

Accepted for per-pass verifier CPU timing and boundary-count telemetry only.
No wall-clock deadline, benchmark performance claim, or full SSA milestone
acceptance is implied.
