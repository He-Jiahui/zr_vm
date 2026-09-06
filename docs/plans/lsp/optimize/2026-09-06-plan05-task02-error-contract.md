---
related_code:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - tests/language_server/lsp_wasm_worker_probe.js
  - tests/language_server/wasm_capability_inventory.js
  - tests/language_server/wasm_capability_inventory_test.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
implementation_files:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/wasm_capability_inventory_test.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
  - tests/acceptance/2026-09-06-lsp-wasm-worker-wiring.md
doc_type: acceptance-record
---

# Plan 05 Task 2: WASM Error Contract

The Web adapter now preserves JSON-RPC error identity across the WASM boundary.
Every native WASM error response includes a numeric `code`; invalid parameters
map to `-32602`, cancellation to `-32800`, content changes to `-32801`, and
other failures to `-32603`. A null Emscripten response pointer is an internal
error before it reaches the worker.

`responseData` returns fallback values only for successful responses whose data
is legitimately absent. Failed responses become `ResponseError` instances with
the original code, message, and optional structured data. This applies to
completion, hover, navigation, workspace, editing, formatting, diagnostics,
and all other worker request routes using the shared helper. A malformed JSON
payload still throws from the bridge parser and therefore cannot be converted
to an empty result.

The worker probe and capability inventory use a `vscode-jsonrpc` test double so
they execute the production adapters. The focused tests cover InvalidParams,
Cancelled, ContentModified, and InternalError envelopes and reject removal of
the native JSON-RPC code field. Core transport-neutral status objects,
versioned workspace edits, and native/WASM golden comparison remain later Task
2 gates.
