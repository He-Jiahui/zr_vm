---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_fusion.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
tests:
  - tests/parser/test_ssa_generated_fusion.c
  - tests/parser/test_ssa_generated_fusion_binding_call.inc
  - tests/cmake/ssa-tests.cmake
  - docs/instruction-generation/execbc-pattern-lowering.md
doc_type: acceptance
status: scoped
---

# BINDING_CALL callee operand-role regression

## Scope

This slice tightens the parser-side `BINDING_CALL` fusion precondition. The
`PLACE_PROJECT` result must be CALL operand 0, the canonical callee. A use only
in an explicit argument slot remains an ordinary argument and does not permit
the projection to be reinterpreted as the callee. Such a pair remains unfused
with `RESULT_MISMATCH`.

This is projection validation. It does not execute a fused runtime handler or
close the full 03.04 plan.

## Regression fixture

`test_ssa_generated_fusion_binding_call.inc` builds a Verify-accepted function
with an independent callee in CALL operand 0 and the projected PLACE_PROJECT
result in CALL operand 1. The fixture runs the ExecIR Oracle first: the callee
provider observes `0xC0`, the projected argument is `0xA0`, and the function
returns `0xD0`. It then requires the original PLACE_PROJECT and CALL, zero
fused pairs, a `RESULT_MISMATCH` fallback, and a valid fusion plan.

Before the matcher change, the focused GCC target built successfully and the
direct test failed at the expected `plan.fusedCount == 0` assertion, after the
Verify and Oracle assertions passed. This showed that generic same-result use
was accepting an argument dependency as a callee dependency.

## RED evidence

The test-only RED used the existing
`/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc` cache:

    cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_ssa_generated_fusion_test -- -j2
    /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc/bin/zr_vm_ssa_generated_fusion_test

The target build exited 0. Before the matcher guard, the direct binary exited
1 at `plan.fusedCount == 0u` in the new fixture. The preceding Verify ALL and
Oracle checks passed, including the independent callee and projected argument
observations.

## GCC GREEN verification

All build outputs used the existing
`/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc` cache. The post-fix focused target
completed successfully:

    cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_ssa_generated_fusion_test -- -j2

The direct Unity binary exited 0:

    /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc/bin/zr_vm_ssa_generated_fusion_test

The registered CTest passed 1/1 in 1.07 seconds:

    ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --output-on-failure -R '^ssa_generated_fusion$'

The pattern generator freshness check exited 0:

    python3 /mnt/e/Git/zr_vm/scripts/codegen/generate_execbc_patterns.py --input /mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def --output /mnt/e/Git/zr_vm/zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h --check

The target build first spent about 23 minutes in CMake `VerifyGlobs.cmake`
scanning the repository over WSL 9p; it then completed the three required
compile/link steps with exit 0. No build output was written outside the D-drive
cache.

The root agent independently verified the frozen source with MSVC: CMake
configure/generate succeeded, `zr_vm_parser_static` and the focused target
built (196/196), the direct binary exited 0, registered CTest passed 1/1, and
the same generator freshness check exited 0.

## Limits

The check relies on the ExecIR CALL operand convention, where operand 0 is the
canonical callee and later operands are explicit arguments. This validates
parser-side projection only; the generated fusion word still has no executable
runtime handler. Other 03.04 boundaries, dispatch integration, and runtime
performance remain open.
