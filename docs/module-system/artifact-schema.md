---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis3.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis4.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_internal.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_parser/include/zr_vm_parser/artifact_exec_ir.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis3.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis4.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_read.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis5_write.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/01-schema-relocation.md
  - user: 2026-09-27 first persistent canonical ExecIR slice
  - user: 2026-09-28 EIS3 counted conditional CFG payload
  - user: 2026-09-28 EIS4 fixed scalar ADD payload
  - user: 2026-09-29 EIS5 dynamic counted scalar CFG payload
  - user: 2026-09-29 EIS5 BOOL predicate extension
  - user: 2026-09-29 EIS5 LT Compare extension
  - user: 2026-09-29 EIS5 six Compare modes extension
  - user: 2026-09-29 EIS5 scalar SUB extension
  - user: 2026-09-29 EIS5 scalar MUL extension
  - user: 2026-09-29 EIS5 scalar DIV extension
  - user: 2026-09-29 EIS5 two-DIV effect-chain extension
tests:
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_exec_ir_artifact_v6_cfg.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_add.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_bool.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_compare.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_div.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_div_chain.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_sub.inc
  - tests/library/test_ssa_exec_ir_artifact_v6_eis5_mul.inc
  - tests/library/test_ssa_schema_relocation.c
  - tests/parser/test_artifact_schema.c
  - tests/acceptance/ssa-artifact-v6-canonical-exec-ir.md
  - tests/acceptance/ssa-artifact-v6-eis3-counted-cfg.md
  - tests/acceptance/ssa-artifact-v6-eis4-scalar-add.md
  - tests/acceptance/ssa-artifact-v6-eis5-counted-cfg.md
  - tests/acceptance/ssa-artifact-v6-eis5-bool-predicate.md
  - tests/acceptance/ssa-artifact-v6-eis5-compare-lt.md
  - tests/acceptance/ssa-artifact-v6-eis5-compare-modes.md
  - tests/acceptance/ssa-artifact-v6-eis5-sub.md
  - tests/acceptance/ssa-artifact-v6-eis5-mul.md
  - tests/acceptance/ssa-artifact-v6-eis5-div.md
  - tests/acceptance/ssa-artifact-v6-eis5-div-chain.md
doc_type: module-detail
status: partial
---

# Canonical ExecIR artifact schema

The current ZRAF container schema is version 6. ZRO section 22,
`EXEC_IR_BUNDLE`, stores a nested ERI1 document containing one `EXEC_IR`
payload. The nested document carries AOT ABI 17, the module hash, and a hash
of the payload. ERI1 remains version 1 because its header and directory wire
layout did not change. EIS1 is the first graph payload version and has a fixed
412 byte little endian layout. It encodes IDs, metadata tokens, contracts,
source IDs, a scalar constant, a value, a block, and complete CONSTANT and
RETURN instructions. Runtime pointers and C struct padding are never written.
EIS2 is a separate 564 byte payload version in the same ERI1 envelope. It
encodes exactly two blocks: an entry BRANCH to a CONSTANT then RETURN block.
Its block records include predecessor and successor ranges, dominators, and
terminator IDs; its edge IDs and instruction fields are explicit little
endian integers. EIS3 is a separate 996 byte payload with a 12 byte header,
an explicit 32 bit total length, eight graph counts, three constants, three
values, three block records, six instructions, and counted result, operand,
successor, and predecessor ID pools. It encodes one fixed three-block
conditional fork whose two arms each contain CONSTANT then RETURN. The EIS1,
EIS2, and EIS3 byte sequences stay unchanged. EIS4 is a separate 716 byte payload for
exactly two i64 constants (20 and 22), three values, one block, and four
instructions: CONSTANT, CONSTANT, ADD, RETURN. Its result and operand pools
each contain three IDs; predecessor and successor counts are zero. EIS4 is a
fixed scalar ADD shape, not a general instruction or CFG format.

