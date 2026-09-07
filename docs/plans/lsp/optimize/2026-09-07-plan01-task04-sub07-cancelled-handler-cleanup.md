---
plan_id: optimize
task: plan01-task04-sub07
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_navigation.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/stdio_lsp_memory.c
  - tests/language_server/test_stdio_handler_cancellation.c
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
  - tests/CMakeLists.txt
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_handler_cancellation
  - language_server_provider_cancellation
  - language_server_stdio_request_progress
  - language_server_stdio_server_lifecycle
  - language_server_stdio_protocol_conformance
  - language_server_stdio_optional_capabilities_smoke
doc_type: plan-record
---

# Plan 01 Task 4 Sub07: Cancelled Handler Cleanup

## 状态与产出记录

- 开始时间: 2026-09-07 10:13 +08:00
- 实际完成时间: 2026-09-07 10:22 +08:00
- 状态: 已完成
- 源码版本: `22a4c3ac` 加本子项 overlay
- 完成项目: 五条 stdio handler 取消失败分支释放部分结果，补充 runtime 分配与 Valgrind 验证
- 产出路径: 两个 handler 文件、独立测试和 CMake 注册、模块文档与本记录

## 缺陷、RED 与修复

Sub06 已证明 provider 的失败返回可能保留一条结果。检查上层 stdio handler 后发现，
references、rename、workspace symbols、document symbols 和 document highlights
直接返回 JSON 空值，没有调用对应数组清理函数。丢失的数组和条目也不会由 context
或 global state 的销毁路径回收。

新增测试首先验证 provider 能产生多条结果，再以回调观察第一条结果，记录该查询
到达的 cancellation 检查次数。随后调用真实 handler，在相同检查点请求取消。
测试使用 tracking allocator，并在真实 `StdioServer_Free` 后要求 runtime 活动
分配数量为零。callback 只在同步查询期间借用测试状态，teardown 先清除 callback。
普通 handler 查询作为独立对照，覆盖同一组查询的正常返回与清理。

MSVC RED: 正常对照通过，五个取消用例均失败，断言为 `Expected 0 Was 2`。缺陷是
部分结果的容器与条目泄漏，而不是无语义结果或测试环境初始化失败。首次测试链接
曾遗漏生产 entry 文件中的公共 helper；补全测试链接后才取得上述有效 RED 证据。

修复在这五条 provider 失败分支调用既有 `free_locations_array`、
`free_symbols_array` 或 `free_highlights_array`。handler 的 JSON 空值约定继续由
请求编排层处理；检测到匹配请求的取消后，最终协议响应仍为 `-32800`。

## Lifetime、Exactness 与 Ownership

provider 输出数组由调用它的 handler 持有，无论返回 true 或 false 都必须清理。
条目的字符串仍归 runtime 所有，因此 handler 只释放条目与数组，不单独释放借用的
字符串。正常、部分结果失败和 teardown 的顺序都使用已有公开释放函数。

测试目标通过生产 stdio target 的 SOURCES 构建。entry 文件还定义公共 URI/string
helper，因此在 tests 目录的 source property 中仅重命名其 main；生产目录的编译
属性不受影响，协议回归验证实际 server 入口。测试中没有启动 reader，也没有向
生产代码添加注入点。GCC/Clang 对测试专用的重命名入口报告 missing-prototype
warning；这不影响生产入口或测试断言。

## 验证命令及结果

```text
GCC build: /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc
Clang build: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
MSVC build: .codex/lsp-optimize-validation/msvc-current

cmake --build <build> --target zr_vm_language_server_stdio_handler_cancellation_test \
  zr_vm_language_server_stdio -j4
  all three builds passed

ctest --test-dir <build> --output-on-failure \
  -R "^language_server_(provider_cancellation|stdio_(handler_cancellation|request_progress|protocol_conformance|server_lifecycle|optional_capabilities_smoke))$"
  GCC: 6/6 passed, exit 0
  Clang ASan/UBSan: 6/6 passed, exit 0, no sanitizer diagnostic
  MSVC Debug: 6/6 passed, exit 0

handler executable / focused verbose CTest:
  GCC, Clang ASan/UBSan, MSVC: 6/6 Unity cases passed

valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_handler_cancellation_test
  6/6 tests passed, exit 0
  194064 allocs, 194064 frees
  in use at exit: 0 bytes in 0 blocks
  ERROR SUMMARY: 0 errors from 0 contexts, no suppressions
```

MSVC 构建和 CTest 通过 `Invoke-VsDevCommand.ps1` 执行。Clang 保留 address/undefined
sanitizer、frame pointer 和 executable `-no-pie`。最后一次测试 allocator 整理后，
GCC 重跑六目标 CTest，Clang 和 MSVC 重建并重跑 handler 目标；生产代码没有再变化。

三工具链源码仍包含其他活动任务 overlay。接受本子项的缺陷修复与证据；完整
handler status/result 迁移、provider 全部扫描路径、50 ms 取消延迟和 Task 6 的
完整生命周期/内存矩阵仍 pending。本次 Valgrind 只验收上述 handler 测试。
