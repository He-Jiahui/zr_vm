# SSA 08.01 EIS4 fixed scalar ADD

## Scope

This leaf adds EIS4, an exact 716-byte ExecIR payload inside the existing ZRAF
v6 `EXEC_IR_BUNDLE` / ERI1 v1 envelope at AOT ABI 17. The supported graph is
one no-argument i64 function with constants 20 and 22, three values, one entry
block, and four instructions in order: CONSTANT, CONSTANT, ADD, RETURN. The
result and operand pools each hold three value IDs; predecessor and successor
counts are zero. The codec rejects other literal bits and graph shapes.

EIS1's 412-byte golden, EIS2's 564-byte branch payload, and EIS3's 996-byte
conditional CFG payload remain unchanged. ZRAF schema 6, ERI1 v1, and AOT ABI
17 are unchanged. This leaf does not implement general arithmetic or CFG
serialization, maps, bindings, relocation resolution, ExecBC, native AOT
calling, package copy, or `ImportByPath` migration.

## Wire layout

| Offset | Bytes | Fields |
| --- | ---: | --- |
| 0 | 12 | magic `EIS4`, version 4, zero reserved field, 32-bit total length 716 |
| 12 | 80 | module identity and execution contract |
| 92 | 88 | function identity, entry/sealed state, execution contract |
| 180 | 32 | counts for constants, values, blocks, instructions, results, operands, successors, predecessors |
| 212 | 32 | two 16-byte i64 constants, fixed to 20 and 22 |
| 244 | 72 | three 24-byte values |
| 316 | 40 | one 40-byte block |
| 356 | 336 | four 84-byte instructions |
| 692 | 12 | three result IDs |
| 704 | 12 | three operand IDs |

The ADD instruction begins at byte 524. Its result range begins at 528 and its
operand range at 536. The third result ID is at 700; the operand pool starts
at 704. The decoder validates the exact length, version, reserved field,
counts, literals, opcode, ranges, IDs, and empty edge ranges before building
and verifying a temporary graph. The caller receives ownership only after the
whole graph verifies.

## RED and implementation

The current-source RED ran through the existing `ssa_exec_ir_artifact_v6_write`
test. The fixture itself verified and executed in the Oracle with result 42;
the first missing capability was the exact-size EIS4 codec query:

```text
FAIL: EIS4 scalar ADD exact 716-byte payload size
```

The new codec is isolated in
`zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis4.c`, with private entry
points in `artifact_exec_ir_scalar_eis4.h`. The existing scalar dispatcher
selects EIS4 by exact graph shape and magic. The canonical opener accepts 716
bytes in its payload-length allowlist. Encoding writes to a temporary buffer;
decoding checks bounded counts and wire shape before reconstructing a
temporary module, verifies it, and publishes only on success.

## Tests and results

The focused GCC 11.4 Debug/Ninja target build in
`D:\tmp\zr_vm\ssa-artifact-v6-gcc` passed. Both existing registered CTests
passed after the final literal-boundary fix on 2026-09-28:

```bash
cmake --build /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  --target zr_vm_ssa_exec_ir_artifact_v6_test -j4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc \
  -R '^ssa_exec_ir_artifact_v6_(write|roundtrip)$' \
  --output-on-failure --no-tests=error
```

- `ssa_exec_ir_artifact_v6_write` passed. It checks the exact 716-byte header,
  direct encode/decode, literal and binding rejection, and transactional
  preservation of the output buffer and existing artifact.
- `ssa_exec_ir_artifact_v6_roundtrip` passed in the separate reader process.
  The opened graph verifies and the ExecIR Oracle returns 42.
- Malformed physical length, declared length, oversized counts, reserved field,
  unsupported version, empty-edge ranges, ADD opcode/range, result IDs, and
  rehashed literal mutation are rejected without publishing a graph. An edit
  without recomputing ERI1's payload hash fails hash validation; rehashed
  malformed fields report the corresponding EIS4 payload offset through the
  outer artifact diagnostic.
- The same test pair also exercises the existing EIS1 byte-for-byte golden,
  EIS2 encode/decode and branch checks, and EIS3 true/false CFG Oracle results
  of 42 and 7.

This acceptance records the focused GCC gates only; Clang and MSVC were not
rerun for this leaf.

## Acceptance decision

The fixed EIS4 CONST 20 + CONST 22 + ADD + RETURN graph is accepted by the
focused GCC write/read gates above. General arithmetic and the remaining 08.01
artifact features remain out of scope.
