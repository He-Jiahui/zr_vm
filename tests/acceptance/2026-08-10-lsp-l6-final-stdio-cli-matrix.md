# LSP L6 Final Stdio and CLI Matrix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-10 01:17 +08:00 | 已完成 | 验收 GCC、Clang 和 MSVC 的完整 L6 registered stdio/CLI matrix，并验证 position-encoding 的合法 initialize handshake。 |

## Evidence

- 旧 one-shot position fixture 在三工具链一致暴露 input-generation fence：
  reader 先处理 didOpen 时 initialize 不能合法返回 capabilities。
- 修正后的 async request/response fixture 在三工具链直接真实 exit 0，保持
  UTF-8 negotiation 和 multi-byte lambda 前缀下的 hover byte-range assertion。
- GCC CTest `^(language_server_stdio|cli_)`：33/33，真实 exit 0，161.12s。
- Clang 同一 CTest matrix：33/33，真实 exit 0，157.52s。
- MSVC 同一 CTest matrix：33/33，真实 exit 0，59.46s。

## Scope

- 该 matrix 收口 LSP 03 L6，不替代后续 LSP plans 的 semantic/provider/debug
  milestones。
