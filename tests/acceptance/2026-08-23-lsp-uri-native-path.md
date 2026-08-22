---
plan_source: docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
scope: task-1-file-uri-native-path
tests:
  - tests/language_server/test_lsp_uri.c
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_document_sync_conformance.js
doc_type: acceptance-record
---

# LSP URI And Native Path Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 06:05 +08:00 | 已完成 | Plan 02 Task 1 file URI/native-path conversion boundary. | GCC, Clang, and MSVC rebuilt focused targets and passed the platform URI matrix plus serial stdio smoke. |

## Checked Behaviors

- Native paths round-trip through percent-encoded `file:` URIs without losing
  spaces, percent signs, `#`, or UTF-8 bytes.
- `FILE://localhost/...` normalizes as a local file URI. Windows drive and UNC
  forms use their canonical native representations.
- Virtual schemes, bare paths, malformed escapes, raw query/fragment syntax,
  encoded separators, and insufficient output buffers fail closed.
- File URI equivalence normalizes dot segments and platform separators; virtual
  documents are never assigned filesystem equivalence.
- The stdio protocol and document synchronization drivers remain successful
  against each toolchain's newly linked server binary.
