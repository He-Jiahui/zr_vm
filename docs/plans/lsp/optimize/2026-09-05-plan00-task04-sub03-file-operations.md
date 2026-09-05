---
related_code:
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/stdio_request_dispatch.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
implementation_files:
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - tests/language_server/stdio_file_operation_capabilities_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
  - docs/plans/syntax/README.md
tests:
  - tests/language_server/stdio_file_operation_capabilities_smoke.js
  - tests/language_server/stdio_smoke.js
doc_type: milestone-record
---

# File Operation Capability Withdrawal

## 状态与产出记录

| 开始时间 | 实际完成时间 | 状态 | 完成项目 | 验证命令及结果 |
| --- | --- | --- | --- | --- |
| 2026-09-05 17:24 +08:00 | 2026-09-05 17:56 +08:00 | 已完成 | 删除始终返回 `null` 的 `willCreateFiles`/`willDeleteFiles` 注册、路由、处理器、常量和断言；保留并验证 `didCreate`/`didDelete`/`didRename` 和有真实版本化编辑的 `willRenameFiles`；新增创建、删除、重命名、快照和陈旧磁盘拒绝专项。 | GCC 11.4 Debug shared focused CTest 8/8；Clang 14 Debug shared focused CTest 8/8；MSVC 19.44 Debug static focused CTest 8/8；独立规格与质量审查通过，文档精度问题已修正。 |

## 变更与语义边界

`workspace/fileOperations` 现在只包含四个真实注册：`didCreate`、
`didDelete`、`didRename` 和 `willRename`。两个空实现请求未被改成“成功但无编辑”
的结果，而是从能力、常量、路由和内部声明全部撤销；客户端请求得到精确的
`-32601 Method not found`。这是 Plan 00 Task 4 明确要求的能力收敛；不修改 ZR
语法或 compiler 语义规则，本子项未发现 syntax 冲突。

新增 fixture 在临时多文件项目中验证：未打开项目通过 `didCreateFiles` 进入索引；
`willRenameFiles` 的导入和模块声明编辑分别保留当前打开版本 7/8 与磁盘版本
`null`；磁盘文件静默改变后编辑计划整体拒绝；实际 `didRenameFiles` 更新 URI，
workspace symbol 和 definition 均指向新文件；`didDeleteFiles` 移除最终项目索引。
同 URI 重命名和两个已撤销请求也有精确负向断言。

模块契约见
[`lsp-workspace-file-operation-contract.md`](../../../cli-and-tooling/lsp-workspace-file-operation-contract.md)。
URI、范围和快照的借用/复制边界只覆盖同步 JSON 序列化期间；WASM 文件操作能力
仍由 Plan 05 单独验收，未由本子项提前宣称。

## 源码版本、产出和剩余门槛

验证源码是 `55e6ba07`（生产代码同 `670e3cd0`）加本子项七个代码/测试路径，
沿用 resolve 记录列出的独立 source export、精确 gitlink 和三工具链构建目录。
未导入其他会话的 semantic/runtime/FFI/benchmark overlay。提交后可通过
`git log -1 --format=%H -- <本记录路径>` 定位子项源码版本。
产出为本记录、模块文档、focused smoke、CTest 注册、主 smoke 断言以及四个
native 删除/路由文件的最小修改。Plan 00 Task 4 仍待最终审计，Plan 01
协议取消/生命周期和 Plan 02 快照统一尚未晋级；完整跨 provider、WASM 与浏览器
桌面 Web 矩阵仍保持未验收。

## RED 与验证命令

修复前的 frozen `670e3cd0` 先通过所有保留操作检查，再显示两个请求都返回
`result: null`，且 initialize 多出两个无实现注册；focused fixture 精确断言失败。
修复后的 GCC/Clang/MSVC 均返回两个完整 MethodNotFound envelope。

```text
cmake --build <build-dir> --target zr_vm_language_server_stdio zr_vm_language_server_lsp_capability_registry_test zr_vm_language_server_stdio_server_lifecycle_test --parallel 6
ctest --test-dir <build-dir> --output-on-failure -R ^language_server_(lsp_capability_registry|stdio_(protocol_inventory|resolve_capabilities_smoke|navigation_capabilities_smoke|file_operation_capabilities_smoke|document_sync_conformance|server_lifecycle|workspace_folders_smoke))$
node --check tests/language_server/stdio_file_operation_capabilities_smoke.js
node --check tests/language_server/stdio_smoke.js
```

三工具链构建 exit 0；GCC 8/8（4.43 s）、Clang 8/8（4.74 s）、MSVC 8/8
（26.10 s）。Node 12 语法检查通过，scoped `git diff --check` exit 0。
该八项集合不包含 `stdio_protocol_conformance`：旧 cancel-known 的准备超时
已单独记入基线并待 Plan 00 Task 3 修复，不把本集合报告成协议整体验收。

规格审查独立复跑 GCC fixture exit 0；质量审查独立复跑 MSVC fixture exit 0。
质量审查的负向对照仅在内存中忽略 `didRenameFiles`，观察到旧 `legacy.zr` 和新
`modern.zr` 同时残留，准确触发目标集合断言，证明后续 `didChange` 不能掩盖
重命名通知失效。模块文档已明确 `version: null` 不能防止响应后的磁盘变更。
