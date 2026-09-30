---
related_code:
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/include/zr_vm_core/gc.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget.c
  - zr_vm_core/src/zr_vm_core/gc/gc_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_compact.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_major_budget.c
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_gc_budget_constructor_defaults.inc
  - tests/acceptance/2026-09-29-gc-budget-constructor-defaults.md
  - tests/acceptance/2026-09-29-gc-major-max-objects-slice.md
doc_type: milestone-detail
status: in-progress
---

# 06.02 并发 Major、GC 预算与移动边界

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 实现预算可观测的 concurrent major、分段 sweep 与选择性 compact，明确最长不可中断区间。

**Architecture：** phase state 持久化扫描 cursor、队列和 debt；并发 mark 选择并固定一种屏障算法，remark 与搬迁保持同步边界，不承诺无法证明的硬实时。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2 frame-safe GC。
- 前置：[06.01 Region、TLAB、Remembered set 与 Minor GC](../06-gc-domain/01-young-allocation.md)。
- 交付：预算 API/统计、major state machine、暂停原因和内存压力策略。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

gc_cycle.c/gc_mark.c 已有 major 支持；先审计当前 incremental-update/SATB 语义，禁止混用两种算法的部分屏障。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c` | phase 驱动和 debt |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/gc/gc_mark.c` | 并发 mark 与工作队列 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/gc.h` | SetBudget/GetStats 对接已有字段 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_budget.c` | 微秒/对象/字节/工作单元预算 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_major.c` | major mark/remark/sweep 状态 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_compact.c` | 明确搬迁和 non-moving 整理模式 |
| 计划新增测试 | `tests/core/test_ssa_major_budget.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |
| 当前构造器回归 | `zr_vm_core/src/zr_vm_core/gc/gc.c` | collector 由非零化 raw allocator 分配，预算可选字段必须有确定默认值；不要整块清零 collector。 |
| 当前构造器回归测试 | `tests/core/test_gc_budget_constructor_defaults.inc` | 经真实 `GlobalState`/collector 构造路径和 allocator typed seeding 验证预算 getter 与 telemetry 默认值。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 确定 mark invariant** 以现有实现验证结果选择一致屏障；以 mutator 写入/删除/新分配三类情况证明并发标记不丢 live 对象，建立最小 race fixture。

- [ ] **2. 定义可切片单位** scan cursor 细化到对象字段/数组片段，避免单个巨大对象撑爆 slice；工作量和 elapsed 双计量，sweep 按 region 分段。

- [ ] **3. 划定不可暂停区** remark/root snapshot、引用修复、锁定 native section 有明确 max-work 和耗时记录；搬迁可延期，若要跨帧恢复 mutator 必须另行实现完整读屏障协议。

- [ ] **4. 处理压力与饥饿** 低预算连续累积 debt 时限制分配/请求 host 维护窗口/报告预算超支；不在帧线程无限 GC 直到结束，OOM 走结构化错误。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
GcStep(budget):
    while workRemaining && withinBudget:
        executeOneBoundedWorkUnit(phase, cursor)
        updateWorkAndElapsed()
        if consistentPausePoint: saveCursor()
    if phaseRequiresAtomicRelocation && insufficientSafeWindow:
        deferRelocationAndReportDebt()
    return {phase, workDone, elapsed, debt, pauseReason, maxAtomicPause}
```

预算是调度目标，不是时间硬保证；报告 p50/p95/p99/max 以及超预算次数。把一整次搬迁包成一个“工作单元”不算实现有界 slice。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 巨型对象/数组、极小预算 | 可分片扫描，记录不可暂停开销 |
| 并发新增/删除引用 | 对象不丢失 |
| 长时间低预算和内存高压 | debt/backpressure 可见且不死循环 |
| pin 碎片和 selective compact | 地址安全，记录回收与真实移动字节 |
| 新建 collector，allocator 先写入有效的旧预算值 | 未配置 getter 均返回 false；collector 与 snapshot 的预算状态为 idle/zero/false |

