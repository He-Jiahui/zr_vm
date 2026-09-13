---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
doc_type: runtime-contract
status: implemented
---

# Packed ExecIR frame layout

`ZrParser_ExecIr_LayoutPackedFrame` is the parser-owned adapter that produces a
pointer-free `SZrExecIrFrameLayout` for later ExecBC, native and AOT consumers.
It keeps logical value IDs separate from physical storage slots, classifies each
value as boxed, scalar, inline span or reference, and reuses a physical slot only
when class, size/alignment and half-open live ranges permit it. Address-escaped
or parameter values are pinned and never reused.

All alignment, size and return-buffer calculations are checked before the output
is committed. Invalid ranges, non-power-of-two alignment and frame limits return
the existing ExecIR diagnostic codes. Layout hashes include function identity,
logical-to-physical mapping and every physical slot's identity, offset, size,
alignment and type token. The legacy `SZrFunction.frameSlotLayouts` and dense
`SZrTypeValueOnStack` mirror remain unchanged; this descriptor is an additive
migration boundary until the runtime consumers are switched in later 04.x work.
