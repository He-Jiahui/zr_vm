---
related_code:
  - zr_vm_language_server/stdio/stdio_lifecycle.c
  - zr_vm_language_server/stdio/stdio_lifecycle.h
  - tests/language_server/test_stdio_server_lifecycle.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 1 Sub02: Lifecycle State Transitions

## 状态与产出记录

- 开始时间: 2026-09-07 04:14 +08:00
- 实际完成时间: 2026-09-07 04:16 +08:00
- 状态: 已完成
- 源码版本: 基于 `ea9a8a76` 的当前工作树；本记录与回归测试由同一阶段提交固化
- 产出路径: `tests/language_server/test_stdio_server_lifecycle.c`、模块文档、计划索引与本记录

本子项为生命周期模块增加直接状态转移回归，而不是只通过 stdio 协议间接观察状态。
它覆盖 `initialized` 到达标记、重复通知、非法重初始化、shutdown 后请求门禁和
两类 exit 退出码；Task 1 的协议负向矩阵和 Plan 01 跨工具链父级门禁仍保持 pending。

## 覆盖边界

测试确认：

- `Init` 从 `NEW` 开始，且 `initializedNotificationReceived` 清零；
- `MarkInitialized` 在 `NEW` 和 `RUNNING` 中不改变状态，只允许
  `INITIALIZING -> RUNNING` 并记录通知到达；
- `INITIALIZING`/`RUNNING` 可处理普通请求，`NEW`/`SHUTDOWN`/`EXITED` 不可处理；
- `BeginShutdown` 只接受活动状态，`BeginInitialize` 不允许绕过已开始的生命周期；
- 成功 shutdown 后 `Exit` 返回 0，未 shutdown 或重复 exit 返回 1，并进入 `EXITED`。

## 验证命令及结果

```text
wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  passed
wsl.exe bash -lc '/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_server_lifecycle_test'
  Pass - stdio server lifecycle

wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  passed
wsl.exe bash -lc '/mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio_server_lifecycle_test'
  Pass - stdio server lifecycle
```

GCC 与 Clang ASan/UBSan 均通过同一 100 次 New/Start/Shutdown/Free 回归及新增状态
断言。该阶段没有修改生命周期实现；直接测试确认现有实现满足本子项的状态契约。
`git diff --check` 对本子项源文件和文档无空白错误。

## 接受决定

接受 Plan 01 Task 1 Sub02。`initialized` 到达记录和状态转移现在有独立的 C 回归证据；
Task 1 的完整协议矩阵、隐式状态清理审计、Windows join 与 Plan 01 父级门禁仍需后续验收。
