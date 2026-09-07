---
plan_id: optimize
task: plan01-task06-sub03
status: completed
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_type_use.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_use.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - tests/parser/test_semantic_query_type_use_cases.h
  - tests/language_server/test_lsp_type_use.c
  - tests/cmake/zr_vm_lsp_type_use_tests.cmake
  - tests/CMakeLists.txt
related_module_docs:
  - docs/parser-and-semantics/semantic-type-use-publication.md
  - docs/cli-and-tooling/lsp-completion-capability-boundary.md
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub03: Generic Type-Use Identity

## 状态与产出记录

- 开始时间: 2026-09-07 20:35 +08:00
- 实际完成时间: 2026-09-07 21:35 +08:00
- 状态: 本子项已完成；完整 smoke 与 Clang protocol 门禁仍未验收
- 源码版本: `0f64fbb7` 加共享工作树 overlay
- 产出路径: parser 共享 TYPE producer、LSP adapter 与 completion 投影、独立测试、模块文档和本记录
- 剩余门槛: Plan 01 完整 smoke、Clang cancellation setup 超时；Plan 03 各 consumer/provider 矩阵

## 失败与责任层

原始 stdio smoke 第 1973 行要求 `Derived` completion detail 包含
`Resolved Type: Derived<Item, 4>`，实际为 `Derived`。同一问题同时出现在
existing interface 的 closed-generic hover 用例。候选底层包括 parser range、
泛型 TypeId、声明 SymbolId、LSP query 和 JSON 投影。

新增 compiler/query 回归首先失败于 `CanonicalTypeAt`：普通泛型注解没有
发布 TYPE reference，原有 30 项通过。LSP 分析另有一个 producer，只发布
TypeId 和 resolved flag，遗漏 SymbolId/name/declaration range。独立 LSP
四项 RED 均失败，覆盖 SymbolAt、hover、completion 和 document replacement。

共享 parser producer 现在保留 use-site TypeId，沿 canonical definitionTypeId
关联唯一 source declaration；准确 name range 来自现有 declaration fact 或
class nameLocation。LSP 分析仅调用该 publisher。completion 通过 SymbolId
把使用处实例类型投影到对应候选。没有查询时 AST 重推断或按显示名称匹配身份。

边界测试还暴露了 wholeRange/AST source 不一致的输入应被拒绝；补齐严格 source
校验。初次 MSVC 构建发现两个 internal helper 未导出，测试目标直接编译
semantic_scope_facts.c 与 lsp_canonical_completion.c，保持生产 export 不变。

扩大底层回归时，canonical graph 的既有 invalid generic contract 用例发现
失败转换提前发布了内层参数引用，reference count 预期 1、实际 3。转换实现
改为不发布引用的私有递归 helper；完整泛型转换成功后才从 canonical argument
列表发布根节点和嵌套类型使用。该原子性回归恢复通过，嵌套 `Box<Box<int>>`
的外层与内层 hover 也独立通过。RED 断言提前退出产生的 LSan 报告不计为 GREEN。

共享工作树新增的 `session_checkpoint.c` 缺少 `checkpoint_capture_value` 前置
声明，使三个工具链重建均失败。本会话只补上声明以恢复构建；该支持性修改随
原所属文件保持未暂存，不纳入本 LSP 提交。所有结果均明确基于上述工作树 overlay。

## 本地参考依据

- Roslyn `lua/roslyn/src/Compilers/CSharp/Portable/Compilation/CSharpSemanticModel.cs`
  的 GetSymbolInfo 进入 semantic worker；
  `Test/Symbol/Compilation/GetSemanticInfoTests.cs:UnboundGenericTypeArity` 通过
  semantic model 查询 generic name，`SemanticModelGetSemanticInfoTests.cs:UnboundTypeInvariants`
  区分构造类型与 ConstructedFrom/OriginalDefinition。
