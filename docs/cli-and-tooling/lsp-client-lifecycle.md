---
related_code:
  - zr_vm_language_server_extension/src/languageServerSession.ts
  - zr_vm_language_server_extension/src/languageClientLifecycle.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/workspaceProjects.ts
  - zr_vm_language_server_extension/src/selectedProjectSync.ts
  - zr_vm_language_server_extension/src/structure.ts
  - zr_vm_language_server_extension/src/diagnosticWorkers.ts
  - zr_vm_language_server_extension/src/diagnosticWorkerProcess.ts
implementation_files:
  - zr_vm_language_server_extension/src/languageServerSession.ts
  - zr_vm_language_server_extension/src/languageClientLifecycle.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/workspaceProjects.ts
  - zr_vm_language_server_extension/src/selectedProjectSync.ts
plan_sources:
  - .codex/sessions/20261115-vsix-loading-slow.md
  - .codex/plans/20260926-lsp-experience-repair.md
  - .codex/plans/20261115-vsix-startup-timeout.md
tests:
  - zr_vm_language_server_extension/test/extensionRestart.test.js
  - zr_vm_language_server_extension/test/helpers/extensionHost.js
  - zr_vm_language_server_extension/test/languageClientLifecycle.test.js
  - zr_vm_language_server_extension/test/workspaceProjects.test.js
  - zr_vm_language_server_extension/test/selectedProjectSync.test.js
  - zr_vm_language_server_extension/test/structureReadiness.test.js
  - zr_vm_language_server_extension/test/diagnosticWorkers.test.js
doc_type: module-detail
---

# LSP client lifecycle

Desktop and Web use `LanguageServerController` to serialize restart requests.
Each caller receives its operation result; the controller stores a settled queue
tail so a rejected start or stop cannot prevent the next restart. Host entrypoints
report startup failures without rejecting extension activation, keeping the
restart command and configuration listener available.

Every attempt owns a `LanguageServerSession`. It owns its watcher, output channel,
transport lifecycle, and client; Web also registers its Worker and Blob URL.
Resources are acquired locally and released in reverse order. All cleanup steps
run even if one fails. Deactivation retires the session synchronously and aborts
the pending startup wait before awaiting the queue. Worker fetch uses this abort
signal; other asynchronous continuations check session retirement before creating
or publishing resources.

Only a fully started current session installs the application request client.
Its cleanup clears that reference only if it still belongs to the same client.
Retired sessions return `DoNotRestart` to the transport close handler, preventing
automatic restarts after extension shutdown or replacement.

Startup has a 30-second deadline. Client disposal is bounded independently, so a
hanging stop does not retain watchers, Workers or URLs. A timeout bounds waiting;
it does not cancel the language-client SDK's initialization promise. The original
promise remains observed, and a retired client is disposed again after actual
settlement. Thus a late successful initialization cannot publish itself or leave
newly registered client resources behind.

Project discovery reuses the current text of already-open manifests, including
unsaved changes, and reads unopened manifests through the workspace filesystem.
Discovery does not open documents merely to parse JSON. This avoids triggering
other language extensions while the server is starting and keeps editor document
state independent of background project discovery.

Desktop startup reads only the remembered project URI for initialization options.
It does not wait for workspace discovery before connecting or before publishing
the initialized request client. Once the native handshake and trace setup finish,
the client is available and project discovery reconciles the selection in the
background. This keeps a slow workspace scan, including repeated scans by views,
outside the 30-second startup deadline. Discovery failures are logged without
retiring the healthy client. The initial synchronization receives the session's
abort signal and checks it before discovery and before sending a notification;
a completed scan cannot notify a retired client. The signal does not cancel the
underlying workspace search.

Discovery shares one in-flight search and a completed manifest snapshot between
views, project actions and selection synchronization. Manifest reads are bounded
to 16 concurrent operations. Generated `.codex/snapshots` are excluded alongside
build outputs and dependencies; vanished manifests are skipped, while other I/O
errors remain observable. Editor changes, external manifest changes and workspace
folder changes invalidate the snapshot. An older in-flight search cannot publish
its result into a newer generation. Active manifest selection can read the open
manifest directly, preserving unsaved text without waiting for workspace search.

The structure controller publishes current-file placeholders, project actions and
the static builtin catalog immediately. Document symbols and project roots load
independently and publish when each completes. Refresh bursts share the current
operation and request at most one follow-up pass, preventing an unbounded queue
of duplicate symbol and project requests. The native parser still owns one shared
context and handles requests serially; these changes parallelize independent view
and filesystem work without accessing that context from multiple threads.

The desktop attempt also owns a lazy diagnostic process pool. The interactive
process synchronizes documents and serves editor requests. It negotiates
`initializationOptions.zrDeferDiagnostics` to omit synchronous lint on document
notifications. The SDK's pull middleware uses separate native processes for lint;
each owns its VM and parser. The session waits for those children to exit during
restart or deactivation. The Web client continues using its existing transport.
See [loading performance](lsp-loading-performance.md) for the worker scheduler
and corelib measurements.

Regression coverage keeps discovery unresolved past the shortened startup budget
and verifies the client is already published, then checks the eventual project
notification. It also covers discovery failure and retirement while discovery is
pending. The real VS Code host reproduction and repaired-host checks are recorded
under `E:/cargo-targets/zr_vm/reports/vsix-reinstall-20261115/`.

Both host clients override the public `start` method to pass every start promise
through the session observer. This includes SDK automatic recovery after a
transport closes. Recovery receives its own startup deadline and retirement
checks, and a late recovery settlement is disposed just like the first start.

The installed language-client 8.1 SDK rejects `stop`/`dispose` while Starting or
StartFailed, before its normal cleanup. Disposal is still invoked: the Node
implementation schedules server-process termination in its `stop` finalizer.
The session also releases the public diagnostic collection and an explicitly
owned output channel. It does not read the lazy `client.outputChannel` getter or
access private SDK fields. The host subclasses consume only the known
"not running" stop error, including the SDK's unawaited stop on initialization
failure; unrelated stop errors remain observable.

The native subclass also checks retirement in the protected
`createMessageTransports` hook, before and after acquisition. The SDK checks the
working directory asynchronously, so its process can be created after an earlier
disposal. A transport acquired by a retired attempt has its reader, writer and
client independently disposed before startup is rejected. This triggers Node's
process finalizer without waiting for server initialization to respond.

The host regression harness executes both actual entrypoints with controlled
external resources. Its client double preserves the SDK's non-running disposal
rejection. Tests cover failed starts and retries, queued restarts, startup timeout,
late settlement after replacement, deactivation during startup/fetch, Worker
construction failure, failed/hanging stops, retired close handling, and exact
Worker/URL/watcher/channel release. Real editor smoke remains the integration
gate for actual client transports and document resynchronization.

The native regressions separately defer transport acquisition past deactivation
and past the startup deadline while leaving initialization unresolved. They verify
that all late transport resources are released at acquisition.
