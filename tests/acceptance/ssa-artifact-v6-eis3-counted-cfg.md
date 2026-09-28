# SSA 08.01 EIS3 counted conditional CFG

## Scope

This follow-up adds EIS3, a fixed 996-byte canonical ExecIR payload inside the
existing ZRAF v6 `EXEC_IR_BUNDLE` / ERI1 v1 envelope at AOT ABI 17. Its single
supported graph has one no-argument i64 function, three constants and values,
an entry conditional branch to two return blocks, and explicit result,
operand, successor, and predecessor pools. EIS3 is a fixed graph shape; this
does not add general CFG serialization, maps, binding, relocation resolution,
ExecBC, package copy, native AOT calling, or `ImportByPath` migration.

EIS1's exact 412-byte golden and EIS2's 564-byte payload remain unchanged.
ZRAF v6, ERI1 v1, and AOT ABI 17 are unchanged.

## Wire layout

| Offset | Bytes | Fields |
| --- | ---: | --- |
| 0 | 12 | magic `EIS3`, version 3, zero reserved field, 32-bit total length 996 |
| 12 | 80 | module id/token/hash and execution contract |
| 92 | 88 | function id/token/signature, entry/sealed state, execution contract |
| 180 | 32 | counts: constants, values, blocks, instructions, results, operands, successors, predecessors |
| 212 | 48 | three 16-byte constants |
| 260 | 72 | three 24-byte values |
| 332 | 120 | three 40-byte block records |
| 452 | 504 | six 84-byte instruction records |
| 956 | 40 | three result IDs, three operand IDs, two successor IDs, two predecessor IDs |

The entry block successor range begins at byte 356, with its count at 360.
The ordered successor IDs begin at 980; reciprocal predecessor IDs begin at
988. The reader checks declared length, version, reserved bits, and fixed count
limits before allocating a temporary graph. It validates exact edges, verifies
the complete graph, and only then transfers ownership to the caller.

## RED and implementation

The current-source RED used the existing focused GCC cache. The registered
write test failed with the expected first missing capability:

```text
FAIL: EIS3 counted CFG exact 996-byte payload size
```

The EIS3 codec lives in `zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis3.c`;
the existing scalar codec dispatches by magic/shape and retains the EIS1 and
EIS2 encoders and decoders. The canonical writer's bounded payload buffer and
the opener's nested payload length allowlist now include 996 bytes. On an
initial green attempt, the new shape predicate incorrectly expected the last
return block's empty successor range to start after the global successor pool.
GDB showed the canonical fixture and decoded graph both use the zero empty
range `{0, 0}`; the predicate was corrected accordingly.

## Tests and results

GCC 11.4 Debug/Ninja target build in `D:\\tmp\\zr_vm\\ssa-artifact-v6-gcc`
passed. The focused registered CTests passed 2/2 on 2026-09-28:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  --target zr_vm_ssa_exec_ir_artifact_v6_test -j4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^ssa_exec_ir_artifact_v6_(write|roundtrip)$' \
  --output-on-failure --no-tests=error
```

- `ssa_exec_ir_artifact_v6_write` passed, including direct 996-byte codec
  checks, unsupported binding/source-map/relocation rejection, and retention
  of an existing file after invalid rewrites.
- `ssa_exec_ir_artifact_v6_roundtrip` passed in the separate reader process;
  both conditions execute through Oracle and return 42 or 7 respectively.
- Malformed declared length, oversized block count, entry successor count,
  successor/predecessor IDs, and unsupported version are rejected without
  publishing a graph. An unhashed edge edit fails ERI1 hash validation; the
  same edit with recomputed hash reports the precise EIS3 payload byte offset
  and the enclosing opener reports its absolute artifact offset.
- The same test pair also passed existing EIS1 byte-for-byte golden, EIS2
  encode/decode/Oracle and malformed-edge checks.

The final incremental build compiled the EIS3 implementation and strengthened
fixture without warnings. `git diff --check` passed before staging. This
acceptance covers GCC only; Clang and MSVC were not rerun for this follow-up.

## Acceptance decision

The fixed EIS3 three-block conditional fork is accepted by the focused GCC
write/read gates above. General CFGs and the remaining 08.01 artifact features
remain out of scope.
