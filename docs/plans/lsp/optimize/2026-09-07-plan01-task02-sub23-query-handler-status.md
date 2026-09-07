---
plan_id: optimize
task: plan01-task02-sub23
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_hierarchy.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_rename.c
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
  - language_server_stdio_type_hierarchy_smoke
doc_type: plan-record
---

# Plan 01 Task 2 Sub23: Query Handler Status

## 状态与产出记录

- 开始时间: 2026-09-07 14:31 +08:00
- 实际完成时间: 2026-09-07 14:40 +08:00
- 状态: 已完成
- 源码版本: `8e6a4e33` 加本子项和其他活动任务的共享源码 overlay
- 完成项目: 十三个 hierarchy/rename/editor handler 采用显式 status/result
- 产出路径: 三个 handler 模块、dispatcher/header、测试、模块文档和本记录

## RED 与实现

扩大 Sub22 的真实 handler 测试到 23 个方法。GCC RED 为 11 项中三项失败：
`textDocument/prepareCallHierarchy` 的持续/首次 JSON 分配失败均为
`Expected 4 Was 1`，已取消请求为 `Expected 2 Was 0`。无效参数、正常结果及五项
部分结果清理全部通过，故失败来自状态分类。

迁移 prepareCallHierarchy、incomingCalls、outgoingCalls、prepareTypeHierarchy、
supertypes、subtypes、prepareRename、rename、implementation、foldingRange、
selectionRange、documentLink 和 codeLens。handler、内部 helper、声明、dispatcher
和 inventory probe 的签名同步改为 `SZrLspHandlerResult`。

参数错误明确返回 INVALID_PARAMS。provider 正常空结果保留原 null/array 语义，
JSON 根分配失败返回 INTERNAL_ERROR，取消在清理 JSON 后返回 CANCELLED。
prepareRename 与 rename 删除序列化失败后返回成功 null 的 fallback。
selectionRange positions 缓冲区和 rename string 分配失败也有明确内部错误分支。

## Ownership 与测试边界

hierarchy 测试先从真实 prepare 请求获得当前语义 item，再用于后续查询；call/type
参数分别保有各自的 position 和 item。测试 cleanup 释放全部 JSON，context/global
继续负责 runtime 字符串。selectionRange 使用非空 positions，避免只覆盖早退空数组。

所有 provider 输出容器沿用既有对应释放函数。成功 JSON 归请求编排层；取消 JSON
由共享 helper 删除。rename 的同步部分结果测试现在同样要求显式 CANCELLED 和
null pointer，而不是接受旧的 JSON 空值。tracking allocator 在每个测试销毁 server
后要求活动 runtime 分配归零。

本子项没有闭合内部 JSON 字段/条目分配失败、parser/provider 运行时分配失败或
workspace-edit snapshot capture/validate 的细分错误。这些边界保留在父级计划中，
不能用当前根分配注入结果替代完整 fault-injection 验收。

## 验证命令及结果

```text
GCC: /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc
Clang ASan/UBSan: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
MSVC Debug: .codex/lsp-optimize-validation/msvc-current

cmake --build <build> --target zr_vm_language_server_stdio_handler_cancellation_test \
  zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_inventory_probe -j4
  GCC, Clang, MSVC: passed, exit 0

ctest --test-dir <build> --output-on-failure \
  -R "^language_server_(provider_cancellation|stdio_(handler_cancellation|request_progress|protocol_conformance|server_lifecycle|optional_capabilities_smoke|protocol_inventory|navigation_capabilities_smoke|type_hierarchy_smoke))$"
  GCC: 9/9 passed, exit 0 (29.74 seconds)
  Clang ASan/UBSan: 9/9 passed, exit 0 (47.60 seconds)
  MSVC Debug: 9/9 passed, exit 0 (33.41 seconds)

valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_handler_cancellation_test
  11/11 tests passed, exit 0
  712914 allocs, 712914 frees; 0 bytes in 0 blocks at exit
  ERROR SUMMARY: 0 errors from 0 contexts, no suppressions
```

MSVC 通过 `Invoke-VsDevCommand.ps1`。WSL GCC/Clang 使用已经配置的 Node 22.13.1；
Clang 保留 ASan/UBSan、frame pointer 与 executable `-no-pie`。
本次无 sanitizer 诊断。`git diff --check` 对本子项文件通过。

## 接受决定

接受这十三个 handler 的显式状态迁移，与 Sub22 累计覆盖二十三个普通请求 handler。
dispatcher 仍有十九个旧式 pointer handler，另有单独编排的 initialize；完整接口迁移、
内部序列化分配失败和父级 Task 2/4/6 验收保持 pending。
