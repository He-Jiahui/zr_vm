---
related_code:
  - zr_vm_language_server/stdio/stdio_server.h
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - tests/language_server/test_stdio_server_lifecycle.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_server_lifecycle
doc_type: plan-record
---

# Plan 01 Task 5: Deterministic Teardown Current Replay

## 状态与产出记录

- 开始时间: 2026-09-07 05:10 +08:00
- 实际完成时间: 2026-09-07 05:18 +08:00
- 状态: 已完成
- 源码版本: 当前 `main` 的 `2f94ce94`；本记录只记录当前源树的 replay，不改写早期
  teardown 实现的 commit identity
- 产出路径: 当前 lifecycle 回归、计划勾选、模块文档与本记录

本次 replay 将 Task 5 的 deterministic teardown 合同映射到当前源树。它确认
server 仍是 heap-owned runtime root，reader 的线程所有权仍由 input state 保存，
普通释放和构造失败都经过同一 ordered path；Plan 01 Task 6 的完整跨工具链门禁
仍单独保留，不能由本记录替代。

## 覆盖边界

- `test_stdio_server_lifecycle.c` 在同一进程中连续执行 100 次
  `New/Start/Shutdown/Free`，没有把进程退出当作资源释放机制；
- reader 输入覆盖有效 `exit` 帧停止路径，`Free` 先 stop、再 join，再清空入队消息，
  最后销毁同步原语、registry、缓存、LSP context 和 global state；调用方提供的
  `FILE *` 仍由调用方关闭；
- 构造失败覆盖 global、context、input initialization，reader 启动失败覆盖
  `ZR_STDIO_SERVER_FAULT_AFTER_READER_START`，每个已初始化资源都回到同一个
  `ZrLanguageServer_StdioServer_Free` 路径；
- `SZrStdioRequestInputState` 保留 Win32 `HANDLE` 或 POSIX `pthread_t`，不会在创建
  后 detach 或丢弃 join 所有权。跨平台 join 和历史 leak/race 证据仍见
  [2026-08-22 Task 5 record](2026-08-22-stdio-deterministic-teardown.md)。

## 验证命令及结果

```text
wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  ninja: no work to do
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure -R "^language_server_stdio_server_lifecycle$"'
  1/1 passed
wsl.exe bash -lc '/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_server_lifecycle_test'
  Pass - stdio server lifecycle

wsl.exe bash -lc 'cmake --build /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current --target zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
  ninja: no work to do
wsl.exe bash -lc 'ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current --output-on-failure -R "^language_server_stdio_server_lifecycle$"'
  1/1 passed, no sanitizer diagnostic
wsl.exe bash -lc '/mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current/bin/zr_vm_language_server_stdio_server_lifecycle_test'
  Pass - stdio server lifecycle
```

## 接受决定

接受当前源树的 Plan 01 Task 5 deterministic teardown 条目。当前 replay 与历史
Valgrind/Helgrind、MSVC join 证据共同覆盖 server ownership、100-cycle release、
fault-injection cleanup 和 Win32/pthread join；Task 6 的完整 protocol、sanitizer、
Valgrind 与 MSVC 汇总仍保持 pending，直到同一当前提交的全量证据重新收齐。
