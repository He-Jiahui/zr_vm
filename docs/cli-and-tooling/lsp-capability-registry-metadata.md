---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_capability_registry.h
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - tests/language_server/lsp_capability_inventory_probe.c
  - tests/language_server/wasm_capability_inventory.js
  - tests/language_server/lsp_native_inventory_contract.js
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
  - tests/CMakeLists.txt
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/test_lsp_capability_registry.c
  - tests/language_server/stdio_protocol_inventory.js
  - tests/language_server/lsp_native_inventory_contract.js
  - tests/language_server/lsp_native_inventory_mutations.js
  - tests/language_server/lsp_wasm_worker_probe.js
  - tests/language_server/wasm_capability_inventory_test.js
  - tests/language_server/stdio_color_capability_smoke.js
doc_type: module-detail
---

# LSP Capability Registry Metadata

## Implementation Ownership

The registry describes protocol capabilities using C-only immutable metadata.
Each descriptor names its protocol method, client capability path, minimum
protocol version, runtime coverage, implementation ownership and protocol test.
Native adapter names are the actual `handle_*` functions; WASM names are the
actual `wasm_ZrLsp*` exports. Test IDs identify registered CTest entries.

`CORE` ownership requires a nonempty core entry point. `NATIVE_ADAPTER`
ownership describes protocol facilities without a dedicated public core API,
such as position negotiation and workspace folder notifications. That ownership
requires native-only coverage and an explicitly null core entry point. It does
not authorize an adapter to infer semantic facts.

The September 5 metadata baseline has 30 descriptors: 19 shared native/WASM
providers and 11 native-only entries. Seven entries are adapter-owned. These
counts describe the existing surfaces and are not full behavioral acceptance.

## Runtime And Version Invariants

An enabled native runtime requires a nonempty adapter; an enabled WASM runtime
requires a nonempty export. Disabled runtime fields must be explicitly null,
including when a descriptor is adapter-owned. Empty strings do not represent
absence. Unknown runtime bits, missing ownership and contradictory ownership
are rejected.

Resolve coverage is separate from base-provider coverage, as described in
[LSP Resolve Capability Contract](lsp-capability-resolve-contract.md). Only
material resolution can be publishable. Inline completion carries minimum
version 3.18 and experimental metadata against the stable 3.17 baseline.
`colorProvider` is absent. No compiler-owned color type or expression facts
justify treating arbitrary hex strings as editable color values. Both
`textDocument/documentColor` and `textDocument/colorPresentation` are unsupported
and return MethodNotFound, including for clients that request color support.

`HasRequiredMetadata` validates structure and relationships.
`IsDescriptorPublishable` additionally rejects identity-only resolution and
non-experimental 3.18 entries. Neither function checks the client's negotiated
capabilities, linked symbol existence, actual WASM assets or semantic accuracy.
The name does not authorize unconditional initialize publication. Runtime
negotiation and inventory verification must supply those additional checks.

## Lifetime And Exactness

`At` and `Find` return borrowed pointers into a static, process-lifetime array.
All descriptor strings are borrowed literals and must not be modified or freed.
The registry does not capture a document, provider generation or semantic
snapshot, and it does not create `SymbolId`, `TypeId` or `PlaceId` identities.
Synthetic descriptors passed to validation remain owned by their caller.

Entry-point strings are identifiers for auditing, not function pointers or
dynamic lookup instructions. Their presence alone cannot prove a successful
query or a usable export. Cross-provider identity and stale snapshot behavior
remain obligations of the compiler/query and adapter contracts.

## Validation And Remaining Work

The C unit suite compares the current registry with audited implementation
names and runtime coverage, exercises native-only and WASM-only descriptors,
and rejects conflicting ownership, missing or extraneous runtime fields,
invalid resolve masks and unsupported version claims. Targeted assertions reject
registration of the withdrawn color scanner. The color protocol regression
checks both client profiles, exact error envelopes, string/identifier hover
payloads and definition targets, and the absence of comment references.

Plan 00 Task 2 Sub02 now connects the compiled registry to native initialize JSON,
production dispatch and CTest registration with a machine-checked inventory.
The probe reports 30 descriptors, 43 native routes and zero native orphan or
overclaim for four negotiated profiles. It also compares the compiled 13-entry
semantic-token legend and rejects malformed boolean capability shapes through
deliberate mutations. The three control descriptors are labeled metadata-only;
their notification behavior is not inferred from names.

The inventory passes GCC/Clang/MSVC focused 14/14. Its CTest runner receives an
absolute executable discovered by `find_program`, so it does not rely on the
interactive test process's `PATH`. The native result is recorded in
[Plan 00 Task 2 Sub02](../plans/lsp/optimize/2026-09-05-plan00-task02-sub02-compiled-native-inventory.md)
and the [acceptance record](../../tests/acceptance/2026-09-05-lsp-native-capability-inventory.md).

This does not prove that a named WASM export is linked into a current asset or
that the worker registers matching handlers. Plan 05 must replace hand-maintained
runtime declarations with the shared contract and validate actual native/Web
behavior. A registered broad smoke test can still fail before reaching a
provider; its ID is provenance, not proof of coverage.

The independent `wasm_capability_inventory.js` checks the CMake export list,
WASM declarations/definitions, bridge `ccall` names, and a source-level execution
of the production worker and bridge against a mock connection/WASM ABI. The probe
executes initialize, document lifecycle, all 23 worker request routes, shutdown
and exit, then records each observed export and the 13-type semantic-token legend.
It reports 30 exports, 28 bridge calls and 23 observed worker routes. With generated
`.js` and `.wasm` paths it additionally checks the actual `WebAssembly.Module.exports`
table; no linked asset is claimed until that mode runs successfully. The mocked
execution is explicit in the report (`schemaVersion: 2`, `mockedWasm: true`,
`workerAssetLoaded: false`).
The source probe requires Node 18 or newer because it loads the extension's
TypeScript 5.9 compiler; CTest must pass that runtime through
`ZR_VM_NODE_EXECUTABLE`.

`stdio_protocol_inventory.js` now invokes that WASM checker after its four native
negotiation profiles and embeds the result under `wasm` in one machine-readable
report. The combined status is `integrated-contract-mapped` only when every native
profile and the WASM source mapping pass. GCC, Clang and MSVC CTest runs cover both
inventory targets. This joins the registry/initialize/dispatch/CTest audit without
treating source execution as proof that a generated WASM asset loaded;
`linkedAssetChecked` remains explicit. `wasm_capability_inventory_test.js` keeps
the checker honest with nine production/mutation fixtures, including inlay route,
provider/export, duplicate/orphan route and token legend drift.

The cross-toolchain evidence is recorded in
[WASM worker wiring acceptance](../../tests/acceptance/2026-09-06-lsp-wasm-worker-wiring.md).

Implementation and verification evidence is recorded in
[Plan 00 Task 2 Sub01](../plans/lsp/optimize/2026-09-05-plan00-task02-sub01-registry-metadata.md)
and [Task 2 Sub02](../plans/lsp/optimize/2026-09-05-plan00-task02-sub02-compiled-native-inventory.md).