| EIS2 byte offsets | Encoded fields |
| --- | --- |
| 0–7 | `EIS2` magic, payload version 2, total length 564 |
| 8–215 | module/function identity and contracts, constant, value |
| 216–295 | two 40 byte block records |
| 296–547 | three 84 byte instructions |
| 548–563 | result ID, operand ID, successor ID, predecessor ID |

The successor ID starts at byte 556 and the predecessor ID at byte 560.

| EIS3 byte offsets | Encoded fields |
| --- | --- |
| 0–11 | `EIS3` magic, version 3, zero reserved field, 32 bit total length 996 |
| 12–91 | module identity and execution contract |
| 92–179 | function identity, entry/sealed state, execution contract |
| 180–211 | counts for constants, values, blocks, instructions, result/operand/successor/predecessor IDs |
| 212–259 | three 16 byte constants |
| 260–331 | three 24 byte values |
| 332–451 | three 40 byte block records |
| 452–955 | six 84 byte instruction records |
| 956–995 | three results, three operands, two successors, two predecessors |

The entry block's successor range starts at byte 356; its count is at 360.
The successor ID pool starts at byte 980 and the reciprocal predecessor pool
at 988. The reader checks fixed count limits before graph allocation, validates
the exact length/version/reserved fields and all edges, then verifies a
temporary graph before publishing it.

| EIS4 byte offsets | Encoded fields |
| --- | --- |
| 0–11 | `EIS4` magic, version 4, zero reserved field, 32 bit total length 716 |
| 12–91 | module identity and execution contract |
| 92–179 | function identity, entry/sealed state, execution contract |
| 180–211 | counts for constants, values, blocks, instructions, result/operand/successor/predecessor IDs |
| 212–243 | two 16 byte constants, fixed to i64 20 and 22 |
| 244–315 | three 24 byte values |
| 316–355 | one 40 byte block record |
| 356–691 | four 84 byte instruction records |
| 692–715 | three result IDs and three operand IDs |

The EIS4 reader checks fixed counts, literal values, instruction ranges, and
the complete one-block shape before publishing a verified temporary graph.

EIS5 is a variable-length counted CFG payload. Its 44-byte header stores
the magic, version, reserved field, declared total byte length, and counts for
constants, values, blocks, instructions, operands, results, successors, and
predecessors. A 168-byte fixed module/function prefix follows, then variable
arrays of 16-byte constants, 24-byte values, 40-byte blocks, 84-byte
instructions, and 4-byte result, operand, successor, and predecessor IDs. The
four ID pools are serialized in that order. The current five-block fixture is
1260 bytes; this is one fixture size, not a format-wide fixed size. Checked
64-bit sizing caps blocks at 256, the combined constant/value/instruction
count at 4096, the combined four-pool count at 16384, and the encoded payload
at 16 MiB. Its allowlist is one no-argument i64 function using CONSTANT, ADD,
SUB, MUL, DIV, BRANCH, CONDITIONAL_BRANCH, and RETURN, with no unsupported maps
or side tables. A bounded EIS5 extension also accepts COMPARE with six
canonical modes stored in `typeToken`: EQ=0, LT=1, LE=2, GT=3, GE=4, and
NE=5. Each mode takes
two i64 operands and produces one BOOL result. The reader checks caps and exact
computed length before allocating bounded decode arrays, verifies a temporary
module, and publishes only after verification. A single DIV retains the
`MAY_THROW` flag and canonical effect token pair 1→2. The bounded two-DIV
extension being verified accepts at most two DIV instructions only when both
belong to the same straight-line block, with tokens chained 1→2→3. Every other
instruction must have zero flags and effect tokens; this does not define
general effect-chain serialization.

