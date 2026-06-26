# 2026-06-25 AOT 11-S7H / 08-S7 / 12-S8G CLI AOT C emission entry

## Status

Completed focused CLI/project emission-entry slice. Full 11-S7 / 08-S7 / 12-S8 remain open.

## Completed

- Added `--emit-aot-c` as a compile option in CLI parsing, validation, and help text.
- Project compilation now resolves AOT C output paths as `bin/aot_c/src/<module>.c`, including dependency module paths rooted under the dependency package binary root.
- Added `ZrCli_Compiler_WriteAotCFileForModule()` to read the just-written `.zro` bytes, populate `SZrAotWriterOptions` with binary embedded-module input, apply project AOT writer policy, and call the AOT C writer.
- Incremental output checking now treats the `.c` file as a required output when `--emit-aot-c` is enabled.
- Manifest format advanced to v3 and records `aot_c` paths so stale AOT C files can be pruned when modules are removed or the option is disabled.
- Added focused CLI tests for argument parsing, path resolution, manifest v3 behavior, and full-AOT project AOT C emission.

## RED / GREEN

- RED: `zr_vm_cli_args_test` failed to compile because `SZrCliCommand` had no `emitAotC` member.
- GREEN: implemented the command field, project path/manifest support, compiler integration, and focused incremental AOT C emission test.

## Validation

- WSL gcc: built `zr_vm_cli_args_test` and `zr_vm_cli_project_incremental_test`; CTest `cli_args|cli_project_incremental` passed 2/2.
- WSL clang: built `zr_vm_cli_executable`, `zr_vm_cli_args_test`, and `zr_vm_cli_project_incremental_test`; CTest `cli_args|cli_project_incremental` passed 2/2.
- Windows MSVC Debug: built `zr_vm_cli_executable`, `zr_vm_cli_args_test`, and `zr_vm_cli_project_incremental_test`; CTest `cli_args|cli_project_incremental` passed 2/2.
- Windows CLI smoke: after deleting the generated AOT C file, `zr_vm_cli.exe --compile ... --emit-aot-c --incremental` reported `compiled=1 skipped=0 removed=0` and regenerated `bin/aot_c/src/main.c`.

## Notes

This slice closes the CLI/project entry to the AOT C writer and proves `.zrp aotMode: "full-aot"` reaches generated C through the project compiler path. It does not complete manifest dynamic generic instance roots, reflection preservation, preserve target token/flat-index binding, default-minimal metadata policy, dump/diff tooling, or complete full-AOT closure diagnostics.
