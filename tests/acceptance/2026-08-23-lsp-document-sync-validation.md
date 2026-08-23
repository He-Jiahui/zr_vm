# LSP Document Synchronization Validation

## Scope

- Plan 02 Task 4 document lifecycle notifications in the native stdio server.
- Strict `didOpen`, `didChange`, `didClose`, and `didSave` state transitions.
- Negotiated position encoding, atomic edit application, recovery, and disk
  snapshot preservation.

## Baseline

- Ranged request conversion had existing clamping behavior for response-facing
  LSP ranges. The Task 4 contract tightens notification edits only: invalid
  `didChange` positions must be rejected and never clamped.
- During implementation, applying that strict edit validator to every response
  range caused the existing inline-value smoke to fail. The repair retained
  strict validation for content changes and restored the response-side range
  conversion contract.
- A first `didClose` implementation restored every readable file path and
  revived a removed workspace root. It was narrowed to the canonical workspace
  index predicate before acceptance.
- A full replacement with syntax errors was initially marked desynchronized
  because semantic analysis returned false after content committed. The final
  path checks the committed content snapshot instead.

## Test Inventory

- Conformance negatives: malformed open, duplicate open, unopened change, empty
  changes, same/stale version, out-of-bounds/reverse range, invalid UTF-8,
  UTF-16 surrogate midpoint, and incorrect `rangeLength`.
- Conformance recovery and atomicity: desynchronized request fence, one full
  replacement recovery, sequential CR/LF/CRLF edits, and multi-change rollback.
- Lifecycle semantics: close restores an indexed project file, virtual close
  removes it, save without text refreshes disk, and text cannot fake a
  same-version update.
- Integration: semantic snapshot, full LSP interface, lifecycle, main stdio,
  inline value, diagnostics, position encoding, protocol inventory/conformance,
  and workspace folders.

## Tooling Evidence

WSL GCC Debug shared used the task-owned build directory
`.codex/build-lsp-snapshot-gcc`. The final command rebuilt the four direct
targets, ran each executable directly, ran the Node conformance client, then
ran the selected CTest protocol set. Its final CTest summary was `8/8` passed,
`0` failed, with process exit `0`.

## Results

- `zr_vm_language_server_lsp_semantic_snapshot_test`: `0 failure(s)`.
- `zr_vm_language_server_lsp_interface_test`: exits `0`.
- `zr_vm_language_server_stdio_server_lifecycle_test`: exits `0`.
- `stdio_document_sync_conformance.js`: exits `0`.
- Selected stdio CTest set: `8/8` passed, including main smoke, inline-value,
  diagnostics, position encoding, protocol inventory/conformance, document
  synchronization, and workspace folders.

## Acceptance Decision

Accepted for Plan 02 Task 4 on 2026-08-23. This decision covers the GCC
document synchronization gate only. The broader GCC/Clang/MSVC matrix remains
the separate Plan 02 Task 7 acceptance gate.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 12:21 +08:00 | 已完成 | 文档同步严格验证、事务性 change、desync 恢复和磁盘快照恢复。 | GCC direct targets exit 0；stdio CTest 8/8。 |