EIS5 constant and value records already carry a `typeToken` and 64-bit value
bits. The BOOL predicate extension keeps the v5 layout and accepts i64 and
BOOL constants/values. BOOL constant bits must be exactly zero or one, and a
CONSTANT result must keep the constant's type. CONDITIONAL_BRANCH accepts
either BOOL or the existing i64 predicate; ADD, SUB, MUL, and DIV
operands/results and RETURN operands remain i64. EIS1–E4 routing and payload
bytes do not change.
An older EIS5 reader rejects BOOL tokens as `INVALID_SECTION`, so this token-set
extension is not forward-readable by older v5 implementations. COMPARE reuses
the instruction `typeToken` for its six canonical modes. Readers predating
COMPARE reject the opcode at the instruction record; the earlier LT-only v5
reader rejects modes other than LT at the mode field. The EIS5 version and
record layout remain unchanged, so artifacts containing these extensions
require a reader with the corresponding support.

`ZrParser_ExecIr_WriteCanonicalZroFile` accepts a validated ZRO metadata
document with seven identity sections and an `SZrExecIrModule`. It supports
exactly one no argument i64 function with the EIS1 one block shape, the EIS2
two block unconditional BRANCH shape, the EIS3 three block conditional fork
shape, the EIS4 fixed scalar ADD shape, or a graph accepted by the bounded EIS5
counted schema, including BOOL predicates, ADD, SUB, MUL, DIV, and all six
Compare modes. Fixed
EIS1–EIS4 candidates retain their original codec priority;
a graph matching a fixed-format count tuple, per-block instruction ranges, and
opcode sequence stays routed to its fixed validator. This preserves rejection
of malformed legacy literals and bindings while allowing valid same-count EIS5
graphs with different instruction layouts. Every unsupported graph side
table, map, binding, relocation, additional function, or opcode is rejected.
Encoding and validation finish before a file is opened. The writer stages the
counted payload in a bounded heap buffer, creates an exclusive temporary file
in the target directory, closes it, and publishes it by same directory rename;
a failed write leaves the previous target bytes intact.

The zero parameter count is an explicit contract row field, and the i64
return is checked in the decoded graph. Neither the signature token nor its
hash is interpreted as a native calling convention. This slice executes
through the ExecIR Oracle; native callable ABI lowering belongs to 07.02.

