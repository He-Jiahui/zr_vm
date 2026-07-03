# AOT 11-S7Z / 12-S7 Export Manifest Declaration Model

Timestamp: 2026-07-02 00:37:35 +08:00

## Scope

This slice adds a declaration-level `.zrp` `exports` manifest model for later cross-module AOT metadata publication.
It accepts `type`, `method`, and `field` targets, stores them on `SZrLibrary_Project`, and mirrors the shape in
`zrp.schema.json`.

This is not the persistent export manifest/table writer, not token binding, and not cross-module provider version
resolution.

## RED

Added `test_project_manifest_normalization_parses_export_declarations`,
`test_project_manifest_normalization_rejects_invalid_export_kind`, and
`test_project_manifest_normalization_rejects_invalid_export_target`.

Initial WSL/GCC focused build failed because `SZrLibrary_Project` did not expose
`exportDeclarationCount`, `exportDeclarations`, or `ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_*`.

## GREEN

Implemented:

- `SZrLibrary_ProjectExportDeclaration` and `EZrLibrary_ProjectExportDeclarationKind`
- `project/project_exports.{h,c}` parser/free module
- `ZrLibrary_Project_New()` and `ZrLibrary_Project_Free()` wiring
- `.zrp` schema parity for top-level `exports`

## Verification

- WSL/GCC build: `cmake --build build-wsl-gcc --target zr_vm_project_manifest_normalization_test -j2` passed.
- WSL/GCC direct: `./build-wsl-gcc/bin/zr_vm_project_manifest_normalization_test` passed 28 tests / 0 failures.
- WSL/clang build + direct: `cmake --build build-wsl-clang --target zr_vm_project_manifest_normalization_test -j2 && ./build-wsl-clang/bin/zr_vm_project_manifest_normalization_test` passed 28 tests / 0 failures.
- Windows/MSVC Debug build: `cmake --build build-msvc --target zr_vm_project_manifest_normalization_test --config Debug --parallel 2` passed.
- Windows/MSVC Debug direct: `build-msvc/bin/Debug/zr_vm_project_manifest_normalization_test.exe` passed 28 tests / 0 failures.

Note: the focused project manifest normalization executable is not currently registered as a CTest in these build trees;
`ctest -R project_manifest_normalization` found no tests.

## Decision

Accepted for the 11-S7 manifest declaration input slice. The remaining export work is the actual persistent
manifest/table writer, target token binding, compacted-token publication into that writer, and provider-side
version/ABI resolution.
