# AOT 11-S7ZQ / 12-S7 Legacy Dependency AssemblyRef Identity Canonicalization

Timestamp: 2026-07-02 08:51:27 +08:00

## Scope

This slice closes the residual 11-S7ZL canonicalization failure around legacy dependency imports and AssemblyRef
identity. It aligns the parser test with the normalized reference model: the static import keeps the canonical module
key `$math@1.2.3/ops/sum`, while the emitted AssemblyRef row uses the referenced provider assembly identity `math`.

No production code changed in this slice.

It does not claim complete metadata sweep/pruning, full trim analyzer coverage, or a complete ABI drift/deopt loop.

## Baseline / RED

- The residual WSL GCC probe from 11-S7ZL failed `zr_vm_project_import_canonicalization_test` at
  `test_project_compile_applies_dependency_import_version_range_to_assembly_ref`.
- The stale assertion searched for an AssemblyRef named `$math@1.2.3/ops/sum`.
- A temporary RED guard confirmed the function already carried a module effect for `$math@1.2.3/ops/sum` with assembly
  identity, so the lower-level metadata binding was present and the assertion was checking the wrong AssemblyRef name.

## Implementation

- `tests/parser/test_project_import_canonicalization.c` now asserts that the module effect has canonical module key
  `$math@1.2.3/ops/sum` and assembly name `math`.
- The same test asserts no AssemblyRef row is emitted under the canonical module key.
- The AssemblyRef lookup now uses `math`, preserving the existing requested/min/max version checks.
- This matches project manifest normalization for old `dependencies`: the target manifest identity, falling back to
  legacy top-level `name`, is the AssemblyRef identity.

## Test Inventory

- `tests/parser/test_project_import_canonicalization.c`
- Regression set:
  `zr_vm_project_import_provider_version_selection_test`, `zr_vm_project_import_resolver_test`,
  `zr_vm_project_manifest_normalization_test`, `zr_vm_project_import_aot_provider_runtime_test`, and
  `zr_vm_aot_c_provider_shared_library_smoke_test`

## Tooling Evidence

- WSL GCC: canonicalization passed `35 Tests 0 Failures`; the adjacent provider/version/resolver/normalization/runtime
  and provider shared-library smoke tests passed.
- WSL clang: the same focused set passed. Existing `project.c` const-qualifier warnings remain visible and are not
  introduced by this slice.
- Windows MSVC Debug: the same focused set passed; provider shared-library smoke remains `0 Failures 1 Ignored` for the
  Unix-only dynamic-loader branch. Existing `project.c` const-qualifier warnings remain visible.

## Acceptance Decision

Accepted for 11-S7ZQ / 12-S7 support: legacy dependency AssemblyRef identity now follows the normalized provider
assembly identity while preserving canonical module keys for static imports and module effects.
