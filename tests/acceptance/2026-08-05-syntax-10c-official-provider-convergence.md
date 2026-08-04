---
scope:
  - Syntax 10C M4 official native provider convergence
  - Syntax 10C M5 canonical consumers and migration
status: accepted
last_verified: 2026-08-05
---

# Syntax 10C Official Provider Convergence

## Scope

This acceptance closes Syntax 10C without reopening 10R or 10F. It verifies:

- the frozen 25-module N0-N3 inventory and provider-phase rules;
- the real task, iteration, container, pooling, thread, reflection, compile,
  testing and Debug owner descriptors;
- the one-shot `debug` to `zr.debug` cutover with no runtime alias;
- the 06A machine edit for a bare Debug import; and
- canonical ModuleIdentity consumption by project imports, reflection, Debug
  and LSP.

The unbuilt `zr_vm_lib_task` directory is intentionally excluded by the Syntax
12 product graph. The accepted `zr.task` provider is the descriptor registered
by `ZrCore_TaskRuntime_RegisterBuiltins`; this acceptance does not count the
excluded directory as a second provider or as evidence.

## Baseline

The official inventory already reserved `zr.debug` and rejected a descriptor
named `debug`, while `zr_vm_lib_debug` still emitted `debug` in both production
descriptors, type-hint JSON and its hook lookup. The original Debug test showed
1/8 passing and seven registration failures.

After the canonical expectations were added, the RED result was 1/9 Debug
tests passing and one new migration failure in a 5-test migration target. A
second RED case proved that a formatted import containing comments was not yet
recognized. The initial owner aggregation then proved that several real
Runtime descriptors had no `publicContractHash`.

## Implementation

- Both trusted and sandboxed Debug descriptors now use only `zr.debug`, declare
  Runtime phase and publish `zr.debug:v1:lua-aligned-debug-surface`.
- The hook-retention lookup and type-hint schema use the same canonical name.
- Task, iteration, container, pooling and thread descriptors explicitly publish
  their official Runtime phase and a versioned public contract hash.
- `test_official_provider_convergence.c` links the real owner libraries and
  validates inventory membership, phase, contract hash, descriptor admission
  and unique type ownership. Reflection and task are read from their actual
  registry entrypoints; no placeholder descriptor is accepted.
- The migration frontend emits a machine-applicable edit only for the literal
  payload of a real `import("debug")`. It skips whitespace/comments, ignores
  comments and quoted examples, and is idempotent after application.
- Module-specifier matching lives in
  `legacy_migration_module_specifier.c`; the already oversized migration
  orchestrator only appends the structured plan item.

No runtime alias, compatibility registration, parser grammar branch or
per-consumer module-name splitter was introduced.

## Test Inventory

| Target | Assertions |
|---|---:|
| `zr_vm_official_provider_convergence_test` | 9 |
| `zr_vm_debug_library_test` | 9 |
| `zr_vm_percent_test_migration_test` | 5 |
| `zr_vm_project_module_specifier_test` | 5 |
| `zr_vm_project_import_canonicalization_test` | 35 |
| `zr_vm_task_job_scheduler_test` | 5 |
| `zr_vm_enumerator_protocol_test` | 5 |
| `zr_vm_generational_pool_test` | 14 |
| `zr_vm_thread_runtime_test` | 25 |
| `zr_vm_testing_assertions_test` | 12 |
| `zr_vm_reflection_type_surface_test` | 21 |
| `zr_vm_language_server_lsp_project_features_test` | 57 |

The first eleven Unity targets total 145 assertions. The LSP harness contributes
57 project-feature checks, for 202 checks per toolchain.

Boundary and failure coverage includes an absent module before registration,
both trusted and sandboxed descriptors, old-name lookup rejection, old
descriptor rejection, wrong phase, duplicate official provider, malformed
reflection roles, contract-only reflection non-materialization, owner TypeDef
exclusivity, trivia inside an import expression, comment/string false positives,
and migration replanning with zero edits.

## Tooling Evidence

The same focused target list was configured and built in:

- GCC 11.4.0: `/home/hejiahui/.cache/zrvm-s08m3-gcc`
- Clang 14.0.0: `/home/hejiahui/.cache/zrvm-s08m3-clang`
- MSVC 19.44.35207 with Ninja:
  `C:\Users\HeJiahui\AppData\Local\Temp\zrvm-s08m3-msvc`

Linux builds used `cmake --build <dir> --target <12 targets> -j2`, followed by
direct execution of each binary. MSVC used the repository `using-vsdevcmd`
environment importer, the identical target list and the identical direct
binary replay.

No debugger or sanitizer was needed: all observed failures were deterministic
descriptor admission or migration-plan assertion failures, and the three
compiler matrix directly exercised the changed paths.

## Results

- GCC: 202/202
- Clang: 202/202
- MSVC: 202/202
- Combined: 606/606
- `git diff --check`: no whitespace errors
- Production search: no bare Debug descriptor, type-hint owner, hook lookup or
  source import remains outside the intentional rejection/migration tests.

GCC and Clang emitted no warning from the focused build. MSVC retained existing
`/W3` to `/W4` override warnings, the existing Debug forward-const/unreachable
warnings, and older test conversion/const warnings; no new failure or warning
originated in the extracted module-specifier matcher.

## Review Findings Closed

1. Product and inventory disagreed on `debug`; all product paths now use
   `zr.debug` and the old name remains rejection-only.
2. Owner tests previously validated only compile/testing and parser attribute
   schemas; they now validate every in-scope real descriptor and unique owner.
3. Runtime owner descriptors lacked public contract hashes; each in-scope owner
   now publishes a versioned value.
4. The first migration matcher missed legal trivia; the structured matcher now
   skips whitespace and comments without scanning quoted/comment text.
5. Adding the matcher directly to a 1900-line file violated the repository
   modularization gate; it was extracted into a cohesive migration component.

## Acceptance Decision

Accepted. Syntax 10R, 10F and 10C are now all promoted, so Syntax 10 is a proven
06B prerequisite. This does not complete the root Syntax redesign: 06B still
owns repository-wide input promotion and cleanup, and 07B still owns current
reference-fixture promotion.