- Rust `lua/rust/src/tools/rust-analyzer/crates/ide/src/goto_definition.rs` 的
  IdentClass definitions 导航和 `goto_def_for_type_alias_generic_parameter` 测试。
- Javac `lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/api/JavacTrees.java`
  的 getElement/getTypeMirror 分别投影已归属 symbol 和 type。

共同契约是声明身份与实例类型分别保留。ZR 通过绑定单个 snapshot 的 SymbolId/TypeId
表达，不新增语言语法、运行时实例化规则或 metadata ABI。

## 验证命令及结果

```text
GCC: /home/hejiahui/.codex-builds/lsp-plan01-task06-sub03-gcc
Clang ASan/UBSan: /home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang
MSVC Debug: .codex/lsp-optimize-validation/msvc-current
```

三个构建均完成以下目标：

```text
cmake --build <build> --parallel 6 --target
  zr_vm_semantic_query_symbols_test zr_vm_semantic_display_test
  zr_vm_canonical_type_graph_test zr_vm_semantic_query_contract_test
  zr_vm_language_server_type_use_test zr_vm_language_server_stdio zr_vm_cli_executable
cmake --build <build> --target zr_vm_descriptor_plugin_fixture_int
```

直接运行五个单元目标，GCC、Clang ASan/UBSan、MSVC Debug 均为真实 exit 0：

| 目标 | 每个工具链结果 |
| --- | --- |
| semantic_query_symbols | 36/36 |
| semantic_display | 22/22 |
| canonical_type_graph | 19/19 |
| semantic_query_contract | 6/6 |
| language_server_type_use | 6/6 |

合计每工具链 89/89。Clang 最终单元运行保留 LeakSanitizer；没有 sanitizer 报告。
日志前缀为 `.codex/lsp-optimize-validation/plan01-task06-sub03-`，单元日志使用
`{gcc,clang,msvc}-final-<target>.log`。

```text
ctest --test-dir <build> --output-on-failure --output-log <toolchain>-protocol.log
  -R "^language_server_(type_use|stdio_(diagnostic_fix_smoke|protocol_conformance|navigation_capabilities_smoke|position_encoding_smoke|resolve_capabilities_smoke))$"
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all
  --error-exitcode=99 <gcc-build>/bin/zr_vm_semantic_query_symbols_test
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all
  --error-exitcode=99 <gcc-build>/bin/zr_vm_language_server_type_use_test
```

GCC/MSVC CTest 为 6/6。Clang 为 5/6：protocol 的 `cancel known request id`
在等待 2048-class document 的 publishDiagnostics 时超过原有 10 秒 setup 期限，
stderr 为空。单独复跑 protocol 以及单独调用该 case 都在同一位置失败，不能按
偶发并行负载忽略。GDB 对同一 2048-class 输入设 publisher 断点，正常发布空诊断
并退出，断点命中 0 次；该类型引用路径不能解释超时。GDB 探针为 ptrace 关闭了
LSan，只用于调用证据，不计 sanitizer GREEN。原 protocol 期限和断言保持不变。
相关日志为 `clang-protocol-solo.log`、`clang-cancel-isolated.log` 和
`clang-cancel-publisher-probe.log`；探针位于同日志目录的
`plan01-task06-sub03-protocol-probe.js`。

Valgrind query 为 1,880,585 次分配/释放，type-use 为 481,907 次分配/释放；
两者退出时均 0 字节/0 blocks、0 errors，exit 0。

最终三工具链原始 smoke 均已越过 generic completion、resolve、definition 和
signature 断言，停于第 2076 行 missing declaration type 的 completion detail
断言。GCC/MSVC 初次因未构建插件 fixture 提前退出，补建后得到相同语义失败；
日志为 `gcc-final-smoke-with-fixture.log`、`msvc-final-smoke-with-fixture.log` 和
`clang-final-smoke.log`。diagnostic-fix smoke 三工具链均通过。完整门禁保持未验收。
