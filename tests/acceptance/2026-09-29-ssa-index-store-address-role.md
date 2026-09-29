---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_fusion.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
tests:
  - tests/parser/test_ssa_generated_fusion.c
  - tests/parser/test_ssa_generated_fusion_index_store.inc
  - tests/cmake/ssa-tests.cmake
  - docs/instruction-generation/execbc-pattern-lowering.md
doc_type: acceptance
status: scoped
---

# INDEX_LOAD_STORE STORE address-role regression

## Scope

This closes one matcher gap in the STORE variant of the parser-owned
INDEX_LOAD_STORE projection. The PLACE_PROJECT result must be the STORE
address at operand 0. If it is only the assigned value at operand 1, the
original two operations remain in the plan with RESULT_MISMATCH.

The generated fusion word still has no executable runtime handler. This record
does not close the full 03.04 plan.

## Regression fixture

The fixture in tests/parser/test_ssa_generated_fusion_index_store.inc first
verifies the function with ZR_EXEC_IR_VERIFY_ALL, then executes it through the
ExecIR Oracle. PLACE_PROJECT returns unsigned token 0xA0; the STORE address is
a separate external value 0xB0. The memory provider observes address 0xB0
and assigned value 0xA0, and the function returns 0xB0.

Before the matcher change, the focused direct test exited 1 at the assertion
that this window must have zero fused pairs. The preceding Verify and Oracle
assertions had passed, so the failure isolated the matcher accepting a
projected value as though it were the projected address.

After the change, the fixture requires the original PLACE_PROJECT and STORE
instructions, zero fused pairs, a RESULT_MISMATCH fallback, and successful
fusion-plan validation. The existing positive STORE-address fixture remains
in the same direct suite.

## GCC verification

Build outputs were confined to the existing
/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc cache.

The focused target build exited 0:

    cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_ssa_generated_fusion_test -- -j2

The direct Unity binary exited 0:

    /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc/bin/zr_vm_ssa_generated_fusion_test

The registered ssa_generated_fusion CTest passed 1/1:

    ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --output-on-failure -R '^ssa_generated_fusion$'

Generator freshness check exited 0:

    python3 /mnt/e/Git/zr_vm/scripts/codegen/generate_execbc_patterns.py --input /mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def --output /mnt/e/Git/zr_vm/zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h --check

The CTest took 0.13 seconds.

## Limits

This is parser-side projection validation. It does not exercise an executable
fused runtime handler, generated dispatch, or performance behavior. The rest
of the 03.04 boundary and integration gates remain open.
