---
plan_id: optimize
task: plan01-task02-sub26
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_json_builder.h
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - tests/language_server/test_stdio_transport_output.c
  - tests/language_server/test_stdio_request_progress.c
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_transport_output
doc_type: plan-record
---

# Plan 01 Task 2 Sub26: Response Envelope Ownership

## 状态与产出记录

- 开始时间: 2026-09-07 18:40 +08:00
- 实际完成时间: 2026-09-07 19:22 +08:00
- 状态: 已完成
- 源码版本: `342cbd17` 加共享工作树 overlay
- 完成项目: response envelope 原子构造、JSON 所有权、传输状态和三工具链故障注入回归
- 计划产出: envelope 构造故障回归、传输返回状态、所有权与验证文档

生产传输的 result/error/notification envelope 未检查嵌套 JSON 分配与挂接，
可能发送不完整消息或遗失传入结果。先对每个输出分配点注入 transient/persistent
故障，要求写出零字节且释放全部 JSON；成功控制必须保持完整 envelope 和 ID。
随后区分构造失败与 I/O 失败。生命周期、进度和诊断缓存对发送状态的消费按后续
子项推进，避免把传输契约与 runtime 事务混为同一验收。

## 实现

`send_json_message`、`send_result_response`、`send_error_response` 和
`send_notification` 现在都返回 `EZrStdioSendStatus`。`BUILD_ERROR` 表示 JSON
树或 frame payload 无法完整构造，`IO_ERROR` 表示 `fprintf`、`fwrite` 或 `fflush`
未能发布完整 frame。每个 API 在任一返回状态下都消费其拥有的 JSON 参数；payload
继续用 `cJSON_free` 释放，以配对 cJSON hook allocator。

response envelope 先验证 `jsonrpc` 与复制的 typed id 都已加入根对象，再接管 result
或 error。notification 同样在 method 和 params 都能完整挂接后才发送。`NULL`
result/params 必须成功创建 JSON null，否则整个 envelope 失败；挂接失败由 consuming
helper 删除尚未移交的 child。这样构造失败不会向 stdout 写任何 header 或 payload，也
不会遗失 caller 传入的 result。

`test_stdio_request_progress.c` 的 transport receiver 同步为返回成功状态，使该测试
继续替换真实 transport，并验证接口变更在现有 progress 链接路径中完整生效。

## 故障注入与验证

新的 `language_server_stdio_transport_output` Unity 测试捕获生产 stdout frame，并让
result、null result、error、notification 和 null notification 的所有输出分配点分别
经历单次和持续故障。五个路径合计 58 个 allocation points，产生 116 次注入；每次
构造失败都要求 `BUILD_ERROR`、零字节输出和零个活动 hook allocation。成功控制通过
生产 frame reader 回读，校验 JSON-RPC version、typed id、result/error/method/params
的完整形状。numeric、null 与 escaped string id 都会保持原类型和值。

测试还把 stdout 重定向到只读文件，确认实际 flush/write failure 返回 `IO_ERROR` 且
释放所有 JSON。GCC、Clang ASan/UBSan 和 MSVC Debug 的 focused CTest 均为 1/1；GCC
相关 stdio CTest 为 4/4。GCC Valgrind 的 7 项 Unity 测试完成 1,563 次分配/释放，
退出时 0 bytes in 0 blocks，ERROR SUMMARY 为 0。日志位于
`.codex/lsp-optimize-validation/plan01-task02-sub26-{gcc,clang,msvc}-ctest.log`、
`plan01-task02-sub26-gcc-related-ctest.log` 与
`plan01-task02-sub26-gcc-valgrind.log`。

## 接受边界

本子项只接受传输层的完整 envelope、所有权与输出状态。initialize/shutdown 的
lifecycle transition、progress publication 和 diagnostics push cache 目前尚未根据
`EZrStdioSendStatus` 提交或回滚自身状态；这些调用方的 publication fence 继续作为
后续 Task 2 子项。其他 handler 的嵌套 serializer/runtime 分类，以及 Plan 01 父级
门禁同样保持 pending。
