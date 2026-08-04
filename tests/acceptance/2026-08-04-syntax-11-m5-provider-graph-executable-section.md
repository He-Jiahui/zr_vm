---
scope: Syntax 11 M5 versioned executable section and transitive provider graph
status: proven
date: 2026-08-04
toolchains:
  - WSL GCC 11.4
  - WSL Clang 14.0
  - MSVC 19.44 x64 Debug
---

# Syntax 11 M5 provider graph and executable section acceptance

## Accepted contract

- A CompileTool ZRM carries a dedicated `zr.compile-tool-executable/v1`
  manifest section using `zr.source/utf8-v1` entries under
  `compile-tools/<module>.zrs`.
- Packaging supplies the ordinary `.zro` module and executable `.zrs` source
  as distinct inputs with independent hashes; missing executable input rejects.
- The compiler consumes only that phase-isolated, hash-authenticated section.
  A missing/incomplete section, a mismatched hash, or the section on a Runtime
  or Test archive fails closed.
- Provider-to-provider `buildDependencies` imports traverse the project-owned
  manifest/lock graph, deduplicate canonical module identities, and never enter
  the runtime dependency graph.
- Loading-state ancestry reports a deterministic cycle such as
  `@cyclea -> @cycleb -> @cyclea`. Missing transitive locks and cycles restore
  all bindings, aliases, imported modules, and provider records created by the
  failed scope.

## Test-first evidence

The initial transitive provider case failed at the former
`compiletool.provider.transitive_not_promoted` boundary. Removing that boundary
first exposed recursive loading across nested compiler states. The provider
ancestry/LOADING state made the real two-archive cycle finite and diagnostic;
the final helper-alias case then drove alias-qualified compile-time calls.

The resulting focused suites pass with zero failures:

- `zr_vm_zrm_container_test`: 8/8.
- `zr_vm_compile_tool_project_import_test`: 9/9.
- `zr_vm_comptime_runtime_contract_test`: 14/14.
- `zr_vm_compile_time_test`: 69/69.
- `zr_vm_comptime_contract_test`: 2/2.
- `zr_vm_attribute_contract_test`: 3/3.
- `zr_vm_declaration_transform_contract_test`: 6/6.

GCC also passed `zr_vm_compiler_integration_test`,
`zr_vm_cli_project_incremental_test`, `zr_vm_project_manifest_v2_test`, and
`zr_vm_language_server_lsp_advanced_editor_features_test`. The five primary
ZRM/provider/comptime/compile-time/LSP targets were rebuilt and replayed under
Clang and MSVC as an independent portability check.

## Decision

The remaining Gate 11 M5 executable-artifact and transitive-provider gaps are
closed. Together with the previously accepted M1-M4 and M5 consumer/cache/
migration records, Syntax 11 is promoted.
