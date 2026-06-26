# 2026-06-24 · AOT 11-S7E / 12-S4B zrp project manifest preserve rule parsing

## Status

Completed as a narrow 11-S7 manifest sub-slice and 12-S4 manifest-root bridge slice.

This records declaration-level `.zrp` `preserve` parsing only. It does not claim symbol/token binding, function flat-index root injection, generic instantiation preservation, annotation roots, feature switches, AOT mode policy, default-minimal metadata policy, or zrp metadata dump/diff tooling.

## Completed

- Added `SZrLibrary_ProjectPreserveRule`, preserve kind/member enums, and `SZrLibrary_Project.preserveRules` / `preserveRuleCount`.
- Added `project_preserve.{h,c}` to parse top-level `.zrp` `preserve` arrays without growing `project.c`.
- Accepted `kind: "type"`, `"method"`, and `"generic"` with required safe `target` text and optional `members: "all"` / `"methods"`.
- Rejected invalid preserve targets containing empty text, whitespace, path separators, `@`, `$`, or invalid dotted segments.
- Added schema parity for `preserve` in `zr_vm_language_server_extension/schemas/zrp.schema.json`.
- Added focused manifest normalization tests for valid preserve parsing and invalid preserve rejection.

## RED/GREEN

- RED: `zr_vm_project_manifest_normalization_test` failed to compile after tests referenced missing preserve fields and enums.
- GREEN: valid type/all and method/default preserve rules parse into the project model, and invalid preserve target input rejects the manifest.

## Verification

- WSL gcc: `zr_vm_project_manifest_normalization_test` 12/0.
- WSL gcc: `zr_vm_project_import_resolver_test` 9/0.
- WSL clang: `zr_vm_project_manifest_normalization_test` 12/0.
- WSL clang: `zr_vm_project_import_resolver_test` 9/0.
- JSON schema syntax: `python -m json.tool zr_vm_language_server_extension/schemas/zrp.schema.json`.
- Windows MSVC: `zr_vm_project_manifest_normalization_test.exe` 12/0.
- Windows MSVC: `zr_vm_project_import_resolver_test.exe` 9/0.
- Windows MSVC CLI smoke: `hello_world` printed `hello world`.

