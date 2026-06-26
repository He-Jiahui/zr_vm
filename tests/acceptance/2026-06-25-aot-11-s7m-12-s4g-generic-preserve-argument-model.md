# AOT 11-S7M / 12-S4G Generic Preserve Argument Model

Timestamp: 2026-06-25 03:02:14 +08:00

Status: completed sub-slice. Full 11-S7 and 12-S4 remain open for generic instantiation root binding, metadata token resolution, cross-module targets, annotation roots, default-minimal metadata policy, and dump/diff tooling.

## Scope

- Added `genericArguments`, `genericArgumentCount`, and `genericArgumentCapacity` to `SZrLibrary_ProjectPreserveRule`.
- Extended `.zrp` `preserve` parsing so `kind: "generic"` requires a non-empty `arguments` array.
- Validated each generic preserve argument with the existing safe dotted target-name rules.
- Rejected missing, empty, non-array, or invalid `arguments`, and rejected `arguments` on non-generic preserve rules.
- Mirrored the `arguments` array, `minItems: 1`, and generic-only conditional constraints in `zrp.schema.json`.

## RED/GREEN

- RED: manifest normalization failed to compile because `SZrLibrary_ProjectPreserveRule` had no generic argument fields.
- RED: `kind: "generic"` without `arguments` was accepted.
- RED: `kind: "generic"` with an empty `arguments` array was accepted.
- RED: `arguments` on a non-generic preserve rule was accepted.
- GREEN: `List` with `arguments: ["Foo", "Bar.Baz"]` parses into the project model.
- GREEN: invalid, missing, empty, non-array, and non-generic `arguments` reject the manifest.

## Verification

- WSL gcc: `zr_vm_project_manifest_normalization_test` passed 25/0.
- WSL clang: `zr_vm_project_manifest_normalization_test` passed 25/0.
- Windows MSVC Debug: `zr_vm_project_manifest_normalization_test` passed 25/0.
- Schema syntax: `python -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json` passed.
- WSL gcc regression CTest: `cli_aot_writer_options|aot_c_code_stripping` passed 2/2.
- `git diff --check` exited 0 with only LF/CRLF warnings.

## Notes

This slice models generic preserve type arguments only. It does not bind those arguments to MethodSpec or TypeSpec metadata tokens, add generic instantiation roots to reachability, resolve cross-module targets, implement annotation roots, or close default-minimal metadata policy.
