# Syntax 10R M2.2 V2 Declaration Acceptance

## Status

Completed 2026-07-24 21:43 +08:00.

## Required Evidence

- `zr_vm_project_manifest_v2_test` validates structured v2 aliases, package exports, and dependencies plus
  failure boundaries for legacy fields, invalid roots, recursion, undeclared package targets, and ambiguous
  sources.
- `zr_vm_project_manifest_normalization_test` preserves the v1 migration reader.
- `zr_vm_project_module_specifier_test` preserves M1 canonical parsing used by the declaration reader.
- GCC, Clang, and MSVC ran the same focused set: v2 5/5, v1 normalization 29/29, M1 module specifier 5/5,
  and `project_manifest_v2|project_module_specifier` CTest 2/2.
- File alias suffixes preserve directory and `.zrp` container behavior, while `.zr` and `.zrm` terminal targets
  reject an appended logical suffix.

## Boundary

This acceptance does not cover canonical v2 writer/lock projection, `.zrm` default entry selection, or provider
phase selection. Those remain Syntax 10R M2.3 and M2.4.
