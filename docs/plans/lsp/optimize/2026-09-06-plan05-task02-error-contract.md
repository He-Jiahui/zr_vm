---
related_code:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
  - tests/language_server/lsp_wasm_worker_probe.js
  - tests/language_server/wasm_capability_inventory.js
  - tests/language_server/wasm_capability_inventory_test.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
implementation_files:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server_extension/src/browser/worker/wasm-bridge.ts
  - zr_vm_language_server_extension/src/browser/worker/server-worker.ts
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/05-native-web-capability-parity.md
tests:
  - tests/language_server/wasm_capability_inventory_test.js
  - zr_vm_language_server_extension/test/serverCapabilities.test.js
  - tests/acceptance/2026-09-06-lsp-wasm-worker-wiring.md
doc_type: acceptance-record
---

# Plan 05 Task 2: WASM Error Contract (preliminary)

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-06 00:40 +08:00 |  | preliminary; superseded by Plan 00 correction | 初步加入 worker 错误传播和 native/WASM envelope 字段。 | `90086f8a` 的实现仍按消息文本推断 code，并把缺少 data 当作成功；未满足验收。 |

本记录保留初步提交的历史证据。实际修复、分配故障测试和当前未完成门槛见
[Plan 00 Task 2 Sub04 response contract](2026-09-07-plan00-task02-sub04-wasm-response-contract.md)。

The initial Web adapter attempted to preserve JSON-RPC error identity across
the WASM boundary, but its message-to-code mapping and optional success data
were insufficient. This record is therefore not an acceptance of Plan 05 Task
2. The corrected boundary is tracked under Plan 00 while core status and
versioned edit work remain open.

The corrected `responseData` requires an own `data` field on success. Failed
responses become `ResponseError` instances with the explicit code, message,
and optional structured data. A malformed JSON payload, UTF-8 decoding failure,
or null response pointer becomes InternalError and cannot be converted to an
empty result.

The worker probe imports the installed browser `ResponseError` implementation
and exercises the production bridge and handlers. The current focused tests
cover InvalidParams, Cancelled, ContentModified, and InternalError envelopes,
malformed envelope variants, pointer release, and removal of the native numeric
code. Core transport-neutral status objects, versioned workspace edits, nested
provider allocation safety, linked WASM/browser smoke, and native/WASM golden
comparison remain later Task 2 gates.
