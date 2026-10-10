---
related_code:
  - tests/parser/test_type_inference.c
  - tests/CMakeLists.txt
  - tests/unity_config.h
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - tests/harness/runtime_support.c
implementation_files:
  - tests/parser/test_type_inference.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_type_inference.c
doc_type: test-contract
---

# 整数字面量推断测试的失败交付

## 用例职责和调用方式

`test_type_inference_integer_literal` 是 type-inference 可执行目标内由 `RUN_TEST` 注册的静态用例。它在本地测试 State/CompilerState 中解析 `123;`，筛出整数字面量，调用 `ZrParser_LiteralType_Infer` 并用硬断言核对推断成功和 `INT64`。

本次只审查这个完整用例、本地日志宏、创建/销毁 helper 和必要 Unity 调用链。文件内其他用例及宏的所有调用者仍需后续扫描。

## State 与 CompilerState 的关系

CompilerState 借用本例 State。普通返回路径由本例手工清理；正常完成路径先释放推断结果、AST 和 CompilerState，再销毁 State，之后记录结束时间。helper 的存在不意味着非局部失败也能到达它们。

BUG: State 已创建后，CompilerState 的 `malloc` 可失败。随后的 Unity 非空硬断言会跳过 State 销毁；目标的 `tearDown` 为空。这个缺口有当前创建分支、断言和销毁路径的静态证据，本阶段没有运行故障注入。

## 本地失败日志与 Unity 判据

本文件的 `TEST_FAIL_CUSTOM` 是本地宏，只输出日志；它不转接公共 `ZR_TEST_FAIL`，也不向 Unity 设置失败标志。

BUG: AST 根和整数字面量节点的两个早期筛选失败分支调用该本地宏后正常返回。如果进入分支前 Unity 的失败/忽略标志未置位，且分支清理正常返回，这个拒绝判定会被汇总为通过。后续需要把筛选结果交给 Unity；日志文字不能承担失败计账。

进入推断后的成功/类型硬断言仍是实际判据，它们失败时可以非局部退出并跳过本例尾部清理。不能把两个早退分支的假通过证明扩大到每一种运行失败。

TODO: 两个早退分支未采样 `endTime`，本地失败宏仍使用零初始化值计算时间。需明确失败日志的计时合同并取得有效的结束样本；这不是未初始化读取。

## 本批验证和记录

本批保留原代码、字符串、空行及 EOL，仅在该用例加入宏观契约和具体 BUG/TODO。应用后须通过该文件翻译单元的前后编译核对注释兼容性；实际结果另按本批收据记录。没有执行完整 type-inference runner，也没有把静态缺陷标记当作已修复或已运行复现。

[本次选定单元台账](../code-review/coverage/tests_parser_type_inference_failure.tsv)只记录一个用例，其他首方单元继续保留待审状态。相关现行台账中的行引用需要随注释移动，历史验收中的旧快照地址保留其原证据含义。
