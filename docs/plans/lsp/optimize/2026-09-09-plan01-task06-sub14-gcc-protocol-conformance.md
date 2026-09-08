---
plan_id: optimize
task: plan01-task06-sub14
status: partial-current-toolchain-evidence
related_code:
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/stdio_protocol_client.js
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
related_module_docs:
  - docs/cli-and-tooling/lsp-stdio-validation.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub14: Current GCC Protocol Conformance

## 状态与产出记录

- 开始时间: 2026-09-09 06:25 +08:00
- 实际完成时间: 2026-09-09 06:40 +08:00
- 状态: 当前 GCC 协议证据完成；跨工具链父门禁继续进行中
- 源码版本: `8b90d8db` plus the shared working-tree overlay
- 产出路径: 当前协议矩阵重放、CTest 回归和本记录
- 剩余门槛: 当前 Clang/MSVC 需在同一源码版本重建后重放；完整 Task 6 仍需
  生产 512 MiB 峰值、内存工具和其他父级验收

## 覆盖边界

The current protocol driver contains 52 cases. The replay covers capability
negotiation, lifecycle ordering, pre/post-initialize and shutdown request
classification, malformed JSON-RPC envelopes, typed and duplicate request IDs,
numeric bounds, cancellation, trace-channel isolation, work-done and partial
progress, and malformed or oversized frames.

## 验证命令及结果

The server was relinked in the existing WSL ext4 GCC cache before replay. The
direct Node client used the repository's Node 22.13.1 runtime:

```text
/home/hejiahui/.codex-tools/node-22.13.1/node-v22.13.1-linux-x64/bin/node \
  tests/language_server/stdio_protocol_conformance.js \
  /home/hejiahui/.codex-builds/lsp-plan01-task06-sub03-gcc/bin/zr_vm_language_server_stdio
52/52 passed, exit 0

ctest --test-dir /home/hejiahui/.codex-builds/lsp-plan01-task06-sub03-gcc \
  --output-on-failure -R '^language_server_stdio_protocol_conformance$'
1/1 passed, exit 0, 17.17 sec
```

The replay produced no protocol failure or server stderr diagnostic. An older
Clang ext4 binary was also probed for comparison, but it timed out in the
capability matrix and known-request diagnostic publication cases (`50/52`);
that binary was not rebuilt by the current cache and is not accepted as current
cross-toolchain evidence. No current MSVC stdio binary exists in the checkout's
validation directories, so the prior MSVC replay remains historical evidence.

## 接受决定

Accept the current GCC protocol conformance result only. Keep the Plan 01 Task
6 protocol and cross-toolchain parent checks open until Clang and MSVC are
rebuilt from the same source revision and the production memory gate is
replayed.
