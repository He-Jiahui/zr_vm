---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/stdio/stdio_document_content.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/stdio/stdio_lsp_parse.c
  - zr_vm_language_server/stdio/stdio_position_encoding.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_server.c
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
  - tests/language_server/stdio_document_sync_conformance.js
  - tests/language_server/test_lsp_semantic_snapshot.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_stdio_server_lifecycle.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/stdio_workspace_folders_smoke.js
doc_type: milestone-record
---

# Plan 02 Task 4: Strict Document Synchronization

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 12:21 +08:00 | 已完成 | 完成 didOpen/didChange/didClose/didSave 的严格版本化和事务性同步，并发布 desynchronized fail-closed 恢复合同。 | GCC Debug shared：semantic snapshot、完整 LSP interface、stdio lifecycle 均直接 exit 0；8 项 stdio CTest 通过。 |

## Delivered Contract

- `didOpen` 只接受整数 `version` 与字符串 `text`。重复 open 被拒绝，不会覆盖
  既有 overlay。
- `didChange` 要求已打开、非空变更数组和严格递增版本。每个 change 在临时缓冲区按
  顺序应用；任一 range、UTF-8、UTF-16 boundary 或 `rangeLength` 校验失败时，旧文本
  和版本保持不变，整条 notification 标记 desynchronized。
- desynchronized URI 的语义请求返回 `-32801 Content modified`。仅单个、无 range 的
  full-content change，或 close/open 重建 overlay，才能清除状态；未打开 URI 的非法
  change 同样不能借用磁盘内容绕过该状态。
- position codec 严格区分 CR、LF、CRLF，验证完整 UTF-8，拒绝 UTF-16 astral
  surrogate 中点。普通 LSP request 的非法 position 可以返回 InvalidParams 或 null；
  只有通知 edit 的非法 range 明确禁止行尾钳制。
- `didClose` 仅对仍在 workspace/project index 内的 file URI 切回磁盘快照；虚拟、删除
  或未索引 URI 清理 parser/project 状态。`didSave` 不用同版本 text 伪造 change，且
  在无 text 时只确认 open overlay 或刷新磁盘代际。
- 文本提交与语义分析结果分离。一个包含语法/语义错误但已提交的全量更新仍保持同步，
  诊断会发布但不会错误进入 desynchronized 状态。

## Validation

```text
ninja -C .codex/build-lsp-snapshot-gcc \
  zr_vm_language_server_lsp_semantic_snapshot_test \
  zr_vm_language_server_lsp_interface_test \
  zr_vm_language_server_stdio_server_lifecycle_test \
  zr_vm_language_server_stdio

.codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_lsp_semantic_snapshot_test
.codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_lsp_interface_test
.codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_stdio_server_lifecycle_test
node tests/language_server/stdio_document_sync_conformance.js \
  .codex/build-lsp-snapshot-gcc/bin/zr_vm_language_server_stdio
ctest --test-dir .codex/build-lsp-snapshot-gcc --output-on-failure -R \
  'language_server_stdio_(smoke|inline_value_semantic_smoke|diagnostic_fix_smoke|position_encoding_smoke|protocol_inventory|protocol_conformance|document_sync_conformance|workspace_folders_smoke)'
```

- semantic snapshot test reports `0 failure(s)`.
- complete LSP interface suite exits 0.
- lifecycle test exits 0.
- document sync conformance exits 0.
- GCC stdio CTest selection passes `8/8`, `0` failures: main smoke, inline value,
  diagnostic fix, position encoding, protocol inventory/conformance, document
  synchronization, and workspace folders.

## Scope Boundary

This record completes only Plan 02 Task 4. Task 5 real incremental parsing,
Task 6 diagnostic aggregation, and Task 7 GCC/Clang/MSVC acceptance remain
open and are not represented as complete here.
