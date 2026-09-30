---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_place_uses.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_internal.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_execbc_vm.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_place_uses.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_internal.h
plan_sources:
  - user: 2026-09-30 dead metadata-free PLACE_BASE VM slice
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_execbc_vm_dead_place.c
  - tests/parser/test_ssa_execbc_vm_dead_place_support.inc
  - tests/acceptance/ssa-execbc-vm-materialization.md
doc_type: module-detail
---

# Dead metadata-free PLACE_BASE in the ExecBC VM materializer

## Purpose

The initial ExecBC-to-Core VM slice has no address or memory ABI. A projection
can still carry a `PLACE_BASE` row as provenance metadata when the row is
unobservable: its scalar result is never read by an instruction, phi incoming,
or copy schedule. This module admits only that dead form and emits one Core
`NOP` for it, preserving the source instruction, block, and PC-map identity.

## Validation boundary

The existing projection-wide rejection of frame layouts, GC maps, deopt/state
maps, memory tokens, and nonzero instruction memory/effect fields remains in
force. The dead-place pass runs after the basic projection pointers are known
and scans every instruction operand and every phi incoming value. It also
checks phi-copy value IDs and phi result IDs before allowing a place value.
The embedded `SZrExecIrGcMap` record is rejected field by field, even when
`gcMapPresent` is false, so stale pointer/count metadata cannot bypass the
metadata-free boundary.

Each dead `PLACE_BASE` must have one scalar operand and one scalar result,
zero auxiliary ranges, zero instruction metadata, no layout or match token,
and a result whose definition and type agree with the row. The result may
carry `PLACE_ADDRESS` and `PROMOTABLE_PLACE` (with the existing promotable
implies address invariant), but its total use count must be zero. Live place
results, `PLACE_PROJECT`, `LOAD`, and `STORE` remain unsupported.

An `EXTERNAL_ENTRY` value is accepted only when it has at least one use and
every use is operand zero of a proven dead `PLACE_BASE`. It cannot carry place
flags, appear in a copy, return, phi incoming, or phi-copy source, or have an
ordinary instruction or phi-result definition. Unknown value flags and an
uninitialized live value are rejected.

## Emission and ownership

The validator establishes the projection-wide proof before plan construction.
The materializer's normal plan append path then lowers each validated
`PLACE_BASE` to Core `NOP`; the existing append operation records its original
instruction ID, source ID, block ID, and PC map row. No place result is read by
the emitted VM function, so the NOP cannot expose an uninitialized frame slot.

## Test coverage

`test_ssa_execbc_vm_dead_place.c` is an independent Unity target. Its positive
fixture shares one external i64 provenance value across two dead i64 bases and
uses a bool dead base as well. It compares the Oracle return and place trace
with the real emitted Core VM return and NOP/source map trace. Negative cases
cover a live place result, an external value mixed into `COPY`, phi incoming
use, malformed place metadata/result shape, place flags on a scalar constant,
unknown and mixed flags, an external value with a local instruction result,
nonzero memory/effect metadata, a memory-token pool, and embedded GC-map fields
with `gcMapPresent` clear.

The root-owned MSVC build and `ssa_execbc_vm_dead_place` CTest passed all 13
cases. The fresh run is recorded in
`D:/tmp/zr_vm/ssa-control/current-scalar-shape-fixtures-ctest.log`; it also
passes the 18-case scalar VM target. Current GCC/Clang and the wider M1
Oracle/ExecBC/AOTIR/C/LLVM matrix remain open. Source literal promotion and
canonical type resolution have their own producer/adapter acceptance and
retain this materializer's metadata guards.
