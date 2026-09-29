# SSA 08.01 EIS5 scalar MUL

## Scope

This bounded leaf adds one i64 `MUL` opcode to the EIS5 v5 counted scalar CFG.
It uses two i64 operands and produces one i64 result. The header, version,
field order, and record widths stay unchanged. The graph loads `6` and `-7`,
multiplies them, and returns `-42`; ADD would return `-1`, and SUB would return
`13`. Both `ZrCore_ExecIr_VerifyModule` and the Oracle accept the graph and
confirm its signed result before encoding.

The one-block, four-instruction payload is 716 bytes, the same length as the
fixed EIS4 ADD form. Direct codec tests check the EIS5 magic/version and round
trip the graph. The existing write/read phases also write and reopen the
canonical artifact in separate process phases, proving that the opener
dispatches by EIS5 magic when the payload length collides with EIS4.

Writer, direct reader, and canonical opener reject BOOL operands or results
for MUL. They also reject ARITHMETIC as an unsupported counted EIS5 opcode at
the instruction field. Rejected writes preserve the destination bytes, and failed
reads do not publish a partial graph. Rehashed reader mutations assert the
payload-relative opcode/value offsets and the opener's corresponding absolute
offsets in `EXEC_IR_BUNDLE`.

## RED

The test-only fixture passed `VerifyModule`, and the Oracle returned `-42`
before the codec allowlist changed. The old writer failed the existing write
CTest at the expected point:

```text
FAIL: EIS5 MUL dynamic size query shares the EIS4 716-byte tuple
```

This isolated the missing serialization support from graph validity and
arithmetic semantics.

## GREEN verification

The existing Clang build cache was reused at
`D:\tmp\zr_vm\ssa-artifact-v6-clang`. The current-source targets built with
exit code 0:

```powershell
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang `
  --target zr_vm_ssa_exec_ir_artifact_v6_test `
           zr_vm_ssa_schema_relocation_test `
           zr_vm_artifact_schema_test -- -j2
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-clang `
  --output-on-failure `
  -R '^(artifact_schema|ssa_schema_relocation|ssa_exec_ir_artifact_v6_write|ssa_exec_ir_artifact_v6_roundtrip)$'
```

Results: `artifact_schema`, `ssa_schema_relocation`,
`ssa_exec_ir_artifact_v6_write`, and `ssa_exec_ir_artifact_v6_roundtrip` all
passed (4/4). The write phase exercises the legacy goldens and rejection cases,
the EIS5 ADD/SUB/BOOL/COMPARE/counted-CFG cases, and the new MUL checks. The
roundtrip phase reopens both the five-block counted graph and the 716-byte MUL
graph in the separate read phase.

Root independently built the frozen current sources with MSVC in its existing
D cache; the build exited 0 and the same four registered CTests passed (4/4).

## Compatibility and acceptance boundary

EIS1–E4 payloads and rejection paths remain unchanged. MUL reuses the existing
EIS5 v5 instruction record; artifacts containing MUL require a reader that
supports this opcode. Earlier EIS5 v5 readers reject it at the instruction
record. The previous SUB leaf's unsupported-opcode mutation now uses
ARITHMETIC. A later bounded DIV leaf adds only the fixed `MAY_THROW` and
effect-token 1→2 pair; see the separate
[EIS5 DIV acceptance](ssa-artifact-v6-eis5-div.md).

This leaf adds only counted EIS5 i64 MUL support. It does not add other
arithmetic opcodes, alter the v5 wire schema, or complete the full 08.01
artifact/relocation plan. Maps, bindings, relocation resolution, ExecBC,
package copying, and AOT projection remain outside this slice.
