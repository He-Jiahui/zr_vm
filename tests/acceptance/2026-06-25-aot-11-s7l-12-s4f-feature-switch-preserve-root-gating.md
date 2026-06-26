# AOT 11-S7L / 12-S4F Feature Switch Preserve Root Gating

Timestamp: 2026-06-25 02:40:15 +08:00

Status: completed sub-slice. Full 11-S7 and 12-S4 remain open for generic preserve roots, metadata token resolution, cross-module targets, annotation roots, default-minimal metadata policy, and dump/diff tooling.

## Scope

- Added `SZrLibrary_ProjectFeatureSwitch` and `featureSwitches` / `featureSwitchCount` / `featureSwitchCapacity` to the project model.
- Added `project_features.{h,c}` to parse top-level `.zrp` `features` boolean maps and free the normalized feature switch table.
- Mirrored the top-level safe dotted boolean feature switch map in `zrp.schema.json`.
- Applied feature-conditioned preserve root gating in `ZrCli_Compiler_ApplyProjectAotPreserveRules()` before resolving method and type-member roots.

## RED/GREEN

- RED: manifest normalization and CLI writer-option tests failed to compile because the feature switch model and preserve-rule gating helper did not exist.
- GREEN: project manifests preserve `features` switch names and true/false values, while invalid switch names reject the manifest.
- GREEN: a matching feature switch preserves `Widget.kept` as a writer manifest root and keeps the generated `zr_aot_fn_2` body.
- GREEN: a mismatched feature switch leaves the writer manifest root list empty and generated C trims `zr_aot_fn_2` while reporting one removed function.

## Verification

- WSL gcc: `zr_vm_project_manifest_normalization_test` passed 19/0.
- WSL gcc: `zr_vm_cli_aot_writer_options_test` passed 5/0.
- WSL clang: `zr_vm_project_manifest_normalization_test` passed 19/0.
- WSL clang: `zr_vm_cli_aot_writer_options_test` passed 5/0.
- Windows MSVC Debug: `zr_vm_project_manifest_normalization_test` passed 19/0.
- Windows MSVC Debug: `zr_vm_cli_aot_writer_options_test` passed 5/0.
- WSL gcc regression CTest: `cli_aot_writer_options|aot_c_code_stripping` passed 2/2.
- Schema syntax: `python -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json` passed.
- `git diff --check` exited 0 with only LF/CRLF warnings.

## Notes

This slice gates only the preserve roots currently supported by the CLI bridge: same-module `method` rules and `type` rules with `members: "methods"` or `"all"`. Generic preserve roots, metadata-token binding, cross-module target binding, annotation-driven roots, default-minimal metadata policy, and dump/diff tooling remain open.
