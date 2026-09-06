---
related_code:
  - tests/language_server/test_stdio_lsp_parse.c
  - tests/cmake/zr_vm_lsp_stdio_parse_tests.cmake
  - tests/language_server/collect_lsp_baseline.js
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_protocol_client.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_envelope_mutations.js
  - tests/language_server/stdio_document_sync_conformance.js
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/test_stdio_server_lifecycle.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/stdio/stdio_document_content.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
  - zr_vm_language_server/stdio/stdio_frame_reader.h
  - zr_vm_language_server/stdio/stdio_frame_reader.c
  - zr_vm_language_server/stdio/stdio_position_encoding.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - tests/cmake/zr_vm_lsp_stdio_parse_tests.cmake
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_envelope_mutations.js
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/stdio/stdio_document_content.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
  - zr_vm_language_server/stdio/stdio_frame_reader.c
  - zr_vm_language_server/stdio/stdio_position_encoding.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_server.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/test_stdio_lsp_parse.c
  - tests/language_server/collect_lsp_baseline_test.js
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_envelope_mutations.js
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_resolve_capabilities_smoke.js
  - tests/language_server/stdio_save_capabilities_smoke.js
  - tests/language_server/stdio_client_commands_smoke.js
  - tests/language_server/stdio_document_sync_conformance.js
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/test_stdio_server_lifecycle.c
doc_type: module-guide
---

# LSP Stdio Validation

## Strict Numeric Parsing

`stdio_lsp_parse.c` is the single validation boundary for LSP sizes, positions
and ranges. `parse_size_value_strict` accepts only finite, non-negative,
integral JSON numbers representable by `TZrSize`; its exclusive upper bound is
computed as the exact `2^N` value instead of converting `SIZE_MAX` to `double`,
which rounds up on 64-bit hosts. Position components use the corresponding
finite `INT32_MAX` bound, and ranges require an ordered start/end pair.

The focused `language_server_stdio_lsp_parse` test covers zero, signed zero,
the largest representable position and size values, fractions, negative and
non-finite values, the rounded `2^N` size boundary, wrong JSON types, malformed
objects, missing fields and reversed ranges. The test is registered through
`tests/cmake/zr_vm_lsp_stdio_parse_tests.cmake` so the numeric contract can be
run independently of the larger protocol suite. See [Plan 01 Task 2 Sub01](../plans/lsp/optimize/2026-09-07-plan01-task02-sub01-strict-numeric-parsing.md)
for the RED/GREEN and three-toolchain evidence.

## Frame Header Exactness

`stdio_frame_reader.c` owns byte-level header framing and validates a complete
header block before allocating the payload. It accepts CRLF lines, requires one
unsigned decimal `Content-Length`, applies the centralized byte/count/payload
limits, and classifies clean EOF, malformed headers, truncated payloads, size
violations and I/O failures separately. Unknown headers remain allowed but count
toward the limits.

The reader rejects a NUL byte while reading headers, before the line is treated
as a C string. Every explicit `charset` parameter is examined; `utf-8` and
`utf8` are accepted (including quoted values), while any other explicit charset
or conflicting value, including a missing value, is malformed. JSON payload parsing remains the later
JSON-RPC concern and is not confused with a framing failure.

The protocol driver covers the NUL and duplicate/conflicting charset cases
alongside the existing missing, duplicate, suffixed, overflowing, truncated,
newline, oversize and excessive-header cases. See [Plan 01 Task 3 Sub01](../plans/lsp/optimize/2026-09-07-plan01-task03-sub01-header-exactness.md)
for the RED/GREEN evidence. The current replay of the complete frame boundary is
recorded in [Plan 01 Task 3 Sub03](../plans/lsp/optimize/2026-09-07-plan01-task03-sub03-frame-limits-and-classification.md);
the broader Plan 01 lifecycle and teardown gates remain separate.

## Response Envelope Validation

