---
related_code:
  - zr_vm_core/include/zr_vm_core/contiguous_view.h
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
doc_type: runtime-contract
status: implemented
---

# Contiguous storage and slice contract

`SZrContiguousView` is a pointer-stable descriptor: it retains an owner root,
byte offset, length, stride, element size/layout hash, storage generation and
lifetime region. Indexing and slicing use checked arithmetic and reject negative,
out-of-range or overflowing operations before calculating an offset.

Views are invalid when their storage generation changes. Pinned views require a
non-zero lifetime region; read-only, inline-storage and pinned capabilities are
explicit flags. The descriptor stores no long-lived element pointer, so callers
must reacquire an address after a safepoint or resize. Existing container and
FFI implementations can adapt to this contract without changing their backing
storage representation.
