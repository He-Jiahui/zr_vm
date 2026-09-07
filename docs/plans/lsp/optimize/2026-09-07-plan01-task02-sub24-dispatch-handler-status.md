---
plan_id: optimize
task: plan01-task02-sub24
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_completion.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - zr_vm_language_server/stdio/stdio_editing.c
  - zr_vm_language_server/stdio/stdio_inline_completion.c
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_linked_editing.c
  - zr_vm_language_server/stdio/stdio_moniker.c
  - zr_vm_language_server/stdio/stdio_project.c
  - zr_vm_language_server/stdio/stdio_semantic_tokens.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
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
  - language_server_stdio_inline_value_semantic_smoke
  - language_server_stdio_diagnostic_fix_smoke
  - language_server_stdio_position_encoding_smoke
  - language_server_stdio_resolve_capabilities_smoke
  - language_server_stdio_save_capabilities_smoke
  - language_server_stdio_file_operation_capabilities_smoke
  - language_server_stdio_snapshot_workspace_diagnostics_smoke
  - language_server_stdio_document_sync_conformance
doc_type: plan-record
---

# Plan 01 Task 2 Sub24: Dispatch Handler Status

## 状态与产出记录

- 开始时间: 2026-09-07 14:41 +08:00
- 实际完成时间: 2026-09-07 15:03 +08:00
- 状态: 已完成普通请求接口迁移；父级完整验收未完成
- 源码版本: `2a3587f9` 加本子项和其他活动任务的共享源码 overlay
- 完成项目: 十九个剩余普通 handler、dispatcher、inventory probe 与状态回归
- 产出路径: 十个 handler 模块、dispatcher/header、测试、模块文档和本记录

## RED 与实现

真实 handler 矩阵扩展到全部四十三条普通请求路由。completion 的持续/首次 JSON
分配失败原来返回 INVALID_PARAMS，取消返回 OK；过期 code action 的首次分配失败
也返回 OK。增加 linked editing 的部分结果取消后，RED 为 13 项中 5 项失败。
Valgrind 同时报告 88 字节泄漏，64 字节直接、24 字节间接，分配栈经过
LspSemanticReferenceQuery_AppendRange、FindReferences 和 linked editing handler。

迁移 completion/resolve、semantic tokens full/delta/range、document/workspace
diagnostics、四种 formatting、codeAction/resolve、linkedEditing、moniker、
inlineValue、inlineCompletion、projectModules 和 willRenameFiles，共十九个
handler。累计四十二个 handler；willSaveWaitUntil 复用 formatting，故共四十三条
route。所有声明和 inventory stub 同步为 `SZrLspHandlerResult`，dispatcher 删除
NULL 推断 INVALID_PARAMS 的旧分支。initialize 仍单独编排。

合法空结果保持既有语义，根 JSON 分配失败为 INTERNAL_ERROR，取消清理后为
CANCELLED。completion 匹配后的序列化失败不再复制 unresolved 输入冒充成功；
stale action 构造失败不再返回空 object；workspace diagnostic report 构造失败
中止整个响应。linked editing 在 provider 取消后立即清理部分 locations，正常和
错误路径也都释放容器；completion 同样补齐 provider false 路径的输出清理。

## 测试与 Ownership

测试从真实 completion 获得 resolve 输入；code action 使用真实捕获的当前文档
snapshot 经生产 serializer 构造。所有 JSON fixture 在 teardown 释放，每个测试
销毁 context/global 后要求 runtime 活动分配归零。

五个矩阵验证持续分配失败、首次分配失败、已取消、无效参数和正常结果，各覆盖
四十三条 route。stale action 同时验证正常 disabled 结果和分配失败；workspace
diagnostic 在外层 object/array 成功后注入首个报告分配失败。六个校准取消用例在
provider 已产生部分结果时取消，另有正常 control。最终共 14 项。

## 验证命令及结果

```text
GCC: /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc
Clang ASan/UBSan: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
MSVC Debug: .codex/lsp-optimize-validation/msvc-current

cmake --build <build> --target zr_vm_language_server_stdio_handler_cancellation_test \
  zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_inventory_probe -j4
  GCC, Clang, MSVC: passed, exit 0

<build>/bin/zr_vm_language_server_stdio_handler_cancellation_test
  GCC, Clang ASan/UBSan, MSVC Debug: 14/14 passed, exit 0

ctest --test-dir <build> --output-on-failure \
  -R "^language_server_(provider_cancellation|stdio_(handler_cancellation|request_progress|protocol_conformance|server_lifecycle|optional_capabilities_smoke|protocol_inventory|navigation_capabilities_smoke|type_hierarchy_smoke|inline_value_semantic_smoke|diagnostic_fix_smoke|position_encoding_smoke|resolve_capabilities_smoke|save_capabilities_smoke|file_operation_capabilities_smoke|snapshot_workspace_diagnostics_smoke|document_sync_conformance))$"
  GCC: 16/17 passed, exit 1 (37.06 seconds)
  Clang ASan/UBSan: 16/17 passed, exit 1 (58.41 seconds)
  MSVC Debug: 16/17 passed, exit 1 (43.27 seconds)
  All three: diagnostic_fix_smoke failed; all other selected targets passed.

valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=definite,indirect --error-exitcode=99 \
  /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_handler_cancellation_test
  14/14 passed, exit 0
  919240 allocs, 919240 frees; 0 bytes in 0 blocks at exit
  ERROR SUMMARY: 0 errors from 0 contexts, no suppressions
```

MSVC 使用 VsDevCommand wrapper 的 `-Command` 参数数组；WSL 两套构建使用
Node 22.13.1。Clang 保留 ASan/UBSan、frame pointer 和 executable `-no-pie`。

## 扩展回归失败及接受边界

GCC/MSVC 的 diagnostic-fix smoke 在第 731 行缺少 possibly_uninitialized_read
publication，与 [09-05 冻结基线](2026-09-05-plan00-task01-sub02-gcc-baseline.md)
和 [crosswalk](2026-09-05-plan00-task01-sub01-execution-crosswalk.md) 相同。
Clang 在第 720 行因 server exit 1 提前失败；提取 child stderr 得到 parser 错误
恢复路径的 4,056 字节/20 次分配泄漏。

为划定责任，结构化解码同一 smoke 的输入 frame，仅保留 initialize、initialized、
didOpen、shutdown 和 exit，共 24 帧。移除全部普通请求后，相同 Clang binary
仍报告 4,056 字节/20 次分配泄漏；栈指向 parser_literals.c、
parser_expression_primary.c 和 parser_types.c。这证明该泄漏不需要本次迁移的
dispatcher 路径，后续按底层 parser ownership 修复。完整 stderr 位于本地
`.codex/lsp-optimize-validation/plan01-task02-sub24-parser-notifications.lsan.log`。

本子项接受普通请求接口迁移和已覆盖的根分配/取消清理。未将扩展 CTest 标记为
全绿，也未将 Task 2/4/6 或 Plan 01 标记完成。initialize、嵌套 JSON 字段/条目、
parser/provider runtime 分配分类、workspace-edit snapshot 错误细分和上述底层
恢复泄漏仍需独立验证。测试夹具之外的既有语义失败保持原断言。
