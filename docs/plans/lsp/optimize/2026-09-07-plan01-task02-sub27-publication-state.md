---
plan_id: optimize
task: plan01-task02-sub27
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_request_progress.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - tests/language_server/test_stdio_initialize.c
  - tests/language_server/test_stdio_request_progress.c
  - tests/language_server/test_stdio_diagnostic_publication.c
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_initialize
  - language_server_stdio_request_progress
  - language_server_stdio_diagnostic_publication
doc_type: plan-record
---

# Plan 01 Task 2 Sub27: Publication State

## 状态与产出记录

- 开始时间: 2026-09-07 19:25 +08:00
- 实际完成时间: 2026-09-07 20:06 +08:00
- 状态: 已完成
- 源码版本: `bc49ad11` 加共享工作树 overlay；本子项从 `1bceb984` 接续
- 完成项目: 生命周期与通知发布状态处理、三工具链回归、逐分配点故障注入和内存验证
- 计划产出: publication-state 回归、JSON 构造故障注入、模块文档与验证记录

## 失败证据与实现

Sub26 提供了输出构造和 I/O 状态，但调用方仍把失败发布当作成功。GCC RED 中
progress 的两个新用例错误地返回 TRUE；initialize 写出失败后已离开 NEW，shutdown
写出失败后已进入 SHUTDOWN；清空诊断通知写出失败后，去重缓存 count 已变为 1。
三个 CTest 目标共五个新断言失败，既有用例与成功控制保持通过。

`handle_request_message` 现在先检查可用状态和取消，再写出 initialize/shutdown
成功响应，只有 `ZR_STDIO_SEND_OK` 才推进生命周期。取消的 shutdown 保持之前的
INITIALIZING/RUNNING 状态。测试覆盖真正只读 stdout、response envelope 根分配失败、
取消和随后成功重试。

work-done 和 partial result 发送返回真实 transport 状态。begin 通知发布失败时不设置
`workDoneBegan`；任一 partial batch 发布失败时停止后续批次，保留原结果，由 controller
返回内部错误。work-done end 无论能否发出都清除当前请求的借用 token，防止污染后续请求。
progress JSON 的 token、value、kind/title/cancellable、batch 和 workspace items 包装
都检查构造或挂接失败，未成功移交的节点由 consuming helper 释放。

普通和清空诊断通知仅在完整构造并成功发送后更新去重缓存。失败不会抑制后续重发，
也不会覆盖已经成功发布的文档版本。测试通过生产 frame reader 解析成功通知，检查
method、URI、diagnostics array 和版本；失败构造必须没有输出并且零个活动 cJSON 分配。

## 验证

GCC、Clang ASan/UBSan 和 MSVC Debug 的相同七个 CTest 目标全部通过，包括
initialize、request progress、diagnostic publication、handler cancellation、transport
output、protocol conformance 和 protocol envelope mutations。各组耗时分别为
16.87、30.75、25.12 秒，未放宽现有协议测试断言或 sanitizer 选项。

直接 Unity 回归分别为 initialize 15/15、progress 11/11、diagnostic publication 5/5。
work-done begin 为 15 个分配点，65 项普通 partial result 为 80 个，workspace diagnostic
partial result 为 84 个；清空和有版本的文档诊断通知分别为 16/18 个，共 213 个分配点。
每点各注入一次单次故障与持续故障，共 426 次新增注入。失败 partial 保留原结果，
之前已经发布的批次仍为完整且有序的 64/1 项；失败通知不进入诊断缓存。

GCC Valgrind 对三个直接测试均返回 0，均为 `0 bytes in 0 blocks` 和 `0 errors`：

| 目标 | 分配数 | 释放数 |
| --- | ---: | ---: |
| initialize | 583,127 | 583,127 |
| request progress | 42,298 | 42,298 |
| diagnostic publication | 160,432 | 160,432 |

构建目录：GCC 为 `/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc`；Clang 为
`/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`；MSVC 为
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`。后者通过
`Invoke-VsDevCommand.ps1 -Command @('cmake', ...)` 和同一 wrapper 的 `ctest` 运行。

复验命令的 GCC 形式如下；Clang 使用上述目录，MSVC 另加 `-C Debug`：

```sh
cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --parallel 8 --target zr_vm_language_server_stdio zr_vm_language_server_stdio_initialize_test zr_vm_language_server_stdio_request_progress_test zr_vm_language_server_stdio_diagnostic_publication_test zr_vm_language_server_stdio_transport_output_test zr_vm_language_server_stdio_handler_cancellation_test
ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure -R '^language_server_stdio_(initialize|request_progress|diagnostic_publication|handler_cancellation|transport_output|protocol_conformance|protocol_envelope_mutations)$'
valgrind --leak-check=full --errors-for-leak-kinds=definite,indirect --error-exitcode=99 /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_diagnostic_publication_test
```

构建和 CTest 日志位于 `.codex/lsp-optimize-validation/plan01-task02-sub27-` 前缀下的
`{gcc,clang,msvc}-{build,ctest}.log`。Valgrind 以同样参数分别运行三个测试，日志为
`plan01-task02-sub27-gcc-{initialize,progress,diagnostics}-valgrind.log`。

## 接受边界

此项接受生命周期状态、work-done/partial 发布状态和诊断去重缓存的提交条件。
I/O 错误可能在部分字节写出后才被报告；已经写入的字节不能回滚。服务器初始化
工作区/selected-project 的运行时失败回滚、诊断内容及其他 handler 的嵌套 serializer
错误分类、Plan 01 完整协议与 smoke 门禁继续待办。