`stdio_protocol_conformance.js` uses `StdioProtocolClient` for framing, typed
request IDs, deadlines, response/notification backlogs and stderr capture. The
smoke driver extends the same client. Every conformance response first checks
an object with `jsonrpc: "2.0"`, the exact typed ID and exactly one of `result`
or `error`. Errors also require the case's exact numeric code and a string
message. Null results remain valid, while an added `error: null`, missing
result or mixed error/result response fails. The fixed server response shape
allows only the three envelope keys; the error payload may retain optional data.

The duplicate-ID case requires exactly one `-32600` error and one successful
empty workspace-symbol result. Numeric `1` and string `"1"` each retain their
own valid success envelope. Trace, work-done and partial-result cases apply the
same checks before inspecting method-specific results.

Numeric JSON-RPC request IDs are accepted only when their finite value is an
integer within `+/-ZR_LSP_JSON_SAFE_INTEGER_MAX` (`9007199254740991`). Fractional,
non-finite and out-of-range values are rejected before request reservation. This
keeps the numeric identity in the JSON-safe range used by the request registry. Response transport
serializes numeric IDs with a 17-digit round-trip representation, so the upper
safe boundary is echoed as `9007199254740991`; `9007199254740992` is rejected as
`-32600 Invalid Request` with a null error ID. Numeric and string IDs remain
separate registry keys.

The protocol driver includes `id: 1.5` as a malformed request-id case and requires
the `-32600` error envelope with `id: null`. See [Plan 01 Task 2 Sub03](../plans/lsp/optimize/2026-09-07-plan01-task02-sub03-integer-request-ids.md)
for the RED/GREEN replay.

The lifecycle target also calls `ZrLanguageServer_StdioJsonRpc_ParseEnvelope`
directly. Its regression checks invalid top-level objects and protocol versions,
typed id rejection, scalar params, valid object/array params, missing-id
notifications and explicit `null` request IDs. See [Plan 01 Task 2 Sub04](../plans/lsp/optimize/2026-09-07-plan01-task02-sub04-envelope-api.md)
for the focused CTest evidence.

The current stdio protocol replay runs all 34 cases on GCC and Clang
ASan/UBSan. It covers pre-initialize and post-shutdown request gates, repeated
initialize, exit ordering, malformed notification suppression, top-level and
request-id rejection, invalid initialize params, and the classified frame
failures. Both servers pass 34/34 with no Clang sanitizer diagnostic. See [Tasks
1-2 protocol negative replay](../plans/lsp/optimize/2026-09-07-plan01-task01-task02-protocol-negative-replay.md).

The hierarchy parameter regression extends that replay to 35 cases. Missing or
malformed params for call/type hierarchy prepare and item requests now return
`-32602 Invalid params`; provider queries that genuinely find no item continue to
return a valid empty array. GCC and Clang ASan/UBSan both pass 35/35. See [Plan
01 Task 2 Sub05](../plans/lsp/optimize/2026-09-07-plan01-task02-sub05-hierarchy-invalid-params.md).

`workDoneToken` and `partialResultToken` use the same finite, integral safe
integer boundary for numeric tokens. Both positive and negative safe endpoints
are preserved in `$/progress`; values outside the boundary are rejected as
`-32602 Invalid params` before any progress notification is sent.

The `initialize` handler applies its method-level parameter shape after envelope
validation: missing, JSON `null`, scalar and array `params` return
`-32602 InvalidParams` before the lifecycle enters `INITIALIZING`. This boundary
does not imply that every other handler has been migrated to the planned shared
status/result contract.

Control notifications are lifecycle-gated. `initialized` only promotes
`INITIALIZING` to `RUNNING`, while `$/setTrace` is ignored before initialization
and after `shutdown`; `exit` remains the only notification that is handled in
every state. A successful `shutdown` followed by `exit` returns process code 0,
whereas an exit without a successful shutdown returns 1.

During an active lifecycle, `$/setTrace` accepts `off`, `messages` and `verbose`.
Trace records are written to stderr only; stdout remains reserved for framed
JSON-RPC messages. `messages` records request/response metadata, while `verbose`
also records notification metadata. Invalid or non-string values leave the
current level unchanged, and the notification itself never produces a response.
The focused cross-toolchain evidence is in [Plan 01 Task 4 Sub04](../plans/lsp/optimize/2026-09-07-plan01-task04-sub04-set-trace-channel.md).

