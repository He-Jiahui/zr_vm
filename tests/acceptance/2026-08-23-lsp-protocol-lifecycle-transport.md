# LSP Protocol Lifecycle And Transport Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 03:54 +08:00 | completed | 已验收 LSP 计划 01 的 Task 1-4：生命周期、JSON-RPC 信封、受限 frame reader、typed request id/取消/progress，以及 document-sync 串行语义。 | GCC Debug shared、Clang Debug static ASan+UBSan、MSVC Debug static 均通过相同 5 个 CTest；protocol conformance 为 29/29。 |

## Checked Behaviors

- 初始化前 request 返回 `-32002`，初始化前 notification 不产生文档状态；第二次 initialize 和 shutdown 后 request 返回 `-32600`。
- 无效 JSON-RPC 信封和参数保持精确错误码，trace 和诊断日志不污染 stdout frame。
- malformed、truncated、oversize frame 均以受分类的 stderr 诊断和非零退出结束。
- numeric/string JSON-RPC id 不冲突，unknown cancellation 无响应，known active id cancellation 返回 `-32800`。
- `didChange` version 2 替换 version 1 的 workspace symbol 内容；Plan 02 fence 前不得发布推测性的 `-32801`。
