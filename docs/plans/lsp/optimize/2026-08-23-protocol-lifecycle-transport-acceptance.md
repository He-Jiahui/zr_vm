---
plan_source: docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
scope: tasks-1-through-4
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_document_sync_conformance
  - language_server_stdio_server_lifecycle
  - language_server_stdio_smoke
  - language_server_stdio_protocol_inventory
doc_type: acceptance-record
---

# LSP Protocol Lifecycle And Transport Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 03:54 +08:00 | completed | Plan 01 Task 1-4 are accepted: explicit lifecycle state machine, strict JSON-RPC envelope and parameter validation, bounded UTF-8 stdio frame reader, and typed request registry with cancellation, progress, trace, and serial document synchronization. | GCC Debug shared, Clang Debug static ASan+UBSan, and MSVC Debug static each pass the same five CTest cases. The protocol driver passes 29 cases, including known-id cancellation and malformed-frame exit classification. |

## Acceptance Boundary

- `initialize`, `initialized`, `shutdown`, and `exit` are represented by the
  explicit lifecycle state machine. Ordinary requests before initialization
  return `-32002`; requests after shutdown return `-32600`; non-`exit`
  notifications outside the allowed state are silent.
- A JSON-RPC message is validated before dispatch. Invalid envelopes produce
  `-32600`, malformed request parameters produce `-32602`, and malformed
  notifications only write to stderr.
- Frame parsing is bounded before allocation and distinguishes clean EOF from
  malformed headers, truncation, oversize input, and I/O failure. Protocol
  stdout contains only framed JSON-RPC messages.
- Registry keys retain JSON-RPC id type and value. Duplicate active ids are
  invalid; unknown cancellations are no-ops; an observed known cancellation
  returns exactly `-32800`.
- Plan 02 remains the owner of immutable snapshots and dependency fences.
  Task 4 deliberately does not synthesize `-32801 ContentModified` from input
  ordering or document generation.

## Validation

The exact CTest expression used for each toolchain was:

```text
language_server_stdio_(server_lifecycle|smoke|protocol_inventory|protocol_conformance|document_sync_conformance)
```

- GCC Debug shared: 5/5 passed.
- Clang Debug static ASan+UBSan with LeakSanitizer enabled: 5/5 passed.
- MSVC Debug static: 5/5 passed.

The Clang runner used `setarch x86_64 -R` for the known WSL sanitizer ASLR
constraint. This changes the runner personality only; it does not suppress
sanitizer reports or relax protocol behavior.
