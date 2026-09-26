---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_tlab.c
  - zr_vm_core/src/zr_vm_core/gc/gc_minor.c
  - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_young_allocation.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_gc_domain_bridge.c
doc_type: milestone-detail
status: planned
---

# 06.01 Region、TLAB、Remembered set 与 Minor GC

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 收敛现有 young generation、region、分配缓冲与 remembered set，实现可验证的 minor collection。

**Architecture：** worker TLAB 只负责快分配，refill/GC 走 safepoint；minor GC 使用精确 roots 和 old-to-young cards，移动发布必须在一致状态完成。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2。
- 前置：[04.04 精确根、Frame 重定位与调试观察](../04-frame-native/04-roots-observation.md)。
- 交付：现有 GC 路径审计、TLAB/refill、card barrier 与 minor evacuation 的明确状态机。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

gc.h/gc_cycle.c 已有 region、evacuation、major phases。应先辨别哪些只是 bookkeeping、哪些真实搬迁；当前 old compaction 代码含保持稳定地址而重排 region 归属的路径，不能把它报告为对象移动压缩。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/gc.h` | 复用 region/phase/budget 定义 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c` | 拆出 minor 责任和状态转换 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c` | worker 分配和 safepoint 协同 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_tlab.c` | TLAB bump/refill 和废弃空间统计 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_minor.c` | minor 标记/移动/引用修复 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c` | card 标记、扫描和清空协议 |
| 计划新增测试 | `tests/core/test_ssa_young_allocation.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 列清现有 phases** 为每个 phase 标 actual action、可暂停点、持锁资源和允许 mutator 状态；先把现有函数无行为迁出巨型 gc_cycle.c。

- [ ] **2. 接入 TLAB** fast allocation 检查 cursor+alignedSize 溢出/limit，refill 走 safepoint；large/pinned/native-visible 分配 non-moving 区，不错误使用 nursery。

- [ ] **3. 实现写屏障** heap store 写入对象与 card 更新满足内存序；覆盖 typed array、inline struct、native/reflection writeback，不能只在普通字段赋值插 barrier。

- [ ] **4. 完成 minor 搬迁** 停住相关 mutator，扫描 roots/cards，复制并设置 forwarding，更新所有引用，最后恢复 mutator；to-space 不足走明确 promotion/failure 策略。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
allocate(size):
    if alignedSize <= tlab.limit - tlab.cursor: bumpAndInitialize()
    else: publishRootsAndRefillAtSafepoint()
store(oldOwner, youngValue): performStore(); recordCard(oldOwner)
minorTransaction:
    stopDomainMutators -> traceRootsAndCards -> evacuate -> rewriteReferences
    -> verifyNoUnresolvedForwarding -> retireFromSpace -> resumeMutators
```

首阶段没有 read barrier/handle indirection 时，不能在 evacuation/rewrite 半途中恢复 mutator；工作预算不足只能在已证明一致边界延后整个搬迁事务。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| TLAB 耗尽、超大分配、对齐溢出 | 安全 refill/分流/错误 |
| old→young 经数组/inline/native 写入 | minor 后对象仍可达 |
| pinned/native-visible 对象 | 地址稳定 |
| to-space 不足或并发 worker 请求 GC | 状态不半发布、不丢 roots |

复用回归入口：`tests/core/test_gc_domain_multimutator.c`、`tests/core/test_gc_domain_bridge.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_young_allocation` 和可执行目标 `zr_vm_ssa_young_allocation_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_young_allocation_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_young_allocation$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** minor 全引用修复、barrier 覆盖和 sanitizer 通过；真实移动与 bookkeeping 压缩分别计数。

**失败恢复：** 可缩小 nursery/停用 TLAB 优化，不能关闭 necessary barriers；遇到预算不够报告 backpressure 而不是不安全恢复。

**文档交付：** 新增 docs/core-runtime/gc-young-allocation.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrGcMinorRequest SZrGcMinorRequest;
typedef struct SZrGcMinorResult SZrGcMinorResult;
TZrBool ZrCore_Gc_RunMinorTransaction(struct SZrState *state,
    const SZrGcMinorRequest *request, SZrGcMinorResult *result);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| TLAB cursor/limit | worker-local allocator | refill 在 safepoint，非线程安全共享 cursor |
| card/remembered set | 所有托管写入入口 | minor 扫描并在正确 epoch 清空 |
| forwarding/ref rewrite | minor transaction | mutator 恢复前所有可达引用一致 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 给现有 GC action 建行为表，先拆 minor 和 barrier，不改变采集策略。

- [ ] **批次 2：** 接 TLAB/large/pinned 分流，分别测试 refill、对齐、溢出、未初始化对象失败清理。

- [ ] **批次 3：** 完成 minor 移动与全引用修复压力测试，逐类覆盖字段、数组、closure、cache、native roots。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange old inline struct field stores young object through native writeback
act minor GC
assert object remains reachable and reference rewritten
arrange pinned nursery candidate
assert address remains stable or allocated nonmoving from start
arrange evacuation failure before commit
assert mutators never observe half-forwarded graph
```

### 迁移结束检查

跨帧暂停只发生在有证明的一致边界。以 region bookkeeping 重排代替真实 compact 时，指标必须写明 movedBytes=0 而非声称完成移动压缩。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

