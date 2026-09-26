---
related_code:
  - tests/benchmarks/registry.cmake
  - tests/benchmarks/zr_runner/benchmark_server.c
  - tests/performance/perf_report.c
implementation_files:
  - tests/benchmarks/registry.cmake
  - tests/benchmarks/zr_runner/benchmark_server.c
  - tests/performance/perf_report.c
  - tests/benchmarks/aot_runner/main.c
  - tests/benchmarks/aot_runner/aot_coverage.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
  - tests/benchmarks/test_benchmark_registry.c
doc_type: milestone-detail
status: planned
---

# 07.04 AOT Runner、覆盖率与发布门禁

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 补齐正式 AOT benchmark runner，诚实报告 native coverage、fallback 与端到端成本。

**Architecture：** 编译/链接/加载与执行采样分离；registry 明确 C-AOT、LLVM-AOT 等 actual backend，coverage 同时给静态覆盖和动态工作量覆盖。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M4 验收；M0 完整 runner 门禁。
- 前置：[07.02 C、LLVM 同源代码生成](../07-aot-backends/02-c-llvm-lowering.md)；[00.01 基线、性能指标与真实执行模式](../00-measurement-contracts/01-baseline-metrics.md)。
- 交付：AOT runner、registry 项、coverage 定义和 90% 代表集目标验证。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

registry 当前没有正式 AOT implementation；现有 AOT contract 测试不等于真实 workload 的 native coverage。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `tests/benchmarks/registry.cmake` | 增加 AOT 实现项和可用性 |
| 现有，修改/复用 | `tests/benchmarks/zr_runner/benchmark_server.c` | 复用 persistent 协议 |
| 现有，修改/复用 | `tests/performance/perf_report.c` | 报告 compile/link/startup/coverage |
| 计划新增 | `tests/benchmarks/aot_runner/main.c` | 只运行已编译注册的 AOT 入口 |
| 计划新增 | `tests/benchmarks/aot_runner/aot_coverage.c` | native/helper/fallback/deopt 分类计数 |
| 计划新增测试 | `tests/benchmarks/test_ssa_aot_runner_coverage.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 建立可重复编译阶段** 同 source/profile/target 产生 artifact 和 native binary；保存命令、工具链、hash、compile/link 时间，计时执行前结束所有编译工作。

- [ ] **2. 实现 actual mode** runner 响应必须含 requested/actual backend 与 entry token；失败或 fallback 单独上报，不能只看退出码和 checksum。

- [ ] **3. 定义 coverage 分母** 静态 lowering operation coverage 仅作诊断；运行期按原始语义 site 执行次数计算 native-covered/total，融合不能减少分母。另报 fallback 次数和时间占比。

- [ ] **4. 跑代表集门禁** numeric/call/member/array/matrix/map/string/GC/async/domain 分组；主计划 90% 目标逐 workload 和整体列出，不用大量纯数值样本稀释关键 fallback。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
dynamicNativeCoverage = nativeExecutedSemanticSites / allExecutedSemanticSites
coverageClass = emittedNative | nativeRuntimeHelper | interpreterFallback
recordRuntimeHelperSeparately()
if requested=AOT && interpreterFallback>0: markMixedExecution()
report(compile, link, load, startup, run, RSS, codeBytes, GC, coverage, deopt)
noCoverageData -> unavailable, never 100%
```

coverage 不是速度；90% native 不保证 p99 改善，性能仍走 3%/预算门禁。抽样覆盖报告采样率与误差，不冒充精确计数。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 故意不支持的一种操作 | fallback 可见，coverage 下降 |
| 融合五操作为一条 | 分母保持语义工作量 |
| AOT 编译失败但解释器能跑 | AOT 条目失败 |
| 相同输入两个 AOT 后端 | checksum/事件一致，成本独立 |

复用回归入口：`tests/benchmarks/test_benchmark_registry.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_aot_runner_coverage` 和可执行目标 `zr_vm_ssa_aot_runner_coverage_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_aot_runner_coverage_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_aot_runner_coverage$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 真实 AOT runner 与报表闭环；达到 90% 需可复算数据，所有 fallback 明确。

**失败恢复：** AOT 不可用标 unavailable，不用 zr_binary 替换成同名 AOT 结果。

**文档交付：** 新增 docs/testing-and-validation/aot-runner-coverage.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrAotCoverageCounts {
    TZrUInt64 nativeSites, nativeHelperSites, interpreterSites, deoptCount;
} SZrAotCoverageCounts;
TZrBool ZrTests_AotCoverage_Validate(const SZrAotCoverageCounts *counts);
/* Coverage ratios are unavailable when the denominator is zero. */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| 语义 site identity | ExecIR/source map | 融合前后覆盖分母相同 |
| mode/counters | 实际 runner 分派 | requested mode 不能覆盖 actual mode |
| compile/link/run timing | 外部编排与 runtime | 编译成本不混入 steady-state |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现已编译 AOT artifact 的独立 runner 与 checksum，不先接优化报表。

- [ ] **批次 2：** 接 coverage/actual backend 与 registry，注入 fallback 验证不是永远 100%。

- [ ] **批次 3：** 跑代表集并发布 per-workload 和 aggregate 的 90%/性能门禁证据。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange 80 native,10 native-helper,10 interpreter semantic sites
assert classes reported separately and interpreter share visible
arrange all counts=0 due to startup failure
assert coverage=unavailable, not 100%
arrange fusion combines five original sites
assert semantic denominator still counts their executed work
```

### 迁移结束检查

AOT runner 缺失不能通过调用现有 binary runner 再改 label 填补。统计总和、采样倍率与无法计数路径必须有自检。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