`stdio_protocol_envelope_mutations.js` imports the production conformance case
list without starting its CLI. Six unchanged cases must pass first. It then
runs 11 cases with one decoded response mutation each, covering missing/wrong
protocol versions, mixed result/error fields, missing results and malformed
error messages. Each case must observe exactly one injected response and fail
on an envelope assertion; a timeout or unrelated failure cannot satisfy the
test. This verifies the actual case wiring, not a separate copy of the rules.

The runner executes serially and restores the client's dispatch prototype in
`finally`. Each case owns and terminates its child process; mutations affect
only the local decoded JSON, with no server source, semantic facts or provider
state changes. CTest registers it as
`language_server_stdio_protocol_envelope_mutations`. See
[Plan 00 Task 3 Sub04](../plans/lsp/optimize/2026-09-06-plan00-task03-sub04-response-envelopes.md)
for RED/GREEN results and the filesystem-specific timeout boundary.

## Client Command Ownership

`workspace/executeCommand` has no native server implementation or capability
registration. Requests for client-owned `zr.runCurrentProject`,
`zr.showReferences`, or unknown commands therefore return the full JSON-RPC
MethodNotFound envelope (`-32601`). A successful `null` response must not
acknowledge work the server never performed. The unused handler, dispatch
branch, build source and protocol constants have been removed.

Code lenses still carry client command IDs and arguments. The extension owns
their execution; the server neither runs a project nor displays editor UI.
`stdio_client_commands_smoke.js` checks empty and command-aware clients,
absence of a server command provider, all three exact error responses, and
clean shutdown. `stdio_resolve_capabilities_smoke.js` independently verifies
the retained real test CodeLens payload. This routing change introduces no
semantic identity, snapshot, borrowed lifetime or edit ownership contract.

The repair and evidence are recorded in
[Plan 00 Task 4 Sub06](../plans/lsp/optimize/2026-09-05-plan00-task04-sub06-client-commands.md).

## Complete Baseline Collection

`tests/language_server/collect_lsp_baseline.js` reads the configured CTest
`language_server` aggregate through `--show-only=json-v1`. It executes all
members even after a failure and records exit/signal/errors, durations and exact
test failure blocks. A printed failure with exit zero remains a failure. Output
directories with completed summaries are rejected to retain historical evidence.
The required source-commit argument is caller provenance; the acceptance record
must also establish that the build came from that exported source.
Multi-config builds use `--config=Debug` (or the selected configuration); the
collector passes it to CTest/CMake and includes the matching `lib/<config>`
directory in native library lookup. Timeout/spawn errors fail collection even
when a child catches termination and exits zero.

The collector is a validation artifact. It owns only its output logs/JSON and
child processes, uses serial native executions and does not mutate LSP facts or
snapshots. The current baseline and per-layer responsibility ledger are in
[the acceptance record](../../tests/acceptance/2026-09-05-lsp-optimize-current-baseline.md).

The project-feature executable also owns a file-local failure counter. Every
`TEST_FAIL` increments it and `main` returns nonzero after any failure, so the
standard CTest aggregate detects the same failures without depending on output
parsing. The counter is confined to the serial test process; it does not alter
semantic fixtures, snapshot lifetime or compiler state. The original 14-failure
exit-zero baseline remains archived as historical evidence.

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

The known-id protocol conformance case queues its request and cancellation
behind a 2048-class `didOpen`. It waits separately for exact URI/version
diagnostics (10000 ms preparation deadline) before consuming the shared client's
response backlog (unchanged 3000 ms response deadline). Early success responses
remain in the backlog and fail the exact cancellation envelope assertion.
This fixture proves queued cancellation delivery; it does not measure active
query interruption or replace the separate frozen 50 ms cancellation gate.

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

