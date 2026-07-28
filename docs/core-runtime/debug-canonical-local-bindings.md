---
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
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_metadata.c
  - docs/plans/lsp/04-debug-and-repl/2026-07-28-e1a-canonical-local-binding-artifact.md
doc_type: module-detail
---

# Debug Canonical Local Bindings

## Scope

The typed-local metadata row is the artifact boundary for debugger and LSP
frame-context reconstruction. Each source local can carry its compiler-owned
canonical `SymbolId`, `TypeId`, `PlaceId`, and whole declaration range through a
compiled function and its `.zro` representation. This metadata is an identity
projection, not a second binding pass.

## Source Projection

`compiler_semantic_ir_register_local` already constructs a local Place from the
canonical type-environment binding. `compiler_semantic_ir_get_slot_identity`
exposes only the corresponding slot identity when all of the TypeId, SymbolId,
and PlaceId are valid. The declaration range comes from that Place. Consumers
must treat a missing identity as unavailable; they must not recover it from a
local name, stack slot, or static display type.

`compiler_build_typed_local_bindings` copies that identity into
`SZrFunctionTypedLocalBinding` alongside the existing display-oriented type
projection. The existing type projection remains useful for legacy metadata but
is not a replacement for canonical `TypeId`.

## Binary Contract

`.zro` source patch 37 appends seven fixed-width fields to every typed-local row:

1. `symbolId`
2. `typeId`
3. `placeId`
4. declaration start line and column
5. declaration end line and column

The writer emits these fields after the existing typed type reference. The IO
reader reads them only for patch 37 or newer. Older artifacts receive zeroed
identity fields, which means frame reconstruction is unavailable rather than
guessed. Runtime loading copies the row unchanged into the executable function.

## Consumer Boundary

This metadata completes only LSP 04 E1a's artifact carrier. Frame generation,
current PC liveness, receiver and generic reconstruction, formal parser/binder
reuse, effect policy, and Debug/REPL transport remain separate milestones. In
particular, `zr_vm_lib_debug/debug_eval.c` must not use these fields to justify
its independent expression parser.

## Validation

`test_binary_roundtrip_preserves_canonical_local_binding_identity` compiles a
function, writes it to `.zro`, reads it, loads runtime metadata, and compares the
canonical IDs and declaration range at both boundaries. The target passed under
GCC, Clang, and MSVC on 2026-07-28.
