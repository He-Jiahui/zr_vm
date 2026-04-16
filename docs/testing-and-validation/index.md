---
related_code:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/test_runner.c
  - tests/TEST_EXECUTION_ORDER.md
  - tests/parser/test_char_and_type_cast.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - docs/zr_language_specification.md
  - docs/zr_language_test_requirements.md
implementation_files:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/test_runner.c
  - tests/TEST_EXECUTION_ORDER.md
  - tests/parser/test_char_and_type_cast.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
plan_sources:
  - user: 2026-04-03 实现 ZR 核心语义外部对齐第一阶段
  - user: 2026-04-05 ctest 需要生成性能测试报告并包含耗时与内存占用
  - docs/zr_language_specification.md
  - docs/zr_language_test_requirements.md
tests:
  - tests/benchmarks/test_benchmark_registry.c
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/performance/perf_runner.c
  - tests/parser/test_char_and_type_cast.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - tests/fixtures/reference/core_semantics/literals/manifest.json
  - tests/fixtures/reference/core_semantics/expressions/manifest.json
  - tests/fixtures/reference/core_semantics/imports/manifest.json
  - tests/fixtures/reference/core_semantics/calls/manifest.json
  - tests/fixtures/reference/core_semantics/casts-and-const/manifest.json
  - tests/fixtures/reference/core_semantics/diagnostics/manifest.json
doc_type: category-index
---

# Testing And Validation

本目录记录 `zr_vm` 用来校对语言语义、补全边界测试、以及把外部参考语言测试资产映射到 ZR 自身回归体系的方法。

## 当前主题

- `core-semantics-reference-alignment.md`
  - 6 个核心语义主题的 reference manifest 结构
  - 第一阶段新增的 4 个 fixture 及其期望结果
  - 现有三个测试入口如何消费这些资产
  - 为什么当前切片先落“外部证据清单 + 最小可运行基线”
- `ctest-performance-reporting.md`
  - `tests/benchmarks` 如何作为 benchmark 的单一事实来源
  - `case x implementation` 报告结构、`relative_to_c` 规则与 `SKIP` 语义
  - `ZR_VM_PERF_WARMUP` / `ZR_VM_PERF_ITERATIONS` 环境变量
  - Windows/MSVC 与 WSL 下的验证命令
- `../reference-alignment/full-stack-test-matrix.md`
  - 10 个固定语义域的全栈主矩阵
  - `tests/fixtures/reference/core_semantics/` 下的新 manifest 合同
  - 120 条首轮 case 的配额、helper 与 executable 映射
  - 首轮 30 条高风险优先 case 清单
  - `source / artifact / runtime / project` 分层验证入口
  - `smoke/core/stress` 三档过滤与 interp/binary 主链路合同

## 阅读顺序

1. 先看 `core-semantics-reference-alignment.md`，了解 reference manifests、fixture 组织方式和本阶段覆盖边界。
2. 需要看性能报告链路时打开 `ctest-performance-reporting.md`，确认 benchmark suite、报告产物和环境变量覆盖。
3. 再看 `../reference-alignment/full-stack-test-matrix.md`，确认当前已经升级到 10 个固定语义域、120 条首轮 inventory，以及现有分层验证入口。
4. 再沿 frontmatter 的 `tests` 字段定位具体 C 测试、manifest 和 fixture 文件。
5. 需要跑快速回归时优先走 `smoke/core/stress` 过滤；AOT 归档资产已移到 `zr_vm_aot/`，不再属于主仓测试入口。
6. 后续新增语义主题时，优先复用主矩阵和 manifest 合同，而不是继续把上游参考散落在临时笔记里。
