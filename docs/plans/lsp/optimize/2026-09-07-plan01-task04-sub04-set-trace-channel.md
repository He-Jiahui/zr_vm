---
plan_id: optimize
task: plan01-task04-sub04
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 4 Sub04: setTrace Stderr Channel

## 状态与产出记录

- 开始时间: 2026-09-07 04:23 +08:00
- 实际完成时间: 2026-09-07 04:25 +08:00
- 状态: 已完成
- 源码版本: `596b01e3` 及其父级 stdio trace 实现提交
- 产出路径: `stdio_requests.c`、`stdio_protocol_conformance.js`、模块文档、计划索引与本记录

本子项关闭 `$/setTrace` 的 stdio 通道边界。活动生命周期中支持 `off`、
`messages` 和 `verbose`；trace 元数据写到 stderr，stdout 只保留 JSON-RPC
frame，notification 不产生 response。初始化前和 shutdown 后的 setTrace 由
lifecycle gate 忽略，非法或非字符串 value 不改变当前 level。

## 验证命令及结果

```text
node -e "require('./tests/language_server/stdio_protocol_conformance').protocolCases().length"
  33

WSL GCC Debug
node tests/language_server/stdio_protocol_conformance.js \
  .codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  33/33 passed, exit 0

WSL Clang Debug ASan/UBSan focused
set trace writes only stderr
  Pass, exit 0, no sanitizer diagnostic
control notifications outside lifecycle are ignored
  Pass, exit 0, no sanitizer diagnostic

MSVC Debug
node tests/language_server/stdio_protocol_conformance.js \
  .codex/lsp-optimize-validation/msvc-current/bin/zr_vm_language_server_stdio.exe
  33/33 passed, exit 0
```

回归用例验证 `messages` 的 request/response trace、`verbose` 的 notification
trace、切回 `off` 后 stderr 不再增长，以及 lifecycle 外 setTrace 没有副作用。

## 接受决定

接受 Plan 01 Task 4 Sub04。trace channel 与 stdout frame 隔离已在三工具链证据中
确认；Task 4 的完整 registry、取消、ContentModified 和统一 progress sink 父级
门禁仍保持 pending。