复用回归入口：`tests/core/test_gc_concurrent_major.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

构造器预算默认值另有一个真实 native RED：`test_gc_budget_constructor_initializes_optional_state`
使用自定义上游分配器，只在类型和大小都匹配 `SZrGarbageCollector` 的那次分配中写入合法、已初始化的预算对象和字段，确认注入地址就是随后发布的实际 collector。它经 `ZrCore_GlobalState_New`、主线程 state 和 registry 初始化后调用公开 getter，在释放 global/state 后再断言，以免失败断言遗留分配。目标 `zr_vm_gc_concurrent_major_test` 尚未注册 CTest；修复前直接运行 11 个测试，其中这一新增用例在 `GetBudget` 返回值处 RED（期望 false、实际 true）。构造器通过 `ZrCore_GcBudget_Init` 和逐字段赋值建立 collector 与 snapshot 的预算默认值，保留其余构造初始化。2026-09-30 UTC，root 在 MSVC 19.44 C11 Debug 下完成 71/71 编译步骤并直接运行全部 11 个测试，0 失败、退出码 0；独立只读审查无阻塞问题。详见 [独立验收记录](../../../../tests/acceptance/2026-09-29-gc-budget-constructor-defaults.md)。构造状态为未配置、`ACCEPTED/IDLE/NONE`、计量字段为 0、pressure/fallback 为 false；未配置时 `GetBudget` 的输出参数不属于契约。这个构造器修复子任务已验证，06.02 整体预算、切片和压力矩阵仍未完成。

并发标记切片的首个真实运行时消费者另有 RED：`test_concurrent_major_slice_honors_max_objects` 通过真实 global、collector、公开 `SetBudget`、活动周期内追加 gray roots 和公开 `GcStep` 建立 fixture。`maxObjects=1` 时，切片前 16 个对象都仍存活、处于 gray 状态，并按精确队列顺序排在已有 root 前；修复前一次切片引用了 8 个对象，期望为 1。root-owned MSVC direct run 在原 12-case binary 中为 12 tests / 1 failure，其余 11 cases 通过；目标没有 CTest 注册。随后已将 `maxObjects=0` 与 `maxObjects=64` 的真实 collector caller-cap fixtures 加入同一测试源，预期各处理 8 个对象；三个 maxObjects case 曾在 header 注释错误引入前通过 MSVC Debug，随后在修正头文件后再次通过当前头文件的五目标增量构建和 direct GC 测试，详见验收记录。生产修复仅在共同 `garbage_collector_concurrent_major_mark_slice` 内以非零配置上限收紧调用方的每次队列弹出数，不扩张调用方上限；配置写入及切片读取通过 GC 域递归 mutation lock 串行化。`ZrCore_GarbageCollector_GetBudget`、`ZrCore_GarbageCollector_EvaluateBudgetStep`、`ZrCore_GarbageCollector_GetBudgetStats`、`ZrCore_Gc_GetStats` 与 `ZrCore_GarbageCollector_GetStatsSnapshot` 不因此获得通用线程安全保证；调用方须串行化这些预算读取/评估和 snapshot 访问与 `SetBudget` 或 GC stats 更新的重叠访问。此路径未调用 `EvaluateBudgetStep`，budget telemetry 仍为 `ACCEPTED/IDLE`、cursor/work 为 0；完整 major、remark、sweep、elapsed/work/byte/pressure 预算仍未覆盖。详见 [独立验收记录](../../../../tests/acceptance/2026-09-29-gc-major-max-objects-slice.md)。

2026-09-30 UTC，临时 `gc.h` 注释错误前的 root-owned MSVC Debug 聚焦构建覆盖 16 个 native target，471/471 steps、exit 0。之后 GCC/WSL 18-target 集成构建在 6/969 steps 因该临时错误关闭的注释导致 C 编译失败、exit 1（不是 CMake 配置失败）；注释已修正。当前头文件下，MSVC Debug 五目标增量构建 `python D:/tmp/zr_vm/ssa-control/run_native.py header-final-build build zr_vm_gc_concurrent_major_test zr_vm_ssa_exec_ir_artifact_v6_test zr_vm_ssa_capability_validation_test zr_vm_ssa_exec_ir_execbc_vm_test zr_vm_ssa_oracle_projections_test` 完成 352/352 steps、exit 0；随后 `python D:/tmp/zr_vm/ssa-control/run_suite.py msvc gc` direct 运行 14/14、exit 0。之后 root-owned Clang GC Unity direct run 的 GDB 状态为 `TestFailures=0`、`TestIgnores=0`、`NumberOfTests=8`，当前正在执行第8个 `test_concurrent_marker_and_mutator_serialize_object_storage`；之前完成的7个case（包括第3–5的全部maxObjects变体）无失败。完整线程栈确认该用例触发既存 nested `MutationBegin`/STW deadlock；保存GDB证据后，经核验exe路径并SIGTERM终止owned PID，Clang GC suite因此为partial/aborted，不能宣称14/14通过。GCC大构建仍在运行。详见acceptance中的状态栈、完整线程栈和termination日志。Release/sanitizer和完整预算矩阵仍未验证；06.02整体保持in-progress。

登记新 CTest 名 `ssa_major_budget` 和可执行目标 `zr_vm_ssa_major_budget_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_major_budget_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_major_budget$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 预算与暂停指标真实，TSan/GC 压力矩阵无新增错误；没有未说明的长暂停路径。

**失败恢复：** 关闭 concurrent mode 可退回安全 stop-the-world mark，但模式/暂停变化必须显式报告，不能仍标 frame-safe 通过。

**文档交付：** 新增 docs/core-runtime/gc-budgeted-major.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrGcBudget SZrGcBudget;
typedef struct SZrGcBudgetStats SZrGcBudgetStats;
TZrBool ZrCore_Gc_SetBudget(struct SZrState *state, const SZrGcBudget *budget);
TZrBool ZrCore_Gc_GetStats(struct SZrState *state, SZrGcBudgetStats *stats);
/* Reuse existing budget fields if the production API already covers them. */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| phase/cursor/debt | GC driver | 每次 slice 结束持久化，不能从头重复扫描 |
| elapsed/work units | budget meter | 同时报告预算与不可中断时间 |
| mark barrier state | 统一并发算法 | SATB 与 incremental-update 不混搭 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 核实当前并发 mark invariant，建立 mutator 写入/删除 race 的确定性调度 fixture。

- [ ] **批次 2：** 为 mark/scan/sweep 拆可恢复 cursor，巨型数组按元素片段扫描。

- [ ] **批次 3：** 接 debt/pressure/maintenance-window 策略，明确 remark/relocation 的不可中断段与预算超支原因。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange huge array and 100-work-unit budget
assert cursor advances incrementally, not whole-array blocking scan
arrange mutator deletes last edge during concurrent mark
assert chosen barrier invariant preserves required liveness
arrange budget insufficient for atomic relocation
assert defer/backpressure recorded; no unsafe resume midway
```

### 迁移结束检查

不能用测试机器平均 pause 替代最大不可中断工作量说明；报告 max、超预算次数和压力策略触发次数。暂停预算不是硬实时保证。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
