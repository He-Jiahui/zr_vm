# AOT 11-S7K / 12-S4E Preserve Feature Condition Model

Timestamp: 2026-06-25 02:23:14 +08:00

Status: completed sub-slice. Full 11-S7 and 12-S4 remain open for feature condition evaluation/build config input, generic preserve roots, metadata token resolution, cross-module targets, annotation roots, default-minimal metadata policy, and dump/diff tooling.

## Scope

- Added `feature`, `hasFeatureValue`, and `featureValue` to `SZrLibrary_ProjectPreserveRule`.
- Extended `.zrp` `preserve` parsing to accept safe dotted `feature` names paired with boolean `featureValue`.
- Rejected half-declared feature conditions: `featureValue` without `feature`, and `feature` without `featureValue`.
- Mirrored the new fields and mutual dependency in `zrp.schema.json`.

## RED/GREEN

- RED: manifest normalization tests failed to compile because `SZrLibrary_ProjectPreserveRule` had no feature condition fields.
- GREEN: project manifests preserve both `featureValue: true` and `featureValue: false` in the project model.
- GREEN: invalid half-declared feature conditions reject the manifest before later AOT writer stages consume it.

## Verification

- WSL gcc: `zr_vm_project_manifest_normalization_test` passed 17/0.
- WSL clang: `zr_vm_project_manifest_normalization_test` passed 17/0.
- Windows MSVC Debug: `zr_vm_project_manifest_normalization_test` passed 17/0.
- Schema syntax: `python -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json` passed.
- WSL gcc regression CTest: `cli_aot_writer_options|aot_c_code_stripping` passed 2/2.
- `git diff --check` exited 0 with only LF/CRLF warnings.

## Notes

This slice models feature-conditioned preserve declarations only. It does not evaluate build feature values, enable or disable writer roots by feature, bind generic preserve arguments, resolve metadata tokens, implement cross-module target binding, or close default-minimal metadata policy.
