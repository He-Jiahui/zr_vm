# Syntax 10R M2.4 Artifact Entry And Provider Phase Acceptance

## Status

Completed 2026-07-24 22:52 +08:00. Started 2026-07-24 22:37 +08:00.

## Required Evidence

- A `.zrm` package-root import resolves through its declared default module to the exact archive `.zro` entry.
- Provider kind, provider phase, artifact entry, and public contract hash stay structured provider facts and do not
  alter the canonical module identity.
- Runtime rejects Test and CompileTool archive providers before either an AOT request or source load can consume an
  entry. Old archives with no phase remain Runtime; malformed phase values fail closed.
- An archive may not declare a default entry that is absent from its module table, whether it was built locally or
  read from an external ZIP manifest.

## Output

- `.zrm` assembly metadata publishes `providerPhase` and `publicContractHash`; archive opening keeps the legacy
  Runtime default only for a missing phase field.
- `SZrLibrary_ProjectImportProviderLocation` and `SZrLibrary_ProjectImportProviderAotLoadRequest` carry the selected
  provider facts separately from `SZrLibrary_ModuleIdentity`.
- Runtime provider source loading reads the selected archive `.zro` bytes; CompileTool and Test providers fail before
  that load boundary.

## Verification

- GCC, Clang, and MSVC Debug each completed `zr_vm_zrm_container_test` 6/6,
  `zr_vm_project_import_resolver_test` 9/9, `zr_vm_project_manifest_v2_test` 8/8,
  `zr_vm_project_manifest_normalization_test` 29/29, and `zr_vm_project_module_specifier_test` 5/5 with process exit
  0.
- GCC, Clang, and MSVC Debug each completed registered CTest `project_module_specifier|project_manifest_v2` 2/2 with
  process exit 0.
- Read-only review closed after malformed phase and missing-default-entry admission checks were added; no P1/P0
  remains.
