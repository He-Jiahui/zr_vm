---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_frame_layout.h
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
plan_sources:
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/core/test_ssa_roots_observation.c
  - tests/acceptance/ssa-core-observation-reused-slot.md
doc_type: module-detail
---

# Core frame observation and reused scalar storage

`ZrCore_Execution_ObserveFrame` consumes an already validated packed layout.
Layouts can assign different logical slots to the same physical storage when
their half-open live ranges do not overlap. The observation request has no
instruction position or active-occupant selector. Consequently, different
logical scalar inputs cannot be resolved by choosing the last layout entry.

## Write contract

With `WRITE_SCALARS`, each scalar entry requests exactly `slot.byteSize` bytes
from the native byte representation of its corresponding `TZrUInt64` input.
Scalar sizes larger than that representation are rejected by the existing
bounds check. Before modifying the frame or any output, the observer checks
each pair of scalar writes whose byte intervals overlap. The bytes requested
for the overlap must be identical. Conflicting bytes return `SLOT_OVERLAP`;
`index` identifies the later layout entry and `relatedIndex` the earlier one.

The check uses actual byte intervals, including overlaps between distinct
physical-slot identifiers that the current layout validator accepts. It does
not infer aliases solely from physical identifiers. Bytes outside the declared
scalar size do not participate in the comparison. This respects native byte
order without interpreting unused high bits as another value.

Compatible requests retain the existing materialization and writeback order.
Each physical identifier appears once in invalidation output. Read-only
observation remains legal for reused storage and returns the current backing
bytes for each logical view; it does not reconstruct historical values.

## Failure boundary and ownership

Layout, capacity, bounds, and scalar-conflict validation precede every frame,
writeback, invalidated-slot, and invalidated-count write. A conflict therefore
preserves all four caller-owned outputs, including unrelated destinations
earlier in layout order. Diagnostics are the sole modified output on failure.
The precheck allocates no memory and owns no resources. The caller must keep
layout, frame, inputs, and outputs valid for the duration of the call.

This change does not select active occupants, modify the frame-layout ABI, or
complete the broader roots/observation milestone. Detailed execution evidence
and remaining validation limits are recorded in the linked acceptance report.
