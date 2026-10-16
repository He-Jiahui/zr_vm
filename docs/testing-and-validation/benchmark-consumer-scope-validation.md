---
related_code:
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/benchmark_reports_to_csv.py
implementation_files:
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/benchmark_reports_to_csv.py
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
  - docs/code-review/coverage/scripts_remaining.tsv
tests:
  - tests/benchmarks/test_benchmark_consumer_scope_types.py
  - tests/benchmarks/test_benchmark_task3_report_consumers.py
  - tests/benchmarks/test_benchmark_task4_integration.py
doc_type: module-detail
---

# 基准报告测量范围的类型验证

聚合器与 CSV 导出器读取外部 JSON 报告。`measurement_scope` 和 `prepare_scope` 均须为已定义的字符串；字段缺失、非法字符串及其他 JSON 类型均使测量契约无效。先验证类型再检查合法值集合，避免数组或对象在集合成员检查时抛出 TypeError。

聚合器仍输出具体字段诊断，并将不可信的 `relative_to_c` 和语言比较比值清空。CSV 保留输入测量字段与其他统计，受影响的比值单元格留空。消费者不重新计算比值，也不修改调用方的原报告。

独立回归分别覆盖两个字段的数组、对象、非空容器、null、布尔值、数字及未知字符串，穿过类型验证、完整 summary 构造、两种 CSV writer。summary 的读入边界和 CSV 的文件边界使用内存夹具；既有 Task3 与 Task4 集成测试覆盖正常可比性门槛。修改前 3 个测试方法产生 24 个 TypeError 子例，日志位于 `E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/benchmark-scopes/`；修复后验证结果另记收据，不将夹具替代当作 CLI 执行信用。

修复后 CPython 3.12.10 的新增 3 组（48 个字段/值/消费路径子例）、既有 Task3 5 项、Task4 16 项均通过，共 24 个测试方法。普通 CTest 的三个注册项同样通过，见 `scope-normal-green-receipt.json` 与 `scope-ctest-green-receipt.json`。Windows 原有 Python 注册指向缺失路径；初次嵌入式运行有一条 CLI 子进程因 `_pth` 隔离无法导入同目录模块而失败。最终使用独立 E 根二进制工具缓存启用正常脚本搜索路径，未改共享解释器或测试逻辑。
