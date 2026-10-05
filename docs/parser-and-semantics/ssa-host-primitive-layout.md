---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - tests/parser/test_ssa_host_primitive_layout.c
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_host_primitive_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_host_primitive_layout.c
plan_sources:
  - docs/plans/ssa/04-frame-native/01-frame-layout.md
  - docs/parser-and-semantics/ssa-dead-source-places.md
tests:
  - tests/parser/test_ssa_host_primitive_layout.c
  - tests/acceptance/ssa-host-primitive-layout.md
  - docs/acceptance/ssa-host-primitive-layout.md
doc_type: module-detail
status: host-primitive-layout-focused-windows-green-accepted
---

# Host Primitive Layout Rows

## Purpose and interface

A later primitive frame producer needs explicit storage facts for the actual
canonical value TypeId. Canonical primitive identity alone does not provide
target byte size or ABI alignment. This finite adapter constructs one
`SZrExecIrLayout` row for the current host's actual canonical primitive i64.
It does not attach a frame or append a row to a module.

```c
TZrBool ZrParser_ExecIr_MakeHostPrimitiveLayout(
        const SZrSemanticContext *context, TZrTypeId typeId,
        TZrUInt32 layoutId, SZrExecIrLayout *output,
        SZrExecIrDiagnostic *diagnostic);
```

The following is the implemented finite contract. The committed unsupported
stub established behavioral RED after actual source prerequisites; the fresh
Windows GREEN passed host 15/15 and compaction 30/30, plus independent 2/2
prerequisites, with no observed UBSan diagnostic.
The [acceptance record](../acceptance/ssa-host-primitive-layout.md) tracks that
evidence without treating static review as a passing run.

## Caller ownership and canonical admission

The caller supplies a TypeId from the same live semantic context and a nonzero
layout ID. All native allocations must be valid. Context, output and optional
diagnostic storage are independent; output cannot overlap context or diagnostic
storage. These are caller preconditions, not arbitrary-pointer validation.
The context is a current interner snapshot with sorted node IDs and actual
readable canonical allocations. Descriptor/address-span checks do not establish
readability. The context is borrowed and remains unchanged. No row allocation occurs.

Before `ZrParser_CanonicalType_Find`, check the canonical array's validity,
element size, length/capacity, live head, capacity multiplication and native
address-span arithmetic. Unknown IDs are invalid ranges. Only an actual
canonical PRIMITIVE node with `ZR_VALUE_TYPE_INT64` is admitted; the actual
FUNCTION callable is unsupported. A zero canonical structural hash is a layout
mismatch. BOOL, unsigned, float, aggregates and reference layouts remain open.

The row uses `sizeof(TZrInt64)` and `alignof(TZrInt64)` from this toolchain's
host storage. `TZrInt64` is the actual C int64_t type; together with the checked
eight-bit bytes this gives eight-byte storage, and its C `alignof` supplies the
valid host alignment. Size/alignment conversion to u32 is checked, without a
separate runtime row-geometry predicate. An actual eight-byte uint64 pattern is
compared against the complete little/big byte sequences; mixed byte order or
non-eight-bit bytes are unsupported. This is an
explicit host adapter; cross-target callers must supply a separate target
layout producer. No pointer-size or default maximum alignment substitutes for
the primitive's real alignment.

## Exact stable row hash

Hash exactly 63 bytes with `ZrCore_Hash_CreateStable64`: the 31 ASCII bytes
`zr.execir.host.primitive-layout` without its terminating NUL, then 32 bytes of
explicit little-endian fields. The fields are serialized individually, with no
struct padding, allocation, prefix API or runtime salt:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 31 | Domain above, excluding NUL |
| 31 | 4 | Schema version 1 |
| 35 | 4 | Actual canonical primitive `valueType` |
| 39 | 8 | Actual canonical node `structuralHash` |
| 47 | 4 | Host i64 byte size |
| 51 | 4 | Host i64 byte alignment |
| 55 | 4 | `CHAR_BIT` |
| 59 | 4 | Actual endianness: 0 little, 1 big |

Local TypeId, layout ID, context address, runtime salt and time are excluded.
The canonical structural hash is included as identity evidence. Reject a zero
computed row hash. Build a private row and publish only after all checks pass;
every failure preserves all output bytes, including padding. Optional diagnostic
storage is cleared on success and reports an explicit failure otherwise.

The current primitive structural-hash producer also excludes local TypeId,
context address, salt and time: `canonical_type_hash_word` encodes a word byte
by byte; `canonical_primitive_hash` uses the fixed canonical hash offset/prime
and only PRIMITIVE kind plus actual valueType. `InternPrimitive` computes this
hash and assigns it to `node.structuralHash`, separately reserving `node.id`.
This static source proof applies only to the current primitive path; it does
not generalize to nominal/generic hashes or future schema/enum changes.

## Module ownership and future consumers

Row `id` is the caller's independent nonzero layout-table identity. `typeToken`
is the actual TypeId in the live context; these identifiers need not be equal.
The fixture checks an initially empty module table, count/capacity and
`layoutCount < UINT32_MAX`, chooses checked `layoutCount + 1`, and actually calls
`ZrCore_ExecIr_ModuleAppendLayout`. The helper itself does not mutate a module.

The existing packed-frame hash does not incorporate this row hash. A future AOT
consumer must retain the module layout table and resolve its rows; a row hash
does not establish source provenance, callable identity, frame geometry,
complete target ABI, executable artifact retention or native execution.
The shared [literal fixture](ssa-dead-source-places.md) supplies actual 9/8 source
identity and context evidence. This adapter does not compact those graphs.

## Test contract and limits

The independent test hash oracle uses arithmetic byte extraction (`% 256`,
`/ 256`) rather than the production encoder, checking the exact 63-byte contract.
Different appended layout IDs and two genuinely different live context
addresses are dynamic exclusion checks. Local TypeIds or salts may coincide in
those contexts; no dynamic claim is made that they differ. Their exclusion is
supported by the exact-byte oracle and source inspection instead.

Array fault and canonical-node hash mutations are restored before any Unity
assertion so teardown sees real owners. Oracle execution results stay in the
caller-owned global teardown owner. The RED built successfully, passed actual
2/2 prerequisites and compaction 30/30, then failed 12 of 15 host cases with the
stub. Final GREEN source pins and logs are recorded in the acceptance document.
The revised unknown-ID guard first proves canonical Find returns null. Linux
GCC/Clang retain the existing WSL service access blocker; MSVC compiled the new
TU only, with C4127 warnings at lines 54 and 106, and exited 0. Full
SSA47, primitive frame publication and AOT/native retention remain **OPEN**.
The pure local tests do not execute network, FFI, providers or security probes.
