---
plan_source: docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
scope: task-6-sanitizer-replay
tests:
  - language_server_stdio_server_lifecycle
  - language_server_stdio_protocol_conformance
  - language_server_stdio_document_sync_conformance
  - language_server_stdio_resolve_capabilities_smoke
  - language_server_stdio_optional_capabilities_smoke
  - language_server_stdio_save_capabilities_smoke
  - language_server_stdio_client_commands_smoke
  - language_server_stdio_file_operation_capabilities_smoke
  - language_server_stdio_workspace_folders_smoke
doc_type: acceptance-record
---

# Plan 01 Task 6: Current Sanitizer Replay

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-09-06 08:47 +08:00 | ext4 replay green; mounted-path timeout boundary; parent pending | 在当前源码上重放 stdio lifecycle、完整 30-case protocol conformance、GCC Valgrind 生命周期和 MSVC stdio capability smoke。Plan 01 Task 6 仍等待完整跨工具链门禁汇总，父任务不提前勾选。 | WSL ext4 上 Clang 14 ASan+UBSan lifecycle、document sync 和 protocol driver 连续 3 次各 30/30 通过；GCC Valgrind 0 errors/0 leaks；MSVC native CTest 7/7 通过。 |

## Clang ASan/UBSan

构建目录为 `.codex/lsp-optimize-validation/clang-asan-current`，配置为 Clang
Debug、shared libraries、ASan+UBSan、`-fno-omit-frame-pointer`，并执行构建和
focused CTest：

```text
cmake --build /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --target zr_vm_language_server_stdio zr_vm_language_server_stdio_server_lifecycle_test \
  --parallel 4
ctest --test-dir /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current \
  --output-on-failure -R "language_server_stdio_(protocol_conformance|server_lifecycle)"
```

可执行文件的 ELF 类型为 `EXEC (Executable file)`。在
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:fast_unwind_on_fatal=0`
和 `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` 下，生命周期循环输出
`Pass - stdio server lifecycle`。将 2 个可执行文件和 9 个 shared libraries
复制到 WSL ext4 并逐个 `cmp` 后，生命周期测试、document sync conformance
和 protocol driver 均通过；protocol driver 连续 3 次退出码均为 0，每次
30/30 cases 通过。

当前 WSL2 的 Clang 14 ASan runtime 在 PIE 可执行文件启动期间会与其 64-bit
allocator 保留区发生非确定性地址冲突，崩溃发生在 `main` 之前。隔离的最小
控制程序在 PIE 形态 30 次中有 5 次初始化崩溃，`-no-pie` 形态 30/30 到达
`main`；因此本次验证链接使用 `-no-pie`。该选项只固定可执行文件地址布局，
没有关闭 ASan/UBSan、LeakSanitizer 或错误报告。直接在 `/mnt/e` 挂载构建上
首次 focused CTest 为 2/2，但后续组合回放出现 4 项、以及单独回放出现 1 项
3 秒响应超时；失败发生在不同的初始化/取消用例。相同二进制和库转到 ext4
后连续回放稳定通过，因此该挂载路径时序边界保留为未闭合证据，不计入稳定
sanitizer 协议门禁。

## GCC Valgrind

对 `/home/hejiahui/.codex-builds/lsp-optimize-20260905-root/gcc` 的未插桩
Debug 生命周期测试执行：

```text
valgrind --leak-check=full \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  .../bin/zr_vm_language_server_stdio_server_lifecycle_test
```

结果为 `Pass - stdio server lifecycle`、`in use at exit: 0 bytes in 0 blocks`、
`All heap blocks were freed`、`ERROR SUMMARY: 0 errors from 0 contexts`。
测试进程共完成 384,948 次分配并全部释放。

## MSVC stdio smoke

在 `.codex/lsp-optimize-validation/msvc` 的 MSVC Debug 缓存中，原生 Windows
CTest 选择以下七项并全部通过（7/7）：protocol conformance、resolve
capabilities、optional capabilities、save capabilities、client commands、file
operation capabilities、workspace folders。

## 范围边界

本记录只确认当前 Task 6 的 sanitizer/stdio replay 证据，不勾选 Plan 01 的
Task 1-6 父级复选框。完整计划仍需将 GCC/Clang/MSVC 的同一提交、Valgrind、
测试矩阵和后续模块文档汇总到统一验收门禁；WASM linked asset 也仍受隔离构建
内存限制影响。
