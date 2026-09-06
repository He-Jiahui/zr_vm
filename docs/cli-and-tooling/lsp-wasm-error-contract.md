---
related_code:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - tests/language_server/lsp_wasm_worker_probe.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
implementation_files:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
  - docs/plans/lsp/optimize/2026-09-06-plan05-task02-error-contract.md
tests:
  - tests/language_server/wasm_capability_inventory_test.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
doc_type: module-detail
---

# WASM LSP Error Contract

WASM exports return a small JSON envelope with `success`, optional `data`,
optional `error`, and a JSON-RPC `code` for failures. The code is selected at
the export boundary from the shared language-server error constants. Invalid
parameters retain `-32602`; cancellation and content-modified conditions
retain their LSP-specific codes; allocation, serialization, and other
unexpected failures use `-32603`.

`ZrWasmBridge` preserves the envelope and assigns `-32603` when Emscripten
returns a null response pointer. JSON parsing failures remain thrown errors.
The browser worker's shared `responseData` helper returns a fallback only after
`success` is true. On failure it throws `ResponseError(code, message, data)` so
the browser language client receives a JSON-RPC error instead of a fabricated
empty result. This makes `null` and empty arrays valid only when the core
operation explicitly succeeded with that result.

The worker wiring probe executes the production worker and bridge with only the
browser connection, JSON-RPC error class, and WASM ABI replaced. It verifies
that error codes and structured data survive each request handler, while the
WASM inventory mutation suite rejects a response serializer that drops the
numeric code.
