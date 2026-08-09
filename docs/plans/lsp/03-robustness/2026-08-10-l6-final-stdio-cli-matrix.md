---
plan_id: lsp-03-robustness
record_id: 2026-08-10-l6-final-stdio-cli-matrix
status: completed
completed_at: 2026-08-10 01:17 +08:00
related_code:
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/stdio_smoke.js
tests:
  - tests/language_server/stdio_position_encoding_smoke.js
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-08-10-lsp-l6-final-stdio-cli-matrix.md
doc_type: milestone-detail
---

# L6 Final Stdio and CLI Matrix

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-10 01:17 +08:00 | 已完成 | 完成 L6 registered stdio/CLI CTest matrix 的 GCC、Clang、MSVC 重放；position-encoding smoke 改为合法异步 initialize handshake，持续断言 UTF-8 byte-column hover range。 |

## 已实现契约

- position-encoding smoke 必须先收到 `initialize` response，随后才发送
  `initialized`、`didOpen` 和 hover request；`shutdown` response 在 `exit`
  前完成。测试不能用违反初始化顺序的一次性 batch 绕过 input-generation
  fence。
- `positionEncodings: [utf-8, utf-16]` 仍协商 `utf-8`，并以多字节 lambda
  前缀验证 import literal hover 的 UTF-8 byte offsets。
- 最终 matrix 固定为所有名称匹配 `^(language_server_stdio|cli_)` 的 33 个
  registered CTests：5 个 stdio protocol smokes 与 28 个 CLI smoke/
  integration suites。每个工具链先生成这些 CTest 对应 executable，再运行
  CTest，缺失 executable 不作为产品测试结论。

## 验证

- TDD RED：GCC、Clang、MSVC 的旧 position test 在一次性写入
  initialize/didOpen/exit 时均得到无 capabilities 的 initialize response；
  reader 先观察 didOpen 后的 `ContentModified` 是正确 fence 行为。
- GREEN：异步 position-encoding smoke 在 GCC、Clang、MSVC 都真实 exit 0。
- GREEN：GCC `ctest -R '^(language_server_stdio|cli_)'` 为 33/33，真实 exit 0。
- GREEN：Clang 同一 matrix 为 33/33，真实 exit 0。
- GREEN：MSVC 同一 matrix 为 33/33，真实 exit 0。
- matrix 包含 L6 main smoke 的 warm/diagnostic/100-file budget、process
  peak-memory gate、rapid churn、position encoding、inline value、diagnostic
  fix、type hierarchy 与 CLI integration。

## 完成边界

- L6 position、snapshot、cache-budget、latency、process peak-memory 和 final
  stdio/CLI matrix 的计划退出条件均已有独立记录和三工具链证据。
- 更广 LSP semantic-inference、debug/repl、provider parity 与后续计划阶段
  仍按各自里程碑继续，不因 L6 完成而标记总体目标完成。
