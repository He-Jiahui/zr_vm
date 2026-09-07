---
plan_id: optimize
task: plan01-task04-sub06
status: completed
related_code:
  - tests/language_server/test_lsp_provider_cancellation.c
  - tests/cmake/zr_vm_lsp_cancellation_tests.cmake
  - tests/CMakeLists.txt
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_type_hierarchy.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_provider_cancellation
doc_type: plan-record
---

# Plan 01 Task 4 Sub06: Provider Loop Cancellation

## 状态与产出记录

- 开始时间: 2026-09-07 09:50 +08:00
- 实际完成时间: 2026-09-07 10:04 +08:00
- 状态: 已完成
- 源码版本: `9d0c37e2` 加本子项测试 overlay
- 完成项目: 七条 provider 查询的循环内取消、清除回调后恢复与部分结果释放回归
- 产出路径: 独立 Unity 测试、CMake 注册、模块文档与本记录

## 验证设计与结果

既有 interface 测试主要验证查询开始前已取消的请求。本子项新增同步回归，先以
同一 fixture 取得多条结果，再安装观察结果数组的 cancellation callback。第一条
结果产生后，callback 返回 true；查询必须返回 false、保留恰好一条部分结果，且
确认 callback 实际观察到了该结果。清除 callback 后再次查询，结果数必须恢复。

| Provider | Fixture 与已验证边界 |
| --- | --- |
| workspace symbols | 单个打开文档中的多个类和函数，投影首项后停止 |
| document symbols | 同一文档中的多个声明，投影首项后停止 |
| references | 一个函数声明和两个调用，首个 location 后停止 |
| rename | 合法新名称，首个 location 后停止 |
| incoming calls | 两个不同 caller，首个 call item 后停止 |
| outgoing calls | 一个 caller 调用两个不同函数，首个 call item 后停止 |
| subtypes | 一个基类的两个直接子类，首个 hierarchy item 后停止 |

所有查询使用生产 `ZrLanguageServer_LspContext_SetRequestCancellationCheck` API。
测试不依赖计时或线程调度，也不向生产代码添加钩子。七项均在现有实现上通过，
因此本项是验收资产补充，没有缺陷 RED 或生产修复。

## Lifetime、Exactness 与 Ownership

callback 的 userData 借用测试结果数组地址，其有效期覆盖单次同步查询。清除时
同时断言 context 的 userData 归零。失败返回仍可能持有部分结果，调用方负责释放：
普通 symbol/location 逐项释放，hierarchy 使用对应 FreeHierarchyCalls 或
FreeHierarchyItems 释放嵌套范围与条目。teardown 先清除 callback，再释放结果、
prepared hierarchy、context 和 runtime。Clang sanitizer 验证包含这些取消清理路径。

## 验证命令及结果

```text
WSL GCC Debug
cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc \
  --target zr_vm_language_server_provider_cancellation_test -j2
  passed
<build>/bin/zr_vm_language_server_provider_cancellation_test
  7/7 passed, exit 0
ctest --test-dir <build> --output-on-failure -R "^language_server_provider_cancellation$"
  1/1 passed, exit 0

Clang 14 Debug ASan/UBSan
build: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
cmake --build <build> --target zr_vm_language_server_provider_cancellation_test -j4
  passed
ctest --test-dir <build> --output-on-failure --verbose -R "^language_server_provider_cancellation$"
  7/7 Unity cases, 1/1 CTest, exit 0, no sanitizer diagnostic

MSVC Debug
build: .codex/lsp-optimize-validation/msvc-current
Invoke-VsDevCommand.ps1 cmake --build <build> \
  --target zr_vm_language_server_provider_cancellation_test --parallel 4
  passed
Invoke-VsDevCommand.ps1 ctest --test-dir <build> --output-on-failure --verbose \
  -R "^language_server_provider_cancellation$"
  7/7 Unity cases, 1/1 CTest, exit 0
```

Clang 复用 Sub05 的 ext4 构建目录，保留 address/undefined sanitizer、frame pointer
和 executable `-no-pie`。三个构建都使用含其他活动任务 overlay 的共享源码工作树；
本结果不是冻结提交的全仓验收。MSVC 首次调用使用 `-V` 与 PowerShell 包装器参数
冲突，未启动测试；改用 `--verbose` 后得到上面的有效结果。

## 接受边界与剩余门槛

接受 Plan 01 Task 4 Sub06，provider 外层结果投影的中途取消与恢复已有确定性证据。
parser 内部扫描不由这些测试覆盖，workspace diagnostics 遍历、dependency/content
fence 和 50 ms 取消延迟继续 pending。Task 4 与 Task 6 的父门禁保持未完成。
