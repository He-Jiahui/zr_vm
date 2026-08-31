---
related_code:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/object/object_inline_array.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/object/object_inline_array.c
plan_sources:
  - user: 2026-08-30 optimize GC layout scanning from the benchmark plan
  - docs/plans/benchmark/optimize/03-memory-object-gc.md
tests:
  - tests/core/test_inline_struct_array_layout.c
  - tests/acceptance/2026-08-30-gc-layout-scan-fast-path.md
doc_type: module-detail
---

# GC layout scan fast path

`ZrCore_TypeLayout_CanSkipGcScan` is a fail-closed descriptor predicate. It
returns true only after full layout validation and only for a structure whose
GC scan kind is `FREE`, whose GC/ownership/ref maps are empty, and whose fields
contain no nested layout or managed-value flags. Value layouts are excluded:
their storage is a `SZrTypeValue` even though they do not expose a field map.

`ZrCore_Object_VisitInlineArrayGcValues` resolves and validates the recorded
layout and checks the complete recorded element range before applying this
predicate. A proven non-GC inline array then returns without visiting each
element. Unknown, stale, malformed, or otherwise non-provable layouts retain
the existing registry/layout visitor path and return failure when that path
cannot resolve the descriptor. No write barrier, ownership operation, or drop
operation is bypassed by this optimization.

The predicate is intentionally conservative around nested layouts. A parent
descriptor with no direct map entries can still contain a nested descriptor
that owns or traces values, so it does not qualify unless all fields are plain
non-managed storage.
