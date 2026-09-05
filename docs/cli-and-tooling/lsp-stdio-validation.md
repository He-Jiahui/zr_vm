---
related_code:
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_document_sync_conformance.js
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/test_stdio_server_lifecycle.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/stdio/stdio_document_content.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
  - zr_vm_language_server/stdio/stdio_position_encoding.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/stdio/stdio_document_content.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
  - zr_vm_language_server/stdio/stdio_position_encoding.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_server.c
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_resolve_capabilities_smoke.js
  - tests/language_server/stdio_document_sync_conformance.js
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/test_stdio_server_lifecycle.c
doc_type: module-guide
---

# LSP Stdio Validation

## Resolve Capability Validation

`stdio_resolve_capabilities_smoke.js` uses the shared protocol client to check
complete initial document links, code lenses, inlay hints and workspace symbols.
It rejects identity-only resolve advertisements and requires the complete
`-32601 Method not found` envelope for each withdrawn method. Native code-action
resolve is tested against current, stale and refreshed document snapshots,
including client version zero. See the
[resolve contract](lsp-capability-resolve-contract.md) for runtime masks and
the Web worker's validation boundary.

## Process Peak Budget

`tests/language_server/stdio_smoke.js` measures the language-server child
while it is alive, after the full protocol workload and after the `shutdown`
response but before the `exit` notification. Linux consumes `/proc/<pid>/status`
`VmHWM`; Windows consumes `Get-Process ... PeakWorkingSet64`. Both are OS
high-water metrics for that child process, not cache-storage estimates.

The default `ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES` limit is 512MiB. A supplied
value must be a positive safe integer. The smoke prints the peak and limit in
bytes and MiB, and fails when the peak exceeds the limit. Unsupported host
platforms fail instead of silently reporting an approximation.

## Lifecycle Races

The stdio reader thread may linearize a `workspace/diagnostic` request before
the immediately following cancel, change, or close notification. The churn
test accepts only a lifecycle error when the later input was observed first,
or a workspace report containing the exact document version at the request
linearization point. It rejects a mixed or newer snapshot. A separate request
cancellation case retains the strict 50ms `RequestCancelled` budget.

## Deterministic Teardown

`SZrStdioServer` owns the native LSP runtime. Construction creates the global
state, initializes its registry, creates the LSP context and request registry,
then initializes the request-input synchronization state. `Start` alone owns
reader creation. The input state retains the Win32 `HANDLE` or `pthread_t`; it
never closes or detaches that handle at creation time.

`Free` uses one ordered path for ordinary shutdown and each construction fault:

1. set the reader stop flag and wake waiting consumers;
2. join the reader after the protocol `exit` notification or EOF ends its
   blocking read;
3. delete each queued JSON message and release the request registry;
4. destroy the input condition variable and mutex;
5. release URI and semantic-token caches, then the LSP context and global
   state.

The reader stops naturally after a valid `exit` notification or input EOF.
The stop flag ensures no additional frame is read after a wake-up; the server
does not cancel a thread asynchronously while it may own C runtime input state.
This preserves a joinable reader without introducing an unsafe cancellation
point during teardown. The caller owns the supplied `FILE *`; the server does
not close it.

`test_stdio_server_lifecycle.c` runs 100 same-process
New/Start/Shutdown/Free cycles, an `exit` frame case, and injected failures
after global, context, input initialization, and reader start. This test exists
to catch teardown faults that a one-shot executable process would hide.

## Teardown Acceptance

The deterministic teardown gate was accepted on 2026-08-23. Clang and GCC
ASan+UBSan each pass the lifecycle, smoke, protocol inventory and protocol
conformance CTest set. GCC's uninstrumented rerun passes the same set with the
default 512 MiB peak-process budget. The sanitizer runs set
`ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES=1073741824` only to account for ASan
shadow memory; they do not relax the production budget.

