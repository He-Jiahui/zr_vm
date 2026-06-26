# AOT 11-S7J / 12-S4D Dotted And Type-Member Preserve Roots

Timestamp: 2026-06-25 02:09:47 +08:00

Status: completed sub-slice. Full 11-S7 and 12-S4 remain open for generic preserve roots, metadata token resolution, cross-module method targets, annotation roots, feature switches, default-minimal metadata policy, and dump/diff tooling.

## Scope

- Extended same-module `method` preserve binding so a dotted target such as `Widget.kept` can match the exact top-level callable binding name.
- Kept current-module prefix fallback for targets such as `main.kept`.
- Added `type` preserve expansion for `members: "methods"` and `members: "all"` by scanning entry function top-level callable bindings for the `<type>.` prefix.
- Reused the existing writer manifest root channel and `MANIFEST` reachability reason.

## RED/GREEN

- RED: new CLI AOT writer options tests for dotted method targets and type-member prefix expansion built but failed with zero resolved roots.
- GREEN: `Widget.kept` resolves to flat index `2`, and `type Widget members methods` resolves to flat indices `1` and `2`.
- Generated C fixtures preserve all three functions and report code-stripping counts `functionsBefore = 3`, `functionsAfter = 3`, `functionsRemoved = 0`.

## Verification

- WSL gcc: CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` passed 3/3.
- WSL clang: CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` passed 3/3.
- Windows MSVC Debug: CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` passed 3/3.

## Notes

This slice only expands current-module callable name matching and type-to-method root expansion. It does not implement generic preserve declarations, metadata-token resolution, cross-module target binding, annotation-driven roots, feature switch defaults, default-minimal metadata, or metadata dump/diff tooling.
