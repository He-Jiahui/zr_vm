# 2026-06-24 AOT 12-S7H Type Layout Trim Before/After Statistics

## Scope

12-S7H publishes opt-in code-stripping before/after statistics for inline type-layout references:

- `code_stripping.typeLayoutsBefore`
- `code_stripping.typeLayoutsAfter`
- `code_stripping.typeLayoutsRemoved`

The count is the number of distinct inline `typeLayoutId` references in the AOT function table before and after
reachability filtering.

This is not a generated type-layout byte delta, zrp metadata section/table/pool breakdown, trim warning, or release
symbol-stripping feature.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.{h,c}`
  exposes `backend_aot_c_type_layout_count_referenced()` for distinct inline layout-reference counting.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  records type-layout reference counts before and after `backend_aot_apply_code_stripping()` and emits the three
  `code_stripping.typeLayouts*` markers in the generated C header.
- `tests/parser/test_aot_c_code_stripping.c`
  attaches synthetic inline layout slots to reachable/removable child functions and verifies ordinary trim, export-root,
  and manifest-root cases.

## RED / GREEN

RED:

- `zr_vm_aot_c_code_stripping_test` failed 3/3 once the fixture required type-layout before/after/removed markers.

GREEN:

- Ordinary trim path reports 2 type-layout references before filtering, 1 after filtering, and 1 removed.
- Export-root and manifest-root preservation paths report 2 before, 2 after, and 0 removed.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test'`
  - 3 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7H only. AOT C now exposes a type-layout reference-count before/after comparison for opt-in code
stripping, but generated layout byte delta, zrp section/table/pool metadata statistics, trim warnings, and release
symbol stripping remain open.
