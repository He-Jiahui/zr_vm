---
doc_type: core-runtime-contract
status: implemented
---

# Pointer-free ExecIR artifact contract

`artifact_exec_ir.h` defines a compact, fixed-width wire contract for an
ExecIR/ExecBC bundle. Headers and section directory entries are encoded
little-endian; section references use byte offsets and element IDs only. No
runtime function, object, allocator, or native entry pointer is accepted by
the writer.

The reader validates the magic, schema version, section count, directory
extent, section alignment metadata, bounds, duplicate kinds, and overlapping
payloads before exposing a section view. The 64 MiB global limit and 32-section
limit are hard loader limits. A failed read zeroes the output view.

Relocation records carry a metadata token, target kind, code offset, and
expected content/contract hashes. `ValidateRelocations` checks record shape and
invokes a host resolver into a caller-provided temporary ID array. Resolution
is all-or-nothing from the caller's perspective: failed records do not modify
later output slots, and no runtime witness is serialized.

The parser wrapper (`ZrParser_ExecIr_Read`/`Write`) intentionally only adapts
reader/writer buffers to the core contract. Publication, process-local target
binding, and generation management remain loader responsibilities after full
validation.
