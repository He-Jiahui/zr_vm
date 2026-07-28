---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-28-e1a-canonical-local-binding-artifact
status: completed
completed_at: 2026-07-28 15:18 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
tests:
  - tests/debug/test_debug_metadata.c
---

# LSP 04 E1a Canonical Local Binding Artifact

## Status And Output Record

| Completed At | Status | Completed Output | Evidence |
|---|---|---|---|
| 2026-07-28 15:18 +08:00 | Completed | Compiled typed-local rows preserve canonical SymbolId, TypeId, PlaceId, and declaration range through source compilation, `.zro` serialization, IO reading, and runtime function projection. | GCC, Clang, and MSVC `zr_vm_debug_metadata_test`: 5 Tests, 0 Failures, real exit 0. |

## Contract

The compiler exposes a typed-local identity only from the exact semantic-IR
slot/Place already registered for the local. Artifact patch 37 serializes the
identity after the legacy typed type reference. Pre-37 artifacts deliberately
produce zero identity fields, so downstream debug/LSP work must report metadata
unavailable rather than infer identity from a local name, slot, AST, or text.

## Completed Plan Items

- E1a artifact carrier for visible local canonical identity.
- Versioned source/binary/runtime typed-local metadata roundtrip.
- Cross-toolchain direct regression coverage for source, binary, and runtime
  projections.

## Not Completed Here

- E1b frame generation, paused-frame PC liveness, receiver, generic context,
  and stale-frame rejection.
- E2 formal parser/binder/Place query reuse for debug expressions.
- E3 effect policy, E4 result transport, and E5 REPL cell generations.
- Migration or removal of the legacy standalone debug expression parser.

## Related Module Documentation

- [Debug canonical local bindings](../../../core-runtime/debug-canonical-local-bindings.md)
