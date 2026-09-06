---
related_code:
  - tests/language_server/wasm_capability_inventory.js
  - tests/language_server/lsp_wasm_worker_probe.js
  - tests/language_server/wasm_capability_inventory_test.js
  - tests/language_server/lsp_native_inventory_contract.js
  - tests/language_server/stdio_protocol_inventory.js
  - tests/CMakeLists.txt
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - language_server_wasm_capability_inventory
  - language_server_wasm_capability_inventory_regression
  - language_server_stdio_protocol_inventory
doc_type: acceptance-record
---

# LSP WASM Worker Wiring

## Scope

This record accepts the Plan 00 Task 5 source-level WASM inventory slice at
commit `dfe80e8c` plus the Node runtime follow-up. The checker now maps the
CMake export list, C++ declarations/definitions, bridge calls, production
worker routes, document lifecycle and semantic-token legend. The worker probe
uses only a mock browser connection and mock WASM ABI; it does not claim that a
generated `.js`/`.wasm` asset was linked or loaded.

## Baseline

The previous string-only checker accepted eight deliberate drift fixtures:
swapped worker providers, swapped bridge exports, a missing inlay route,
duplicate/orphan routes, reordered or extra token legend entries, and a
capability without a provider. No generated WASM asset was available for linked
export validation.

## Test Inventory

- `lsp_wasm_worker_probe.js` transpiles and executes the production
  `server-worker.ts` and `wasm-bridge.ts` against a mocked connection/WASM ABI.
  It covers initialize/initialized, open/change/save/close, all 23 feature
  requests, shutdown and exit, records exact export calls, checks response
  pointer release, and checks the published capability and token legend.
- `wasm_capability_inventory_test.js` runs the real inventory CLI against the
  production fixture and the eight drift fixtures. Production plus all eight
  rejection cases pass (`9/9`).
- `lsp_native_inventory_contract.js` maps the 19 WASM-covered registry entries
  to the observed route/export report and rejects mismatched resolve flags,
  missing routes, missing exports, capability drift and token ordering drift.
- `stdio_protocol_inventory.js` embeds the WASM report after its four native
  negotiation profiles. The current profile mutation counts are `31/31/32/32`.

## Tooling Evidence

The source probe requires Node 18 or newer because the extension uses TypeScript
5.9. WSL validation installed the official Node `v22.13.1` Linux binary under
the private `.codex` validation directory and verified its SHA-256 from the
Node release `SHASUMS256.txt`. The configured CTest runtime is passed through
`ZR_VM_NODE_EXECUTABLE`; Node 12 is rejected instead of silently reducing the
probe to static checks.

Commands and key outputs:

```text
node --check tests/language_server/lsp_wasm_worker_probe.js
node --check tests/language_server/wasm_capability_inventory.js
node tests/language_server/wasm_capability_inventory_test.js
  WASM inventory regression: 9/9

<Node 22> tests/language_server/stdio_protocol_inventory.js <GCC paths>
  integrated-contract-mapped ... 31,31,32,32 ... 23
<Node 22> tests/language_server/stdio_protocol_inventory.js <Clang paths>
  integrated-contract-mapped ... 31,31,32,32 ... 23
node tests/language_server/stdio_protocol_inventory.js <MSVC paths>
  integrated-contract-mapped ... 31,31,32,32 ... 23

ctest --test-dir .codex/lsp-optimize-validation/clang-asan-current
  -R "language_server_(stdio_protocol_inventory|wasm_capability_inventory|wasm_capability_inventory_regression)$"
  100% tests passed, 3 tests passed
```

GCC and MSVC configured inventory selections also pass their two existing
inventory tests (`2/2` each). The Clang current-source build additionally
registers and passes the nine-case regression test (`3/3`).

## Results

The final report is schema version 2 with 30 runtime exports, 28 bridge calls,
23 observed worker routes and 13 ordered token types. The integrated native
runner reports 19 registry entries covered by the WASM route/export mapping and
zero native orphan/overclaim across all four negotiated profiles. Response
pointers are released for every probed route, and exit closes the mocked worker
host. The source mutation suite rejects all eight previously accepted drifts.

## Acceptance Decision

Accepted for the Plan 00 Task 5 source-level integrated inventory slice at
`dfe80e8c`. Linked WASM export-table loading, generated worker asset loading,
full native/Web behavioral parity, complete control/notification parity and
semantic acceptance remain open gates for Plans 00 and 05.