`ZrCore_Module_OpenExecIrArtifact` is the dedicated ZRAF entry. It requires
the caller's expected public identity, validates the outer ZRO and each
required section, checks the nested ABI and hashes, accepts exact legacy EIS1–
EIS4 lengths or an EIS5 length within the 16 MiB cap, decodes into a temporary
model, runs `ZrCore_ExecIr_VerifyModule`, compares the decoded module/function
contract with the outer metadata, then publishes the graph. EIS5 validates
its magic, version, counts, reserved field, and exact computed length before
allocation. The focused write and cross-process tests preserve the EIS1
golden, fixed EIS2–EIS4 payloads, the EIS5 i64-predicate and BOOL-predicate
graphs, the SUB, MUL, and DIV results, and true/false branches for all six
Compare modes.
Direct and rehashed outer-payload tests reject BOOL bits outside zero/one and
mismatched constant/result types. SUB and MUL tests reject non-i64
inputs/results. SUB and MUL reject ARITHMETIC as an unsupported opcode through
direct codec reads and the canonical opener, with no partial graph publication.
Compare tests reject mode 6, non-i64 inputs, and non-BOOL results through
direct codec reads and the canonical opener, with no partial graph publication.
EIS2's successor and reciprocal predecessor must name the two serialized
blocks exactly. EIS3's ordered successors and reciprocal predecessors must
match all three blocks; invalid counts or edges are rejected with their
payload byte offsets and without publishing a graph. EIS4 pins both literal
bits, the ADD opcode and operand/result ranges, and the empty edge ranges;
rehash mutations are rejected at their payload offset.
EIS5's ordered edge pools and reciprocal predecessor references are checked;
rehashing an invalid or nonreciprocal edge still fails without publishing a
graph. Compare uses the existing v5 instruction record for EQ=0, LT=1, LE=2,
GT=3, GE=4, or NE=5; it requires two i64 operands and one BOOL result. All
failed reads leave the caller's empty graph empty. The six-mode true/false
matrix, mode-6 rejection at the mode field, and operand/result type checks are
recorded in
[the EIS5 Compare modes acceptance](../../tests/acceptance/ssa-artifact-v6-eis5-compare-modes.md).
SUB uses the existing v5 instruction record and requires two i64 operands and
one i64 result. Its 716-byte payload shares EIS4's fixed length but retains
EIS5 magic and dynamic-schema routing; see
[the EIS5 SUB acceptance](../../tests/acceptance/ssa-artifact-v6-eis5-sub.md).
Readers predating SUB reject the opcode at the instruction record.
MUL uses the same counted v5 instruction record and i64 operand/result guard;
its 716-byte payload also shares EIS4's fixed length and retains EIS5 magic.
The MUL acceptance records its signed `6 * -7 = -42` result and ARITHMETIC
rejection.
Readers predating MUL reject its opcode at the instruction record.
DIV reuses the EIS5 v5 opcode, flags, and effect-token fields without changing
the header or record width. Its tested graph computes `84 / -7 = -12`; the
Oracle rejects a zero divisor and `INT64_MIN / -1` before performing C
division. The writer and reader accept only `MAY_THROW` with effect IDs 1→2 on
DIV, reject non-i64 operands/results and malformed effect fields, and continue
to reject ARITHMETIC. Earlier EIS5 v5 readers reject DIV at the instruction
record. See the
[EIS5 DIV acceptance](../../tests/acceptance/ssa-artifact-v6-eis5-div.md).
The two-DIV follow-on keeps the same v5 record layout and limits the chain to
one straight-line block. The independent MSVC and Clang target builds and the
`artifact_schema`, `ssa_schema_relocation`,
`ssa_exec_ir_artifact_v6_write`, and `ssa_exec_ir_artifact_v6_roundtrip` CTests
pass. The Clang build completed 359/359 steps after VerifyGlobs and automatic
CMake regeneration completed normally. No backup harness was used; its empty
scratch directory was removed. The exact commands, test timings, and RED are
recorded in [the EIS5 DIV-chain acceptance](../../tests/acceptance/ssa-artifact-v6-eis5-div-chain.md).
Readers that predate DIV reject its opcode. Readers that support only the
single 1→2 form reject the second DIV at its effect input field. This bounded
follow-on does not accept a chain across blocks or more than two DIVs.
A schema 5 ZRAF is rejected with
`UNSUPPORTED_VERSION` and diagnostic expected/actual versions.
The separate historical `01ZR` `.zro` binary path remains handled by
`ZrCore_Module_ImportByPath`; this API rejects its magic.

## Legacy ExecBC patch 44

The historical `01ZR` `.zro` writer now emits patch 44 after appending
`MARK_CLOSE_PROXY` at ExecBC opcode 245. Its reader rejects patches above its
compiled maximum before loading instruction arrays; this prevents a patch 43
runtime from dispatching the unknown opcode. Patch 44 adds no fields, so the
new reader continues to accept a patch 43 payload, while it rejects patch 45.
The legacy writer has no safe opcode scan, so it writes patch 44 even for an
opcode-free function; such newly written files require a patch 44 reader.
Existing opcode numbers through 244 are unchanged. ZRAF schema 6, EIS1–EIS4
payloads, ERI1 v1, and AOT ABI 17 stay unchanged; EIS5 adds a counted ExecIR
payload version. BOOL predicates and all six canonical Compare modes reuse
EIS5 v5 typed fields without changing payload length or header. Readers
predating Compare reject its opcode; the earlier LT-only v5 reader rejects
modes other than LT. See the bounded
[EIS5 Compare modes acceptance](../../tests/acceptance/ssa-artifact-v6-eis5-compare-modes.md).
These serialized payloads do not carry ExecBC opcode numbers. A generated AOT
module using the proxy still requires the newly exported runtime helper when
linked.

This remains a partial vertical slice of plan 08.01. It does not provide general CFG,
maps, binding, relocation resolution, ExecBC, package copy, AOT projection,
or `ImportByPath` migration. The legacy ERI1 raw section codec and relocation
unit test remain available for their existing callers; accepting raw bytes
there does not make those bytes an executable canonical graph.
