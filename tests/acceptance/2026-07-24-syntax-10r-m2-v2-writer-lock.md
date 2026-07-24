# Syntax 10R M2.3 V2 Writer And Lock Acceptance

## Status

Completed 2026-07-24 22:22 +08:00. Started 2026-07-24 21:43 +08:00.

## Required Evidence

- Canonical v2 writer must emit deterministic alias/export/dependency ordering and round-trip through the v2 reader.
- v1 migration inputs and machine-local locators must be rejected by the publisher writer, including `file:`/drive
  spellings, empty-authority URI forms, and localhost/IPv4/IPv6 loopback authorities.
- Lock output must contain resolved version, content hash, transitive identity, and structured provider kind without
  serializing manifest source locators or local cache paths.

## Output

- Canonical writer and independent dependency-lock projection are public library APIs with deterministic JSON output.
- The writer accepts `registry` package IDs and external network authorities, including external bracketed IPv6, while
  rejecting all local/loopback locator forms before serialization.
- Review closed without P1/P0 after the authority and IPv6 fail-closed tests were added.

## Verification

- GCC, Clang, and MSVC Debug: `zr_vm_project_manifest_v2_test` 8/8, `zr_vm_project_manifest_normalization_test`
  29/29, and `zr_vm_project_module_specifier_test` 5/5; each process exited 0.
- GCC, Clang, and MSVC Debug CTest: `project_manifest_v2|project_module_specifier` 2/2, exit 0.
