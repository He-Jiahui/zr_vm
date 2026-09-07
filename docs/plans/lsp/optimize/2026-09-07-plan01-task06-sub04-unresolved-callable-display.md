---
plan_id: optimize
task: plan01-task06-sub04
status: completed
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_display_unresolved_cases.h
  - tests/language_server/test_lsp_unresolved_callable_display_cases.h
  - tests/CMakeLists.txt
related_module_docs:
  - docs/parser-and-semantics/callable-display-resolution.md
  - docs/cli-and-tooling/lsp-completion-capability-boundary.md
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub04: Unresolved Callable Display

## 状态与产出记录

- 开始时间: 2026-09-07 21:39 +08:00
- 实际完成时间: 2026-09-07 22:11 +08:00
- 状态: 本子项完成；完整 smoke 父门禁未完成
- 源码版本: `37b3746e` 加共享工作树 overlay
- 产出路径: parser signature producer、source scope 接入、独立回归和模块文档
- 剩余门槛: Plan 01 完整 smoke 的 workspace diagnostic 内部错误与 Clang cancellation setup；Plan 03 consumer/provider 矩阵

## 失败与责任层

Sub03 后三个工具链的 stdio smoke 都停在第 2076 行。源代码声明
`fn redact(value: MissingType): MissingType`，预期参数和返回值显示
`cannot infer exact type`，实际 completion 为保留 `MissingType` 的完整签名。

逐层检查候选包括 AST 类型注解、canonical type adapter、TYPE reference 的
isResolved、source scope signature producer、SymbolAt/VisibleSymbols 和
completion/hover 投影。`CanonicalType_FromName` 可以保留尚未绑定的 nominal
名称，而 TYPE reference 已携带 false resolution。最低失败层是 parser 的
CreateCallableSignature 忽略这一显式未解析事实，直接格式化 canonical TypeId。

新增 parser RED 分别验证参数、返回值和两者的 false resolution，三项均失败；
resolved 对照和既有 22 项均通过。LSP RED 的 completion/hover 均失败，原有
六项 type-use 回归通过。日志前缀：
`.codex/lsp-optimize-validation/plan01-task06-sub04-`。

## 实现与边界

签名 producer 按 AST 类型使用节点身份读取已发布 TYPE reference，显式 unresolved
或不一致 TypeId 会形成 unavailable 文本。它不按类型名称搜索或重新推断类型。
source scope 分析发布签名到同一 SymbolId 和同一 TypeId 的 resolved references；
不同 symbol、不同实例 TypeId 和 unresolved symbol reference 不接收此签名。
completion 和 hover 继续只读投影。

没有 TYPE reference 的 canonical-only callable contract 保持已有格式化行为。
本项使显式负向解析事实生效，不宣称全部注解 producer 已覆盖，也不改变 canonical
type graph 的 missing/error 表达。完整 coverage 仍属于 Plan 03。

MSVC call-query 既有测试直接调用未导出的内部 helper，扩展验证首次暴露链接失败。
测试目标直接编译 call semantic facts 及 argument semantic facts 两个内部实现，
并加入私有 include path；未扩大生产 DLL export。

## 本地参考依据

Roslyn `lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/FunctionPointerTests.cs`
在错误函数指针参数用例中仍检查 callable symbol 的存在，并断言该参数
`IsErrorType()`。类型的可显示名称不等于成功绑定的类型。ZR 沿用已发布 TYPE
reference 的解析状态，并使用原 stdio 契约规定的 unavailable 文本；不修改
类型名称、语法、ABI 或诊断身份。

## 验证命令及结果

使用 Sub03 的专用 GCC ext4、Clang ASan/UBSan ext4 和 MSVC Debug 构建目录。
共享 core 头文件变化触发约 840 个编译单元重建；全部结束后收集单元证据。
构建目标：

```text
cmake --build <build> --parallel 4 --target zr_vm_semantic_display_test
  zr_vm_semantic_query_symbols_test zr_vm_semantic_query_calls_test
  zr_vm_canonical_type_graph_test zr_vm_semantic_query_contract_test
  zr_vm_language_server_type_use_test zr_vm_language_server_stdio zr_vm_cli_executable
```

| 直接运行目标 | GCC | Clang ASan/UBSan | MSVC Debug |
| --- | --- | --- | --- |
| semantic_display | 27/27 | 27/27 | 27/27 |
| semantic_query_symbols | 36/36 | 36/36 | 36/36 |
| semantic_query_calls | 32/32 | 32/32 | 32/32 |
| canonical_type_graph | 19/19 | 19/19 | 19/19 |
| semantic_query_contract | 6/6 | 6/6 | 6/6 |
| language_server_type_use | 9/9 | 9/9 | 9/9 |

每工具链 129/129，所有进程真实 exit 0。Clang 保留 LeakSanitizer，单元日志没有
ASan/UBSan/LSan 报告。日志为 `<toolchain>-final-<target>.log`；WSL 单元执行脚本
为同目录的 `plan01-task06-sub04-run-units.sh`。

```text
ctest --test-dir <build> --output-on-failure --output-log <toolchain>-stdio.log
  -R "^language_server_stdio_(smoke|diagnostic_fix_smoke|position_encoding_smoke|resolve_capabilities_smoke|navigation_capabilities_smoke)$"
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all
  --error-exitcode=99 <gcc-build>/bin/zr_vm_semantic_display_test
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all
  --error-exitcode=99 <gcc-build>/bin/zr_vm_language_server_type_use_test
```

三工具链 CTest 均为 4/5，四个独立 smoke 通过。完整 smoke 通过了原 MissingType
completion/hover、后续 native callable 和性能采样，推进至第 3168 行
`workspace/diagnostic` 返回 `-32603 Internal error`。该后续故障需要单独定位，
不能把本组 CTest 记为全部通过。Clang protocol 的 cancellation setup 超时沿用
Sub03 记录，本项未修改取消路径或测试期限。

最后一次 GCC 重建后再次收集的 Valgrind：display 14,664 次分配/释放，LSP
707,869 次分配/释放；两者退出时均 0 bytes/0 blocks、0 errors、exit 0。最终日志为
`valgrind-display-final.log` 和 `valgrind-lsp-final.log`。早于最后一次重建的日志
保留为过程证据，不用于最终结论。
