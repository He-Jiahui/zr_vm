---
plan_id: optimize
task: plan01-task06-sub06
status: completed
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_cast.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_cast.h
  - tests/parser/test_cast_operand_facts.c
  - tests/language_server/test_lsp_cast_operand_facts.c
  - tests/cmake/zr_vm_cast_operand_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub06: Cast Operand Semantic Facts

## 状态与产出记录

- 开始时间: 2026-09-07 22:33 +08:00
- 实际完成时间: 2026-09-07 23:30 +08:00
- 状态: cast 操作数事实修复完成；父级完整门禁未完成
- 源码版本: `81e233a8` 加共享工作树 overlay；RED 为 `6a69e4f7` overlay
- 产出路径: parser cast 推断 helper、parser/LSP 回归与模块文档
- 剩余门槛: Linux CLI binary member 事实、导入缓存泄漏及父级完整 smoke

## 失败与责任层

Sub05 后三工具链完整 stdio smoke 均在第 3373 行失败。import literal 能跳到
binary entry，而 `var right = <int> binaryStage.binarySeed();` 的成员 definition
返回空数组。该变量由 binary lambda export 提供，但失效原因不是 lambda 名称。

GDB 在查询清理前捕获 kind NONE、hasCanonicalSymbol false，旧 metadata adapter
仍可拿到成员及其声明。成员调用事实发布函数没有为 binarySeed 执行。
ExpressionType_Infer 的 TYPE_CAST_EXPRESSION 分支只转换目标类型，没有分析
expression 操作数。因此 cast 本身得到 int，内部调用缺少 canonical 事实。

最低修复层为 parser expression inference：分析操作数并释放临时 inferred type，
随后仍以显式目标类型作为 cast 结果。LSP 继续读取已经发布的事实。

本地参考：Roslyn `lua/roslyn/src/Compilers/CSharp/Portable/Binder/Binder_Expressions.cs`
的 BindCast 先 BindValue(node.Expression)，再 BindType(node.Type)，最后绑定转换。
ZR 本项只闭合已有分析遍历，不新增转换合法性规则。syntax 07 对 `<Type>expr`
仍标记 surfacePending，本项不改变其拼写或验收状态。

日志前缀：`.codex/lsp-optimize-validation/plan01-task06-sub06-`。

## RED 与实现

GCC 与 MSVC 修复前 CTest 为 0/2，parser 的 4 个用例中 3 个因缺少操作数
expression fact 失败，未知调用对照通过；binary/native 两个 LSP 用例均在
cast 位置缺少 canonical symbol。未 cast 的对照已正常解析。

新 private cast helper 分析非空操作数、释放临时 inferred type，再转换显式
target annotation；外层既有 numeric conversion fact 保持。操作数推断失败
不会清除其诊断或补造目标，也不会丢弃显式 cast 目标类型。

[模块契约](../../../parser-and-semantics/cast-operand-semantic-facts.md) 记录
事实精度、临时类型和 snapshot 所有权。本项测试单独注册在小型 CMake fragment，
不向已有大型测试源文件继续堆叠用例。

MSVC 第一轮 GREEN：新增 6/6、既有类型推断 124/124、call query 32/32、
canonical type graph 19/19。完整 stdio smoke 已通过 binary definition、
references、highlights、hover 与 watched-file refresh，推进至第 3817 行
`workspace/didRenameFiles must refresh semantic facts on the added ModuleIdentity edge`。
完整 smoke 仍返回失败，父级门禁保持未完成。

## 最终验证与边界

- GCC 11.4、Clang 14 ASan/UBSan、MSVC Debug 的 parser cast 4/4、LSP cast
  2/2、call query 32/32、canonical type graph 19/19 均通过，真实 exit 0。
  LSP 断言比较同一 analyzer 的 SymbolId、TypeId、provider generation、owner、
  target kind、metadata/signature tokens/hash 和准确 definition URI/range。
- 三工具链 document-sync 与 file-operation capabilities 两项协议 CTest 均通过。
- Valgrind parser 为 2,327 次分配/释放，LSP 为 539,092 次分配/释放；两项均
  0 bytes at exit、0 errors，真实 exit 0。
- 扩展 type-inference：GCC/MSVC 124/124、exit 0；Clang 暴露测试字面量越界，
  由 [Sub08](2026-09-07-plan01-task06-sub08-type-test-string-boundary.md) 修复后
  124 个功能用例完成，但保留 5,187 bytes/44 allocations 导入缓存泄漏、exit 1。
- 使用 `git show 6a69e4f7:tests/language_server/stdio_smoke.js` 原内容重放，
  MSVC 已通过 binary 跳转/引用/高亮/hover/refresh，停在 canonical double 的
  旧 float 断言。GCC/Clang 则停于 binary fixture 的 member_not_found。
  最小 CLI fixture 中移除 cast 后也出现同一错误和空 definition；直接 writer
  binary 与 native 的 cast 回归均通过，因此 CLI artifact 身份链单独继续诊断。
  未修改完整 smoke 的断言以接受该错误。

GCC 构建目录为 `/home/hejiahui/.codex-builds/lsp-plan01-task06-sub03-gcc`，
Clang 为 `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`，MSVC 为
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`。构建目标包含
`zr_vm_cast_operand_facts_test`、`zr_vm_language_server_cast_operand_facts_test`、
`zr_vm_type_inference_test`、`zr_vm_semantic_query_calls_test`、
`zr_vm_canonical_type_graph_test`、`zr_vm_language_server_stdio` 和
`zr_vm_cli_executable`。执行 `ctest --test-dir <build> --output-on-failure -R
'^(cast_operand_facts|language_server_cast_operand_facts)$'` 及三个既有单元程序。
Valgrind 参数为 `--leak-check=full --show-leak-kinds=all
--errors-for-leak-kinds=definite,indirect --error-exitcode=99`。Clang 保留
ASan/UBSan/LSan 和 `-no-pie`，未关闭检测项。

此次只提交 cast producer、独立测试/CMake 注册及文档。Sub07 的 rename 验收
调整和共享 provider/runtime 改动不属于本提交。
