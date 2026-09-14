---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_send_sync.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_send_sync.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_send_sync.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_send_sync.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/03-domain-sharing.md
  - user: 2026-09-12 SSA domain sharing implementation
tests:
  - tests/core/test_ssa_domain_sharing.c
doc_type: module-detail
---

# Domain Worker Sharing

## Purpose

This module makes cross-worker sharing explicit. Parser ExecIR analysis emits
separate Send and Sync proofs; runtime admission requires those proofs together
with same-domain membership and a live object, then registers a GC root handle.

## Behavior Model

`SHARED` and GC-managed values are conservatively considered Send and Sync.
`UNIQUE` values are Send-only, so they may be handed off but cannot be shared by
reference. Borrowed and unknown ownership reject both proofs and preserve the
first blocking value identity for diagnostics. Runtime sharing rejects foreign
domains, missing proof, dropping objects, and failed root registration.

## Design and Rationale

Domain identity is necessary but not sufficient for data-race safety. The core
API therefore never infers type properties from an object name; callers must
provide the compiler proof. A successful admission creates a normal domain root
handle, ensuring the consumer is covered by GC snapshots and relocation.

## Test Coverage

`test_ssa_domain_sharing.c` checks immutable/shared values, Send-only unique
ownership, and borrowed-alias diagnostics. Runtime admission reuses the existing
root-handle and mutator tests; registration in the project CTest file remains an
integration task.

## Open Issues

Native thread-affine metadata and field-level mutable-lock proofs are not yet
represented in ExecIR value flags; unknown ownership remains conservatively
rejected until those producers exist.
