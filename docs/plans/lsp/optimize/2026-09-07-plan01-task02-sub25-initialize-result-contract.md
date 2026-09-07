---
plan_id: optimize
task: plan01-task02-sub25
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_json_builder.h
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/stdio_initialize.c
  - zr_vm_language_server/stdio/stdio_initialize_capabilities.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_semantic_tokens_json.c
  - zr_vm_language_server/stdio/stdio_completion_json.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - tests/language_server/test_stdio_initialize.c
  - tests/language_server/test_stdio_optional_capability_allocations.c
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_initialize
  - language_server_stdio_optional_capability_allocations
  - language_server_stdio_protocol_conformance
doc_type: plan-record
---

# Plan 01 Task 2 Sub25: Initialize Result Contract

## 状态与产出记录

- 开始时间: 2026-09-07 15:21 +08:00
- 实际完成时间: 2026-09-07 18:30 +08:00
- 状态: 已完成
- 源码版本: `b33adec1` 加共享工作树 overlay
- 完成项目: initialize 显式 status/result、嵌套 JSON 故障原子失败、取消回滚、生命周期失败重试及三工具链回归
- 计划产出: 显式 initialize 状态、完整 JSON 构造失败检查、生命周期回归和模块文档

初始化目前只检查最外层对象分配，嵌套 capability、legend 和 file-operation 的
构造错误可能留下缺项结果或泄漏；controller 在结果构造前进入 INITIALIZING，
失败后普通请求仍可能被放行。本子项先建立真实 handler 与 controller 回归，逐一
注入成功初始化路径中的 JSON 分配失败，再收敛结果所有权和状态转换。

协议依据为 [LSP 3.17 initialize](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/general/initialize.md)：
成功 InitializeResult 是进入常规通信的前提。失败时保留未初始化状态；成功后仍
拒绝重复 initialize。工作区 runtime 分配细分、response frame 构造失败和其他
handler 的嵌套序列化故障继续按父计划独立验收。

## RED 与实现

初始 GCC 回归 8 项中 7 项失败。四个 JSON fault sweep 都在第 20 次分配失败时
仍返回 OK；直接 handler 对空参数和预取消同样返回 OK。controller 帧测试还暴露
序列化输出使用 `free` 而非 cJSON hook 的配对释放问题。代码审查确认原 controller
在调用 handler 前进入 INITIALIZING，失败路径没有恢复 NEW。

initialize 现在返回 `SZrLspHandlerResult`。所有嵌套对象、字段、字符串和数组条目
构造都检查返回值；新增私有 consuming attachment helper，在挂接失败时释放
尚未移交所有权的 item。树上已挂接的子节点统一随根结果释放。语义令牌 legend、
completion commit characters、advanced editor 和 file-operation 注册表同样
完整检查失败，错误不再形成部分能力声明。

optional editor builder 改为返回成功标志，只在构造全部完成后更新两个 dispatch
flag；其调用者丢弃失败的 capability 树。initialize handler 在 JSON 错误或取消
时恢复原 position encoding 和 flags。此前 optional allocation 回归的降级成功
预期更新为整体构造失败，保留全部六个具体分配点的十二次故障检查。

controller 绑定真实 request cancellation callback，只有 handler 成功且当前请求
未取消才进入 INITIALIZING。新回归捕获并解析生产 controller 的 Content-Length
帧，验证 `-32603` / `-32800` 后仍在 NEW，普通请求返回 `-32002`，重试可以成功，
成功后重复 initialize 仍返回 `-32600`。输出 buffer 使用 `cJSON_free` 配对。

## 故障扫描与结果

完整可选能力配置有 227 次 JSON 分配，基础配置有 223 次。每个分配点分别注入
单次失败和从该点起持续失败，共 900 次注入，均要求 INTERNAL_ERROR、空 result、
协商字段恢复及活动 JSON 分配归零。额外测试覆盖成功 capability 树、无效参数、
预取消、最后一次 JSON 分配时取消，以及 controller 生命周期。最终为 10 项 Unity。

GCC、Clang ASan/UBSan 和 MSVC 的相关 CTest 组均通过 12/12，耗时分别为 29.44、
49.05 和 51.56 秒；GCC 初始化 Valgrind 通过 10/10，421,115 次分配与释放完全配对，
退出时 0 字节/0 块，0 errors，未使用 suppression。证据日志位于
`.codex/lsp-optimize-validation/` 下的 `plan01-task02-sub25-{gcc,clang,msvc}-ctest.log`
和 `plan01-task02-sub25-valgrind.log`。
其余工具链结果待当前回归结束后补齐。

## 接受边界

本子项接受 initialize 的显式状态、JSON 构造完整性、已覆盖的取消清理和失败重试。
工作区 runtime 初始化仍有 void API，内部失败细分和事务回滚继续待办；response
envelope、serialization/write 错误以及最终 publication fence 继续独立推进。
其他 handler 的内部 JSON/runtime 分配分类、诊断/泛型语义 smoke 以及 Plan 01
父级门禁尚未关闭。
