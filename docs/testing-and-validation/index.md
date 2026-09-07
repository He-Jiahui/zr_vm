---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - zr_vm_core/include/zr_vm_core/array.h
  - zr_vm_core/include/zr_vm_core/string.h
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/zr_vm_register_executable_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/benchmark_task4_environment.cmake
  - scripts/benchmark/benchmark_task4_contract.py
  - scripts/benchmark/benchmark_report_publisher.py
  - scripts/benchmark/benchmark_environment_contract.py
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/build_benchmark_release.sh
  - scripts/benchmark/run_wsl_benchmarks_report_csv.sh
  - scripts/benchmark/benchmark_execution_plan.py
  - scripts/benchmark/benchmark_statistics.py
  - tests/performance/perf_runner.c
  - tests/test_runner.c
  - tests/TEST_EXECUTION_ORDER.md
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_type_inference.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
  - docs/zr_language_specification.md
  - docs/zr_language_test_requirements.md
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/CMakeLists.txt
  - tests/cmake/zr_vm_register_executable_suite.cmake
  - tests/cmake/run_executable_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/benchmark_task4_environment.cmake
  - scripts/benchmark/benchmark_task4_contract.py
  - scripts/benchmark/benchmark_report_publisher.py
  - scripts/benchmark/benchmark_environment_contract.py
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/build_benchmark_release.sh
  - scripts/benchmark/run_wsl_benchmarks_report_csv.sh
  - scripts/benchmark/benchmark_execution_plan.py
  - scripts/benchmark/benchmark_statistics.py
  - tests/performance/perf_runner.c
  - tests/cmake/run_benchmark_task3_suite_contract_test.cmake
  - tests/benchmarks/test_benchmark_execution_plan.py
  - tests/benchmarks/test_benchmark_statistics.py
  - tests/benchmarks/test_benchmark_task3_report_consumers.py
  - tests/benchmarks/test_benchmark_task4_integration.py
  - tests/cmake/run_benchmark_task4_environment_contract_test.cmake
  - tests/acceptance/2026-08-30-benchmark-task4-integration.md
  - tests/test_runner.c
  - tests/TEST_EXECUTION_ORDER.md
  - tests/parser/test_char_and_type_cast.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
plan_sources:
  - user: 2026-04-03 实现 ZR 核心语义外部对齐第一阶段
  - user: 2026-04-05 ctest 需要生成性能测试报告并包含耗时与内存占用
  - user: 2026-08-28 require full MSVC CTest execution without command-length skips
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
  - docs/zr_language_specification.md
  - docs/zr_language_test_requirements.md
tests:
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/acceptance/2026-08-25-clang-force-inline-portability.md
  - tests/benchmarks/test_benchmark_registry.c
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/benchmark_task3_case_assembly.cmake
  - tests/cmake/run_benchmark_task3_suite_contract_test.cmake
  - tests/benchmarks/test_benchmark_task3_report_consumers.py
  - tests/acceptance/2026-08-30-benchmark-calibration-sampling-randomization.md
  - tests/cmake/run_executable_suite.cmake
  - tests/acceptance/2026-08-10-ownership-object-member-separation.md
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

- `compiler-inline-portability.md`
  - GCC、Clang 与 MSVC 的 compiler detection 和 force-inline 合同
  - 为什么 Clang Debug 静态链接必须使用 `always_inline`
  - 如何用真实静态 test executable 证明 public header helpers 不产生悬空外部符号
- `core-semantics-reference-alignment.md`
  - 6 个核心语义主题的 reference manifest 结构
  - 第一阶段新增的 4 个 fixture 及其期望结果
  - 现有三个测试入口如何消费这些资产
  - 为什么当前切片先落“外部证据清单 + 最小可运行基线”
  - `ctest-performance-reporting.md`
  - `tests/benchmarks` 如何作为 benchmark 的单一事实来源
  - `case x implementation` 报告结构、`relative_to_c` 规则与 `SKIP` 语义
  - process 与 persistent runtime scope、严格 line protocol、session RSS 合同
  - `performance/` 与 `performance_steady/` 的隔离及 steady 聚合命令
