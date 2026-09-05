---
related_code:
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/stdio/stdio_workspace_files.c
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/stdio_file_operation_capabilities_smoke.js
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# Workspace File Operation Contract

The native stdio server advertises only file-operation requests and
notifications that have an observable implementation. `didCreate`, `didDelete`
and `didRename` use the workspace index; `willRename` returns a versioned
`WorkspaceEdit` after validating the cached disk/open-document snapshots.
`willCreateFiles` and `willDeleteFiles` are intentionally absent because their
implementation returned only `null`; requests receive the exact JSON-RPC
`-32601 Method not found` envelope.

`willRename` borrows URI and source data from the current semantic/workspace
context while it constructs JSON synchronously. It copies edit ranges and
captures the open document version (`null` for an unopened disk snapshot).
Clients must honor open-document versions to reject stale edits. A `null`
version does not guard against disk changes after response construction. A changed
unopened source file invalidates the plan and returns `null`; it is not silently
re-read to manufacture an edit. The operation is limited to files accepted by
the active workspace roots and supported ZR/module extensions.

The smoke fixture covers the four registrations, exact negative envelopes,
unopened project indexing, open-overlay version changes, stale disk rejection,
same-URI rejection, rename indexing and definition navigation. Browser parity
for file operations remains a Plan 05 gate; this document records native
ownership only.
