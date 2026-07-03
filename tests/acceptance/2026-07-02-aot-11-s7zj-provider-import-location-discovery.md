# 11-S7ZJ / 12-S7 Provider Import Location Discovery

Time: 2026-07-02 05:58:12 +08:00

## Scope

This slice closes the first standalone provider import-path discovery step for AOT metadata export consumption.
It does not load the provider module into runtime, choose among multiple candidate versions, publish compacted
tokens back into a `.zrp` file, or run a full trim analyzer.

## RED

- `tests/library/test_project_import_resolver.c` was extended to call a new provider-location API from both
  `.zrp` project references and `.zrm` assembly references.
- The focused WSL GCC build failed because `SZrLibrary_ProjectImportProviderLocation` and
  `ZrLibrary_Project_ResolveImportProviderLocation()` did not exist.

## GREEN

- `zr_vm_library/include/zr_vm_library/project.h` now exposes `SZrLibrary_ProjectImportProviderLocation`.
- `zr_vm_library/src/zr_vm_library/project/project_import_provider_location.c` resolves an import specifier to:
  - the canonical `$alias@version/module` provider module key,
  - assembly identity and requested/min/max version strings from the manifest reference,
  - `.zrm` archive entry pointers for assembly-container providers, or source/binary/intermediate paths for
    project-manifest providers.
- The implementation reuses the existing resolver, dependency version-range query, and `.zrm` module-entry lookup
  instead of duplicating manifest parsing rules.

## Verification

- WSL GCC direct `zr_vm_project_import_resolver_test`: 9/0.
- WSL clang direct `zr_vm_project_import_resolver_test`: 9/0.
- Windows MSVC Debug direct `zr_vm_project_import_resolver_test`: 9/0.
- The focused target is not registered as an individual CTest in the current build trees.
