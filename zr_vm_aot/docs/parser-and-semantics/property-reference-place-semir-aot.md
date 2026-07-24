---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_member_access.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_member_access.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m4-ref-return-place-region-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration-implementation-plan.md
tests:
  - tests/parser/test_property_ref_return.c
  - tests/parser/test_property_consumer_contracts.c
  - tests/parser/test_property_consumer_stripping_cases.h
  - tests/parser/test_aot_c_code_stripping.c
doc_type: module-detail
---

# Property Reference Place `SemIR -> ExecBC -> AOT`

## Stable semantic operations

Reference-return properties do not reuse value `META_GET`. The compiler projects a stable
`PROPERTY_REF_GET` operation with exact property/accessor identity and a reference result, then emits
`DEREFERENCE` and `PROPERTY_REF_STORE` only when the source context needs a referent Place or mutation.
These operations are appended to the execution SemIR enum, preserving every existing numeric id.

ExecBC may use separate member/index create and load/store operations, but quickening is not the
semantic contract. The source function and its executable-artifact reload must expose the same stable
SemIR sequence and the same result TypeId.

## Managed runtime boundary

Both AOT backends call the shared `ZrLibrary_AotRuntime_PropertyReference*` helpers. The carried value
contains a managed base and structured projection; generated C and LLVM never cache an unguarded
interior pointer across a call, frame relocation, or GC movement. Writable access is checked by the
shared store helper. Native direct pointers remain rejected unless a future descriptor publishes an
explicit pinned/managed ABI.

## Backend parity

The C emitter and LLVM lowering handle create-member, create-index, load and store explicitly. The
LLVM module prelude declares the same runtime helper signatures used by generated C. Focused tests
assert both textual contracts, compile the generated C and LLVM objects, and compare source/reloaded
SemIR plus cold/second-run VM results.

## Property accessor stripping roots

Opt-in AOT code stripping treats concrete property accessors as structured roots. The reachability
scanner reads compiled prototype rows, requires a non-sentinel `propertyIdentity` and a getter,
setter, or initializer `accessorRole`, then resolves the row's `functionConstantIndex` through the
normal callable constant table. The retained function is reported as `root.property_accessor` with
no predecessor. Abstract or bodyless accessor rows have no concrete function and are skipped.

The scanner never derives a root from a property name or hidden accessor spelling. This keeps
reflection and dynamic property dispatch valid after stripping while ordinary unreferenced methods
remain removable. Malformed packed prototype rows fail the stripping analysis instead of being
partially scanned.

## Boundary

This document covers executable semantics only. LSP presentation, final reflection PropertyDef
projection, and legacy migration are Syntax 05 M5 consumers. They must join canonical SymbolId and
TypeId facts and may not infer reference behavior from names or emitted text.
