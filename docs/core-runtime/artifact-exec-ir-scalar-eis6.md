---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_exec_ir_scalar.h
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_read.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_write.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_read.c
  - zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_write.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/01-schema-relocation.md
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
tests:
  - tests/parser/test_ssa_binding_rows_artifact.c
  - tests/parser/test_ssa_binding_rows_artifact_capacity.inc
  - tests/library/test_ssa_canonical_zraf_eis6_guard.inc
doc_type: module-detail
---

# EIS6 Typed Scalar Binding Persistence

The public scalar codec chooses EIS6 for typed binding-row schema version 1,
including an explicit empty table. Legacy schema-zero selection and EIS1-5
bytes remain unchanged. EIS6 retains the EIS5 scalar graph and contracts, then
adds instruction memory-token ranges, a memory-token pool, and typed rows.

The little-endian format has a 56-byte header, a 224-byte fixed prefix,
4-byte memory-token IDs and 96-byte rows. Row fields are serialized
individually; no pointer or compiler struct padding is stored. Existing node,
block, pool-item and byte limits apply to the combined payload. Size arithmetic
uses checked 64-bit operations and requires the encoded length to match exactly.

The codec supports the same scalar graph shape plus CALL. It preserves each
CALL row reference, effect IDs and memory ranges. A row is checked against its
instruction before the normal row setter can rewrite associations; conflicting
reciprocal references are rejected. Reserved row capacity is an in-memory
allocation detail, so only the row count and live entries are persisted.
INVOKE, phi, ownership, GC/deopt/frame/state maps, and general source canonical
type tables remain outside this format's supported subset.

Binding persistence does not prove symbol closure. Core permits a typed CALL
without supplied binding facts; EIS6 preserves that empty table. The canonical
ZRAF module and hotpatch loaders reject typed EIS6 after a successful public
decode, until the module symbol-closure consumer has been connected. This
prevents a permissive persistence codec from being treated as closure proof.

Writing validates first and stages bytes in native temporary storage, leaving
the caller's buffer unchanged on failure. Reading requires a fresh empty
output, validates lengths/counts before graph allocation, constructs and
verifies a temporary module, and publishes only after every check succeeds.
Rejection frees staged rows and the whole temporary graph.

The static test links instrumented copies of the three allocation-owning
implementation units. It fails each observed decode allocation in turn,
requires LIMIT, byte-identical empty output and restored allocation balance,
then retries and checks the original binding hash. The tracker allocates no
memory itself. Header, row-field, range, association, truncation, deterministic
encoding, legal capacity and rowless CALL cases run alongside legacy wire
roundtrip/write and canonical-loader rejection tests.
