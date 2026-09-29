# SSA 08.01 EIS5 two-DIV effect chain

## Scope

This bounded follow-on extends the EIS5 v5 counted CFG with at most two
sequential signed-i64 `DIV` instructions in the same straight-line basic block.
The first DIV carries `MAY_THROW` and effect IDs 1→2; the second carries
`MAY_THROW` and IDs 2→3. The v5 header, instruction width, and EIS1–EIS4
formats remain unchanged. All other supported instructions retain zero flags
and effect IDs. Cross-block chains, more than two DIV instructions, and general
effect-chain serialization are outside this slice.

An earlier EIS5 reader that already supports the single-DIV form expects 1→2
on every DIV and therefore rejects the second DIV at its `effectIn` field.
Readers predating DIV reject the opcode itself. Both compatibility boundaries
follow from the unchanged EIS5 v5 version and record layout.

## Test-only RED

The new one-block fixture builds constants 84, -7, and 3, then evaluates
`84 / -7` followed by `-12 / 3`. `VerifyModule` accepts the canonical 1→2→3
effect chain, and the ExecIR Oracle returns -4 before the codec size query.
The existing Clang target built successfully, and the exact write CTest then
failed at the intended unsupported writer boundary:

```text
EIS5 two-DIV effect-chain size query: status=10 offset=820 size=4294967295 expectedSize=964
FAIL: EIS5 two-DIV effect-chain dynamic size query
```

The diagnostic is the second DIV's `effectIn` field. The old writer's
single-pair validator expected 1 there instead of the canonical 2.

## Current-source validation

The direct codec and canonical writer/opener assertions, rehashed malformed
effect-chain cases, and no-partial-publish checks pass on independent MSVC and
Clang builds. The MSVC build covered three current-source targets:
`zr_vm_ssa_exec_ir_artifact_v6_test`, `zr_vm_ssa_schema_relocation_test`, and
`zr_vm_artifact_schema_test`. All four registered CTests passed (4/4) on each
compiler: `artifact_schema`, `ssa_schema_relocation`,
`ssa_exec_ir_artifact_v6_write`, and `ssa_exec_ir_artifact_v6_roundtrip`.

The Clang build used the existing D-drive cache and completed all 359 steps
with exit code 0:

```text
cmd.exe /c wsl.exe -d Ubuntu-22.04 -- cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang --target zr_vm_ssa_exec_ir_artifact_v6_test zr_vm_ssa_schema_relocation_test zr_vm_artifact_schema_test -j2
```

The original CMake job completed VerifyGlobs and automatic regeneration, then
continued through compilation and linking to completion. No backup harness was
used; the empty harness scratch directory was removed.

The write and cross-process roundtrip CTests passed 2/2 (write 1.11 s,
roundtrip 0.16 s; CTest total 2.33 s):

```text
cmd.exe /c wsl.exe -d Ubuntu-22.04 -- ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang --output-on-failure -R ssa_exec_ir_artifact_v6
```

The schema CTests also passed individually. `artifact_schema` took 0.61 s
(CTest total 1.64 s), and `ssa_schema_relocation` took 0.02 s (CTest total
0.38 s):

```text
cmd.exe /c wsl.exe -d Ubuntu-22.04 -- ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang --output-on-failure -R artifact_schema
cmd.exe /c wsl.exe -d Ubuntu-22.04 -- ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang --output-on-failure -R ssa_schema_relocation
```

This bounded result does not claim full 08.01 completion.
