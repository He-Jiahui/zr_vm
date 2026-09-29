# SSA 08.01 EIS5 i64 LT Compare leaf

## Scope

This bounded leaf adds one supported operation to the counted EIS5 v5 scalar
CFG: `COMPARE` with LT mode (`typeToken == 1`), two i64 operands, and one BOOL
result. A `CONDITIONAL_BRANCH` uses that result to choose i64 42 or 7. The
44-byte header, field order, record widths, payload version, ZRAF v6 envelope,
ERI1 v1 bundle, and AOT ABI 17 are unchanged. The fixture encodes to 1508
bytes.

The ExecIR verifier accepts this graph, and the Oracle evaluates both
conditions: `1 < 2` returns 42; `3 < 2` returns 7. Tests exercise direct codec
read/write and canonical artifact write/open across the existing write/read
process phases.

Only LT is accepted for EIS5 Compare. Its inputs must be i64 and result BOOL;
other Compare modes, result types, operand types, and opcodes remain outside
this leaf. Older EIS5 v5 readers do not recognize `COMPARE` in their opcode
allowlist and reject its instruction record as `INVALID_SECTION`. The payload
version is unchanged, so readers need this opcode extension to open these
artifacts.

## RED

Before the EIS5 codec accepted Compare, the fixture first passed
`ZrCore_ExecIr_VerifyModule` and the Oracle returned the expected values. The
existing write CTest then failed at the first missing codec behavior:

```text
FAIL: EIS5 i64 LT Compare dynamic size query
```

## GREEN verification

The current-source Clang build used the existing D cache
`D:\tmp\zr_vm\ssa-artifact-v6-clang`:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  --target zr_vm_ssa_exec_ir_artifact_v6_test \
           zr_vm_ssa_schema_relocation_test \
           zr_vm_artifact_schema_test -- -j2
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang \
  --output-on-failure \
  -R '^(artifact_schema|ssa_schema_relocation|ssa_exec_ir_artifact_v6_write|ssa_exec_ir_artifact_v6_roundtrip)$'
```

The build exited 0. All four CTests passed: `artifact_schema`,
`ssa_schema_relocation`, `ssa_exec_ir_artifact_v6_write`, and
`ssa_exec_ir_artifact_v6_roundtrip`.

Root independently built the current source with MSVC in
`D:\tmp\zr_vm\ssa-artifact-v6-msvc`; the build exited 0 and the same four
CTest gates passed 4/4.

The write/read tests preserve both Oracle results through direct decoding and
canonical artifact opening. The writer rejects mode 2 at payload byte offset
800, a non-i64 Compare input at value-record offset 276, and a non-BOOL Compare
result at value-record offset 324. Rehashed direct reads reject the same mode
and type mutations with `INVALID_SECTION` at those offsets and leave the output
graph empty. The input mutation changes the first constant and its value token
to BOOL so the reader reaches Compare operand validation; the diagnostic
reports value-record offset 276. The canonical opener rejects these mutations
in `EXEC_IR_BUNDLE` at absolute offsets `payloadOffset + 800`,
`payloadOffset + 276`, and `payloadOffset + 324`.

The same gates retain the EIS1 byte-for-byte golden, EIS2-EIS4 payload and
rejection regressions, existing EIS5 i64 and BOOL predicates, and the EIS5
996-byte count-collision route.

## Acceptance boundary

This accepts only i64 LT Compare with a BOOL result in the existing bounded
EIS5 scalar CFG. It does not add other Compare modes, other scalar operations,
general ExecIR persistence, maps, bindings, relocation resolution, ExecBC,
package copy, AOT projection, native callable ABI, or completion of the full
08.01 milestone.
