---
related_code:
  - zr_vm_language_server/stdio/stdio_requests.c
implementation_files:
  - zr_vm_language_server/stdio/stdio_requests.c
plan_sources:
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - user: optimize LSP semantic inference according to the documented milestones
tests:
  - tests/language_server/stdio_protocol_conformance.js
doc_type: milestone-detail
---

# LSP Type Hierarchy Partial Results

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 22:41 +08:00 | completed | `typeHierarchy/supertypes` 与 `typeHierarchy/subtypes` 接入结构化数组 partial sink；精确 token 的层级项经 `$/progress` 发送，最终 JSON-RPC response 为 `null`。 |

## Contract

- Type hierarchy traversals produce direct `TypeHierarchyItem[]` results, so they
  share the bounded array sink already used by workspace symbols, references,
  and call hierarchy traversals.
- The sink preserves each serialized hierarchy item, keeps the configured batch
  limit, and checks the active request cancellation callback before each batch.
- Requests without `partialResultToken` retain their existing ordinary array
  response. `textDocument/prepareTypeHierarchy` is not a partial-result method.

## Evidence

The isolated GCC Debug stdio target rebuilt with exit code `0`.
`stdio_protocol_conformance.js` passed `27/27`. Its regression fixture defines
`PartialDerived : PartialBase`, verifies `PartialBase` arrives through the
supertypes token, verifies `PartialDerived` arrives through the subtypes token,
and verifies both final responses are JSON `null`.

## Remaining Work

Task 4 still needs the specialized workspace-diagnostic report partial schema.
Plan 02 remains responsible for any future ContentModified dependency fence.
