# SSA 08.01 EIS5 canonical Compare modes

## Scope

This bounded EIS5 v5 extension accepts the six canonical Compare modes already
implemented by the ExecIR Oracle. The mode is stored in the existing instruction
`typeToken`; the header, field order, record widths, payload version, ZRAF v6
container, ERI1 v1 bundle, and AOT ABI 17 do not change. Each Compare has two
i64 operands and one BOOL result. A following conditional branch returns i64
42 for true and i64 7 for false. The fixture payload remains 1508 bytes.

The test table exercises a true and false input for every mode:

| Mode | True input | False input |
| --- | --- | --- |
| EQ (0) | 1 == 1 | 1 == 2 |
| LT (1) | 1 < 2 | 2 < 1 |
| LE (2) | 1 <= 2 | 3 <= 2 |
| GT (3) | 3 > 2 | 1 > 2 |
| GE (4) | 2 >= 2 | 1 >= 2 |
| NE (5) | 1 != 2 | 2 != 2 |

Every graph first passes `ZrCore_ExecIr_VerifyModule`; the Oracle confirms the
expected branch and return value. Direct scalar codec write/read covers all
twelve cases. The canonical writer emits each case in the existing write phase,
and the separate read phase opens and evaluates all twelve artifacts.

## RED

Before widening the codec's Compare allowlist, the six-mode graphs passed the
ExecIR verifier and the Oracle preflight. The current-source Clang build exited
0. The exact RED command then failed at the first unsupported mode, EQ=0:

```text
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang --output-on-failure -R '^ssa_exec_ir_artifact_v6_write$'
FAIL: EIS5 Compare mode 0 dynamic size query
```

## GREEN verification

The current-source Clang build used the existing D cache
`D:\tmp\zr_vm\ssa-artifact-v6-clang`:

```powershell
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang `
  --target zr_vm_ssa_exec_ir_artifact_v6_test -- -j2
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang `
  --output-on-failure `
  -R '^(artifact_schema|ssa_schema_relocation|ssa_exec_ir_artifact_v6_write|ssa_exec_ir_artifact_v6_roundtrip)$'
```

The build exited 0. All four CTests passed: `artifact_schema`,
`ssa_schema_relocation`, `ssa_exec_ir_artifact_v6_write`, and
`ssa_exec_ir_artifact_v6_roundtrip`.

Root independently built the frozen codec and test sources with MSVC in
`D:\tmp\zr_vm\ssa-artifact-v6-msvc`; that build exited 0 and the same four
CTest gates passed 4/4.

The Compare mode field is at payload byte offset 800. The writer's size query
and the direct reader reject mode 6 with `INVALID_SECTION` at offset 800. The
canonical opener rejects a rehashed mode-6 artifact in `EXEC_IR_BUNDLE` at
absolute offset `payloadOffset + 800`. The writer's size query, direct reader,
and opener also reject a non-i64 input at payload value-record offset 276 and a
non-BOOL result at offset 324. The input mutation changes both the first
constant type and its value type to BOOL so validation reaches the Compare
operand. Rehashed reader mutations leave the output graph empty; failed
canonical opens leave the caller-provided module empty.

The four gates retain the EIS1 byte-for-byte golden and EIS2 through EIS4
payload and rejection regressions, prior EIS5 i64 and BOOL predicates, and the
996-byte EIS5 count-collision route. Readers predating Compare reject its opcode
at the instruction record; the earlier LT-only v5 reader rejects modes other
than LT at the mode field. Because the payload version is unchanged, artifacts
using these modes require a reader with six-mode Compare support.

## Acceptance boundary

This adds only the six canonical i64-to-BOOL Compare modes to the existing
counted EIS5 scalar CFG. It does not add further opcodes, general ExecIR
persistence, maps, bindings, relocation resolution, ExecBC, package copy, AOT
projection, native callable ABI, or completion of the full 08.01 milestone.
