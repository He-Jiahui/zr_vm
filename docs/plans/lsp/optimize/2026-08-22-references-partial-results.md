# LSP References Partial Results

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 22:35 +08:00 | completed | `textDocument/references` 接入既有 array partial sink；精确 token 的引用位置数组经 `$/progress` 发送，最终 JSON-RPC response 为 `null`。 |

## Contract

- References and workspace symbols share the bounded array sink. Batches remain
  limited to `64` items and check the active request cancellation callback
  before each send.
- Requests without `partialResultToken` preserve the existing direct result.
- This change only adds references to the method gate; no parser, semantic
  query, hierarchy, diagnostics, or content-generation behavior changes.

## Evidence

The isolated GCC Debug stdio target rebuilt with exit code `0`.
`stdio_protocol_conformance.js` passed `25/25`, including a source fixture
with a declaration and two uses of `partialReference`; the partial notification
contains the reference locations under the supplied token and the final result
is JSON `null`.

## Remaining Work

Task 4 still needs hierarchy array coverage and the workspace-diagnostic
report partial schema. Plan 02 retains ownership of ContentModified fencing.
