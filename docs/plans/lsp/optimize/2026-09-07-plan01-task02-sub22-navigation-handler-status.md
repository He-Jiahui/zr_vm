---
plan_id: optimize
task: plan01-task02-sub22
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_json_rpc.h
  - zr_vm_language_server/stdio/stdio_handler_result.h
  - zr_vm_language_server/stdio/stdio_navigation.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - tests/language_server/test_stdio_handler_cancellation.c
  - tests/language_server/lsp_capability_inventory_probe.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_handler_cancellation
  - language_server_provider_cancellation
  - language_server_stdio_request_progress
  - language_server_stdio_server_lifecycle
  - language_server_stdio_protocol_conformance
  - language_server_stdio_protocol_inventory
  - language_server_stdio_optional_capabilities_smoke
  - language_server_stdio_navigation_capabilities_smoke
doc_type: plan-record
---

# Plan 01 Task 2 Sub22: Navigation Handler Status

## 状态与产出记录

- 开始时间: 2026-09-07 10:47 +08:00
- 实际完成时间: 2026-09-07 14:29 +08:00
- 状态: 已完成
- 源码版本: 开始于 `bfdfc333`，验证时为 `b6c7086e` 加本子项和其他任务的共享源码 overlay
- 完成项目: navigation 十个 handler 显式返回 status/result，直接回归与 ownership 验证
- 产出路径: stdio handler/dispatcher/header、两个测试、模块文档和本记录

## 缺陷与修复

dispatcher 原来把所有 handler 的 C null pointer 推断成 InvalidParams，因而合法请求
在 JSON 分配失败时返回错误分类。GCC RED 在真实 dispatcher/handler 上安装 cJSON
失败 allocator，hover 的预期为 `INTERNAL_ERROR(4)`，实际为 `INVALID_PARAMS(1)`；
当时九项测试中只有该项失败，正常请求和原有取消清理仍通过。

`SZrLspHandlerResult` 将状态与 JSON 结果分离。hover、rich hover、signature help、
inlay hint、definition、native declaration document、references、document symbols、
workspace symbols 和 document highlights 明确区分参数错误、成功空值、内部错误与取消。
dispatcher 对这十条分支直接转发状态，其他分支暂时保留原接口。

hover/rich hover/signature help 删除序列化失败后补造 JSON null 的 fallback。
native declaration 的文本转换失败和 workspace query string 的分配失败明确返回内部错误。
provider 失败时也释放已构造的结果。共享 helper 在转移 JSON ownership 前检查 request
callback，取消时删除 JSON，并返回 `CANCELLED` 和 null pointer。既有请求编排层据此
发送 `-32800` 或 `-32603`，参数错误保持 `-32602`。

## Lifetime、Exactness 与 Ownership

provider 输出归 handler 清理；其中 runtime string 仍为借用。JSON 成功结果的所有权
由 handler 经 dispatcher 转移给请求编排层，错误结果不携带 JSON。取消检查使用当前
context 的借用 callback，不引入后台线程或额外 request identity。

测试沿用真实 server/context 构造和 teardown、tracking allocator 与同步取消校准。
四个已迁移的部分结果用例现在要求显式 CANCELLED；rename 尚未迁移，保留其既有空值
与编排层取消处理。新增矩阵遍历全部十个方法，覆盖持续 JSON 分配失败、仅首次分配
失败、已取消请求、无效参数与合法请求。仅首次失败测试防止成功 null fallback 掩盖错误。
能力清单继续包含生产 dispatcher，只更新十个 stub 的签名。

## 验证命令及结果

```text
GCC build: /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc
Clang ASan/UBSan build: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
MSVC Debug build: .codex/lsp-optimize-validation/msvc-current

cmake --build <build> --target zr_vm_language_server_stdio_handler_cancellation_test \
  zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_inventory_probe -j4
  all three builds passed

ctest --test-dir <build> --output-on-failure \
  -R "^language_server_(provider_cancellation|stdio_(handler_cancellation|request_progress|protocol_conformance|server_lifecycle|optional_capabilities_smoke|protocol_inventory|navigation_capabilities_smoke))$"
  GCC: 8/8 passed, exit 0
  MSVC: 8/8 passed, exit 0
  Clang: 7/8 passed; inventory rejected Node 12 before worker probe
  Clang inventory after Node 22 configuration: 1/1 passed, exit 0
  all three handler executables: 11/11 Unity cases passed

valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_handler_cancellation_test
  11/11 passed, exit 0
  356758 allocs, 356758 frees; 0 bytes in 0 blocks at exit
  ERROR SUMMARY: 0 errors from 0 contexts; no suppressions
```

MSVC build/CTest 通过 `Invoke-VsDevCommand.ps1`。Clang 保留 address/undefined
sanitizer、frame pointer 和 executable `-no-pie`；本次通过的测试未报告 sanitizer 错误。
Clang inventory 的失败来自配置中的 `/usr/bin/node` v12.22.9，要求 Node 18+。
构建目录已重新配置为 GCC 同用的本地 Node 22.13.1，源码与断言没有改变。
随后单独重跑 `language_server_stdio_protocol_inventory` 通过，因此八个目标均有当前
源码的通过证据。其余七项不因 Node 配置修正重复执行。

## 剩余门槛

本子项只接受十个导航 handler 的显式状态迁移和上述 fault injection 边界。
内部 JSON 字段/条目分配失败、URI parser/runtime provider 分配失败的完整分类以及其他
handler 迁移仍 pending；不得以当前 root-allocation 测试声称任意 OOM 都已闭合。
Plan 01 Task 2、Task 4 和 Task 6 的完整门禁继续 pending。
