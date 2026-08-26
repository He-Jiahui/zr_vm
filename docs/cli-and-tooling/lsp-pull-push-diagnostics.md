---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/lsp_diagnostic_store.h
  - zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/test_stdio_server_lifecycle.c
  - tests/language_server/stdio_diagnostics_generation_smoke.js
  - tests/language_server/stdio_smoke.js
  - zr_vm_language_server_extension/test/serverDiagnostics.test.js
doc_type: module-detail
---

# LSP Pull/Push Diagnostics

## Scope

The diagnostic store owns pull-diagnostic identity for native stdio and the
browser WASM worker. Adapters serialize reports and notifications; they do not
construct a result identity from source text, member names, or a second
diagnostic model.

## Result Identity

`ZrLanguageServer_LspDiagnosticStore_BuildResultId` formats a stable identity
from the semantic snapshot's document, project, provider, semantic, and
dependency generations plus a sorted structured payload hash. The payload has
the complete diagnostic range, severity, descriptor/code, message, related
information, and fixes. Related information and fixes are sorted before the
top-level diagnostic list so producer append order cannot alter an equivalent
report identity.

When no semantic snapshot applies, such as an opened `.zrp` document, the
store obtains the file-version content generation and provider generation. A
report with zero diagnostics still receives a valid identity. This fallback is
document-backed; it is not a synthetic empty or fixed resultId.

## Native Protocol

`textDocument/diagnostic` validates `textDocument.uri` before querying. Invalid
parameters are JSON-RPC `-32602`, and a matching `previousResultId` alone may
produce `unchanged`.

Workspace reports begin with every URI in the indexed project source graph and
then add opened overlays that the graph does not contain. Duplicate canonical
URIs collapse through the shared URI-equivalence API. Unopened reports set
`version` to JSON null. Index traversal and report generation check the active
request cancellation predicate.

Push diagnostics retain a separate cache entry per URI. A notification is
suppressed only when both the canonical resultId and the open document version
match. Pull requests do not populate this cache, so a pull response cannot
prevent the first push for the same editor generation.

The stdio server owns the push-cache lifetime. Diagnostic request handling owns
lookup and mutation, while `stdio_server.c` performs cache destruction as part
of server teardown. The destructor is file-local to the server so the focused
lifecycle target can validate complete teardown without linking unrelated
diagnostic protocol handlers.

## Browser Boundary

The browser worker obtains both document and workspace reports from the WASM
bridge. It has no TypeScript text hash or open-document-only workspace loop.
Its version-aware push cache follows the same rule as native. The exported C++
entry points call the C diagnostic store, preserving resultId equality across
the two adapters.

## Validation

`stdio_diagnostics_generation_smoke.js` freezes invalid-parameter rejection,
stable unchanged reports, nonsemantic empty reports, unopened workspace source
coverage, and dependency-driven importer identity changes. The general stdio
smoke covers push/pull coexistence, latency, and process memory. Browser static
coverage rejects a reintroduced TypeScript identity implementation and confirms
the two WASM bridge calls. `test_stdio_server_lifecycle.c` covers repeated and
fault-injected server teardown, including release of the diagnostic push cache.
