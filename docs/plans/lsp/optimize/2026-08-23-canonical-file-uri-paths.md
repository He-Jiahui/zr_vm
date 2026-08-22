---
plan_source: docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
task: Task 1 - unified file URI and native path
status: completed
tests:
  - zr_vm_language_server_lsp_uri_test
  - stdio_protocol_conformance.js
  - stdio_document_sync_conformance.js
doc_type: milestone-record
---

# Plan 02 Task 1: Canonical File URI Paths

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 06:05 +08:00 | 已完成 | 发布唯一 `lsp_uri` 双向 API；project/navigation 直接消费该 API；stdio 仅在成功的 `file:` URI 转换后才调用 `fopen`；补齐 URI 正反矩阵。 | GCC URI 14/14、Clang URI 14/14、MSVC URI 16/16；三套 stdio protocol 29/29 与 document-sync smoke 均为真实 exit 0。 |

## Delivered Contract

- `FileToNativePath` only accepts absolute `file:` documents, performs
  strict single-pass percent decoding, and rejects virtual schemes, malformed
  escapes, raw query/fragment syntax, and encoded native separators.
- `FromNativePath` percent-encodes UTF-8 bytes and preserves the platform
  native distinction between POSIX paths, Windows drive paths, and UNC paths.
- `Equivalent` normalizes only valid file URIs. It leaves virtual URI identity
  byte-exact and does not make a filesystem alias from a custom scheme.
- Existing public compatibility entry points forward to the new implementation;
  project and navigation no longer own conversion helpers.

## Validation

- GCC Debug shared rebuilt the affected LSP/parser dependency graph and linked
  the URI test and stdio server. URI: 14/14. Protocol: 29/29. Document sync:
  passed.
- Clang Debug rebuilt the affected LSP shared library, URI test, and stdio
  server. URI: 14/14. Protocol: 29/29. Document sync: passed.
- MSVC Debug rebuilt the affected project/navigation LSP objects and shared
  library. URI: 16/16, including drive and UNC checks. Protocol: 29/29.
  Document sync: passed.

This record accepts Plan 02 Task 1 only. Workspace folders, semantic snapshots,
incremental parsing, and diagnostic generation remain separate tasks.
