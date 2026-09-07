---
plan_id: optimize
task: plan01-task06-sub01
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_server.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_request_progress.c
  - tests/language_server/test_stdio_server_lifecycle.c
  - tests/language_server/test_stdio_handler_cancellation.c
  - tests/language_server/test_lsp_provider_cancellation.c
  - tests/language_server/test_stdio_request_progress.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_server_lifecycle
  - language_server_stdio_handler_cancellation
  - language_server_provider_cancellation
  - language_server_stdio_request_progress
  - language_server_stdio_protocol_conformance
  - language_server_stdio_optional_capabilities_smoke
  - language_server_stdio_smoke
doc_type: plan-record
---

# Plan 01 Task 6 Sub01: Native Memory Matrix

## 状态与产出记录

- 开始时间: 2026-09-07 10:26 +08:00
- 实际完成时间: 2026-09-07 10:45 +08:00
- 状态: 已完成本子项；Task 6 父门禁未完成
- 源码版本: `4b07a398`，含其他活动任务的共享源码 overlay
- 完成项目: 当前 lifecycle Valgrind、GCC ASan/UBSan 和 MSVC Debug ASan 构建与回放
- 产出路径: 现有回归资产的验证结果、模块文档、Task 6 逐条接受与本记录

## 构建配置与覆盖

| 构建 | 配置 | 本次接受结果 |
| --- | --- | --- |
| `.codex/build-lsp-opt-gcc` | GCC Debug，未插桩 | lifecycle Valgrind 0 bytes / 0 errors |
| `/home/hejiahui/.codex-builds/lsp-plan01-task06-gcc-asan` | GCC 11.4 Debug ASan/UBSan，ext4 | lifecycle/provider/handler/progress 4/4；protocol/optional 2/2 |
| `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang` | Clang 14 Debug ASan/UBSan，ext4 | Sub07 当前六目标 6/6；完整 smoke 的既有语义失败重新确认 |
| `.codex/lsp-asan-msvc` | MSVC 19.44 Debug ASan | 首轮六目标 6/6；复跑 lifecycle 和 optional 通过，protocol 超时 1 项 |

生命周期测试在同一进程执行 100 次 New/Start/Shutdown/Free，覆盖有效 exit 输入、
typed request registry、frame/envelope 状态和四个启动失败点。服务器持有 reader、
registry、缓存、context、global，先 stop/join reader，再释放请求和缓存，最后释放
context/global。输入 FILE 仍由测试调用方关闭。取消相关测试另外验证借用 callback
清除、部分结果释放以及后续查询恢复。

GCC/Clang 的 C 编译标志为 `-fsanitize=address,undefined -fno-omit-frame-pointer`，
executable link 包含 `-fsanitize=address,undefined -no-pie`。MSVC C 编译标志为
`/fsanitize=address`，Debug flags 为 `/MDd /Zi /Ob0 /Od`，EXE/shared Debug link
使用 `/DEBUG /INCREMENTAL:NO`。这避免 `/RTC1` 与 ASan 冲突，仍为 Debug runtime。

## 可复现命令

```text
cmake -S /mnt/e/Git/zr_vm -B /home/hejiahui/.codex-builds/lsp-plan01-task06-gcc-asan \
  -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF -DBUILD_TESTS=ON \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"

Invoke-VsDevCommand.ps1 cmake -S . -B .codex/lsp-asan-msvc -G Ninja \
  -DCMAKE_C_COMPILER=cl -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF -DBUILD_TESTS=ON \
  -DCMAKE_C_FLAGS="/fsanitize=address" -DCMAKE_C_FLAGS_DEBUG="/MDd /Zi /Ob0 /Od" \
  -DCMAKE_EXE_LINKER_FLAGS_DEBUG="/DEBUG /INCREMENTAL:NO" \
  -DCMAKE_SHARED_LINKER_FLAGS_DEBUG="/DEBUG /INCREMENTAL:NO"

cmake --build <build> --target zr_vm_language_server_stdio \
  zr_vm_language_server_stdio_server_lifecycle_test \
  zr_vm_language_server_stdio_handler_cancellation_test \
  zr_vm_language_server_provider_cancellation_test \
  zr_vm_language_server_stdio_request_progress_test -j4

ctest --test-dir <build> --output-on-failure \
  -R "^language_server_(provider_cancellation|stdio_(handler_cancellation|request_progress|protocol_conformance|server_lifecycle|optional_capabilities_smoke))$"

valgrind --leak-check=full --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_server_lifecycle_test
  Pass - stdio server lifecycle
  3301645 allocs, 3301645 frees; in use at exit: 0 bytes in 0 blocks
  ERROR SUMMARY: 0 errors from 0 contexts, no suppressions; exit 0
```

Windows 的 build/CTest 同样通过 `Invoke-VsDevCommand.ps1` 导入 VS 环境。GCC 新构建
完成 907 步，MSVC 完成其全部目标。GCC 实际分两次运行 CTest：4/4 为 7.76 秒，
protocol/optional 2/2 为 39.86 秒；均 exit 0，无 sanitizer 诊断。

MSVC 首轮六目标 6/6 为 116.11 秒，无 ASan 报告。后续三目标复跑为 2/3：lifecycle
37.02 秒通过、optional 14.04 秒通过，protocol 在 shutdown-before-initialize 的
3 秒响应期限内未收到响应，其余 51 个 case 通过，stderr 无 ASan 报告。该时序失败
未归因为内存缺陷，也未通过放宽断言掩盖；协议稳定性仍 pending。

## 完整 Smoke 的边界

Clang 完整 smoke 最初缺少独立目标 `zr_vm_descriptor_plugin_fixture_int`，尚未
进入 server 测试主体。显式传递 CLI 不能替代该共享库，补构建该目标后实际运行：

```text
cmake --build <clang-build> --target zr_vm_cli_executable zr_vm_descriptor_plugin_fixture_int -j4
node tests/language_server/stdio_smoke.js <clang-build>/bin/zr_vm_language_server_stdio \
  <clang-build>/bin/zr_vm_cli
  exit 1: generic completion detail should include the normalized closed instantiation
  tests/language_server/stdio_smoke.js:1973
```

该失败已由 [Plan 00 baseline crosswalk](2026-09-05-plan00-task01-sub01-execution-crosswalk.md)
登记为 Plan 03 compiler/completion consumer 责任，发生在本轮内存修复前。未修改或
弱化该断言，本次不接受完整 smoke。测试所需 CLI 和 descriptor plugin 都已可用，
后续可直接从语义失败继续诊断。

## 接受决定

Task 6 的 Valgrind 与 MSVC Debug ASan lifecycle 两项获得当前版本证据并勾选。
GCC/Clang lifecycle 的 sanitizer 证据也已补齐，但含完整 stdio smoke 的组合条目
仍 pending。protocol 稳定性、全量 handler status/result、50 ms 取消预算及最终
冻结提交验收保持未完成；不以此次子项接受作为向 Plan 02 晋级的依据。
