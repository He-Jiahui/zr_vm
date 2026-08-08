# LSP L6 Stdio Content-Modified Fence Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 17:26 +08:00 | 已完成 | active `workspace/diagnostic`在reader观察到`didChange`后只返回`-32801`，v2更新后新的diagnostic request仍可成功返回。 |

## Acceptance Scope

- `tests/language_server/stdio_smoke.js`先建立并验证generic document v1的workspace diagnostic。
- 测试发送一个`workspace/diagnostic` request，紧接着发送同一document的`textDocument/didChange` v2。
- 先前request必须拒绝success result并精确返回JSON-RPC `-32801`。测试随后等待v2 diagnostics，并断言新的workspace diagnostic报告v2。
- 测试不根据diagnostic文本、URI命名、AST或member name推断请求是否过期；判定仅来自stdio request input generation。

## Evidence

- RED：旧server执行新增断言以exit 1失败，确认它仍会发送陈旧success response。
- GREEN：MSVC stdio smoke、精确CTest smoke均1/1真实exit 0；完整Node smoke连续10次10/10真实exit 0；LSP interface suite真实exit 0。
- WSL GCC 11.4和Clang 14：每套C11 POSIX transport harness真实exit 0，覆盖reader在激活前已排队`didChange`的顺序。

## Decision

- 接受reader-observed input-generation stale-response fence。
- L6整体仍须完成immutable snapshot、edit/cancel/close压力、性能和内存预算门禁。
