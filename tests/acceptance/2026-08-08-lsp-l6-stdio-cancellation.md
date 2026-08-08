# LSP L6 Stdio Cancellation Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:02 +08:00 | 已完成 | 两个乱序取消的`workspace/diagnostic`请求在50ms内仅返回JSON-RPC `-32800`，后续diagnostic请求保持可用。 |

## Acceptance Scope

- `tests/language_server/stdio_smoke.js`在一次正常workspace diagnostic后连续创建两个request。
- 客户端先取消后入队request、再取消活动request；测试拒绝任何success result，并要求两个error code均为`-32800`。
- 测量从第一个request创建到两个取消response到达的elapsed time；大于或等于50ms使smoke失败。
- 取消后继续执行原有unchanged workspace diagnostic断言，避免以关闭server或丢弃队列伪造成功。

## Evidence

- RED server：新增取消断言以exit 1失败，表明旧实现把`$/cancelRequest`作为no-op。
- MSVC：重建`zr_vm_language_server_stdio`，精确CTest smoke 1/1真实exit 0，连续10次Node smoke均真实exit 0。
- GCC 11.4与Clang 14：各自的POSIX stdin request/cancel harness真实exit 0。完整Linux LSP target build仍受宿主前台命令的120秒上限影响，未作为通过证据。

## Decision

- 接受本stdio cancellation leaf。
- L6整体仍须完成snapshot race、edit/close stress、可重复性能和内存预算门禁。
