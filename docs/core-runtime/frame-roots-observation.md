---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
doc_type: runtime-contract
status: implemented
---

# ExecIR frame roots and observation

The parser-owned frame-root adapter projects packed logical values to physical
frame offsets. Only explicitly classified managed, derived, or inline-field
entries are visited; uninitialized entries expose a null address and scalar
slots are never scanned by accident. Derived entries retain their base slot and
offset metadata for moving collectors and stack relocation.

`ZrParser_ExecIr_ObserveFrame` provides a bounded materialization/writeback
boundary for debugger and deoptimization consumers. It validates the descriptor
before reading storage, copies scalar payloads into the frame when supplied,
returns logical values, and records physical slots whose optimization facts must
be invalidated. Missing precise metadata is an error, not a whole-frame or
whole-heap scan fallback. Runtime GC integration and native pin lifetimes remain
owned by later core adapters.
