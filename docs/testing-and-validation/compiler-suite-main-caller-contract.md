---
related_code:
  - tests/parser/test_compiler_call_lowering_focus_main.c
  - tests/parser/test_compiler_integration_main.c
  - tests/parser/test_escape_metadata_pipeline_main.c
  - tests/parser/test_escape_metadata_pipeline.c
  - tests/CMakeLists.txt
  - tests/cmake/zr_vm_register_executable_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - tests/harness/zr_test_log_macros.h
  - tests/unity_lifecycle_noop.c
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
implementation_files:
  - tests/parser/test_compiler_call_lowering_focus_main.c
  - tests/parser/test_compiler_integration_main.c
  - tests/parser/test_escape_metadata_pipeline_main.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/parser/test_compiler_features.c
  - tests/parser/test_escape_metadata_pipeline.c
doc_type: test-contract
---

# 编译与逃逸测试入口的调用契约

## 启动方式和职责

| 入口目标 | 当前调用方式 | 已核对的派发范围 |
| --- | --- | --- |
| `zr_vm_compiler_call_lowering_focus_test` | 直接运行；当前注册配置未把此目标放入 CTest suite | 九个调用降级回归入口 |
| `zr_vm_compiler_integration_test` | 直接运行，或由 CTest `language_pipeline` manifest suite runner 启动 | 127 个基础、表达式、语句、元数据、所有权和工程回归入口 |
| `zr_vm_escape_pipeline_test` | 直接运行，或由 CTest `escape_pipeline` executable suite runner 启动 | 十二个编译/二进制形状及运行时闭包入口 |

三个 `main` 负责注册、阶段提示和进程结果汇总。它们通过 `RUN_TEST` 交给 Unity 派发，不作为可重入 helper 使用。集成入口另复用 escape 的两个形状用例；不能由这些注册关系推导所有被派发函数的实现和语义已经审查完成。

## 日志和结果的调用链

标题、分隔线和 Completed 文本只标识阶段。公共日志分隔宏不会登记测试，也不会清除 Unity 的失败状态；这里没有启动公共 timer。真实结果由用例断言、`UnityConcludeTest` 计账和 `UNITY_END()` 交付。

`UNITY_END()` 返回失败计数，零值表示没有被 Unity 计为失败；忽略用例仍单独统计。suite runner 展开目标或 manifest 后启动进程，并检查进程结果。仅有日志输出或进程启动记录不能替代该结果。

## 资源和失败边界

用例及其 helper 取得 VM、源码、函数、二进制缓冲或其他 fixture；main 没有接管这些资源。focus 和独立 escape 目标链接空 `setUp`/`tearDown`，集成目标的 features 源也提供空生命周期函数。

Unity 硬断言可以非局部退出当前用例，跳过局部尾部清理。即使 Unity 另行保护 `tearDown`，空回调也不会释放没有交给它的资源。

TODO: escape 用例已有正常路径源码漏释放以及硬断言越过清理的具体记录。入口只能说用例持有并尝试清理，不能保证释放；后续应沿 `ReadSource` 和各资源创建/释放链核实并解决这些缺口。

## 本批验证和审查边界

本批审查三个完整 main、当前 CMake/runner/Unity 调用链及必要的资源反例。候选保留全部原物理行、字符串和 EOL，只加入注释；应用后须通过三个入口翻译单元的前后编译核对注释兼容性；实际结果另按本批验证收据记录。本阶段不执行整个 suite。

台账只更新这三个函数的结论，并迁移相关源码行引用的两个端点。既有声明、分组、其他接口和被注册用例的结论不获得新的完成信用。

- [focus 与集成入口台账](../code-review/coverage/tests_parser_compiler_suite_mains.tsv)
- [escape 入口台账](../code-review/coverage/tests_parser_artifact_metadata.tsv)
