---
related_code:
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_source_free.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_source_free.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
plan_sources:
  - docs/plans/syntax/astra.md
  - user: 2026-09-05 repair source graph leaks exposed by ownership binary roundtrip validation
tests:
  - tests/artifact/test_manifest_roundtrip.c
  - tests/artifact/test_io_source_lifetime_cases.h
  - tests/parser/test_ownership_abrupt_roundtrip_cases.h
doc_type: module-detail
---

# IO Source Lifetime

`ZrCore_Io_ReadSourceNew` returns a temporary decoded source graph. The caller
owns this graph and releases it once through `ZrCore_Io_ReadSourceFree`, whether
or not it was loaded into runtime functions. Passing a null source is allowed.
The global state used by the reader must remain alive until the source is freed.

## Allocation Ownership

The reader allocates source and module records, tagged declarations, function
records, native arrays, and binary blobs with `ZR_MEMORY_NATIVE_TYPE_IO`.
`io_source_free.c` walks these records recursively and frees each allocation
using the same allocation category and element size.

Nested storage includes function-valued constants and defaults, property
accessors, method and meta overloads, closure children, typed generic constraints,
callable effects, compile-time parameter and path metadata, and debug line and
range tables. Each occurrence is separately allocated by the reader; the native
graph has no shared child ownership. Enum fields use `sizeof(SZrIoEnumField)` for
both allocation and release.

Strings and object or array constant values are created with GC-managed APIs.
The source destructor does not free them. Name arrays own the array storage,
not the strings referenced by their elements. Temporary native string buffers
are released by the reader immediately after string creation.

## Runtime Loading

`ZrCore_Io_LoadEntryFunctionToRuntime` copies instructions, frame layouts,
metadata arrays, signature and prototype blobs, and closure child containers
into runtime-owned storage. Function-valued constants and child functions are
loaded recursively. Managed names and constant objects are shared with the
runtime and remain subject to normal GC lifetime rules.

After loading finishes, the caller may free the source graph before executing
the loaded function. This also applies when loading rejects a successfully
decoded source. Source release does not close the `SZrIo` reader or free its
input byte buffer; those lifetimes remain with the reader's caller or close
callback. No serialized layout or public API changes are required.

`ReadSourceFree` accepts completed decoded graphs. A read that fails before
returning its graph still needs separate reader-side partial-construction
cleanup; this destructor does not change that failure path.

## Validation

`zr_vm_test_manifest_roundtrip_test` includes two source lifetime regressions.
They compile and decode a function tree containing a class property, default
parameter metadata, and exception handlers, then check that IO allocation counts
and exact allocated byte sizes balance after source release. One case frees an
unloaded graph. The other loads the graph, frees both the graph and serialized
input, and executes the runtime function to verify its independent storage.

The existing manifest roundtrip also frees a source after a rejected runtime
load. Ownership binary roundtrip cases exercise source release with pending
control metadata. Sanitizer runs should cover these call sites to detect stale
runtime references and allocations missed by the recursive walk.
