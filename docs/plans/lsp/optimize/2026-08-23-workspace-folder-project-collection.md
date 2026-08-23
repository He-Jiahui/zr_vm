---
plan_source: docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
task: Task 2 - workspace folders and project collection
status: completed
tests:
  - stdio_workspace_folders_smoke.js
  - stdio_protocol_conformance.js
  - stdio_document_sync_conformance.js
  - zr_vm_language_server_lsp_interface_test
doc_type: milestone-record
---

# Plan 02 Task 2: Workspace Folder Project Collection

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 08:12 +08:00 | 已完成 | 建立 canonical workspace folder set；实现多根项目加载、root 增删、打开文档 overlay 保留、root 边界文件事件过滤与 project rename 回归。 | GCC、Clang、MSVC 均完成最终 stdio 构建并通过 protocol、document-sync、workspace-folders 三项 CTest；MSVC fresh cache 另通过完整 `zr_vm_language_server_lsp_interface_test`。 |

## Delivered Contract

- Initialize only uses `workspaceFolders` when supplied; otherwise it falls
  back to `rootUri` and then `rootPath`. Every accepted root is normalized by
  the canonical file-URI codec.
- `workspace/didChangeWorkspaceFolders` updates the root collection. Removing
  a root releases only project-owned state that is no longer covered by another
  root, while explicitly opened documents remain available as overlays until
  they close.
- `workspaceFolders.changeNotifications` is advertised only because a request
  handler, a workspace index, and protocol coverage are present together.
- Watched-file and file-operation handlers reject virtual URIs and local file
  URIs outside registered roots unless the document is explicitly open. This
  prevents file events from becoming a local disk-read escape hatch.
- The smoke matrix covers same-name `.zrp` files in independent roots, a nested
  root, selected-project removal, overlay retention and release, project-file
  rename, an outside-root event, and a virtual URI event.

## Validation

- GCC Debug shared: final 713-target stdio build exit 0. Protocol,
  document-sync, and workspace-folders CTest all passed (3/3).
- Clang Debug shared: final 752-target stdio build exit 0. The same CTest set
  passed (3/3).
- MSVC Debug shared: fresh serial 762-target stdio build exit 0. The same
  CTest set passed (3/3), and `zr_vm_language_server_lsp_interface_test`
  passed in full.

This record accepts Plan 02 Task 2 only. Semantic snapshots, incremental
parsing, and diagnostics generation remain separate tasks.
