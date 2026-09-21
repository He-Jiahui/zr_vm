---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_roots.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_roots.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/execution_frame_layout.h
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
doc_type: runtime-contract
status: implemented
---

# ExecIR frame roots and observation

The parser-owned frame-root adapter projects packed logical values to physical
frame offsets. Only explicitly classified managed, derived, or inline-field
entries are visited; uninitialized entries are skipped and scalar slots are
never scanned by accident. Derived entries retain their base slot and offset
metadata for moving collectors and stack relocation. The visitor preflights all
root metadata, then visits managed/inline roots before derived roots regardless
of map order so a moving collector can update bases first. After the derived
callback, the adapter reloads the possibly moved base and checked-recomputes the
derived slot as `base + derivedOffset`; managed/derived storage must be at least
pointer-sized.
Root-map construction and visitation both reject unknown root kinds before an
address is formed or a callback is invoked.
Construction also validates slot count/capacity relationships and required
physical and logical tables before allocating a candidate map; failure leaves
the caller's existing map intact.
Managed and derived roots are accepted only for packed REF/BOXED values, while
inline-field roots require INLINE_SPAN/BOXED storage. Scalar bit patterns are
therefore never promoted to roots solely because they resemble an address.
The base of a derived root must independently be REF/BOXED storage.

`ZrParser_ExecIr_ObserveFrame` provides a bounded materialization/writeback
boundary for debugger and deoptimization consumers. It validates the descriptor
before reading storage, copies supplied payloads only into slots classified as
scalar, returns logical values, and records physical slots whose optimization
facts must be invalidated. Reference and inline storage is never overwritten by
the scalar input channel. Invalidation capacity is preflighted before any output
changes, and reused logical values emit each physical slot only once. Missing
precise metadata is an error, not a whole-frame or whole-heap scan fallback.
Runtime GC integration and native pin lifetimes remain owned by later core
adapters.

Observation rejects malformed slot count/capacity and storage-count
relationships before changing frame bytes, writeback values, or invalidation
state. Unknown packed slot-class enum values fail in the same preflight.

The core adapter exposes the same boundary to runtime-neutral ExecBC, AOT and
JIT consumers through `ZrCore_ExecutionFrameRootMap_Build` and
`ZrCore_Execution_VisitFrameRoots`. Root-map validation checks the layout hash
and physical offsets before a callback can run. Managed roots are visited
before derived roots; after a callback updates a base, the derived address is
recomputed with checked pointer arithmetic. `ZrCore_Execution_ObserveFrame`
preflights every destination, materializes only scalar slots, returns bounded
writeback values, and reports unique physical slots to invalidate. A short
frame or capacity error leaves both frame bytes and invalidation state
unchanged. The `ssa_core_roots_observation` test covers relocation, inline
fields, scalar-looking pointer bits, and this atomic failure boundary.
