---
related_code:
  - zr_vm_language_server_extension/src/languageServerSession.ts
  - zr_vm_language_server_extension/src/languageClientLifecycle.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/workspaceProjects.ts
implementation_files:
  - zr_vm_language_server_extension/src/languageServerSession.ts
  - zr_vm_language_server_extension/src/languageClientLifecycle.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/src/workspaceProjects.ts
plan_sources:
  - .codex/plans/20260926-lsp-experience-repair.md
tests:
  - zr_vm_language_server_extension/test/extensionRestart.test.js
  - zr_vm_language_server_extension/test/helpers/extensionHost.js
  - zr_vm_language_server_extension/test/languageClientLifecycle.test.js
  - zr_vm_language_server_extension/test/workspaceProjects.test.js
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
