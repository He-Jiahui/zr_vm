# AOT 11-S7ZN / 12-S7 Provider Version Selection Range Guard

Timestamp: 2026-07-02 07:25:55 +08:00

## Scope

This slice closes focused provider import coverage for exact alias/version selection when one root project references
multiple versions of the same provider assembly, plus fail-closed manifest parsing for strict three-part semver ranges
that do not contain the referenced provider version.

It does not claim automatic range-based candidate selection, cross-provider export metadata attach, full metadata
sweep/pruning, full trim analyzer coverage, or complete runtime ABI drift/deopt closure.

## Baseline / RED

- Added `tests/library/test_project_import_provider_version_selection.c` with two provider aliases for the same
  `zr.math` assembly: `mathV2` at `2.1.0` and `mathV3` at `3.1.0`.
- Positive coverage required `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()` to resolve
  `&mathV2.ops.sum` and `&mathV3.ops.sum` to distinct canonical module keys and provider-local filesystem paths.
- Negative coverage required `ZrLibrary_Project_New()` to reject a `.zrp` reference declaring provider version `3.1.0`
  while constraining the reference to `[2.0.0, 3.0.0)`.
- Initial RED after test harness repair: the positive exact-alias path already passed, but the negative manifest case
  failed with `Expected NULL`, proving the declared range was not enforced.

## Implementation

- `zr_vm_library/src/zr_vm_library/project/project.c` now validates declared provider reference ranges when the actual
  provider version, lower bound, and upper bound are all strict `major.minor.patch` semver strings.
- The guard is applied to legacy dependency `.zrp` references, `references` `.zrm` references, and `references` `.zrp`
  references.
- Invalid strict ranges where `min >= max` are rejected. Strict actual versions lower than `min` or greater than or
  equal to `max` are rejected.
- Missing or legacy non-strict version strings remain compatible with the existing manifest behavior.

## Test Inventory

- `tests/library/test_project_import_provider_version_selection.c`
- `zr_vm_project_import_provider_version_selection_test`
- Regression set:
  `zr_vm_project_import_resolver_test`, `zr_vm_project_manifest_normalization_test`,
  `zr_vm_project_import_aot_provider_runtime_test`, and `zr_vm_aot_c_provider_shared_library_smoke_test`

## Tooling Evidence

- WSL GCC: focused version-selection test plus resolver, manifest normalization, provider runtime, and provider
  shared-library smoke passed.
- WSL clang: focused version-selection test plus resolver, manifest normalization, provider runtime, and provider
  shared-library smoke passed.
- Windows MSVC Debug: focused version-selection test plus resolver, manifest normalization, provider runtime passed;
  provider shared-library smoke built and reported the existing Unix-only dynamic-loader branch as ignored.

Existing `project.c` const-qualifier warnings remain visible in these builds and are not introduced by this slice.

## Acceptance Decision

Accepted for 11-S7ZN / 12-S7 support: exact provider alias/version path selection is covered, and strict declared
semver range drift now fails closed during project manifest parse.
