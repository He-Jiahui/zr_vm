---
related_code:
  - zr_vm_language_server/stdio/stdio_json_rpc.c
  - zr_vm_language_server/stdio/stdio_lifecycle.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - tests/language_server/stdio_protocol_conformance.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Tasks 1-2: Protocol Negative Replay

## 状态与产出记录

- 开始时间: 2026-09-07 05:24 +08:00
- 实际完成时间: 2026-09-07 05:36 +08:00
- 状态: 已完成
- 源码版本: 当前 `main` 的 `2f94ce94`；本记录覆盖协议测试回放，不宣称 handler
  status/result 统一迁移
- 产出路径: 当前 protocol conformance 回放、计划勾选、模块文档与本记录

本次回放将 Task 1 的生命周期负向合同和 Task 2 已实现的 envelope/notification
边界重新验证到同一个当前源树。顶层 envelope 的直接 C API 回归仍由
[Task 2 Sub04](2026-09-07-plan01-task02-sub04-envelope-api.md) 覆盖；本记录关注
实际 stdio 进程的 response、exit code、notification side effect 和 stderr/stdout
边界。

## 覆盖边界

- initialize 前普通 request 返回 `-32002 ServerNotInitialized`，initialize 前的
  非 `exit` notification 无 response 且没有改变后续行为；
- 第二次 initialize 返回 `-32600 Invalid Request`；shutdown 前普通 request 被拒绝，
  shutdown 后普通 request 返回 `-32600`；shutdown 前后 exit 分别返回 1/0；
- 缺失或错误 `jsonrpc`、数组/标量顶层消息、bool/structured/fractional request id、
  initialize 的缺失/null/标量/数组 params、非法位置和 range 数字均返回精确错误，
  malformed notification 只记录日志而不发送 response；
- 合法 notification、显式 null request id、typed/duplicate numeric/string IDs、未知
  与已知 cancellation、trace/progress、partial result 和 malformed frame cases
  保持既有 envelope/frame 断言。

## 验证命令及结果

```text
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  34/34 passed, exit 0

node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio
  34/34 passed, exit 0, no ASan/UBSan diagnostic
```

两次回放均包含完整 34 个 case，stdout 只被协议客户端解析为合法 frame；stderr
捕获了预期的 trace/reader 分类信息，未被误当成 response。

## 接受决定

接受 Task 1 的三条 lifecycle RED 门禁和 Task 2 的 malformed notification no-response
门禁。Task 2 的“request 缺失 id”仍由 envelope API 按 JSON-RPC notification 语义分类，
不在本记录中重新解释；统一 handler status/result、所有方法级 params 契约和 Plan 01
父级门禁继续 pending。
