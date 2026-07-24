# Syntax 10R M1 Specifier Foundation Implementation Plan

Design source: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`,
10R M1 Specifier foundation.

## Scope

Implement the independent M1 substrate only:

1. Add structured ModuleDomain, ModuleIdentity, and ModuleSpecifier public data.
2. Parse official native, registered native, workspace, relative, alias, single-segment package, and canonical
   `file:` source spellings.
3. Resolve relative spellings from an already canonical Workspace or Package identity.
4. Preserve the existing 06A project import resolver as a migration adapter.

## TDD Sequence

1. Add `tests/library/test_project_module_specifier.c` with expected domain, normalization, relative, package,
   URI, and rejection cases; add its focused CTest registration.
2. Run the target before implementation and verify missing ModuleSpecifier API linker failures.
3. Add `project_module_specifier.c` as a separate module because the legacy resolver already exceeds the
   repository source-size threshold.
4. Re-run the new focused test and existing project import resolver regression.
5. Validate GCC, Clang, and MSVC, then update module docs and the acceptance record.

## Exact Write Set

- `zr_vm_library/include/zr_vm_library/project.h`
- `zr_vm_library/src/zr_vm_library/project/project_module_specifier.c`
- `tests/library/test_project_module_specifier.c`
- `tests/CMakeLists.txt`
- `docs/module-system/module-specifier-identity.md`
- `docs/module-system/index.md`
- `tests/acceptance/2026-07-24-syntax-10r-m1-specifier-foundation.md`
- `docs/plans/syntax/10-native-ffi-module-package/*.md`

## Explicit Non-Goals

M1 does not parse `.zrp` v2, expand manifest aliases, choose providers, open `file:` targets, validate artifact
identity, record locks, alter TypeId generation, or replace the 06A legacy resolver. Those are M2 or later
owner work and remain outside this commit.
