# AOT 11-S7ZO / 12-S7 Provider Export Metadata Attach Fixture

Timestamp: 2026-07-02 08:13:22 +08:00

## Scope

This slice closes the focused end-to-end verification gap for provider manifest export metadata attach on the generated
provider dynamic-library path.

It does not claim a complete metadata sweep/pruning implementation, full trim analyzer coverage, or complete runtime
ABI drift/deopt closure. No production runtime behavior changed in this slice; the existing generated manifest export
table and metadata runtime mirror/view paths already supported the scenario.

## Baseline / RED

- The provider shared-library success fixture proved strict runtime loading, but it did not assert that `.zrp`
  `exports` declarations became writer manifest entries, generated `SZrAotManifestExportEntry` rows, and attached
  metadata-runtime manifest export views after dynamic loading.
- The first focused check was therefore a coverage gap rather than a production failure: the test had no assertions for
  provider method/field export token attachment.

## Implementation

- `tests/parser/test_aot_c_provider_shared_library_smoke.c` now writes provider manifest exports for method `add` and
  field `seed`.
- The fixture parses the provider `.zrp`, applies
  `ZrCli_Compiler_ApplyProjectAotExportDeclarations()`, and asserts writer options bind both exports to `MEMBER_DEF`
  metadata tokens.
- The generated C is checked for manifest export count markers, method/field export entries, member-token flags, and
  `manifestExports` / `manifestExportCount` descriptor wiring.
- After strict AOT runtime import, the loaded provider module's attached metadata runtime is queried for both manifest
  export views, including `HAS_MEMBER_TOKEN` flags and valid `MEMBER_DEF` tokens.

## Test Inventory

- `tests/parser/test_aot_c_provider_shared_library_smoke.c`
- `zr_vm_aot_c_provider_shared_library_smoke_test`
- Target-local CMake wiring for `compiler_aot_exports.c` and its includes.

## Tooling Evidence

- WSL GCC: focused provider shared-library smoke passed `1 Tests 0 Failures 0 Ignored`.
- WSL clang: focused provider shared-library smoke passed `1 Tests 0 Failures 0 Ignored`.
- Windows MSVC Debug: focused provider shared-library smoke built and reported the existing Unix-only dynamic-loader
  branch as `1 Tests 0 Failures 1 Ignored`.

## Acceptance Decision

Accepted for 11-S7ZO / 12-S7 support: generated provider libraries now have end-to-end fixture coverage proving
manifest export declarations are attached through generated C and visible from the provider metadata runtime after load.
