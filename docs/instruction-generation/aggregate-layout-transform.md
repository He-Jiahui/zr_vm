---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_aggregate_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_materialization_map.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_aggregate_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_materialization_map.c
plan_sources:
  - docs/plans/ssa/05-data-layout/04-aggregate-soa.md
  - user: 2026-09-13 conservative aggregate SROA and AoS/SoA contract
tests:
  - tests/parser/test_ssa_aggregate_soa.c
doc_type: module-detail
---

# Aggregate layout transformation contract

## Purpose

The parser produces a pointer-free, scalar witness for aggregate layout
optimisation.  The witness supports local scalar replacement of aggregates
(SROA), a closed-lifetime AoS/SoA candidate, and a logical-to-physical
materialisation map.  It is deliberately a planning contract: it does not
rewrite an `SZrExecIrFunction`, allocate storage, or retain a runtime address.
The eventual lowering/backend can reject a stale plan and fall back to the
ordinary boxed/AoS representation.

## Facts and safety gates

`SZrExecIrAggregateFacts` records logical type/layout identity, generation,
source/IR coordinates, field offsets and sizes, initialisation state,
ownership, root slots, drop order, and alias evidence.  Boolean visibility and
escape facts are independent so a producer cannot accidentally infer private
layout from a missing field.  Unknown flags, overlapping fields, invalid
alignment/size, stale schema, unknown escape, address observation, reflection,
FFI, serialization, public slices, unions/overlays, and non-reconstructible
identity all block SROA or SoA.  A ref field retained by native code is treated
as address observation.  Unique/shared/GC fields require an explicit proven
ownership transfer; GC-root fields require a root slot.  Drop order is copied
by logical field index and is never inferred from physical column order.

Initialisation is tracked per field.  A field that may be read before
initialisation is rejected, but an as-yet-uninitialised field that is not read
can still be scalarized and remains clear in the candidate's bit mask.  A
producer that zero-initialises a field can mark it `initialized` without a
separate aggregate-wide bit.

## SROA candidate

`ZrParser_ExecIr_SroaBuildCandidate` emits one scalar slot per known field only
when all local gates pass.  The candidate carries initialized, ownership, root,
and drop masks plus both directions of the field/slot map.  Its hash covers the
logical identity, generation, bridge version, maps, and masks.  Validation
rechecks the source facts and hash before a backend consumes it.

## AoS/SoA candidate and evidence

`ZrParser_ExecIr_DataLayoutCanUseSoA` requires a private closed world, a closed
container lifetime, no unknown escape or address observation, reconstructible
identity, alias evidence where aliasing is observed, and a profile/cost record.
The bridge estimate must be strictly smaller than the locality estimate.  A
failed gate is not an error for `BuildCandidate`: it returns an ordinary AoS
candidate with the precise `fallbackReason`, preserving a safe generic path.

Cost evidence is tagged as `ESTIMATED` or `MEASURED`.  Estimated move/bridge /
locality values and PMU cache-miss counters are stored in separate fields;
unknown measured counters use `UINT64_MAX`.  No static estimate is presented
as a measured cache miss reduction.

Physical field order is a deterministic descending loop-use-density order (ties
use logical index).  The physical layout hash includes logical layout hash,
strategy, field order, field sizes, and bridge version.  Ownership, root, and
drop metadata are marked as migrated on every candidate and validated before
materialisation.

## Materialisation and identity

`SZrExecIrMaterializationMap` maps each logical field to a physical index and
offset while retaining type/layout, initialisation, ownership, root slot, drop
order, and alias class.  `Begin` checks generation and logical layout identity;
`AddField` rejects duplicate logical or physical indices; `Finalize` requires a
complete one-to-one map and computes a stable map hash.  `Validate` checks the
candidate, generation, logical/physical hashes, every field's semantic
metadata, and the identity token.  Thus two deoptimised references carrying the
same identity token can rebuild one logical object and preserve their alias
relationship.  A mismatched token or generation is rejected rather than
silently cloning an object.

The map is also suitable for debug, deopt, exception, and native boundaries:
those boundaries can request logical materialisation while the optimised
physical columns remain private.  This implementation only builds and checks
the map; actual object allocation and attachment to an ExecIR state map remain
backend/runtime work.

## High-level planning

`ZrParser_ExecIr_PlanAggregateLayout` first attempts SROA and then a profitable
SoA candidate.  If proof is incomplete it emits a `GENERIC` plan with a
diagnostic status as `fallbackReason`; no user annotation is required to force
an unsafe transformation.  `ZrParser_ExecIr_ApplyAggregateLayout` is a
pointer-free validation/no-op boundary for this milestone.  It accepts generic
and AoS fallback plans and validates SROA/SoA identity before a future lowering
stage performs the physical rewrite.

## Test coverage

`tests/parser/test_ssa_aggregate_soa.c` exercises:

- two-field private SROA and per-field initialisation bits;
- union/overlay, uninitialised-read, address/native-retained, unknown-escape,
  public, FFI, and unsafe-ownership rejection diagnostics;
- profitable closed-lifetime SoA versus AoS fallback and separate measured
  evidence fields;
- identity-preserving materialisation with root/ownership/drop metadata;
- conservative generic planning when escape proof is unavailable.

The focused fixture is intentionally standalone until the owning CMake target
is registered by the integration owner.  It has no heap ownership or lease to
balance: all plans are fixed-size values and can be discarded on OOM,
cancellation, repeat calls, or partial construction without a release hook.

## Out of scope

This contract does not change public object layouts, split public slices, invoke
native callbacks, perform GC scanning, or emit machine code.  Runtime storage
allocation, real PMU collection, ExecIR instruction rewriting, and CTest/CMake
registration are integration tasks for the parent milestone.
