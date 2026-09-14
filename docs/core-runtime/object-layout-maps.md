---
related_code:
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/object_layout_map.h
  - zr_vm_core/src/zr_vm_core/object/object_layout_map.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_layout_visibility.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_layout_visibility.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/object_layout_map.h
  - zr_vm_core/src/zr_vm_core/object/object_layout_map.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_layout_visibility.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_layout_visibility.c
tests:
  - tests/core/test_ssa_objects_layout_maps.c
plan_sources:
  - docs/plans/ssa/05-data-layout/01-objects-layout-maps.md
doc_type: milestone-detail
status: draft
---

# Object layout maps

The parser visibility pass decides whether a closed-world object may receive an
internal physical layout. Reflection, serialization, FFI, address escape, or
an open-world prototype blocks transformation. Public consumers therefore keep
the canonical logical member order and offsets.

For a private layout, the core `SZrObjectLayoutMap` records descriptor index,
logical offset, physical offset, size, alignment, shape generation, and separate
logical/physical hashes. `ZrCore_Object_ResolveLayoutMember` requires the current
shape generation before returning an offset; stale shape entries are rejected
instead of being used as a fast path. Dynamic members continue through the
existing dictionary/prototype path.

Construction and ownership cleanup remain outside this map contract. A
constructor publishes its final shape only after initialized fields are valid;
failure cleanup must use the initialized-field bitmap. Public reflection or FFI
bridges materialize logical layout and never write internal offsets back into
the public ABI.
