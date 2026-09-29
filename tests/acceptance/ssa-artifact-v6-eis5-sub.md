# SSA 08.01 EIS5 scalar SUB

## Scope

This bounded leaf adds one i64 `SUB` shape to the EIS5 v5 counted scalar CFG:
two i64 operands, one i64 result, then `RETURN`. The header, payload version,
field order, and record widths stay unchanged. The test graph loads -11 and 4,
subtracts the second from the first, and returns -15; ADD with the same inputs
would return -7. `VerifyModule` accepts the graph and the ExecIR Oracle confirms
the signed result before serialization.

The payload is 716 bytes, equal to the existing fixed EIS4 ADD payload length.
The canonical writer must route this graph by its full instruction shape and
emit EIS5 magic/version 5 rather than treating the byte length as EIS4. The
existing write and read phases cover direct codec roundtrip and canonical
artifact write/open across processes.

The writer, direct reader, and canonical opener reject a BOOL SUB input, a
non-i64 SUB result, and, in the original test baseline, an unsupported MUL
opcode. Rejected writes preserve
the destination bytes; rejected reads do not publish a partial graph. The
rehashed reader mutations check payload offsets 244 (input value record), 292
(result value record), and 524 (instruction opcode); canonical opener checks
the corresponding absolute offsets within `EXEC_IR_BUNDLE`.

## RED

The test-only graph first passed `ZrCore_ExecIr_VerifyModule` and the Oracle
returned -15. With the old EIS5 writer, the exact write CTest then failed at:

```text
FAIL: EIS5 SUB dynamic size query shares the EIS4 716-byte tuple
```

This established the missing opcode support without relying on an invalid
fixture or a guessed arithmetic result.

## GREEN verification

The existing Clang build cache was reused at
`D:\tmp\zr_vm\ssa-artifact-v6-clang`. The current-source targets built with
exit code 0, and the focused four CTests passed:

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
passed (4/4). The write and roundtrip tests also passed together after the
final fixture-offset correction.

Root independently built the frozen current sources with MSVC in its existing
D cache; the build exited 0 and the same four registered CTests passed 4/4.

The tests retain the EIS1 byte-for-byte golden, EIS2 through EIS4 payload and
rejection cases, EIS5 i64 and BOOL predicates, six Compare modes, the 996-byte
same-count CFG collision, and existing dynamic count/edge mutations. Earlier
EIS5 v5 readers do not recognize SUB and reject it at the instruction record.
When this slice was first verified, MUL was also outside the EIS5 allowlist,
so its then-current unsupported-opcode mutation used MUL. The later independent
MUL slice adds that opcode; the current SUB fixture now uses DIV for its
unsupported-opcode regression. See the separate
[EIS5 MUL acceptance](ssa-artifact-v6-eis5-mul.md) for the added opcode and
remaining DIV rejection.

## Acceptance boundary

This change adds only the counted EIS5 i64 SUB form. It does not add other
arithmetic opcodes, alter the v5 wire schema, or complete the full 08.01
artifact/relocation plan. Maps, bindings, relocation resolution, ExecBC,
package copying, and AOT projection remain outside this leaf.
