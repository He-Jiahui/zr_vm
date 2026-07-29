---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
plan_sources:
  - user: 2026-07-30 execute AOT plans 07 through 12 and record each completed sub-milestone
  - docs/plans/aot/12-code-stripping.md
tests:
  - tests/parser/test_aot_c_code_stripping.c
  - tests/acceptance/2026-07-30-aot-12-s1c-s2g-s6b-type-layout-reachability-manifest.md
doc_type: module-detail
---

# AOT Type-Layout Reachability Manifest

## Purpose

The type-layout reachability layer explains every `SZrTypeLayout` retained by opt-in AOT C code stripping. It runs
after function reachability has filtered the function table, so frame dependencies can point at stable retained
function flat indices instead of transient table positions. It validates the complete retained-layout set before
publishing any type-layout rows.

This module is separate from `backend_aot_c_type_layouts.c`. The latter resolves and emits C layout declarations,
registration tables, token tables, and byte statistics; the reachability module owns only graph reasons, provenance,
deterministic ordering, and fail-closed publication.

## Node And Reason Contract

Each row is keyed by a valid `typeLayoutId`; ID `0` is valid and the invalid sentinel is `UINT32_MAX`. Version 1 has
two finite reasons:

- `root.reflection_annotation`: an explicit `dynamicDependencyTypeLayoutId`, type token, or field token retained the
  layout. Root rows use `predecessorFunction=none`.
- `edge.frame_layout`: an inline-struct frame slot in a retained function references the layout. The predecessor is
  that function's stable flat index.

An explicit root wins when the same layout is also referenced by a retained frame. Multiple frame references select
the first function in stable filtered-table order. A trimmed function cannot be a frame predecessor because collection
runs only after `backend_aot_filter_function_table_by_reachability()` has completed.

## Deterministic Collection

The collector performs validation before output:

1. Validate the retained function table, non-null functions, unique flat indices, and frame-layout storage.
2. Validate each explicit root, reject duplicate or sentinel IDs, and prove that a retained function can resolve its
   struct or union layout.
3. Validate every retained inline-struct frame reference and prove that its layout resolves from that function.
4. Scan `typeLayoutId` from `0` in ascending order, select root precedence, and build a compact row array.
5. Require the row count to equal the emitter's independently computed `typeLayoutsAfter` count.

Only after both collection passes agree does the writer emit:

```text
/* reachability.typeLayoutManifest.version = 1 */
/* reachability.typeLayoutManifest.count = 2 */
/* reachability.typeLayoutManifest.node[1] = reason=edge.frame_layout predecessorFunction=1 */
/* reachability.typeLayoutManifest.node[2] = reason=root.reflection_annotation predecessorFunction=none */
```

The emitter removes the generated file and returns false when validation, allocation, count parity, or file output
fails. No unresolved retained layout is silently represented by a missing declaration and no partial artifact is
accepted.

## Test Coverage

`test_aot_c_code_stripping.c` covers a frame-only retained layout, a dynamic dependency root beside a frame edge,
field-token roots that demonstrate root-over-edge precedence, ascending row order, omission of a trimmed layout, and
rejection of an unresolved layout referenced by a retained function. Separate cases cover ID `0` as a frame edge and
as an annotation root, plus a retained-table position whose stable predecessor flat index is `2`. The negative case
also verifies that the writer deletes the partial generated C file.

The collector also defensively rejects malformed tables, duplicate or sentinel roots, invalid frame storage, count
divergence, allocation failure, and output failure. This slice's public integration evidence directly fault-injects
the unresolved-layout path and partial-file cleanup; the other defensive branches are implementation guardrails, not
separately claimed fault-injection coverage.

The focused acceptance matrix runs both function reachability and C code stripping on WSL GCC, WSL Clang, and a clean
Windows MSVC build. Generic dictionaries, native imports, module initializers, reflection metadata nodes, debug
sidecars, and resource Drop nodes remain later AOT 12 graph families.
