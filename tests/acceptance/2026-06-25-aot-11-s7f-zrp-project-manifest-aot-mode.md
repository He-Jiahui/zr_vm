# 2026-06-25 · AOT 11-S7F zrp project manifest AOT mode parsing

## Status

Completed as a narrow 11-S7 manifest sub-slice.

This records declaration-level `.zrp` `aotMode` parsing only. It does not claim CLI/writer option injection, full-AOT closure diagnostics, dynamic generic instance preservation, default-minimal metadata policy, or zrp metadata dump/diff tooling.

## Completed

- Added `EZrLibrary_ProjectAotMode` with `HYBRID` and `FULL_AOT` modes.
- Added `SZrLibrary_Project.aotMode`; absent `aotMode` defaults to hybrid.
- Added `project_aot_options.{h,c}` to parse top-level `.zrp` `aotMode` without growing `project.c`.
- Accepted `aotMode: "hybrid"` and `aotMode: "full-aot"`.
- Rejected non-string or unsupported `aotMode` values.
- Added schema parity for `aotMode` in `zr_vm_language_server_extension/schemas/zrp.schema.json`.
- Added focused manifest normalization tests for the default, valid full-AOT parse, and invalid mode rejection.

## RED/GREEN

- RED: `zr_vm_project_manifest_normalization_test` failed to compile after tests referenced missing AOT mode fields and enum values.
- GREEN: missing `aotMode` defaults to hybrid, `full-aot` maps into the project model, and invalid mode input rejects the manifest.

## Verification

- WSL gcc: `zr_vm_project_manifest_normalization_test` 14/0.
- WSL gcc: `zr_vm_project_import_resolver_test` 9/0.
- WSL clang: `zr_vm_project_manifest_normalization_test` 14/0.
- WSL clang: `zr_vm_project_import_resolver_test` 9/0.
- JSON schema syntax: `python -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json`.
- Windows MSVC: `zr_vm_project_manifest_normalization_test.exe` 14/0.
- Windows MSVC: `zr_vm_project_import_resolver_test.exe` 9/0.
- Windows MSVC CLI smoke: `hello_world` printed `hello world`.

## Notes

- The implementation intentionally stores policy only in the project model. A later slice must wire `project->aotMode == FULL_AOT` into `SZrAotWriterOptions.requireFullAot` through the CLI/compiler pipeline.
- Fast build targets can reuse stale objects after public struct layout changes. Focused verification rebuilt the affected library and test objects before recording results.
