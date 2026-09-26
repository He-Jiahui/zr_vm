---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_language_server_extension/src/browser/worker/document-sync.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - zr_vm_language_server_extension/src/browser/worker/wasm-response.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/structure.ts
  - zr_vm_language_server/src/zr_vm_language_server/protocol/lsp_capability_registry.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_language_server_extension/src/browser/worker/document-sync.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - zr_vm_language_server_extension/src/browser/worker/wasm-response.ts
plan_sources:
  - .codex/plans/20260926-lsp-experience-repair.md
tests:
  - zr_vm_language_server_extension/test/webDocumentSync.test.js
  - zr_vm_language_server_extension/test/serverDiagnostics.test.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
  - zr_vm_language_server_extension/test/webStructure.test.js
  - tests/language_server/lsp_wasm_worker_probe.js
  - tests/language_server/wasm_capability_inventory_test.js
doc_type: module-detail
---

# Web document synchronization and capability scope

`DocumentSyncStore` owns the text/version snapshot for each open document. Backend
mutations run in URI order; the JS snapshot commits only after a successful WASM
response. Invalid notifications or failed updates mark the document desynchronized
and preserve the last committed snapshot. An incremental edit cannot recover that
state. Recovery requires a newer single full replacement without `range` or
`rangeLength`, or closing and reopening the document. A failed initial update can
also recover through a full replacement newer than its open version.

Versions and position components are nonnegative LSP integers. Web's WASM ABI uses
signed 32-bit versions. Edits validate each range against the preceding temporary
result; the whole notification commits atomically. Position conversion recognizes
LF, CR and CRLF, excludes line terminators from line content, and rejects positions
inside a UTF-16 surrogate pair. `rangeLength`, when present, must equal the replaced
UTF-16 length, including crossed line terminators. Invalid Unicode replacement text
is rejected before UTF-8 encoding. Duplicate opens are ignored. `didSave` never
updates text or repeats a backend update with the same version.

Each open creates a generation, and each change advances its revision immediately.
Document requests wait for preceding mutations, require a synchronized open
snapshot, and reject an obsolete result with LSP `ContentModified` (-32801).
Push diagnostics use the same generation/revision fence and backend result ID;
version and generation both participate in duplicate suppression. Close invalidates
outstanding work immediately and publishes empty diagnostics before awaiting the
queued backend close. Reopening at the same version cannot reuse the old generation.

Web exposes document features for opened files and the built-in declaration
document endpoint. It has no workspace filesystem bridge in this iteration.
Workspace symbols, workspace diagnostics and the project-module request are not
registered; initialize advertises document diagnostics without workspace or
inter-file support. The C capability registry marks workspace symbols native-only,
while existing WASM exports remain available for ABI compatibility.

No-match editor requests use normal LSP empty results. WASM definition,
document-highlight, inlay-hint and code-action adapters therefore return
successful empty arrays when the native query has no symbol or edit; malformed
parameters and allocation failures remain error responses. This prevents a caret
on whitespace from surfacing an internal-error toast in Web.

`zr-decompiled:` documents are editor-only declaration projections. The worker
does not synchronize them into the WASM source index, so diagnostics and feature
requests complete with empty results while the virtual document provider remains
responsible for loading its displayed text.

The Web host sends no selected-project notification or unsupported file-watcher
synchronization. Current-file and built-in structure views remain available. The
project module view displays an explanation that project indexing is unavailable
in VS Code Web and directs the user to open a Zr file for language features.

Tests execute production worker handlers with controlled asynchronous backends,
covering invalid and reordered edits, Unicode and newline boundaries, backend
failure, failed-open recovery, save invariance, stale query/diagnostic suppression,
and serialized close/reopen. The inventory probe separately executes production
worker/bridge adapters and checks that protocol routes agree with capability
metadata and linked exports.

The WASM target compiles the parser sources directly and includes the parser's
private root alongside its component include directories. Debug compilation uses
`-O0`; Release retains `-O2`. Emscripten uses the existing portable switch runtime
dispatcher. The computed-goto dispatcher creates costly irreducible regions for
the WASM backend; native GCC/Clang continue to use their existing dispatch table.
This platform selection changes the implementation of dispatch, not instruction
semantics or exported LSP/WASM APIs.
