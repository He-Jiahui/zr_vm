---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/test_ssa_deopt_aggregates.c
doc_type: testing-guide
status: focused-passed
---

# SSA pass-manager token phi hash acceptance

## Scope

The function hash used for analysis-cache identity and pass-change accounting
must change when any block effect or memory phi result or incoming range
changes.  It must return to its baseline after the field is restored.  The
test covers all memory regions, both start and count coordinates, and the
effect phi.  It does not establish backend parity or finish the M1 verifier
milestone.

The two directly compiled parser test targets must include the loop effect
collector with the effect synthesis entry point; otherwise a fresh build
fails to link before any hash assertion runs.

## Reproduce

From the repository root, with the configured build directories:

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_pass_manager_scalar_test zr_vm_ssa_deopt_aggregates_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_(pass_manager_scalar|deopt_aggregates)$' --output-on-failure --no-tests=error
```

Before the hash change, the newly rebuilt scalar test failed at the first
`effectPhiResult` assertion; the deopt aggregates test passed.  See the
recorded toolchain results below for the final acceptance decision.

## Toolchain results (2026-09-26)

- WSL GCC 11.4: freshly rebuilt both targets, focused CTest 2/2 passed;
  `ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure
  --no-tests=error -j 4` passed 80/80.  Only the directly affected targets
  were rebuilt before that broad run; other existing SSA binaries were not
  refreshed.
- WSL Clang 14: freshly rebuilt both targets; focused CTest 2/2 passed.
- Windows MSVC 19.44: freshly rebuilt both targets; focused CTest 2/2 passed.
  The build emitted existing `/W3` versus `/W4` override and unrelated
  long-object-path CMake warnings.
- WSL GCC AddressSanitizer/UndefinedBehaviorSanitizer build
  `build/ssa-gcc-asan-phase80`: freshly rebuilt both targets and ran focused
  CTest 2/2 passed, with no sanitizer failure.

Accepted for token-phi hash identity and direct-test linkage only.  The
broader M1 pass-failure artifact requirement remains open.
