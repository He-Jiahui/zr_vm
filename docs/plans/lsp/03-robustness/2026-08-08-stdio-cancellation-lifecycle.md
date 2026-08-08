---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
implementation_files:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
plan_sources:
  - user: 2026-08-08 improve semantic inference and execute docs/plans/lsp stages
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-08-lsp-l6-stdio-cancellation.md
doc_type: milestone-detail
plan_id: lsp-03-robustness
record_id: 2026-08-08-stdio-cancellation-lifecycle
status: completed
completed_at: 2026-08-08 17:02 +08:00
evidence_scope: stdio-cancellation-lifecycle
---

# Stdio Cancellation Lifecycle

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:02 +08:00 | 已完成 | stdio reader-thread cancellation registry、queued/active request cancellation、workspace diagnostics cooperative stop、JSON-RPC `RequestCancelled` response、50ms rapid out-of-order cancellation smoke。 |

## 已实现契约

- stdin reader thread是唯一读取和解析传输帧的一方。它以精确 JSON-RPC `id` 的 serialized JSON identity登记请求，并直接消费`$/cancelRequest`；取消通知不会进入语义或通知 dispatcher。
- 主线程仍是`SZrLspContext`、semantic cache和stdout的唯一所有者。reader只写受锁保护的消息队列和取消登记，避免把现有语义状态暴露给并发访问。
- 请求被主线程激活后，handler在派发前、派发后和正常结果写出前检查精确活动request identity。已取消请求只返回`-32800` / `Request cancelled`，不发送旧的success result。
- `workspace/diagnostic`在每个workspace文件项之间检查同一活动token。取消会释放已部分构建的result，由request层返回统一错误；后续未取消diagnostic请求继续走既有full/unchanged report路径。
- 不按method name、source text、AST或semantic symbol推断取消目标。不同类型的JSON-RPC id保留其serialized identity；没有匹配的登记项时取消通知保持无副作用。

## TDD 与验证

- RED：旧MSVC stdio server执行新增smoke时以exit 1结束，断言`cancelled workspace/diagnostic must return RequestCancelled without a stale success result`失败，证明原`$/cancelRequest` no-op路径不能满足合同。
- GREEN：MSVC重新构建`zr_vm_language_server_stdio`后，`ctest --test-dir .codex/build-e5-closure-msvc -R "^language_server_stdio_smoke$" --output-on-failure`为1/1、真实exit 0。该smoke连续发送两个`workspace/diagnostic`，按后入队request优先的乱序发送两个取消通知，两个response均断言`-32800`，并断言从首request到两项response完成小于50ms。
- 稳定性：同一MSVC stdio smoke连续执行10次，10/10真实exit 0；`zr_vm_language_server_lsp_interface_test.exe`也真实exit 0，保留既有LSP context/cache回归。
- POSIX：WSL GCC 11.4和Clang 14均以各自CMake target的C11/warning/include设置编译四个改动stdio source，随后把`stdio_transport.c`和cJSON链接为最小stdin request/cancel harness。两套harness均真实exit 0，验证reader登记、活动token匹配和pthread reader的取消可见性。

## 未完成边界

- 此记录只完成L6的stdio cancellation leaf。immutable snapshot race、rapid didChange/cancel/close的完整压力矩阵、p95/p99和峰值内存报告、100-file workspace预算仍未完成，不能将L6整体标记为完成。
- 当前Linux完整CMake stdio target在宿主120秒前台包装限制内未完成，不把它误记为通过；本记录保留MSVC完整stdio E2E以及GCC/Clang实际POSIX transport harness证据。完整三工具链LSP矩阵仍属于后续L6整体门禁。