The 100-cycle lifecycle executable also passes Valgrind Memcheck with 54,339
allocations and frees, zero live bytes and zero errors, and passes Helgrind
with zero race reports. MSVC Debug passes the four-test stdio CTest set; MSVC
AddressSanitizer passes the lifecycle executable.

## Protocol Lifecycle And Transport Completion

The protocol lifecycle and transport tasks were revalidated on 2026-08-23
after deterministic teardown became the runtime path. The protocol driver has
29 cases: it covers lifecycle ordering, JSON-RPC envelope and numeric bounds,
typed duplicate and cancellation ids, stdout-isolated trace output, progress,
partial results, and classified malformed frames. It explicitly cancels a
known active workspace-symbol request and requires `-32800`.

`stdio_document_sync_conformance.js` is a separate request/response test. It
opens document version 1, replaces it through `didChange` version 2, confirms
the new symbol is indexed, and confirms the old symbol is gone. Before the
Plan 02 dependency fence exists, this serial transition must complete normally
and must not emit speculative `-32801 ContentModified`.

GCC Debug shared, Clang Debug static ASan+UBSan, and MSVC Debug static each
pass `language_server_stdio_(server_lifecycle|smoke|protocol_inventory|protocol_conformance|document_sync_conformance)` with five passing tests.

## Position-Encoding Handshake

The position-encoding smoke uses a request/response client. It waits for the
`initialize` response before sending `initialized` and `didOpen`, then requests
hover and waits for `shutdown` before `exit`. A one-shot batch can let the
reader thread observe `didOpen` before initialize activates; the resulting
`ContentModified` response correctly enforces the input-generation fence but
does not represent a legal LSP client handshake.

## Strict Document Synchronization

Plan 02 Task 4 makes document notifications transactional. `didOpen` requires a
string `text` and an integer `version`; duplicate opens are rejected so they
cannot silently replace an existing overlay. `didChange` requires an already
open document, a nonempty change list, and a strictly newer integer version.
Each ranged edit is translated by the negotiated UTF-16 or UTF-8 codec without
clamping. Invalid line/column boundaries, reverse ranges, a UTF-16 surrogate
midpoint, invalid UTF-8, or a mismatched `rangeLength` reject the entire
notification.

The server stages every sequential `contentChanges` operation in a temporary
buffer and only publishes the new text/version after all operations succeed.
An invalid notification leaves the previous snapshot intact, marks its URI as
desynchronized, and makes later semantic requests fail closed with
`-32801 Content modified`. Recovery is deliberately narrow: a single
range-less full-content change, or a close/open overlay rebuild, clears the
state. An invalid change for an unopened URI is recorded too, so a later
request cannot accidentally consult disk content as a substitute for a valid
overlay.

`didClose` clears the open overlay but preserves an indexed workspace document
by reloading its disk/project content; virtual, deleted, and no-longer-indexed
documents are removed. `didSave` without text refreshes a disk document or
confirms an existing open overlay. A supplied `text` does not become a same-
version change. A syntactically invalid but committed full replacement remains
synchronized: the server determines commit status from the content snapshot,
not from the semantic-analysis success value used to publish diagnostics.

The conformance driver covers malformed opens, duplicate and stale versions,
atomic multi-change rollback, CR/LF/CRLF, astral and combining Unicode,
invalid UTF-8, UTF-8 `rangeLength`, close-to-disk restoration, save refresh,
and invalid request positions. The GCC Task 4 run also executes the adjacent
snapshot, interface, lifecycle, protocol, position-encoding, diagnostic, and
workspace smoke gates.

## Matrix

The L6 final matrix runs every CTest whose name begins
`language_server_stdio` or `cli_`: the deterministic lifecycle test, five
stdio protocol smokes and 28 CLI smokes/integration suites. Run GCC and Clang
CTest from WSL so the configured `/usr/bin/node` is valid; run MSVC CTest from
the native Visual Studio shell.

## Scope

This test budgets the native stdio server process only. The context-local
semantic-cache LRU separately reports exact `SZrAnalysisCache` storage and
must not be described as RSS, allocator peak, or total process memory.
