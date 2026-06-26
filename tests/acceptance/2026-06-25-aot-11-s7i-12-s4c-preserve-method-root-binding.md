# AOT 11-S7I / 12-S4C Preserve Method Root Binding

Timestamp: 2026-06-25 01:53:48 +08:00

Status: completed sub-slice. Full 11-S7 and 12-S4 remain open for type/generic preserve, metadata token resolution, cross-module method targets, annotations, feature switches, default-minimal metadata, and dump/diff tooling.

## Scope

- Added `ZrParser_Writer_ResolveTopLevelCallableFlatIndex()` so AOT code can resolve an entry function top-level callable binding name to its stable flat function index.
- Added `SZrCliAotPreserveRoots` and `ZrCli_Compiler_ApplyProjectAotPreserveRules()` for project AOT C emission.
- Bound same-module `.zrp` `preserve` rules with `kind: "method"` to `SZrAotWriterOptions.manifestPreserveFunctionFlatIndices`.
- Reused the existing opt-in code-stripping `MANIFEST` root path, rather than adding a default trimming feature switch in this slice.

## RED/GREEN

- RED: `tests/cli/test_cli_aot_writer_options.c` initially failed to build because the CLI preserve root container/helper and writer callable flat-index resolver did not exist.
- GREEN: the focused test resolves `main.kept` to flat index `2`, injects it as a writer manifest root, and generated C keeps `zr_aot_fn_0`, `zr_aot_fn_1`, and `zr_aot_fn_2` with code-stripping counts `3/3/0`.

## Verification

- WSL gcc: CTest `cli_args|cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` passed 4/4.
- WSL clang: CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` passed 3/3.
- Windows MSVC Debug: CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` passed 3/3.

## Notes

This slice only binds current-module top-level `method` preserve targets. It does not implement type/generic preserve roots, metadata-token binding, cross-module method target resolution, annotation roots, feature-switch defaults, default-minimal metadata policy, or metadata dump/diff tooling.
