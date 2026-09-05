---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/04-editor-feature-correctness.md
  - docs/plans/syntax/README.md
tests:
  - tests/language_server/test_lsp_capability_registry.c
  - tests/language_server/stdio_navigation_capabilities_smoke.js
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_workspace_folders_smoke.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
doc_type: milestone-record
---

# Navigation Alias Capability Withdrawal

## 状态与产出记录

| Started | Completed | Status | Completed items | Evidence |
| --- | --- | --- | --- | --- |
| 2026-09-05 16:59 +08:00 | 2026-09-05 17:22 +08:00 | completed | Native aliases withdrawn; exact local navigation, MethodNotFound and browser absence verified; module docs and independent reviews complete. | Three-toolchain focused results and separate baseline failures below. |

## Required Contract

Declaration and type definition have distinct semantic targets. Their current
core wrappers still delegate to `GetDefinition`, so native initialize and
dispatch must withdraw them until Plan 04 implements their contracts. Their
registry entries cannot continue to describe implemented native/WASM providers.
Explicit withdrawn requests must return the exact MethodNotFound envelope.

Native definition remains a required feature. Existing native implementation
consumes canonical `ImplementationsOf(SymbolId)` and native folder changes update
the workspace, so the historical blanket withdrawal instruction is superseded
only for these implemented paths. Add exact protocol implementation target/range
checks with unrelated same-name members, and replay actual folder updates.

## RED And Baseline

The initial focused fixture fails because `declarationProvider` is advertised.
The expanded positive fixture discovers that the existing reaching-write
definition example returns `[]`. The unchanged C regression binary also fails
all three cases: simple and branch reaching writes return zero definitions,
and the missing-source case cannot prepare its expected query. These remain
Plan 03 Tasks 7/8 and Plan 04 Task 1 defects, with active symbol-projection and
type-query changes still owned by their existing sessions. No semantic code or
these failing C assertions is changed here.

The final fixture first proves the supported Device declaration token definition
and complete Device/Sensor implementation set with exact ranges, excludes the
unrelated Other class with its same-name read method, and confirms no diagnostics.
Those positive checks pass on the old frozen binary before its expected
declarationProvider RED. This leaf's positive preservation proof does not stand
in for the pending reaching-write failures.

## Completed Output And Verification

- Registry descriptors are removed for both unsupported aliases; count is 31.
- Native initialize flags, dispatch routes, stdio wrappers/prototypes and four
  unused field/method constants are removed. The internal core aliases still
  require real semantic implementation in Plan 04.
- The new focused CTest uses the shared protocol client, checks both exact
  MethodNotFound envelopes and the full positive target set/ranges. Main smoke
  and protocol capability snapshot no longer require the aliases.
- Browser callback tests assert that both capabilities and handlers are absent.
- Native workspace-folder capability remains implemented; its actual updates
  are rerun on all three toolchains.

| Verification | Observed result |
| --- | --- |
| GCC 11.4 Debug shared build | stdio, registry, lifecycle exit 0 |
| GCC focused CTest | 8/8 pass, 8.78 s |
| Clang 14 Debug shared build | stdio, registry, lifecycle exit 0 |
| Clang focused CTest | 7/8 pass, 10.08 s; only prior cancel-known timing issue fails |
| MSVC 19.44 Debug static build | stdio, registry, lifecycle exit 0 |
| MSVC focused CTest | 7/8 pass, 13.95 s; only cancel-known timing issue fails |
| Browser actual callback tests | 8/8 pass |
| Extension full unit | 39/39 pass |
| Scoped diff whitespace check | exit 0, only repository CRLF advisories |

The three-toolchain passing set includes registry, initialize inventory, resolve
smoke, new navigation smoke, document sync, workspace folders and lifecycle.
GCC also passes protocol conformance. Clang/MSVC run all protocol cases but
cancel-known exceeds the old 3000 ms deadline. A probe on the current Clang
binary reproduces the cause of that deadline failure: the preceding 2048-class
didOpen analysis occupies 3897.40 ms; diagnostics and the correct `-32800`
response arrive 0.33 ms apart. The harness charges preparation time to the
request timeout. This remains explicitly pending under Plan 00 Task 3 / Plan 01
Task 4; no timeout or frozen cancellation budget is changed in this leaf.

Validation directories and toolchain configurations are the exact exported
source/build directories in the [resolve record](2026-09-05-plan00-task04-sub01-identity-resolve.md).
The overlay is ce04018c plus only this leaf's 13 code/test paths. Native commands:

```text
cmake --build <build-dir> --target zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_registry_test zr_vm_language_server_stdio_server_lifecycle_test --parallel 6
ctest --test-dir <build-dir> --output-on-failure -R ^language_server_(lsp_capability_registry|stdio_(protocol_inventory|resolve_capabilities_smoke|navigation_capabilities_smoke|protocol_conformance|document_sync_conformance|server_lifecycle|workspace_folders_smoke))$
node --test zr_vm_language_server_extension/test/serverCapabilities.test.js
npm --prefix zr_vm_language_server_extension run test:unit
```

Spec and independent quality review approve the code/test contract. The record
now states the known reaching-write failures, and module documentation explicitly
distinguishes borrowed URI storage from copied ranges. Module documentation is
`docs/cli-and-tooling/lsp-navigation-capability-boundary.md`, linked from its
category index; it records lifetime, exactness, ownership and the remaining
core/provider obligations.

## Remaining Gates

This withdrawal leaf is complete; the commit containing this record owns only
its explicit code/test/documentation paths. Resolve its source identity with
`git log -1 --format=%H -- <this-record>`; the execution summary links the commit.
The two aliases require real Plan 04 implementations before re-publication.
The reaching-write defects, protocol setup timeout and all-provider/browser
matrices remain open parent-plan gates and are not waived by this leaf.
