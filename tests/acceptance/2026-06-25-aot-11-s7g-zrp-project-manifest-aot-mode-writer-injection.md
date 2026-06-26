# 2026-06-25 AOT 11-S7G / 08-S7 / 12-S8 zrp project manifest AOT mode writer injection

## Status

Completed focused bridge slice. Full 11-S7 / 08-S7 / 12-S8 remain open.

## Completed

- Added `ZrCli_Compiler_ApplyProjectAotWriterOptions()` as the CLI/compiler bridge from `SZrLibrary_Project.aotMode` to `SZrAotWriterOptions.requireFullAot`.
- `aotMode: "full-aot"` now sets `requireFullAot = ZR_TRUE`.
- Missing `aotMode` remains the manifest default `hybrid` and clears `requireFullAot`.
- Existing writer options such as `requireExecutableLowering` and `enableCodeStripping` are preserved.
- Added focused CLI project tests covering full-AOT and default hybrid behavior.

## RED / GREEN

- RED: `zr_vm_cli_project_incremental_test` failed to link because `ZrCli_Compiler_ApplyProjectAotWriterOptions` was declared by the test but not implemented.
- GREEN: implemented the helper and switched new test path construction to the checked local path helper.

## Validation

- WSL gcc: `zr_vm_cli_project_incremental_test` 10/0.
- WSL clang: `zr_vm_cli_project_incremental_test` 10/0.
- Windows MSVC Debug: `zr_vm_cli_project_incremental_test` 10/0.
- Windows MSVC Debug CLI smoke: `hello_world` printed `hello world`.

## Notes

This slice exposes the policy injection point for project-driven AOT writer options. It does not add a CLI AOT C emission mode, manifest dynamic generic roots, preserve target token binding, or full-AOT closure diagnostics.
