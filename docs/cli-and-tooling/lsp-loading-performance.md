---
related_code:
  - zr_vm_language_server_extension/src/workspaceProjects.ts
  - zr_vm_language_server_extension/src/structure.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/diagnosticWorkers.ts
  - zr_vm_language_server_extension/src/diagnosticWorkerProcess.ts
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_lint.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_cli/lint/src/analysis/bindings/bindings_retention.c
  - zr_vm_cli/lint/src/analysis/source.c
implementation_files:
  - zr_vm_language_server_extension/src/diagnosticWorkers.ts
  - zr_vm_language_server_extension/src/diagnosticWorkerProcess.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
plan_sources:
  - .codex/sessions/20261115-vsix-loading-slow.md
tests:
  - zr_vm_language_server_extension/test/diagnosticWorkers.test.js
  - zr_vm_language_server_extension/test/extensionRestart.test.js
  - zr_vm_language_server_extension/test/structureReadiness.test.js
  - zr_vm_language_server_extension/test/workspaceProjects.test.js
  - tests/language_server/stdio_deferred_diagnostics_smoke.js
  - tests/language_server/stdio_diagnostics_generation_smoke.js
  - tests/language_server/stdio_overlay_scope_smoke.js
  - tests/lint/analysis/test_analysis_node_index.c
  - tests/lint/analysis/bindings/test_retention_edge_index.c
doc_type: module-detail
---

# LSP loading performance

## What blocked the editor

Project discovery reread every `.zrp` for several simultaneous consumers and included
historical copies under `.codex/snapshots`. A VS Code run captured `EMFILE` while
reading these manifests. The structure view also waited for symbols, project data,
and the builtin catalog before publishing any roots. Discovery now shares one
cached snapshot, excludes generated directories, and limits manifest reads to 16.
The view publishes its initial roots immediately and fills in each section as the
independent project and symbol requests complete.

The native stdio dispatcher originally ran lint during each document notification
and pull, on the same thread that handles completion and navigation. An overlay
scope error initially masked the true cost: passing manifests and unrelated open
scripts to lint made it reject requests before analysis. After filtering overlays,
one corelib document pull took 12.9 s, a five-document workspace pull took 48.9 s,
and a completion queued behind the opening notifications took 50.2 s. Repeating a
matching diagnostic result ID was already cheap, so caching alone did not remove
the first delay.

The valid lint path also exposed a prototype lifetime bug and two hot lookup loops.
The parser can grow `typePrototypes` during conversion; the semantic analyzer now
uses a stable context snapshot and reacquires the prototype after growth. GDB and
PDB symbolization located the long scans at `zr_binding_edge` and
`zr_lint_analysis_find_node`. Open addressing now indexes binding edges and native
AST node pointers while preserving arrival order and first-handle identity.

## Diagnostic worker lifetime

The desktop client starts one interactive native server and up to two lazy native
diagnostic workers. Each process has its own VM, parser and cache. Both process
roles negotiate `zrDeferDiagnostics`, so document synchronization never runs
implicit lint. The VS Code SDK continues owning diagnostic collections and pulls;
its middleware forwards those pulls to raw stdio workers. The main process handles
interactive requests, and a worker failure does not fall back to lint there.

Each worker pulls only after receiving pending workspace notifications and current
open buffer contents. It receives the selected project and all workspace roots
when created. Document work is queued ahead of background workspace work. A file
edit or project change invalidates queued and running work, and the middleware
checks the revision again after converting results. A worker running obsolete work
is terminated; its slot is retained until the process exits. Session retirement
waits for both workers before starting a replacement client.

The SDK asks for workspace diagnostics every two seconds. A completed workspace
report is reused until a document, project, watched file or configuration change
invalidates it. Without an event, it is revalidated after five minutes so disk
inputs beyond the workspace watcher do not stay stale indefinitely. The middleware compares the client's previous IDs with the saved
reports locally, so independent worker generations do not cause a full analysis
on every idle poll. An ordinary cancellation returns no workspace chunk and does
not count against the SDK's polling error limit.

## Measurements and limits

The real editor probe opens `corelib.zrp`, `main.zr`, `arrays.zr`, `encoding.zr`,
and `sets.zr` in an isolated VS Code profile over the actual repository. It checks
completion before and during background lint, structure roots, diagnostics shown
by the editor, and another completion after restarting the language server.
Native probes separately exercise full/unchanged identity, push suppression,
disk and open-buffer dependency invalidation, and overlay scope.

| Measurement | Observed result |
| --- | ---: |
| Complete structure views, old installed package | Timed out at 60 s |
| Complete structure views, final installed package | 16.8 s from probe start |
| Native first completion after five open notifications, synchronous lint | 50.2 s |
| Native first completion with implicit lint deferred | 0.69 s |
| Final VS Code completion while workers were active | 0.39 s |
| Final VS Code document diagnostic request | 17.4 s |
| Final VS Code completed cold workspace poll | 50.9 s |
| Next workspace poll for the same inputs | 3 ms |

These are local observations, not latency budgets. The final host returned
14 completion items, 15 document symbols, and 18 diagnostics for `arrays.zr`.
The probe restarted the service, obtained completion again, and exited with all
of its diagnostic processes reaped. The foreground open/initialization path
still has its own cost; the above completion scales separate native request time
from the end-to-end first VS Code request.

Cold lint still builds semantic facts for the project. Large scripts can take
seconds to report diagnostics, and the first workspace sweep can take longer.
The process cap limits memory and concurrent CPU use. Existing `throw` and member
resolution diagnostics in corelib still need semantic investigation. The broader
stdio smoke fails while compiling its watched-binary fixture with both the former
package and the rebuilt CLI; the focused diagnostics regressions execute without
that fixture.

Local measurement and installation receipts are in
`E:/cargo-targets/zr_vm/reports/vsix-reinstall-20261115/`.
