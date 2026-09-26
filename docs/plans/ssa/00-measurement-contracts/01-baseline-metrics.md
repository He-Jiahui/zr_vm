---
related_code:
  - tests/performance/perf_statistics.c
  - tests/performance/persistent_protocol.c
  - tests/benchmarks/registry.cmake
  - scripts/benchmark/benchmark_environment_contract.py
implementation_files:
  - tests/performance/perf_statistics.c
  - tests/performance/persistent_protocol.c
  - tests/benchmarks/registry.cmake
  - scripts/benchmark/benchmark_environment_contract.py
  - tests/performance/perf_backend_metrics.h
  - tests/performance/perf_ssa_metrics.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/benchmarks/test_ssa_baseline_metrics.c
  - tests/benchmarks/test_benchmark_environment_contract.py
  - tests/benchmarks/test_benchmark_task4_integration.py
doc_type: milestone-detail
status: planned
---

# 00.01 基线、性能指标与真实执行模式

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立可复现的当前基线，分离执行模式和阶段成本，以真实数据选择优化。

**Architecture：** 复用 persistent runner、校验和、环境指纹与现有报表；新增字段只在协议边界转换，测试内部使用同一统计模型。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M0；为 M2–M7 提供性能门禁。
- 前置：无；从当前 main 的可复现快照开始。
- 交付：基线清单、字段定义、A/B 采样协议、失败样本报告与 AOT runner 接口占位契约。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

tests/performance 已有 persistent runner；registry 当前列出 zr_interp/zr_binary，没有正式 zr_aot 项。2026-09-11 CallBinding 验收记录报告 GCC/Clang focused 通过，同时记录 MSVC GC 崩溃和 Callgrind Ir=0，必须保留这些未通过证据，不能把历史 102/102 当成本次基线。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `tests/performance/perf_statistics.c` | 复用统计和采样质量判断 |
| 现有，修改/复用 | `tests/performance/persistent_protocol.c` | 扩展执行模式与阶段字段 |
| 现有，修改/复用 | `tests/benchmarks/registry.cmake` | 登记代表工作负载与实现模式 |
| 现有，修改/复用 | `scripts/benchmark/benchmark_environment_contract.py` | 复用环境指纹，不另造协议 |
| 计划新增 | `tests/performance/perf_backend_metrics.h` | 分离 requested/actual backend、coverage、fallback、deopt |
| 计划新增 | `tests/performance/perf_ssa_metrics.c` | 归一化帧、GC、分配与编译数据 |
| 计划新增测试 | `tests/benchmarks/test_ssa_baseline_metrics.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 固化当前快照** 记录 HEAD、dirty patch 摘要、编译器/链接器/CPU/OS/电源策略及构建参数；按源码、binary、AOT 分组，失败进程保留 stderr/退出码。

- [ ] **2. 扩展采样结构** compile/link/load/startup/steady-state 分开计时，记录 checksum、helper count、分配、GC 工作量、pause/frame p50/p95/p99/max、RSS 与 code size；缺失值用 unavailable，不填 0。

- [ ] **3. 采集配对样本** 同机同构建参数、同输入、预热后随机 A/B 顺序，多轮独立进程加 persistent 内循环；固定 benchmark tier 和优化配置，报告 CV 与置信区间。

- [ ] **4. 实现收益判断** 指令数或可靠时间至少改善 3% 才声明性能收益；帧 p99/GC pause 改善单列。代表集稳定退化超过 3% 阻止合入，噪声超标则判 inconclusive 并重测，不替换成更好看的指标。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
sample = {requestedBackend, actualBackend, phase, checksum, elapsed, counters}
if processExit != 0 or checksum != expected: result = INVALID
else if actualBackend != requestedBackend: result = FALLBACK_VISIBLE
else if environmentA != environmentB: result = INCOMPARABLE
else if varianceTooHigh or insufficientSamples: result = INCONCLUSIVE
else: improvement = (baseline - candidate) / baseline
record(improvement, confidenceInterval, absoluteCost, regressionCases)
```

Callgrind Ir 衡量指令工作量，不能直接等同 wall-clock；硬件 cache miss 必须来自 PMU，静态布局估计只能标 estimated。计时内不能混入日志/编译，JIT 冷启动与稳态分开。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| checksum 错、崩溃或 Ir=0 | 样本无效，禁止计算收益 |
| requested=AOT、actual=ExecBC | fallback 非零，报告未达到纯 AOT |
| 稳定配对改善 3.5%，与噪声样本改善 4% | 前者按区间判断，后者 inconclusive |
| 无 PMU 权限、RSS 无法读取 | 字段 unavailable，不能声称无 cache miss/零内存 |

复用回归入口：`tests/benchmarks/test_benchmark_environment_contract.py`、`tests/benchmarks/test_benchmark_task4_integration.py`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_baseline_metrics` 和可执行目标 `zr_vm_ssa_baseline_metrics_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_baseline_metrics_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_baseline_metrics$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 代表集每项有 checksum、模式、环境和独立阶段数据；初期允许 AOT 未接入，但 M0 完整收口须等待 07.04，不能伪报完成。

**失败恢复：** 协议使用版本字段；旧报告只读转换，新增测量失败不改动执行语义，保留原始样本重新分析。

**文档交付：** 更新 docs/testing-and-validation 下 benchmark 协议和 docs/plans/benchmark/optimize 的关联说明；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrPerfBackendMetrics SZrPerfBackendMetrics;
typedef struct SZrPerfComparison SZrPerfComparison;
TZrBool ZrTests_Perf_ValidateMetrics(const SZrPerfBackendMetrics *sample);
TZrBool ZrTests_Perf_ComparePaired(const SZrPerfBackendMetrics *baseline,
    const SZrPerfBackendMetrics *candidate, SZrPerfComparison *result);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| actualBackend/phase | runner 在执行边界记录 | report 不从文件扩展名推断模式 |
| checksum/iterationCount | workload 返回并校验 | 归一化必须使用相同有效工作量 |
| fingerprint/raw samples | 环境采集与 persistent 协议 | 原始数据保留，派生报表可重算 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 只增加字段和无效样本判定；拿现有 runner 回放测试，确保旧字段未被改义。

- [ ] **批次 2：** 增加配对比较与置信区间/方差门禁；合成稳定、噪声、缺失和崩溃样本验证分类。

- [ ] **批次 3：** 采集当前代表集并建立 M0 baseline；07.04 接入后再补 AOT，不等待 AOT 才记录已有基线。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange baseline=[100,101,99], candidate=[96,97,95], same checksum/env
act comparePaired()
assert valid comparable result with raw sample count preserved
arrange candidate.exit=crash, candidate.Ir=0
assert INVALID and no percentage benefit
arrange requested=AOT, actual=ExecBC
assert fallback visible and AOT acceptance=false
```

### 迁移结束检查

旧 task4 环境协议继续作为唯一环境字段来源。删除临时的一次性均值计算脚本前先迁移其消费者；不得把本计划新报表与旧 runner 的不同输入混作 A/B。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

