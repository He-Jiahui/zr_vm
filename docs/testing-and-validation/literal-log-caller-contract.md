---
related_code:
  - tests/parser/test_prototype.c
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_literal_surface_main.c
  - tests/CMakeLists.txt
  - tests/unity_lifecycle_noop.c
  - tests/harness/zr_test_log_macros.h
  - tests/harness/runtime_support.c
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - tests/test_support.h
  - tests/harness/runtime_support.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
implementation_files:
  - tests/parser/test_prototype.c
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_literal_surface_main.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_literal_surface_main.c
  - tests/fixtures/parser/test_prototype_struct.zr
  - tests/fixtures/parser/test_prototype_class.zr
doc_type: test-contract
---

# 字面量测试的调用与日志契约

## 入口和使用方式

`zr_vm_literal_surface_test` 的 CMake 目标包含 prototype、const、char/type-cast 三组用例、独立 main 和空生命周期源，并链接 parser/core。main 经 `RUN_TEST` 派发，返回 `UNITY_END()` 的失败计数；构建注册和 suite 日志不证明各测试的名义场景已验收。

本次契约覆盖八个 prototype、二十二个 char/type-cast 函数、三个 reference helper 和一个 main。const 源和未选定的声明、类型、宏、辅助函数仍需另外审查，不能由同目标注册推导已完成。

## 日志与失败控制流

公共 `ZR_TEST_PASS` 输出调用方已取得的时间差，不清除已有 Unity 失败状态，也不产生成功断言。公共 `ZR_TEST_FAIL` 设置当前 Unity 失败标志但保持控制流，调用方仍负责释放资源和返回。

Unity 硬断言可经 `TEST_ABORT`/`longjmp` 跳回测试保护入口。Unity 另行保护 `tearDown`，本目标的 `setUp`/`tearDown` 均为空；它们没有取得局部 State、Global、文本或 AST 的所有权。到达日志不证明未到达该位置的失败路径已清理。

计时区间由调用方决定：prototype 正常路径包含 Global 释放，多数字符样本先打印再销毁 State，文档/矩阵样本在文本释放或 helper 返回后打印；这些数值不能直接作为等同工作量的基准。

## Prototype 样本的判据

- struct 编译样本打印元数据；数量不符只打印诊断，末尾仍可 PASS，保留具体 TODO。
- class 编译样本可仅凭 Animal 名称存在进入通过分支；未充分证明实际原型重建和继承关系，保留 TODO。
- 原型工厂样本检查名称和种类，但对象为 NULL 只省略诊断；对象构造的验收意图仍需核实。
- 直接设置父原型和发布模块导出的样本检查各自对象身份，不能扩大为源码编译或序列化关系已经正确。
- 字段偏移样本仅输出非空字段表，没有数值布局断言。装载继承样本的 fixture 未声明期望父关系，关系不符也可能打印 PASS；分别保留 TODO。
- 嵌套 struct 复制样本用硬断言检查源与副本的对象/字段隔离，断言失败会跳过正常 Global 释放，已标 BUG。

## Char、cast 和 reference 的判据

解析样本核对根、语句容器或错误恢复节点；Function 非空的编译样本不执行转换值或证明具体指令。关系表达式样本核对 AST、parser 错误状态和诊断回调，防止尝试 cast 的诊断泄漏到 `<` 表达式。

manifest、矩阵和文档样本按文本标记、case kind 和最低数量检查库存。子串命中不证明 JSON 结构、唯一性或所有 fixture 的执行；取得文本后的硬断言也可能跳过 `free`。

`execute_reference_test_fixture_expect_int` 编译后调用 `ZrTests_Function_ExecuteExpectInt64`，核对整数结果；`compile_reference_fixture_expect_failure` 只要求 Function 为 NULL；`parse_reference_fixture_expect_diagnostic` 只检查 parser 错误信号及适用的消息片段。各自清理责任属于当前调用链，硬断言不会自动执行 helper 尾部。

## 已证明的 BUG 与后续核查

BUG: 多处用例/helper 在取得 State、AST、Function 或拥有型文本后使用 Unity 硬断言；非局部退出跳过尾部清理，空生命周期不兜底。这些台账条目记录具体受影响入口，不能当作已运行复现多个独立泄漏。

BUG: invalid-hex 源名的文本为 36 字节、含 NUL 存储为 37，却请求长度 38；multiline 源名的文本为 42、含 NUL 为 43，却请求 44。`ZrCore_String_Create` 的短串哈希链按请求长度读取，在 `ZrCore_Hash_Create`/`ZrHashSeedCreateInternal`/`XXH3_64bits_withSeed` 中形成静态数组越界读取。invalid-char-width 请求 37、含 NUL 存储也为 37，不能标为同类越界。

TODO: 名称/元数据打印、编译返回 NULL 或文本子串检查的判据，需按各样本已有疑点核对预期能力与具体诊断；没有断言不能由日志补偿。本轮只记录契约和问题，运行行为与签名不变。

## 验证和索引

应用后须通过翻译单元的前后编译核对注释兼容性，并确认原物理行、字符串及 EOL 精确保持；实际编译结果另按本批收据记录。静态 BUG 调用链不是运行复现。本阶段不执行上述测试或授予整文件、整模块及全部日志调用者完成信用。

[选定单元及调用证据](../code-review/coverage/tests_parser_syntax_literals_migration.tsv)记录函数、调用点、生命周期和 BUG/TODO。未选定单元保留原台账结论，后续扫描仍需核实其真正调用意图。
