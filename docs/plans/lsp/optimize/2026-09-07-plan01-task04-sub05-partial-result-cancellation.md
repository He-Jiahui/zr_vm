---
plan_id: optimize
task: plan01-task04-sub05
status: completed
related_code:
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_request_progress.c
  - zr_vm_language_server/stdio/stdio_request_progress.h
  - tests/language_server/stdio_protocol_conformance.js
  - tests/language_server/test_stdio_request_progress.c
  - tests/cmake/zr_vm_lsp_stdio_progress_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_protocol_conformance
  - language_server_stdio_server_lifecycle
  - language_server_stdio_optional_capabilities_smoke
  - language_server_stdio_request_progress
doc_type: plan-record
---

# Plan 01 Task 4 Sub05: Partial Result Cancellation

## 状态与产出记录

- 开始时间: 2026-09-07 08:30 +08:00
- 实际完成时间: 2026-09-07 09:50 +08:00
- 状态: 已完成
- 实现提交: `10fe1e20`
- 产出路径: progress 模块、请求编排、协议回归、模块文档与本记录

## 缺陷与实现

`handle_request_message` 原先在 handler 返回后立即清除 context cancellation
callback，而 partial-result sink 随后仍通过该 callback 查询取消。输入线程能将
精确请求 ID 标记为 cancelled，但已经进入结果发送阶段的请求继续输出全部批次，
最终返回成功 `null`。

work-done/partial-result 编排现已独立到 `stdio_request_progress.c/.h`。请求 callback
保留到 snapshot 校验、partial 发布和最终状态判定结束；所有提前返回路径均清除它。
每批 partial 发送前后、替换最终结果前和最终状态判定时检查取消，因此最后一批发送
期间观测到的取消同样映射为 `-32800 Request cancelled`。已发送的通知无法撤回，
未完成的原始结果由请求清理路径释放。成功数组和 workspace diagnostic `{items}`
schema、work-done begin/end、token identity 和无 partial token 的普通响应保持原契约。

## RED / GREEN

新增协议用例打开含 1024 个类的文档，等待对应 URI/version 的 diagnostics 后发起
带 partial token 的 workspace symbol 请求。客户端收到第一批 64 个结果后，才发送
匹配 ID 的 `$/cancelRequest`，要求最终收到完整 `-32800` 错误 envelope。

- RED: 旧 GCC 服务器收到取消后仍返回 `result: null`，错误 envelope 断言失败。
- GREEN: `10fe1e20` 的 GCC focused case 通过；完整协议回放 52/52 通过。
- 初始 4096 项 fixture 在查询阶段超时，未进入 partial sink，排除在 RED 证据之外。

该端到端用例检查活动结果流中的取消与错误分类；它不测量 provider 查询循环的
取消延迟，也不要求输出管道中已发送的消息被撤回。

另有六项 C 回归直接链接 progress 模块与真实 registry，以同步通知接收器在指定
批次标记取消，覆盖首批停止、末批取消、workspace diagnostic 末批取消、数字 `7`
与字符串 `"7"` 隔离、成功 `{items}` 的完整顺序、未提供 partial token 的普通结果。
取消路径保留原结果供调用方清理，成功路径替换为 JSON null，work-done begin/end
和 token 清理均有断言。该接收器只存在于测试目标，生产 transport 无测试钩子。

## 验证命令及结果

```text
WSL GCC Debug
cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --target zr_vm_language_server_stdio zr_vm_language_server_stdio_request_progress_test -j2
  passed
node /mnt/e/Git/zr_vm/tests/language_server/stdio_protocol_conformance.js \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio
  52/52 passed, exit 0
ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure \
  -R "^language_server_stdio_(protocol_conformance|server_lifecycle|optional_capabilities_smoke|request_progress)$"
  4/4 passed, exit 0
bin/zr_vm_language_server_stdio_request_progress_test
  6/6 passed, exit 0

Clang 14 Debug ASan/UBSan
isolated build: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
cmake --build <build> --target zr_vm_language_server_stdio \
  zr_vm_language_server_stdio_server_lifecycle_test \
  zr_vm_language_server_stdio_request_progress_test -j4
  853/853 passed
node tests/language_server/stdio_protocol_conformance.js <build>/bin/zr_vm_language_server_stdio
  52/52 passed, exit 0, no sanitizer diagnostic
ctest --test-dir <build> --output-on-failure \
  -R "^language_server_stdio_(protocol_conformance|server_lifecycle|optional_capabilities_smoke|request_progress)$"
  4/4 passed, exit 0, no sanitizer diagnostic
<build>/bin/zr_vm_language_server_stdio_request_progress_test
  6/6 passed, exit 0, no sanitizer diagnostic

MSVC Debug
build: .codex/lsp-optimize-validation/msvc-current
Invoke-VsDevCommand.ps1 cmake --build <build> --target zr_vm_language_server_stdio \
  zr_vm_language_server_stdio_server_lifecycle_test zr_vm_language_server_stdio_request_progress_test -j2
  passed
node tests/language_server/stdio_protocol_conformance.js <build>/bin/zr_vm_language_server_stdio.exe
  52/52 passed, exit 0
Invoke-VsDevCommand.ps1 ctest --test-dir <build> --output-on-failure \
  -R "^language_server_stdio_(protocol_conformance|server_lifecycle|optional_capabilities_smoke|request_progress)$"
  4/4 passed, exit 0
<build>/bin/zr_vm_language_server_stdio_request_progress_test.exe
  6/6 passed, exit 0
```

Clang 配置保留 `-fsanitize=address,undefined -fno-omit-frame-pointer` 和 executable
`-no-pie`。最初 `/tmp/zr-lsp-plan01-sub22-clang-20260907` 构建结束后目录已不存在，
该次结果不计入 sanitizer 通过；上述用户目录构建重新完成全部构建与运行验证。

GCC 验证使用包含其他活动任务 overlay 的共享源码工作树。Clang ext4 目录隔离
构建产物；源码仍来自同一工作树，因此此处是本子项验证，不作为冻结提交的全仓验收。

## 接受边界

接受 Plan 01 Task 4 Sub05；实现提交 `10fe1e20`，确定性 C 回归、模块文档与本记录
由后续同一阶段提交补齐。`git diff --check` 和 JavaScript syntax check 通过。

Task 4 的 provider 长循环 cancellation、dependency/content fence 和完整统一
progress sink 父门禁保持 pending；Task 6 仍需要当前版本的完整矩阵。
