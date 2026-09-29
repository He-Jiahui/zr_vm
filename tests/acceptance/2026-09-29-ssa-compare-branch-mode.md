# SSA 03.04 Compare branch mode projection

## Scope

This subtask corrects the parser-side `COMPARE_BRANCH_INT` fusion plan. The
ExecIR Compare `typeToken` is a canonical mode selector: 0=EQ, 1=LT, 2=LE,
3=GT, 4=GE, and 5=NE. The comparison result is BOOL, with two signed i64
inputs. The plan side entry now preserves the mode separately, includes it in
the generated hash, and rejects non-canonical modes during validation. Plan
schema version is 2.

The test covers all six modes, deterministic plan hashes, plan validation,
wrong input/result types, and invalid mode 6 fallback. Existing synthetic
Compare fixtures now have the same i64-to-BOOL shape while retaining their
branch target and source-map assertions.

## RED evidence

Before the matcher change, the mode-2 (LE) fixture built successfully, but the
direct test reported `fused=5` and failed the expected six-window assertion.
The generic matcher had compared the selector value 2 with the BOOL result
type token 1, so it retained the Compare/branch pair as unfused instructions.

## GCC validation

All build output stayed in the existing D-drive cache
`/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc`.

```text
wsl.exe -d Ubuntu-22.04 -- cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc --target zr_vm_ssa_generated_fusion_test -- -j4
wsl.exe -d Ubuntu-22.04 -- /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc/bin/zr_vm_ssa_generated_fusion_test
wsl.exe -d Ubuntu-22.04 -- ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc -R ssa_generated_fusion --output-on-failure --no-tests=error
wsl.exe -d Ubuntu-22.04 -- python3 /mnt/e/Git/zr_vm/scripts/codegen/generate_execbc_patterns.py --input /mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def --output /mnt/e/Git/zr_vm/zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h --check
```

The focused target built with exit status 0. The direct test exited 0. The
registered CTest ran `ssa_generated_fusion` and passed 1/1. The generator check
exited 0 with no stale output.

The root agent independently validated the frozen source in
`D:/tmp/zr_vm/ssa-artifact-v6-msvc`: the focused target build exited 0, the
binary exited 0 when run directly, and registered `ssa_generated_fusion`
CTest passed 1/1 in 0.18 seconds.

## Boundary

This record covers parser-side projection, metadata retention, deterministic
hashing, and validation. It does not establish an executable runtime handler
for the generated fused word. Runtime dispatch integration and the remaining
03.04 performance, source/resume, and boundary matrix stay open.