- `benchmark-task4-environment-cache-publishing.md`
  - post-capture 环境证据、baseline 比较、WSL cache identity 与原子发布
  - Windows 诊断模式以及跨平台可比性边界
  - `ZR_VM_PERF_WARMUP` / `ZR_VM_PERF_ITERATIONS` 环境变量
  - Windows/MSVC 与 WSL 下的验证命令
  - `benchmark-calibration-sampling-randomization.md`
  - flat filtered execution plan、固定 seed 与版本化 shuffle 合同
  - process/steady/profile 的校准、样本预算与覆盖规则
  - schema 3 的 MAD、CV、bootstrap CI、稳定性与 fail-closed ratio 合同
- `benchmark-task4-environment-cache-publishing.md`
  - 完成环境指纹、单核隔离与 baseline fail-closed 比较
  - WSL source/toolchain keyed cache 与报告-only 原子发布
- `ctest-executable-suite-manifests.md`
  - 大型 executable suite 如何把 tier 列表移出 CTest 命令行
  - multi-config 与 single-config 如何生成配置专属 manifest
  - manifest 缺失、child 启动失败和非零退出如何保持真实失败
- `../reference-alignment/full-stack-test-matrix.md`
  - 10 个固定语义域的全栈主矩阵
  - `tests/fixtures/reference/core_semantics/` 下的新 manifest 合同
  - 120 条首轮 case 的配额、helper 与 executable 映射
  - 首轮 30 条高风险优先 case 清单
  - `source / artifact / runtime / project` 分层验证入口
  - `smoke/core/stress` 三档过滤与 interp/binary 主链路合同
- `syntax-reference-v1-fixture.md`
  - Syntax 07A 的单一 project fixture、coverage feature ids 和 collection contract
  - `design-pending` owner gate 规则，以及 provider/file-locator golden 的 path hygiene
  - formatted/minified `.zrs` 与 source-range-independent `.zri` fingerprint evidence

## Parser Fixture String Bounds

Parser test source names use `ZrCore_String_CreateFromNative` for NUL-terminated
literals. The helper computes the byte length; hardcoded lengths can make the
hash function read beyond the fixture string before parsing starts. The runtime
owns the created string, while the literal remains borrowed input for the call.
The iterable import case in `tests/parser/test_type_inference.c` is covered by
the full type-inference runner under Clang ASan/UBSan; see
[Plan 01 Task 6 Sub08](../plans/lsp/optimize/2026-09-07-plan01-task06-sub08-type-test-string-boundary.md).

## 阅读顺序

1. 先看 `compiler-inline-portability.md`，确认当前工具链检测和 header-inline 链接合同。
2. 再看 `core-semantics-reference-alignment.md`，了解 reference manifests、fixture 组织方式和本阶段覆盖边界。
3. 聚合 executable suite 在 Windows 上启动失败时，查看 `ctest-executable-suite-manifests.md`，确认配置专属 manifest 和失败传播合同。
4. 需要看性能报告链路时打开 `ctest-performance-reporting.md`，确认 benchmark suite、报告产物和环境变量覆盖。
5. 需要判断 benchmark 是否可比较时，再看 `benchmark-calibration-sampling-randomization.md`，确认执行计划、校准、稳定性和 schema 3 合同。
6. 需要判断环境、baseline 或发布是否可接受时，打开 `benchmark-task4-environment-cache-publishing.md`。
7. 再看 `../reference-alignment/full-stack-test-matrix.md`，确认当前已经升级到 10 个固定语义域、120 条首轮 inventory，以及现有分层验证入口。
8. 需要跟进 Syntax 07A fixture 时看 `syntax-reference-v1-fixture.md`，确认 feature slot 是否为 current、negative 或 design-pending。
9. 再沿 frontmatter 的 `tests` 字段定位具体 C 测试、manifest 和 fixture 文件。
10. 需要跑快速回归时优先走 `smoke/core/stress` 过滤；AOT 归档资产已移到 `zr_vm_aot/`，不再属于主仓测试入口。
10. 后续新增语义主题时，优先复用主矩阵和 manifest 合同，而不是继续把上游参考散落在临时笔记里。
