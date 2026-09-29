# SSA 03.04 LOAD_ADD_INT type contract

## Scope

This subtask narrows generated `LOAD_ADD_INT` to the verified signed i64
contract. A managed-heap `LOAD` must produce i64, and the following `ADD` must
consume that value and another i64, producing i64. Double, int32, and uint64
windows remain unfused with `TYPE_MISMATCH`. The pattern identity and generated
`SIGNED_I64_LOAD_ADD` constraint carry the contract through plan hashing and
validation, so the side entry does not need an extra type field or a plan
layout version change.

## RED evidence

The D GCC focused target built, and the new fixture passed
`ZrCore_ExecIr_VerifyFunction(..., ZR_EXEC_IR_VERIFY_ALL, ...)`. Its Oracle
memory callback returned DOUBLE 40.5; the DOUBLE ADD with 1.25 returned 41.75.
The direct binary then exited 1 at the intended assertion that this
Verify-accepted DOUBLE pair remain unfused. GDB at that assertion reported
`fusedCount=1`, `fallbackCount=1`, and `sideEntryCount=1`, with the side entry
pattern `LOAD_ADD_INT`.

## GCC validation

All build outputs stayed in the existing D-drive cache
`/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc`.

```text
wsl.exe -d Ubuntu-22.04 -- cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_ssa_generated_fusion_test -j2
wsl.exe -d Ubuntu-22.04 -- /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc/bin/zr_vm_ssa_generated_fusion_test
wsl.exe -d Ubuntu-22.04 -- ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc -R '^ssa_generated_fusion$' --output-on-failure --no-tests=error
wsl.exe -d Ubuntu-22.04 -- python3 /mnt/e/Git/zr_vm/scripts/codegen/generate_execbc_patterns.py --input /mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def --output /mnt/e/Git/zr_vm/zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h --check
```

The focused target built successfully; direct execution exited 0; registered
`ssa_generated_fusion` CTest passed 1/1 in 0.17 seconds; generator `--check`
exited 0. The fixture checks the signed i64 positive case's stable generated
hash and `Validate`, along with DOUBLE, int32, and uint64 fallback.

## Boundary

This covers ExecIR Verify, Oracle execution of the original DOUBLE operations,
and parser-side fusion-plan projection. It does not execute a `LOAD_ADD_INT`
fused runtime handler. Dispatcher integration, end-to-end source/resume
behavior, performance budgets, and the full 03.04 boundary matrix remain open.
