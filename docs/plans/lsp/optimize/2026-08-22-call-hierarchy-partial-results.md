# LSP Call Hierarchy Partial Results

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-22 22:50 +08:00 | completed | `callHierarchy/incomingCalls` 与 `callHierarchy/outgoingCalls` 接入 direct-array partial sink；results 经精确 token 的 `$/progress` 发送，最终 JSON-RPC response 为 `null`。 |

## Contract

- Incoming and outgoing call hierarchy responses use the same bounded array
  batches and per-batch cancellation boundary as workspace symbols and
  references.
- The protocol driver proves incoming callers are preserved in partial output;
  the method gate covers both call directions, whose serialized result schema
  is the same direct JSON array.
- Type hierarchy and workspace diagnostic reports remain independent pending
  their own protocol contracts.

## Evidence

The isolated GCC Debug stdio target rebuilt with exit code `0`.
`stdio_protocol_conformance.js` passed `26/26`; its call hierarchy case opens
`partialCallee` and `partialCaller`, prepares the callee item, receives the
incoming caller through `$/progress`, and receives a final JSON null result.

## Remaining Work

Task 4 still needs type hierarchy partial arrays and the workspace-diagnostic
report partial schema. Plan 02 retains ownership of ContentModified fencing.
