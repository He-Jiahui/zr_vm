---
related_code:
  - zr_vm_core/include/zr_vm_core/contiguous_view.h
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
tests:
  - tests/core/test_ssa_arrays_slices.c
  - tests/acceptance/ssa-arrays-slices-view-boundaries.md
doc_type: runtime-contract
status: partial
---

# Contiguous storage and slice contract

`SZrContiguousView` is a copyable metadata descriptor: it records an owner
identity, byte offset, element count, stride, element size/layout hash, storage
generation, and lifetime region. It does not own the owner or retain an element
address. The caller must keep the owner alive and resolve a fresh address after
a safepoint or storage move.

`ZrCore_View_Validate` checks descriptor fields and the last element's byte
extent only when `length > 0`. A zero-length view has no element extent: a
representable end offset, including `SIZE_MAX`, is valid, but no index into it
is valid. A caller-supplied nonzero current generation must match the recorded
generation. A pinned view requires a nonzero lifetime region. Read-only,
inline-storage, and pinned capabilities are explicit flags.

`ZrCore_View_Slice` first checks that `start` and `length` select a window inside
the parent, then checks `start * stride` and the byte-offset addition for
overflow. It validates a local child descriptor before publishing it. A
successful tail slice with `start == parent.length` and `length == 0` therefore
remains valid; a failed call leaves the destination descriptor unchanged.
`ZrCore_View_IndexOffset`, also used by the parser's current index-lowering
adapter, checks the signed index before computing a byte offset. It leaves the
output offset unchanged on failure.

| Condition | Diagnostic |
| --- | --- |
| Invalid descriptor or missing destination pointer | `INVALID` |
| Negative or out-of-range index; slice window outside the parent | `BOUNDS` |
| Checked byte-offset multiplication/addition overflow | `OVERFLOW` |
| Supplied current generation differs; pinned region is missing | `GENERATION`; `LIFETIME`, respectively |

This is still a metadata contract. The core helpers do not verify the backing
allocation, GC root, layout hash, write permission, or actual pin state.
`Slice` and `IndexOffset` pass generation zero to their internal validation,
so an adapter must check the current storage generation and those storage
properties before real access. Existing container and FFI views have not yet
been migrated to a shared core storage adapter, and this test does not establish
real bounds-check elimination or backend behavior.

For a valid nonempty descriptor, validation has already proved that its last
element fits, so every in-range index has a representable byte offset. The
`IndexOffset` arithmetic overflow branch remains defensive; the reachable
diagnostic distinction here is an out-of-range index reporting `BOUNDS` rather
than `OVERFLOW`.
