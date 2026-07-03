# AOT 11-S7ZP / 12-S7 Provider Range Candidate Selection

Timestamp: 2026-07-02 08:16:21 +08:00

## Scope

This slice adds automatic range-based provider candidate selection for `.zrp` `references` entries. A reference can now
declare `candidates` instead of an exact `path`; the loader probes the candidates, filters by declared assembly and
version constraints, and selects the highest strict `major.minor.patch` version inside the declared range.

It does not claim complete metadata sweep/pruning, full trim analyzer coverage, or a complete ABI drift/deopt loop.

## Baseline / RED

- Added `test_provider_import_selects_highest_candidate_within_declared_range`, requiring a root reference to select
  `zr.math` `2.2.0` from candidates `2.0.5`, `2.2.0`, and out-of-range `3.1.0` for `[2.0.0, 3.0.0)`.
- Added `test_provider_import_rejects_candidate_set_without_range_match`, requiring a candidate-only reference to fail
  closed when no provider version satisfies the declared range.
- Initial RED failed before selection: the old loader rejected `references.math` because candidate-only references had
  no required `path`.

## Implementation

- `zr_vm_library/src/zr_vm_library/project/project.c` accepts `references.alias.candidates[]` as an alternative to
  `path`; exact references may still use `path`, but `path` and `candidates` are mutually exclusive.
- Candidate items can be string paths or objects with `path` and optional `version`.
- Candidate probing resolves paths relative to the owning `.zrp`, reads `.zrp` manifest identity or opens `.zrm`
  provider archives, filters by declared `assembly`, exact candidate/requested version where present, and existing
  `[minVersionInclusive, maxVersionExclusive)` constraints.
- Automatic ordering requires strict three-part semver; the highest matching candidate is selected and then fed back
  through the existing normalized reference/package path so canonical imports remain `$alias@version/module`.
- Non-selected candidates are only probed and are not added to the project dependency table. Malformed candidate sets or
  no-match candidate sets fail closed.
- `zr_vm_language_server_extension/schemas/zrp.schema.json` now validates either exact `path` references or candidate
  references and includes a `mathRange` example.

## Test Inventory

- `tests/library/test_project_import_provider_version_selection.c`
- `zr_vm_project_import_provider_version_selection_test`
- Regression set:
  `zr_vm_project_import_resolver_test`, `zr_vm_project_manifest_normalization_test`,
  `zr_vm_project_import_aot_provider_runtime_test`, and `zr_vm_aot_c_provider_shared_library_smoke_test`

## Tooling Evidence

- WSL GCC: version-selection test passed `4 Tests 0 Failures`, and resolver, manifest normalization, provider runtime,
  and provider shared-library smoke regressions passed.
- WSL clang: same focused set passed. Existing `project.c` const-qualifier warnings remain visible and are not introduced
  by this slice.
- Windows MSVC Debug: same focused set passed; provider shared-library smoke remains `0 Failures 1 Ignored` for the
  Unix-only dynamic-loader branch. Existing `project.c` const-qualifier warnings remain visible.
- Schema parse check: `zrp.schema.json parsed`.

## Acceptance Decision

Accepted for 11-S7ZP / 12-S7 support: candidate-only provider references can select the highest in-range provider
without mutating dependency state for non-selected candidates, and no-match candidate sets now fail closed.
