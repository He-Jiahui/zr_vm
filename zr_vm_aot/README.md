# `zr_vm_aot`

This directory is an **archive** of historical AOT assets. The active AOT backend
and the live AOT runtime no longer live here (see `scripts/layout/layout_map.tsv`
for the old → new path map). Some archive files are still consumed by the main
test build, so the directory must not be deleted or reorganised casually.

## Where the active code lives

| Active path (top level) | Role |
| --- | --- |
| `../zr_vm_parser/src/zr_vm_parser/backend_aot/` | Active AOT C and LLVM backend, compiled into the parser by `zr_vm_parser/CMakeLists.txt`. It is no longer under `zr_vm_aot/`. |
| `../zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c` | Live AOT runtime linked into products. |
| `../zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | Current production AOT ABI. |

## What in this archive is still consumed

| Archive path | Consumer |
| --- | --- |
| `tests/parser/test_*.c` (6 sources) | Compiled as Unity test executables by `tests/cmake/aot-pipeline-targets.cmake` through `zr_vm_add_execbc_aot_pipeline_test` (`tests/CMakeLists.txt`). They are `add_executable` targets only and are **not registered with CTest**. |
| `zr_vm_library/src/zr_vm_library/aot_runtime.c` | Read as a **source-text oracle** by `tests/parser/test_aot_c_source_contracts.c` (the runtime needles must appear in both the live and the archived runtime). Not linked into any product. |
| `tests/fixtures/**` | Listed by `tests/fixtures/syntax_migration_inventory/expected/repository-inventory.json`; do not move or delete without updating that inventory. |
| `zr_vm_library/include/zr_vm_library/aot_runtime.h`, `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | Historical headers, referenced by the code-review coverage inventory `docs/code-review/coverage/zr_vm_aot_tests_runtime.tsv`. Not interchangeable with the top-level files of the same name. |

Everything else (`docs/`, `scripts/`, `tests/acceptance/`, `tests/projects/*.gdb`,
`tests/scripts/`) is historical reference only; `tests/scripts/CMakeLists.txt` is
not added to the build by any `add_subdirectory`.

## Known divergences (archive only)

- `zr_vm_library/src/zr_vm_library/aot_runtime.c`
  `ZrLibrary_AotRuntime_OwnDetach` calls `ZrCore_Ownership_DetachValue` only (see
  its `BUG` note). This copy is a source-text oracle and is not linked into any
  product. Current source lowering emits `OWN_INTO_GC_BOX` →
  `ZrLibrary_AotRuntime_OwnIntoGcBox`, and the live legacy `OwnDetach` in the
  top-level runtime runs IntoGcBox-or-Detach (see
  `docs/core-runtime/gc-domain-single-mutator-bridge.md`). Do not edit the
  archived file to "fix" this: changing it can break the needles checked by
  `tests/parser/test_aot_c_source_contracts.c`.

## Rules

- The historical files are not interchangeable with the current implementations
  and must not be deleted merely because they share file names.
- Before moving or deleting an archive file, update every consumer listed above
  in the same change.