The request registry keys active work by JSON-RPC ID kind and value. Numeric `1`
and string `"1"` reserve independently; a repeated same-typed ID is rejected,
`$/cancelRequest` marks only the matching entry, and completion removes the entry
so a later request starts with a fresh cancellation flag. The lifecycle executable
contains the direct registry regression in addition to its server teardown loop.
See [Plan 01 Task 4 Sub04](../plans/lsp/optimize/2026-09-07-plan01-task04-sub04-request-registry-identity.md)
for the focused evidence.

`test_stdio_server_lifecycle.c` runs 100 same-process
New/Start/Shutdown/Free cycles, an `exit` frame case, and injected failures
after global, context, input initialization, and reader start. This test exists
to catch teardown faults that a one-shot executable process would hide.

The same executable directly exercises `SZrStdioLifecycle`: initialization starts
in `NEW`, only `INITIALIZING` can consume `initialized`, and that notification is
recorded when the state enters `RUNNING`. Repeated or early `initialized` events
are ignored, ordinary requests are rejected in `NEW`/`SHUTDOWN`/`EXITED`, and
`Exit` returns zero only after a successful shutdown. Current GCC and Clang
ASan/UBSan runs include these assertions; the focused result is recorded in
[Plan 01 Task 1 Sub02](../plans/lsp/optimize/2026-09-07-plan01-task01-sub02-state-transitions.md).

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

The current source replay on `2f94ce94` reruns the lifecycle executable and CTest
after the protocol envelope, frame, ID and progress changes. GCC and Clang
ASan/UBSan each pass `language_server_stdio_server_lifecycle` (`1/1`), and the
direct executable reports `Pass - stdio server lifecycle`. The target still
contains the 100-cycle same-process loop, valid `exit` stop path and global,
context, input-init and reader-start fault injection. See [Plan 01 Task 5 current
replay](../plans/lsp/optimize/2026-09-07-plan01-task05-deterministic-teardown-current.md).

## Protocol Lifecycle And Transport Completion

The protocol lifecycle and transport tasks were revalidated on 2026-08-23
after deterministic teardown became the runtime path. The protocol driver has
29 cases at that historical acceptance: it covers lifecycle ordering, JSON-RPC envelope and numeric bounds,
typed duplicate and cancellation ids, stdout-isolated trace output, progress,
partial results, and classified malformed frames. It explicitly cancels a
known active workspace-symbol request and requires `-32800`.

The current driver has 31 cases. Its known-id fixture exercises a queued request;
the [2026-09-05 setup-deadline repair](../plans/lsp/optimize/2026-09-05-plan00-task03-sub01-cancellation-setup.md)
records three-toolchain 30/30 results without claiming active-query latency.

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

## Save Capability Contract

Native `textDocumentSync` publishes `openClose`, incremental `change`,
`willSaveWaitUntil`, and `save: {includeText: false}`. It does not publish
`willSave`: no pre-save notification handler consumes that event. The unused
field constant was removed with the declaration in Plan 00 Task 4 Sub05.
Unsupported notifications follow the protocol's ordinary ignore path.

`willSaveWaitUntil` uses the existing formatting request handler and returns
text edits for the current document content. `didSave` without text confirms
the open overlay or refreshes indexed disk state through the existing document
lifecycle. These are distinct from the withdrawn notification declaration.
The adapter adds no saved-document snapshot, borrowed query result or semantic
identity. Returned JSON and edits follow their existing request ownership;
the client applies edits with a newer document version before sending didSave.

The focused save fixture checks an empty client and a save-aware client,
the complete textDocumentSync object, exact formatting text/range, diagnostics
at versions 1 and 2, an empty edit after formatting, definition coordinates and
clean shutdown. A disk fixture changes a cached class declaration, requires
didSave to publish diagnostics with the next generation, and checks the new
definition range. General formatter correctness and versioned edit acceptance
remain separate Plan 02/04 obligations. Evidence is in
[Plan 00 Task 4 Sub05](../plans/lsp/optimize/2026-09-05-plan00-task04-sub05-save-notification.md).

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
