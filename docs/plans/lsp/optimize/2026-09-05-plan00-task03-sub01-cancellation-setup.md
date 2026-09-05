---
related_code:
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_client.js
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_documents.c
implementation_files:
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - tests/language_server/stdio_protocol_conformance.js
doc_type: milestone-record
---

# Known Request Cancellation Setup

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 17:47 +08:00 | 2026-09-05 18:07 +08:00 | completed | Separate the preceding document preparation deadline from queued request cancellation; retain exact URI/version, request id and -32800 envelope checks; complete both independent review stages. | GCC 30/30 (15.46 s), Clang 30/30 (23.40 s), MSVC 30/30 (25.90 s), all exit 0; Node syntax and scoped diff checks pass. |

## RED And Responsibility

The original Clang protocol driver passes 29/30 cases and fails only
`cancel known request id`: `timed out waiting for response id=cancel-known-request
stderr=` at `stdio_protocol_client.js:177`. The request deadline starts while
the preceding 2048-class `didOpen` still owns the synchronous analysis queue.
The existing request registry already retains the exact queued cancellation;
the defect is the fixture charging document preparation to response waiting.

The local reproduction and previous baseline probe observe the correct
`-32800` response after document diagnostics. Probe timestamps use different
initialization/open origins, so this record does not present absolute probe
timestamps as a measured document-analysis duration.

## Changed Contract

The fixture still sends the same 2048-class document, workspace-symbol request
and exact-id cancellation immediately. The shared client keeps early responses
in its response backlog while the fixture waits up to 10000 ms for preparation
diagnostics matching the exact URI and version. It then consumes that backlog
with the unchanged 3000 ms response bound. An early incorrect success remains
observable and fails the existing JSON-RPC version/id/error/no-result checks.

The fixture owns only its test process and client waiters; `withClient` performs
the same cleanup on setup or response failure. No runtime, shared-client or
semantic code changes. No syntax conflict or language-rule change is involved.

This is queued known-id cancellation conformance. It does not establish latency
during an executing semantic query. The separate frozen <=50 ms cancellation
observation gate and its strict smoke assertion remain unchanged and pending.

## Verification And Source

```text
wsl.exe --exec node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc/bin/zr_vm_language_server_stdio
wsl.exe --exec node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js /home/hejiahui/.codex-builds/lsp-optimize-20260905-root/clang/bin/zr_vm_language_server_stdio
node tests/language_server/stdio_protocol_conformance.js E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc/bin/zr_vm_language_server_stdio.exe
node --check tests/language_server/stdio_protocol_conformance.js
git diff --check -- tests/language_server/stdio_protocol_conformance.js
```

All 30 registered protocol cases pass on the three frozen binaries from the
[file-operation leaf](2026-09-05-plan00-task04-sub03-file-operations.md): original
c95e5387 export plus committed resolve/navigation/file-operation paths. Concurrent
ownership/semantic/runtime commits and dirty overlays are not part of those
binaries. This distinction is preserved for the future integrated replay.
The commit containing this record owns the single test change, this evidence,
module guidance and plan/index links; `git log -1 --format=%H -- <this-record>`
identifies it.

Specification review verifies that matching diagnostics occur after document
update and that the response backlog cannot hide incorrect responses. Quality
review verifies Node 12 compatibility, queue ordering and failure cleanup.
Module guidance is [LSP Stdio Validation](../../../cli-and-tooling/lsp-stdio-validation.md).
Plan 00 Task 3 still requires the complete capability/version and negative
protocol audit; Plan 01's fault injection, sanitizer and latency gates have not
been promoted by this test repair.
