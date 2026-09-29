# SSA 03.04 Increment loop branch type contract

## Scope

This subtask verifies the parser-side `INCREMENT_LOOP_BRANCH` projection for
signed i64 arithmetic. The ExecIR Oracle accepts an i64 ADD result as a
conditional branch value: 4+1 takes the truthy successor, while 5+(-5) takes
the false successor. The matcher now requires both ADD inputs, the ADD result,
and the branch condition to be signed i64, with the ADD result used directly
as the condition.

The generated pattern row records `SIGNED_I64_CONDITION`. The side entry keeps
the pattern identity and the plan hash includes the generated pattern-schema
hash, so validation is tied to this type contract without adding a redundant
type field or changing the plan layout version. An object-typed fixture passes
ExecIR Verify but remains unfused with `TYPE_MISMATCH`. Branch successors,
projected PCs, source IDs, and fixed-width output size remain asserted.

The focused regression also calls the extracted private operand/result pool
accessors with an empty suffix and a guarded one-element buffer. The initial
test failed because the old `index > count - start` test admitted index zero
when no pool element remained. Both accessors now reject that range before a
read.

## RED evidence

Before the signed-i64 matcher guard, the D GCC target built successfully. The
direct test ran Verify and both Oracle branch cases, then exited 1 because the
matcher fused the Verify-accepted object-typed ADD/branch pair instead of
falling back. This establishes the negative type boundary independently from
the parser's structural verifier.

After extracting the typed pool accessors, a focused malformed-range assertion
also exited 1 against the old inclusive bound. Its guard element kept the test
memory-safe while demonstrating that an empty suffix was incorrectly reported
as readable.

## GCC validation

All build outputs stayed in the existing D-drive cache
`/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc`.

```text
wsl.exe -d Ubuntu-22.04 -- cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_ssa_generated_fusion_test -- -j4
wsl.exe -d Ubuntu-22.04 -- /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc/bin/zr_vm_ssa_generated_fusion_test
wsl.exe -d Ubuntu-22.04 -- ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc -R '^ssa_generated_fusion$' --output-on-failure
wsl.exe -d Ubuntu-22.04 -- python3 /mnt/e/Git/zr_vm/scripts/codegen/generate_execbc_patterns.py --input /mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def --output /mnt/e/Git/zr_vm/zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h --check
```

The focused target built with exit status 0, the direct test exited 0, the
registered `ssa_generated_fusion` CTest passed 1/1, and the generator check
exited 0 with no stale output. The direct fixture confirms stable generated
plan hashes, the pattern-schema constraint, plan validation, the two i64 Oracle
branch outcomes, the object fallback, and rejection of empty operand/result
pool suffixes.

The independent MSVC validation used the existing D-drive cache
`D:/tmp/zr_vm/ssa-artifact-v6-msvc`. The `zr_vm_ssa_generated_fusion_test`
target and `zr_vm_parser_static` library builds exited 0, the direct test exited
0, registered `ssa_generated_fusion` CTest passed 1/1, and the Windows
generator `--check` exited 0.

## Boundary

This is parser-side fusion-plan projection plus ExecIR Oracle validation of the
original unfused operations. It does not execute an `INCREMENT_LOOP_BRANCH`
fused word or establish a runtime handler. Dispatcher integration, end-to-end
source/resume behavior, performance budgets, and the full 03.04 boundary
matrix remain open.
