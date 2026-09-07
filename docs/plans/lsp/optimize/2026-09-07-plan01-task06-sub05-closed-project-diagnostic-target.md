---
plan_id: optimize
task: plan01-task06-sub05
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_documents.c
  - tests/language_server/test_stdio_document_close.c
  - tests/language_server/stdio_snapshot_workspace_diagnostics_smoke.js
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
related_module_docs:
  - docs/cli-and-tooling/lsp-stdio-validation.md
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub05: Closed Project Diagnostic Target

## 状态与产出记录

- 开始时间: 2026-09-07 22:15 +08:00
- 实际完成时间: 2026-09-07 22:31 +08:00
- 状态: 本子项完成；完整 smoke 父门禁未完成
- 源码版本: `fdc981c1` 加共享工作树 overlay
- 产出路径: stdio 文档生命周期回归、协议回归与模块文档
- 剩余门槛: 完整 smoke 二进制导入成员跳转断言与既有 Clang cancellation setup 超时

## 失败与责任层

Sub04 后三个完整 stdio smoke 均在第 3168 行 `workspace/diagnostic` 返回
`-32603 Internal error`。GDB 在 `serialize_workspace_diagnostic_report_for_uri`
的失败分支捕获 `file:///tmp/zr-stdio-generic-plugin-*/src/main.zr`，其
fileVersion 为 NULL。URI collection 没有失败。

该项目文件在第 2335 行 didClose，此时不属于任何已登记工作区。关闭流程先将
isOpenDocument 清除，按现有 workspace 契约释放 parser/analyzer；项目 source
record 仍存在。EnsureScannedSourceGraph 看到 hasSemanticProjectLoad 为 true
直接成功，后续 URI 枚举含已释放文件，GetDiagnostics 拒绝缺失 fileVersion。

责任层是 stdio 文档关闭流程漏掉项目索引清理。不得把此错误改为序列化空报告，
也不得为了消除错误而重新读取已移除工作区的磁盘文件。现有
stdio_workspace_folders_smoke 明确要求关闭保留 overlay 后释放根目录外状态。

新增协议 RED 在关闭显式打开的工作区外项目后首次请求 workspace diagnostics
复现 -32603。新增 C 用例覆盖外部项目关闭/重复关闭/重开、保留其他已打开文档、
工作区内回到磁盘快照、关闭已删除磁盘文件。

调试与验证日志前缀：`.codex/lsp-optimize-validation/plan01-task06-sub05-`。

## 实现与所有权

关闭分支在发布空诊断后，循环调用既有 RemoveFileRecordByUri，释放所有索引中
匹配的记录及其 parser/analyzer 缓存，再执行原有无项目文档清理。循环适配同一
URI 可能属于多个索引的情况。成功恢复磁盘内容的工作区分支保持原有行为；其他
打开文档仍保留其对象、内容和版本。重复关闭可安全完成，重开可重新登记该 URI。

URI 与诊断查询仍来自 context 所拥有的记录与快照。未增加磁盘读取权限、语义
重推断或诊断序列化兜底，也未扩大生产 API 导出。

## 验证命令及结果

C RED 四项中三项在相同断言失败：已释放文件仍存在于诊断 URI 集合；磁盘恢复
对照通过。MSVC 协议 RED 在新增 close 后请求精确复现 -32603。

```text
cmake --build <build> --parallel 6 --target
  zr_vm_language_server_stdio_document_close_test zr_vm_language_server_stdio
  zr_vm_language_server_stdio_server_lifecycle_test zr_vm_language_server_lsp_semantic_snapshot_test
ctest --test-dir <build> --output-on-failure
  -R "^(language_server_lsp_semantic_snapshot|language_server_stdio_(document_close|server_lifecycle))$"
ctest --test-dir <build> --output-on-failure
  -R "^language_server_stdio_(smoke|snapshot_workspace_diagnostics_smoke|workspace_folders_smoke|document_sync_conformance)$"
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all
  --error-exitcode=99 <gcc-build>/bin/zr_vm_language_server_stdio_document_close_test
```

使用 Sub03 的专用 GCC ext4、Clang ASan/UBSan ext4、MSVC Debug 构建目录。

| 验证 | GCC | Clang ASan/UBSan | MSVC Debug |
| --- | --- | --- | --- |
| C 关闭回归 | 4/4 | 4/4 | 4/4 |
| 关闭/快照/lifecycle CTest | 3/3, exit 0 | 3/3, exit 0 | 3/3, exit 0 |
| 同步/工作区根/工作区诊断协议 | 3/3 | 3/3 | 3/3 |
| 完整 stdio smoke | 第 3373 行失败 | 第 3373 行失败 | 第 3373 行失败 |

三个完整 smoke 均通过第 3168 行工作区诊断及随后的取消、unchanged resultId、
generation/stale churn 检查。当前失败是 binary imported member definition
未返回测试要求的 binary metadata declaration，需要后续按事实层定位。含完整
smoke 的 CTest 命令真实 exit 1，不能将父门禁记为通过。Clang 日志没有 sanitizer
报告。本项未重跑已登记的 cancellation setup 超时。

Valgrind 四项全通过，全部堆块已释放，ERROR SUMMARY 为 0。原始证据为
`valgrind.log`；三工具链 C 与协议证据分别为 `<toolchain>-units.log` 和
`<toolchain>-stdio.log`。
